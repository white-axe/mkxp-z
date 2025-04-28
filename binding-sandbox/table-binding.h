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
    static VALUE table_class;

    SANDBOX_COROUTINE(table_binding_init,
        SANDBOX_DEF_ALLOC_WITH_INIT(table_type, new Table(0, 0, 0))
        SANDBOX_DEF_DFREE(Table)
        SANDBOX_DEF_LOAD(Table)

        static VALUE initialize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                int32_t x;
                int32_t y;
                int32_t z;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        y = z = 1;

                        // TODO: throw error if too many or too few arguments
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        x = std::max(x, (int32_t)0);
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            y = std::max(y, (int32_t)0);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(z, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                z = std::max(z, (int32_t)0);
                            }
                        }

                        Table *table = get_private_data<Table>(self);
                        if (table != NULL) {
                            table->resize(x, y, z);
                        } else {
                            table = new Table(x, y, z);
                            set_private_data(self, table);
                        }
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE initialize_copy(VALUE self, VALUE value) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE self, VALUE value) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        if (self != value) {
                            SANDBOX_AWAIT(rb_obj_init_copy, self, value);
                            set_private_data(self, new Table(*get_private_data<Table>(value)));
                        }
                    }

                    return self;
                }
            )

            return sb()->bind<struct coro>()()(self, value);
        }

        static VALUE resize(int32_t argc, wasm_ptr_t argv, VALUE self) {
            SANDBOX_COROUTINE(coro,
                Table *table;
                int32_t x;
                int32_t y;
                int32_t z;

                VALUE operator()(int32_t argc, wasm_ptr_t argv, VALUE self) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        y = z = 1;

                        // TODO: throw error if too many or too few arguments
                        SANDBOX_AWAIT_AND_SET(x, rb_num2int, ((VALUE *)(**sb() + argv))[0]);
                        x = std::max(x, (int32_t)0);
                        if (argc >= 2) {
                            SANDBOX_AWAIT_AND_SET(y, rb_num2int, ((VALUE *)(**sb() + argv))[1]);
                            y = std::max(y, (int32_t)0);
                            if (argc >= 3) {
                                SANDBOX_AWAIT_AND_SET(z, rb_num2int, ((VALUE *)(**sb() + argv))[2]);
                                z = std::max(z, (int32_t)0);
                            }
                        }

                        get_private_data<Table>(self)->resize(x, y, z);
                    }

                    return SANDBOX_NIL;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        static VALUE xsize(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->xSize());
        }

        static VALUE ysize(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->ySize());
        }

        static VALUE zsize(VALUE self) {
            return sb()->bind<struct rb_ll2inum>()()(get_private_data<Table>(self)->zSize());
        }

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

                        SANDBOX_AWAIT_AND_SET(v, rb_num2int, ((VALUE *)(**sb() + argv))[std::min(argc, (int32_t)4) - 1]);
                        table->set(v, x, y, z);

                        return ((VALUE *)(**sb() + argv))[std::min(argc, (int32_t)4) - 1];
                    }

                    return SANDBOX_UNDEF;
                }
            )

            return sb()->bind<struct coro>()()(argc, argv, self);
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                table_type = sb()->rb_data_type("Table", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(table_class, rb_define_class, "Table", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, table_class, alloc);
                SANDBOX_AWAIT(rb_define_singleton_method, table_class, "_load", (VALUE (*)(ANYARGS))load, 1);
                SANDBOX_AWAIT(rb_define_method, table_class, "initialize", (VALUE (*)(ANYARGS))initialize, -1);
                SANDBOX_AWAIT(rb_define_method, table_class, "initialize_copy", (VALUE (*)(ANYARGS))initialize_copy, 1);
                SANDBOX_AWAIT(rb_define_method, table_class, "resize", (VALUE (*)(ANYARGS))resize, -1);
                SANDBOX_AWAIT(rb_define_method, table_class, "xsize", (VALUE (*)(ANYARGS))xsize, 0);
                SANDBOX_AWAIT(rb_define_method, table_class, "ysize", (VALUE (*)(ANYARGS))ysize, 0);
                SANDBOX_AWAIT(rb_define_method, table_class, "zsize", (VALUE (*)(ANYARGS))zsize, 0);
                SANDBOX_AWAIT(rb_define_method, table_class, "[]", (VALUE (*)(ANYARGS))get, -1);
                SANDBOX_AWAIT(rb_define_method, table_class, "[]=", (VALUE (*)(ANYARGS))set, -1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_TABLE_BINDING_H
