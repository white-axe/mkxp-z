/*
** audio-binding.h
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

#ifndef MKXPZ_SANDBOX_AUDIO_BINDING_H
#define MKXPZ_SANDBOX_AUDIO_BINDING_H

#include "core.h"
#include "sandbox.h"

namespace mkxp_sandbox {
    SANDBOX_COROUTINE(audio_binding_init,
        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        static VALUE bgm_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t filename;
                int32_t volume;
                int32_t pitch;
                double pos;
                int32_t track;
                bool have_track;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        volume = 100;
                        pitch = 100;
                        pos = 0.0;
                        have_track = false;

                        SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(pitch, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                if (argc >= 4) {
                                    SANDBOX_AWAIT_AND_SET(pos, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                                    if (argc >= 5) {
                                        SANDBOX_AWAIT_AND_SET(track, rb_num2int, ((VALUE *)(**sb() + argv))[4]);
                                        have_track = true;
                                    }
                                }
                            }
                        }

                        if (have_track) {
                            mkxp_retro::audio->bgmPlay((const char *)(**sb() + filename), volume, pitch, pos, track);
                        } else {
                            mkxp_retro::audio->bgmPlay((const char *)(**sb() + filename), volume, pitch, pos);
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE bgm_stop(VALUE self) {
            mkxp_retro::audio->bgmStop();
            return SANDBOX_NIL;
        }

        static VALUE bgs_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t filename;
                int32_t volume;
                int32_t pitch;
                double pos;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        volume = 100;
                        pitch = 100;
                        pos = 0.0;

                        SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(pitch, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                if (argc >= 4) {
                                    SANDBOX_AWAIT_AND_SET(pos, rb_num2dbl, ((VALUE *)(**sb() + argv))[3]);
                                }
                            }
                        }

                        mkxp_retro::audio->bgsPlay((const char *)(**sb() + filename), volume, pitch, pos);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE bgs_stop(VALUE self) {
            mkxp_retro::audio->bgsStop();
            return SANDBOX_NIL;
        }

        static VALUE me_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t filename;
                int32_t volume;
                int32_t pitch;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        volume = 100;
                        pitch = 100;

                        SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(pitch, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            }
                        }

                        mkxp_retro::audio->mePlay((const char *)(**sb() + filename), volume, pitch);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE me_stop(VALUE self) {
            mkxp_retro::audio->meStop();
            return SANDBOX_NIL;
        }

        static VALUE se_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                wasm_ptr_t filename;
                int32_t volume;
                int32_t pitch;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        volume = 100;
                        pitch = 100;

                        SANDBOX_AWAIT_AND_SET(filename, rb_string_value_cstr, (VALUE *)(**sb() + argv));
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(volume, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(pitch, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            }
                        }

                        mkxp_retro::audio->sePlay((const char *)(**sb() + filename), volume, pitch);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE se_stop(VALUE self) {
            mkxp_retro::audio->seStop();
            return SANDBOX_NIL;
        }

        VALUE module;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Audio");
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_play", (VALUE (*)(ANYARGS))bgm_play, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_stop", (VALUE (*)(ANYARGS))bgm_stop, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_pos", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_volume", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_set_volume", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_play", (VALUE (*)(ANYARGS))bgs_play, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_stop", (VALUE (*)(ANYARGS))bgs_stop, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_pos", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_play", (VALUE (*)(ANYARGS))me_play, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_stop", (VALUE (*)(ANYARGS))me_stop, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "se_play", (VALUE (*)(ANYARGS))se_play, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "se_stop", (VALUE (*)(ANYARGS))se_stop, 0);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_AUDIO_BINDING_H
