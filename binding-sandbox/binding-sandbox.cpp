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
#include "core.h"
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
    typedef decl_slots<ID> slots;

    void operator()(VALUE string, VALUE filename) {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(0, rb_intern, "eval");
            SANDBOX_AWAIT(rb_funcall, SANDBOX_NIL, SANDBOX_SLOT(0), 3, string, SANDBOX_NIL, filename);
        }
    }
};

// Runs the game scripts.
struct run_rmxp_scripts : boost::asio::coroutine {
    typedef decl_slots<VALUE, VALUE, wasm_size_t, wasm_size_t, VALUE, wasm_ptr_t, wasm_ptr_t, wasm_size_t, wasm_size_t, VALUE, VALUE> slots;
    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            // Unmarshal the script array into slot 1
            if (rgssVer == 1) {
                SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rxdata", "rb");
            } else if (rgssVer == 2) {
                SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rvdata", "rb");
            } else if (rgssVer == 3) {
                SANDBOX_AWAIT_S(0, rb_file_open, "Data/Scripts.rvdata2", "rb");
            } else {
                std::abort();
            }
            SANDBOX_AWAIT_S(1, rb_marshal_load, SANDBOX_SLOT(0));
            SANDBOX_AWAIT(rb_io_close, SANDBOX_SLOT(0));

            // Assign it to the "$RGSS_SCRIPTS" global variable
            SANDBOX_AWAIT(rb_gv_set, "$RGSS_SCRIPTS", SANDBOX_SLOT(1));

            // Get the number of scripts into slot 2
            SANDBOX_AWAIT_S(2, get_length, SANDBOX_SLOT(1));

            sb().script_decode_buffer.resize(0x1000);

            for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(3)) {
                // Get the script into slot 4
                SANDBOX_AWAIT_S(4, rb_ary_entry, SANDBOX_SLOT(1), SANDBOX_SLOT(3));
                // Skip this script object if it's not an array
                SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, SANDBOX_SLOT(4), sb()->rb_cArray());
                if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                    continue;
                }

                // Get the name of the script, the zlib-compressed script contents of the script and the length of said contents into slots 5, 6 and 7, respectively
                SANDBOX_AWAIT_S(0, rb_ary_entry, SANDBOX_SLOT(4), 1);
                SANDBOX_AWAIT_S(5, rb_string_value_cstr, &SANDBOX_SLOT(0));
                SANDBOX_AWAIT_S(0, rb_ary_entry, SANDBOX_SLOT(4), 2);
                SANDBOX_AWAIT_S(6, rb_string_value_ptr, &SANDBOX_SLOT(0));
                SANDBOX_AWAIT_S(7, get_bytesize, SANDBOX_SLOT(0));

                // Decompress the script contents
                {
                    int zlib_result = Z_OK;
                    unsigned long buffer_len = sb().script_decode_buffer.size() - 1;
                    while (true) {
                        zlib_result = uncompress(
                            sb().script_decode_buffer.data(),
                            &buffer_len,
                            (const unsigned char *)sb()->str(SANDBOX_SLOT(6)),
                            SANDBOX_SLOT(7)
                        );
                        sb().script_decode_buffer[buffer_len] = 0;

                        if (zlib_result != Z_BUF_ERROR) {
                            break;
                        }

                        if (2UL * (unsigned long)sb().script_decode_buffer.size() <= (unsigned long)sb().script_decode_buffer.size()) {
                            MKXPZ_THROW(std::bad_alloc());
                        }
                        sb().script_decode_buffer.resize(2 * sb().script_decode_buffer.size());
                        buffer_len = sb().script_decode_buffer.size() - 1;
                    }

                    if (zlib_result != Z_OK) {
                        mkxp_retro::log_printf(RETRO_LOG_ERROR, "Error decoding script %zu: '%s'\n", SANDBOX_SLOT(3), sb()->str(SANDBOX_SLOT(5)));
                        break;
                    }
                }

                SANDBOX_AWAIT_S(0, rb_utf8_str_new_cstr, (const char *)sb().script_decode_buffer.data());
                SANDBOX_AWAIT(rb_ary_store, SANDBOX_SLOT(4), 3, SANDBOX_SLOT(0));
            }

            sb().script_decode_buffer.clear();

            // TODO: preload scripts

            while (true) {
                for (SANDBOX_SLOT(3) = 0; SANDBOX_SLOT(3) < SANDBOX_SLOT(2); ++SANDBOX_SLOT(3)) {
                    // Get the script into slot 4
                    SANDBOX_AWAIT_S(4, rb_ary_entry, SANDBOX_SLOT(1), SANDBOX_SLOT(3));
                    // Skip this script object if it's not an array
                    SANDBOX_AWAIT_S(0, rb_obj_is_kind_of, SANDBOX_SLOT(4), sb()->rb_cArray());
                    if (!SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                        continue;
                    }

                    SANDBOX_AWAIT_S(9, rb_ary_entry, SANDBOX_SLOT(4), 3);
                    SANDBOX_AWAIT_S(10, rb_ary_entry, SANDBOX_SLOT(4), 1);
                    SANDBOX_AWAIT(eval_script, SANDBOX_SLOT(9), SANDBOX_SLOT(10));
                }

                // Stop unless a reset was requested
                SANDBOX_AWAIT_S(0, rb_gv_get, "$!");
                SANDBOX_AWAIT_S(0, rb_obj_class, SANDBOX_SLOT(0));
                if (SANDBOX_SLOT(0) != reset_class) {
                    break;
                }

                SANDBOX_AWAIT(audio_reset);
                SANDBOX_AWAIT(graphics_reset);
            }
        }
    }

    ~run_rmxp_scripts() {
        sb().script_decode_buffer.clear();
    }
};

static VALUE load_data(VALUE self, VALUE path) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<wasm_ptr_t, VALUE, VALUE> slots;

        VALUE operator()(VALUE path) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_S(0, rb_string_value_cstr, &path);
                SANDBOX_AWAIT_S(1, rb_file_open, sb()->str(SANDBOX_SLOT(0)), "rb");
                SANDBOX_AWAIT_S(2, rb_marshal_load, SANDBOX_SLOT(1));
                SANDBOX_AWAIT(rb_io_close, SANDBOX_SLOT(1));
            }

            return SANDBOX_SLOT(2);
        }
    };

    return sb()->bind<struct coro>()()(path);
}

static VALUE rgss_main(VALUE self) {
    struct coro : boost::asio::coroutine {
        static VALUE func(VALUE block) {
            struct coro : boost::asio::coroutine {
                typedef decl_slots<ID> slots;

                VALUE operator()(VALUE block) {
                    BOOST_ASIO_CORO_REENTER (this) {
                        SANDBOX_AWAIT_S(0, rb_intern, "call");
                        SANDBOX_AWAIT(rb_funcall, block, SANDBOX_SLOT(0), 0);
                    }

                    return SANDBOX_UNDEF;
                }
            };

            return sb()->bind<struct coro>()()(block);
        }

        static VALUE rescue(VALUE arg, VALUE exception) {
            return exception;
        }

        typedef decl_slots<VALUE, VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                while (true) {
                    SANDBOX_AWAIT_S(0, rb_block_proc);
                    SANDBOX_AWAIT_S(1, rb_rescue2, func, SANDBOX_SLOT(0), rescue, SANDBOX_NIL, sb()->rb_eException(), 0);

                    if (SANDBOX_SLOT(1) == SANDBOX_UNDEF) {
                        return SANDBOX_NIL;
                    }

                    SANDBOX_AWAIT_S(0, rb_gv_get, "$!");
                    SANDBOX_AWAIT_S(0, rb_obj_class, SANDBOX_SLOT(0));
                    if (SANDBOX_SLOT(0) == reset_class) {
                        SANDBOX_AWAIT(audio_reset);
                        SANDBOX_AWAIT(graphics_reset);
                    } else {
                        SANDBOX_AWAIT(rb_exc_raise, SANDBOX_SLOT(1));
                    }
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

static VALUE rgss_stop(VALUE self) {
    struct coro : boost::asio::coroutine {
        typedef decl_slots<VALUE> slots;

        VALUE operator()(VALUE self) {
            BOOST_ASIO_CORO_REENTER (this) {
                while (true) {
                    SANDBOX_GUARD_L(SANDBOX_SLOT(0) = shState->graphics().update(sb().e));
                    if (SANDBOX_SLOT(0)) {
                        SANDBOX_YIELD;
                    }
                }
            }

            return SANDBOX_NIL;
        }
    };

    return sb()->bind<struct coro>()()(self);
}

void sandbox_binding_init::operator()() {
    BOOST_ASIO_CORO_REENTER (this) {
        SANDBOX_AWAIT(exception_binding_init);

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
