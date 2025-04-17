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
    SANDBOX_COROUTINE(graphics_binding_init,
        static VALUE update(VALUE self) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        shState->graphics().update();
                        SANDBOX_YIELD;
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

                        for (i = 0; i < duration; ++i) {
                            shState->graphics().transition(duration, trans_map, vague, i, i);
                            SANDBOX_YIELD;
                        }
                    }

                    return SANDBOX_NIL;
                }

                ~coro() {
                    if (trans_map != NULL) {
                        delete trans_map;
                    }
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
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

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);

                        for (i = 0; i < duration; ++i) {
                            shState->graphics().fadeout(duration, i, i);
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

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(duration, rb_num2int, value);

                        for (i = 0; i < duration; ++i) {
                            shState->graphics().fadein(duration, i, i);
                            SANDBOX_YIELD;
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE frame_reset(VALUE self) {
            GFX_LOCK;
            shState->graphics().frameReset();
            GFX_UNLOCK;
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

        static VALUE width(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().width());
        }

        static VALUE height(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(shState->graphics().height());
        }

        static VALUE snap_to_bitmap(VALUE self) {
            SANDBOX_COROUTINE(coro,
                Bitmap *bitmap;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        GFX_GUARD_EXC(bitmap = shState->graphics().snapToBitmap(););
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Bitmap");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, bitmap);
                        SANDBOX_AWAIT(bitmap_init_props, bitmap, obj);
                    }

                    return obj;
                }
            )

            return sb()->bind<struct coro>()()(self);
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

        VALUE module;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Graphics");
                SANDBOX_AWAIT(rb_define_module_function, module, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "freeze", (VALUE (*)(ANYARGS))freeze, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "transition", (VALUE (*)(ANYARGS))transition, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "wait", (VALUE (*)(ANYARGS))wait, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "fadeout", (VALUE (*)(ANYARGS))fadeout, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "fadein", (VALUE (*)(ANYARGS))fadein, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_reset", (VALUE (*)(ANYARGS))frame_reset, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_count", (VALUE (*)(ANYARGS))get_frame_count, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_count=", (VALUE (*)(ANYARGS))set_frame_count, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_rate", (VALUE (*)(ANYARGS))get_frame_rate, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "frame_rate=", (VALUE (*)(ANYARGS))set_frame_rate, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "width", (VALUE (*)(ANYARGS))width, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "height", (VALUE (*)(ANYARGS))height, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "snap_to_bitmap", (VALUE (*)(ANYARGS))snap_to_bitmap, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "brightness", (VALUE (*)(ANYARGS))get_brightness, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "brightness=", (VALUE (*)(ANYARGS))set_brightness, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_GRAPHICS_BINDING_H
