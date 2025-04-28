/*
** tilemapvx-binding.h
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

#ifndef MKXPZ_SANDBOX_TILEMAPVX_BINDING_H
#define MKXPZ_SANDBOX_TILEMAPVX_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "tilemapvx.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type tilemapvx_type;
    static VALUE tilemapvx_class;
    static struct mkxp_sandbox::bindings::rb_data_type bitmap_array_type;
    static VALUE bitmap_array_class;

    SANDBOX_COROUTINE(tilemapvx_binding_init,
        SANDBOX_COROUTINE(bitmap_array_binding_init,
            SANDBOX_DEF_ALLOC(bitmap_array_type)

            static VALUE get(VALUE self, VALUE i) {
                SANDBOX_COROUTINE(coro,
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
                )

                return sb()->bind<struct coro>()()(self, i);
            }

            static VALUE set(VALUE self, VALUE i, VALUE obj) {
                SANDBOX_COROUTINE(coro,
                    TilemapVX::BitmapArray *bitmap_array;
                    Bitmap *bitmap;
                    VALUE ary;
                    wasm_size_t index;
                    VALUE value;

                    VALUE operator()(VALUE self, VALUE i, VALUE obj) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            bitmap_array = get_private_data<TilemapVX::BitmapArray>(self);
                            if (bitmap_array == NULL) {
                                return self;
                            }

                            bitmap = get_private_data<Bitmap>(obj);
                            SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);

                            GFX_LOCK;
                            bitmap_array->set(index, bitmap);
                            SANDBOX_AWAIT_AND_SET(ary, rb_iv_get, self, "array");
                            SANDBOX_AWAIT(rb_ary_store, ary, index, obj);
                            GFX_UNLOCK;
                        }

                        return self;
                    }
                )

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
        )

        SANDBOX_DEF_ALLOC(tilemapvx_type)
        SANDBOX_DEF_DFREE(TilemapVX)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                TilemapVX *tilemap;
                VALUE viewport_obj;
                Viewport *viewport;
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;
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
                        tilemap = new TilemapVX(viewport);

                        set_private_data(self, tilemap);

                        /* Dispose the old bitmap array if we're reinitializing.
                         * See the comment in setPrivateData for more info. */
                        SANDBOX_AWAIT_AND_SET(obj, rb_iv_get, self, "bitmap_array");
                        if (obj != SANDBOX_NIL) {
                            set_private_data(obj, NULL);
                        }

                        SANDBOX_AWAIT_AND_SET(obj, wrap_property, self, &tilemap->getBitmapArray(), "bitmap_array", bitmap_array_class);

                        SANDBOX_AWAIT_AND_SET(ary, rb_class_new_instance, 0, NULL, sb()->rb_cArray());
                        for (i = 0; i < 9; ++i) {
                            SANDBOX_AWAIT(rb_ary_push, ary, SANDBOX_NIL);
                        }

                        SANDBOX_AWAIT(rb_iv_set, obj, "array", ary);

                        /* Circular reference so both objects are always
                         * alive at the same time */
                        SANDBOX_AWAIT(rb_iv_set, obj, "tilemap", self);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            TilemapVX *tilemap = get_private_data<TilemapVX>(self);
            if (tilemap != NULL) {
                tilemap->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            TilemapVX *tilemap = get_private_data<TilemapVX>(self);
            return tilemap == NULL || tilemap->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE update(VALUE self) {
            GFX_LOCK;
            get_private_data<TilemapVX>(self)->update();
            GFX_UNLOCK;
            return SANDBOX_NIL;
        }

        static VALUE bitmaps(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "bitmap_array");
        }

        static VALUE get_map_data(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "map_data");
        }

        static VALUE set_map_data(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setMapData(get_private_data<Table>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "map_data", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_flash_data(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "flash_data");
        }

        static VALUE set_flash_data(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setFlashData(get_private_data<Table>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "flash_data", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_flags(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "flags");
        }

        static VALUE set_flags(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setFlags(get_private_data<Table>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "flags", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_visible(VALUE self) {
            return get_private_data<TilemapVX>(self)->getVisible() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE set_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setVisible(value != SANDBOX_FALSE && value != SANDBOX_NIL));
            return value;
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<TilemapVX>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t ox;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<TilemapVX>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t oy;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<TilemapVX>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(bitmap_array_binding_init);

                tilemapvx_type = sb()->rb_data_type("Tilemap", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(tilemapvx_class, rb_define_class, "Tilemap", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, tilemapvx_class, alloc);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "bitmaps", (VALUE (*)(ANYARGS))bitmaps, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "map_data", (VALUE (*)(ANYARGS))get_map_data, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "map_data=", (VALUE (*)(ANYARGS))set_map_data, 1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "flash_data", (VALUE (*)(ANYARGS))get_flash_data, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "flash_data=", (VALUE (*)(ANYARGS))set_flash_data, 1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, rgssVer == 3 ? "flags" : "passages", (VALUE (*)(ANYARGS))get_flags, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, rgssVer == 3 ? "flags=" : "passages=", (VALUE (*)(ANYARGS))set_flags, 1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "visible", (VALUE (*)(ANYARGS))get_visible, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "visible=", (VALUE (*)(ANYARGS))set_visible, 1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "ox", (VALUE (*)(ANYARGS))get_ox, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "ox=", (VALUE (*)(ANYARGS))set_ox, 1);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "oy", (VALUE (*)(ANYARGS))get_oy, 0);
                SANDBOX_AWAIT(rb_define_method, tilemapvx_class, "oy=", (VALUE (*)(ANYARGS))set_oy, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_TILEMAPVX_BINDING_H
