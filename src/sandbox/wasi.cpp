/*
** wasi.cpp
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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <zip.h>
#include "wasi.h"
#include "sandbox.h"
#include <mkxp-retro-ruby/mkxp-retro-ruby.h>
#include "../core.h"

extern unsigned char mkxp_retro_dist_zip[];
extern unsigned int mkxp_retro_dist_zip_len;

namespace mkxp_retro {
    retro_log_printf_t log_printf;
    retro_video_refresh_t video_refresh;
    retro_audio_sample_t audio_sample;
    retro_audio_sample_batch_t audio_sample_batch;
    retro_environment_t environment;
    retro_input_poll_t input_poll;
    retro_input_state_t input_state;
    struct retro_perf_callback perf;
}

//#define WASI_DEBUG(...) mkxp_retro::log_printf(RETRO_LOG_INFO, __VA_ARGS__)
#define WASI_DEBUG(...)

#define WASM_MEM(address) ((void *)&wasi->ruby->w2c_memory.data[address])

struct wasi_zip_handle *file_entry::zip_handle() {
    return (wasi_zip_handle *)handle;
}

struct wasi_zip_dir_handle *file_entry::zip_dir_handle() {
    return (wasi_zip_dir_handle *)handle;
}

struct wasi_zip_file_handle *file_entry::zip_file_handle() {
    return (wasi_zip_file_handle *)handle;
}

static std::vector<path_cache_entry_t> compute_path_cache(zip_t *zip) {
    if (zip == NULL) {
        return std::vector<path_cache_entry_t>();
    }

    std::vector<path_cache_entry_t> path_cache;

    zip_int64_t num_entries = zip_get_num_entries(zip, 0);

    for (zip_int64_t i = 0; i < num_entries; ++i) {
        std::string name(zip_get_name(zip, i, 0));
        if (!name.empty() && name.back() == '/') {
            name.pop_back();
        }
        u32 n_slashes = 0;
        for (u32 i = 0; i < name.length(); ++i) {
            if (name[i] == '/') {
                ++n_slashes;
            }
        }
        path_cache.push_back({n_slashes, name});
    }

    std::sort(path_cache.begin(), path_cache.end());

    return path_cache;
}

wasi_zip_container::wasi_zip_container() : source(NULL), zip(NULL), path_cache(compute_path_cache(zip)) {}

wasi_zip_container::wasi_zip_container(const char *path, zip_flags_t flags) : source(NULL), zip(zip_open(path, flags, NULL)), path_cache(compute_path_cache(zip)) {}

wasi_zip_container::wasi_zip_container(const void *buffer, zip_uint64_t length, zip_flags_t flags) : source(zip_source_buffer_create(buffer, length, 0, NULL)), zip(source == NULL ? NULL : zip_open_from_source(source, flags, NULL)), path_cache(compute_path_cache(zip)) {}

wasi_zip_container::~wasi_zip_container() {
    if (zip != NULL) {
        zip_close(zip);
        if (source != NULL) {
            zip_source_close(source);
        }
    }
}

wasi_zip_file_container::wasi_zip_file_container() : file(NULL) {}

wasi_zip_file_container::wasi_zip_file_container(wasi_zip_container &zip, zip_uint64_t index, zip_flags_t flags) : file(zip_fopen_index(zip.zip, index, flags)) {}

wasi_zip_file_container::~wasi_zip_file_container() {
    if (file != NULL) {
        zip_fclose(file);
    }
}

wasi_t::w2c_wasi__snapshot__preview1(std::shared_ptr<struct w2c_ruby> ruby, const char *game_path) : ruby(ruby), dist(new wasi_zip_container(mkxp_retro_dist_zip, mkxp_retro_dist_zip_len, ZIP_RDONLY)), game(new wasi_zip_container(game_path, ZIP_RDONLY)) {
    if (dist->zip == NULL || game->zip == NULL) {
        throw SandboxTrapException();
    }

    // Initialize WASI file descriptor table
    fdtable.push_back({.type = file_entry::STDIN});
    fdtable.push_back({.type = file_entry::STDOUT});
    fdtable.push_back({.type = file_entry::STDERR});

    struct wasi_zip_handle *dist_handle = (struct wasi_zip_handle *)std::malloc(sizeof(struct wasi_zip_handle));
    if (dist_handle == NULL) {
        throw SandboxOutOfMemoryException();
    }
    new(&dist_handle->zip) std::shared_ptr<struct wasi_zip_container>(dist);
    new(&dist_handle->path) std::string("/mkxp-retro-dist");
    fdtable.push_back({.type = file_entry::ZIP, .handle = dist_handle});

    struct wasi_zip_handle *game_handle = (struct wasi_zip_handle *)std::malloc(sizeof(struct wasi_zip_handle));
    if (game_handle == NULL) {
        throw SandboxOutOfMemoryException();
    }
    new(&game_handle->zip) std::shared_ptr<struct wasi_zip_container>(game);
    new(&game_handle->path) std::string("/mkxp-retro-game");
    fdtable.push_back({.type = file_entry::ZIP, .handle = game_handle});
}

wasi_t::~w2c_wasi__snapshot__preview1() {
    // Close all of the open WASI file descriptors
    for (size_t i = fdtable.size(); i > 0;) {
        deallocate_file_descriptor(--i);
    }
}

// Gets information about a file or directory at a certain path within a zip file.
static struct wasi_zip_stat wasi_zip_stat(zip_t *zip, const char *path, u32 path_len) {
    struct wasi_zip_stat info;
    zip_stat_t stat;

    char *buf = (char *)std::calloc(path_len + 2, 1);
    if (buf == NULL) {
        throw SandboxOutOfMemoryException();
    }
    std::strncpy(buf, path, path_len);

    // Trim trailing '/' from the path
    info.path_len = std::strlen(buf);
    char *ptr = buf + info.path_len;
    while (ptr != buf && *--ptr == '/') {
        *ptr = 0;
        --info.path_len;
    }

    // Trim leading '/' and '.' from the path
    info.path_offset = 0;
    ptr = buf;
    while (*ptr == '/' || *ptr == '.') {
        ++ptr;
        ++info.path_offset;
        --info.path_len;
    }

    if (*ptr == 0) {
        std::free(buf);
        info.exists = true;
        info.filetype = WASI_IFDIR;
        info.inode = -1;
        info.size = 0;
        info.mtime = 0;
        return info;
    }

    if (zip_stat(zip, ptr, 0, &stat) == 0) {
        std::free(buf);
        info.exists = true;
        info.filetype = WASI_IFREG;
        info.inode = stat.index;
        info.size = stat.size;
        info.mtime = stat.mtime * 1000000000L;
        return info;
    }

    ptr[std::strlen(ptr)] = '/';
    if (zip_stat(zip, ptr, 0, &stat) == 0) {
        std::free(buf);
        info.exists = true;
        info.filetype = WASI_IFDIR;
        info.inode = stat.index;
        info.size = stat.size;
        info.mtime = stat.mtime * 1000000000L;
        return info;
    }

    std::free(buf);
    info.exists = false;
    return info;
}

// Gets information about a file or directory at a certain index within a zip file.
static struct wasi_zip_stat wasi_zip_stat_entry(zip_t *zip, struct file_entry &entry) {
    struct wasi_zip_stat info;
    zip_stat_t stat;

    switch (entry.type) {
        case file_entry::ZIPDIR:
            if (zip_stat_index(zip, entry.zip_dir_handle()->index, 0, &stat) == 0) {
                info.exists = true;
                info.filetype = WASI_IFDIR;
                info.inode = stat.index;
                info.size = stat.size;
                info.mtime = stat.mtime * 1000000000L;
            } else {
                info.exists = false;
            }
            return info;

        case file_entry::ZIPFILE:
            if (zip_stat_index(zip, entry.zip_file_handle()->index, 0, &stat) == 0) {
                info.exists = true;
                info.filetype = WASI_IFREG;
                info.inode = stat.index;
                info.size = stat.size;
                info.mtime = stat.mtime * 1000000000L;
            } else {
                info.exists = false;
            }
            return info;

        default:
            info.exists = false;
            return info;
    }
}

u32 wasi_t::allocate_file_descriptor() {
    if (vacant_fds.empty()) {
        u32 fd = fdtable.size();
        fdtable.push_back({.type = file_entry::VACANT});
        return fd;
    } else {
        u32 fd = vacant_fds.back();
        vacant_fds.pop_back();
        return fd;
    }
}

void wasi_t::deallocate_file_descriptor(u32 fd) {
    switch (fdtable[fd].type) {
        case file_entry::ZIP:
        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            std::free(fdtable[fd].handle);
            break;

        default:
            break;
    }

    if (!fdtable.empty() && fd == fdtable.size() - 1) {
        fdtable.pop_back();
    } else {
        fdtable[fd].type = file_entry::VACANT;
        vacant_fds.push_back(fd);
    }
}

extern "C" u32 w2c_wasi__snapshot__preview1_args_get(wasi_t *wasi, usize argv, usize argv_buf) {
    WASI_DEBUG("args_get()\n");
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_args_sizes_get(wasi_t *wasi, usize argc, usize argv_buf_size) {
    WASI_DEBUG("args_sizes_get(0x%08x, 0x%08x)\n", argc, argv_buf_size);
    WASM_SET(u32, argc, 0);
    WASM_SET(u32, argv_buf_size, 0);
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_clock_res_get(wasi_t *wasi, u32 id, usize result) {
    WASI_DEBUG("clock_res_get(%u)\n", id);
    WASM_SET(u64, result, 1000L);
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_clock_time_get(wasi_t *wasi, u32 id, u64 precision, usize result) {
    WASI_DEBUG("clock_time_get(%u, %lu)\n", id, precision);
    WASM_SET(u64, result, mkxp_retro::perf.get_time_usec != nullptr ? mkxp_retro::perf.get_time_usec() * 1000L : 0);
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_environ_get(wasi_t *wasi, usize env, usize env_buf) {
    WASI_DEBUG("environ_get(0x%08x, 0x%08x)\n", env, env_buf);
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_environ_sizes_get(wasi_t *wasi, usize env_size, usize env_buf_size) {
    WASI_DEBUG("environ_sizes_get()\n");
    WASM_SET(u32, env_size, 0);
    WASM_SET(u32, env_buf_size, 0);
    return WASI_ESUCCESS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_advise(wasi_t *wasi, u32 fd, u64 offset, u64 len, u32 advice) {
    WASI_DEBUG("fd_advise(%u, %lu, %lu, 0x%08x)\n", fd, offset, len, advice);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_close(wasi_t *wasi, u32 fd) {
    WASI_DEBUG("fd_close(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIP:
            return WASI_EINVAL;

        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            wasi->deallocate_file_descriptor(fd);
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_datasync(wasi_t *wasi, u32 fd) {
    WASI_DEBUG("fd_datasync(%u)\n", fd);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_fdstat_get(wasi_t *wasi, u32 fd, usize result) {
    WASI_DEBUG("fd_fdstat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPFILE:
            WASM_SET(u8, result, WASI_IFCHR); // fs_filetype
            WASM_SET(u16, result + 2, 0); // fs_flags
            WASM_SET(u64, result + 8, WASI_FD_READ | WASI_FD_WRITE | WASI_FD_FILESTAT_GET); // fs_rights_base
            WASM_SET(u64, result + 16, 0); // fs_rights_inheriting
            return WASI_ESUCCESS;

        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            WASM_SET(u8, result, WASI_IFDIR); // fs_filetype
            WASM_SET(u16, result + 2, 0); // fs_flags
            WASM_SET(u64, result + 8, WASI_PATH_OPEN | WASI_FD_READDIR | WASI_PATH_FILESTAT_GET | WASI_FD_FILESTAT_GET); // fs_rights_base
            WASM_SET(u64, result + 16, 0); // fs_rights_inheriting
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_fdstat_set_flags(wasi_t *wasi, u32 fd, u32 flags) {
    WASI_DEBUG("fd_fdstat_set_flags(%u, %u)\n", fd, flags);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIP:
        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_filestat_get(wasi_t *wasi, u32 fd, usize result) {
    WASI_DEBUG("fd_filestat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
            WASM_SET(u64, result, fd); // dev
            WASM_SET(u64, result + 8, 0); // ino
            WASM_SET(u8, result + 16, WASI_IFCHR); // filetype
            WASM_SET(u32, result + 24, 1); // nlink
            WASM_SET(u64, result + 32, 0); // size
            WASM_SET(u64, result + 40, 0); // atim
            WASM_SET(u64, result + 48, 0); // mtim
            WASM_SET(u64, result + 56, 0); // ctim
            return WASI_ESUCCESS;

        case file_entry::ZIP:
            {
                struct wasi_zip_stat info = wasi_zip_stat(wasi->fdtable[fd].zip_handle()->zip->zip, "", 0);
                if (!info.exists) {
                    return WASI_ENOENT;
                }
                WASM_SET(u64, result, fd); // dev
                WASM_SET(u64, result + 8, info.inode); // ino
                WASM_SET(u8, result + 16, info.filetype); // filetype
                WASM_SET(u32, result + 24, 1); // nlink
                WASM_SET(u64, result + 32, info.size); // size
                WASM_SET(u64, result + 40, info.mtime); // atim
                WASM_SET(u64, result + 48, info.mtime); // mtim
                WASM_SET(u64, result + 56, info.mtime); // ctim
                return WASI_ESUCCESS;
            }

        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            {
                u32 parent_fd = wasi->fdtable[fd].type == file_entry::ZIPDIR
                    ? wasi->fdtable[fd].zip_dir_handle()->parent_fd
                    : wasi->fdtable[fd].zip_file_handle()->parent_fd;
                struct wasi_zip_stat info = wasi_zip_stat_entry(
                    wasi->fdtable[parent_fd].zip_handle()->zip->zip,
                    wasi->fdtable[fd]
                );
                if (!info.exists) {
                    return WASI_ENOENT;
                }
                WASM_SET(u64, result, parent_fd); // dev
                WASM_SET(u64, result + 8, info.inode); // ino
                WASM_SET(u8, result + 16, info.filetype); // filetype
                WASM_SET(u32, result + 24, 1); // nlink
                WASM_SET(u64, result + 32, info.size); // size
                WASM_SET(u64, result + 40, info.mtime); // atim
                WASM_SET(u64, result + 48, info.mtime); // mtim
                WASM_SET(u64, result + 56, info.mtime); // ctim
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_filestat_set_size(wasi_t *wasi, u32 fd, u64 size) {
    WASI_DEBUG("fd_filestat_set_size(%u, %lu)\n", fd, size);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
            return WASI_EINVAL;

        default:
            return WASI_ENOSYS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_pread(wasi_t *wasi, u32 fd, usize iovs, u32 iovs_len, u64 offset, usize result) {
    WASI_DEBUG("fd_pread(%u, 0x%08x (%u), %lu)\n", fd, iovs, iovs_len, offset);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_prestat_dir_name(wasi_t *wasi, u32 fd, usize path, u32 path_len) {
    WASI_DEBUG("fd_prestat_dir_name(%u, 0x%x (%u))\n", fd, path, path_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::ZIP:
            std::strncpy((char *)WASM_MEM(path), wasi->fdtable[fd].zip_handle()->path.c_str(), path_len);
            return WASI_ESUCCESS;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            return WASI_EINVAL;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_prestat_get(wasi_t *wasi, u32 fd, usize result) {
    WASI_DEBUG("fd_prestat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::ZIP:
            WASM_SET(u32, result, 0);
            WASM_SET(u32, result + 4, wasi->fdtable[fd].zip_handle()->path.length());
            return WASI_ESUCCESS;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            return WASI_EINVAL;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_pwrite(wasi_t *wasi, u32 fd, usize iovs, u32 iovs_len, u64 offset, usize result) {
    WASI_DEBUG("fd_pwrite(%u, 0x%08x (%u), %lu)\n", fd, iovs, iovs_len, offset);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_read(wasi_t *wasi, u32 fd, usize iovs, u32 iovs_len, usize result) {
    WASI_DEBUG("fd_read(%u, 0x%08x (%u))\n", fd, iovs, iovs_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
            WASM_SET(u32, result, 0);
            return WASI_ESUCCESS;

        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            return WASI_EINVAL;

        case file_entry::ZIPFILE:
            {
                u32 size = 0;
                while (iovs_len > 0) {
                    zip_int64_t n = zip_fread(wasi->fdtable[fd].zip_file_handle()->zip_file_handle.file, WASM_MEM(WASM_GET(u32, iovs)), WASM_GET(u32, iovs + 4));
                    if (n < 0) return WASI_EIO;
                    size += n;
                    iovs += 8;
                    --iovs_len;
                }
                WASM_SET(u32, result, size);
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_readdir(wasi_t *wasi, u32 fd, usize buf, u32 buf_len, u64 cookie, usize result) {
    WASI_DEBUG("fd_readdir(%u, 0x%08x (%u), %lu)\n", fd, buf, buf_len, cookie);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPFILE:
            return WASI_EINVAL;

        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            {
                usize original_buf = buf;
                std::string *prefix = wasi->fdtable[fd].type == file_entry::ZIP ? NULL : &wasi->fdtable[fd].zip_dir_handle()->path;
                if (prefix != NULL && *prefix == "/") {
                    prefix = NULL;
                }

                u32 n_slashes = 0;
                    if (prefix != NULL) {
                    for (u32 i = 0; i < prefix->length(); ++i) {
                        if ((*prefix)[i] == '/') {
                            ++n_slashes;
                        }
                    }
                }

                std::shared_ptr<struct wasi_zip_container> zip = wasi->fdtable[fd].type == file_entry::ZIP
                    ? wasi->fdtable[fd].zip_handle()->zip
                    : wasi->fdtable[wasi->fdtable[fd].zip_dir_handle()->parent_fd].zip_handle()->zip;

                auto it = std::lower_bound(
                    zip->path_cache.begin(),
                    zip->path_cache.end(),
                    prefix == NULL ? std::make_pair(n_slashes, "") : std::make_pair(n_slashes, *prefix)
                );

                it += cookie;
                while (it != zip->path_cache.end() && it->first == n_slashes && (prefix == NULL || std::strncmp(it->second.c_str(), prefix->c_str(), prefix->length()) == 0)) {
                    ++cookie;

                    struct wasi_zip_stat info = wasi_zip_stat(
                        wasi->fdtable[fd].type == file_entry::ZIP
                            ? wasi->fdtable[fd].zip_handle()->zip->zip
                            : wasi->fdtable[wasi->fdtable[fd].zip_dir_handle()->parent_fd].zip_handle()->zip->zip,
                        it->second.c_str(),
                        it->second.length()
                    );
                    if (!info.exists) {
                        ++it;
                        continue;
                    }

                    u32 suffix_length = it->second.length() - (prefix == NULL ? 0 : prefix->length());

                    if (buf - original_buf + 8 > buf_len) {
                        WASM_SET(u32, result, buf - original_buf);
                        return WASI_ESUCCESS;
                    }
                    WASM_SET(u64, buf, cookie);
                    buf += 8;

                    if (buf - original_buf + 8 > buf_len) {
                        WASM_SET(u32, result, buf - original_buf);
                        return WASI_ESUCCESS;
                    }
                    WASM_SET(u64, buf, info.inode);
                    buf += 8;

                    if (buf - original_buf + 4 > buf_len) {
                        WASM_SET(u32, result, buf - original_buf);
                        return WASI_ESUCCESS;
                    }
                    WASM_SET(u32, buf, suffix_length);
                    buf += 4;

                    if (buf - original_buf + 4 > buf_len) {
                        WASM_SET(u32, result, buf - original_buf);
                        return WASI_ESUCCESS;
                    }
                    WASM_SET(u8, buf, info.filetype);
                    buf += 4;

                    u32 len = std::min(suffix_length, original_buf + buf_len - buf);
                    std::memcpy(WASM_MEM(buf), it->second.c_str() + (prefix == NULL ? 0 : prefix->length()), len);
                    buf += len;

                    ++it;
                }

                WASM_SET(u32, result, buf - original_buf);
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_renumber(wasi_t *wasi, u32 fd, u32 to) {
    WASI_DEBUG("fd_renumber(%u, %u)\n", fd, to);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIP:
            return WASI_EINVAL;

        case file_entry::ZIPFILE:
        case file_entry::ZIPDIR:
            break;
    }

    if (fd == to) {
        return WASI_ESUCCESS;
    }

    if (to >= wasi->fdtable.size()) return WASI_EBADF;

    switch (wasi->fdtable[to].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIP:
            return WASI_EINVAL;

        case file_entry::ZIPDIR:
        case file_entry::ZIPFILE:
            wasi->deallocate_file_descriptor(to);
            if (to == wasi->fdtable.size()) {
                wasi->fdtable.push_back(wasi->fdtable[fd]);
            } else {
                wasi->fdtable[to] = wasi->fdtable[fd];
            }
            if (!wasi->fdtable.empty() && fd == wasi->fdtable.size() - 1) {
                wasi->fdtable.pop_back();
            } else {
                wasi->fdtable[fd].type = file_entry::VACANT;
                wasi->vacant_fds.push_back(fd);
            }
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_seek(wasi_t *wasi, u32 fd, u64 offset, u32 whence, usize result) {
    WASI_DEBUG("fd_seek(%u, %lu, %u)\n", fd, offset, whence);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_sync(wasi_t *wasi, u32 fd) {
    WASI_DEBUG("fd_sync(%u)\n", fd);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_tell(wasi_t *wasi, u32 fd, usize result) {
    WASI_DEBUG("fd_tell(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            return WASI_EINVAL;

        case file_entry::ZIPFILE:
            WASM_SET(u64, result, zip_ftell(wasi->fdtable[fd].zip_file_handle()->zip_file_handle.file));
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_fd_write(wasi_t *wasi, u32 fd, usize iovs, u32 iovs_len, usize result) {
    WASI_DEBUG("fd_write(%u, 0x%08x (%u))\n", fd, iovs, iovs_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
            WASM_SET(u32, result, 0);
            return WASI_ESUCCESS;

        case file_entry::STDOUT:
        case file_entry::STDERR:
            {
                u32 size = 0;
                while (iovs_len > 0) {
                    char *buf = (char *)std::calloc(WASM_GET(u32, iovs + 4) + 1, 1);
                    if (buf == NULL) {
                        throw SandboxOutOfMemoryException();
                    }
                    std::strncpy(buf, (char *)WASM_MEM(WASM_GET(u32, iovs)), WASM_GET(u32, iovs + 4));
                    for (char *ptr = std::strtok(buf, "\n"); ptr != NULL; ptr = std::strtok(NULL, "\n")) {
                        mkxp_retro::log_printf(wasi->fdtable[fd].type == file_entry::STDOUT ? RETRO_LOG_INFO : RETRO_LOG_ERROR, "%s\n", ptr);
                    }
                    std::free(buf);
                    size += WASM_GET(u32, iovs + 4);
                    iovs += 8;
                    --iovs_len;
                }
                WASM_SET(u32, result, size);
                return WASI_ESUCCESS;
            }

        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            return WASI_EINVAL;

        case file_entry::ZIPFILE:
            return WASI_EROFS;
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_create_directory(wasi_t *wasi, u32 fd, usize path, u32 path_len) {
    WASI_DEBUG("path_create_directory(%u, \"%.*s\")\n", fd, path_len, (char *)WASM_MEM(path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_filestat_get(wasi_t *wasi, u32 fd, u32 flags, usize path, u32 path_len, usize result) {
    WASI_DEBUG("path_filestat_get(%u, %u, \"%.*s\")\n", fd, flags, path_len, (char *)WASM_MEM(path));

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPFILE:
            return WASI_EINVAL;

        case file_entry::ZIP:
            {
                struct wasi_zip_stat info = wasi_zip_stat(wasi->fdtable[fd].zip_handle()->zip->zip, (char *)WASM_MEM(path), path_len);
                if (!info.exists) {
                    return WASI_ENOENT;
                }
                WASM_SET(u64, result, fd); // dev
                WASM_SET(u64, result + 8, info.inode); // ino
                WASM_SET(u8, result + 16, info.filetype); // filetype
                WASM_SET(u32, result + 24, 1); // nlink
                WASM_SET(u64, result + 32, info.size); // size
                WASM_SET(u64, result + 40, info.mtime); // atim
                WASM_SET(u64, result + 48, info.mtime); // mtim
                WASM_SET(u64, result + 56, info.mtime); // ctim
                return WASI_ESUCCESS;
            }

        case file_entry::ZIPDIR:
            {
                char *buf = (char *)std::calloc(wasi->fdtable[fd].zip_dir_handle()->path.length() + path_len + 1, 1);
                if (buf == NULL) {
                    throw SandboxOutOfMemoryException();
                }
                std::strcpy(buf, wasi->fdtable[fd].zip_dir_handle()->path.c_str());
                std::strncat(buf, (char *)WASM_MEM(path), path_len);
                struct wasi_zip_stat info = wasi_zip_stat(wasi->fdtable[wasi->fdtable[fd].zip_dir_handle()->parent_fd].zip_handle()->zip->zip, buf, std::strlen(buf));
                if (!info.exists) {
                    return WASI_ENOENT;
                }
                std::free(buf);
                WASM_SET(u64, result, wasi->fdtable[fd].zip_dir_handle()->parent_fd); // dev
                WASM_SET(u64, result + 8, info.inode); // ino
                WASM_SET(u8, result + 16, info.filetype); // filetype
                WASM_SET(u32, result + 24, 1); // nlink
                WASM_SET(u64, result + 32, info.size); // size
                WASM_SET(u64, result + 40, info.mtime); // atim
                WASM_SET(u64, result + 48, info.mtime); // mtim
                WASM_SET(u64, result + 56, info.mtime); // ctim
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_filestat_set_times(wasi_t *wasi, u32 fd, u32 flags, usize path, u32 path_len, u64 atim, u64 ntim, u32 fst_flags) {
    WASI_DEBUG("path_filestat_set_times(%u, %u, \"%.*s\", %lu, %lu, %u)\n", fd, flags, path_len, (char *)WASM_MEM(path), atim, ntim, fst_flags);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_link(wasi_t *wasi, u32 old_fd, u32 old_flags, usize old_path, u32 old_path_len, u32 new_fd, usize new_path, u32 new_path_len) {
    WASI_DEBUG("path_link(%u, %u, \"%.*s\", %u, \"%.*s\")\n", old_fd, old_flags, old_path_len, (char *)WASM_MEM(old_path), new_fd, new_path_len, (char *)WASM_MEM(new_path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_open(wasi_t *wasi, u32 fd, u32 dirflags, usize path, u32 path_len, u32 oflags, u64 fs_base_rights, u64 fs_rights_inheriting, u32 fdflags, usize result) {
    WASI_DEBUG("path_open(%u, %u, \"%.*s\", %u, %lu, %lu, %u)\n", fd, dirflags, path_len, (char *)WASM_MEM(path), oflags, fs_base_rights, fs_rights_inheriting, fdflags);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case file_entry::VACANT:
            return WASI_EBADF;

        case file_entry::STDIN:
        case file_entry::STDOUT:
        case file_entry::STDERR:
        case file_entry::ZIPFILE:
            return WASI_EINVAL;

        case file_entry::ZIP:
        case file_entry::ZIPDIR:
            {
                char *path_buf = (char *)std::calloc(wasi->fdtable[fd].type == file_entry::ZIPDIR ? wasi->fdtable[fd].zip_dir_handle()->path.length() + path_len + 1 : path_len + 1, 1);
                if (path_buf == NULL) {
                    throw SandboxOutOfMemoryException();
                }

                if (wasi->fdtable[fd].type == file_entry::ZIPDIR) {
                    std::strcpy(path_buf, wasi->fdtable[fd].zip_dir_handle()->path.c_str());
                }
                std::strncat(path_buf, (char *)WASM_MEM(path), path_len);

                struct wasi_zip_stat info = wasi_zip_stat(
                    wasi->fdtable[fd].type == file_entry::ZIP
                        ? wasi->fdtable[fd].zip_handle()->zip->zip
                        : wasi->fdtable[wasi->fdtable[fd].zip_dir_handle()->parent_fd].zip_handle()->zip->zip,
                    path_buf,
                    std::strlen(path_buf)
                );
                if (!info.exists) {
                    return WASI_ENOENT;
                }

                if (info.filetype == WASI_IFDIR) {
                    path_buf[info.path_offset + info.path_len] = 0;
                    std::string new_path(path_buf + info.path_offset);
                    std::free(path_buf);
                    new_path.push_back('/');

                    struct wasi_zip_dir_handle *handle = (struct wasi_zip_dir_handle *)std::malloc(sizeof(struct wasi_zip_dir_handle));
                    if (handle == NULL) {
                        throw SandboxOutOfMemoryException();
                    }
                    handle->index = info.inode;
                    new(&handle->path) std::string(new_path);
                    handle->parent_fd = wasi->fdtable[fd].type == file_entry::ZIPDIR ? wasi->fdtable[fd].zip_dir_handle()->parent_fd : fd;

                    u32 new_fd = wasi->allocate_file_descriptor();
                    WASM_SET(u32, result, new_fd);
                    wasi->fdtable[new_fd] = {.type = file_entry::ZIPDIR, .handle = handle};
                } else {
                    std::free(path_buf);

                    struct wasi_zip_file_handle *handle = (struct wasi_zip_file_handle *)std::malloc(sizeof(struct wasi_zip_file_handle));
                    if (handle == NULL) {
                        throw SandboxOutOfMemoryException();
                    }
                    handle->index = info.inode;
                    new(&handle->zip_file_handle) wasi_zip_file_container(
                        wasi->fdtable[fd].type == file_entry::ZIP
                            ? *wasi->fdtable[fd].zip_handle()->zip
                            : *wasi->fdtable[wasi->fdtable[fd].zip_dir_handle()->parent_fd].zip_handle()->zip,
                        info.inode,
                        0
                    );
                    handle->parent_fd = wasi->fdtable[fd].type == file_entry::ZIPDIR ? wasi->fdtable[fd].zip_dir_handle()->parent_fd : fd;

                    u32 new_fd = wasi->allocate_file_descriptor();
                    WASM_SET(u32, result, new_fd);
                    wasi->fdtable[new_fd] = {.type = file_entry::ZIPFILE, .handle = handle};
                }

                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_readlink(wasi_t *wasi, u32 fd, usize path, u32 path_len, usize buf, u32 buf_len, usize result) {
    WASI_DEBUG("path_readlink(%u, \"%.*s\", 0x%08x (%u))\n", fd, path_len, (char *)WASM_MEM(path), buf, buf_len);
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_remove_directory(wasi_t *wasi, u32 fd, usize path, u32 path_len) {
    WASI_DEBUG("path_remove_directory(%u, \"%.*s\")\n", fd, path_len, (char *)WASM_MEM(path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_rename(wasi_t *wasi, u32 fd, usize old_path, u32 old_path_len, u32 new_fd, usize new_path, u32 new_path_len) {
    WASI_DEBUG("path_rename(%u, \"%.*s\", %u, \"%.*s\")\n", fd, old_path_len, (char *)WASM_MEM(old_path), new_fd, new_path_len, (char *)WASM_MEM(new_path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_symlink(wasi_t *wasi, usize old_path, u32 old_path_len, u32 fd, usize new_path, u32 new_path_len) {
    WASI_DEBUG("path_symlink(\"%.*s\", %u, \"%.*s\")\n", old_path_len, (char *)WASM_MEM(old_path), fd, new_path_len, (char *)WASM_MEM(new_path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_path_unlink_file(wasi_t *wasi, u32 fd, usize path, u32 path_len) {
    WASI_DEBUG("path_unlink_file(%u, \"%.*s\")\n", fd, path_len, (char *)WASM_MEM(path));
    return WASI_ENOSYS;
}

extern "C" u32 w2c_wasi__snapshot__preview1_poll_oneoff(wasi_t *wasi, usize in, usize out, u32 nsubscriptions, usize result) {
    WASI_DEBUG("poll_oneoff(0x%08x, 0x%08x, %u)\n", in, out, nsubscriptions);
    return WASI_ENOSYS;
}

extern "C" void w2c_wasi__snapshot__preview1_proc_exit(wasi_t *wasi, u32 rval) {
    WASI_DEBUG("proc_exit(%u)\n", rval);
    throw SandboxTrapException();
}

extern "C" u32 w2c_wasi__snapshot__preview1_random_get(wasi_t *wasi, usize buf, u32 buf_len) {
    WASI_DEBUG("random_get(0x%08x (%u))\n", buf, buf_len);

    static std::random_device dev;
    static std::mt19937 rng(dev());

    static u32 rng_buffer;
    static u32 rng_buffer_size = 0;

    while (buf_len > 0) {
        if (rng_buffer_size == 0) {
            rng_buffer = rng();
            rng_buffer_size = 4;
        } else {
            u32 n = std::min(rng_buffer_size, buf_len);
            std::memcpy(WASM_MEM(buf), (u8 *)&rng_buffer + (4 - n), n);
            buf += n;
            buf_len -= n;
            rng_buffer_size -= n;
        }
    }
    return WASI_ESUCCESS;
}
