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

    SANDBOX_COROUTINE(bitmap_init_props,
        ID id;
        VALUE klass;
        VALUE obj;
        VALUE font;
        VALUE hires_font;

        void operator()(Bitmap *bitmap, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, "Font");
                SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                SANDBOX_AWAIT_AND_SET(font, rb_class_new_instance, 0, NULL, klass);
                SANDBOX_AWAIT(rb_iv_set, self, "font", font);

                // Leave property as default nil if hasHires() is false.
                if (bitmap->hasHires()) {
                    bitmap->assumeRubyGC();
                    SANDBOX_AWAIT_AND_SET(id, rb_intern, "Bitmap");
                    SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                    SANDBOX_AWAIT_AND_SET(self, rb_obj_alloc, klass);
                    set_private_data(obj, bitmap->getHires());
                    SANDBOX_AWAIT(rb_iv_set, self, "hires", obj);

                    SANDBOX_AWAIT_AND_SET(id, rb_intern, "Font");
                    SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                    SANDBOX_AWAIT_AND_SET(hires_font, rb_class_new_instance, 0, NULL, klass);
                    SANDBOX_AWAIT(rb_iv_set, obj, "font", hires_font);
                    bitmap->getHires()->setInitFont(get_private_data<Font>(hires_font));
                }

                bitmap->setInitFont(get_private_data<Font>(font));
            }
        }
    )

    SANDBOX_COROUTINE(bitmap_binding_init,
        SANDBOX_DEF_ALLOC(bitmap_type)
        SANDBOX_DEF_DFREE(Bitmap)

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
                        SANDBOX_AWAIT(bitmap_init_props, bitmap, self);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE initialize_copy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                Bitmap *orig;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (self == value) {
                            return self;
                        }

                        SANDBOX_AWAIT(rb_obj_init_copy, self, value);

                        orig = get_private_data<Bitmap>(value);
                        GFX_GUARD_EXC(bitmap = new Bitmap(*orig););

                        SANDBOX_AWAIT(bitmap_init_props, bitmap, self);
                        bitmap->setFont(orig->getFont());
                        set_private_data(self, bitmap);
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
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

        static VALUE width(VALUE self) {
            return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->width());
        }

        static VALUE height(VALUE self) {
            return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->height());
        }

        static VALUE get_hires(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "hires");
        }

        static VALUE set_hires(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(get_private_data<Bitmap>(self)->setHires(get_private_data<Bitmap>(value)));
                        SANDBOX_AWAIT(rb_iv_set, self, "hires", value);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
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
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->rect()));
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                int x;
                int y;
                VALUE srcObj;
                VALUE srcRectObj;
                int opacity;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                        srcObj = ((VALUE *)(**sb() + argv))[2];
                        srcRectObj = ((VALUE *)(**sb() + argv))[3];
                        if (argc > 4) {
                            SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, ((VALUE *)(**sb() + argv))[4]);
                        }

                        Bitmap *src = get_private_data<Bitmap>(srcObj);
                        if (src != NULL) {
                            Rect *srcRect = get_private_data<Rect>(srcRectObj);
                            if (argc > 4) {
                                GFX_GUARD_EXC(get_private_data<Bitmap>(self)->blt(x, y, *src, srcRect->toIntRect(), opacity););
                            } else {
                                GFX_GUARD_EXC(get_private_data<Bitmap>(self)->blt(x, y, *src, srcRect->toIntRect()););
                            }
                        }
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE stretch_blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE destRectObj;
                VALUE srcObj;
                VALUE srcRectObj;
                int opacity;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        destRectObj = ((VALUE *)(**sb() + argv))[0];
                        srcObj = ((VALUE *)(**sb() + argv))[1];
                        srcRectObj = ((VALUE *)(**sb() + argv))[2];
                        if (argc > 3) {
                            SANDBOX_AWAIT_AND_SET(opacity, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                        }

                        Bitmap *src = get_private_data<Bitmap>(srcObj);
                        if (src != NULL) {
                            Rect *destRect = get_private_data<Rect>(destRectObj);
                            Rect *srcRect = get_private_data<Rect>(srcRectObj);
                            if (argc > 4) {
                                GFX_GUARD_EXC(get_private_data<Bitmap>(self)->stretchBlt(destRect->toIntRect(), *src, srcRect->toIntRect(), opacity););
                            } else {
                                GFX_GUARD_EXC(get_private_data<Bitmap>(self)->stretchBlt(destRect->toIntRect(), *src, srcRect->toIntRect()););
                            }
                        }
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
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

        static VALUE clear(VALUE self) {
            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->clear());
            return SANDBOX_NIL;
        }

        static VALUE get_pixel(VALUE self, VALUE xval, VALUE yval) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                int x;
                int y;
                Color *color;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self, VALUE xval, VALUE yval) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, xval);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, yval);

                        {
                            Color value;
                            if (bitmap->surface() != NULL || bitmap->megaSurface() != NULL) {
                                value = bitmap->getPixel(x, y);
                            } else {
                                GFX_GUARD_EXC(value = bitmap->getPixel(x, y););
                            }
                            color = new Color(value);
                        }

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, color);
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self, xval, yval);
        }

        static VALUE set_pixel(VALUE self, VALUE xval, VALUE yval, VALUE colorObj) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                int x;
                int y;

                VALUE operator()(VALUE self, VALUE xval, VALUE yval, VALUE colorObj) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, xval);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, yval);

                        Color *color = get_private_data<Color>(colorObj);
                        GFX_GUARD_EXC(bitmap->setPixel(x, y, *color););
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, xval, yval, colorObj);
        }

        static VALUE hue_change(VALUE self, VALUE hueval) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                int hue;

                VALUE operator()(VALUE self, VALUE hueval) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        SANDBOX_AWAIT_AND_SET(hue, rb_num2int, hueval);

                        GFX_GUARD_EXC(bitmap->hueChange(hue););
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, hueval);
        }

        static VALUE draw_text(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                wasm_ptr_t str;
                VALUE obj;
                int align;
                int x;
                int y;
                int width;
                int height;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        bitmap = get_private_data<Bitmap>(self);

                        if (argc == 2 || argc == 3) {
                            if (rgssVer >= 2) {
                                SANDBOX_AWAIT_AND_SET(obj, rb_obj_as_string, ((VALUE *)(**sb() + argv))[1]);
                                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                            } else {
                                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, (VALUE *)(**sb() + argv) + 1);
                            }
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
                            if (rgssVer >= 2) {
                                SANDBOX_AWAIT_AND_SET(obj, rb_obj_as_string, ((VALUE *)(**sb() + argv))[4]);
                                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                            } else {
                                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, (VALUE *)(**sb() + argv) + 4);
                            }
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

        static VALUE text_size(VALUE self, VALUE text) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self, VALUE text) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (rgssVer >= 2) {
                            SANDBOX_AWAIT_AND_SET(obj, rb_obj_as_string, text);
                            SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                        } else {
                            SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &text);
                        }
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Rect");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->textSize((const char *)(**sb() + str))));
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self, text);
        }

        static VALUE get_raw_data(VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE value;
                wasm_ptr_t str;

                VALUE operator()(VALUE self) {
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    int size = bitmap->width() * bitmap->height() * 4;

                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(value, rb_str_new_cstr, "");
                        SANDBOX_AWAIT(rb_str_resize, value, size);
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_ptr, &value);
                        GFX_GUARD_EXC(bitmap->getRaw(**sb() + str, size););
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE set_raw_data(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;
                wasm_size_t size;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_ptr, &value);
                        SANDBOX_AWAIT_AND_SET(size, get_bytesize, value);
                        Bitmap *bitmap = get_private_data<Bitmap>(self);
                        GFX_GUARD_EXC(bitmap->replaceRaw(**sb() + str, size););
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE to_file(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &value);
                        Bitmap *bitmap = get_private_data<Bitmap>(self);
                        GFX_GUARD_EXC(bitmap->saveToFile((const char *)(**sb() + str)););
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE snap_to_bitmap(VALUE self, VALUE position) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                VALUE obj;
                int32_t pos;

                VALUE operator()(VALUE self, VALUE position) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (position == SANDBOX_NIL) {
                            pos = -1;
                        } else {
                            SANDBOX_AWAIT_AND_SET(pos, rb_num2int, position);
                        }

                        GFX_GUARD_EXC(bitmap = new Bitmap(*get_private_data<Bitmap>(self), pos););

                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_class, self);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, obj);

                        SANDBOX_AWAIT(bitmap_init_props, bitmap, obj);
                        set_private_data(obj, bitmap);
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self, position);
        }

        static VALUE gradient_fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;
                bool vertical;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (argc == 3 || argc == 4) {
                            if (argc == 4) {
                                vertical = SANDBOX_VALUE_TO_BOOL(((VALUE *)(**sb() + argv))[3]);
                            } else {
                                vertical = false;
                            }
                            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->gradientFillRect(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), get_private_data<Color>(((VALUE *)(**sb() + argv))[1])->norm, get_private_data<Color>(((VALUE *)(**sb() + argv))[2])->norm, vertical););
                        } else {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            if (argc >= 7) {
                                vertical = SANDBOX_VALUE_TO_BOOL(((VALUE *)(**sb() + argv))[6]);
                            } else {
                                vertical = false;
                            }
                            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->gradientFillRect(x, y, w, h, get_private_data<Color>(((VALUE *)(**sb() + argv))[4])->norm, get_private_data<Color>(((VALUE *)(**sb() + argv))[5])->norm, vertical););
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE clear_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                int32_t x;
                int32_t y;
                int32_t w;
                int32_t h;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (argc == 1) {
                            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->clearRect(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect()););
                        } else {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->clearRect(x, y, w, h););
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE blur(VALUE self) {
            get_private_data<Bitmap>(self)->blur();
            return SANDBOX_NIL;
        }

        static VALUE radial_blur(VALUE self, VALUE angle, VALUE divisions) {
            SANDBOX_COROUTINE(coro,
                int32_t a;
                int32_t d;

                VALUE operator()(VALUE self, VALUE angle, VALUE divisions) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(a, rb_num2int, angle);
                        SANDBOX_AWAIT_AND_SET(d, rb_num2int, divisions);
                        get_private_data<Bitmap>(self)->radialBlur(a, d);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, angle, divisions);
        }

        static VALUE mega(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            bool ret;
            GFX_GUARD_EXC(ret = bitmap->isMega(););
            return SANDBOX_BOOL_TO_VALUE(ret);
        }

        static VALUE max_size(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(Bitmap::maxSize());
        }

        static VALUE get_animated(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            bool ret;
            GFX_GUARD_EXC(ret = bitmap->isAnimated(););
            return SANDBOX_BOOL_TO_VALUE(ret);
        }

        static VALUE get_playing(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            bool ret;
            GFX_GUARD_EXC(ret = bitmap->isPlaying(););
            return SANDBOX_BOOL_TO_VALUE(ret);
        }

        static VALUE set_playing(VALUE self, VALUE value) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            GFX_GUARD_EXC(SANDBOX_VALUE_TO_BOOL(value) ? bitmap->play() : bitmap->stop(););
            return SANDBOX_NIL;
        }

        static VALUE play(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            GFX_GUARD_EXC(bitmap->play(););
            return SANDBOX_NIL;
        }

        static VALUE stop(VALUE self) {
            Bitmap *bitmap = get_private_data<Bitmap>(self);
            GFX_GUARD_EXC(bitmap->stop(););
            return SANDBOX_NIL;
        }

        static VALUE goto_and_play(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t frame;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(frame, rb_num2int, value);
                        Bitmap *bitmap = get_private_data<Bitmap>(self);
                        GFX_GUARD_EXC(bitmap->gotoAndPlay(frame););
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE goto_and_stop(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t frame;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(frame, rb_num2int, value);
                        Bitmap *bitmap = get_private_data<Bitmap>(self);
                        GFX_GUARD_EXC(bitmap->gotoAndStop(frame););
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_font(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "font");
        }

        static VALUE set_font(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE f;
                VALUE prop;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        Font *font;
                        font = get_private_data<Font>(value);
                        if (font != NULL) {
                            GFX_GUARD_EXC(get_private_data<Bitmap>(self)->setFont(*font);)

                            SANDBOX_AWAIT_AND_SET(f, rb_iv_get, self, "font");
                            SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "name");
                            SANDBOX_AWAIT(rb_iv_set, f, "name", prop);
                            SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "size");
                            SANDBOX_AWAIT(rb_iv_set, f, "size", prop);
                            SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "bold");
                            SANDBOX_AWAIT(rb_iv_set, f, "bold", prop);
                            SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "italic");
                            SANDBOX_AWAIT(rb_iv_set, f, "italic", prop);

                            if (rgssVer >= 2) {
                                SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "shadow");
                                SANDBOX_AWAIT(rb_iv_set, f, "shadow", prop);
                            }

                            if (rgssVer >= 3) {
                                SANDBOX_AWAIT_AND_SET(prop, rb_iv_get, value, "outline");
                                SANDBOX_AWAIT(rb_iv_set, f, "outline", prop);
                            }
                        }
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                bitmap_type = sb()->rb_data_type("Bitmap", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Bitmap", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "dispose", (VALUE (*)(ANYARGS))dispose, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "disposed?", (VALUE (*)(ANYARGS))disposed, 0);

                SANDBOX_AWAIT(rb_define_method, klass, "width", (VALUE (*)(ANYARGS))width, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "height", (VALUE (*)(ANYARGS))height, 0);

                SANDBOX_AWAIT(rb_define_method, klass, "hires", (VALUE (*)(ANYARGS))get_hires, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "hires=", (VALUE (*)(ANYARGS))set_hires, 1);

                SANDBOX_AWAIT(rb_define_method, klass, "rect", (VALUE (*)(ANYARGS))rect, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "blt", (VALUE (*)(ANYARGS))blt, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "stretch_blt", (VALUE (*)(ANYARGS))stretch_blt, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "fill_rect", (VALUE (*)(ANYARGS))fill_rect, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "clear", (VALUE (*)(ANYARGS))clear, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "get_pixel", (VALUE (*)(ANYARGS))get_pixel, 2);
                SANDBOX_AWAIT(rb_define_method, klass, "set_pixel", (VALUE (*)(ANYARGS))set_pixel, 3);
                SANDBOX_AWAIT(rb_define_method, klass, "hue_change", (VALUE (*)(ANYARGS))hue_change, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "draw_text", (VALUE (*)(ANYARGS))draw_text, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "text_size", (VALUE (*)(ANYARGS))text_size, 1);

                SANDBOX_AWAIT(rb_define_method, klass, "raw_data", (VALUE (*)(ANYARGS))get_raw_data, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "raw_data=", (VALUE (*)(ANYARGS))set_raw_data, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "to_file", (VALUE (*)(ANYARGS))to_file, 1);

                SANDBOX_AWAIT(rb_define_method, klass, "gradient_fill_rect", (VALUE (*)(ANYARGS))gradient_fill_rect, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "clear_rect", (VALUE (*)(ANYARGS))clear_rect, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "blur", (VALUE (*)(ANYARGS))blur, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "radial_blur", (VALUE (*)(ANYARGS))radial_blur, 2);

                SANDBOX_AWAIT(rb_define_method, klass, "mega?", (VALUE (*)(ANYARGS))mega, 0);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "max_size", (VALUE (*)(ANYARGS))max_size, 0);

                SANDBOX_AWAIT(rb_define_method, klass, "animated?", (VALUE (*)(ANYARGS))get_animated, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "playing", (VALUE (*)(ANYARGS))get_playing, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "playing=", (VALUE (*)(ANYARGS))set_playing, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "play", (VALUE (*)(ANYARGS))play, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "stop", (VALUE (*)(ANYARGS))stop, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "goto_and_play", (VALUE (*)(ANYARGS))goto_and_play, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "goto_and_stop", (VALUE (*)(ANYARGS))goto_and_stop, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, 1);

                SANDBOX_AWAIT(rb_define_method, klass, "font", (VALUE (*)(ANYARGS))get_font, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "font=", (VALUE (*)(ANYARGS))set_font, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_BITMAP_BINDING_H
