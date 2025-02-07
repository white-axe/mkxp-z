/*
** bitmap-binding.h
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

#ifndef MKXPZ_SANDBOX_BITMAP_BINDING_H
#define MKXPZ_SANDBOX_BITMAP_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "bitmap.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type bitmap_type;

    SANDBOX_COROUTINE(bitmap_binding_init,
        SANDBOX_DEF_ALLOC(bitmap_type)
        SANDBOX_DEF_DFREE(Bitmap)

        SANDBOX_COROUTINE(init_props,
            VALUE id;
            VALUE klass;
            VALUE font;

            void operator()(Bitmap *bitmap, VALUE obj) {
                BOOST_ASIO_CORO_REENTER (this) {
                    SANDBOX_AWAIT_AND_SET(id, rb_intern, "Font");
                    SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                    SANDBOX_AWAIT_AND_SET(font, rb_class_new_instance, 0, NULL, klass);
                    SANDBOX_AWAIT(rb_iv_set, obj, "font", font);
                }
            }
        )

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                wasm_ptr_t filename;
                wasm_size_t width;
                wasm_size_t height;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (argc == 1) {
                            SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                            GFX_GUARD_EXC(bitmap = new Bitmap((const char *)(**sb() + filename));)
                        } else {
                            SANDBOX_AWAIT_AND_SET(width, rb_num2ulong, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(height, rb_num2ulong, ((VALUE *)(**sb() + argv))[1]);
                            GFX_GUARD_EXC(bitmap = new Bitmap(width, height);)
                        }

                        set_private_data(self, bitmap);
                        SANDBOX_AWAIT(init_props, bitmap, self);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE dispose(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            if (bitmap != NULL) {
                bitmap->dispose();
            }
            return SANDBOX_NIL;
        }

        static VALUE disposed(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            return bitmap == NULL || bitmap->isDisposed() ? SANDBOX_TRUE : SANDBOX_FALSE;
        }

        static VALUE clear(VALUE self) {
            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->clear());
            return SANDBOX_NIL;
        }

        static VALUE fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                int x;
                int y;
                int width;
                int height;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        if (argc == 2) {
                            GFX_GUARD_EXC(bitmap->fillRect(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), get_private_data<Color>(((VALUE *)(**sb() + argv))[1])->norm);)
                        } else {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(width, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(height, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            GFX_GUARD_EXC(bitmap->fillRect(x, y, width, height, get_private_data<Color>(((VALUE *)(**sb() + argv))[4])->norm);)
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE draw_text(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                wasm_ptr_t str;
                int align;
                int x;
                int y;
                int width;
                int height;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        // TODO: handle RGSS version >= 2

                        if (argc == 2 || argc == 3) {
                            SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, (VALUE *)(**sb() + argv) + 1);
                            if (argc == 2) {
                                GFX_GUARD_EXC(bitmap->drawText(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), (const char *)(**sb() + str));)
                            } else {
                                SANDBOX_AWAIT_AND_SET(align, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                GFX_GUARD_EXC(bitmap->drawText(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), (const char *)(**sb() + str), align);)
                            }
                        } else {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(width, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(height, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, (VALUE *)(**sb() + argv) + 4);
                            if (argc < 6) {
                                GFX_GUARD_EXC(bitmap->drawText(x, y, width, height, (const char *)(**sb() + str));)
                            } else {
                                SANDBOX_AWAIT_AND_SET(align, rb_num2int, ((VALUE *)(**sb() + argv))[5]);
                                GFX_GUARD_EXC(bitmap->drawText(x, y, width, height, (const char *)(**sb() + str), align);)
                            }
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE get_font(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "font");
        }

        static VALUE width(VALUE self) {
            return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->width());
        }

        static VALUE height(VALUE self) {
            return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->height());
        }

        static VALUE rect(VALUE self) {
            SANDBOX_COROUTINE(coro,
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->rect()));
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE text_size(VALUE self, VALUE text) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self, VALUE text) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &text);
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_class_new_instance, 0, NULL, klass);
                        set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->textSize((const char *)(**sb() + str))));
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self, text);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                bitmap_type = sb()->rb_data_type("Bitmap", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Bitmap", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "clear", (VALUE (*)(ANYARGS))clear, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "fill_rect", (VALUE (*)(ANYARGS))fill_rect, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "draw_text", (VALUE (*)(ANYARGS))draw_text, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "font", (VALUE (*)(ANYARGS))get_font, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "width", (VALUE (*)(ANYARGS))width, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "height", (VALUE (*)(ANYARGS))height, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "rect", (VALUE (*)(ANYARGS))rect, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "text_size", (VALUE (*)(ANYARGS))text_size, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_BITMAP_BINDING_H
