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

#include <memory>
#include <boost/optional.hpp>
#include <mkxp-sandbox-bindgen.h>
#include "types.h"

#define SANDBOX_COROUTINE(name, definition) struct name : boost::asio::coroutine { BOOST_TYPE_INDEX_REGISTER_CLASS inline name(struct mkxp_sandbox::binding_base &bind) {} definition };

#define SANDBOX_AWAIT(coroutine, ...) \
    do { \
        { \
            struct mkxp_sandbox::bindings::stack_frame_guard<struct coroutine> frame = mkxp_sandbox::sb()->bind<struct coroutine>(); \
            frame()(__VA_ARGS__); \
            if (frame().is_complete()) break; \
        } \
        BOOST_ASIO_CORO_YIELD; \
    } while (1)

#define SANDBOX_AWAIT_AND_SET(variable, coroutine, ...) \
    do { \
        { \
            struct mkxp_sandbox::bindings::stack_frame_guard<struct coroutine> frame = mkxp_sandbox::sb()->bind<struct coroutine>(); \
            auto ret = frame()(__VA_ARGS__); \
            if (frame().is_complete()) { \
                variable = ret; \
                break; \
            } \
        } \
        BOOST_ASIO_CORO_YIELD; \
    } while (1)

#define SANDBOX_YIELD \
    do { \
        sb()._begin_yield(); \
        BOOST_ASIO_CORO_YIELD; \
        sb()._end_yield(); \
    } while (0)

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
        bool yielding;
        usize sandbox_malloc(usize size);
        void sandbox_free(usize ptr);

        public:
        inline struct mkxp_sandbox::bindings &operator*() noexcept { return *bindings; }
        inline struct mkxp_sandbox::bindings *operator->() noexcept { return &*bindings; }
        sandbox();
        ~sandbox();

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
