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

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type viewport_type;

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
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        // TODO: allow 0 arguments if RGSS version >= 3
                        if (argc == 1) {
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

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &viewport->getRect());
                        SANDBOX_AWAIT(rb_iv_set, self, "rect", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &viewport->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Tone");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &viewport->getTone());
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", obj);

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

        static VALUE get_rect(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "rect");
        }

        static VALUE set_rect(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setRect(*get_private_data<Rect>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "rect", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "color");
        }

        static VALUE set_color(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setColor(*get_private_data<Color>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "color", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_tone(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "tone");
        }

        static VALUE set_tone(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setTone(*get_private_data<Tone>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Viewport>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t ox;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Viewport>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t oy;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_z(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Viewport>(self)->getZ());
        }

        static VALUE set_z(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t z;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(z, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Viewport>(self)->setZ(z));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                viewport_type = sb()->rb_data_type("Viewport", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Viewport", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "rect", (VALUE (*)(ANYARGS))get_rect, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "rect=", (VALUE (*)(ANYARGS))set_rect, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "color", (VALUE (*)(ANYARGS))get_color, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))set_color, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "tone", (VALUE (*)(ANYARGS))get_tone, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "tone=", (VALUE (*)(ANYARGS))set_tone, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "ox", (VALUE (*)(ANYARGS))get_ox, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "ox=", (VALUE (*)(ANYARGS))set_ox, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "oy", (VALUE (*)(ANYARGS))get_oy, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "oy=", (VALUE (*)(ANYARGS))set_oy, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "z", (VALUE (*)(ANYARGS))get_z, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "z=", (VALUE (*)(ANYARGS))set_z, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_VIEWPORT_BINDING_H
