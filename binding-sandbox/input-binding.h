/*
** input-binding.h
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

#ifndef MKXPZ_SANDBOX_INPUT_BINDING_H
#define MKXPZ_SANDBOX_INPUT_BINDING_H

#include "sandbox.h"
#include "core.h"
#include "input.h"

namespace mkxp_sandbox {
    struct {
        const char *str;
        Input::ButtonCode val;
    } static codes[] = {
        {"DOWN", Input::Down},
        {"LEFT", Input::Left},
        {"RIGHT", Input::Right},
        {"UP", Input::Up},
        {"C", Input::C},
        {"Z", Input::Z},
        {"A", Input::A},
        {"B", Input::B},
        {"X", Input::X},
        {"Y", Input::Y},
        {"L", Input::L},
        {"R", Input::R},
        {"SHIFT", Input::Shift},
        {"CTRL", Input::Ctrl},
        {"ALT", Input::Alt},
        {"F5", Input::F5},
        {"F6", Input::F6},
        {"F7", Input::F7},
        {"F8", Input::F8},
        {"F9", Input::F9},
        {"MOUSELEFT", Input::MouseLeft},
        {"MOUSEMIDDLE", Input::MouseMiddle},
        {"MOUSERIGHT", Input::MouseRight},
        {"MOUSEX1", Input::MouseX1},
        {"MOUSEX2", Input::MouseX2},
    };

    SANDBOX_COROUTINE(input_binding_init,
        static VALUE delta(VALUE self) {
            return sb()->bind<struct rb_float_new>()()(mkxp_retro::input->getDelta());
        }

        static VALUE update(VALUE self) {
            mkxp_retro::input->update();
            return SANDBOX_NIL;
        }

        static VALUE press(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isPressed(button) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE trigger(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isTriggered(button) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE repeat(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isRepeated(button) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE release(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isReleased(button) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE count(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;
                int32_t count;
                VALUE value;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        count = mkxp_retro::input->count(button);
                        SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, count);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE time(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;
                double time;
                VALUE value;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        time = mkxp_retro::input->repeatTime(button);
                        SANDBOX_AWAIT_AND_SET(value, rb_float_new, time);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE pressex(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isPressedEx(button, false) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE triggerex(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isTriggeredEx(button, false) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE repeatex(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isRepeatedEx(button, false) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE releaseex(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        return mkxp_retro::input->isReleasedEx(button, false) ? SANDBOX_TRUE : SANDBOX_FALSE;
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE repeatcount(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;
                int32_t count;
                VALUE value;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        count = mkxp_retro::input->repeatcount(button, false);
                        SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, count);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE timeex(VALUE self, VALUE code) {
            SANDBOX_COROUTINE(coro,
                int32_t button;
                double time;
                VALUE value;

                VALUE operator()(VALUE self, VALUE code) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(button, rb_num2int, code);
                        time = mkxp_retro::input->repeatTimeEx(button, false);
                        SANDBOX_AWAIT_AND_SET(value, rb_float_new, time);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, code);
        }

        static VALUE dir4(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir4Value());
        }

        static VALUE dir8(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir8Value());
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        static VALUE todo_bool(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_FALSE;
        }

        static VALUE todo_number(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(0);
        }

        VALUE module;
        VALUE button_val;
        size_t i;
        ID id;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Input");
                SANDBOX_AWAIT(rb_define_module_function, module, "delta", (VALUE (*)(ANYARGS))delta, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "update", (VALUE (*)(ANYARGS))update, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "press?", (VALUE (*)(ANYARGS))press, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "trigger?", (VALUE (*)(ANYARGS))trigger, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeat?", (VALUE (*)(ANYARGS))repeat, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "release?", (VALUE (*)(ANYARGS))release, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "count", (VALUE (*)(ANYARGS))count, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "time?", (VALUE (*)(ANYARGS))time, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "pressex?", (VALUE (*)(ANYARGS))pressex, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "triggerex?", (VALUE (*)(ANYARGS))triggerex, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeatex?", (VALUE (*)(ANYARGS))repeatex, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "releaseex?", (VALUE (*)(ANYARGS))releaseex, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "repeatcount", (VALUE (*)(ANYARGS))repeatcount, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "timeex?", (VALUE (*)(ANYARGS))timeex, 1);
                SANDBOX_AWAIT(rb_define_module_function, module, "dir4", (VALUE (*)(ANYARGS))dir4, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "dir8", (VALUE (*)(ANYARGS))dir8, 0);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_x", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_y", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "scroll_v", (VALUE (*)(ANYARGS))todo_number, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_in_window", (VALUE (*)(ANYARGS))todo_bool, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "mouse_in_window?", (VALUE (*)(ANYARGS))todo_bool, -1);

                for (i = 0; i < sizeof(codes) / sizeof(*codes); ++i) {
                    SANDBOX_AWAIT_AND_SET(id, rb_intern, codes[i].str);
                    SANDBOX_AWAIT_AND_SET(button_val, rb_ll2inum, codes[i].val);
                    SANDBOX_AWAIT(rb_const_set, module, id, button_val);
                }
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_INPUT_BINDING_H
