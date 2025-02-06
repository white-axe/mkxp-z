/*
** sprite-binding.h
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

#ifndef MKXPZ_SANDBOX_SPRITE_BINDING_H
#define MKXPZ_SANDBOX_SPRITE_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "sprite.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type sprite_type;

    SANDBOX_COROUTINE(sprite_binding_init,
        SANDBOX_DEF_ALLOC(sprite_type)
        SANDBOX_DEF_DFREE(Sprite)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Sprite *sprite;
                VALUE viewport_obj;
                Viewport *viewport;
                ID id;
                VALUE klass;
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

                        sprite = new Sprite(viewport);
                        SANDBOX_AWAIT(rb_iv_set, self, "viewport", viewport_obj);

                        set_private_data(self, sprite);
                        sprite->initDynAttribs();

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &sprite->getSrcRect());
                        SANDBOX_AWAIT(rb_iv_set, self, "src_rect", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &sprite->getColor());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Tone");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, &sprite->getTone());
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", obj);

                        GFX_UNLOCK
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Sprite *sprite = get_private_data<Sprite>(self);
            if (sprite != NULL) {
                sprite->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Sprite *sprite = get_private_data<Sprite>(self);
            return sprite == NULL || sprite->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE get_bitmap(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "bitmap");
        }

        static VALUE set_bitmap(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Sprite>(self)->setBitmap(get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "bitmap", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_src_rect(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "src_rect");
        }

        static VALUE set_src_rect(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Sprite>(self)->setSrcRect(*get_private_data<Rect>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "src_rect", value);
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
                        GFX_GUARD_EXC(get_private_data<Sprite>(self)->setColor(*get_private_data<Color>(value)));
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
                        GFX_GUARD_EXC(get_private_data<Sprite>(self)->setTone(*get_private_data<Tone>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "tone", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                sprite_type = sb()->rb_data_type("Sprite", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Sprite", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "bitmap", (VALUE (*)(ANYARGS))get_bitmap, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "bitmap=", (VALUE (*)(ANYARGS))set_bitmap, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "src_rect", (VALUE (*)(ANYARGS))get_src_rect, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "src_rect=", (VALUE (*)(ANYARGS))set_src_rect, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "color", (VALUE (*)(ANYARGS))get_color, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))set_color, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "tone", (VALUE (*)(ANYARGS))get_tone, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "tone=", (VALUE (*)(ANYARGS))set_tone, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_SPRITE_BINDING_H
