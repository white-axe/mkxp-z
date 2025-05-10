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
#include "filesystem.h"

using namespace mkxp_sandbox;

void mkxp_sandbox::set_private_data(VALUE obj, void *ptr) {
    /* RGSS's behavior is to just leak memory if a disposable is reinitialized,
     * with the original disposable being left permanently instantiated,
     * but that's (1) bad, and (2) would currently cause memory access issues
     * when things like a sprite's src_rect inevitably get GC'd, so we're not
     * copying that. */

    wasm_ptr_t data = sb()->rtypeddata_data(obj);

    // Free the old value if it already exists (initialize called twice?)
    if (sb()->ref<wasm_ptr_t>(data) != 0 && sb()->ref<void *>(sb()->ref<wasm_ptr_t>(data)) != ptr) {
        sb()->rtypeddata_dfree(obj, sb()->ref<wasm_ptr_t>(data));
        sb()->ref<wasm_ptr_t>(data) = 0;
    }

    if (sb()->ref<wasm_ptr_t>(data) == 0) {
        wasm_ptr_t buf = sb()->sandbox_malloc(sizeof(void *));
        sb()->ref<void *>(buf) = ptr;
        sb()->ref<wasm_ptr_t>(data) = buf;
    }
}

wasm_size_t get_length::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "length");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

wasm_size_t get_bytesize::operator()(VALUE obj) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_intern, "bytesize");
        SANDBOX_AWAIT_S(1, rb_funcall, obj, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(2, rb_num2ulong, SANDBOX_SLOT(1));
    }

    return SANDBOX_SLOT(2);
}

VALUE wrap_property::operator()(VALUE self, void *ptr, const char *iv, VALUE klass) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT_S(0, rb_obj_alloc, klass);
        set_private_data(SANDBOX_SLOT(0), ptr);
        SANDBOX_AWAIT(rb_iv_set, self, iv, SANDBOX_SLOT(0));
    }

    return SANDBOX_SLOT(0);
}

void log_backtrace::operator()(VALUE exception) {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(rb_p, exception);
        SANDBOX_AWAIT_S(0, rb_intern, "backtrace");
        SANDBOX_AWAIT_S(1, rb_funcall, exception, SANDBOX_SLOT(0), 0);
        SANDBOX_AWAIT_S(0, rb_intern, "join");
        SANDBOX_AWAIT_S(2, rb_str_new_cstr, "\n\t");
        SANDBOX_AWAIT_S(1, rb_funcall, SANDBOX_SLOT(1), SANDBOX_SLOT(0), 1, SANDBOX_SLOT(2));
        SANDBOX_AWAIT_S(3, rb_string_value_cstr, &SANDBOX_SLOT(1));
        mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", sb()->str(SANDBOX_SLOT(3)));
    }
}
