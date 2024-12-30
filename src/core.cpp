/*
** main.cpp
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
#include "core.h"
#include "sandbox/sandbox.h"

using namespace mkxp_retro;

static void fallback_log(enum retro_log_level level, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

static uint32_t *frame_buf;
static std::unique_ptr<Sandbox> sandbox;

static bool init_sandbox() {
    sandbox.reset();

    try {
        sandbox.reset(new Sandbox());

        sandbox->rb_eval_string("puts 'Hello, World!'");
        sandbox->rb_eval_string("require 'zlib'; p Zlib::Deflate::deflate('hello')");
    } catch (SandboxException) {
        log_printf(RETRO_LOG_ERROR, "Failed to initialize Ruby\n");
        sandbox.reset();
        return false;
    }

    return true;
}

void retro_set_environment(retro_environment_t cb) {
    environment = cb;

    struct retro_log_callback log;
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_printf = log.log;
    } else {
        log_printf = fallback_log;
    }

    bool support_no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &support_no_game);

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

void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_refresh = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_sample = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_sample_batch = cb;
}

void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
    input_state = cb;
}

void retro_init() {
    frame_buf = (uint32_t *)std::calloc(640 * 480, sizeof(uint32_t));
}

void retro_deinit() {
    std::free(frame_buf);
}

unsigned int retro_api_version() {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "mkxp-z";
    info->library_version = "rolling";
    info->need_fullpath = false;
    info->valid_extensions = "zip";
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
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

void retro_set_controller_port_device(unsigned int port, unsigned int device) {

}

void retro_reset() {
    init_sandbox();
}

void retro_run() {
    input_poll();
    video_refresh(frame_buf, 640, 480, 640 * 4);
    audio_sample(0, 0);
}

size_t retro_serialize_size() {
    return 0;
}

bool retro_serialize(void *data, size_t size) {
    return true;
}

bool retro_unserialize(const void *data, size_t size) {
    return true;
}

void retro_cheat_reset() {

}

void retro_cheat_set(unsigned int index, bool enabled, const char *code) {

}

bool retro_load_game(const struct retro_game_info *info) {
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        log_printf(RETRO_LOG_ERROR, "XRGB8888 is not supported\n");
        return false;
    }

    return init_sandbox();
}

bool retro_load_game_special(unsigned int type, const struct retro_game_info *info, size_t num) {
    return false;
}

void retro_unload_game() {
    sandbox.reset();
}

unsigned int retro_get_region() {
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned int id) {
    return NULL;
}

size_t retro_get_memory_size(unsigned int id) {
    return 0;
}
