/*
** graphics-binding.h
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

#ifndef MKXPZ_SANDBOX_GRAPHICS_BINDING_H
#define MKXPZ_SANDBOX_GRAPHICS_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "bitmap-binding.h"
#include "sharedstate.h"
#include "graphics.h"
#include "bitmap.h"

namespace mkxp_sandbox {
    static VALUE graphics_module;

    SANDBOX_COROUTINE(graphics_binding_init,
        static VALUE delta(VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE value;

                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_LOCK;
                        SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, shState->graphics().getDelta());
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE update(VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (shState->graphics().update()) {
                            SANDBOX_YIELD;
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE freeze(VALUE self) {
            shState->graphics().freeze();
            return SANDBOX_NIL;
        }

        static VALUE transition(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE frame_reset(VALUE self) {
            GFX_LOCK;
            shState->graphics().frameReset();
            GFX_UNLOCK;
            return SANDBOX_NIL;
        }

        static VALUE screenshot(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &value);
                        GFX_GUARD_EXC(shState->graphics().screenshot((const char *)(**sb() + str)););
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE reset(VALUE self) {
            GFX_GUARD_EXC(shState->graphics().reset(););
            return SANDBOX_NIL;
        }

        static VALUE get_frame_count(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().getFrameCount());
        }

        static VALUE set_frame_count(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int frame_count;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(frame_count, rb_num2int, value);
                        GFX_LOCK;
                        shState->graphics().setFrameCount(frame_count);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_frame_rate(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().getFrameRate());
        }

        static VALUE set_frame_rate(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int frame_rate;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(frame_rate, rb_num2int, value);
                        GFX_LOCK;
                        shState->graphics().setFrameRate(frame_rate);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

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

        static VALUE wait(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE fadeout(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE fadein(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE snap_to_bitmap(VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                VALUE obj;

                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(bitmap = shState->graphics().snapToBitmap(););
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, bitmap_class);
                        set_private_data(obj, bitmap);
                        SANDBOX_AWAIT(bitmap_init_props, bitmap, obj);
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self);
        }

        static VALUE resize_screen(VALUE self, VALUE width, VALUE height) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(self, width, height);
        }

        static VALUE resize_window(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
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
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE center(VALUE self) {
            shState->graphics().center();
            return SANDBOX_NIL;
        }

        static VALUE get_brightness(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().getBrightness());
        }

        static VALUE set_brightness(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t brightness;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(brightness, rb_num2int, value);
                        GFX_LOCK;
                        shState->graphics().setBrightness(brightness);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE play_movie(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t str;
                int32_t volume;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        // TODO: require at least 1 argument
                        SANDBOX_AWAIT_AND_SET(str, rb_string_value_cstr, &((VALUE *)(**sb() + argv))[0]);
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                        } else {
                            volume = 100;
                        }
                        bool skippable = argc >= 3 ? SANDBOX_VALUE_TO_BOOL(((VALUE *)(**sb() + argv))[2]) : false;
                        // TODO: use SANDBOX_YIELD as required
                        GFX_GUARD_EXC(shState->graphics().playMovie((const char *)(**sb() + str), volume, skippable));
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE get_fullscreen(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getFullscreen());
        }

        static VALUE set_fullscreen(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setFullscreen(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_show_cursor(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getShowCursor());
        }

        static VALUE set_show_cursor(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setShowCursor(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_scale(VALUE self) {
            return sb()->bind<struct rb_float_new>()()(shState->graphics().getScale());
        }

        static VALUE set_scale(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                double scale;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(scale, rb_num2dbl, value);
                        GFX_LOCK;
                        shState->graphics().setScale(scale);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_frameskip(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getFrameskip());
        }

        static VALUE set_frameskip(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setFrameskip(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_fixed_aspect_ratio(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getFixedAspectRatio());
        }

        static VALUE set_fixed_aspect_ratio(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setFixedAspectRatio(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_smooth_scaling(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().getSmoothScaling());
        }

        static VALUE set_smooth_scaling(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t smooth_scaling;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(smooth_scaling, rb_num2int, value);
                        GFX_LOCK;
                        shState->graphics().setSmoothScaling(smooth_scaling);
                        GFX_UNLOCK;
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_integer_scaling(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getIntegerScaling());
        }

        static VALUE set_integer_scaling(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setIntegerScaling(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_last_mile_scaling(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getLastMileScaling());
        }

        static VALUE set_last_mile_scaling(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setLastMileScaling(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        static VALUE get_threadsafe(VALUE self) {
            return SANDBOX_BOOL_TO_VALUE(shState->graphics().getThreadsafe());
        }

        static VALUE set_threadsafe(VALUE self, VALUE value) {
            GFX_LOCK;
            shState->graphics().setThreadsafe(SANDBOX_VALUE_TO_BOOL(value));
            GFX_UNLOCK;
            return value;
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(graphics_module, rb_define_module, "Graphics");
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "freeze", (VALUE (*)(ANYARGS))freeze, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "transition", (VALUE (*)(ANYARGS))transition, -1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_reset", (VALUE (*)(ANYARGS))frame_reset, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "screenshot", (VALUE (*)(ANYARGS))screenshot, 1);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "__reset__", (VALUE (*)(ANYARGS))reset, 0);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_count", (VALUE (*)(ANYARGS))get_frame_count, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_count=", (VALUE (*)(ANYARGS))set_frame_count, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_rate", (VALUE (*)(ANYARGS))get_frame_rate, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frame_rate=", (VALUE (*)(ANYARGS))set_frame_rate, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "average_frame_rate", (VALUE (*)(ANYARGS))average_frame_rate, 0);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "width", (VALUE (*)(ANYARGS))width, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "height", (VALUE (*)(ANYARGS))height, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_width", (VALUE (*)(ANYARGS))display_width, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "display_height", (VALUE (*)(ANYARGS))display_height, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "wait", (VALUE (*)(ANYARGS))wait, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadeout", (VALUE (*)(ANYARGS))fadeout, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fadein", (VALUE (*)(ANYARGS))fadein, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_screen", (VALUE (*)(ANYARGS))resize_screen, 2);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "resize_window", (VALUE (*)(ANYARGS))resize_window, -1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "center", (VALUE (*)(ANYARGS))center, 0);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "brightness", (VALUE (*)(ANYARGS))get_brightness, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "brightness=", (VALUE (*)(ANYARGS))set_brightness, 1);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "play_movie", (VALUE (*)(ANYARGS))play_movie, -1);

                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fullscreen", (VALUE (*)(ANYARGS))get_fullscreen, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fullscreen=", (VALUE (*)(ANYARGS))set_fullscreen, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "show_cursor", (VALUE (*)(ANYARGS))get_show_cursor, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "show_cursor=", (VALUE (*)(ANYARGS))set_show_cursor, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "scale", (VALUE (*)(ANYARGS))get_scale, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "scale=", (VALUE (*)(ANYARGS))set_scale, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frameskip", (VALUE (*)(ANYARGS))get_frameskip, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "frameskip=", (VALUE (*)(ANYARGS))set_frameskip, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fixed_aspect_ratio", (VALUE (*)(ANYARGS))get_fixed_aspect_ratio, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "fixed_aspect_ratio=", (VALUE (*)(ANYARGS))set_fixed_aspect_ratio, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "smooth_scaling", (VALUE (*)(ANYARGS))get_smooth_scaling, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "smooth_scaling=", (VALUE (*)(ANYARGS))set_smooth_scaling, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "integer_scaling", (VALUE (*)(ANYARGS))get_integer_scaling, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "integer_scaling=", (VALUE (*)(ANYARGS))set_integer_scaling, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "last_mile_scaling", (VALUE (*)(ANYARGS))get_last_mile_scaling, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "last_mile_scaling=", (VALUE (*)(ANYARGS))set_last_mile_scaling, 1);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "threadsafe", (VALUE (*)(ANYARGS))get_threadsafe, 0);
                SANDBOX_AWAIT(rb_define_module_function, graphics_module, "threadsafe=", (VALUE (*)(ANYARGS))set_threadsafe, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_GRAPHICS_BINDING_H
