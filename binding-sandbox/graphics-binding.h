/*
** graphics-binding.h
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

#ifndef MKXPZ_SANDBOX_GRAPHICS_BINDING_H
#define MKXPZ_SANDBOX_GRAPHICS_BINDING_H

#include "sandbox.h"
#include "sharedstate.h"
#include "graphics.h"

namespace mkxp_sandbox {
    SANDBOX_COROUTINE(graphics_binding_init,
        static VALUE update(VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_YIELD;
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE frame_reset(VALUE self) {
            GFX_LOCK;
            shState->graphics().frameReset();
            GFX_UNLOCK;
            return SANDBOX_NIL;
        }

        static VALUE get_frame_count(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().getFrameCount());
        }

        static VALUE set_frame_count(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int frame_count;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(frame_count, rb_num2int, value);
                        GFX_LOCK;
                        shState->graphics().setFrameCount(frame_count);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_frame_rate(VALUE self) {
            return sb()->bind<struct rb_float_new>()()(60.0); // TODO: use actual FPS
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        VALUE module;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Graphics");
                SANDBOX_AWAIT(rb_define_module_function, module, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "freeze", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "transition", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_reset", (VALUE (*)(ANYARGS))frame_reset, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_count", (VALUE (*)(ANYARGS))get_frame_count, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_count=", (VALUE (*)(ANYARGS))set_frame_count, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_rate", (VALUE (*)(ANYARGS))get_frame_rate, 0);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_GRAPHICS_BINDING_H
