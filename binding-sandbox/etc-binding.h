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
            SANDBOX_DEF_ALLOC_WITH_INIT(color_type, new Color())
            SANDBOX_DEF_DFREE(Color)
            SANDBOX_DEF_LOAD(Color)

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    color_type = sb()->rb_data_type("Color", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Color", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                }
            }
        )

        SANDBOX_COROUTINE(tone_binding_init,
            SANDBOX_DEF_ALLOC_WITH_INIT(tone_type, new Tone())
            SANDBOX_DEF_DFREE(Tone)
            SANDBOX_DEF_LOAD(Tone)

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    tone_type = sb()->rb_data_type("Tone", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Tone", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                }
            }
        )

        SANDBOX_COROUTINE(rect_binding_init,
            SANDBOX_DEF_ALLOC_WITH_INIT(rect_type, new Rect())
            SANDBOX_DEF_DFREE(Rect)
            SANDBOX_DEF_LOAD(Rect)

            VALUE klass;

            void operator()() {
                BOOST_ASIO_CORO_REENTER (this) {
                    rect_type = sb()->rb_data_type("Rect", NULL, dfree, NULL, NULL, 0, 0, 0);
                    SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Rect", sb()->rb_cObject());
                    SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                    SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
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
