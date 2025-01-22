/*
** window-binding.h
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

#ifndef MKXPZ_SANDBOX_WINDOW_BINDING_H
#define MKXPZ_SANDBOX_WINDOW_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "window.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type window_type;

    SANDBOX_COROUTINE(window_binding_init,
        SANDBOX_DEF_ALLOC(window_type)
        SANDBOX_DEF_DFREE(Window)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Window *window;
                VALUE viewport_obj;
                Viewport *viewport;
                VALUE klass;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        viewport_obj = SANDBOX_NIL;
                        viewport = NULL;
                        if (argc > 0) {
                            viewport_obj = *(VALUE *)(**sb() + argv);
                            if (viewport_obj != SANDBOX_NIL) {
                                viewport = get_private_data<Viewport>(viewport_obj);
                            }
                        }

                        GFX_LOCK
                        window = new Window(viewport);
                        SANDBOX_AWAIT(rb_iv_set, self, "viewport", viewport_obj);

                        set_private_data(self, window);
                        window->initDynAttribs();
                        GFX_UNLOCK
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE get_contents(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "contents");
        }

        static VALUE set_contents(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Window>(self)->setContents(value == SANDBOX_NIL ? NULL : get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "contents", value);
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return self;
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                window_type = sb()->rb_data_type("Window", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Window", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "windowskin=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "contents", (VALUE (*)(ANYARGS))get_contents, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "contents=", (VALUE (*)(ANYARGS))set_contents, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "stretch=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "cursor_rect=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "active=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "pause=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "x=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "y=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "width=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "height=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "z=", (VALUE (*)(ANYARGS))todo, -1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_WINDOW_BINDING_H
