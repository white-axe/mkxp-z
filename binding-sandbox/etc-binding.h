/*
** etc-binding.h
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

#ifndef MKXPZ_SANDBOX_ETC_BINDING_H
#define MKXPZ_SANDBOX_ETC_BINDING_H

#include "sandbox.h"
#include "etc.h"
#include "binding-util.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type color_type;
    static struct mkxp_sandbox::bindings::rb_data_type tone_type;
    static struct mkxp_sandbox::bindings::rb_data_type rect_type;

    SANDBOX_COROUTINE(etc_binding_init,
        SANDBOX_COROUTINE(color_binding_init,
            SANDBOX_DEF_ALLOC_WITH_INIT(color_type, new Color)
            SANDBOX_DEF_DFREE(Color)
            SANDBOX_DEF_LOAD(Color)

            static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Color *color;
                    double red;
                    double green;
                    double blue;
                    double alpha;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            if (argc == 0) {
                                color = new Color();
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
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Color *color;
                    double red;
                    double green;
                    double blue;
                    double alpha;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            color = get_private_data<Color>(self);

                            SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                            if (argc <= 3) {
                                alpha = 255;
                            } else {
                                SANDBOX_AWAIT_AND_SET(alpha, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                            }

                            color->set(red, green, blue, alpha);
                        }

                        return SANDBOX_NIL;
                    }
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE get_red(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Color>(self)->getRed());
            }

            static VALUE set_red(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int red;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(red, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Color>(self)->setRed(red));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_green(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Color>(self)->getGreen());
            }

            static VALUE set_green(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int green;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(green, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Color>(self)->setGreen(green));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_blue(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Color>(self)->getBlue());
            }

            static VALUE set_blue(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int blue;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(blue, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Color>(self)->setBlue(blue));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_alpha(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Color>(self)->getAlpha());
            }

            static VALUE set_alpha(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int alpha;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(alpha, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Color>(self)->setAlpha(alpha));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    color_type = sb()->rb_data_type("Color", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Color", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "set", (VALUE (*)(ANYARGS))set, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "red", (VALUE (*)(ANYARGS))get_red, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "red=", (VALUE (*)(ANYARGS))set_red, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "green", (VALUE (*)(ANYARGS))get_green, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "green=", (VALUE (*)(ANYARGS))set_green, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "blue", (VALUE (*)(ANYARGS))get_blue, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "blue=", (VALUE (*)(ANYARGS))set_blue, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "alpha", (VALUE (*)(ANYARGS))get_alpha, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "alpha=", (VALUE (*)(ANYARGS))set_alpha, 1);
                }
            }
        )

        SANDBOX_COROUTINE(tone_binding_init,
            SANDBOX_DEF_ALLOC_WITH_INIT(tone_type, new Tone)
            SANDBOX_DEF_DFREE(Tone)
            SANDBOX_DEF_LOAD(Tone)

            static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Tone *tone;
                    double red;
                    double green;
                    double blue;
                    double gray;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            if (argc == 0) {
                                tone = new Tone();
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
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Tone *tone;
                    double red;
                    double green;
                    double blue;
                    double gray;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            tone = get_private_data<Tone>(self);

                            SANDBOX_AWAIT_AND_SET(red, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(green, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(blue, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                            if (argc <= 3) {
                                gray = 0;
                            } else {
                                SANDBOX_AWAIT_AND_SET(gray, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                            }

                            tone->set(red, green, blue, gray);
                        }

                        return SANDBOX_NIL;
                    }
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE get_red(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tone>(self)->getRed());
            }

            static VALUE set_red(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int red;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(red, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Tone>(self)->setRed(red));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_green(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tone>(self)->getGreen());
            }

            static VALUE set_green(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int green;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(green, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Tone>(self)->setGreen(green));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_blue(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tone>(self)->getBlue());
            }

            static VALUE set_blue(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int blue;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(blue, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Tone>(self)->setBlue(blue));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_gray(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Tone>(self)->getGray());
            }

            static VALUE set_gray(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int gray;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(gray, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Tone>(self)->setGray(gray));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    tone_type = sb()->rb_data_type("Tone", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Tone", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "set", (VALUE (*)(ANYARGS))set, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "red", (VALUE (*)(ANYARGS))get_red, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "red=", (VALUE (*)(ANYARGS))set_red, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "green", (VALUE (*)(ANYARGS))get_green, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "green=", (VALUE (*)(ANYARGS))set_green, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "blue", (VALUE (*)(ANYARGS))get_blue, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "blue=", (VALUE (*)(ANYARGS))set_blue, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "gray", (VALUE (*)(ANYARGS))get_gray, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "gray=", (VALUE (*)(ANYARGS))set_gray, 1);
                }
            }
        )

        SANDBOX_COROUTINE(rect_binding_init,
            SANDBOX_DEF_ALLOC_WITH_INIT(rect_type, new Rect)
            SANDBOX_DEF_DFREE(Rect)
            SANDBOX_DEF_LOAD(Rect)

            static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Rect *rect;
                    int x;
                    int y;
                    int width;
                    int height;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            if (argc == 0) {
                                rect = new Rect();
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
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
                SANDBOX_COROUTINE(coro,
                    Rect *rect;
                    int x;
                    int y;
                    int width;
                    int height;

                    VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            rect = get_private_data<Rect>(self);

                            SANDBOX_AWAIT_AND_SET(x, rb_num2dbl, ((VALUE *)(**sb() + argv))[0]);
                            SANDBOX_AWAIT_AND_SET(y, rb_num2dbl, ((VALUE *)(**sb() + argv))[1]);
                            SANDBOX_AWAIT_AND_SET(width, rb_num2dbl, ((VALUE *)(**sb() + argv))[2]);
                            SANDBOX_AWAIT_AND_SET(height, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);

                            rect->set(x, y, width, height);
                        }

                        return SANDBOX_NIL;
                    }
                )

                return sb()->bind<struct coro>()()(argc, argv, self);
            }

            static VALUE empty(VALUE self) {
                get_private_data<Rect>(self)->empty();
                return self;
            }

            static VALUE get_x(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Rect>(self)->getX());
            }

            static VALUE set_x(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int x;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(x, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Rect>(self)->setX(x));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_y(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Rect>(self)->getY());
            }

            static VALUE set_y(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int y;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Rect>(self)->setY(y));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_width(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Rect>(self)->getWidth());
            }

            static VALUE set_width(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int width;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(width, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Rect>(self)->setWidth(width));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            static VALUE get_height(VALUE self) {
                return sb()->bind<struct rb_ll2inum>()()(get_private_data<Rect>(self)->getHeight());
            }

            static VALUE set_height(VALUE self, VALUE value) {
                SANDBOX_COROUTINE(coro,
                    int height;

                    VALUE operator()(VALUE self, VALUE value) {
                        BOOST_ASIO_CORO_REENTER (this) {
                            SANDBOX_AWAIT_AND_SET(height, rb_num2int, value);
                            GFX_GUARD_EXC(get_private_data<Rect>(self)->setHeight(height));
                        }

                        return value;
                    }
                )

                return sb()->bind<struct coro>()()(self, value);
            }

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    rect_type = sb()->rb_data_type("Rect", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Rect", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "set", (VALUE (*)(ANYARGS))set, -1);
                    SANDBOX_AWAIT(rb_define_method, klass, "empty", (VALUE (*)(ANYARGS))empty, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "x", (VALUE (*)(ANYARGS))get_x, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "x=", (VALUE (*)(ANYARGS))set_x, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "y", (VALUE (*)(ANYARGS))get_y, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "y=", (VALUE (*)(ANYARGS))set_y, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "width", (VALUE (*)(ANYARGS))get_width, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "width=", (VALUE (*)(ANYARGS))set_width, 1);
                    SANDBOX_AWAIT(rb_define_method, klass, "height", (VALUE (*)(ANYARGS))get_height, 0);
                    SANDBOX_AWAIT(rb_define_method, klass, "height=", (VALUE (*)(ANYARGS))set_height, 1);
                }
            }
        )

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(color_binding_init);
                SANDBOX_AWAIT(tone_binding_init);
                SANDBOX_AWAIT(rect_binding_init);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_ETC_BINDING_H
