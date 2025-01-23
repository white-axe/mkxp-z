/*
** input-binding.h
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

#ifndef MKXPZ_SANDBOX_INPUT_BINDING_H
#define MKXPZ_SANDBOX_INPUT_BINDING_H

#include "sandbox.h"

namespace mkxp_sandbox {
    static const char *codes[] = {
        "DOWN",
        "LEFT",
        "RIGHT",
        "UP",
        "C",
        "Z",
        "A",
        "B",
        "X",
        "Y",
        "L",
        "R",
        "SHIFT",
        "CTRL",
        "ALT",
        "F5",
        "F6",
        "F7",
        "F8",
        "F9",
        "MOUSELEFT",
        "MOUSEMIDDLE",
        "MOUSERIGHT",
        "MOUSEX1",
        "MOUSEX2",
    };

    SANDBOX_COROUTINE(input_binding_init,
        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        static VALUE todo_bool(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_FALSE;
        }

        static VALUE todo_number(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return sb()->bind<rb_ll2inum>()()(0);
        }

        VALUE module;
        size_t i;
        ID id;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Input");
                SANDBOX_AWAIT(rb_define_module_function, module, "delta", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "update", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "press?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "trigger?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeat?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "release?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "count", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "time?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "pressex?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "triggerex?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeatex?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "releaseex?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeatcount", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "timeex?", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "dir4", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "dir8", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_x", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_y", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "scroll_v", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_in_window", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_in_window?", (VALUE (*)(ANYARGS))todo_bool, -1);

                for (i = 0; i < sizeof(codes) / sizeof(*codes); ++i) {
                    SANDBOX_AWAIT_AND_SET(id, rb_intern, codes[i]);
                    SANDBOX_AWAIT(rb_const_set, module, id, SANDBOX_NIL);
                }
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_INPUT_BINDING_H
