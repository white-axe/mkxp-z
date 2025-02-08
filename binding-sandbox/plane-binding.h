/*
** plane-binding.h
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

#ifndef MKXPZ_SANDBOX_PLANE_BINDING_H
#define MKXPZ_SANDBOX_PLANE_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "plane.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type plane_type;

    SANDBOX_COROUTINE(plane_binding_init,
        SANDBOX_DEF_ALLOC(plane_type)
        SANDBOX_DEF_DFREE(Plane)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Plane *plane;
                VALUE viewport_obj;
                Viewport *viewport;
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;
                ID id;
                VALUE klass;
                VALUE obj;
                VALUE ary;
                unsigned int i;

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
                        plane = new Plane(viewport);

                        set_private_data(self, plane);

                        plane->initDynAttribs();

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &plane->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Tone");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &plane->getTone());
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", obj);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Plane *plane = get_private_data<Plane>(self);
            if (plane != NULL) {
                plane->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Plane *plane = get_private_data<Plane>(self);
            return plane == NULL || plane->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE get_bitmap(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "bitmap");
        }

        static VALUE set_bitmap(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setBitmap(get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "bitmap", value);
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
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setColor(*get_private_data<Color>(value)));
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
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setTone(*get_private_data<Tone>(value)));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_visible(VALUE self) {
            return get_private_data<Plane>(self)->getVisible() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Plane>(self)->setVisible(value != SANDBOX_FALSE && value != SANDBOX_NIL));
            return value;
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t ox;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t oy;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_zoom_x(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getZoomX());
        }

        static VALUE set_zoom_x(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t zoom_x;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(zoom_x, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setZoomX(zoom_x));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_zoom_y(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getZoomY());
        }

        static VALUE set_zoom_y(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t zoom_y;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(zoom_y, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setZoomY(zoom_y));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_z(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getZ());
        }

        static VALUE set_z(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t z;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(z, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setZ(z));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getOpacity());
        }

        static VALUE set_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t opacity;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setOpacity(opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_blend_type(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Plane>(self)->getBlendType());
        }

        static VALUE set_blend_type(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t blend_type;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(blend_type, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Plane>(self)->setBlendType(blend_type));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                plane_type = sb()->rb_data_type("Plane", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Plane", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "bitmap", (VALUE (*)(ANYARGS))get_bitmap, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "bitmap=", (VALUE (*)(ANYARGS))set_bitmap, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "color", (VALUE (*)(ANYARGS))get_color, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))set_color, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "tone", (VALUE (*)(ANYARGS))get_tone, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "tone=", (VALUE (*)(ANYARGS))set_tone, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "visible", (VALUE (*)(ANYARGS))get_visible, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "visible=", (VALUE (*)(ANYARGS))set_visible, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "ox", (VALUE (*)(ANYARGS))get_ox, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "ox=", (VALUE (*)(ANYARGS))set_ox, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "oy", (VALUE (*)(ANYARGS))get_oy, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "oy=", (VALUE (*)(ANYARGS))set_oy, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "zoom_x", (VALUE (*)(ANYARGS))get_zoom_x, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "zoom_x=", (VALUE (*)(ANYARGS))set_zoom_x, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "zoom_y", (VALUE (*)(ANYARGS))get_zoom_y, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "zoom_y=", (VALUE (*)(ANYARGS))set_zoom_y, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "z", (VALUE (*)(ANYARGS))get_z, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "z=", (VALUE (*)(ANYARGS))set_z, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "opacity", (VALUE (*)(ANYARGS))get_opacity, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "opacity=", (VALUE (*)(ANYARGS))set_opacity, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "blend_type", (VALUE (*)(ANYARGS))get_blend_type, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "blend_type=", (VALUE (*)(ANYARGS))set_blend_type, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_PLANE_BINDING_H
