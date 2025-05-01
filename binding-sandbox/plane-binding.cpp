/*
** plane-binding.cpp
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

#include "plane-binding.h"
#include "etc-binding.h"
#include "etc.h"
#include "plane.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::plane_class;
static struct bindings::rb_data_type plane_type;

SANDBOX_DEF_ALLOC(plane_type);
SANDBOX_DEF_DFREE(Plane);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        Plane *plane;
        VALUE viewport_obj;
        Viewport *viewport;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
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
                SANDBOX_AWAIT(rb_iv_set, self, "viewport", viewport_obj);

                set_private_data(self, plane);

                plane->initDynAttribs();

                SANDBOX_AWAIT(wrap_property, self, &plane->getColor(), "color", color_class);
                SANDBOX_AWAIT(wrap_property, self, &plane->getTone(), "tone", tone_class);
                GFX_UNLOCK
            }

            return SANDBOX_NIL;
        }
    };

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

SANDBOX_DEF_GFX_PROP_OBJ_REF(Plane, Viewport, Viewport, viewport);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Plane, Bitmap, Bitmap, bitmap);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Plane, Color, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Plane, Tone, Tone, tone);
SANDBOX_DEF_GFX_PROP_B(Plane, Visible, visible);
SANDBOX_DEF_GFX_PROP_I(Plane, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Plane, OY, oy);
SANDBOX_DEF_GFX_PROP_F(Plane, ZoomX, zoom_x);
SANDBOX_DEF_GFX_PROP_F(Plane, ZoomY, zoom_y);
SANDBOX_DEF_GFX_PROP_I(Plane, Z, z);
SANDBOX_DEF_GFX_PROP_I(Plane, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Plane, BlendType, blend_type);

void plane_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        plane_type = sb()->rb_data_type("Plane", NULL, dfree, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(plane_class, rb_define_class, "Plane", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, plane_class, alloc);
        SANDBOX_AWAIT(rb_define_method, plane_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, plane_class, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
        SANDBOX_AWAIT(rb_define_method, plane_class, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
        SANDBOX_INIT_PROP_BIND(plane_class, viewport);
        SANDBOX_INIT_PROP_BIND(plane_class, bitmap);
        SANDBOX_INIT_PROP_BIND(plane_class, color);
        SANDBOX_INIT_PROP_BIND(plane_class, tone);
        SANDBOX_INIT_PROP_BIND(plane_class, visible);
        SANDBOX_INIT_PROP_BIND(plane_class, ox);
        SANDBOX_INIT_PROP_BIND(plane_class, oy);
        SANDBOX_INIT_PROP_BIND(plane_class, zoom_x);
        SANDBOX_INIT_PROP_BIND(plane_class, zoom_y);
        SANDBOX_INIT_PROP_BIND(plane_class, z);
        SANDBOX_INIT_PROP_BIND(plane_class, opacity);
        SANDBOX_INIT_PROP_BIND(plane_class, blend_type);
    }
}
