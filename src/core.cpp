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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <boost/optional.hpp>
#include <AL/alc.h>
#include <AL/alext.h>
#include <fluidlite.h>
#include <fluidsynth_priv.h>
#include "git-hash.h"
#include "binding-sandbox/sandbox.h"
#include "binding-sandbox/binding-sandbox.h"
#include "binding-sandbox/core.h"
#include "filesystem.h"

using namespace mkxp_retro;
using namespace mkxp_sandbox;

static inline void *malloc_align(size_t alignment, size_t size) {
#if defined(__unix__) || defined(__APPLE__)
    void *mem;
    return posix_memalign(&mem, alignment, size) ? NULL : mem;
#elif defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

extern unsigned char GMGSx_sf2[];
extern unsigned int GMGSx_sf2_len;

static ALCdevice *al_device = NULL;
static ALCcontext *al_context = NULL;
static LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT = NULL;
static LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT = NULL;
static int16_t *sound_buf;

static void fallback_log(enum retro_log_level level, const char *fmt, ...) {
    std::va_list va;
    va_start(va, fmt);
    std::vfprintf(stderr, fmt, va);
    va_end(va);
}

static void fluid_log(int level, char *message, void *data) {
    switch (level) {
        case FLUID_PANIC:
            log_printf(RETRO_LOG_ERROR, "fluidsynth: panic: %s\n", message);
            break;
        case FLUID_ERR:
            log_printf(RETRO_LOG_ERROR, "fluidsynth: error: %s\n", message);
            break;
        case FLUID_WARN:
            log_printf(RETRO_LOG_WARN, "fluidsynth: warning: %s\n", message);
            break;
        case FLUID_INFO:
            log_printf(RETRO_LOG_INFO, "fluidsynth: %s\n", message);
            break;
        case FLUID_DBG:
            log_printf(RETRO_LOG_DEBUG, "fluidsynth: debug: %s\n", message);
            break;
    }
}

static uint32_t *frame_buf;
boost::optional<struct sandbox> mkxp_retro::sandbox;
boost::optional<Audio> mkxp_retro::audio;
boost::optional<Input> mkxp_retro::input;
boost::optional<FileSystem> mkxp_retro::fs;
static std::string game_path;

static VALUE func(VALUE arg) {
    SANDBOX_COROUTINE(coro,
        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(sandbox_binding_init);
            }
        }
    )

    sb()->bind<struct coro>()()();

    return arg;
}

static VALUE rescue(VALUE arg, VALUE exception) {
    SANDBOX_COROUTINE(coro,
        void operator()(VALUE exception) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_eval_string, "puts 'Entered rescue()'");
                SANDBOX_AWAIT(rb_p, exception);
            }
        }
    )

    sb()->bind<struct coro>()()(exception);

    return arg;
}

SANDBOX_COROUTINE(main,
    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT(rb_rescue, func, 0, rescue, 0);
        }
    }
)

static void deinit_sandbox() {
    mkxp_retro::sandbox.reset();
    audio.reset();
    if (al_context != NULL) {
        alcDestroyContext(al_context);
        al_context = NULL;
    }
    if (al_device != NULL) {
        alcCloseDevice(al_device);
        al_device = NULL;
    }
    fs.reset();
    input.reset();
}

static bool init_sandbox() {
    deinit_sandbox();

    input.emplace();

    fs.emplace((const char *)NULL, false);

    {
        std::string parsed_game_path(fs->normalize(game_path.c_str(), true, true));

        // If the game path doesn't end with ".mkxp" or ".mkxpz", remove the last component from the path since we want to mount the directory that the file is in, not the file itself.
        if (
            !(parsed_game_path.length() >= 5 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 5), ".mkxp") == 0)
                && !(parsed_game_path.length() >= 5 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 5), ".MKXP") == 0)
                && !(parsed_game_path.length() >= 6 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 6), ".mkxpz") == 0)
                && !(parsed_game_path.length() >= 6 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 6), ".MKXPZ") == 0)
        ) {
            size_t last_slash_index = parsed_game_path.find_last_of('/');
            if (last_slash_index == std::string::npos) {
                last_slash_index = 0;
            }
            parsed_game_path = parsed_game_path.substr(0, last_slash_index);
        }

        fs->addPath(parsed_game_path.c_str(), "/mkxp-retro-game");
    }

    fs->createPathCache();

    alcLoopbackOpenDeviceSOFT = (LPALCLOOPBACKOPENDEVICESOFT)alcGetProcAddress(NULL, "alcLoopbackOpenDeviceSOFT");
    if (alcLoopbackOpenDeviceSOFT == NULL) {
        log_printf(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcLoopbackOpenDeviceSOFT`\n");
        return false;
    }

    alcRenderSamplesSOFT = (LPALCRENDERSAMPLESSOFT)alcGetProcAddress(NULL, "alcRenderSamplesSOFT");
    if (alcRenderSamplesSOFT == NULL) {
        log_printf(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcRenderSamplesSOFT`\n");
        return false;
    }

    al_device = alcLoopbackOpenDeviceSOFT(NULL);
    if (al_device == NULL) {
        log_printf(RETRO_LOG_ERROR, "Failed to initialize OpenAL loopback device\n");
        return false;
    }

    static const ALCint al_attrs[] = {
        ALC_FORMAT_CHANNELS_SOFT,
        ALC_STEREO_SOFT,
        ALC_FORMAT_TYPE_SOFT,
        ALC_SHORT_SOFT,
        ALC_FREQUENCY,
        44100,
        0,
    };
    al_context = alcCreateContext(al_device, al_attrs);
    if (al_context == NULL || alcMakeContextCurrent(al_context) == AL_FALSE) {
        log_printf(RETRO_LOG_ERROR, "Failed to create OpenAL context\n");
        return false;
    }

    fluid_set_log_function(FLUID_PANIC, fluid_log, NULL);
    fluid_set_log_function(FLUID_ERR, fluid_log, NULL);
    fluid_set_log_function(FLUID_WARN, fluid_log, NULL);
    fluid_set_log_function(FLUID_INFO, fluid_log, NULL);
    fluid_set_log_function(FLUID_DBG, fluid_log, NULL);

    static fluid_fileapi_t fluid_fileapi = {
        .free = [](fluid_fileapi_t *f) {
            return 0;
        },
        .fopen = [](fluid_fileapi_t *f, const char *filename) {
            assert(std::strcmp(filename, "/GMGSx.sf2") == 0);
            return std::calloc(1, sizeof(long));
        },
        .fread = [](void *buf, int count, void *handle) {
            assert(*(long *)handle + count < GMGSx_sf2_len);
            std::memcpy(buf, GMGSx_sf2 + *(long *)handle, count);
            *(long *)handle += count;
            return (int)FLUID_OK;
        },
        .fseek = [](void *handle, long offset, int origin) {
            switch (origin) {
                case SEEK_CUR:
                    *(long *)handle += offset;
                    break;
                case SEEK_END:
                    *(long *)handle = GMGSx_sf2_len + offset;
                    break;
                default:
                    *(long *)handle = offset;
                    break;
            }
            return (int)FLUID_OK;
        },
        .fclose = [](void *handle) {
            std::free(handle);
            return (int)FLUID_OK;
        },
        .ftell = [](void *handle) {
            return *(long *)handle;
        },
    };
    fluid_set_default_fileapi(&fluid_fileapi);

    audio.emplace();

    SharedState::initInstance(NULL);

    try {
        mkxp_retro::sandbox.emplace();
    } catch (SandboxException) {
        log_printf(RETRO_LOG_ERROR, "Failed to initialize Ruby\n");
        deinit_sandbox();
        return false;
    }

    return true;
}

extern "C" RETRO_API void retro_set_environment(retro_environment_t cb) {
    environment = cb;

    struct retro_log_callback log;
#ifndef __EMSCRIPTEN__
    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log)) {
        log_printf = log.log;
    } else {
#endif // __EMSCRIPTEN__
        log_printf = fallback_log;
#ifndef __EMSCRIPTEN__
    }
#endif // __EMSCRIPTEN__

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
    frame_buf = (uint32_t *)std::calloc(640 * 480, sizeof *frame_buf);
    sound_buf = (int16_t *)malloc_align(16, 735 * 2 * sizeof *sound_buf);
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
    info->library_version = MKXPZ_VERSION "/" MKXPZ_GIT_HASH;
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

    if (!mkxp_retro::sandbox.has_value()) {
        return;
    }

    try {
        if (sb().run<struct main>()) {
            log_printf(RETRO_LOG_INFO, "[Sandbox] Ruby terminated normally\n");
            deinit_sandbox();
            return;
        }
    } catch (SandboxException) {
        log_printf(RETRO_LOG_ERROR, "[Sandbox] Ruby threw an exception\n");
        deinit_sandbox();
        return;
    }

    video_refresh(frame_buf, 640, 480, 640 * 4);

    audio->render();
    alcRenderSamplesSOFT(al_device, sound_buf, 735);
    audio_sample_batch(sound_buf, 735);
}

extern "C" RETRO_API size_t retro_serialize_size() {
    return 0;
}

extern "C" RETRO_API bool retro_serialize(void *data, size_t len) {
    return true;
}

extern "C" RETRO_API bool retro_unserialize(const void *data, size_t len) {
    return true;
}

extern "C" RETRO_API void retro_cheat_reset() {

}

extern "C" RETRO_API void retro_cheat_set(unsigned int index, bool enabled, const char *code) {

}

extern "C" RETRO_API bool retro_load_game(const struct retro_game_info *info) {
    if (info == NULL || info->path == NULL) {
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
    deinit_sandbox();
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
