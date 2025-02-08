/*
** table-binding.h
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

#ifndef MKXPZ_SANDBOX_TABLE_BINDING_H
#define MKXPZ_SANDBOX_TABLE_BINDING_H

#include <algorithm>
#include "sandbox.h"
#include "table.h"
#include "binding-util.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type table_type;

    SANDBOX_COROUTINE(table_binding_init,
        SANDBOX_DEF_ALLOC_WITH_INIT(table_type, new Table(0, 0, 0))
        SANDBOX_DEF_DFREE(Table)
        SANDBOX_DEF_LOAD(Table)

        static VALUE get(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Table *table;
                VALUE value;
                int32_t x;
                int32_t y;
                int32_t z;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        table = get_private_data<Table>(self);
                        y = z = 0;

                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(z, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                // TODO: throw error if too many arguments
                            }
                        }

                        if (x < 0 || x >= table->xSize() || y < 0 || y >= table->ySize() || z < 0 || z >= table->zSize()) {
                            value = SANDBOX_NIL;
                        } else {
                            SANDBOX_AWAIT_AND_SET(value, rb_ll2inum, table->get(x, y, z));
                        }
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE set(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Table *table;
                int32_t x;
                int32_t y;
                int32_t z;
                int16_t v;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        table = get_private_data<Table>(self);
                        y = z = 0;

                        // TODO: throw error if too few arguments

                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        if (argc >= 3) {
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            if (argc >= 4) {
                                SANDBOX_AWAIT_AND_SET(z, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                            }
                        }

                        SANDBOX_AWAIT_AND_SET(v, rb_num2int, ((VALUE *)(**sb() + argv))[std::min(argc, 4) - 1]);
                        table->set(v, x, y, z);

                        return ((VALUE *)(**sb() + argv))[std::min(argc, 4) - 1];
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                table_type = sb()->rb_data_type("Table", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Table", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
                SANDBOX_AWAIT(rb_define_singleton_method, klass, "_load", (VALUE (*)(ANYARGS))load, 1);
                SANDBOX_AWAIT(rb_define_method, klass, "[]", (VALUE (*)(ANYARGS))get, -1);
                SANDBOX_AWAIT(rb_define_method, klass, "[]=", (VALUE (*)(ANYARGS))set, -1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_TABLE_BINDING_H
