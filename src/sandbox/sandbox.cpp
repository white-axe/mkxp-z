/*
** sandbox.cpp
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

#include <cstdio>
#include <cstring>
#include <string>
#include <wasm-rt.h>
#include "wasi.h"
#include <mkxp-retro-ruby/mkxp-retro-ruby.h>
#include "sandbox.h"

#define MJIT_ENABLED 0
#define MJIT_VERBOSE 0
#define MJIT_MAX_CACHE 100
#define MJIT_MIN_CALLS 10000
#define YJIT_ENABLED 0

#define RB (ruby.get())
#define WASM_NULL 0
#define WASM_MEM(address) ((void *)&ruby->w2c_memory.data[address])
#define AWAIT(statement) do statement; while (w2c_ruby_mkxp_sandbox_yield(RB))
#define VALIDATE_MALLOC(ptr) do { if (ptr == WASM_NULL || ptr >= ruby.w2c_memory.size) throw SandboxOutOfMemoryException(); } while (0)

// This function is imported by wasm-rt-impl.c from wasm2c
extern "C" void mkxp_sandbox_trap_handler(wasm_rt_trap_t code) {
    throw SandboxTrapException();
}

usize Sandbox::sandbox_malloc(usize size) {
    usize buf = w2c_ruby_mkxp_sandbox_malloc(RB, size);

    // Verify that the returned pointer is non-null and the entire allocated buffer is in valid memory
    usize buf_end;
    if (buf == WASM_NULL || __builtin_add_overflow(buf, size, &buf_end) || buf_end >= ruby->w2c_memory.size) {
        throw SandboxOutOfMemoryException();
    }

    return buf;
}

void Sandbox::sandbox_free(usize ptr) {
    w2c_ruby_mkxp_sandbox_free(RB, ptr);
}

Sandbox::Sandbox() : ruby(new struct w2c_ruby), wasi(new wasi_t(ruby)) {
    try {
        // Initialize the sandbox
        wasm_rt_init();
        wasm2c_ruby_instantiate(RB, wasi.get());
        w2c_ruby_mkxp_sandbox_init(RB);

        // Determine Ruby command-line arguments
        std::vector<std::string> args{"mkxp-z"};
        args.push_back("-e ");
        if (MJIT_ENABLED) {
            std::string verboseLevel("--mjit-verbose=");
            std::string maxCache("--mjit-max-cache=");
            std::string minCalls("--mjit-min-calls=");
            args.push_back("--mjit");
            verboseLevel += std::to_string(MJIT_VERBOSE);
            maxCache += std::to_string(MJIT_MAX_CACHE);
            minCalls += std::to_string(MJIT_MIN_CALLS);
            args.push_back(verboseLevel.c_str());
            args.push_back(maxCache.c_str());
            args.push_back(minCalls.c_str());
        } else if (YJIT_ENABLED) {
            args.push_back("--yjit");
        }

        // Copy all the command-line arguments into the sandbox (sandboxed code can't access memory that's outside the sandbox!)
        usize argv_buf = sandbox_malloc(args.size() * sizeof(usize));
        for (usize i = 0; i < args.size(); ++i) {
            usize arg_buf = sandbox_malloc(args[i].length() + 1);
            std::strcpy((char *)WASM_MEM(arg_buf), args[i].c_str());
            WASM_SET(usize, argv_buf + i * sizeof(usize), arg_buf);
        }
        usize sysinit_buf = sandbox_malloc(sizeof(usize) + sizeof(u32));
        WASM_SET(u32, sysinit_buf + sizeof(usize), args.size());
        WASM_SET(usize, sysinit_buf, argv_buf);

        // Pass the command-line arguments to Ruby
        w2c_ruby_ruby_sysinit(RB, sysinit_buf + sizeof(usize), sysinit_buf);
        AWAIT(w2c_ruby_ruby_init_stack(RB, ruby->w2c_0x5F_stack_pointer));
        AWAIT(w2c_ruby_ruby_init(RB));
        usize node;
        AWAIT(node = w2c_ruby_ruby_options(RB, args.size(), argv_buf));

        // Start up Ruby executable node
        bool valid;
        u32 state;
        usize state_buf = sandbox_malloc(sizeof(usize));
        AWAIT(valid = w2c_ruby_ruby_executable_node(RB, node, state_buf));
        if (valid) {
            AWAIT(state = w2c_ruby_ruby_exec_node(RB, WASM_GET(u32, state_buf)));
        }
        if (!valid || state) {
            throw SandboxNodeException();
        }
        sandbox_free(state_buf);

        // Set the default encoding to UTF-8
        usize encoding;
        AWAIT(encoding = w2c_ruby_rb_utf8_encoding(RB));
        usize enc;
        AWAIT(enc = w2c_ruby_rb_enc_from_encoding(RB, encoding));
        AWAIT(w2c_ruby_rb_enc_set_default_internal(RB, enc));
        AWAIT(w2c_ruby_rb_enc_set_default_external(RB, enc));
    } catch (SandboxNodeException e) {
        wasm2c_ruby_free(RB);
        wasm_rt_free();
        throw e;
    } catch (SandboxOutOfMemoryException e) {
        wasm2c_ruby_free(RB);
        wasm_rt_free();
        throw e;
    } catch (SandboxTrapException e) {
        wasm2c_ruby_free(RB);
        wasm_rt_free();
        throw e;
    }
}

Sandbox::~Sandbox() {
    try {
        w2c_ruby_mkxp_sandbox_deinit(RB);
    } catch (SandboxTrapException) {}
    wasm2c_ruby_free(RB);
    wasm_rt_free();
}

VALUE Sandbox::rb_eval_string(const char *str) {
    usize buf = sandbox_malloc(std::strlen(str) + 1);
    std::strcpy((char *)WASM_MEM(buf), str);
    VALUE val;
    AWAIT(val = w2c_ruby_rb_eval_string(RB, buf));
    sandbox_free(buf);
    return val;
}
