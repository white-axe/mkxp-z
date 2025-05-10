/*
** audio-binding.cpp
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

#include "audio-binding.h"
#include "core.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::audio_module;

static VALUE bgm_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, wasm_ptr_t, int32_t, int32_t, int32_t, uint8_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = 0.0;
                SANDBOX_SLOT(2) = 100;
                SANDBOX_SLOT(3) = 100;
                SANDBOX_SLOT(5) = false;

                SANDBOX_AWAIT_S(1, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        if (argc >= 4) {
                            SANDBOX_AWAIT_S(0, rb_num2dbl, sb()->ref<VALUE>(argv, 3));
                            if (argc >= 5) {
                                SANDBOX_AWAIT_S(4, rb_num2int, sb()->ref<VALUE>(argv, 4));
                                SANDBOX_SLOT(5) = true;
                            }
                        }
                    }
                }

                if (SANDBOX_SLOT(5)) {
                    mkxp_retro::audio->bgmPlay(sb()->str(SANDBOX_SLOT(1)), SANDBOX_SLOT(2), SANDBOX_SLOT(3), SANDBOX_SLOT(0), SANDBOX_SLOT(4));
                } else {
                    mkxp_retro::audio->bgmPlay(sb()->str(SANDBOX_SLOT(1)), SANDBOX_SLOT(2), SANDBOX_SLOT(3), SANDBOX_SLOT(0));
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgm_stop(VALUE self) {
    mkxp_retro::audio->bgmStop();
    return SANDBOX_NIL;
}

static VALUE bgm_fade(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(1) = -127;
                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                }
                mkxp_retro::audio->bgmFade(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgm_pos(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(2) = -127;
                if (argc >= 1) {
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 0));
                }
                SANDBOX_SLOT(0) = mkxp_retro::audio->bgmPos(SANDBOX_SLOT(2));
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgm_volume(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(1) = -127;
                if (argc >= 1) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 0));
                }
                SANDBOX_SLOT(2) = mkxp_retro::audio->bgmGetVolume(SANDBOX_SLOT(1));
                SANDBOX_AWAIT_S(0, rb_ll2inum, SANDBOX_SLOT(2));
            }

            return SANDBOX_SLOT(0);
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgm_set_volume(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(1) = -127;
                SANDBOX_AWAIT_S(0, rb_num2int, sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                }
                mkxp_retro::audio->bgmSetVolume(SANDBOX_SLOT(0), SANDBOX_SLOT(1));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgs_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, wasm_ptr_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = 0.0;
                SANDBOX_SLOT(2) = 100;
                SANDBOX_SLOT(3) = 100;

                SANDBOX_AWAIT_S(1, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(3, rb_num2int, sb()->ref<VALUE>(argv, 2));
                        if (argc >= 4) {
                            SANDBOX_AWAIT_S(0, rb_num2dbl, sb()->ref<VALUE>(argv, 3));
                        }
                    }
                }

                mkxp_retro::audio->bgsPlay(sb()->str(SANDBOX_SLOT(1)), SANDBOX_SLOT(2), SANDBOX_SLOT(3), SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE bgs_stop(VALUE self) {
    mkxp_retro::audio->bgsStop();
    return SANDBOX_NIL;
}

static VALUE bgs_fade(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, value);
                mkxp_retro::audio->bgsFade(SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE bgs_pos(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<double, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(0) = mkxp_retro::audio->bgsPos();
                SANDBOX_AWAIT_S(1, rb_float_new, SANDBOX_SLOT(0));
            }

            return SANDBOX_SLOT(1);
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE me_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(1) = 100;
                SANDBOX_SLOT(2) = 100;

                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    }
                }

                mkxp_retro::audio->mePlay(sb()->str(SANDBOX_SLOT(0)), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE me_stop(VALUE self) {
    mkxp_retro::audio->meStop();
    return SANDBOX_NIL;
}

static VALUE me_fade(VALUE self, VALUE value) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<int32_t> slots;

        VALUE operator()(VALUE self, VALUE value) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_num2int, value);
                mkxp_retro::audio->meFade(SANDBOX_SLOT(0));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self, value);
}

static VALUE se_play(int32_t argc, wasm_ptr_t argv, VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, int32_t, int32_t> slots;

        VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_SLOT(1) = 100;
                SANDBOX_SLOT(2) = 100;

                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &sb()->ref<VALUE>(argv, 0));
                if (argc >= 2) {
                    SANDBOX_AWAIT_S(1, rb_num2int, sb()->ref<VALUE>(argv, 1));
                    if (argc >= 3) {
                        SANDBOX_AWAIT_S(2, rb_num2int, sb()->ref<VALUE>(argv, 2));
                    }
                }

                mkxp_retro::audio->sePlay(sb()->str(SANDBOX_SLOT(0)), SANDBOX_SLOT(1), SANDBOX_SLOT(2));
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(argc, argv, self);
}

static VALUE se_stop(VALUE self) {
    mkxp_retro::audio->seStop();
    return SANDBOX_NIL;
}

static VALUE setup_midi(VALUE self) {
    mkxp_retro::audio->setupMidi();
    return SANDBOX_NIL;
}

static VALUE reset(VALUE self) {
    mkxp_retro::audio->reset();
    return SANDBOX_NIL;
}

void audio_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_R(audio_module, rb_define_module, "Audio");
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_play", (VALUE (*)(ANYARGS))bgm_play, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_stop", (VALUE (*)(ANYARGS))bgm_stop, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_fade", (VALUE (*)(ANYARGS))bgm_fade, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_pos", (VALUE (*)(ANYARGS))bgm_pos, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_volume", (VALUE (*)(ANYARGS))bgm_volume, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgm_set_volume", (VALUE (*)(ANYARGS))bgm_set_volume, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgs_play", (VALUE (*)(ANYARGS))bgs_play, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgs_stop", (VALUE (*)(ANYARGS))bgs_stop, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgs_fade", (VALUE (*)(ANYARGS))bgs_fade, 1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "bgs_pos", (VALUE (*)(ANYARGS))bgs_pos, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "me_play", (VALUE (*)(ANYARGS))me_play, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "me_stop", (VALUE (*)(ANYARGS))me_stop, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "me_fade", (VALUE (*)(ANYARGS))me_fade, 1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "se_play", (VALUE (*)(ANYARGS))se_play, -1);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "se_stop", (VALUE (*)(ANYARGS))se_stop, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "setup_midi", (VALUE (*)(ANYARGS))setup_midi, 0);
        SANDBOX_AWAIT(rb_define_module_function, audio_module, "__reset__", (VALUE (*)(ANYARGS))reset, 0);
    }
}
