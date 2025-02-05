/*
** types.h
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

#ifndef MKXPZ_SANDBOX_TYPES_H
#define MKXPZ_SANDBOX_TYPES_H

#include <cstdint>

#ifdef MKXPZ_RETRO_MEMORY64
#define usize u64
typedef int64_t wasm_ssize_t;
typedef uint64_t wasm_size_t;
#else
#define usize u32
typedef int32_t wasm_ssize_t;
typedef uint32_t wasm_size_t;
#endif

#define ANYARGS ...
typedef wasm_size_t wasm_ptr_t;
typedef wasm_size_t VALUE;
typedef wasm_size_t ID;

#ifndef WASM_RT_CORE_TYPES_DEFINED
#define WASM_RT_CORE_TYPES_DEFINED
typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;
#endif // WASM_RT_CORE_TYPES_DEFINED

struct SandboxException {};
// The call to `ruby_executable_node()` or `ruby_exec_node()` failed when initializing Ruby.
struct SandboxNodeException : SandboxException {};
// Failed to allocate memory.
struct SandboxOutOfMemoryException : SandboxException {};
// An exception occurred inside of Ruby and was not caught.
struct SandboxTrapException : SandboxException {};

#endif // MKXPZ_SANDBOX_TYPES_H
