/*
** tilemap-binding.cpp
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

#include "tilemap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "tilemap.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::tilemap_class;
VALUE mkxp_sandbox::tilemap_autotiles_class;
static struct bindings::rb_data_type tilemap_type;
static struct bindings::rb_data_type tilemap_autotiles_type;

struct tilemap_autotiles_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC(tilemap_autotiles_type)

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
                    if (get_private_data<Tilemap::Autotiles>(self) == nullptr) {
                        return self;
                    }

                    SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);

                    GFX_LOCK;
                    get_private_data<Tilemap::Autotiles>(self)->set(index, get_private_data<Bitmap>(obj));
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
            tilemap_autotiles_type = sb()->rb_data_type("TilemapAutotiles", NULL, NULL, NULL, NULL, 0, 0, 0);
            SANDBOX_AWAIT_AND_SET(tilemap_autotiles_class, rb_define_class, "TilemapAutotiles", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, tilemap_autotiles_class, alloc);

            SANDBOX_AWAIT(rb_define_method, tilemap_autotiles_class, "[]", (VALUE (*)(ANYARGS))get, 1);
            SANDBOX_AWAIT(rb_define_method, tilemap_autotiles_class, "[]=", (VALUE (*)(ANYARGS))set, 2);
        }
    }
};

SANDBOX_DEF_ALLOC(tilemap_type)

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
                    Tilemap *tilemap = new Tilemap(viewport);

                    set_private_data(self, tilemap);

                    tilemap->initDynAttribs();
                }

                /* Dispose the old autotiles if we're reinitializing.
                 * See the comment in setPrivateData for more info. */
                SANDBOX_AWAIT_AND_SET(obj, rb_iv_get, self, "autotiles");
                if (obj != SANDBOX_NIL) {
                    set_private_data(obj, NULL);
                }

                SANDBOX_AWAIT_AND_SET(obj, wrap_property, self, &get_private_data<Tilemap>(self)->getAutotiles(), "autotiles", tilemap_autotiles_class);

                SANDBOX_AWAIT(wrap_property, self, &get_private_data<Tilemap>(self)->getColor(), "color", color_class);
                SANDBOX_AWAIT(wrap_property, self, &get_private_data<Tilemap>(self)->getTone(), "tone", tone_class);

                SANDBOX_AWAIT_AND_SET(ary, rb_class_new_instance, 0, NULL, sb()->rb_cArray());
                for (i = 0; i < 7; ++i) {
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

static VALUE autotiles(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "autotiles");
}

static VALUE update(VALUE self) {
    GFX_LOCK;
    get_private_data<Tilemap>(self)->update();
    GFX_UNLOCK;
    return SANDBOX_NIL;
}

static VALUE viewport(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "viewport");
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Bitmap, Tileset, tileset);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, MapData, map_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, FlashData, flash_data);
SANDBOX_DEF_GFX_PROP_OBJ_REF(Tilemap, Table, Priorities, priorities);
SANDBOX_DEF_GFX_PROP_B(Tilemap, Visible, visible);
SANDBOX_DEF_GFX_PROP_I(Tilemap, OX, ox);
SANDBOX_DEF_GFX_PROP_I(Tilemap, OY, oy);

SANDBOX_DEF_GFX_PROP_I(Tilemap, Opacity, opacity);
SANDBOX_DEF_GFX_PROP_I(Tilemap, BlendType, blend_type);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Tilemap, Color, Color, color);
SANDBOX_DEF_GFX_PROP_OBJ_VAL(Tilemap, Tone, Tone, tone);

void tilemap_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(tilemap_autotiles_binding_init);

        tilemap_type = sb()->rb_data_type("Tilemap", NULL, dfree<Tilemap>, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(tilemap_class, rb_define_class, "Tilemap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, tilemap_class, alloc);
        SANDBOX_AWAIT(rb_define_method, tilemap_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(disposable_binding_init<Tilemap>, tilemap_class);

        SANDBOX_AWAIT(rb_define_method, tilemap_class, "autotiles", (VALUE (*)(ANYARGS))autotiles, 0);
        SANDBOX_AWAIT(rb_define_method, tilemap_class, "update", (VALUE (*)(ANYARGS))update, 0);

        SANDBOX_AWAIT(rb_define_method, tilemap_class, "viewport", (VALUE (*)(ANYARGS))viewport, 0);

        SANDBOX_INIT_PROP_BIND(tilemap_class, tileset);
        SANDBOX_INIT_PROP_BIND(tilemap_class, map_data);
        SANDBOX_INIT_PROP_BIND(tilemap_class, flash_data);
        SANDBOX_INIT_PROP_BIND(tilemap_class, priorities);
        SANDBOX_INIT_PROP_BIND(tilemap_class, visible);
        SANDBOX_INIT_PROP_BIND(tilemap_class, ox);
        SANDBOX_INIT_PROP_BIND(tilemap_class, oy);

        SANDBOX_INIT_PROP_BIND(tilemap_class, opacity);
        SANDBOX_INIT_PROP_BIND(tilemap_class, blend_type);
        SANDBOX_INIT_PROP_BIND(tilemap_class, color);
        SANDBOX_INIT_PROP_BIND(tilemap_class, tone);
    }
}
