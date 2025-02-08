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
#include "binding-util.h"
#include "audio-binding.h"
#include "bitmap-binding.h"
#include "etc-binding.h"
#include "font-binding.h"
#include "graphics-binding.h"
#include "input-binding.h"
#include "plane-binding.h"
#include "sprite-binding.h"
#include "table-binding.h"
#include "tilemap-binding.h"
#include "viewport-binding.h"
#include "window-binding.h"

extern const char module_rpg1[];
extern const char module_rpg2[];
extern const char module_rpg3[];

namespace mkxp_sandbox {
    // Evaluates a script, returning the exception if it encountered an exception or `SANDBOX_UNDEF` otherwise.
    SANDBOX_COROUTINE(eval_script,
        private:
        static VALUE func(VALUE arg) {
            SANDBOX_COROUTINE(coro,
                VALUE string;
                VALUE filename;
                ID id;

                void operator()(VALUE arg) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(string, rb_ary_entry, arg, 0);
                        SANDBOX_AWAIT_AND_SET(filename, rb_ary_entry, arg, 1);
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "eval");
                        SANDBOX_AWAIT(rb_funcall, SANDBOX_NIL, id, 3, string, SANDBOX_NIL, filename);
                    }
                }
            )

            sb()->bind<struct coro>()()(arg);
            return SANDBOX_UNDEF;
        }

        static VALUE rescue(VALUE arg, VALUE exception) {
            SANDBOX_COROUTINE(coro,
                VALUE operator()(VALUE exception) {
                    return exception;
                }
            )

            return sb()->bind<struct coro>()()(exception);
        }

        public:
        VALUE value;

        VALUE operator()(VALUE string, VALUE filename) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(value, rb_class_new_instance, 0, NULL, sb()->rb_cArray());
                SANDBOX_AWAIT(rb_ary_push, value, string);
                SANDBOX_AWAIT(rb_ary_push, value, filename);
                SANDBOX_AWAIT_AND_SET(value, rb_rescue, func, value, rescue, SANDBOX_NIL);
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
            BOOST_ASIO_CORO_REENTER (this) {
                decode_buffer = NULL;

                // Unmarshal the script array
                SANDBOX_AWAIT_AND_SET(value, rb_file_open, "/mkxp-retro-game/Data/Scripts.rxdata", "rb"); // TODO: handle VX/VXA games
                SANDBOX_AWAIT_AND_SET(scripts, rb_marshal_load, value);
                SANDBOX_AWAIT(rb_io_close, value);

                // Assign it to the "$RGSS_SCRIPTS" global variable
                SANDBOX_AWAIT(rb_gv_set, "$RGSS_SCRIPTS", scripts);

                SANDBOX_AWAIT_AND_SET(script_count, get_length, scripts);
                decode_buffer_len = 0x1000;
                if ((decode_buffer = (unsigned char *)std::malloc(decode_buffer_len)) == NULL) {
                    throw SandboxOutOfMemoryException();
                }

                for (i = 0; i < script_count; ++i) {
                    // Skip this script object if it's not an array
                    SANDBOX_AWAIT_AND_SET(script, rb_ary_entry, scripts, i);
                    SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, script, sb()->rb_cArray());
                    if (value != SANDBOX_TRUE) {
                        continue;
                    }

                    // Get the name of the script and the zlib-compressed script contents
                    SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, script, 1);
                    SANDBOX_AWAIT_AND_SET(script_name, rb_string_value_cstr, &value);
                    SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, script, 2);
                    SANDBOX_AWAIT_AND_SET(script_string_len, get_length, value);
                    SANDBOX_AWAIT_AND_SET(script_string, rb_string_value_ptr, &value);

                    // Decompress the script contents
                    {
                        int zlib_result = Z_OK;
                        unsigned long buffer_len = decode_buffer_len - 1;
                        while (true) {
                            zlib_result = uncompress(
                                decode_buffer,
                                &buffer_len,
                                (unsigned char *)(**sb() + script_string),
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
                            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Error decoding script %zu: '%s'\n", i, **sb() + script_name);
                            break;
                        }
                    }

                    SANDBOX_AWAIT_AND_SET(value, rb_utf8_str_new_cstr, (const char *)decode_buffer);
                    SANDBOX_AWAIT(rb_ary_store, script, 3, value);
                }

                std::free(decode_buffer);
                decode_buffer = NULL;

                // TODO: preload scripts

                while (true) {
                    for (i = 0; i < script_count; ++i) {
                        // Skip this script object if it's not an array
                        SANDBOX_AWAIT_AND_SET(script, rb_ary_entry, scripts, i);
                        SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, script, sb()->rb_cArray());
                        if (value != SANDBOX_TRUE) {
                            continue;
                        }

                        SANDBOX_AWAIT_AND_SET(script_filename_value, rb_ary_entry, script, 1);
                        SANDBOX_AWAIT_AND_SET(script_string_value, rb_ary_entry, script, 3);
                        SANDBOX_AWAIT_AND_SET(value, eval_script, script_string_value, script_filename_value);
                        if (value != SANDBOX_UNDEF) {
                            SANDBOX_AWAIT(log_backtrace, value);
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

    SANDBOX_COROUTINE(sandbox_binding_init,
        static VALUE load_data(VALUE self, VALUE path) {
            SANDBOX_COROUTINE(coro,
                VALUE value;
                VALUE file;
                wasm_ptr_t ptr;
                std::string path_str;

                VALUE operator()(VALUE path) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(ptr, rb_string_value_cstr, &path);
                        path_str = std::string("/mkxp-retro-game/");
                        path_str.append((const char *)(**sb() + ptr));
                        SANDBOX_AWAIT_AND_SET(file, rb_file_open, path_str.c_str(), "rb");
                        SANDBOX_AWAIT_AND_SET(value, rb_marshal_load, file);
                        SANDBOX_AWAIT(rb_io_close, file);
                    }

                    return value;
                }
            )

            return sb()->bind<struct coro>()()(path);
        }

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(table_binding_init);
                SANDBOX_AWAIT(etc_binding_init);
                SANDBOX_AWAIT(font_binding_init);
                SANDBOX_AWAIT(bitmap_binding_init);
                SANDBOX_AWAIT(sprite_binding_init);
                SANDBOX_AWAIT(viewport_binding_init);
                SANDBOX_AWAIT(plane_binding_init);

                // TODO: pick the correct window and tilemap bindings depending on RPG Maker version
                SANDBOX_AWAIT(window_binding_init);
                SANDBOX_AWAIT(tilemap_binding_init);

                SANDBOX_AWAIT(input_binding_init);
                SANDBOX_AWAIT(audio_binding_init);
                SANDBOX_AWAIT(graphics_binding_init);

                SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "load_data", (VALUE (*)(ANYARGS))load_data, 1);

                // TODO: pick the correct module to load depending on RPG Maker version
                SANDBOX_AWAIT(rb_eval_string, module_rpg1);

                SANDBOX_AWAIT(run_rmxp_scripts);
            }
        }
    )
}

#endif // MKXPZ_BINDING_SANDBOX_H
