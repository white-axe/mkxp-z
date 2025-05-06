/*
** bitmap-binding.cpp
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

#include "bitmap-binding.h"
#include "disposable-binding.h"
#include "etc-binding.h"
#include "font-binding.h"
#include "bitmap.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::bitmap_class;
static struct bindings::rb_data_type bitmap_type;

SANDBOX_DEF_ALLOC(bitmap_type);

void bitmap_init_props::operator()(VALUE self) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(font, rb_class_new_instance, 0, NULL, font_class);
        SANDBOX_AWAIT(rb_iv_set, self, "font", font);

        // Leave property as default nil if hasHires() is false.
        if (get_private_data<Bitmap>(self)->hasHires()) {
            get_private_data<Bitmap>(self)->assumeRubyGC();
            SANDBOX_AWAIT_AND_SET(hires, wrap_property, self, get_private_data<Bitmap>(self)->getHires(), "hires", bitmap_class);

            SANDBOX_AWAIT_AND_SET(hires_font, rb_class_new_instance, 0, NULL, font_class);
            SANDBOX_AWAIT(rb_iv_set, hires, "font", hires_font);
            get_private_data<Bitmap>(self)->getHires()->setInitFont(get_private_data<Font>(hires_font));
        }

        get_private_data<Bitmap>(self)->setInitFont(get_private_data<Font>(font));
    }
}

static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t filename;
        wasm_size_t width;
        wasm_size_t height;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 1) {
                    SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                    SANDBOX_AWAIT(update_cwd);
                } else {
                    SANDBOX_AWAIT_AND_SET(width, rb_num2ulong, ((VALUE *)(**sb() + argv))[0]);
                    SANDBOX_AWAIT_AND_SET(height, rb_num2ulong, ((VALUE *)(**sb() + argv))[1]);
                }

                {
                    Bitmap *bitmap;
                    if (argc == 1) {
                        GFX_GUARD_EXC(bitmap = new Bitmap((const char *)(**sb() + filename));)
                    } else {
                        GFX_GUARD_EXC(bitmap = new Bitmap(width, height);)
                    }
                    set_private_data(self, bitmap);
                }

                SANDBOX_AWAIT(bitmap_init_props, self);
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE initialize_copy(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (self == value) {
                    return self;
                }

                SANDBOX_AWAIT(rb_obj_init_copy, self, value);

                {
                    Bitmap *bitmap;
                    GFX_GUARD_EXC(bitmap = new Bitmap(*get_private_data<Bitmap>(value)););
                    set_private_data(self, bitmap);
                }

                SANDBOX_AWAIT(bitmap_init_props, self);
                get_private_data<Bitmap>(self)->setFont(get_private_data<Bitmap>(value)->getFont());
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE width(VALUE self) {
    return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->width());
}

static VALUE height(VALUE self) {
    return sb()->bind<struct rb_ull2inum>()()(get_private_data<Bitmap>(self)->height());
}

SANDBOX_DEF_GFX_PROP_OBJ_REF(Bitmap, Bitmap, Hires, hires);

static VALUE rect(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE obj;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, rect_class);
                set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->rect()));
            }

            return obj;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE stretch_blt(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        int x;
        int y;
        int width;
        int height;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 2) {
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    GFX_GUARD_EXC(bitmap->fillRect(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), get_private_data<Color>(((VALUE *)(**sb() + argv))[1])->norm);)
                } else {
                    SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                    SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                    SANDBOX_AWAIT_AND_SET(width, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                    SANDBOX_AWAIT_AND_SET(height, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    GFX_GUARD_EXC(bitmap->fillRect(x, y, width, height, get_private_data<Color>(((VALUE *)(**sb() + argv))[4])->norm);)
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE clear(VALUE self) {
    GFX_GUARD_EXC(get_private_data<Bitmap>(self)->clear());
    return SANDBOX_NIL;
}

static VALUE get_pixel(VALUE self, VALUE xval, VALUE yval) {
    struct coro : boost::asio::coroutine {
        int x;
        int y;
        VALUE obj;

        VALUE operator()(VALUE self, VALUE xval, VALUE yval) {
            BOOST_ASIO_CORO_REENTER (this) {

                SANDBOX_AWAIT_AND_SET(x, rb_num2int, xval);
                SANDBOX_AWAIT_AND_SET(y, rb_num2int, yval);

                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, color_class);

                {
                    Color value;
                    Color *color;
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    if (bitmap->surface() != NULL || bitmap->megaSurface() != NULL) {
                        value = bitmap->getPixel(x, y);
                    } else {
                        GFX_GUARD_EXC(value = bitmap->getPixel(x, y););
                    }
                    color = new Color(value);
                    set_private_data(obj, color);
                }
            }

            return obj;
        }
    };

    return sb()->bind<struct coro>()()(self, xval, yval);
}

static VALUE set_pixel(VALUE self, VALUE xval, VALUE yval, VALUE colorObj) {
    struct coro : boost::asio::coroutine {
        int x;
        int y;

        VALUE operator()(VALUE self, VALUE xval, VALUE yval, VALUE colorObj) {
            BOOST_ASIO_CORO_REENTER (this) {

                SANDBOX_AWAIT_AND_SET(x, rb_num2int, xval);
                SANDBOX_AWAIT_AND_SET(y, rb_num2int, yval);

                {
                    Color *color = get_private_data<Color>(colorObj);
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    GFX_GUARD_EXC(bitmap->setPixel(x, y, *color););
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, xval, yval, colorObj);
}

static VALUE hue_change(VALUE self, VALUE hueval) {
    struct coro : boost::asio::coroutine {
        int hue;

        VALUE operator()(VALUE self, VALUE hueval) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(hue, rb_num2int, hueval);

                {
                    Bitmap *bitmap = get_private_data<Bitmap>(self);
                    GFX_GUARD_EXC(bitmap->hueChange(hue););
                }
            }

            return self;
        }
    };

    return sb()->bind<struct coro>()()(self, hueval);
}

static VALUE draw_text(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;
        VALUE obj;
        int align;
        int x;
        int y;
        int width;
        int height;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc == 2 || argc == 3) {
                    if (rgssVer >= 2) {
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_as_string, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                    } else {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, (VALUE *)(**sb() + argv) + 1);
                    }
                    if (argc == 2) {
                        GFX_GUARD_EXC(get_private_data<Bitmap>(self)->drawText(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), (const char *)(**sb() + str));)
                    } else {
                        SANDBOX_AWAIT_AND_SET(align, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                        GFX_GUARD_EXC(get_private_data<Bitmap>(self)->drawText(get_private_data<Rect>(((VALUE *)(**sb() + argv))[0])->toIntRect(), (const char *)(**sb() + str), align);)
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
                        GFX_GUARD_EXC(get_private_data<Bitmap>(self)->drawText(x, y, width, height, (const char *)(**sb() + str));)
                    } else {
                        SANDBOX_AWAIT_AND_SET(align, rb_num2int, ((VALUE *)(**sb() + argv))[5]);
                        GFX_GUARD_EXC(get_private_data<Bitmap>(self)->drawText(x, y, width, height, (const char *)(**sb() + str), align);)
                    }
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE text_size(VALUE self, VALUE text) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;
        VALUE obj;

        VALUE operator()(VALUE self, VALUE text) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (rgssVer >= 2) {
                    SANDBOX_AWAIT_AND_SET(obj, rb_obj_as_string, text);
                    SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &obj);
                } else {
                    SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &text);
                }
                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, rect_class);
                set_private_data(obj, new Rect(get_private_data<Bitmap>(self)->textSize((const char *)(**sb() + str))));
            }

            return obj;
        }
    };

    return sb()->bind<struct coro>()()(self, text);
}

static VALUE get_raw_data(VALUE self) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE set_raw_data(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE to_file(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &value);
                SANDBOX_AWAIT(update_cwd);
                Bitmap *bitmap = get_private_data<Bitmap>(self);
                GFX_GUARD_EXC(bitmap->saveToFile((const char *)(**sb() + str)););
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE snap_to_bitmap(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE obj;
        int32_t pos;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc < 1) {
                    pos = -1;
                } else {
                    SANDBOX_AWAIT_AND_SET(pos, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                }

                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, bitmap_class);

                {
                    Bitmap *bitmap;
                    GFX_GUARD_EXC(bitmap = new Bitmap(*get_private_data<Bitmap>(self), pos););
                    set_private_data(obj, bitmap);
                }

                SANDBOX_AWAIT(bitmap_init_props, obj);
            }

            return obj;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE gradient_fill_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE clear_rect(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE blur(VALUE self) {
    get_private_data<Bitmap>(self)->blur();
    return SANDBOX_NIL;
}

static VALUE radial_blur(VALUE self, VALUE angle, VALUE divisions) {
    struct coro : boost::asio::coroutine {
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
    };

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

SANDBOX_DEF_GFX_PROP_B(Bitmap, Playing, playing);

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
    struct coro : boost::asio::coroutine {
        int32_t frame;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(frame, rb_num2int, value);
                Bitmap *bitmap = get_private_data<Bitmap>(self);
                GFX_GUARD_EXC(bitmap->gotoAndPlay(frame););
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE goto_and_stop(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        int32_t frame;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(frame, rb_num2int, value);
                Bitmap *bitmap = get_private_data<Bitmap>(self);
                GFX_GUARD_EXC(bitmap->gotoAndStop(frame););
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE frame_count(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Bitmap>(self)->numFrames());
}

static VALUE current_frame(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(get_private_data<Bitmap>(self)->currentFrameI());
}

static VALUE add_frame(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE value;
        int32_t pos;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                // TODO: require at least 1 argument
                if (argc < 2) {
                    pos = -1;
                } else {
                    SANDBOX_AWAIT_AND_SET(pos, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                    if (pos < 0) {
                        pos = 0;
                    }
                }
                if (argc >= 1) {
                    Bitmap *src = get_private_data<Bitmap>(((VALUE *)(**sb() + argv))[0]);
                    Bitmap *b = get_private_data<Bitmap>(self);
                    GFX_GUARD_EXC(pos = b->addFrame(*src, pos););
                }
                SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, pos);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE remove_frame(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        int32_t pos;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (argc < 2) {
                    pos = -1;
                } else {
                    SANDBOX_AWAIT_AND_SET(pos, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                    if (pos < 0) {
                        pos = 0;
                    }
                }
                Bitmap *b = get_private_data<Bitmap>(self);
                GFX_GUARD_EXC(b->removeFrame(pos););
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE next_frame(VALUE self) {
    Bitmap *b = get_private_data<Bitmap>(self);
    GFX_GUARD_EXC(b->nextFrame());
    return sb()->bind<struct rb_ll2inum>()()(b->currentFrameI());
}

static VALUE previous_frame(VALUE self) {
    Bitmap *b = get_private_data<Bitmap>(self);
    GFX_GUARD_EXC(b->previousFrame());
    return sb()->bind<struct rb_ll2inum>()()(b->currentFrameI());
}

SANDBOX_DEF_GFX_PROP_F(Bitmap, AnimationFPS, frame_rate);
SANDBOX_DEF_GFX_PROP_B(Bitmap, Looping, looping);

static VALUE get_font(VALUE self) {
    return sb()->bind<struct rb_iv_get>()()(self, "font");
}

static VALUE set_font(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
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
    };

    return sb()->bind<struct coro>()()(self, value);
}

void bitmap_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        bitmap_type = sb()->rb_data_type("Bitmap", NULL, dfree<Bitmap>, NULL, NULL, 0, 0, 0);
        SANDBOX_AWAIT_AND_SET(bitmap_class, rb_define_class, "Bitmap", sb()->rb_cObject());
        SANDBOX_AWAIT(rb_define_alloc_func, bitmap_class, alloc);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
        SANDBOX_AWAIT(disposable_binding_init<Bitmap>, bitmap_class);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "width", (VALUE (*)(ANYARGS))width, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "height", (VALUE (*)(ANYARGS))height, 0);

        SANDBOX_INIT_PROP_BIND(bitmap_class, hires);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "rect", (VALUE (*)(ANYARGS))rect, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "blt", (VALUE (*)(ANYARGS))blt, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "stretch_blt", (VALUE (*)(ANYARGS))stretch_blt, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "fill_rect", (VALUE (*)(ANYARGS))fill_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "clear", (VALUE (*)(ANYARGS))clear, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "get_pixel", (VALUE (*)(ANYARGS))get_pixel, 2);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "set_pixel", (VALUE (*)(ANYARGS))set_pixel, 3);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "hue_change", (VALUE (*)(ANYARGS))hue_change, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "draw_text", (VALUE (*)(ANYARGS))draw_text, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "text_size", (VALUE (*)(ANYARGS))text_size, 1);

        SANDBOX_INIT_PROP_BIND(bitmap_class, raw_data);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "to_file", (VALUE (*)(ANYARGS))to_file, 1);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "gradient_fill_rect", (VALUE (*)(ANYARGS))gradient_fill_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "clear_rect", (VALUE (*)(ANYARGS))clear_rect, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "blur", (VALUE (*)(ANYARGS))blur, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "radial_blur", (VALUE (*)(ANYARGS))radial_blur, 2);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "mega?", (VALUE (*)(ANYARGS))mega, 0);
        SANDBOX_AWAIT(rb_define_singleton_method, bitmap_class, "max_size", (VALUE (*)(ANYARGS))max_size, 0);

        SANDBOX_AWAIT(rb_define_method, bitmap_class, "animated?", (VALUE (*)(ANYARGS))get_animated, 0);
        SANDBOX_INIT_PROP_BIND(bitmap_class, playing);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "play", (VALUE (*)(ANYARGS))play, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "stop", (VALUE (*)(ANYARGS))stop, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "goto_and_play", (VALUE (*)(ANYARGS))goto_and_play, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "goto_and_stop", (VALUE (*)(ANYARGS))goto_and_stop, 1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "frame_count", (VALUE (*)(ANYARGS))frame_count, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "current_frame", (VALUE (*)(ANYARGS))current_frame, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "add_frame", (VALUE (*)(ANYARGS))add_frame, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "remove_frame", (VALUE (*)(ANYARGS))remove_frame, -1);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "next_frame", (VALUE (*)(ANYARGS))next_frame, 0);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "previous_frame", (VALUE (*)(ANYARGS))previous_frame, 0);
        SANDBOX_INIT_PROP_BIND(bitmap_class, frame_rate);
        SANDBOX_INIT_PROP_BIND(bitmap_class, looping);
        SANDBOX_AWAIT(rb_define_method, bitmap_class, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, -1);

        SANDBOX_INIT_PROP_BIND(bitmap_class, font);
    }
}
