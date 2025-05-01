/*
** windowvx-binding.cpp
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

#include "windowvx-binding.h"
#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "viewportelement-binding.h"
#include "bitmap.h"
#include "windowvx.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::windowvx_class;
static struct bindings::rb_data_type windowvx_type;

SANDBOX_DEF_ALLOC(windowvx_type);
SANDBOX_DEF_DFREE(WindowVX);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE obj;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                x = y = w = h = 0;

                GFX_LOCK;
                if (rgssVer >= 3) {
                    if (argc == 4) {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                        SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                    }
                    WindowVX *window = new WindowVX(x, y, w, h);
                    set_private_data(self, window);
                    window->initDynAttribs();
                } else {
                    SANDBOX_AWAIT(viewportelement_initialize<WindowVX>, argc, argv, self);
                }

                SANDBOX_AWAIT(wrap_property, self, &get_private_data<WindowVX>(self)->getCursorRect(), "cursor_rect", rect_class);

                if (rgssVer >= 3) {
                    SANDBOX_AWAIT(wrap_property, self, &get_private_data<WindowVX>(self)->getTone(), "tone", tone_class);
                }

                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, bitmap_class);
                set_private_data(obj, new Bitmap(1, 1));
                SANDBOX_AWAIT(bitmap_init_props, obj);
                SANDBOX_AWAIT(rb_iv_set, self, "contents", obj);
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE update(VALUE self) {
    GFX_GUARD_EXC(get_private_data<WindowVX>(self)->update();)
    return SANDBOX_NIL;
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(WindowVX, Bitmap, Windowskin, windowskin);
SANDBOX_DEF_GFX_PROP_OBJ_REF(WindowVX, Bitmap, Contents, contents);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(WindowVX, Rect, CursorRect, cursor_rect);
SANDBOX_DEF_GFX_PROP_B(WindowVX, Active, active);
SANDBOX_DEF_GFX_PROP_B(WindowVX, Pause, pause);
SANDBOX_DEF_GFX_PROP_I(WindowVX, X, x);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Y, y);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Width, width);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Height, height);
SANDBOX_DEF_GFX_PROP_I(WindowVX, OX, ox);
SANDBOX_DEF_GFX_PROP_I(WindowVX, OY, oy);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, BackOpacity, back_opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, ContentsOpacity, contents_opacity);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Openness, openness);

static VALUE move(VALUE self, VALUE xv, VALUE yv, VALUE wv, VALUE hv) {
    struct coro : boost::asio::coroutine {
        WindowVX *window;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;

        VALUE operator()(VALUE self, VALUE xv, VALUE yv, VALUE wv, VALUE hv) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(x, rb_num2int, xv);
                SANDBOX_AWAIT_AND_SET(y, rb_num2int, yv);
                SANDBOX_AWAIT_AND_SET(w, rb_num2int, wv);
                SANDBOX_AWAIT_AND_SET(h, rb_num2int, hv);
                GFX_LOCK;
                get_private_data<WindowVX>(self)->move(x, y, w, h);
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, xv, yv, wv, hv);
}

static VALUE is_open(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->isOpen());
}

static VALUE is_closed(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(get_private_data<WindowVX>(self)->isClosed());
}

SANDBOX_DEF_GFX_PROP_B(WindowVX, ArrowsVisible, arrows_visible);
SANDBOX_DEF_GFX_PROP_I(WindowVX, Padding, padding);
SANDBOX_DEF_GFX_PROP_I(WindowVX, PaddingBottom, padding_bottom);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(WindowVX, Tone, Tone, tone);

void windowvx_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        windowvx_type = sb()->rb_data_type("Window", NULL, dfree, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(windowvx_class, rb_define_class, "Window", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, windowvx_class, alloc);
        SANDBOX_AWAIT(rb_define_method, windowvx_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<WindowVX>, windowvx_class);
        SANDBOX_AWAIT(viewportelement_binding_init<WindowVX>, windowvx_class);
        SANDBOX_AWAIT(rb_define_method, windowvx_class, "update", (VALUE (*)(ANYARGS))update, 0);
        SANDBOX_INIT_PROP_BIND(windowvx_class, windowskin);
        SANDBOX_INIT_PROP_BIND(windowvx_class, contents);
        SANDBOX_INIT_PROP_BIND(windowvx_class, cursor_rect);
        SANDBOX_INIT_PROP_BIND(windowvx_class, active);
        SANDBOX_INIT_PROP_BIND(windowvx_class, pause);
        SANDBOX_INIT_PROP_BIND(windowvx_class, x);
        SANDBOX_INIT_PROP_BIND(windowvx_class, y);
        SANDBOX_INIT_PROP_BIND(windowvx_class, width);
        SANDBOX_INIT_PROP_BIND(windowvx_class, height);
        SANDBOX_INIT_PROP_BIND(windowvx_class, ox);
        SANDBOX_INIT_PROP_BIND(windowvx_class, oy);
        SANDBOX_INIT_PROP_BIND(windowvx_class, opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, back_opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, contents_opacity);
        SANDBOX_INIT_PROP_BIND(windowvx_class, openness);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "move", (VALUE (*)(ANYARGS))move, 4);
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "open?", (VALUE (*)(ANYARGS))is_open, 0);
            SANDBOX_AWAIT(rb_define_method, windowvx_class, "close?", (VALUE (*)(ANYARGS))is_closed, 0);
            SANDBOX_INIT_PROP_BIND(windowvx_class, arrows_visible);
            SANDBOX_INIT_PROP_BIND(windowvx_class, padding);
            SANDBOX_INIT_PROP_BIND(windowvx_class, padding_bottom);
            SANDBOX_INIT_PROP_BIND(windowvx_class, tone);
        }
    }
}
