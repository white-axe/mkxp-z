/*
** serializable-binding.h
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

#ifndef MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H
#define MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H

#include "binding-util.h"

namespace mkxp_sandbox {
    template <class C> VALUE serializable_load(VALUE klass, VALUE src) {
        struct coro : boost::asio::coroutine {
        private:
            VALUE obj;
            wasm_ptr_t ptr;
            wasm_size_t len;

        public:
            VALUE operator()(VALUE klass, VALUE src) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                    SANDBOX_AWAIT_AND_SET(ptr, rb_string_value_ptr, &src);
                    SANDBOX_AWAIT_AND_SET(len, get_bytesize, src);
                    set_private_data(obj, C::deserialize((const char *)(**sb() + ptr), len)); /* TODO: free when sandbox is deallocated */
                }

                return obj;
            }
        };

        return sb()->bind<struct coro>()()(klass, src);
    }

    template <class C> VALUE serializable_dump(VALUE self, VALUE depth) {
        struct coro : boost::asio::coroutine {
        private:
            VALUE obj;
            wasm_ptr_t ptr;
            wasm_size_t len;

        public:
            VALUE operator()(VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    len = get_private_data<C>(self)->serialSize();
                    SANDBOX_AWAIT_AND_SET(obj, rb_str_new_cstr, "");
                    SANDBOX_AWAIT(rb_str_resize, obj, len);
                    SANDBOX_AWAIT_AND_SET(ptr, rb_string_value_ptr, &obj);
                    get_private_data<C>(self)->serialize((char *)(**sb() + ptr));
                }

                return obj;
            }
        };

        return sb()->bind<struct coro>()()(self);
    }

    template <class C> struct serializable_binding_init : boost::asio::coroutine {
        void operator()(VALUE klass) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))serializable_load<C>, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "_dump", (VALUE (*)(ANYARGS))serializable_dump<C>, 1);
            }
        }
    };
}

#endif // MKXPZ_SANDBOX_SERIALIZABLE_BINDING_H
