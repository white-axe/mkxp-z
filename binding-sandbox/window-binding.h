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
#include "etc-binding.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type window_type;
    static VALUE window_class;

    SANDBOX_COROUTINE(window_binding_init,
        SANDBOX_DEF_ALLOC(window_type)
        SANDBOX_DEF_DFREE(Window)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Window *window;
                VALUE viewport_obj;
                Viewport *viewport;
                VALUE obj;

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

                        SANDBOX_AWAIT(wrap_property, self, &window->getCursorRect(), "cursor_rect", rect_class);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Window *window = get_private_data<Window>(self);
            if (window != NULL) {
                window->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Window *window = get_private_data<Window>(self);
            return SANDBOX_BOOL_TO_VALUE(window == NULL || window->isDisposed());
        }

        static VALUE update(VALUE self) {
            GFX_GUARD_EXC(get_private_data<Window>(self)->update();)
            return SANDBOX_NIL;
        }

        SANDBOX_DEF_GFX_PROP_OBJ_REF(Window, Viewport, Viewport, viewport);
        SANDBOX_DEF_GFX_PROP_OBJ_REF(Window, Bitmap, Windowskin, windowskin);
        SANDBOX_DEF_GFX_PROP_OBJ_REF(Window, Bitmap, Contents, contents);
        SANDBOX_DEF_GFX_PROP_OBJ_VAL(Window, Rect, CursorRect, cursor_rect);
        SANDBOX_DEF_GFX_PROP_B(Window, Stretch, stretch);
        SANDBOX_DEF_GFX_PROP_B(Window, Active, active);
        SANDBOX_DEF_GFX_PROP_B(Window, Visible, visible);
        SANDBOX_DEF_GFX_PROP_B(Window, Pause, pause);
        SANDBOX_DEF_GFX_PROP_I(Window, X, x);
        SANDBOX_DEF_GFX_PROP_I(Window, Y, y);
        SANDBOX_DEF_GFX_PROP_I(Window, Width, width);
        SANDBOX_DEF_GFX_PROP_I(Window, Height, height);
        SANDBOX_DEF_GFX_PROP_I(Window, OX, ox);
        SANDBOX_DEF_GFX_PROP_I(Window, OY, oy);
        SANDBOX_DEF_GFX_PROP_I(Window, Z, z);
        SANDBOX_DEF_GFX_PROP_I(Window, Opacity, opacity);
        SANDBOX_DEF_GFX_PROP_I(Window, BackOpacity, back_opacity);
        SANDBOX_DEF_GFX_PROP_I(Window, ContentsOpacity, contents_opacity);

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                window_type = sb()->rb_data_type("Window", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(window_class, rb_define_class, "Window", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, window_class, alloc);
                SANDBOX_AWAIT(rb_define_method, window_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, window_class, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_method, window_class, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, window_class, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_INIT_PROP_BIND(window_class, viewport);
                SANDBOX_INIT_PROP_BIND(window_class, windowskin);
                SANDBOX_INIT_PROP_BIND(window_class, contents);
                SANDBOX_INIT_PROP_BIND(window_class, cursor_rect);
                SANDBOX_INIT_PROP_BIND(window_class, stretch);
                SANDBOX_INIT_PROP_BIND(window_class, active);
                SANDBOX_INIT_PROP_BIND(window_class, visible);
                SANDBOX_INIT_PROP_BIND(window_class, pause);
                SANDBOX_INIT_PROP_BIND(window_class, x);
                SANDBOX_INIT_PROP_BIND(window_class, y);
                SANDBOX_INIT_PROP_BIND(window_class, width);
                SANDBOX_INIT_PROP_BIND(window_class, height);
                SANDBOX_INIT_PROP_BIND(window_class, ox);
                SANDBOX_INIT_PROP_BIND(window_class, oy);
                SANDBOX_INIT_PROP_BIND(window_class, z);
                SANDBOX_INIT_PROP_BIND(window_class, opacity);
                SANDBOX_INIT_PROP_BIND(window_class, back_opacity);
                SANDBOX_INIT_PROP_BIND(window_class, contents_opacity);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_WINDOW_BINDING_H
