/*
** sandbox.h
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

#ifndef MKXPZ_SANDBOX_H
#define MKXPZ_SANDBOX_H

#include <atomic>
#include <memory>
#include <vector>
#include <boost/optional.hpp>
#include <mkxp-sandbox-bindgen.h>
#include "wasm-types.h"
#include "audio.h"
#include "etc.h"
#include "graphics.h"

namespace mkxp_sandbox {
    struct sandbox;
}

namespace mkxp_retro {
    extern boost::optional<struct mkxp_sandbox::sandbox> sandbox;
}

namespace mkxp_sandbox {
    struct sandbox {
        private:
        std::shared_ptr<struct w2c_ruby> ruby;
        std::unique_ptr<struct w2c_wasi__snapshot__preview1> wasi;
        boost::optional<struct mkxp_sandbox::bindings> bindings;
        std::atomic<Movie *> movie;
        bool yielding;
        wasm_ptr_t sandbox_malloc(wasm_size_t size);
        void sandbox_free(wasm_ptr_t ptr);

        public:
        Exception e;
        std::vector<uint8_t> script_decode_buffer;
        std::vector<std::string> font_names_buffer;
        Bitmap *trans_map;
        Color bitmap_pixel_buffer;
        AudioMutex movie_mutex;
        bool transitioning;
        inline struct mkxp_sandbox::bindings &operator*() noexcept { return *bindings; }
        inline struct mkxp_sandbox::bindings *operator->() noexcept { return &*bindings; }
        sandbox();
        ~sandbox();
        bool sandbox_serialize_fdtable(void *&data, wasm_size_t &max_size) const;
        Movie *get_movie_from_main_thread();
        Movie *get_movie_from_audio_thread();
        void set_movie(Movie *new_movie);

        // Gets the current working directory in the sandbox.
        // The returned pointer doesn't need to be freed,
        // and lives until the next time this function is called, or until the sandbox is destroyed,
        // whichever comes first.
        const char *getcwd();

        // Internal utility method of the `SANDBOX_YIELD` macro.
        inline void _begin_yield() {
            yielding = true;
            w2c_ruby_asyncify_start_unwind(ruby.get(), ruby->w2c_mkxp_sandbox_async_buf);
        }

        // Internal utility method of the `SANDBOX_YIELD` macro.
        inline void _end_yield() {
            w2c_ruby_asyncify_stop_rewind(ruby.get());
        }

        // Executes the given coroutine as the top-level coroutine. Don't call this from inside of another coroutine; use `sb()->bind<T>()` instead.
        // Returns whether or not the coroutine completed execution.
        template <typename T> inline bool run() {
            if (yielding) {
                w2c_ruby_asyncify_start_rewind(ruby.get(), ruby->w2c_mkxp_sandbox_async_buf);
                yielding = false;
            }
            for (;;) {
                {
                    struct mkxp_sandbox::bindings::stack_frame_guard<T> frame = bindings->bind<T>();
                    frame()();
                    if (yielding || frame().is_complete()) break;
                }
                w2c_ruby_mkxp_sandbox_yield(ruby.get());
            }
            return !yielding;
        }
    };

    inline struct sandbox &sb() noexcept {
        return *mkxp_retro::sandbox;
    }
}

#endif // MKXPZ_SANDBOX_H
