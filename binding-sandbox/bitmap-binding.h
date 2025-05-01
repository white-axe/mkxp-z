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

#include "binding-util.h"
#include "bitmap.h"

namespace mkxp_sandbox {
    extern VALUE bitmap_class;

    struct bitmap_init_props : boost::asio::coroutine {
        void operator()(Bitmap *bitmap, VALUE self);
    private:
        VALUE font;
        VALUE hires;
        VALUE hires_font;
    };

    struct bitmap_binding_init : boost::asio::coroutine {
        void operator()();
    };
}

#endif // MKXPZ_SANDBOX_BITMAP_BINDING_H
