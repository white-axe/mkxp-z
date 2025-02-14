/*
** alstream.cpp
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

#include "alstream.h"

#include "sharedstate.h"
#include "sharedmidistate.h"
#include "eventthread.h"
#include "filesystem.h"
#include "exception.h"
#include "aldatasource.h"
#include "fluid-fun.h"
#include "sdl-util.h"
#include "debugwriter.h"

#ifndef MKXPZ_RETRO
#  include <SDL_mutex.h>
#  include <SDL_thread.h>
#  include <SDL_timer.h>
#endif // MKXPZ_RETRO

ALStream::ALStream(LoopMode loopMode,
		           const std::string &threadId)
	: looped(loopMode == Looped),
	  state(Closed),
	  source(0),
#ifdef MKXPZ_RETRO
	  streamInited(false),
	  sourceExhausted(false),
	  threadTermReq(false),
	  needsRewind(false),
#else
	  thread(0),
#endif // MKXPZ_RETRO
	  preemptPause(false),
      pitch(1.0f)
{
	alSrc = AL::Source::gen();

	AL::Source::setVolume(alSrc, 1.0f);
	AL::Source::setPitch(alSrc, 1.0f);
	AL::Source::detachBuffer(alSrc);

	for (int i = 0; i < STREAM_BUFS; ++i)
		alBuf[i] = AL::Buffer::gen();

#ifndef MKXPZ_RETRO
	pauseMut = SDL_CreateMutex();

	threadName = std::string("al_stream (") + threadId + ")";
#endif // MKXPZ_RETRO
}

ALStream::~ALStream()
{
	close();

	AL::Source::clearQueue(alSrc);
	AL::Source::del(alSrc);

	for (int i = 0; i < STREAM_BUFS; ++i)
		AL::Buffer::del(alBuf[i]);

#ifndef MKXPZ_RETRO
	SDL_DestroyMutex(pauseMut);
#endif // MKXPZ_RETRO
}

void ALStream::close()
{
	checkStopped();

	switch (state)
	{
	case Playing:
	case Paused:
		stopStream();
	case Stopped:
		closeSource();
		state = Closed;
	case Closed:
		return;
	}
}

void ALStream::open(const std::string &filename)
{
	openSource(filename);

	state = Stopped;
}

void ALStream::stop()
{
	checkStopped();

	switch (state)
	{
	case Closed:
	case Stopped:
		return;
	case Playing:
	case Paused:
		stopStream();
	}

	state = Stopped;
}

void ALStream::play(double offset)
{
	if (!source)
		return;

	checkStopped();

	switch (state)
	{
	case Closed:
	case Playing:
		return;
	case Stopped:
		startStream(offset);
		break;
	case Paused :
		resumeStream();
	}

	state = Playing;
}

void ALStream::pause()
{
	checkStopped();

	switch (state)
	{
	case Closed:
	case Stopped:
	case Paused:
		return;
	case Playing:
		pauseStream();
	}

	state = Paused;
}

void ALStream::setVolume(float value)
{
	AL::Source::setVolume(alSrc, value);
}

void ALStream::setPitch(float value)
{
	/* If the source supports setting pitch natively,
	 * we don't have to do it via OpenAL */
	if (source && source->setPitch(value))
		AL::Source::setPitch(alSrc, 1.0f);
	else
		AL::Source::setPitch(alSrc, value);
}

ALStream::State ALStream::queryState()
{
	checkStopped();

	return state;
}

double ALStream::queryOffset()
{
	if (state == Closed || !source)
		return 0;

	double procOffset = static_cast<double>(procFrames) / source->sampleRate();

	// TODO: getSecOffset returns a float, we should improve precision to double.
	return procOffset + AL::Source::getSecOffset(alSrc);
}

void ALStream::closeSource()
{
	delete source;
}

struct ALStreamOpenHandler : FileSystem::OpenHandler
{
	bool looped;
	ALDataSource *source;
	std::string errorMsg;

	ALStreamOpenHandler(bool looped)
	    : looped(looped), source(0)
	{}

	bool tryRead(
#ifdef MKXPZ_RETRO
		std::shared_ptr<struct FileSystem::File> ops,
#else
		SDL_RWops &ops,
#endif // MKXPZ_RETRO
		const char *ext
	) {
		/* Try to read ogg file signature */
		char sig[5] = { 0 };
#ifdef MKXPZ_RETRO
		PHYSFS_readBytes(ops->get(), sig, 4);
		PHYSFS_seek(ops->get(), 0);
#else
		SDL_RWread(&ops, sig, 1, 4);
		SDL_RWseek(&ops, 0, RW_SEEK_SET);
#endif // MKXPZ_RETRO

		try
		{
			if (!strcmp(sig, "OggS"))
			{
				source = createVorbisSource(ops, looped);
				return true;
			}

			if (!strcmp(sig, "MThd"))
			{
#ifdef MKXPZ_RETRO
				shState->midiState().initIfNeeded();
#else
				shState->midiState().initIfNeeded(shState->config());
#endif // MKXPZ_RETRO

				if (HAVE_FLUID)
				{
					source = createMidiSource(ops, looped);
					return true;
				}
			}

#ifdef MKXPZ_RETRO
			source = createSndfileSource(ops, looped);
#else
			source = createSDLSource(ops, ext, STREAM_BUF_SIZE, looped);
#endif // MKXPZ_RETRO
		}
		catch (const Exception &e)
		{
			/* All source constructors will close the passed ops
			 * before throwing errors */
			errorMsg = e.msg;
			return false;
		}

		return true;
	}
};

void ALStream::openSource(const std::string &filename)
{
	ALStreamOpenHandler handler(looped);
	try
	{
#ifdef MKXPZ_RETRO
		std::string path("/mkxp-retro-game/");
		path.append(filename);
		mkxp_retro::fs->openRead(handler, path.c_str()); // TODO: move into shState
#else
		shState->fileSystem().openRead(handler, filename.c_str());
#endif // MKXPZ_RETRO
	} catch (const Exception &e)
	{
		/* If no file was found then we leave the stream open.
		 * A PHYSFSError means we found a match but couldn't
		 * open the file, so we'll close it in that case. */
		if (e.type != Exception::NoFileError)
			close();
		
		throw e;
	}

	close();

	if (!handler.source)
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "Unable to decode audio stream: %s: %s",
		         filename.c_str(), handler.errorMsg.c_str());

		Debug() << buf;
	}
	
	source = handler.source;
#ifndef MKXPZ_RETRO
	needsRewind.clear();
#endif // MKXPZ_RETRO
}

void ALStream::stopStream()
{
#ifdef MKXPZ_RETRO
	threadTermReq = true;
#else
	threadTermReq.set();

	if (thread)
	{
		SDL_WaitThread(thread, 0);
		thread = 0;
		needsRewind.set();
	}
#endif // MKXPZ_RETRO

	/* Need to stop the source _after_ the thread has terminated,
	 * because it might have accidentally started it again before
	 * seeing the term request */
	AL::Source::stop(alSrc);

	procFrames = 0;
}

void ALStream::startStream(double offset)
{
	AL::Source::clearQueue(alSrc);

	preemptPause = false;
#ifdef MKXPZ_RETRO
	streamInited = false;
	sourceExhausted = false;
	threadTermReq = false;
#else
	streamInited.clear();
	sourceExhausted.clear();
	threadTermReq.clear();
#endif // MKXPZ_RETRO

	startOffset = offset;
	procFrames = offset * source->sampleRate();

#ifdef MKXPZ_RETRO
	renderInit();
#else
	thread = createSDLThread
		<ALStream, &ALStream::streamData>(this, threadName);
#endif // MKXPZ_RETRO
}

void ALStream::pauseStream()
{
#ifndef MKXPZ_RETRO
	SDL_LockMutex(pauseMut);
#endif // MKXPZ_RETRO

	if (AL::Source::getState(alSrc) != AL_PLAYING)
		preemptPause = true;
	else
		AL::Source::pause(alSrc);

#ifndef MKXPZ_RETRO
	SDL_UnlockMutex(pauseMut);
#endif // MKXPZ_RETRO
}

void ALStream::resumeStream()
{
#ifndef MKXPZ_RETRO
	SDL_LockMutex(pauseMut);
#endif // MKXPZ_RETRO

	if (preemptPause)
		preemptPause = false;
	else
		AL::Source::play(alSrc);

#ifndef MKXPZ_RETRO
	SDL_UnlockMutex(pauseMut);
#endif // MKXPZ_RETRO
}

void ALStream::checkStopped()
{
	/* This only concerns the scenario where
	 * state is still 'Playing', but the stream
	 * has already ended on its own (EOF, Error) */
	if (state != Playing)
		return;

	/* If streaming thread hasn't queued up
	 * buffers yet there's not point in querying
	 * the AL source */
	if (!streamInited)
		return;

	/* If alSrc isn't playing, but we haven't
	 * exhausted the data source yet, we're just
	 * having a buffer underrun */
	if (!sourceExhausted)
		return;

	if (AL::Source::getState(alSrc) == AL_PLAYING)
		return;

	stopStream();
	state = Stopped;
}

void ALStream::renderInit() {
	bool firstBuffer = true;
	ALDataSource::Status status;

	if (needsRewind)
		source->seekToOffset(startOffset);

	for (int i = 0; i < STREAM_BUFS; ++i)
	{
		if (threadTermReq)
			return;

		AL::Buffer::ID buf = alBuf[i];

		status = source->fillBuffer(buf);

		if (status == ALDataSource::Error)
			return;

		AL::Source::queueBuffer(alSrc, buf);

		if (firstBuffer)
		{
			resumeStream();

			firstBuffer = false;
#ifdef MKXPZ_RETRO
			streamInited = true;
#else
			streamInited.set();
#endif // MKXPZ_RETRO
		}

		if (threadTermReq)
			return;

		if (status == ALDataSource::EndOfStream)
		{
#ifdef MKXPZ_RETRO
			sourceExhausted = true;
#else
			sourceExhausted.set();
#endif // MKXPZ_RETRO
			break;
		}
	}
}

void ALStream::render() {
	ALint procBufs = AL::Source::getProcBufferCount(alSrc);

	while (procBufs--)
	{
		if (threadTermReq)
			break;

		AL::Buffer::ID buf = AL::Source::unqueueBuffer(alSrc);

#ifndef MKXPZ_NO_EXCEPTIONS // `unqueueBuffer` will abort on error if C++ exceptions are disabled so we only need to check if `buf == AL::Buffer::ID(0)` if C++ exceptions are enabled
#  ifdef MKXPZ_RETRO
		if (buf == AL::Buffer::ID(0))
		{
			mkxp_retro::log_printf(RETRO_LOG_ERROR, "Error unqueueing OpenAL buffer\n");
			std::abort();
		}
#  else
		/* If something went wrong, try again later */
		if (buf == AL::Buffer::ID(0))
			break;
#  endif // MKXPZ_RETRO
#endif // MKXPZ_NO_EXCEPTIONS

		if (buf == lastBuf)
		{
			/* Reset the processed sample count so
			 * querying the playback offset returns 0.0 again */
			procFrames = source->loopStartFrames();
			lastBuf = AL::Buffer::ID(0);
		}
		else
		{
			/* Add the frame count contained in this
			 * buffer to the total count */
			ALint bits = AL::Buffer::getBits(buf);
			ALint size = AL::Buffer::getSize(buf);
			ALint chan = AL::Buffer::getChannels(buf);

			if (bits != 0 && chan != 0)
				procFrames += ((size / (bits / 8)) / chan);
		}

		if (sourceExhausted)
			continue;

		ALDataSource::Status status = source->fillBuffer(buf);

		if (status == ALDataSource::Error)
		{
#ifdef MKXPZ_RETRO
			sourceExhausted = true;
#else
			sourceExhausted.set();
#endif // MKXPZ_RETRO
			return;
		}

		AL::Source::queueBuffer(alSrc, buf);

		/* In case of buffer underrun,
		 * start playing again */
		if (AL::Source::getState(alSrc) == AL_STOPPED)
			AL::Source::play(alSrc);

		/* If this was the last buffer before the data
		 * source loop wrapped around again, mark it as
		 * such so we can catch it and reset the processed
		 * sample count once it gets unqueued */
		if (status == ALDataSource::WrapAround)
			lastBuf = buf;

		if (status == ALDataSource::EndOfStream)
#ifdef MKXPZ_RETRO
			sourceExhausted = true;
#else
			sourceExhausted.set();
#endif // MKXPZ_RETRO
	}
}

#ifndef MKXPZ_RETRO
/* thread func */
void ALStream::streamData()
{
	if (threadTermReq)
		return;

	/* Fill up queue */
	renderInit();

	if (threadTermReq)
		return;

	/* Wait for buffers to be consumed, then
	 * refill and queue them up again */
	do
	{
		shState->rtData().syncPoint.passSecondarySync();

		render();

		if (threadTermReq)
			break;

		SDL_Delay(AUDIO_SLEEP);
	}
	while (!sourceExhausted);
}
#endif // MKXPZ_RETRO
