/*
** sndfilesource.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "aldatasource.h"
#include "exception.h"

#include <cstdio>
#include <vector>
#include <sndfile.hh>

static SF_VIRTUAL_IO sfvirtual = {
	.get_filelen = [](void *handle) {
#ifdef MKXPZ_RETRO
		PHYSFS_Stat stat;
		return PHYSFS_stat(((struct FileSystem::File *)handle)->path(), &stat) == 0 ? 0 : (sf_count_t)stat.filesize;
#else
		Sint64 size = 0;
		Sint64 pos = SDL_RWtell((SDL_RWops *)handle);
		if (pos >= 0) {
			size = SDL_RWseek((SDL_RWops *)handle, 0, RW_SEEK_END);
			SDL_RWseek((SDL_RWops *)handle, pos, RW_SEEK_SET);
		}
		return (sf_count_t)size;
#endif // MKXPZ_RETRO
	},

	.seek = [](sf_count_t offset, int whence, void *handle) {
#ifdef MKXPZ_RETRO
		switch (whence) {
			case SF_SEEK_CUR:
				{
					sf_count_t pos = PHYSFS_tell(((struct FileSystem::File *)handle)->get());
					if (pos >= 0) {
						offset += pos;
					}
				}
				break;
			case SF_SEEK_END:
				{
					PHYSFS_Stat stat;
					if (PHYSFS_stat(((struct FileSystem::File *)handle)->path(), &stat) != 0) {
						offset += stat.filesize;
					}
				}
				break;
		}
		PHYSFS_seek(((struct FileSystem::File *)handle)->get(), offset);
		return offset;
#else
		return (sf_count_t)SDL_RWseek((SDL_RWops *)handle, offset, whence);
#endif // MKXPZ_RETRO
	},

	.read = [](void *ptr, sf_count_t count, void *handle) {
#ifdef MKXPZ_RETRO
		return (sf_count_t)PHYSFS_readBytes(((struct FileSystem::File *)handle)->get(), ptr, count);
#else
		return (sf_count_t)SDL_RWread((SDL_RWops *)handle, ptr, 1, count);
#endif // MKXPZ_RETRO
	},

	.write = [](const void *ptr, sf_count_t count, void *handle) {
		return (sf_count_t)-1;
	},

	.tell = [](void *handle) {
#ifdef MKXPZ_RETRO
		return (sf_count_t)PHYSFS_tell(((struct FileSystem::File *)handle)->get());
#else
		return (sf_count_t)SDL_RWtell((SDL_RWops *)handle);
#endif // MKXPZ_RETRO
	},
};

struct SndfileSource : ALDataSource
{
#ifdef MKXPZ_RETRO
	std::shared_ptr<struct FileSystem::File> src;
#else
	SDL_RWops src;
#endif // MKXPZ_RETRO

	SndfileHandle handle;

	uint64_t currentFrame;

	bool looped;

	struct
	{
		int channels;
		int rate;
		int frameSize;
		ALenum alFormat;
	} info;

	std::vector<int16_t> sampleBuf;

	SndfileSource(
#ifdef MKXPZ_RETRO
			std::shared_ptr<struct FileSystem::File> ops,
#else
			SDL_RWops &ops,
#endif // MKXPZ_RETRO
			bool looped)
	    : src(ops),
#ifdef MKXPZ_RETRO
	      handle(sfvirtual, ops.get()),
#else
	      handle(sfvirtual, &ops),
#endif // MKXPZ_RETRO
	      currentFrame(0),
	      looped(looped)
	{
		if (handle.error())
		{
#ifndef MKXPZ_RETRO
			SDL_RWclose(&src);
#endif // MKXPZ_RETRO
			throw Exception(Exception::MKXPError, "sndfile: Cannot read file");
		}

		/* Extract bitstream info */
		info.channels = handle.channels();
		info.rate = handle.samplerate();

		if (info.channels > 2)
		{
#ifndef MKXPZ_RETRO
			SDL_RWclose(&src);
#endif // MKXPZ_RETRO
			throw Exception(Exception::MKXPError, "Cannot handle audio with more than 2 channels");
		}

		info.alFormat = chooseALFormat(sizeof(int16_t), info.channels);
		info.frameSize = sizeof(int16_t) * info.channels;

		sampleBuf.resize(STREAM_BUF_SIZE);
	}

	~SndfileSource()
	{
#ifndef MKXPZ_RETRO
		SDL_RWclose(&src);
#endif // MKXPZ_RETRO
	}

	int sampleRate()
	{
		return info.rate;
	}

	void seekToOffset(double seconds)
	{
		currentFrame = std::lround(seconds * (double)info.rate);

		if (currentFrame < 0 || currentFrame >= (uint64_t)handle.frames())
		{
			currentFrame = 0;
		}

		handle.seek(currentFrame, SF_SEEK_SET);
	}

	Status fillBuffer(AL::Buffer::ID alBuffer)
	{
		void *bufPtr = sampleBuf.data();
		uint64_t availBuf = sampleBuf.size();
		uint64_t bufUsed  = 0;

		uint64_t canRead = availBuf;

		Status retStatus = ALDataSource::NoError;

		bool readAgain = false;

		if (looped)
		{
			uint64_t tilLoopEnd = handle.frames() * info.frameSize;

			canRead = std::min(availBuf, tilLoopEnd);
		}

		while (canRead > 16)
		{
			sf_count_t res = handle.read((int16_t *)bufPtr, availBuf / sizeof(int16_t));

			if (res < 0)
			{
				/* Read error */
				retStatus = ALDataSource::Error;

				break;
			}

			if (res == 0)
			{
				/* EOF */
				if (looped)
				{
					retStatus = ALDataSource::WrapAround;
					seekToOffset(0);
				}
				else
				{
					retStatus = ALDataSource::EndOfStream;
				}

				/* If we sought right to the end of the file,
				 * we might be EOF without actually having read
				 * any data at all yet (which mustn't happen),
				 * so we try to continue reading some data. */
				if (bufUsed > 0)
					break;

				if (readAgain)
				{
					/* We're still not getting data though.
					 * Just error out to prevent an endless loop */
					retStatus = ALDataSource::Error;
					break;
				}

				readAgain = true;
			}

			bufUsed += res;
			bufPtr = &sampleBuf[bufUsed];
			currentFrame += (res * sizeof(int16_t)) / info.frameSize;

			if (looped && currentFrame >= (uint64_t)handle.frames())
			{
				/* Determine how many frames we're
				 * over the loop end */
				uint64_t discardFrames = currentFrame - handle.frames();
				bufUsed -= discardFrames * info.channels;

				retStatus = ALDataSource::WrapAround;

				/* Seek to loop start */
				seekToOffset(0);

				break;
			}

			canRead -= res * sizeof(int16_t);
		}

		if (retStatus != ALDataSource::Error)
			AL::Buffer::uploadData(alBuffer, info.alFormat, sampleBuf.data(),
			                       bufUsed*sizeof(int16_t), info.rate);

		return retStatus;
	}

	uint64_t loopStartFrames()
	{
		return 0;
	}

	bool setPitch(float)
	{
		return false;
	}
};

ALDataSource *createSndfileSource(
#ifdef MKXPZ_RETRO
				std::shared_ptr<struct FileSystem::File> ops,
#else
				SDL_RWops &ops,
#endif // MKXPZ_RETRO
				bool looped)
{
	return new SndfileSource(ops, looped);
}
