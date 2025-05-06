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
#include <mkxp-sandbox-ruby.h>
#include "mkxp-polyfill.h"
#include "sandbox.h"

#define RB (ruby.get())
#define WASM_NULL 0
#define WASM_MEM(address) ((void *)&ruby->w2c_memory.data[address])
#define AWAIT(statement) do statement; while (w2c_ruby_mkxp_sandbox_yield(RB))

using namespace mkxp_sandbox;

usize sandbox::sandbox_malloc(usize size) {
    usize buf = w2c_ruby_mkxp_sandbox_malloc(RB, size);

    // Verify that the returned pointer is non-null and the entire allocated buffer is in valid memory
    usize buf_end;
    if (buf == WASM_NULL || (buf_end = buf + size) < buf || buf_end >= ruby->w2c_memory.size) {
        throw std::bad_alloc();
    }

    return buf;
}

void sandbox::sandbox_free(usize ptr) {
    w2c_ruby_mkxp_sandbox_free(RB, ptr);
}

sandbox::sandbox() : ruby(new struct w2c_ruby), wasi(new wasi_t(ruby)), bindings(ruby), movie(nullptr), yielding(false), transitioning(false) {
    // Initialize the sandbox
    wasm2c_ruby_instantiate(RB, wasi.get());
    w2c_ruby_mkxp_sandbox_init(
        RB,
        0,              // heap_free_slots
        1.1,            // growth_factor
        0,              // growth_max_slots
        0,              // heap_free_slots_min_ratio
        0,              // heap_free_slots_goal_ratio
        0,              // heap_free_slots_max_ratio
        0,              // uncollectible_wb_unprotected_objects_limit_ratio
        0,              // oldobject_limit_factor
        1 * 0x100000,   // malloc_limit_min
        4 * 0x100000,   // malloc_limit_max
        1.1,            // malloc_limit_growth_factor
        4 * 0x100000,   // oldmalloc_limit_min
        8 * 0x100000,   // oldmalloc_limit_max
        1.1             // oldmalloc_limit_growth_factor
    );

    // Change the current working directory to the game directory
    usize chdir_buf = sandbox_malloc(sizeof("/game"));
    std::strcpy((char *)WASM_MEM(chdir_buf), "/game");
    w2c_ruby_mkxp_sandbox_chdir(RB, chdir_buf);
    sandbox_free(chdir_buf);

    // Determine Ruby command-line arguments
    std::vector<std::string> args{"mkxp-z"};
    args.push_back("/dist/bin/mkxp-z");

    // Copy all the command-line arguments into the sandbox (sandboxed code can't access memory that's outside the sandbox!)
    usize argv_buf = sandbox_malloc(args.size() * sizeof(usize));
    for (usize i = 0; i < args.size(); ++i) {
        usize arg_buf = sandbox_malloc(args[i].length() + 1);
        std::strcpy((char *)WASM_MEM(arg_buf), args[i].c_str());
        WASM_SET(usize, argv_buf + i * sizeof(usize), arg_buf);
    }

    // Pass the command-line arguments to Ruby
    AWAIT(w2c_ruby_ruby_init_stack(RB, w2c_ruby_rb_wasm_get_stack_pointer(RB)));
    AWAIT(w2c_ruby_ruby_init(RB));
    usize node;
    AWAIT(node = w2c_ruby_ruby_options(RB, args.size(), argv_buf));

    // Start up Ruby executable node
    bool valid;
    u32 state;
    usize state_buf = sandbox_malloc(sizeof(usize));
    AWAIT(valid = w2c_ruby_ruby_executable_node(RB, node, state_buf));
    if (valid) {
        AWAIT(state = w2c_ruby_ruby_exec_node(RB, node));
    }
    if (!valid || state) {
        std::abort();
    }
    sandbox_free(state_buf);

    // Set the default encoding to UTF-8
    usize encoding;
    AWAIT(encoding = w2c_ruby_rb_utf8_encoding(RB));
    usize enc;
    AWAIT(enc = w2c_ruby_rb_enc_from_encoding(RB, encoding));
    AWAIT(w2c_ruby_rb_enc_set_default_internal(RB, enc));
    AWAIT(w2c_ruby_rb_enc_set_default_external(RB, enc));
}

sandbox::~sandbox() {
    set_movie(nullptr);
    if (yielding) {
        w2c_ruby_asyncify_stop_unwind(ruby.get());
    }
    bindings.reset(); // Destroy the bindings before destroying the runtime since the bindings destructor requires the runtime to be alive
    wasm2c_ruby_free(RB);
}

Movie *sandbox::get_movie_from_main_thread() {
    return movie.load(std::memory_order_relaxed); // No need for synchronization because we always set the movie from the main thread
}

Movie *sandbox::get_movie_from_audio_thread() {
    return movie.load(std::memory_order_seq_cst);
}

void sandbox::set_movie(Movie *new_movie) {
    Movie *old_movie = get_movie_from_main_thread();
    if (old_movie == new_movie) {
        return;
    }
    if (old_movie != nullptr) {
        movie.store(nullptr, std::memory_order_seq_cst);
        AudioMutexGuard guard(movie_mutex);
        Graphics::stopMovie(old_movie);
    }
    if (new_movie != nullptr) {
        movie.store(new_movie, std::memory_order_seq_cst);
    }
}

const char *sandbox::getcwd() {
    if (w2c_ruby_mkxp_sandbox_getcwd(ruby.get())) {
        return (const char *)(**bindings + ruby->w2c_mkxp_sandbox_cwd);
    } else {
        return "/game";
    }
}
