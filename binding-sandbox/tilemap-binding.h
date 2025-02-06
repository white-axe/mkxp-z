/*
** tilemap-binding.h
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

#ifndef MKXPZ_SANDBOX_TILEMAP_BINDING_H
#define MKXPZ_SANDBOX_TILEMAP_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "tilemap.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type tilemap_type;
    static struct mkxp_sandbox::bindings::rb_data_type tilemap_autotiles_type;

    SANDBOX_COROUTINE(tilemap_binding_init,
        SANDBOX_COROUTINE(tilemap_autotiles_binding_init,
            SANDBOX_DEF_ALLOC(tilemap_autotiles_type)

            static VALUE get(VALUE self, VALUE i) {
                SANDBOX_COROUTINE(coro,
                    VALUE ary;
                    wasm_size_t index;
                    VALUE value;

                    VALUE operator()(VALUE self, VALUE i) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(ary, rb_iv_get, self, "array");
                            SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);
                            SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, ary, i);
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, i);
            }

            static VALUE set(VALUE self, VALUE i, VALUE obj) {
                SANDBOX_COROUTINE(coro,
                    Tilemap::Autotiles *autotiles;
                    Bitmap *bitmap;
                    VALUE ary;
                    wasm_size_t index;
                    VALUE value;

                    VALUE operator()(VALUE self, VALUE i, VALUE obj) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            autotiles = get_private_data<Tilemap::Autotiles>(self);
                            if (autotiles == NULL) {
                                return self;
                            }

                            bitmap = get_private_data<Bitmap>(obj);
                            SANDBOX_AWAIT_AND_SET(index, rb_num2ulong, i);

                            GFX_LOCK;
                            autotiles->set(index, bitmap);
                            SANDBOX_AWAIT_AND_SET(ary, rb_iv_get, self, "array");
                            SANDBOX_AWAIT(rb_ary_store, ary, i, obj);
                            GFX_UNLOCK;
                        }

                        return self;
                    }
                )

                return sb()->bind<struct coro>()()(self, i, obj);
            }

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    tilemap_autotiles_type = sb()->rb_data_type("TilemapAutotiles", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "TilemapAutotiles", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_method, klass, "[]", (VALUE (*)(ANYARGS))get, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "[]=", (VALUE (*)(ANYARGS))set, 2);
                }
            }
        )

        SANDBOX_DEF_ALLOC(tilemap_type)
        SANDBOX_DEF_DFREE(Tilemap)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Tilemap *tilemap;
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
                        tilemap = new Tilemap(viewport);

                        set_private_data(self, tilemap);

                        tilemap->initDynAttribs();

                        /* Dispose the old autotiles if we're reinitializing.
                         * See the comment in setPrivateData for more info. */
                        SANDBOX_AWAIT_AND_SET(obj, rb_iv_get, self, "autotiles");
                        if (obj != SANDBOX_NIL) {
                            set_private_data(obj, NULL);
                        }

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "TilemapAutotiles");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &tilemap->getAutotiles());
                        SANDBOX_AWAIT(rb_iv_set, self, "autotiles", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &tilemap->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Tone");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &tilemap->getTone());
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", obj);

                        SANDBOX_AWAIT_AND_SET(obj, rb_iv_get, self, "autotiles");

                        SANDBOX_AWAIT_AND_SET(ary, rb_class_new_instance, 0, NULL, sb()->rb_cArray());
                        for (i = 0; i < 7; ++i) {
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
            Tilemap *tilemap = get_private_data<Tilemap>(self);
            if (tilemap != NULL) {
                tilemap->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Tilemap *tilemap = get_private_data<Tilemap>(self);
            return tilemap == NULL || tilemap->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE autotiles(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "autotiles");
        }

        static VALUE update(VALUE self, VALUE value) {
            GFX_LOCK;
            get_private_data<Tilemap>(self)->update();
            GFX_UNLOCK;
            return SANDBOX_NIL;
        }

        static VALUE get_tileset(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "tileset");
        }

        static VALUE set_tileset(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setTileset(get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "tileset", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_map_data(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "map_data");
        }

        static VALUE set_map_data(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setMapData(get_private_data<Table>(value)));
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
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setMapData(get_private_data<Table>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "flash_data", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_priorities(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "priorities");
        }

        static VALUE set_priorities(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setMapData(get_private_data<Table>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "priorities", value);
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
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setColor(*get_private_data<Color>(value)));
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
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setTone(*get_private_data<Tone>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_visible(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tilemap>(self)->getOX());
        }

        static VALUE set_visible(VALUE self, VALUE value) {
            GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setVisible(value != SANDBOX_FALSE && value != SANDBOX_NIL));
            return value;
        }

        static VALUE get_ox(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tilemap>(self)->getOX());
        }

        static VALUE set_ox(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t ox;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ox, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setOX(ox));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_oy(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tilemap>(self)->getOY());
        }

        static VALUE set_oy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t oy;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(oy, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setOY(oy));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_opacity(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tilemap>(self)->getOpacity());
        }

        static VALUE set_opacity(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t opacity;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setOpacity(opacity));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_blend_type(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tilemap>(self)->getBlendType());
        }

        static VALUE set_blend_type(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    int32_t blend_type;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(blend_type, rb_num2int, value);
                        GFX_GUARD_EXC(get_private_data<Tilemap>(self)->setBlendType(blend_type));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(tilemap_autotiles_binding_init);

                tilemap_type = sb()->rb_data_type("Tilemap", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Tilemap", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "autotiles", (VALUE (*)(ANYARGS))autotiles, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "tileset", (VALUE (*)(ANYARGS))get_tileset, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "tileset=", (VALUE (*)(ANYARGS))set_tileset, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "map_data", (VALUE (*)(ANYARGS))get_map_data, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "map_data=", (VALUE (*)(ANYARGS))set_map_data, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "flash_data", (VALUE (*)(ANYARGS))get_flash_data, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "flash_data=", (VALUE (*)(ANYARGS))set_flash_data, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "priorities", (VALUE (*)(ANYARGS))get_priorities, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "priorities=", (VALUE (*)(ANYARGS))set_priorities, 1);
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
                SANDBOX_AWAIT(rb_define_method, klass, "opacity", (VALUE (*)(ANYARGS))get_opacity, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "opacity=", (VALUE (*)(ANYARGS))set_opacity, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "blend_type", (VALUE (*)(ANYARGS))get_blend_type, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "blend_type=", (VALUE (*)(ANYARGS))set_blend_type, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_TILEMAP_BINDING_H
