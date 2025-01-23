/*
** audio-binding.h
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

#ifndef MKXPZ_SANDBOX_AUDIO_BINDING_H
#define MKXPZ_SANDBOX_AUDIO_BINDING_H

#include "sandbox.h"

namespace mkxp_sandbox {
    SANDBOX_COROUTINE(audio_binding_init,
        static VALUE todo(int32_t argc, wasm_ptr_t argv, VALUE self) {
            return self;
        }

        VALUE module;

        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT_AND_SET(module, rb_define_module, "Audio");
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_play", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_stop", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_pos", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_volume", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgm_set_volume", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_play", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_stop", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "bgs_pos", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_play", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_stop", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "me_fade", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "se_play", (VALUE (*)(ANYARGS))todo, -1);
                SANDBOX_AWAIT(rb_define_module_function, module, "se_stop", (VALUE (*)(ANYARGS))todo, -1);
            }
        }
    )
}

#endif // MKXPZ_SANDBOX_AUDIO_BINDING_H
