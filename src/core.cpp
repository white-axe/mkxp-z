/*
** core.cpp
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

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <memory>
#include "sandbox/sandbox.h"
#include "core.h"

#define SANDBOX_AWAIT(coroutine, ...) \
    do { \
        { \
            auto frame = sandbox->bindings.bind<struct coroutine>(); \
            frame()(__VA_ARGS__); \
            if (frame().is_complete()) break; \
        } \
        yield; \
    } while (1)

using namespace mkxp_retro;

static void fallback_log(enum retro_log_level level, const char *fmt, ...) {
    std::va_list va;
    va_start(va, fmt);
    std::vfprintf(stderr, fmt, va);
    va_end(va);
}

static uint32_t *frame_buf;
static std::unique_ptr<struct mkxp_sandbox::sandbox> sandbox;
static const char *game_path = NULL;

static VALUE my_cpp_func(w2c_ruby *ruby, int32_t argc, wasm_ptr_t argv, VALUE self) {
    log_printf(RETRO_LOG_INFO, "Hello from Ruby land! my_cpp_func(argc=%d, argv=0x%08x, self=0x%08x)\n", argc, argv, self);
    return self;
}

static bool init_sandbox() {
    struct main : boost::asio::coroutine {
        void operator()() {
            reenter (this) {
                SANDBOX_AWAIT(mkxp_sandbox::rb_eval_string, "puts 'Hello, World!'");

                SANDBOX_AWAIT(mkxp_sandbox::rb_eval_string, "require 'zlib'; p Zlib::Deflate::deflate('hello')");

                SANDBOX_AWAIT(mkxp_sandbox::rb_define_global_function, "my_cpp_func", (VALUE (*)(void *, ANYARGS))my_cpp_func, -1);
                SANDBOX_AWAIT(mkxp_sandbox::rb_eval_string, "my_cpp_func(1, nil, 3, 'this is a string', :symbol, 2)");

                SANDBOX_AWAIT(mkxp_sandbox::rb_eval_string, "p Dir.glob '/mkxp-retro-game/*'");
            }
        }
    };

    sandbox.reset();

    try {
        sandbox.reset(new struct mkxp_sandbox::sandbox(game_path));
        sandbox->run<struct main>();
    } catch (SandboxException) {
        log_printf(RETRO_LOG_ERROR, "Failed to initialize Ruby\n");
        sandbox.reset();
        return false;
    }

    return true;
}

extern "C" RETRO_API void retro_set_environment(retro_environment_t cb) {
    environment = cb;

    struct retro_log_callback log;
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_printf = log.log;
    } else {
        log_printf = fallback_log;
    }

    perf = {
        .get_time_usec = nullptr,
        .get_cpu_features = nullptr,
        .get_perf_counter = nullptr,
        .perf_register = nullptr,
        .perf_start = nullptr,
        .perf_stop = nullptr,
        .perf_log = nullptr,
    };
    cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf);
}

extern "C" RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_refresh = cb;
}

extern "C" RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_sample = cb;
}

extern "C" RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_sample_batch = cb;
}

extern "C" RETRO_API void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll = cb;
}

extern "C" RETRO_API void retro_set_input_state(retro_input_state_t cb) {
    input_state = cb;
}

extern "C" RETRO_API void retro_init() {
    frame_buf = (uint32_t *)std::calloc(640 * 480, sizeof(uint32_t));
}

extern "C" RETRO_API void retro_deinit() {
    std::free(frame_buf);
}

extern "C" RETRO_API unsigned int retro_api_version() {
    return RETRO_API_VERSION;
}

extern "C" RETRO_API void retro_get_system_info(struct retro_system_info *info) {
    std::memset(info, 0, sizeof *info);
    info->library_name = "mkxp-z";
    info->library_version = "rolling";
    info->valid_extensions = "mkxp|mkxpz|json|ini|rxproj|rvproj|rvproj2";
    info->need_fullpath = true;
    info->block_extract = true;
}

extern "C" RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
    std::memset(info, 0, sizeof *info);
    info->timing = {
        .fps = 60.0,
        .sample_rate = 44100.0,
    };
    info->geometry = {
        .base_width = 640,
        .base_height = 480,
        .max_width = 640,
        .max_height = 480,
        .aspect_ratio = 640.0f / 480.0f,
    };
}

extern "C" RETRO_API void retro_set_controller_port_device(unsigned int port, unsigned int device) {

}

extern "C" RETRO_API void retro_reset() {
    init_sandbox();
}

extern "C" RETRO_API void retro_run() {
    input_poll();
    video_refresh(frame_buf, 640, 480, 640 * 4);
    audio_sample(0, 0);
}

extern "C" RETRO_API size_t retro_serialize_size() {
    return 0;
}

extern "C" RETRO_API bool retro_serialize(void *data, size_t size) {
    return true;
}

extern "C" RETRO_API bool retro_unserialize(const void *data, size_t size) {
    return true;
}

extern "C" RETRO_API void retro_cheat_reset() {

}

extern "C" RETRO_API void retro_cheat_set(unsigned int index, bool enabled, const char *code) {

}

extern "C" RETRO_API bool retro_load_game(const struct retro_game_info *info) {
    if (info == NULL) {
        log_printf(RETRO_LOG_ERROR, "This core cannot start without a game\n");
        return false;
    }
    game_path = info->path;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        log_printf(RETRO_LOG_ERROR, "XRGB8888 is not supported\n");
        return false;
    }

    return init_sandbox();
}

extern "C" RETRO_API bool retro_load_game_special(unsigned int type, const struct retro_game_info *info, size_t num) {
    return false;
}

extern "C" RETRO_API void retro_unload_game() {
    sandbox.reset();
}

extern "C" RETRO_API unsigned int retro_get_region() {
    return RETRO_REGION_NTSC;
}

extern "C" RETRO_API void *retro_get_memory_data(unsigned int id) {
    return NULL;
}

extern "C" RETRO_API size_t retro_get_memory_size(unsigned int id) {
    return 0;
}
