/*
** extra-ruby-bindings.h
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

/* This file contains bindings that expose low-level functionality of the Ruby VM to the outside of the sandbox it's running in. They are used by sandbox-bindgen. */

#ifndef SANDBOX_RUBY_BINDINGS_H
#define SANDBOX_RUBY_BINDINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "wasm/asyncify.h"
#include "wasm/fiber.h"
#include "wasm/machine.h"
#include "wasm/setjmp.h"

#define MKXP_SANDBOX_API __attribute__((__visibility__("default")))

/* This function should be called immediately after initializing the sandbox to perform initialization, before calling any other functions. */
MKXP_SANDBOX_API void mkxp_sandbox_init(void) {
    void __wasm_call_ctors(void); /* Defined by wasi-libc from the WASI SDK */
    __wasm_call_ctors();
}

/* This function should be called immediately before deinitializing the sandbox. */
MKXP_SANDBOX_API void mkxp_sandbox_deinit(void) {
    void __wasm_call_dtors(void); /* Defined by wasi-libc from the WASI SDK */
    __wasm_call_dtors();
}

/* Exposes the `malloc()` function. */
MKXP_SANDBOX_API void *mkxp_sandbox_malloc(size_t size) {
    return malloc(size);
}

/* Exposes the `free()` function. */
MKXP_SANDBOX_API void mkxp_sandbox_free(void *ptr) {
    free(ptr);
}

/* The offset of the `data` field within a `struct RTypedData`. */
MKXP_SANDBOX_API size_t mkxp_sandbox_rtypeddata_data_offset = offsetof(struct RTypedData, data);

/* Calls the `dmark()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dmark(struct RTypedData *data, void *ptr) {
    if (data->type->function.dmark != NULL) {
        data->type->function.dmark(ptr);
    }
}

/* Calls the `dfree()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dfree(struct RTypedData *data, void *ptr) {
    if (data->type->function.dfree != NULL) {
        data->type->function.dfree(ptr);
    }
}

/* Calls the `dsize()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API size_t mkxp_sandbox_rtypeddata_dsize(struct RTypedData *data, const void *ptr) {
    if (data->type->function.dsize != NULL) {
        return data->type->function.dsize(ptr);
    } else {
        return 0;
    }
}

/* Calls the `dcompact()` function from a `struct RTypedData *` on a given memory location. */
MKXP_SANDBOX_API void mkxp_sandbox_rtypeddata_dcompact(struct RTypedData *data, void *ptr) {
    if (data->type->function.dcompact != NULL) {
        data->type->function.dcompact(ptr);
    }
}

MKXP_SANDBOX_API void (*mkxp_sandbox_fiber_entry_point)(void *, void *) = NULL;
MKXP_SANDBOX_API void *mkxp_sandbox_fiber_arg0 = NULL;
MKXP_SANDBOX_API void *mkxp_sandbox_fiber_arg1 = NULL;

/* This function drives Ruby's asynchronous runtime. It's based on the `rb_wasm_rt_start()` function from wasm/runtime.c in the Ruby source code.
 * After calling any function that starts with `rb_` or `ruby_` other than `ruby_sysinit()`, you need to call `mkxp_sandbox_yield()`.
 * If `mkxp_sandbox_yield()` returns false, you may proceed as usual.
 * However, if it returns true, then you need to call the `rb_`/`ruby_` function again with the same arguments
 * and then call `mkxp_sandbox_yield()` again, and repeat until `mkxp_sandbox_yield()` returns false. */
MKXP_SANDBOX_API bool mkxp_sandbox_yield(void) {
    static bool new_fiber_started = false;

    void *asyncify_buf;
    bool unwound = false;

    extern void *rb_asyncify_unwind_buf; /* Defined in wasm/setjmp.c in Ruby source code */

    while (1) {
        if (unwound) {
            if (mkxp_sandbox_fiber_entry_point != NULL) {
                mkxp_sandbox_fiber_entry_point(mkxp_sandbox_fiber_arg0, mkxp_sandbox_fiber_arg1);
            } else {
                return true;
            }
        } else {
            unwound = true;
        }

        if (rb_asyncify_unwind_buf == NULL) {
            break;
        }

        asyncify_stop_unwind();

        if ((asyncify_buf = rb_wasm_handle_jmp_unwind()) != NULL) {
            asyncify_start_rewind(asyncify_buf);
            continue;
        }
        if ((asyncify_buf = rb_wasm_handle_scan_unwind()) != NULL) {
            asyncify_start_rewind(asyncify_buf);
            continue;
        }

        asyncify_buf = rb_wasm_handle_fiber_unwind(&mkxp_sandbox_fiber_entry_point, &mkxp_sandbox_fiber_arg0, &mkxp_sandbox_fiber_arg1, &new_fiber_started);
        if (asyncify_buf != NULL) {
            asyncify_start_rewind(asyncify_buf);
            continue;
        } else if (new_fiber_started) {
            continue;
        }

        break;
    }

    mkxp_sandbox_fiber_entry_point = NULL;
    new_fiber_started = false;
    return false;
}

#endif /* SANDBOX_RUBY_BINDINGS_H */
