/*
** binding-sandbox.cpp
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

#include "binding-sandbox.h"
#include <zlib.h>
#include "sharedstate.h"
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
#include "tilemapvx-binding.h"
#include "viewport-binding.h"
#include "window-binding.h"
#include "windowvx-binding.h"

using namespace mkxp_sandbox;

extern const char module_rpg1[];
extern const char module_rpg2[];
extern const char module_rpg3[];

struct eval_script : boost::asio::coroutine {
    void operator()(VALUE string, VALUE filename) {
        ID id;

        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_AND_SET(id, rb_intern, "eval");
            SANDBOX_AWAIT(rb_funcall, SANDBOX_NIL, id, 3, string, SANDBOX_NIL, filename);
        }
    }
};

// Runs the game scripts.
struct run_rmxp_scripts : boost::asio::coroutine {
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
            if (rgssVer == 1) {
                SANDBOX_AWAIT_AND_SET(value, rb_file_open, "Data/Scripts.rxdata", "rb");
            } else if (rgssVer == 2) {
                SANDBOX_AWAIT_AND_SET(value, rb_file_open, "Data/Scripts.rvdata", "rb");
            } else if (rgssVer == 3) {
                SANDBOX_AWAIT_AND_SET(value, rb_file_open, "Data/Scripts.rvdata2", "rb");
            } else {
                std::abort();
            }
            SANDBOX_AWAIT_AND_SET(scripts, rb_marshal_load, value);
            SANDBOX_AWAIT(rb_io_close, value);

            // Assign it to the "$RGSS_SCRIPTS" global variable
            SANDBOX_AWAIT(rb_gv_set, "$RGSS_SCRIPTS", scripts);

            SANDBOX_AWAIT_AND_SET(script_count, get_length, scripts);
            decode_buffer_len = 0x1000;
            if ((decode_buffer = (unsigned char *)std::malloc(decode_buffer_len)) == NULL) {
                throw std::bad_alloc();
            }

            for (i = 0; i < script_count; ++i) {
                // Skip this script object if it's not an array
                SANDBOX_AWAIT_AND_SET(script, rb_ary_entry, scripts, i);
                SANDBOX_AWAIT_AND_SET(value, rb_obj_is_kind_of, script, sb()->rb_cArray());
                if (!SANDBOX_VALUE_TO_BOOL(value)) {
                    continue;
                }

                // Get the name of the script and the zlib-compressed script contents
                SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, script, 1);
                SANDBOX_AWAIT_AND_SET(script_name, rb_string_value_cstr, &value);
                SANDBOX_AWAIT_AND_SET(value, rb_ary_entry, script, 2);
                SANDBOX_AWAIT_AND_SET(script_string_len, get_bytesize, value);
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
                            throw std::bad_alloc();
                        }
                        decode_buffer_len = new_decode_buffer_len;
                        unsigned char *new_decode_buffer = (unsigned char *)std::realloc(decode_buffer, decode_buffer_len);
                        if (new_decode_buffer == NULL) {
                            throw std::bad_alloc();
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
                    SANDBOX_AWAIT(eval_script, script_string_value, script_filename_value);
                }
            }
        }
    }

    ~run_rmxp_scripts() {
        if (decode_buffer != NULL) {
            std::free(decode_buffer);
        }
    }
};

static VALUE load_data(VALUE self, VALUE path) {
    struct coro : boost::asio::coroutine {
        VALUE value;
        VALUE file;
        wasm_ptr_t ptr;

        VALUE operator()(VALUE path) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(ptr, rb_string_value_cstr, &path);
                SANDBOX_AWAIT_AND_SET(file, rb_file_open, (const char *)(**sb() + ptr), "rb");
                SANDBOX_AWAIT_AND_SET(value, rb_marshal_load, file);
                SANDBOX_AWAIT(rb_io_close, file);
            }

            return value;
        }
    };

    return sb()->bind<struct coro>()()(path);
}

static VALUE rgss_main(VALUE self) {
    struct coro : boost::asio::coroutine {
        static VALUE func(VALUE block) {
            struct coro : boost::asio::coroutine {
                ID id;

                VALUE operator()(VALUE block) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_AND_SET(id, rb_intern, "call");
                        SANDBOX_AWAIT(rb_funcall, block, id, 0);
                    }

                    return SANDBOX_UNDEF;
                }
            };

            return sb()->bind<struct coro>()()(block);
        }

        static VALUE rescue(VALUE arg, VALUE exception) {
            return exception;
        }

        VALUE block;
        VALUE exception;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(block, rb_block_proc);
                SANDBOX_AWAIT_AND_SET(exception, rb_rescue2, func, block, rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
                if (exception != SANDBOX_UNDEF) {
                    // TODO: handle reset
                    SANDBOX_AWAIT(rb_exc_raise, exception);
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE rgss_stop(VALUE self) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                while (true) {
                    SANDBOX_YIELD;
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

void sandbox_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(table_binding_init);
        SANDBOX_AWAIT(etc_binding_init);
        SANDBOX_AWAIT(font_binding_init);
        SANDBOX_AWAIT(bitmap_binding_init);
        SANDBOX_AWAIT(sprite_binding_init);
        SANDBOX_AWAIT(viewport_binding_init);
        SANDBOX_AWAIT(plane_binding_init);

        if (rgssVer == 1) {
            SANDBOX_AWAIT(window_binding_init);
            SANDBOX_AWAIT(tilemap_binding_init);
        } else {
            SANDBOX_AWAIT(windowvx_binding_init);
            SANDBOX_AWAIT(tilemapvx_binding_init);
        }

        SANDBOX_AWAIT(input_binding_init);
        SANDBOX_AWAIT(audio_binding_init);
        SANDBOX_AWAIT(graphics_binding_init);

        SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "load_data", (VALUE (*)(ANYARGS))load_data, 1);

        if (rgssVer >= 3) {
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "rgss_main", (VALUE (*)(ANYARGS))rgss_main, 0);
            SANDBOX_AWAIT(rb_define_module_function, sb()->rb_mKernel(), "rgss_stop", (VALUE (*)(ANYARGS))rgss_stop, 0);
        }

        if (rgssVer == 1) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg1);
        } else if (rgssVer == 2) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg2);
        } else if (rgssVer == 3) {
            SANDBOX_AWAIT(rb_eval_string, module_rpg3);
        } else {
            std::abort();
        }

        SANDBOX_AWAIT(run_rmxp_scripts);
    }
}
