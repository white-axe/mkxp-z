/*
** binding-sandbox.h
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

#ifndef MKXPZ_BINDING_SANDBOX_H
#define MKXPZ_BINDING_SANDBOX_H

#include <zlib.h>
#include "binding-sandbox/core.h"
#include "sandbox.h"

namespace mkxp_sandbox {
    // Gets the length of a Ruby object.
    SANDBOX_COROUTINE(length,
        ID id;
        VALUE length_value;
        wasm_size_t result;

        wasm_size_t operator()(VALUE ary) {
            reenter (this) {
                SANDBOX_AWAIT_AND_SET(id, mkxp_sandbox::rb_intern, "length");
                SANDBOX_AWAIT_AND_SET(length_value, mkxp_sandbox::rb_funcall, ary, id, 0);
                SANDBOX_AWAIT_AND_SET(result, mkxp_sandbox::rb_num2ulong, length_value);
            }

            return result;
        }
    )

    // Prints the backtrace of a Ruby exception to the log.
    SANDBOX_COROUTINE(log_backtrace,
        ID id;
        VALUE backtrace;
        VALUE separator;
        wasm_ptr_t backtrace_str;

        void operator()(VALUE exception) {
            reenter (this) {
                SANDBOX_AWAIT(mkxp_sandbox::rb_p, exception);
                SANDBOX_AWAIT_AND_SET(id, mkxp_sandbox::rb_intern, "backtrace");
                SANDBOX_AWAIT_AND_SET(backtrace, mkxp_sandbox::rb_funcall, exception, id, 0);
                SANDBOX_AWAIT_AND_SET(id, mkxp_sandbox::rb_intern, "join");
                SANDBOX_AWAIT_AND_SET(separator, mkxp_sandbox::rb_str_new_cstr, "\n\t");
                SANDBOX_AWAIT_AND_SET(backtrace, mkxp_sandbox::rb_funcall, backtrace, id, 1, separator);
                SANDBOX_AWAIT_AND_SET(backtrace_str, mkxp_sandbox::rb_string_value_cstr, &backtrace);
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "%s\n", *mkxp_sandbox::sandbox->bindings + backtrace_str);
            }
        }
    )

    // Evaluates a script, returning the exception if it encountered an exception or `SANDBOX_UNDEF` otherwise.
    SANDBOX_COROUTINE(eval_script,
        private:
        static VALUE func(void *_, VALUE arg) {
            SANDBOX_COROUTINE(coro,
                VALUE string;
                VALUE filename;
                ID id;

                void operator()(VALUE arg) {
                    reenter (this) {
                        SANDBOX_AWAIT_AND_SET(string, mkxp_sandbox::rb_ary_entry, arg, 0);
                        SANDBOX_AWAIT_AND_SET(filename, mkxp_sandbox::rb_ary_entry, arg, 1);
                        SANDBOX_AWAIT_AND_SET(id, mkxp_sandbox::rb_intern, "eval");
                        SANDBOX_AWAIT(mkxp_sandbox::rb_funcall, SANDBOX_NIL, id, 3, string, SANDBOX_NIL, filename);
                    }
                }
            )

            mkxp_sandbox::sandbox->bindings.bind<struct coro>()()(arg);
            return SANDBOX_UNDEF;
        }

        static VALUE rescue(void *_, VALUE arg, VALUE exception) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE exception) {
                    return exception;
                }
            )

            return mkxp_sandbox::sandbox->bindings.bind<struct coro>()()(exception);
        }

        public:
        VALUE value;

        VALUE operator()(VALUE string, VALUE filename) {
            reenter (this) {
                SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_class_new_instance, 0, NULL, mkxp_sandbox::sandbox->bindings.rb_cArray());
                SANDBOX_AWAIT(mkxp_sandbox::rb_ary_push, value, string);
                SANDBOX_AWAIT(mkxp_sandbox::rb_ary_push, value, filename);
                SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_rescue, func, value, rescue, SANDBOX_NIL);
            }

            return value;
        }
    )

    // Runs the game scripts.
    SANDBOX_COROUTINE(run_rmxp_scripts,
        VALUE value;
        VALUE scripts;
        wasm_size_t script_count;
        unsigned char *decode_buffer;
        unsigned long decode_buffer_len;
        wasm_size_t i;
        VALUE script;
        wasm_ptr_t script_name;
        wasm_ptr_t script_string;
        wasm_size_t script_string_len;
        wasm_size_t len;
        VALUE script_string_value;
        VALUE script_filename_value;

        void operator()() {
            reenter (this) {
                decode_buffer = NULL;

                // Unmarshal the script array
                SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_file_open, "/mkxp-retro-game/Data/Scripts.rxdata", "rb"); // TODO: handle VX/VXA games
                SANDBOX_AWAIT_AND_SET(scripts, mkxp_sandbox::rb_marshal_load, value);
                SANDBOX_AWAIT(mkxp_sandbox::rb_io_close, value);

                // Assign it to the "$RGSS_SCRIPTS" global variable
                SANDBOX_AWAIT(mkxp_sandbox::rb_gv_set, "$RGSS_SCRIPTS", scripts);

                SANDBOX_AWAIT_AND_SET(script_count, mkxp_sandbox::length, scripts);
                decode_buffer_len = 0x1000;
                if ((decode_buffer = (unsigned char *)std::malloc(decode_buffer_len)) == NULL) {
                    throw SandboxOutOfMemoryException();
                }

                for (i = 0; i < script_count; ++i) {
                    // Skip this script object if it's not an array
                    SANDBOX_AWAIT_AND_SET(script, mkxp_sandbox::rb_ary_entry, scripts, i);
                    SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_obj_is_kind_of, script, mkxp_sandbox::sandbox->bindings.rb_cArray());
                    if (value != SANDBOX_TRUE) {
                        continue;
                    }

                    // Get the name of the script and the zlib-compressed script contents
                    SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_ary_entry, script, 1);
                    SANDBOX_AWAIT_AND_SET(script_name, mkxp_sandbox::rb_string_value_cstr, &value);
                    SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_ary_entry, script, 2);
                    SANDBOX_AWAIT_AND_SET(script_string_len, mkxp_sandbox::length, value);
                    SANDBOX_AWAIT_AND_SET(script_string, mkxp_sandbox::rb_string_value_ptr, &value);

                    // Decompress the script contents
                    {
                        int zlib_result = Z_OK;
                        unsigned long buffer_len = decode_buffer_len - 1;
                        while (true) {
                            zlib_result = uncompress(
                                decode_buffer,
                                &buffer_len,
                                (unsigned char *)(*mkxp_sandbox::sandbox->bindings + script_string),
                                script_string_len
                            );
                            decode_buffer[buffer_len] = 0;

                            if (zlib_result != Z_BUF_ERROR) {
                                break;
                            }

                            unsigned long new_decode_buffer_len;
                            if (__builtin_add_overflow(decode_buffer_len, decode_buffer_len, &new_decode_buffer_len)) {
                                throw SandboxOutOfMemoryException();
                            }
                            decode_buffer_len = new_decode_buffer_len;
                            unsigned char *new_decode_buffer = (unsigned char *)std::realloc(decode_buffer, decode_buffer_len);
                            if (new_decode_buffer == NULL) {
                                throw SandboxOutOfMemoryException();
                            }
                            decode_buffer = new_decode_buffer;
                            buffer_len = decode_buffer_len - 1;
                        }

                        if (zlib_result != Z_OK) {
                            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Error decoding script %zu: '%s'\n", i, *mkxp_sandbox::sandbox->bindings + script_name);
                            break;
                        }
                    }

                    SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_utf8_str_new_cstr, (const char *)decode_buffer);
                    SANDBOX_AWAIT(mkxp_sandbox::rb_ary_store, script, 3, value);
                }

                std::free(decode_buffer);
                decode_buffer = NULL;

                // TODO: preload scripts

                while (true) {
                    for (i = 0; i < script_count; ++i) {
                        // Skip this script object if it's not an array
                        SANDBOX_AWAIT_AND_SET(script, mkxp_sandbox::rb_ary_entry, scripts, i);
                        SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::rb_obj_is_kind_of, script, mkxp_sandbox::sandbox->bindings.rb_cArray());
                        if (value != SANDBOX_TRUE) {
                            continue;
                        }

                        SANDBOX_AWAIT_AND_SET(script_filename_value, mkxp_sandbox::rb_ary_entry, script, 1);
                        SANDBOX_AWAIT_AND_SET(script_string_value, mkxp_sandbox::rb_ary_entry, script, 3);
                        SANDBOX_AWAIT_AND_SET(value, mkxp_sandbox::eval_script, script_string_value, script_filename_value);
                        if (value != SANDBOX_UNDEF) {
                            SANDBOX_AWAIT(mkxp_sandbox::log_backtrace, value);
                            break;
                        }
                    }
                    if (value != SANDBOX_UNDEF) {
                        break;
                    }
                }
            }
        }

        ~run_rmxp_scripts() {
            if (decode_buffer != NULL) {
                std::free(decode_buffer);
            }
        }
    )
}

#endif // MKXPZ_BINDING_SANDBOX_H
