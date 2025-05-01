/*
** etc-binding.cpp
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

#include "etc-binding.h"
#include "serializable-binding.h"
#include "mkxp-polyfill.h" // std::sprintf
#include "etc.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::color_class;
VALUE mkxp_sandbox::tone_class;
VALUE mkxp_sandbox::rect_class;
static struct bindings::rb_data_type color_type;
static struct bindings::rb_data_type tone_type;
static struct bindings::rb_data_type rect_type;

struct color_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC_WITH_INIT(color_type, new Color);

    static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            Color *color;
            double red;
            double green;
            double blue;
            double alpha;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 0) {
                        color = new Color;
                    } else {
                        SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                        if (argc <= 3) {
                            color = new Color(red, green, blue);
                        } else {
                            SANDBOX_AWAIT_AND_SET(alpha, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                            color = new Color(red, green, blue, alpha);
                        }
                    }

                    set_private_data(self, color);
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
                    if (self != value) {
                        SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                        set_private_data(self, new Color(*get_private_data<Color>(value)));
                    }
                }

                return self;
            }
        };

        return sb()->bind<struct coro>()()(self, value);
    }

    static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            double red;
            double green;
            double blue;
            double alpha;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 1) {
                        *get_private_data<Color>(self) = *get_private_data<Color>(((VALUE *)(**sb() + argv))[0]);
                    } else {
                        SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                        if (argc <= 3) {
                            alpha = 255;
                        } else {
                            SANDBOX_AWAIT_AND_SET(alpha, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                        }
                        get_private_data<Color>(self)->set(red, green, blue, alpha);
                    }
                }

                return SANDBOX_NIL;
            }
        };

        return sb()->bind<struct coro>()()(argc, argv, self);
    }

    SANDBOX_DEF_PROP_D(Color, Red, red);
    SANDBOX_DEF_PROP_D(Color, Green, green);
    SANDBOX_DEF_PROP_D(Color, Blue, blue);
    SANDBOX_DEF_PROP_D(Color, Alpha, alpha);

    static VALUE equal(VALUE self, VALUE other) {
        struct coro : boost::asio::coroutine {
            VALUE value;

            VALUE operator()(VALUE self, VALUE other) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (rgssVer >= 3) {
                        SANDBOX_AWAIT_AND_SET(value, rb_typeddata_is_kind_of, other, color_type);
                        if (!SANDBOX_VALUE_TO_BOOL(value)) {
                            return SANDBOX_FALSE;
                        }
                    }
                    return SANDBOX_BOOL_TO_VALUE(*get_private_data<Color>(self) == *get_private_data<Color>(other));
                }

                return value;
            }
        };

        return sb()->bind<struct coro>()()(self, other);
    }

    static VALUE stringify(VALUE self) {
        char buf[sizeof("(%f, %f, %f, %f)") + 4 * 24] = {0};
        Color *color = get_private_data<Color>(self);
        std::sprintf(buf, "(%f, %f, %f, %f)", color->red, color->green, color->blue, color->alpha);
        return sb()->bind<struct rb_str_new_cstr>()()(buf);
    }

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            color_type = sb()->rb_data_type("Color", NULL, dfree<Color>, NULL, NULL, 0, 0, 0);
            SANDBOX_AWAIT_AND_SET(color_class, rb_define_class, "Color", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, color_class, alloc);
            SANDBOX_AWAIT(rb_define_method, color_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
            SANDBOX_AWAIT(rb_define_method, color_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
            SANDBOX_AWAIT(serializable_binding_init<Color>, color_class);
            SANDBOX_AWAIT(rb_define_method, color_class, "set", (VALUE (*)(ANYARGS))set, -1);
            SANDBOX_INIT_PROP_BIND(color_class, red);
            SANDBOX_INIT_PROP_BIND(color_class, green);
            SANDBOX_INIT_PROP_BIND(color_class, blue);
            SANDBOX_INIT_PROP_BIND(color_class, alpha);
            SANDBOX_AWAIT(rb_define_method, color_class, "==", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, color_class, "===", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, color_class, "to_s", (VALUE (*)(ANYARGS))stringify, 0);
            SANDBOX_AWAIT(rb_define_method, color_class, "inspect", (VALUE (*)(ANYARGS))stringify, 0);
        }
    }
};

struct tone_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC_WITH_INIT(tone_type, new Tone);

    static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            Tone *tone;
            double red;
            double green;
            double blue;
            double gray;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 0) {
                        tone = new Tone;
                    } else {
                        SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                        if (argc <= 3) {
                            tone = new Tone(red, green, blue);
                        } else {
                            SANDBOX_AWAIT_AND_SET(gray, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                            tone = new Tone(red, green, blue, gray);
                        }
                    }

                    set_private_data(self, tone);
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
                    if (self != value) {
                        SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                        set_private_data(self, new Tone(*get_private_data<Tone>(value)));
                    }
                }

                return self;
            }
        };

        return sb()->bind<struct coro>()()(self, value);
    }

    static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            double red;
            double green;
            double blue;
            double gray;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 1) {
                        *get_private_data<Tone>(self) = *get_private_data<Tone>(((VALUE *)(**sb() + argv))[0]);
                    } else {
                        SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                        if (argc <= 3) {
                            gray = 0;
                        } else {
                            SANDBOX_AWAIT_AND_SET(gray, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                        }
                        get_private_data<Tone>(self)->set(red, green, blue, gray);
                    }
                }

                return SANDBOX_NIL;
            }
        };

        return sb()->bind<struct coro>()()(argc, argv, self);
    }

    SANDBOX_DEF_PROP_D(Tone, Red, red);
    SANDBOX_DEF_PROP_D(Tone, Green, green);
    SANDBOX_DEF_PROP_D(Tone, Blue, blue);
    SANDBOX_DEF_PROP_D(Tone, Gray, gray);

    static VALUE equal(VALUE self, VALUE other) {
        struct coro : boost::asio::coroutine {
            VALUE value;

            VALUE operator()(VALUE self, VALUE other) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (rgssVer >= 3) {
                        SANDBOX_AWAIT_AND_SET(value, rb_typeddata_is_kind_of, other, color_type);
                        if (!SANDBOX_VALUE_TO_BOOL(value)) {
                            return SANDBOX_FALSE;
                        }
                    }
                    return SANDBOX_BOOL_TO_VALUE(*get_private_data<Tone>(self) == *get_private_data<Tone>(other));
                }

                return value;
            }
        };

        return sb()->bind<struct coro>()()(self, other);
    }

    static VALUE stringify(VALUE self) {
        char buf[sizeof("(%f, %f, %f, %f)") + 4 * 24] = {0};
        Tone *tone = get_private_data<Tone>(self);
        std::sprintf(buf, "(%f, %f, %f, %f)", tone->red, tone->green, tone->blue, tone->gray);
        return sb()->bind<struct rb_str_new_cstr>()()(buf);
    }

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            tone_type = sb()->rb_data_type("Tone", NULL, dfree<Tone>, NULL, NULL, 0, 0, 0);
            SANDBOX_AWAIT_AND_SET(tone_class, rb_define_class, "Tone", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, tone_class, alloc);
            SANDBOX_AWAIT(rb_define_method, tone_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
            SANDBOX_AWAIT(rb_define_method, tone_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
            SANDBOX_AWAIT(serializable_binding_init<Tone>, tone_class);
            SANDBOX_AWAIT(rb_define_method, tone_class, "set", (VALUE (*)(ANYARGS))set, -1);
            SANDBOX_INIT_PROP_BIND(tone_class, red);
            SANDBOX_INIT_PROP_BIND(tone_class, green);
            SANDBOX_INIT_PROP_BIND(tone_class, blue);
            SANDBOX_INIT_PROP_BIND(tone_class, gray);
            SANDBOX_AWAIT(rb_define_method, tone_class, "==", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, tone_class, "===", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, tone_class, "to_s", (VALUE (*)(ANYARGS))stringify, 0);
            SANDBOX_AWAIT(rb_define_method, tone_class, "inspect", (VALUE (*)(ANYARGS))stringify, 0);
        }
    }
};

struct rect_binding_init : boost::asio::coroutine {
    SANDBOX_DEF_ALLOC_WITH_INIT(rect_type, new Rect);

    static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            Rect *rect;
            int x;
            int y;
            int width;
            int height;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 0) {
                        rect = new Rect;
                    } else {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(width, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                        SANDBOX_AWAIT_AND_SET(height, rb_num2int, ((VALUE *)(**sb() + argv))[3]);
                        rect = new Rect(x, y, width, height);
                    }

                    set_private_data(self, rect);
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
                    if (self != value) {
                        SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                        set_private_data(self, new Rect(*get_private_data<Rect>(value)));
                    }
                }

                return self;
            }
        };

        return sb()->bind<struct coro>()()(self, value);
    }

    static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
        struct coro : boost::asio::coroutine {
            int x;
            int y;
            int width;
            int height;

            VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (argc == 1) {
                        *get_private_data<Rect>(self) = *get_private_data<Rect>(((VALUE *)(**sb() + argv))[0]);
                    } else {
                        SANDBOX_AWAIT_AND_SET(x, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                        SANDBOX_AWAIT_AND_SET(y, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                        SANDBOX_AWAIT_AND_SET(width, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                        SANDBOX_AWAIT_AND_SET(height, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                        get_private_data<Rect>(self)->set(x, y, width, height);
                    }
                }

                return SANDBOX_NIL;
            }
        };

        return sb()->bind<struct coro>()()(argc, argv, self);
    }

    static VALUE empty(VALUE self) {
        get_private_data<Rect>(self)->empty();
        return self;
    }

    SANDBOX_DEF_PROP_D(Rect, X, x);
    SANDBOX_DEF_PROP_D(Rect, Y, y);
    SANDBOX_DEF_PROP_D(Rect, Width, width);
    SANDBOX_DEF_PROP_D(Rect, Height, height);

    static VALUE equal(VALUE self, VALUE other) {
        struct coro : boost::asio::coroutine {
            VALUE value;

            VALUE operator()(VALUE self, VALUE other) {
                BOOST_ASIO_CORO_REENTER (this) {
                    if (rgssVer >= 3) {
                        SANDBOX_AWAIT_AND_SET(value, rb_typeddata_is_kind_of, other, color_type);
                        if (!SANDBOX_VALUE_TO_BOOL(value)) {
                            return SANDBOX_FALSE;
                        }
                    }
                    return SANDBOX_BOOL_TO_VALUE(*get_private_data<Rect>(self) == *get_private_data<Rect>(other));
                }

                return value;
            }
        };

        return sb()->bind<struct coro>()()(self, other);
    }

    static VALUE stringify(VALUE self) {
        char buf[50] = {0};
        Rect *rect = get_private_data<Rect>(self);
        std::sprintf(buf, "(%d, %d, %d, %d)", rect->x, rect->y, rect->width, rect->height);
        return sb()->bind<struct rb_str_new_cstr>()()(buf);
    }

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            rect_type = sb()->rb_data_type("Rect", NULL, dfree<Rect>, NULL, NULL, 0, 0, 0);
            SANDBOX_AWAIT_AND_SET(rect_class, rb_define_class, "Rect", sb()->rb_cObject());
            SANDBOX_AWAIT(rb_define_alloc_func, rect_class, alloc);
            SANDBOX_AWAIT(rb_define_method, rect_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
            SANDBOX_AWAIT(rb_define_method, rect_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
            SANDBOX_AWAIT(serializable_binding_init<Rect>, rect_class);
            SANDBOX_AWAIT(rb_define_method, rect_class, "set", (VALUE (*)(ANYARGS))set, -1);
            SANDBOX_AWAIT(rb_define_method, rect_class, "empty", (VALUE (*)(ANYARGS))empty, 0);
            SANDBOX_INIT_PROP_BIND(rect_class, x);
            SANDBOX_INIT_PROP_BIND(rect_class, y);
            SANDBOX_INIT_PROP_BIND(rect_class, width);
            SANDBOX_INIT_PROP_BIND(rect_class, height);
            SANDBOX_AWAIT(rb_define_method, rect_class, "==", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, rect_class, "===", (VALUE (*)(ANYARGS))equal, 1);
            SANDBOX_AWAIT(rb_define_method, rect_class, "to_s", (VALUE (*)(ANYARGS))stringify, 0);
            SANDBOX_AWAIT(rb_define_method, rect_class, "inspect", (VALUE (*)(ANYARGS))stringify, 0);
        }
    }
};

void etc_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(color_binding_init);
        SANDBOX_AWAIT(tone_binding_init);
        SANDBOX_AWAIT(rect_binding_init);
    }
}
