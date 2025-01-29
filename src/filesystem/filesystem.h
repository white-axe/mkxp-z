/*
** filesystem.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef MKXPZ_RETRO
#  include <memory>
#endif // MKXPZ_RETRO
#include <physfs.h>
#include <SDL_rwops.h>
#include <string>

#include "filesystemImpl.h"

namespace mkxp_fs = filesystemImpl;

struct FileSystemPrivate;
class SharedFontState;

class FileSystem
{
public:
	enum OpenMode
	{
		Read,
		Write,
		Append,
	};

	struct File
	{
		friend class FileSystem;

	private:
		PHYSFS_File *inner;
		std::string _path;

	public:
		File(const struct File &) = delete;
		inline File(FileSystem &fs, const char *filename, OpenMode mode) {
			_path = fs.normalize(filename, false, false);
			switch (mode) {
				case OpenMode::Read:
					inner = PHYSFS_openRead(_path.c_str());
					break;
				case OpenMode::Write:
					inner = PHYSFS_openWrite(_path.c_str());
					break;
				case OpenMode::Append:
					inner = PHYSFS_openAppend(_path.c_str());
					break;
			}
		}
		inline ~File() {
			PHYSFS_close(inner);
		}
		inline const char *path() {
			return _path.c_str();
		}
		inline PHYSFS_File *get() {
			return inner;
		}
		inline PHYSFS_File *operator->() {
			return get();
		}
		inline PHYSFS_File &operator*() {
			return *get();
		}
	};

	FileSystem(const char *argv0,
	           bool allowSymlinks);
	~FileSystem();

	void addPath(const char *path, const char *mountpoint = 0, bool reload = false);
    void removePath(const char *path, bool reload = false);

	/* Call these after the last 'addPath()' */
	void createPathCache();
    
    void reloadPathCache();

	/* Scans "Fonts/" and creates inventory of
	 * available font assets */
	void initFontSets(SharedFontState &sfs);

	struct OpenHandler
	{
		/* Try to read and interpret data provided from ops.
		 * If data cannot be parsed, return false, otherwise true.
		 * Can be called multiple times until a parseable file is found.
		 * It's the handler's responsibility to close every passed
		 * ops structure, even when data could not be parsed.
		 * After this function returns, ops becomes invalid, so don't take
		 * references to it. Instead, copy the structure without closing
		 * if you need to further read from it later. */
		virtual bool tryRead(
#ifdef MKXPZ_RETRO
			std::shared_ptr<struct FileSystem::File> ops,
#else
			SDL_RWops &ops,
#endif // MKXPZ_RETRO
			const char *ext
		) = 0;
	};

	void openRead(OpenHandler &handler,
	              const char *filename);

	/* Circumvents extension supplementing */
	void openReadRaw(SDL_RWops &ops,
	                 const char *filename,
	                 bool freeOnClose = false);

	std::string normalize(const char *pathname, bool preferred, bool absolute);

	/* Does not perform extension supplementing */
	bool exists(const char *filename);

	const char *desensitize(const char *filename);

	bool enumerate(const char *path, PHYSFS_EnumerateCallback callback, void *data);

private:
	FileSystemPrivate *p;
};

extern const Uint32 SDL_RWOPS_PHYSFS;

#endif // FILESYSTEM_H
