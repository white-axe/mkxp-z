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

/* This file contains bindings that expose low-level functionality of the Ruby VM to the outside of the sandbox it's running in. They can be called from sandbox.cpp. */

#ifndef SANDBOX_EXTRA_RUBY_BINDINGS_H
#define SANDBOX_EXTRA_RUBY_BINDINGS_H

#include <stdbool.h>
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

/* Ruby's `rb_`/`ruby_` functions may return early before they're actually finished running.
 * You can use `mkxp_sandbox_complete()` to check if the most recent call to a `rb_`/`ruby_` function finished.
 * If `mkxp_sandbox_complete()` returns false, the `rb_`/`ruby_` function is not done executing yet and needs to be called again with the same arguments. */
MKXP_SANDBOX_API bool mkxp_sandbox_complete(void) {
    extern void *rb_asyncify_unwind_buf; /* Defined in wasm/setjmp.c in Ruby source code */
    return rb_asyncify_unwind_buf == NULL;
}

/* This function drives Ruby's asynchronous runtime. It's based on the `rb_wasm_rt_start()` function from wasm/runtime.c in the Ruby source code.
 * After calling any function that starts with `rb_` or `ruby_` other than `ruby_sysinit()`, you need to call `mkxp_sandbox_yield()`.
 * If `mkxp_sandbox_yield()` returns false, you may proceed as usual.
 * However, if it returns true, then you need to call the `rb_`/`ruby_` function again with the same arguments
 * and then call `mkxp_sandbox_yield()` again, and repeat until `mkxp_sandbox_yield()` returns false. */
MKXP_SANDBOX_API bool mkxp_sandbox_yield(void) {
    static void (*fiber_entry_point)(void *, void *) = NULL;
    static bool new_fiber_started = false;
    static void *arg0;
    static void *arg1;

    void *asyncify_buf;
    bool unwound = false;

    while (1) {
        if (unwound) {
            if (fiber_entry_point != NULL) {
                fiber_entry_point(arg0, arg1);
            } else {
                return true;
            }
        } else {
            unwound = true;
        }

        if (mkxp_sandbox_complete()) {
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

        asyncify_buf = rb_wasm_handle_fiber_unwind(&fiber_entry_point, &arg0, &arg1, &new_fiber_started);
        if (asyncify_buf != NULL) {
            asyncify_start_rewind(asyncify_buf);
            continue;
        } else if (new_fiber_started) {
            continue;
        }

        break;
    }

    fiber_entry_point = NULL;
    new_fiber_started = false;
    return false;
}

#endif /* SANDBOX_EXTRA_RUBY_BINDINGS_H */
