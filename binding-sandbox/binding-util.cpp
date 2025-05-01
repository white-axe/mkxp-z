/*
** binding-util.cpp
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

#include "binding-util.h"

using namespace mkxp_sandbox;

void mkxp_sandbox::set_private_data(VALUE obj, void *ptr) {
    /* RGSS's behavior is to just leak memory if a disposable is reinitialized,
     * with the original disposable being left permanently instantiated,
     * but that's (1) bad, and (2) would currently cause memory access issues
     * when things like a sprite's src_rect inevitably get GC'd, so we're not
     * copying that. */

    wasm_ptr_t data = sb()->rtypeddata_data(obj);

    // Free the old value if it already exists (initialize called twice?)
    if (*(wasm_ptr_t *)(**sb() + data) != 0 && *(void **)(**sb() + *(wasm_ptr_t *)(**sb() + data)) != ptr) {
        sb()->rtypeddata_dfree(obj, *(wasm_ptr_t *)(**sb() + data));
        *(wasm_ptr_t *)(**sb() + data) = 0;
    }

    if (*(wasm_ptr_t *)(**sb() + data) == 0) {
        wasm_ptr_t buf = sb()->sandbox_malloc(sizeof(void *));
        *(void **)(**sb() + buf) = ptr;
        *(wasm_ptr_t *)(**sb() + data) = buf;
    }
}

wasm_size_t get_length::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(id, rb_intern, "length");
        SANDBOX_AWAIT_AND_SET(length_value, rb_funcall, obj, id, 0);
        SANDBOX_AWAIT_AND_SET(result, rb_num2ulong, length_value);
    }

    return result;
}

wasm_size_t get_bytesize::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(id, rb_intern, "bytesize");
        SANDBOX_AWAIT_AND_SET(length_value, rb_funcall, obj, id, 0);
        SANDBOX_AWAIT_AND_SET(result, rb_num2ulong, length_value);
    }

    return result;
}

VALUE wrap_property::operator()(VALUE self, void *ptr, const char *iv, VALUE klass) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_AND_SET(obj, rb_obj_alloc, klass);
        set_private_data(obj, ptr);
        SANDBOX_AWAIT(rb_iv_set, self, iv, obj);
    }

    return obj;
}

void log_backtrace::operator()(VALUE exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(rb_p, exception);
        SANDBOX_AWAIT_AND_SET(id, rb_intern, "backtrace");
        SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, exception, id, 0);
        SANDBOX_AWAIT_AND_SET(id, rb_intern, "join");
        SANDBOX_AWAIT_AND_SET(separator, rb_str_new_cstr, "\n\t");
        SANDBOX_AWAIT_AND_SET(backtrace, rb_funcall, backtrace, id, 1, separator);
        SANDBOX_AWAIT_AND_SET(backtrace_str, rb_string_value_cstr, &backtrace);
        mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", **sb() + backtrace_str);
    }
}
