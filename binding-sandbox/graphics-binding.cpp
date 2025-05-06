/*
** graphics-binding.cpp
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

#include "graphics-binding.h"
#include "bitmap-binding.h"
#include "bitmap.h"
#include "graphics.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::graphics_module;

static VALUE delta(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE value;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                GFX_LOCK;
                SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, shState->graphics().getDelta());
                GFX_UNLOCK;
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE update(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                if (shState->graphics().update()) {
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE freeze(VALUE self) {
    shState->graphics().freeze();
    return SANDBOX_NIL;
}

static VALUE transition(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        Bitmap *trans_map;
        wasm_ptr_t str;
        int32_t duration;
        int32_t vague;
        int32_t i;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                trans_map = NULL;
                duration = 8;
                vague = 40;

                sb().transitioning = true;

                if (!shState->graphics().frozen()) {
                    return SANDBOX_NIL;
                }

                if (argc >= 1) {
                    SANDBOX_AWAIT_AND_SET(duration, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                    if (argc >= 2) {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &((VALUE *)(**sb() + argv))[1]);
                        if (*(const char *)(**sb() + str)) {
                            SANDBOX_AWAIT(update_cwd);
                            trans_map = new Bitmap((const char *)(**sb() + str));
                        }
                        if (argc >= 3) {
                            SANDBOX_AWAIT_AND_SET(vague, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                        }
                    }
                }

                if (duration <= 0) {
                    shState->graphics().transition(duration, trans_map, vague, 0, 0);
                } else {
                    for (i = 0; i < duration; ++i) {
                        shState->graphics().transition(duration, trans_map, vague, i, i);
                        SANDBOX_YIELD;
                    }
                }
            }

            return SANDBOX_NIL;
        }

        ~coro() {
            sb().transitioning = false;

            if (trans_map != NULL) {
                delete trans_map;
            }
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE frame_reset(VALUE self) {
    GFX_LOCK;
    shState->graphics().frameReset();
    GFX_UNLOCK;
    return SANDBOX_NIL;
}

static VALUE screenshot(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &value);
                SANDBOX_AWAIT(update_cwd);
                GFX_GUARD_EXC(shState->graphics().screenshot((const char *)(**sb() + str)););
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE reset(VALUE self) {
    GFX_GUARD_EXC(shState->graphics().reset(););
    return SANDBOX_NIL;
}

SANDBOX_DEF_GRA_PROP_I(FrameCount, frame_count);
SANDBOX_DEF_GRA_PROP_I(FrameRate, frame_rate);

static VALUE average_frame_rate(VALUE self) {
    return sb()->bind<struct rb_float_new>()()(shState->graphics().averageFrameRate());
}

static VALUE width(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().width());
}

static VALUE height(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().height());
}

static VALUE display_width(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().displayWidth());
}

static VALUE display_height(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(shState->graphics().displayHeight());
}

static VALUE wait_(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        int32_t duration;
        int32_t i;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);

                for (i = 0; i < duration; ++i) {
                    shState->graphics().wait(duration, i, i);
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE fadeout(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        int32_t duration;
        int32_t i;
        int32_t brightness;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);
                brightness = shState->graphics().getBrightness();

                for (i = 0; i < duration; ++i) {
                    shState->graphics().fadeout(duration, i, i, brightness);
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE fadein(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        int32_t duration;
        int32_t i;
        int32_t brightness;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);
                brightness = shState->graphics().getBrightness();

                for (i = 0; i < duration; ++i) {
                    shState->graphics().fadein(duration, i, i, brightness);
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE snap_to_bitmap(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE obj;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, bitmap_class);
                {
                    Bitmap *bitmap;
                    GFX_GUARD_EXC(bitmap = shState->graphics().snapToBitmap(););
                    set_private_data(obj, bitmap);
                }
                SANDBOX_AWAIT(bitmap_init_props, obj);
            }

            return obj;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE resize_screen(VALUE self, VALUE width, VALUE height) {
    struct coro : boost::asio::coroutine {
        int32_t w;
        int32_t h;

        VALUE operator()(VALUE self, VALUE width, VALUE height) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(w, rb_num2int, width);
                SANDBOX_AWAIT_AND_SET(h, rb_num2int, height);
                GFX_LOCK;
                shState->graphics().resizeScreen(w, h);
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, width, height);
}

static VALUE resize_window(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        int32_t w;
        int32_t h;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                // TODO: require at least 2 arguments
                SANDBOX_AWAIT_AND_SET(w, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                SANDBOX_AWAIT_AND_SET(h, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                GFX_LOCK;
                if (argc >= 3) {
                    shState->graphics().resizeWindow(w, h, SANDBOX_VALUE_TO_BOOL(((VALUE *)(**sb() + argv))[2]));
                } else {
                    shState->graphics().resizeWindow(w, h);
                }
                GFX_UNLOCK;
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE center(VALUE self) {
    shState->graphics().center();
    return SANDBOX_NIL;
}

SANDBOX_DEF_GRA_PROP_I(Brightness, brightness);

static VALUE play_movie(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        wasm_ptr_t str;
        int32_t volume;
        bool skippable;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                // TODO: require at least 1 argument
                SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &((VALUE *)(**sb() + argv))[0]);
                if (argc >= 2) {
                    SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                } else {
                    volume = 100;
                }
                skippable = argc >= 3 ? SANDBOX_VALUE_TO_BOOL(((VALUE *)(**sb() + argv))[2]) : false;

                SANDBOX_AWAIT(update_cwd);
                GFX_GUARD_EXC(sb().set_movie(shState->graphics().playMovie((const char *)(**sb() + str), volume, skippable)););
                while (sb().get_movie_from_main_thread() != nullptr) {
                    SANDBOX_YIELD;
                    GFX_GUARD_EXC(sb().set_movie(shState->graphics().playMovie(sb().get_movie_from_main_thread())););
                }
            }

            return SANDBOX_NIL;
        }

        ~coro() {
            GFX_GUARD_EXC(sb().set_movie(nullptr););
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

SANDBOX_DEF_GRA_PROP_B(Fullscreen, fullscreen);
SANDBOX_DEF_GRA_PROP_B(ShowCursor, show_cursor);
SANDBOX_DEF_GRA_PROP_D(Scale, scale);
SANDBOX_DEF_GRA_PROP_B(Frameskip, frameskip);
SANDBOX_DEF_GRA_PROP_B(FixedAspectRatio, fixed_aspect_ratio);
SANDBOX_DEF_GRA_PROP_B(SmoothScaling, smooth_scaling);
SANDBOX_DEF_GRA_PROP_B(IntegerScaling, integer_scaling);
SANDBOX_DEF_GRA_PROP_B(LastMileScaling, last_mile_scaling);
SANDBOX_DEF_GRA_PROP_B(Threadsafe, threadsafe);

void graphics_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(graphics_module, rb_define_module, "Graphics");
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "update", (VALUE (*)(ANYARGS))update, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "freeze", (VALUE (*)(ANYARGS))freeze, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "transition", (VALUE (*)(ANYARGS))transition, -1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_reset", (VALUE (*)(ANYARGS))frame_reset, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "screenshot", (VALUE (*)(ANYARGS))screenshot, 1);

        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "__reset__", (VALUE (*)(ANYARGS))reset, 0);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frame_count);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frame_rate);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "average_frame_rate", (VALUE (*)(ANYARGS))average_frame_rate, 0);

        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "width", (VALUE (*)(ANYARGS))width, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "height", (VALUE (*)(ANYARGS))height, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_width", (VALUE (*)(ANYARGS))display_width, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_height", (VALUE (*)(ANYARGS))display_height, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "wait", (VALUE (*)(ANYARGS))wait_, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadeout", (VALUE (*)(ANYARGS))fadeout, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadein", (VALUE (*)(ANYARGS))fadein, 1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, 0);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_screen", (VALUE (*)(ANYARGS))resize_screen, 2);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_window", (VALUE (*)(ANYARGS))resize_window, -1);
        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "center", (VALUE (*)(ANYARGS))center, 0);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, brightness);

        SANDBOX_AWAIT(rb_define_module_function, graphics_module, "play_movie", (VALUE (*)(ANYARGS))play_movie, -1);

        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, fullscreen);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, show_cursor);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, scale);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, frameskip);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, fixed_aspect_ratio);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, smooth_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, integer_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, last_mile_scaling);
        SANDBOX_INIT_MODULE_PROP_BIND(graphics_module, threadsafe);
    }
}
