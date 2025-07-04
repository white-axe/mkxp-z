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
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <random>
#include <sstream>
#include <utility>
#include <mkxp-sandbox-ruby.h>
#include "filesystem.h"
#include "core.h"
#include "wasi.h"
#include "binding-base.h"
#include "sandbox-serial-util.h"

using namespace mkxp_sandbox;

//#define WASI_DEBUG(...) mkxp_retro::log_printf(RETRO_LOG_INFO, __VA_ARGS__)
#define WASI_DEBUG(...)

static inline size_t strlen_safe(const char *str, size_t max_length) {
    const char *ptr = (const char *)std::memchr(str, 0, max_length);
    return ptr == nullptr ? max_length : ptr - str;
}

struct fs_dir *wasi_file_entry::dir_handle() const noexcept {
    return (struct fs_dir *)handle;
}

struct fs_file *wasi_file_entry::file_handle() const noexcept {
    return (struct fs_file *)handle;
}

wasi_t::w2c_wasi__snapshot__preview1(std::shared_ptr<struct w2c_ruby> ruby) : ruby(ruby), prng_buffer_size(0) {
    // Initialize PRNG
    static_assert(sizeof(unsigned int) == sizeof(uint32_t), "unsigned int should be 32 bits");
    static std::random_device dev;
    prng_state = dev();
    prng_state <<= 32U;
    prng_state |= dev();
    std::memset(prng_buffer, 0, 4);

    // Initialize WASI file descriptor table
    fdtable.push_back({nullptr, wasi_fd_type::STDIN});
    fdtable.push_back({nullptr, wasi_fd_type::STDOUT});
    fdtable.push_back({nullptr, wasi_fd_type::STDERR});
    fdtable.push_back({new fs_dir {"/Game", 0, true}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/Save", 0, true}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/System", 0, false}, wasi_fd_type::FS});
    fdtable.push_back({new fs_dir {"/Dist", 0, false}, wasi_fd_type::FS});
}

wasi_t::~w2c_wasi__snapshot__preview1() {
    // Close all of the open WASI file descriptors
    for (uint32_t i = fdtable.size(); i > 0;) {
        deallocate_file_descriptor(--i);
    }
}

struct fs_enumerate_data {
    wasi_t *wasi;
    uint32_t fd;
    wasm_ptr_t original_buf;
    wasm_ptr_t buf;
    uint32_t buf_len;
    uint64_t initial_cookie;
    uint64_t cookie;
    wasm_ptr_t result;
};

uint32_t wasi_t::allocate_file_descriptor(enum wasi_fd_type type, void *handle) {
    if (vacant_fds.empty()) {
        if (fdtable.size() >= UINT32_MAX) {
            MKXPZ_THROW(std::bad_alloc());
        }
        uint32_t fd = fdtable.size();
        fdtable.push_back({handle, type});
        return fd;
    } else {
        uint32_t fd = vacant_fds.minimum();
        vacant_fds.pop_minimum();
        fdtable[fd].handle = handle;
        fdtable[fd].type = type;
        return fd;
    }
}

void wasi_t::deallocate_file_descriptor(uint32_t fd) {
    if (fdtable[fd].type == wasi_fd_type::VACANT) {
        return;
    }

    if (fdtable[fd].handle != nullptr) {
        switch (fdtable[fd].type) {
            case wasi_fd_type::FS:
            case wasi_fd_type::FSDIR:
                delete fdtable[fd].dir_handle();
                break;
            case wasi_fd_type::FSFILE:
                delete fdtable[fd].file_handle();
                break;
            default:
                break;
        }
    }

    if (fd == fdtable.size() - 1) {
        fdtable.pop_back();
        while (!fdtable.empty() && fdtable.back().type == wasi_fd_type::VACANT) {
            assert(!vacant_fds.empty() && vacant_fds.maximum() == fdtable.size() - 1);
            vacant_fds.pop_maximum();
            fdtable.pop_back();
        }
    } else {
        fdtable[fd] = {nullptr, wasi_fd_type::VACANT};
        vacant_fds.push(fd);
    }
}

wasm_size_t wasi_t::strlen(wasm_ptr_t address) const noexcept {
    return sandbox_strlen(*ruby, address);
}

void wasi_t::strcpy(wasm_ptr_t dst_address, const char *src) const noexcept {
    sandbox_strcpy(*ruby, dst_address, src);
}

void wasi_t::strncpy_s(wasm_ptr_t dst_address, const char *src, wasm_size_t max_size) const noexcept {
    sandbox_strncpy_s(*ruby, dst_address, src, max_size);
}

struct mkxp_sandbox::sandbox_str_guard wasi_t::str(wasm_ptr_t address) const noexcept {
    return sandbox_str(*ruby, address);
}

bool wasi_t::sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const {
    if (!::sandbox_serialize(prng_state, data, max_size)) return false;
    if (max_size < 4) return false;
    std::memcpy(data, prng_buffer, 4);
    data = (uint8_t *)data + 4;
    max_size -= 4;
    if (!::sandbox_serialize((uint8_t)prng_buffer_size, data, max_size)) return false;

    if (!::sandbox_serialize((uint32_t)fdtable.size(), data, max_size)) return false;

    uint32_t num_free_handles = 0;

    for (const struct wasi_file_entry &entry : fdtable) {
        if (entry.type == wasi_fd_type::FSDIR) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)1, data, max_size)) return false;
            if (!::sandbox_serialize(entry.dir_handle()->root, data, max_size)) return false;
            if (!::sandbox_serialize(entry.dir_handle()->path, data, max_size)) return false;
        } else if (entry.type == wasi_fd_type::FSFILE) {
            if (num_free_handles > 0) {
                if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
                if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
                num_free_handles = 0;
            }
            if (!::sandbox_serialize((uint8_t)2, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->root, data, max_size)) return false;
            if (!::sandbox_serialize(entry.file_handle()->file.path(), data, max_size)) return false;
            {
                PHYSFS_File *file = entry.file_handle()->file.get();
                if (!::sandbox_serialize(file == nullptr ? (int64_t)0 : std::max((int64_t)0, (int64_t)PHYSFS_tell(file)), data, max_size)) return false;
            }
            {
                PHYSFS_File *file = entry.file_handle()->file.get_write();
                if (!::sandbox_serialize(file == nullptr ? (int64_t)0 : std::max((int64_t)0, (int64_t)PHYSFS_tell(file)), data, max_size)) return false;
            }
        } else {
            ++num_free_handles;
        }
    }
    if (num_free_handles > 0) {
        if (!::sandbox_serialize((uint8_t)0, data, max_size)) return false;
        if (!::sandbox_serialize(num_free_handles, data, max_size)) return false;
        num_free_handles = 0;
    }

    return true;
}

bool wasi_t::sandbox_deserialize(const void *&data, mkxp_sandbox::wasm_size_t &max_size) {
    if (!::sandbox_deserialize(prng_state, data, max_size)) return false;
    if (max_size < 4) return false;
    std::memcpy(prng_buffer, data, 4);
    data = (uint8_t *)data + 4;
    max_size -= 4;
    {
        uint8_t size;
        if (!::sandbox_deserialize(size, data, max_size)) return false;
        prng_buffer_size = size % 5;
    }

    uint32_t size;
    if (!::sandbox_deserialize(size, data, max_size)) return false;
    if (size < fdtable.size() && (fdtable[size].type == wasi_fd_type::FS || fdtable[size].type == wasi_fd_type::STDIN || fdtable[size].type == wasi_fd_type::STDOUT || fdtable[size].type == wasi_fd_type::STDERR)) return false;

    for (uint32_t i = fdtable.size(); i > size;) {
        deallocate_file_descriptor(--i);
    }
    vacant_fds.clear();
    fdtable.resize(size, {nullptr, wasi_fd_type::VACANT});

    uint32_t i = 0;
    while (i < size) {
        uint8_t type;
        if (!::sandbox_deserialize(type, data, max_size)) return false;

        if (type == 0) {
            uint32_t num_free_handles;
            if (!::sandbox_deserialize(num_free_handles, data, max_size)) return false;
            if (i + num_free_handles > size || i + num_free_handles < i) return false;
            for (uint32_t j = i; j < i + num_free_handles; ++j) {
                if (fdtable[i].type != wasi_fd_type::FSDIR && fdtable[i].type != wasi_fd_type::FSFILE) {
                    continue;
                }
                deallocate_file_descriptor(j);
                vacant_fds.clear();
            }
            i += num_free_handles;
        } else {
            if (fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSDIR && fdtable[i].type != wasi_fd_type::FSFILE) {
                return false;
            }
            if (type == 1) {
                if (fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSDIR) {
                    deallocate_file_descriptor(i);
                    vacant_fds.clear();
                }
                uint32_t root;
                if (!::sandbox_deserialize(root, data, max_size)) return false;
                if (root >= fdtable.size() || fdtable[root].type != wasi_fd_type::FS) return false;
                std::string path;
                if (!::sandbox_deserialize(path, data, max_size)) return false;
                path = mkxp_retro::fs->normalize(path.c_str(), false, true);
                fdtable[i] = {new fs_dir {path, root, fdtable[root].dir_handle()->writable}, wasi_fd_type::FSDIR};
            } else if (type == 2) {
                if (fdtable[i].type != wasi_fd_type::VACANT && fdtable[i].type != wasi_fd_type::FSFILE) {
                    deallocate_file_descriptor(i);
                    vacant_fds.clear();
                }
                uint32_t root;
                if (!::sandbox_deserialize(root, data, max_size)) return false;
                if (root >= fdtable.size() || fdtable[root].type != wasi_fd_type::FS) return false;
                std::string path;
                if (!::sandbox_deserialize(path, data, max_size)) return false;
                path = mkxp_retro::fs->normalize(path.c_str(), false, true);
                struct fs_file *handle = new fs_file {{*mkxp_retro::fs, path.c_str(), fdtable[root].dir_handle()->writable ? fdtable[root].dir_handle()->path.c_str() : nullptr, false}, root};
                if (!handle->file.is_open() || (fdtable[root].dir_handle()->writable && !handle->file.is_write_open())) {
                    delete handle;
                    return false;
                }
                fdtable[i] = {handle, wasi_fd_type::FSFILE};
                {
                    PHYSFS_File *file = handle->file.get();
                    int64_t pos;
                    if (!::sandbox_deserialize(pos, data, max_size)) return false;
                    if (pos > 0) {
                        PHYSFS_seek(file, pos);
                    }
                }
                {
                    PHYSFS_File *file = handle->file.get_write();
                    int64_t pos;
                    if (!::sandbox_deserialize(pos, data, max_size)) return false;
                    if (file != nullptr && pos > 0) {
                        PHYSFS_seek(file, pos);
                    }
                }
            } else {
                return false;
            }
            ++i;
        }
    }

    std::vector<u32> new_vacant_fds;
    for (uint32_t j = 0; j < fdtable.size(); ++j) {
        if (fdtable[j].type == wasi_fd_type::VACANT) {
            new_vacant_fds.push_back(j);
        }
    }
    vacant_fds = boost::container::priority_deque<uint32_t>(std::less<uint32_t>(), std::move(new_vacant_fds));

    return true;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_args_get(wasi_t *wasi, wasm_ptr_t argv, wasm_ptr_t argv_buf) {
    WASI_DEBUG("args_get()\n");
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_args_sizes_get(wasi_t *wasi, wasm_ptr_t argc, wasm_ptr_t argv_buf_size) {
    WASI_DEBUG("args_sizes_get(0x%08x, 0x%08x)\n", argc, argv_buf_size);
    wasi->ref<uint32_t>(argc) = 0;
    wasi->ref<uint32_t>(argv_buf_size) = 0;
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_clock_res_get(wasi_t *wasi, uint32_t id, wasm_ptr_t result) {
    WASI_DEBUG("clock_res_get(%u)\n", id);
    wasi->ref<uint64_t>(result) = 1000;
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_clock_time_get(wasi_t *wasi, uint32_t id, uint64_t precision, wasm_ptr_t result) {
    WASI_DEBUG("clock_time_get(%u, %lu)\n", id, precision);
    wasi->ref<uint64_t>(result) = mkxp_retro::perf.get_time_usec != nullptr ? mkxp_retro::perf.get_time_usec() * 1000L : 0;
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_environ_get(wasi_t *wasi, wasm_ptr_t env, wasm_ptr_t env_buf) {
    WASI_DEBUG("environ_get(0x%08x, 0x%08x)\n", env, env_buf);
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_environ_sizes_get(wasi_t *wasi, wasm_ptr_t env_size, wasm_ptr_t env_buf_size) {
    WASI_DEBUG("environ_sizes_get()\n");
    wasi->ref<uint32_t>(env_size) = 0;
    wasi->ref<uint32_t>(env_buf_size) = 0;
    return WASI_ESUCCESS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_advise(wasi_t *wasi, uint32_t fd, uint64_t offset, uint64_t len, uint32_t advice) {
    WASI_DEBUG("fd_advise(%u, %lu, %lu, 0x%08x)\n", fd, offset, len, advice);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_close(wasi_t *wasi, uint32_t fd) {
    WASI_DEBUG("fd_close(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
            return WASI_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            wasi->deallocate_file_descriptor(fd);
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_datasync(wasi_t *wasi, uint32_t fd) {
    WASI_DEBUG("fd_datasync(%u)\n", fd);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_fdstat_get(wasi_t *wasi, uint32_t fd, wasm_ptr_t result) {
    WASI_DEBUG("fd_fdstat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            wasi->ref<uint8_t>(result) = WASI_IFCHR; // fs_filetype
            wasi->ref<uint16_t>(result + 2) = 0; // fs_flags
            wasi->ref<uint64_t>(result + 8) = WASI_FD_READ | WASI_FD_WRITE | WASI_FD_FILESTAT_GET; // fs_rights_base
            wasi->ref<uint64_t>(result + 16) = 0; // fs_rights_inheriting
            return WASI_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            wasi->ref<uint8_t>(result) = WASI_IFDIR; // fs_filetype
            wasi->ref<uint16_t>(result + 2) = 0; // fs_flags
            wasi->ref<uint64_t>(result + 8) = WASI_PATH_OPEN | WASI_FD_READDIR | WASI_PATH_FILESTAT_GET | WASI_FD_FILESTAT_GET; // fs_rights_base
            wasi->ref<uint64_t>(result + 16) = 0; // fs_rights_inheriting
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_fdstat_set_flags(wasi_t *wasi, uint32_t fd, uint32_t flags) {
    WASI_DEBUG("fd_fdstat_set_flags(%u, %u)\n", fd, flags);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_filestat_get(wasi_t *wasi, uint32_t fd, wasm_ptr_t result) {
    WASI_DEBUG("fd_filestat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint64_t>(result) = fd; // dev
            wasi->ref<uint64_t>(result + 8) = 0; // ino
            wasi->ref<uint8_t>(result + 16) = WASI_IFCHR; // filetype
            wasi->ref<uint32_t>(result + 24) = 1; // nlink
            wasi->ref<uint64_t>(result + 32) = 0; // size
            wasi->ref<uint64_t>(result + 40) = 0; // atim
            wasi->ref<uint64_t>(result + 48) = 0; // mtim
            wasi->ref<uint64_t>(result + 56) = 0; // ctim
            return WASI_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].dir_handle()->path.c_str(), &stat)) {
                    return WASI_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASI_EIO;
                }
                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = WASI_IFDIR; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = stat.filesize; // size
                wasi->ref<uint64_t>(result + 40) = stat.accesstime * 1000000000L; // atim
                wasi->ref<uint64_t>(result + 48) = stat.modtime * 1000000000L; // mtim
                wasi->ref<uint64_t>(result + 56) = stat.createtime * 1000000000L; // ctim
                return WASI_ESUCCESS;
            }

        case wasi_fd_type::FSFILE:
            {
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(wasi->fdtable[fd].file_handle()->file.path(), &stat)) {
                    return WASI_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASI_EIO;
                }
                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = WASI_IFREG; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = stat.filesize; // size
                wasi->ref<uint64_t>(result + 40) = stat.accesstime * 1000000000L; // atim
                wasi->ref<uint64_t>(result + 48) = stat.modtime * 1000000000L; // mtim
                wasi->ref<uint64_t>(result + 56) = stat.createtime * 1000000000L; // ctim
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_filestat_set_size(wasi_t *wasi, uint32_t fd, uint64_t size) {
    WASI_DEBUG("fd_filestat_set_size(%u, %lu)\n", fd, size);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            return WASI_EINVAL;

        default:
            return WASI_ENOSYS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_pread(wasi_t *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, uint64_t offset, wasm_ptr_t result) {
    WASI_DEBUG("fd_pread(%u, 0x%08x (%u), %lu)\n", fd, iovs, iovs_len, offset);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_prestat_dir_name(wasi_t *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    WASI_DEBUG("fd_prestat_dir_name(%u, 0x%x (%u))\n", fd, path, path_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::FS:
            wasi->strncpy_s(path, wasi->fdtable[fd].dir_handle()->path.c_str(), path_len + 1);
            return WASI_ESUCCESS;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_prestat_get(wasi_t *wasi, uint32_t fd, wasm_ptr_t result) {
    WASI_DEBUG("fd_prestat_get(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::FS:
            wasi->ref<uint32_t>(result) = 0;
            wasi->ref<uint32_t>(result + 4) = wasi->fdtable[fd].dir_handle()->path.length();
            return WASI_ESUCCESS;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_pwrite(wasi_t *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, uint64_t offset, wasm_ptr_t result) {
    WASI_DEBUG("fd_pwrite(%u, 0x%08x (%u), %lu)\n", fd, iovs, iovs_len, offset);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_read(wasi_t *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, wasm_ptr_t result) {
    WASI_DEBUG("fd_read(%u, 0x%08x (%u))\n", fd, iovs, iovs_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            wasi->ref<uint32_t>(result) = 0;
            return WASI_ESUCCESS;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            return WASI_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    if (length > 0) {
#ifdef MKXPZ_BIG_ENDIAN
                        uint8_t *buffer = &wasi->ref<uint8_t>(ptr, length - 1);
#else
                        uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                        PHYSFS_sint64 n = PHYSFS_readBytes(wasi->fdtable[fd].file_handle()->file.get(), buffer, length);
#ifdef MKXPZ_BIG_ENDIAN
                        std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                        if (n < 0) return WASI_EIO;
                        size += n;
                    }
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_readdir(wasi_t *wasi, uint32_t fd, wasm_ptr_t buf, uint32_t buf_len, uint64_t cookie, wasm_ptr_t result) {
    WASI_DEBUG("fd_readdir(%u, 0x%08x (%u), %lu)\n", fd, buf, buf_len, cookie);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                struct fs_enumerate_data edata = {
                    wasi,
                    fd,
                    buf,
                    buf,
                    buf_len,
                    cookie,
                    0,
                    result,
                };
                bool success = PHYSFS_enumerate(
                    wasi->fdtable[fd].dir_handle()->path.c_str(),
                    [](void *data, const char *path, const char *filename) {
                        struct fs_enumerate_data *edata = (struct fs_enumerate_data *)data;
                        wasi_t *wasi = edata->wasi;

                        PHYSFS_Stat stat;
                        if (!PHYSFS_stat(edata->wasi->fdtable[edata->fd].dir_handle()->path.c_str(), &stat)) {
                            return PHYSFS_ENUM_OK;
                        }
                        if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                            return PHYSFS_ENUM_OK;
                        }

                        if (edata->cookie++ < edata->initial_cookie) {
                            return PHYSFS_ENUM_OK;
                        }

                        if (edata->buf - edata->original_buf + 8 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        std::memcpy(wasi->ptr_unaligned<uint64_t>(edata->buf), &edata->cookie, 8);
                        edata->buf += 8;

                        if (edata->buf - edata->original_buf + 8 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        std::memset(wasi->ptr_unaligned<uint64_t>(edata->buf), 0, 8);
                        edata->buf += 8;

                        if (edata->buf - edata->original_buf + 4 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        {
                            const uint32_t value = std::strlen(filename);
                            std::memcpy(wasi->ptr_unaligned<uint32_t>(edata->buf), &value, 4);
                        }
                        edata->buf += 4;

                        if (edata->buf - edata->original_buf + 4 > edata->buf_len) {
                            return PHYSFS_ENUM_STOP;
                        }
                        wasi->ref<uint8_t>(edata->buf) = stat.filetype;
                        edata->buf += 4;

                        uint32_t len = std::min(std::strlen(filename), (size_t)(edata->original_buf + edata->buf_len - edata->buf));
                        wasi->arycpy(edata->buf, filename, std::strlen(filename));
                        edata->buf += len;
                        return PHYSFS_ENUM_OK;
                    },
                    (void *)&edata
                );
                if (success) {
                    wasi->ref<uint32_t>(result) = edata.buf - edata.original_buf;
                    return WASI_ESUCCESS;
                }
                return success ? WASI_ESUCCESS : WASI_ENOENT;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_renumber(wasi_t *wasi, uint32_t fd, uint32_t to) {
    WASI_DEBUG("fd_renumber(%u, %u)\n", fd, to);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
            return WASI_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            break;
    }

    if (fd == to) {
        return WASI_ESUCCESS;
    }

    if (to >= wasi->fdtable.size()) return WASI_EBADF;

    switch (wasi->fdtable[to].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
            return WASI_EINVAL;

        case wasi_fd_type::FSDIR:
        case wasi_fd_type::FSFILE:
            wasi->deallocate_file_descriptor(to);
            if (to == wasi->fdtable.size()) {
                wasi->fdtable.push_back(wasi->fdtable[fd]);
            } else {
                wasi->fdtable[to] = wasi->fdtable[fd];
            }

            if (fd == wasi->fdtable.size() - 1) {
                wasi->fdtable.pop_back();
                while (!wasi->fdtable.empty() && wasi->fdtable.back().type == wasi_fd_type::VACANT) {
                    assert(!wasi->vacant_fds.empty() && wasi->vacant_fds.maximum() == wasi->fdtable.size() - 1);
                    wasi->vacant_fds.pop_maximum();
                    wasi->fdtable.pop_back();
                }
            } else {
                wasi->fdtable[fd] = {nullptr, wasi_fd_type::VACANT};
                wasi->vacant_fds.push(fd);
            }

            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_seek(wasi_t *wasi, uint32_t fd, uint64_t offset, uint32_t whence, wasm_ptr_t result) {
    WASI_DEBUG("fd_seek(%u, %lu, %u)\n", fd, offset, whence);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_sync(wasi_t *wasi, uint32_t fd) {
    WASI_DEBUG("fd_sync(%u)\n", fd);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_tell(wasi_t *wasi, uint32_t fd, wasm_ptr_t result) {
    WASI_DEBUG("fd_tell(%u)\n", fd);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            return WASI_EINVAL;

        case wasi_fd_type::FSFILE:
            wasi->ref<uint64_t>(result) = PHYSFS_tell(wasi->fdtable[fd].file_handle()->file.get());
            return WASI_ESUCCESS;
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_fd_write(wasi_t *wasi, uint32_t fd, wasm_ptr_t iovs, uint32_t iovs_len, wasm_ptr_t result) {
    WASI_DEBUG("fd_write(%u, 0x%08x (%u))\n", fd, iovs, iovs_len);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
            wasi->ref<uint32_t>(result) = 0;
            return WASI_ESUCCESS;

        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
            {
                uint32_t size = 0;
                std::string buf;
                while (iovs_len > 0) {
                    const char *str = wasi->str(wasi->ref<uint32_t>(iovs));
                    buf.append(str, strlen_safe(str, wasi->ref<uint32_t>(iovs + 4)));
                    size += wasi->ref<uint32_t>(iovs + 4);
                    iovs += 8;
                    --iovs_len;
                }
                std::string line;
                std::istringstream stream(buf);
                while (std::getline(stream, line)) {
                    mkxp_retro::log_printf(wasi->fdtable[fd].type == wasi_fd_type::STDOUT ? RETRO_LOG_INFO : RETRO_LOG_ERROR, "%s\n", line.c_str());
                }
                wasi->ref<uint32_t>(result) = size;
                return WASI_ESUCCESS;
            }

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            return WASI_EINVAL;

        case wasi_fd_type::FSFILE:
            {
                if (!wasi->fdtable[fd].file_handle()->file.is_write_open()) {
                    return WASI_EROFS;
                }
                uint32_t size = 0;
                while (iovs_len > 0) {
                    uint32_t ptr = wasi->ref<uint32_t>(iovs);
                    uint32_t length = wasi->ref<uint32_t>(iovs + 4);
                    if (length > 0) {
#ifdef MKXPZ_BIG_ENDIAN
                        uint8_t *buffer = &wasi->ref<uint8_t>(ptr, length - 1);
#else
                        uint8_t *buffer = &wasi->ref<uint8_t>(ptr);
#endif // MKXPZ_BIG_ENDIAN
                        PHYSFS_sint64 n = PHYSFS_writeBytes(wasi->fdtable[fd].file_handle()->file.get_write(), buffer, length);
#ifdef MKXPZ_BIG_ENDIAN
                        std::reverse(buffer, buffer + length);
#endif // MKXPZ_BIG_ENDIAN
                        if (n > 0) {
                            size += n;
                        }
                    }
                    iovs += 8;
                    --iovs_len;
                }
                wasi->ref<uint32_t>(result) = size;
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_create_directory(wasi_t *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    WASI_DEBUG("path_create_directory(%u, \"%.*s\")\n", fd, path_len, (const char *)wasi->str(path));
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_filestat_get(wasi_t *wasi, uint32_t fd, uint32_t flags, wasm_ptr_t path, uint32_t path_len, wasm_ptr_t result) {
    WASI_DEBUG("path_filestat_get(%u, %u, \"%.*s\")\n", fd, flags, path_len, (const char *)wasi->str(path));

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                PHYSFS_Stat stat;
                std::string new_path(wasi->fdtable[fd].dir_handle()->path);
                new_path.push_back('/');
                const char *str = wasi->str(path);
                new_path.append(str, strlen_safe(str, path_len));
                new_path = mkxp_retro::fs->normalize(new_path.c_str(), false, true);
                if (std::strncmp(new_path.c_str(), wasi->fdtable[fd].dir_handle()->path.c_str(), wasi->fdtable[fd].dir_handle()->path.length()) != 0) {
                    return WASI_EPERM;
                }

                if (!PHYSFS_stat(new_path.c_str(), &stat)) {
                    return WASI_ENOENT;
                }
                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASI_EIO;
                }

                wasi->ref<uint64_t>(result) = fd; // dev
                wasi->ref<uint64_t>(result + 8) = 0; // ino
                wasi->ref<uint8_t>(result + 16) = stat.filetype == PHYSFS_FILETYPE_DIRECTORY ? WASI_IFDIR : WASI_IFREG; // filetype
                wasi->ref<uint32_t>(result + 24) = 1; // nlink
                wasi->ref<uint64_t>(result + 32) = stat.filetype; // size
                wasi->ref<uint64_t>(result + 40) = stat.accesstime * 1000000000L; // atim
                wasi->ref<uint64_t>(result + 48) = stat.modtime * 1000000000L; // mtim
                wasi->ref<uint64_t>(result + 56) = stat.createtime * 1000000000L; // ctim
                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_filestat_set_times(wasi_t *wasi, uint32_t fd, uint32_t flags, wasm_ptr_t path, uint32_t path_len, uint64_t atim, uint64_t ntim, uint32_t fst_flags) {
    WASI_DEBUG("path_filestat_set_times(%u, %u, \"%.*s\", %lu, %lu, %u)\n", fd, flags, path_len, (const char *)wasi->str(path), atim, ntim, fst_flags);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_link(wasi_t *wasi, uint32_t old_fd, uint32_t old_flags, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t new_fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    WASI_DEBUG("path_link(%u, %u, \"%.*s\", %u, \"%.*s\")\n", old_fd, old_flags, old_path_len, (const char *)wasi->str(old_path), new_fd, new_path_len, (const char *)wasi->str(new_path));
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_open(wasi_t *wasi, uint32_t fd, uint32_t dirflags, wasm_ptr_t path, uint32_t path_len, uint32_t oflags, uint64_t fs_base_rights, uint64_t fs_rights_inheriting, uint32_t fdflags, wasm_ptr_t result) {
    WASI_DEBUG("path_open(%u, %u, \"%.*s\", %u, %lu, %lu, %u)\n", fd, dirflags, path_len, (const char *)wasi->str(path), oflags, fs_base_rights, fs_rights_inheriting, fdflags);

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    if (wasi->fdtable.size() >= UINT32_MAX && wasi->vacant_fds.empty()) {
        return WASI_EMFILE;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                PHYSFS_Stat stat;
                std::string new_path(wasi->fdtable[fd].dir_handle()->path);
                new_path.push_back('/');
                const char *str = wasi->str(path);
                new_path.append(str, strlen_safe(str, path_len));
                new_path = mkxp_retro::fs->normalize(new_path.c_str(), false, true);

                // Verify that the path we're opening is a descendant of the directory corresponding to `fd`
                if (std::strncmp(new_path.c_str(), wasi->fdtable[fd].dir_handle()->path.c_str(), wasi->fdtable[fd].dir_handle()->path.length()) != 0) {
                    return WASI_EPERM;
                }

                bool exists = PHYSFS_stat(new_path.c_str(), &stat);

                // Fail if bit 0 of oflags isn't set and the path doesn't exist
                if (!exists && !(oflags & (1 << 0))) {
                    return WASI_ENOENT;
                }

                // Fail if bit 1 of oflags is set and the path exists and isn't a directory
                if (exists && oflags & (1 << 1) && stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASI_ENOTDIR;
                }

                // Fail if bit 2 of oflags is set and the path exists
                if (exists && oflags & (1 << 2)) {
                    return WASI_EEXIST;
                }

                // Fail if the path exists and isn't a regular file or directory (e.g. a device file, named pipe, socket or symbolic link)
                if (exists && (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                    return WASI_EIO;
                }

                bool truncate = oflags & (1 << 3);
                bool needs_write = !exists || truncate;
                bool writable = wasi->fdtable[fd].dir_handle()->writable;

                // Fail if we need to create a new file or truncate a file in a read-only file system
                if (needs_write && !writable && oflags & (1 << 0)) {
                    return WASI_EROFS;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;

                if (exists && stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    struct fs_dir *handle = new fs_dir {new_path, root, writable};
                    wasi->ref<uint32_t>(result) = wasi->allocate_file_descriptor(wasi_fd_type::FSDIR, handle);
                } else {
                    const char *write_path_prefix;
                    if (writable) {
                        uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                        write_path_prefix = wasi->fdtable[root].dir_handle()->path.c_str();
                    } else {
                        write_path_prefix = nullptr;
                    }

                    struct fs_file *handle = new fs_file {{*mkxp_retro::fs, new_path.c_str(), write_path_prefix, truncate, exists}, root};

                    // Check for errors opening the read handle and/or write handle
                    if (!handle->file.is_open() || (needs_write && writable && !handle->file.is_write_open())) {
                        PHYSFS_ErrorCode error = handle->file.get_read_error();
                        if (error == handle->file.get_read_error()) {
                            error = handle->file.get_write_error();
                        }
                        delete handle;
                        switch (error) {
                            case PHYSFS_ERR_READ_ONLY:
                            case PHYSFS_ERR_NO_WRITE_DIR:
                                return WASI_EROFS;
                            case PHYSFS_ERR_PERMISSION:
                                return WASI_EACCES;
                            default:
                                return WASI_EIO;
                        }
                    }

                    wasi->ref<uint32_t>(result) = wasi->allocate_file_descriptor(wasi_fd_type::FSFILE, handle);
                }

                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_readlink(wasi_t *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len, wasm_ptr_t buf, uint32_t buf_len, wasm_ptr_t result) {
    WASI_DEBUG("path_readlink(%u, \"%.*s\", 0x%08x (%u))\n", fd, path_len, (const char *)wasi->str(path), buf, buf_len);
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_remove_directory(wasi_t *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    WASI_DEBUG("path_remove_directory(%u, \"%.*s\")\n", fd, path_len, (const char *)wasi->str(path));

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASI_EROFS;
                }

                PHYSFS_Stat stat;
                std::string new_path(wasi->fdtable[fd].dir_handle()->path);
                new_path.push_back('/');
                const char *str = wasi->str(path);
                new_path.append(str, strlen_safe(str, path_len));
                new_path = mkxp_retro::fs->normalize(new_path.c_str(), false, true);

                // Verify that the path we're opening is a descendant of the directory corresponding to `fd`
                if (std::strncmp(new_path.c_str(), wasi->fdtable[fd].dir_handle()->path.c_str(), wasi->fdtable[fd].dir_handle()->path.length()) != 0) {
                    return WASI_EPERM;
                }

                if (!PHYSFS_stat(wasi->fdtable[fd].dir_handle()->path.c_str(), &stat)) {
                    return WASI_ENOENT;
                }

                if (stat.filetype != PHYSFS_FILETYPE_DIRECTORY) {
                    return WASI_ENOTDIR;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (!PHYSFS_delete(new_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_DIR_NOT_EMPTY:
                            return WASI_ENOTEMPTY;
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            return WASI_EROFS;
                        case PHYSFS_ERR_PERMISSION:
                            return WASI_EACCES;
                        default:
                            return WASI_EIO;
                    }
                }

                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_rename(wasi_t *wasi, uint32_t fd, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t new_fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    WASI_DEBUG("path_rename(%u, \"%.*s\", %u, \"%.*s\")\n", fd, old_path_len, (const char *)wasi->str(old_path), new_fd, new_path_len, (const char *)wasi->str(new_path));
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_symlink(wasi_t *wasi, wasm_ptr_t old_path, uint32_t old_path_len, uint32_t fd, wasm_ptr_t new_path, uint32_t new_path_len) {
    WASI_DEBUG("path_symlink(\"%.*s\", %u, \"%.*s\")\n", old_path_len, (const char *)wasi->str(old_path), fd, new_path_len, (const char *)wasi->str(new_path));
    return WASI_ENOSYS;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_path_unlink_file(wasi_t *wasi, uint32_t fd, wasm_ptr_t path, uint32_t path_len) {
    WASI_DEBUG("path_unlink_file(%u, \"%.*s\")\n", fd, path_len, (const char *)wasi->str(path));

    if (fd >= wasi->fdtable.size()) {
        return WASI_EBADF;
    }

    switch (wasi->fdtable[fd].type) {
        case wasi_fd_type::VACANT:
            return WASI_EBADF;

        case wasi_fd_type::STDIN:
        case wasi_fd_type::STDOUT:
        case wasi_fd_type::STDERR:
        case wasi_fd_type::FSFILE:
            return WASI_EINVAL;

        case wasi_fd_type::FS:
        case wasi_fd_type::FSDIR:
            {
                if (!wasi->fdtable[fd].dir_handle()->writable) {
                    return WASI_EROFS;
                }

                PHYSFS_Stat stat;
                std::string new_path(wasi->fdtable[fd].dir_handle()->path);
                new_path.push_back('/');
                const char *str = wasi->str(path);
                new_path.append(str, strlen_safe(str, path_len));
                new_path = mkxp_retro::fs->normalize(new_path.c_str(), false, true);

                // Verify that the path we're opening is a descendant of the directory corresponding to `fd`
                if (std::strncmp(new_path.c_str(), wasi->fdtable[fd].dir_handle()->path.c_str(), wasi->fdtable[fd].dir_handle()->path.length()) != 0) {
                    return WASI_EPERM;
                }

                if (!PHYSFS_stat(wasi->fdtable[fd].dir_handle()->path.c_str(), &stat)) {
                    return WASI_ENOENT;
                }

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    return WASI_EISDIR;
                } else if (stat.filetype != PHYSFS_FILETYPE_REGULAR) {
                    return WASI_EIO;
                }

                uint32_t root = wasi->fdtable[fd].type == wasi_fd_type::FS ? fd : wasi->fdtable[fd].dir_handle()->root;
                if (!PHYSFS_delete(new_path.c_str() + wasi->fdtable[root].dir_handle()->path.length())) {
                    switch (PHYSFS_getLastErrorCode()) {
                        case PHYSFS_ERR_READ_ONLY:
                        case PHYSFS_ERR_NO_WRITE_DIR:
                            return WASI_EROFS;
                        case PHYSFS_ERR_PERMISSION:
                            return WASI_EACCES;
                        default:
                            return WASI_EIO;
                    }
                }

                return WASI_ESUCCESS;
            }
    }

    return WASI_EBADF;
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_poll_oneoff(wasi_t *wasi, wasm_ptr_t in, wasm_ptr_t out, uint32_t nsubscriptions, wasm_ptr_t result) {
    WASI_DEBUG("poll_oneoff(0x%08x, 0x%08x, %u)\n", in, out, nsubscriptions);
    return WASI_ENOSYS;
}

extern "C" void w2c_wasi__snapshot__preview1_proc_exit(wasi_t *wasi, uint32_t rval) {
    WASI_DEBUG("proc_exit(%u)\n", rval);
    std::abort();
}

extern "C" uint32_t w2c_wasi__snapshot__preview1_random_get(wasi_t *wasi, wasm_ptr_t buf, uint32_t buf_len) {
    WASI_DEBUG("random_get(0x%08x (%u))\n", buf, buf_len);

    while (buf_len > 0) {
        if (wasi->prng_buffer_size == 0) {
            wasi->prng_buffer_size = 4;

            // PCG32 XSH RR (based on https://github.com/imneme/pcg-cpp, licensed MIT)
            uint64_t state = wasi->prng_state;
            wasi->prng_state = wasi->prng_state * (uint64_t)6364136223846793005U + (uint64_t)1442695040888963407U; // Advance state before computing output to improve instruction-level parallelism
            uint32_t xsh = (state ^ (state >> 18U)) >> 27U;
            uint32_t rot = state >> 59U;
            uint32_t out = xsh >> rot | xsh << ((uint32_t)31U - rot);
#ifdef MKXPZ_BIG_ENDIAN
            // Byte swap the output on big-endian machines to preserve state state compatibility across machines with different endiannesses
            std::reverse_copy((uint8_t *)&out, (uint8_t *)&out + 4, wasi->prng_buffer);
#else
            std::memcpy(wasi->prng_buffer, &out, 4);
#endif // MKXPZ_BIG_ENDIAN
        } else {
            uint32_t n = std::min(buf_len, wasi->prng_buffer_size);
            wasi->arycpy(buf, wasi->prng_buffer + ((uint32_t)4 - n), n);
            buf += n;
            buf_len -= n;
            wasi->prng_buffer_size -= n;
        }
    }

    return WASI_ESUCCESS;
}
