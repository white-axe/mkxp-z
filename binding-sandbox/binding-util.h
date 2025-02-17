/*
** binding-util.h
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

#ifndef MKXPZ_SANDBOX_BINDING_UTIL_H
#define MKXPZ_SANDBOX_BINDING_UTIL_H

#include "core.h"
#include "sandbox.h"

#define GFX_GUARD_EXC(exp) exp // TODO: implement

#define SANDBOX_DEF_ALLOC(rbtype) \
    static VALUE alloc(VALUE _klass) { \
        SANDBOX_COROUTINE(alloc, \
            VALUE _obj; \
            VALUE operator()(VALUE _klass) { \
                BOOST_ASIO_CORO_REENTER (this) { \
                    SANDBOX_AWAIT_AND_SET(_obj, mkxp_sandbox::rb_data_typed_object_wrap, _klass, 0, rbtype); \
                } \
                return _obj; \
            } \
        ) \
        return mkxp_sandbox::sb()->bind<struct alloc>()()(_klass); \
    }

#define SANDBOX_DEF_DFREE(T) \
    static void dfree(wasm_ptr_t _buf) { \
        delete *(T **)(**mkxp_sandbox::sb() + _buf); \
    }

#define SANDBOX_DEF_LOAD(T) \
    static VALUE load(VALUE _self, VALUE _serialized) { \
        return mkxp_sandbox::sb()->bind<struct _load_inner<T>>()()(_self, _serialized); \
    }

namespace mkxp_sandbox {
    // Given Ruby typed data `obj`, retrieves the private data field of `obj`.
    template <typename T> inline T *get_private_data(VALUE obj) {
        return *(T **)(**sb() + *(wasm_ptr_t *)(**sb() + sb()->rtypeddata_data(obj)));
    }

    // Given Ruby typed data `obj`, stores `ptr` into the private data field of `obj`.
    SANDBOX_COROUTINE(set_private_data,
        wasm_ptr_t data;
        wasm_ptr_t buf;

        void operator()(VALUE obj, void *ptr) {
            BOOST_ASIO_CORO_REENTER (this) {
                /* RGSS's behavior is to just leak memory if a disposable is reinitialized,
                 * with the original disposable being left permanently instantiated,
                 * but that's (1) bad, and (2) would currently cause memory access issues
                 * when things like a sprite's src_rect inevitably get GC'd, so we're not
                 * copying that. */

                data = sb()->rtypeddata_data(obj);

                // Free the old value if it already exists (initialize called twice?)
                if (*(wasm_ptr_t *)(**sb() + data) != 0 && *(void **)(**sb() + *(wasm_ptr_t *)(**sb() + data)) != ptr) {
                    sb()->rtypeddata_dfree(obj, *(wasm_ptr_t *)(**sb() + data));
                    sb()->sandbox_free(*(wasm_ptr_t *)(**sb() + data));
                    *(wasm_ptr_t *)(**sb() + data) = 0;
                }

                if (*(wasm_ptr_t *)(**sb() + data) == 0) {
                    SANDBOX_AWAIT_AND_SET(buf, sandbox_malloc, sizeof(void *));
                    *(void **)(**sb() + buf) = ptr;
                    *(wasm_ptr_t *)(**sb() + data) = buf;
                }
            }
        }
    )

    // Gets the length of a Ruby object.
    SANDBOX_COROUTINE(get_length,
        ID id;
        VALUE length_value;
        wasm_size_t result;

        wasm_size_t operator()(VALUE obj) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "length");
                SANDBOX_AWAIT_AND_SET(length_value, rb_funcall, obj, id, 0);
                SANDBOX_AWAIT_AND_SET(result, rb_num2ulong, length_value);
            }

            return result;
        }
    )

    // Internal-use utility coroutine for the `SANDBOX_DEF_LOAD` macro.
    template <typename T> SANDBOX_COROUTINE(_load_inner,
        VALUE obj;
        wasm_ptr_t ptr;
        wasm_size_t len;

        VALUE operator()(VALUE self, VALUE serialized) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, self);
                SANDBOX_AWAIT_AND_SET(ptr, rb_string_value_ptr, &serialized);
                SANDBOX_AWAIT_AND_SET(len, get_length, serialized);
                SANDBOX_AWAIT(set_private_data, obj, T::deserialize((const char *)(**sb() + ptr), len)); // TODO: free when sandbox is deallocated
            }

            return obj;
        }
    )

    // Prints the backtrace of a Ruby exception to the log.
    SANDBOX_COROUTINE(log_backtrace,
        ID id;
        VALUE backtrace;
        VALUE separator;
        wasm_ptr_t backtrace_str;

        void operator()(VALUE exception) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_p, exception);
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "backtrace");
                SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, exception, id, 0);
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "join");
                SANDBOX_AWAIT_AND_SET(separator, rb_str_new_cstr, "\n\t");
                SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, backtrace, id, 1, separator);
                SANDBOX_AWAIT_AND_SET(backtrace_str, rb_string_value_cstr, &backtrace);
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", **sb() + backtrace_str);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_BINDING_UTIL_H
