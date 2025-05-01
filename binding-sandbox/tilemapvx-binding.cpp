/*
** tilemapvx-binding.cpp
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

#include "tilemapvx-binding.h"
#include "disposable-binding.h"
#include "tilemapvx.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::tilemapvx_class;
VALUE mkxp_sandbox::bitmap_array_class;
static struct bindings::rb_data_type tilemapvx_type;
static struct bindings::rb_data_type bitmap_array_type;

struct bitmap_array_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC(bitmap_array_type);

    static VALUE get(VALUE self, VALUE i) {
        struct coro : boost::asio::coroutine {
            VALUE ary;
            wasm_size_t index;
            VALUE value;

            VALUE operator()(VALUE self, VALUE i) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT_AND_SET(ary, rb_iv_get, self, "array");
                    SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);
                    SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, ary, index);
                }

                return value;
            }
        };

        return sb()->bind<struct coro>()()(self, i);
    }

    static VALUE set(VALUE self, VALUE i, VALUE obj) {
        struct coro : boost::asio::coroutine {
            VALUE ary;
            wasm_size_t index;
            VALUE value;

            VALUE operator()(VALUE self, VALUE i, VALUE obj) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (get_private_data<TilemapVX::BitmapArray>(self) == nullptr) {
                        return self;
                    }

                    SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);

                    GFX_LOCK;
                    get_private_data<TilemapVX::BitmapArray>(self)->set(index, get_private_data<Bitmap>(obj));
                    SANDBOX_AWAIT_AND_SET(ary, rb_iv_get, self, "array");
                    SANDBOX_AWAIT(rb_ary_store, ary, index, obj);
                    GFX_UNLOCK;
                }

                return self;
            }
        };

        return sb()->bind<struct coro>()()(self, i, obj);
    }

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            bitmap_array_type = sb()->rb_data_type("BitmapArray", NULL, NULL, NULL, NULL, 0, 0, 0);
            SANDBOX_AWAIT_AND_SET(bitmap_array_class, rb_define_class, "BitmapArray", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, bitmap_array_class, alloc);

            SANDBOX_AWAIT(rb_define_method, bitmap_array_class, "[]", (VALUE (*)(ANYARGS))get, 1);
            SANDBOX_AWAIT(rb_define_method, bitmap_array_class, "[]=", (VALUE (*)(ANYARGS))set, 2);
        }
    }
};

SANDBOX_DEF_ALLOC(tilemapvx_type);

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE viewport_obj;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
        VALUE obj;
        VALUE ary;
        uint32_t i;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                {
                    viewport_obj = SANDBOX_NIL;
                    Viewport *viewport = nullptr;
                    if (argc > 0) {
                        viewport_obj = *(VALUE *)(**sb() + argv);
                        if (viewport_obj != SANDBOX_NIL) {
                            viewport = get_private_data<Viewport>(viewport_obj);
                        }
                    }

                    GFX_LOCK;
                    set_private_data(self, new TilemapVX(viewport));
                }

                /* Dispose the old bitmap array if we're reinitializing.
                 * See the comment in setPrivateData for more info. */
                SANDBOX_AWAIT_AND_SET(obj, rb_iv_get, self, "bitmap_array");
                if (obj != SANDBOX_NIL) {
                    set_private_data(obj, NULL);
                }

                SANDBOX_AWAIT_AND_SET(obj, wrap_property, self, &get_private_data<TilemapVX>(self)->getBitmapArray(), "bitmap_array", bitmap_array_class);

                SANDBOX_AWAIT_AND_SET(ary, rb_class_new_instance, 0, NULL, sb()->rb_cArray());
                for (i = 0; i < 9; ++i) {
                    SANDBOX_AWAIT(rb_ary_push, ary, SANDBOX_NIL);
                }

                SANDBOX_AWAIT(rb_iv_set, obj, "array", ary);

                /* Circular reference so both objects are always
                 * alive at the same time */
                SANDBOX_AWAIT(rb_iv_set, obj, "tilemap", self);

                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bitmaps(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "bitmap_array");
}

static VALUE update(VALUE self) {
    GFX_LOCK;
    get_private_data<TilemapVX>(self)->update();
    GFX_UNLOCK;
    return SANDBOX_NIL;
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Viewport, Viewport, viewport);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, MapData, map_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, FlashData, flash_data);
SANDBOX_DEF_GFX_PROP_B(TilemapVX, Visible, visible);
SANDBOX_DEF_GFX_PROP_I(TilemapVX, OX, ox);
SANDBOX_DEF_GFX_PROP_I(TilemapVX, OY, oy);

SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, Flags, flags);
SANDBOX_DEF_GFX_PROP_OBJ_REF(TilemapVX, Table, Flags, passages);

void tilemapvx_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(bitmap_array_binding_init);

        tilemapvx_type = sb()->rb_data_type("Tilemap", NULL, dfree<TilemapVX>, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(tilemapvx_class, rb_define_class, "Tilemap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, tilemapvx_class, alloc);
        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<TilemapVX>, tilemapvx_class);

        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "bitmaps", (VALUE (*)(ANYARGS))bitmaps, 0);
        SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_INIT_PROP_BIND(tilemapvx_class, viewport);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, map_data);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, flash_data);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, visible);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, ox);
        SANDBOX_INIT_PROP_BIND(tilemapvx_class, oy);

        if (rgssVer == 3) {
            SANDBOX_INIT_PROP_BIND(tilemapvx_class, flags);
        } else {
            SANDBOX_INIT_PROP_BIND(tilemapvx_class, passages);
        }
    }
}
