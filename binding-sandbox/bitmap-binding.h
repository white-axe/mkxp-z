/*
** bitmap-binding.h
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

#ifndef MKXPZ_SANDBOX_BITMAP_BINDING_H
#define MKXPZ_SANDBOX_BITMAP_BINDING_H

#include "sandbox.h"
#include "binding-util.h"
#include "bitmap.h"

namespace mkxp_sandbox {
    static struct mkxp_sandbox::bindings::rb_data_type bitmap_type;

    SANDBOX_COROUTINE(bitmap_binding_init,
        SANDBOX_DEF_ALLOC(bitmap_type)
        SANDBOX_DEF_DFREE(Bitmap)

        VALUE klass;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                bitmap_type = sb()->rb_data_type("Bitmap", NULL, dfree, NULL, NULL, 0, 0, 0);
                SANDBOX_AWAIT_AND_SET(klass, rb_define_class, "Bitmap", sb()->rb_cObject());
                SANDBOX_AWAIT(rb_define_alloc_func, klass, alloc);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_BITMAP_BINDING_H
