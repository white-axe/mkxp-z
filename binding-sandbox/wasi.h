/*
** wasi.h
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

#ifndef MKXPZ_SANDBOX_WASI_H
#define MKXPZ_SANDBOX_WASI_H

#include <memory>
#include <string>
#include <vector>
#include <zip.h>
#include "filesystem.h"
#include "types.h"

// Internal utility macros
#define _WASM_CAT(x, y) x##y
#define WASM_CAT(x, y) _WASM_CAT(x, y)
#if WABT_BIG_ENDIAN
    #define WASM_ORDER_u8(value) (value)
    #define WASM_ORDER_s8(value) (value)
    #define WASM_ORDER_u16(value) __builtin_bswap16(value)
    #define WASM_ORDER_s16(value) __builtin_bswap16(value)
    #define WASM_ORDER_u32(value) __builtin_bswap32(value)
    #define WASM_ORDER_s32(value) __builtin_bswap32(value)
    #define WASM_ORDER_u64(value) __builtin_bswap64(value)
    #define WASM_ORDER_s64(value) __builtin_bswap64(value)
    #define WASM_ORDER_f32(value) __builtin_bswap32(value)
    #define WASM_ORDER_f64(value) __builtin_bswap64(value)
#else
    #define WASM_ORDER_u8(value) (value)
    #define WASM_ORDER_s8(value) (value)
    #define WASM_ORDER_u16(value) (value)
    #define WASM_ORDER_s16(value) (value)
    #define WASM_ORDER_u32(value) (value)
    #define WASM_ORDER_s32(value) (value)
    #define WASM_ORDER_u64(value) (value)
    #define WASM_ORDER_s64(value) (value)
    #define WASM_ORDER_f32(value) (value)
    #define WASM_ORDER_f64(value) (value)
#endif // WABT_BIG_ENDIAN
#define WASM_ORDER_usize(value) WASM_CAT(WASM_ORDER_, usize)(value)

// Memory manipulation macros
#define WASM_GET(type, address) WASM_ORDER_##type(*(type *)WASM_MEM(address)) // Returns the value at the given memory address (address should be `usize`), cast to the given type
#define WASM_SET(type, address, value) do *(type *)WASM_MEM(address) = WASM_ORDER_##type(value); while (0) // Sets the value of the given type to the given value at the given memory address (address should be `usize`)

// WASI error numbers
#define WASI_ESUCCESS 0 // No error occurred. System call completed successfully.
#define WASI_E2BIG 1 // Argument list too long.
#define WASI_EACCES 2 // Permission denied.
#define WASI_EADDRINUSE 3 // Address in use.
#define WASI_EADDRNOTAVAIL 4 // Address not available.
#define WASI_EAFNOSUPPORT 5 // Address family not supported.
#define WASI_EAGAIN 6 // Resource unavailable, or operation would block.
#define WASI_EALREADY 7 // Connection already in progress.
#define WASI_EBADF 8 // Bad file descriptor.
#define WASI_EBADMSG 9 // Bad message.
#define WASI_EBUSY 10 // Device or resource busy.
#define WASI_ECANCELED 11 // Operation canceled.
#define WASI_ECHILD 12 // No child processes.
#define WASI_ECONNABORTED 13 // Connection aborted.
#define WASI_ECONNREFUSED 14 // Connection refused.
#define WASI_ECONNRESET 15 // Connection reset.
#define WASI_EDEADLK 16 // Resource deadlock would occur.
#define WASI_EDESTADDRREQ 17 // Destination address required.
#define WASI_EDOM 18 // Mathematics argument out of domain of function.
#define WASI_EDQUOT 19 // Reserved.
#define WASI_EEXIST 20 // File exists.
#define WASI_EFAULT 21 // Bad address.
#define WASI_EFBIG 22 // File too large.
#define WASI_EHOSTUNREACH 23 // Host is unreachable.
#define WASI_EIDRM 24 // Identifier removed.
#define WASI_EILSEQ 25 // Illegal byte sequence.
#define WASI_EINPROGRESS 26 // Operation in progress.
#define WASI_EINTR 27 // Interrupted function.
#define WASI_EINVAL 28 // Invalid argument.
#define WASI_EIO 29 // I/O error.
#define WASI_EISCONN 30 // Socket is connected.
#define WASI_EISDIR 31 // Is a directory.
#define WASI_ELOOP 32 // Too many levels of symbolic links.
#define WASI_EMFILE 33 // File descriptor value too large.
#define WASI_EMLINK 34 // Too many links.
#define WASI_EMSGSIZE 35 // Message too large.
#define WASI_EMULTIHOP 36 // Reserved.
#define WASI_ENAMETOOLONG 37 // Filename too long.
#define WASI_ENETDOWN 38 // Network is down.
#define WASI_ENETRESET 39 // Connection aborted by network.
#define WASI_ENETUNREACH 40 // Network unreachable.
#define WASI_ENFILE 41 // Too many files open in system.
#define WASI_ENOBUFS 42 // No buffer space available.
#define WASI_ENODEV 43 // No such device.
#define WASI_ENOENT 44 // No such file or directory.
#define WASI_ENOEXEC 45 // Executable file format error.
#define WASI_ENOLCK 46 // No locks available.
#define WASI_ENOLINK 47 // Reserved.
#define WASI_ENOMEM 48 // Not enough space.
#define WASI_ENOMSG 49 // No message of the desired type.
#define WASI_ENOPROTOOPT 50 // Protocol not available.
#define WASI_ENOSPC 51 // No space left on device.
#define WASI_ENOSYS 52 // Function not supported.
#define WASI_ENOTCONN 53 // The socket is not connected.
#define WASI_ENOTDIR 54 // Not a directory or a symbolic link to a directory.
#define WASI_ENOTEMPTY 55 // Directory not empty.
#define WASI_ENOTRECOVERABLE 56 // State not recoverable.
#define WASI_ENOTSOCK 57 // Not a socket.
#define WASI_ENOTSUP 58 // Not supported, or operation not supported on socket.
#define WASI_ENOSTDIO 59 // Inappropriate I/O control operation.
#define WASI_ENXIO 60 // No such device or address.
#define WASI_EOVERFLOW 61 // Value too large to be stored in data type.
#define WASI_EOWNERDEAD 62 // Previous owner died.
#define WASI_EPERM 63 // Operation not permitted.
#define WASI_EPIPE 64 // Broken pipe.
#define WASI_EPROTO 65 // Protocol error.
#define WASI_EPROTONOSUPPORT 66 // Protocol not supported.
#define WASI_EPROTOTYPE 67 // Protocol wrong type for socket.
#define WASI_ERANGE 68 // Result too large.
#define WASI_EROFS 69 // Read-only file system.
#define WASI_ESPIPE 70 // Invalid seek.
#define WASI_ESRCH 71 // No such process.
#define WASI_ESTALE 72 // Reserved.
#define WASI_ETIMEDOUT 73 // Connection timed out.
#define WASI_ETXTBSY 74 // Text file busy.
#define WASI_EXDEV 75 // Cross-device link.
#define WASI_ENOTCAPABLE 76 // Extension: Capabilities insufficient.

// WASI file types
#define WASI_IFUNK 0 // Unknown
#define WASI_IFBLK 1 // Block device
#define WASI_IFCHR 2 // Character device
#define WASI_IFDIR 3 // Directory
#define WASI_IFREG 4 // Regular file
#define WASI_IFSOCKD 5 // Datagram socket
#define WASI_IFSOCKS 6 // Stream socket
#define WASI_IFLNK 7 // Symbolic link

// WASI file flags
#define WASI_APPEND (1 << 0)
#define WASI_DSYNC (1 << 1)
#define WASI_NONBLOCK (1 << 2)
#define WASI_RSYNC (1 << 3)
#define WASI_SYNC (1 << 4)

// WASI rights flags
#define WASI_FD_DATASYNC (1 << 0)
#define WASI_FD_READ (1 << 1)
#define WASI_FD_SEEK (1 << 2)
#define WASI_FD_FDSTAT_SET_FLAGS (1 << 3)
#define WASI_FD_SYNC (1 << 4)
#define WASI_FD_TELL (1 << 5)
#define WASI_FD_WRITE (1 << 6)
#define WASI_FD_ADVISE (1 << 7)
#define WASI_FD_ALLOCATE (1 << 8)
#define WASI_PATH_CREATE_DIRECTORY (1 << 9)
#define WASI_PATH_CREATE_FILE (1 << 10)
#define WASI_PATH_LINK_SOURCE (1 << 11)
#define WASI_PATH_LINK_TARGET (1 << 12)
#define WASI_PATH_OPEN (1 << 13)
#define WASI_FD_READDIR (1 << 14)
#define WASI_PATH_READLINK (1 << 15)
#define WASI_PATH_RENAME_SOURCE (1 << 16)
#define WASI_PATH_RENAME_TARGET (1 << 17)
#define WASI_PATH_FILESTAT_GET (1 << 18)
#define WASI_PATH_FILESTAT_SET_SIZE (1 << 19)
#define WASI_PATH_FILESTAT_SET_TIMES (1 << 20)
#define WASI_FD_FILESTAT_GET (1 << 21)
#define WASI_FD_FILESTAT_SET_SIZE (1 << 22)
#define WASI_FD_FILESTAT_SET_TIMES (1 << 23)
#define WASI_PATH_SYMLINK (1 << 24)
#define WASI_PATH_REMOVE_DIRECTORY (1 << 25)
#define WASI_PATH_UNLINK_FILE (1 << 26)
#define WASI_POLL_FD_READWRITE (1 << 27)
#define WASI_SOCK_SHUTDOWN (1 << 28)
#define WASI_SOCK_ACCEPT (1 << 29)

typedef std::pair<u32, std::string> path_cache_entry_t;

struct wasi_zip_container {
    private:
    zip_source_t *const source;

    public:
    zip_t *const zip;
    std::vector<path_cache_entry_t> path_cache;
    wasi_zip_container();
    wasi_zip_container(const char *path, zip_flags_t flags);
    wasi_zip_container(const void *buffer, zip_uint64_t length, zip_flags_t flags);
    ~wasi_zip_container();
};

struct wasi_zip_file_container {
    zip_file_t *const file;
    wasi_zip_file_container();
    wasi_zip_file_container(wasi_zip_container &zip, zip_uint64_t index, zip_flags_t flags);
    ~wasi_zip_file_container();
};

struct wasi_zip_handle {
    std::shared_ptr<struct wasi_zip_container> zip; // Zip handle that can be used with libzip
    std::string path; // Mount point of this archive relative to the root of the virtual filesystem, normalized to start with one leading slash and no trailing slashes (e.g. "/example/path")
};

struct wasi_zip_dir_handle {
    zip_uint64_t index; // Index of this directory within the zip file
    std::string path; // Path of this directory relative to the root of the zip file, normalized to start with no leading slashes or dots and one trailing slash (e.g. "example/path/")
    u32 parent_fd; // WASI file descriptor of the zip file that contains this directory
};

struct wasi_zip_file_handle {
    zip_uint64_t index; // Index of this file within the zip file
    struct wasi_zip_file_container zip_file_handle; // Handle to this file that can be used with libzip
    u32 parent_fd; // WASI file descriptor of the zip file that contains this file
};

struct undefined {};

enum wasi_fd_type {
    STDIN, // This file descriptor is standard input. The `handle` field is null.
    STDOUT, // This file descriptor is standard output. The `handle` field is null.
    STDERR, // This file descriptor is standard error. The `handle` field is null.
    FS, // This file descriptor is a preopened directory handled by the mkxp-z filesystem code. The `handle` field is a `std::string *` containing the path of the directory.
    FSDIR, // This file descriptor is a directory handled by the mkxp-z filesystem code. The `handle` field is a `std::string *` containing the path of the directory.
    FSFILE, // This file descriptor is a file handled by the mkxp-z filesystem code. The `handle` field is a `struct FileSystem::File *`.
    ZIP, // This file descriptor is a read-only zip file. The `handle` field is a `struct wasi_zip_handle *`.
    ZIPDIR, // This file descriptor is a directory inside of a zip file. The `handle` field is a `struct wasi_zip_dir_handle *`.
    ZIPFILE, // This file descriptor is a file inside of a zip file. The `handle` field is a `struct wasi_zip_file_handle *`.
    VACANT, // Indicates this is a vacant file descriptor that doesn't correspond to a file. The `handle` field is null.
};

struct wasi_file_entry {
    wasi_fd_type type;

    // The file/directory handle that the file descriptor corresponds to. The exact type of this handle depends on the type of file descriptor.
    void *handle;

    std::string *dir_handle();
    struct FileSystem::File *file_handle();
    struct wasi_zip_handle *zip_handle();
    struct wasi_zip_dir_handle *zip_dir_handle();
    struct wasi_zip_file_handle *zip_file_handle();
};

struct wasi_zip_stat {
    bool exists;
    u8 filetype;
    u64 inode;
    u64 size;
    u64 mtime;
    std::string normalized_path;
};

typedef struct w2c_wasi__snapshot__preview1 {
    std::shared_ptr<struct w2c_ruby> ruby;

    std::shared_ptr<struct wasi_zip_container> dist;

    // WASI file descriptor table. Maps WASI file descriptors (unsigned 32-bit integers) to file handles.
    std::vector<wasi_file_entry> fdtable;

    // List of vacant WASI file descriptors so that we can reallocate vacant WASI file descriptors in O(1) amortized time.
    std::vector<u32> vacant_fds;

    w2c_wasi__snapshot__preview1(std::shared_ptr<struct w2c_ruby> ruby);
    ~w2c_wasi__snapshot__preview1();
    u32 allocate_file_descriptor(enum wasi_fd_type type, void *handle = NULL);
    void deallocate_file_descriptor(u32 fd);
} wasi_t;

#endif /* MKXPZ_SANDBOX_WASI_H */
