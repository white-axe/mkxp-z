/*
** input-binding.cpp
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

#include "input-binding.h"
#include "sharedstate.h"

using namespace mkxp_sandbox;

VALUE mkxp_sandbox::input_module;

static VALUE symhash;

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

struct get_button_arg : boost::asio::coroutine {
    VALUE value;
    int32_t button;

    int32_t operator()(VALUE arg) {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, arg, sb()->rb_cInteger());
            if (SANDBOX_VALUE_TO_BOOL(value)) {
                SANDBOX_AWAIT_AND_SET(button, rb_num2int, arg);
            } else {
                SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, arg, sb()->rb_cSymbol());
                if (SANDBOX_VALUE_TO_BOOL(value) && rgssVer >= 3) {
                    SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, Input::None);
                    SANDBOX_AWAIT_AND_SET(value, rb_hash_lookup2, symhash, arg, value);
                    SANDBOX_AWAIT_AND_SET(button, rb_num2int, value);
                } else {
                    // FIXME: RMXP allows only few more types that
                    // don't make sense (symbols in pre 3, floats)
                    button = 0;
                }
            }
        }

        return button;
    }
};

static VALUE delta(VALUE self) {
    return sb()->bind<struct rb_float_new>()()(mkxp_retro::input->getDelta());
}

static VALUE update(VALUE self) {
    mkxp_retro::input->update();
    return SANDBOX_NIL;
}

static VALUE press(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isPressed(button));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE trigger(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isTriggered(button));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeat(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isRepeated(button));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE release(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isReleased(button));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE count(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;
        int32_t count;
        VALUE value;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                count = mkxp_retro::input->count(button);
                SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, count);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE time_(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;
        double time;
        VALUE value;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                time = mkxp_retro::input->repeatTime(button);
                SANDBOX_AWAIT_AND_SET(value, rb_float_new, time);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE pressex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isPressedEx(button, false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE triggerex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isTriggeredEx(button, false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeatex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isRepeatedEx(button, false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE releaseex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->isReleasedEx(button, false));
            }

            return SANDBOX_UNDEF;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE repeatcount(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;
        int32_t count;
        VALUE value;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                count = mkxp_retro::input->repeatcount(button, false);
                SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, count);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE timeex(VALUE self, VALUE code) {
    struct coro : boost::asio::coroutine {
        int32_t button;
        double time;
        VALUE value;

        VALUE operator()(VALUE self, VALUE code) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(button, get_button_arg, code);
                time = mkxp_retro::input->repeatTimeEx(button, false);
                SANDBOX_AWAIT_AND_SET(value, rb_float_new, time);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(self, code);
}

static VALUE dir4(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir4Value());
}

static VALUE dir8(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->dir8Value());
}

static VALUE mouse_x(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->mouseX());
}

static VALUE mouse_y(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->mouseY());
}

static VALUE scroll_v(VALUE self) {
    return sb()->bind<struct rb_ll2inum>()()(mkxp_retro::input->scrollV());
}

static VALUE mouse_in_window(VALUE self) {
    return SANDBOX_BOOL_TO_VALUE(mkxp_retro::input->mouseInWindow());
}

void input_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(input_module, rb_define_module, "Input");
        SANDBOX_AWAIT(rb_define_module_function, input_module, "delta", (VALUE (*)(ANYARGS))delta, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "update", (VALUE (*)(ANYARGS))update, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "press?", (VALUE (*)(ANYARGS))press, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "trigger?", (VALUE (*)(ANYARGS))trigger, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeat?", (VALUE (*)(ANYARGS))repeat, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "release?", (VALUE (*)(ANYARGS))release, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "count", (VALUE (*)(ANYARGS))count, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "time?", (VALUE (*)(ANYARGS))time_, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "pressex?", (VALUE (*)(ANYARGS))pressex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "triggerex?", (VALUE (*)(ANYARGS))triggerex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeatex?", (VALUE (*)(ANYARGS))repeatex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "releaseex?", (VALUE (*)(ANYARGS))releaseex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "repeatcount", (VALUE (*)(ANYARGS))repeatcount, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "timeex?", (VALUE (*)(ANYARGS))timeex, 1);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "dir4", (VALUE (*)(ANYARGS))dir4, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "dir8", (VALUE (*)(ANYARGS))dir8, 0);

        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_x", (VALUE (*)(ANYARGS))mouse_x, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_y", (VALUE (*)(ANYARGS))mouse_y, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "scroll_v", (VALUE (*)(ANYARGS))scroll_v, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_in_window", (VALUE (*)(ANYARGS))mouse_in_window, 0);
        SANDBOX_AWAIT(rb_define_module_function, input_module, "mouse_in_window?", (VALUE (*)(ANYARGS))mouse_in_window, 0);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT_AND_SET(symhash, rb_hash_new);

            for (i = 0; i < sizeof(codes) / sizeof(*codes); ++i) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, codes[i].str);
                SANDBOX_AWAIT_AND_SET(button_val, rb_ll2inum, codes[i].val);

                /* In RGSS3 all Input::XYZ constants are equal to :XYZ symbols,
                 * to be compatible with the previous convention */
                SANDBOX_AWAIT_AND_SET(id_val, rb_id2sym, id);
                SANDBOX_AWAIT(rb_const_set, input_module, id, id_val);
                SANDBOX_AWAIT(rb_hash_aset, symhash, id_val, button_val);
            }

            SANDBOX_AWAIT(rb_iv_set, input_module, "buttoncodes", symhash);
        } else {
            for (i = 0; i < sizeof(codes) / sizeof(*codes); ++i) {
                SANDBOX_AWAIT_AND_SET(id, rb_intern, codes[i].str);
                SANDBOX_AWAIT_AND_SET(button_val, rb_ll2inum, codes[i].val);
                SANDBOX_AWAIT(rb_const_set, input_module, id, button_val);
            }
        }
    }
}
