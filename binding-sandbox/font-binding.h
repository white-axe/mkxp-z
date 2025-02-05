/*
** font-binding.h
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

#ifndef MKXPZ_SANDBOX_FONT_BINDING_H
#define MKXPZ_SANDBOX_FONT_BINDING_H

#include "sandbox.h"

namespace mkxp_sandbox {
    SANDBOX_COROUTINE(font_binding_init,
        VALUE klass;

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Font", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_method, klass, "name=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "size=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "bold=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "italic=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))todo, -1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_FONT_BINDING_H
