/*
** binding-base.h
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

#ifndef MKXPZ_SANDBOX_BINDING_BASE_H
#define MKXPZ_SANDBOX_BINDING_BASE_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
#include <boost/container_hash/hash.hpp>
#include <boost/type_index.hpp>
#include <boost/asio/coroutine.hpp>
#include <mkxp-retro-ruby.h>
#include "binding-sandbox/types.h"

#if WABT_BIG_ENDIAN
#  define SERIALIZE_32(value) __builtin_bswap32(value)
#  define SERIALIZE_64(value) __builtin_bswap64(value)
#else
#  define SERIALIZE_32(value) (value)
#  define SERIALIZE_64(value) (value)
#endif

#ifdef MKXPZ_RETRO_MEMORY64
#  define SERIALIZE_VALUE(value) SERIALIZE_64(value)
#else
#  define SERIALIZE_VALUE(value) SERIALIZE_32(value)
#endif

namespace mkxp_sandbox {
    struct binding_base {
        private:

        typedef std::tuple<wasm_ptr_t, wasm_ptr_t, wasm_ptr_t> key_t;

        struct stack_frame {
            struct binding_base &bind;
            void (*destructor)(void *ptr);
            boost::typeindex::type_index type;
            wasm_ptr_t ptr;
            stack_frame(struct binding_base &bind, void (*destructor)(void *ptr), boost::typeindex::type_index type, wasm_ptr_t ptr);
            ~stack_frame();
        };

        struct fiber {
            key_t key;
            std::vector<struct stack_frame> stack;
            size_t stack_ptr;
        };

        wasm_ptr_t next_func_ptr;
        std::shared_ptr<struct w2c_ruby> _instance;
        std::unordered_map<key_t, struct fiber, boost::hash<key_t>> fibers;

        public:

        binding_base(std::shared_ptr<struct w2c_ruby> m);
        ~binding_base();
        struct w2c_ruby &instance() const noexcept;
        uint8_t *get() const noexcept;
        uint8_t *operator*() const noexcept;
        wasm_ptr_t sandbox_malloc(wasm_size_t);
        void sandbox_free(wasm_ptr_t ptr);
        wasm_ptr_t sandbox_create_func_ptr();
        wasm_ptr_t rtypeddata_data(VALUE obj) const noexcept;
        void rtypeddata_dmark(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dfree(wasm_ptr_t data, wasm_ptr_t ptr);
        wasm_size_t rtypeddata_dsize(wasm_ptr_t data, wasm_ptr_t ptr);
        void rtypeddata_dcompact(wasm_ptr_t data, wasm_ptr_t ptr);

        template <typename T> struct stack_frame_guard {
            friend struct binding_base;

            private:

            struct binding_base &bind;
            struct fiber &fiber;
            wasm_ptr_t ptr;

            static void stack_frame_destructor(void *ptr) {
                ((T *)ptr)->~T();
            }

            static struct fiber &init_fiber(struct binding_base &bind) {
                key_t key = {
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_entry_point),
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_arg0),
                    *(wasm_ptr_t *)(*bind + bind.instance().w2c_mkxp_sandbox_fiber_arg1),
                };
                if (bind.fibers.count(key) == 0) {
                    bind.fibers[key] = (struct fiber){.key = key};
                }
                return bind.fibers[key];
            }

            static wasm_ptr_t init_inner(struct binding_base &bind, struct fiber &fiber) {
                wasm_ptr_t sp = w2c_ruby_rb_wasm_get_stack_pointer(&bind.instance());

                if (fiber.stack_ptr == fiber.stack.size()) {
                    fiber.stack.emplace_back(
                        bind,
                        stack_frame_destructor,
                        boost::typeindex::type_id<T>(),
                        (sp -= sizeof(T))
                    );
                    assert(sp % sizeof(VALUE) == 0);
                    new(*bind + sp) T(bind);
                } else if (fiber.stack_ptr > fiber.stack.size()) {
                    throw SandboxTrapException();
                }

                if (fiber.stack[fiber.stack_ptr].type == boost::typeindex::type_id<T>()) {
                    w2c_ruby_rb_wasm_set_stack_pointer(&bind.instance(), sp);
                    return fiber.stack[fiber.stack_ptr++].ptr;
                } else {
                    while (fiber.stack.size() > fiber.stack_ptr) {
                        fiber.stack.pop_back();
                    }
                    ++fiber.stack_ptr;
                    fiber.stack.emplace_back(
                        bind,
                        stack_frame_destructor,
                        boost::typeindex::type_id<T>(),
                        (sp -= sizeof(T))
                    );
                    assert(sp % sizeof(VALUE) == 0);
                    new(*bind + sp) T(bind);
                    w2c_ruby_rb_wasm_set_stack_pointer(&bind.instance(), sp);
                    return sp;
                }
            }

            stack_frame_guard(struct binding_base &b) : bind(b), fiber(init_fiber(b)), ptr(init_inner(b, fiber)) {}

            public:

            ~stack_frame_guard() {
                if (get()->is_complete()) {
                    while (fiber.stack.size() > fiber.stack_ptr) {
                        fiber.stack.pop_back();
                    }

                    // Check for stack corruptions
                    assert(fiber.stack.size() == fiber.stack_ptr);
                    assert(fiber.stack.back().type == boost::typeindex::type_id<T>());

                    w2c_ruby_rb_wasm_set_stack_pointer(&bind.instance(), fiber.stack.back().ptr + sizeof(T));
                    fiber.stack.pop_back();
                }

                --fiber.stack_ptr;

                if (fiber.stack.empty()) {
                    bind.fibers.erase(fiber.key);
                }
            }

            inline T *get() const noexcept {
                return (T *)(*bind + ptr);
            }

            inline T &operator()() const noexcept {
                return *get();
            }
        };

        template <typename T> struct stack_frame_guard<T> bind() {
            return *this;
        }
    };
}

#undef SERIALIZE_32
#undef SERIALIZE_64
#undef SERIALIZE_VALUE

#endif // MKXPZ_SANDBOX_BINDING_BASE
