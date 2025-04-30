/*
** viewport-binding.h
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

#ifndef MKXPZ_SANDBOX_VIEWPORT_BINDING_H
#define MKXPZ_SANDBOX_VIEWPORT_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "viewport.h"
#include "etc-binding.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type viewport_type;
    static VALUE viewport_class;

    SANDBOX_COROUTINE(viewport_binding_init,
        SANDBOX_DEF_ALLOC(viewport_type)
        SANDBOX_DEF_DFREE(Viewport)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Viewport *viewport;
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;
                VALUE obj;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (argc == 0 && rgssVer >= 3) {
                            GFX_LOCK;
                            viewport = new Viewport;
                        } else if (argc == 1) {
                            GFX_LOCK;
                            viewport = new Viewport(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0]));
                        } else {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            GFX_LOCK;
                            viewport = new Viewport(x, y, w, h);
                        }

                        set_private_data(self, viewport);

                        viewport->initDynAttribs();

                        SANDBOX_AWAIT(wrap_property, self, &viewport->getRect(), "rect", rect_class);
                        SANDBOX_AWAIT(wrap_property, self, &viewport->getColor(), "color", color_class);
                        SANDBOX_AWAIT(wrap_property, self, &viewport->getTone(), "tone", tone_class);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Viewport *viewport = get_private_data<Viewport>(self);
            if (viewport != NULL) {
                viewport->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Viewport *viewport = get_private_data<Viewport>(self);
            return viewport == NULL || viewport->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE flash(VALUE self, VALUE obj, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int duration;

                VALUE operator()(VALUE self, VALUE obj, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);
                        get_private_data<Viewport>(self)->flash(obj == SANDBOX_NIL ? NULL : &get_private_data<Color>(obj)->norm, duration);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, obj, value);
        }

        static VALUE update(VALUE self) {
            GFX_LOCK;
            get_private_data<Viewport>(self)->update();
            GFX_UNLOCK;
            return SANDBOX_NIL;
        }

        SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Rect, Rect, rect);
        SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Color, Color, color);
        SANDBOX_DEF_GFX_PROP_OBJ_VAL(Viewport, Tone, Tone, tone);
        SANDBOX_DEF_GFX_PROP_I(Viewport, OX, ox);
        SANDBOX_DEF_GFX_PROP_I(Viewport, OY, oy);
        SANDBOX_DEF_GFX_PROP_I(Viewport, Z, z);

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                viewport_type = sb()->rb_data_type("Viewport", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(viewport_class, rb_define_class, "Viewport", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, viewport_class, alloc);
                SANDBOX_AWAIT(rb_define_method, viewport_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, viewport_class, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, viewport_class, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, viewport_class, "flash", (VALUE (*)(ANYARGS))flash, 2);
                SANDBOX_AWAIT(rb_define_method, viewport_class, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_INIT_PROP_BIND(viewport_class, rect);
                SANDBOX_INIT_PROP_BIND(viewport_class, color);
                SANDBOX_INIT_PROP_BIND(viewport_class, tone);
                SANDBOX_INIT_PROP_BIND(viewport_class, ox);
                SANDBOX_INIT_PROP_BIND(viewport_class, oy);
                SANDBOX_INIT_PROP_BIND(viewport_class, z);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_VIEWPORT_BINDING_H
