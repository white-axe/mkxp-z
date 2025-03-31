/*
** font-binding.h
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

#ifndef MKXPZ_SANDBOX_FONT_BINDING_H
#define MKXPZ_SANDBOX_FONT_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "font.h"
#include "etc.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type font_type;

    SANDBOX_COROUTINE(font_binding_init,
        SANDBOX_DEF_ALLOC(font_type)
        SANDBOX_DEF_DFREE(Font)

        VALUE klass;

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Font *font;
                int32_t size;
                ID id;
                VALUE klass;
                VALUE obj;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (argc == 0) {
                            font = new Font();
                        } else if (argc == 1) {
                            font = new Font(NULL); // TODO: handle names
                        } else {
                            SANDBOX_AWAIT_AND_SET(size, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            font = new Font(NULL, size); // TODO: handle names
                        }
                        set_private_data(self, font);

                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "Color");
                        SANDBOX_AWAIT_AND_SET(klass, rb_const_get, sb()->rb_cObject(), id);
                        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
                        set_private_data(obj, new Color());
                        SANDBOX_AWAIT(rb_iv_set, self, "color", obj);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE get_size(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Font>(self)->getSize());
        }

        static VALUE set_size(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                int32_t size;

                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(size, rb_num2int, value);
                        get_private_data<Font>(self)->setSize(size);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE get_color(VALUE self) {
            return sb()->bind<struct rb_iv_get>()()(self, "color");
        }

        static VALUE set_color(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        get_private_data<Font>(self)->setColor(*get_private_data<Color>(value));
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return SANDBOX_NIL;
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                font_type = sb()->rb_data_type("Font", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Font", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_method, klass, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "name=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "size", (VALUE (*)(ANYARGS))get_size, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "size=", (VALUE (*)(ANYARGS))set_size, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "bold=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "italic=", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "color", (VALUE (*)(ANYARGS))get_color, 0);
                SANDBOX_AWAIT(rb_define_method, klass, "color=", (VALUE (*)(ANYARGS))set_color, 1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_FONT_BINDING_H
