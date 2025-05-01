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

#include <atomic>
#include <cstdarg>

#include <boost/optional.hpp>
#include <alc.h>
#include <alext.h>
#include <fluidsynth.h>

#include "mkxp-polyfill.h" // std::mutex
#include "git-hash.h"

#include "binding-sandbox.h"

#include "al-util.h"
#include "audio.h"
#include "eventthread.h"
#include "filesystem.h"
#include "gl-fun.h"
#include "glstate.h"
#include "graphics.h"
#include "sharedmidistate.h"
#include "sharedstate.h"

#define THREADED_AUDIO_SAMPLES (((size_t)SYNTH_SAMPLERATE * (size_t)AUDIO_SLEEP) / (size_t)1000)

using namespace mkxp_retro;
using namespace mkxp_sandbox;

struct lock_guard {
    std::mutex &mutex;

    lock_guard(std::mutex &mutex) : mutex(mutex) {
        mutex.lock();
    }

    lock_guard(const struct lock_guard &guard) = delete;

    lock_guard(struct lock_guard &&guard) noexcept = delete;

    struct lock_guard &operator=(const struct lock_guard &guard) = delete;

    struct lock_guard &operator=(struct lock_guard &&guard) noexcept = delete;

    ~lock_guard() {
        mutex.unlock();
    }
};

template <typename T> struct atomic {
#ifdef MKXPZ_HAVE_THREADED_AUDIO
    std::atomic<T> atom;
#else
    T atom;
#endif // MKXPZ_HAVE_THREADED_AUDIO

    atomic() {}

    atomic(T value) : atom(value) {}

    atomic(const struct atomic &guard) = delete;

    atomic(struct atomic &&guard) noexcept = delete;

    struct atomic &operator=(const struct atomic &guard) = delete;

    struct atomic &operator=(struct atomic &&guard) noexcept = delete;

    T load_relaxed() const noexcept {
#ifdef MKXPZ_HAVE_THREADED_AUDIO
        return atom.load(std::memory_order_relaxed);
#else
        return atom;
#endif // MKXPZ_HAVE_THREADED_AUDIO
    }

    operator T() const noexcept {
#ifdef MKXPZ_HAVE_THREADED_AUDIO
        return atom.load(std::memory_order_seq_cst);
#else
        return atom;
#endif // MKXPZ_HAVE_THREADED_AUDIO
    }

    void operator=(T value) noexcept {
#ifdef MKXPZ_HAVE_THREADED_AUDIO
        atom.store(value, std::memory_order_seq_cst);
#else
        atom = value;
#endif // MKXPZ_HAVE_THREADED_AUDIO
    }

    void operator+=(T value) noexcept {
#ifdef MKXPZ_HAVE_THREADED_AUDIO
        atom.fetch_add(value, std::memory_order_seq_cst);
#else
        atom += value;
#endif // MKXPZ_HAVE_THREADED_AUDIO
    }
};

static uint64_t frame_count;
static struct atomic<uint64_t> frame_time;
static uint64_t frame_time_remainder;
static uint64_t retro_run_count;

extern const uint8_t mkxp_retro_dist_zip[];
extern const size_t mkxp_retro_dist_zip_len;

static bool initialized = false;
static ALCdevice *al_device = NULL;
static ALCcontext *al_context = NULL;
static LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT = NULL;
static LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT = NULL;
static int16_t *sound_buf = NULL;
static bool retro_framebuffer_supported;
static bool dupe_supported;
static PHYSFS_File *rgssad = NULL;
static retro_system_av_info av_info;
static struct retro_audio_callback audio_callback;
static struct retro_frame_time_callback frame_time_callback = {
    .callback = [](retro_usec_t delta) {
        frame_time += delta;
        frame_time_remainder += delta;
    },
};
static std::mutex threaded_audio_mutex;
static bool threaded_audio_enabled = false;
static bool frame_time_callback_enabled = false;
static struct atomic<bool> shared_state_initialized(false);

namespace mkxp_retro {
    retro_log_printf_t log_printf;
    retro_video_refresh_t video_refresh;
    retro_audio_sample_batch_t audio_sample_batch;
    retro_environment_t environment;
    retro_input_poll_t input_poll;
    retro_input_state_t input_state;
    struct retro_perf_callback perf;
    retro_hw_render_callback hw_render;
    bool input_polled;

    uint64_t get_ticks_ms() noexcept {
        return frame_time / 1000;
    }

    uint64_t get_ticks_us() noexcept {
        return frame_time;
    }

    double get_refresh_rate() noexcept {
        return av_info.timing.fps;
    }

    bool using_threaded_audio() noexcept {
        return threaded_audio_enabled;
    }
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...) {
    std::va_list va;
    va_start(va, fmt);
    std::vfprintf(stderr, fmt, va);
    va_end(va);
}

static void fluid_log(int level, const char *message, void *data) {
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
static boost::optional<Config> conf;
static boost::optional<RGSSThreadData> thread_data;
static std::string game_path;

static void audio_render(size_t samples) {
    audio->render();
    alcRenderSamplesSOFT(al_device, sound_buf, samples);
    for (size_t n = 0; n < samples;) {
        n += audio_sample_batch(sound_buf + 2 * n, samples - n);
    }
}

static VALUE func(VALUE arg) {
    struct coro : boost::asio::coroutine {
        void operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(sandbox_binding_init);
            }
        }
    };

    sb()->bind<struct coro>()()();

    return arg;
}

static VALUE rescue(VALUE arg, VALUE exception) {
    struct coro : boost::asio::coroutine {
        void operator()(VALUE exception) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(rb_eval_string, "puts 'Entered rescue()'");
                SANDBOX_AWAIT(rb_p, exception);
            }
        }
    };

    sb()->bind<struct coro>()()(exception);

    return arg;
}

struct main : boost::asio::coroutine {
    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT(rb_rescue2, func, SANDBOX_NIL, rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
        }
    }
};

static void deinit_sandbox() {
    shared_state_initialized = false;
    struct lock_guard guard(threaded_audio_mutex);

    if (sound_buf != NULL) {
        mkxp_aligned_free(sound_buf);
        sound_buf = NULL;
    }
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
    if (rgssad != NULL) {
        PHYSFS_close(rgssad);
        rgssad = NULL;
    }
    thread_data.reset();
    conf.reset();
    fs.reset();
    input.reset();
}

static bool init_sandbox() {
    deinit_sandbox();

    input.emplace();

    fs.emplace((const char *)NULL, false);

    {
        std::string parsed_game_path(game_path);

        // If the game path doesn't end with ".mkxp" or ".mkxpz", remove the last component from the path since we want to mount the directory that the file is in, not the file itself.
        if (
            !(parsed_game_path.length() >= 5 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 5), ".mkxp") == 0)
                && !(parsed_game_path.length() >= 5 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 5), ".MKXP") == 0)
                && !(parsed_game_path.length() >= 6 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 6), ".mkxpz") == 0)
                && !(parsed_game_path.length() >= 6 && std::strcmp(parsed_game_path.c_str() + (parsed_game_path.length() - 6), ".MKXPZ") == 0)
        ) {
            size_t last_slash_index = parsed_game_path.find_last_of('/');
#ifdef _WIN32
            size_t last_backslash_index = parsed_game_path.find_last_of('\\');
            if (last_backslash_index != std::string::npos) {
                last_slash_index = last_slash_index == std::string::npos ? last_backslash_index : std::max(last_slash_index, last_backslash_index);
            }
#endif // _WIN32
            if (last_slash_index == std::string::npos) {
                last_slash_index = 0;
            }
            parsed_game_path = parsed_game_path.substr(0, last_slash_index);
        }

        fs->addPath(parsed_game_path.c_str(), "/mkxp-retro-game");

        conf.emplace();
        conf->read(0, NULL);
        SharedState::rgssVersion = conf->rgssVersion;
        thread_data.emplace((EventThread *)NULL, (const char *)NULL, (SDL_Window *)NULL, (ALCdevice *)NULL, 60, 1, *conf);

        if ((rgssad = PHYSFS_openRead(("/mkxp-retro-game/" + conf->execName + ".rgssad").c_str())) != NULL) {
            PHYSFS_mountHandle(rgssad, (conf->execName + ".rgssad").c_str(), "/mkxp-retro-game", 1);
        } else if ((rgssad = PHYSFS_openRead(("/mkxp-retro-game/" + conf->execName + ".rgss2a").c_str())) != NULL) {
            PHYSFS_mountHandle(rgssad, (conf->execName + ".rgss2a").c_str(), "/mkxp-retro-game", 1);
        } else if ((rgssad = PHYSFS_openRead(("/mkxp-retro-game/" + conf->execName + ".rgss3a").c_str())) != NULL) {
            PHYSFS_mountHandle(rgssad, (conf->execName + ".rgss3a").c_str(), "/mkxp-retro-game", 1);
        }

        PHYSFS_mountMemory(mkxp_retro_dist_zip, mkxp_retro_dist_zip_len, NULL, "mkxp-retro-dist.zip", "/mkxp-retro-dist", 1);
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
        SYNTH_SAMPLERATE,
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

    audio.emplace(*thread_data);

    mkxp_retro::sandbox.emplace();

    {
        float refresh_rate;
        if (environment(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &refresh_rate)) {
            av_info.timing.fps = refresh_rate;
        } else {
            refresh_rate = 60;
        }
    }

    av_info.geometry.base_width = conf->defScreenW;
    av_info.geometry.base_height = conf->defScreenH;
    av_info.geometry.max_width = av_info.geometry.base_width;
    av_info.geometry.max_height = av_info.geometry.base_height;
    av_info.geometry.aspect_ratio = (float)av_info.geometry.base_width / (float)av_info.geometry.base_height;
    av_info.timing.sample_rate = (double)SYNTH_SAMPLERATE;
    frame_time_callback.reference = 1000000 / (rgssVer == 1 ? 40 : 60);
    frame_time_callback_enabled = environment(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_time_callback);

    sound_buf = (int16_t *)mkxp_aligned_malloc(16, (threaded_audio_enabled ? THREADED_AUDIO_SAMPLES : (size_t)std::ceil((double)SYNTH_SAMPLERATE / av_info.timing.fps)) * 2 * sizeof(int16_t));
    if (sound_buf == NULL) {
        throw std::bad_alloc();
    }

    frame_count = 0;
    frame_time = 0;
    frame_time_remainder = 0;
    retro_run_count = 0;

    return true;
}

extern "C" RETRO_API void retro_set_environment(retro_environment_t cb) {
    environment = cb;

    // Bug in RetroArch:
    // retro_set_environment is called multiple times and only the first time
    // callbacks will work and return true.
    if (initialized) {
        return;
    }

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
    initialized = true;
    frame_buf = (uint32_t *)std::calloc(640 * 480, sizeof *frame_buf);
}

extern "C" RETRO_API void retro_deinit() {
    std::free(frame_buf);
    initialized = false;
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
    *info = av_info;
}

extern "C" RETRO_API void retro_set_controller_port_device(unsigned int port, unsigned int device) {

}

extern "C" RETRO_API void retro_reset() {
    init_sandbox();
}

extern "C" RETRO_API void retro_run() {
    bool should_render = mkxp_retro::sandbox.has_value() && (frame_count == 0 || frame_time_remainder >= frame_time_callback.reference);

    if (should_render) {
        frame_time_remainder %= frame_time_callback.reference;
    }

    if (!frame_time_callback_enabled) {
        uint64_t reference = 1000000 / av_info.timing.fps;
        frame_time += reference;
        frame_time_remainder += reference;
    }

    input_polled = false;

    // We deferred initializing the shared state since the OpenGL symbols aren't available until the first call to `retro_run()`
    if (mkxp_retro::sandbox.has_value() && !shared_state_initialized.load_relaxed()) {
        SharedState::initInstance(&thread_data.get());
        shared_state_initialized = true;
    } else if (hw_render.context_type != RETRO_HW_CONTEXT_NONE && (should_render || (!dupe_supported && mkxp_retro::sandbox.has_value()))) {
        glState.reset();
    }

    if (should_render) {
        if (sb().run<struct main>()) {
            log_printf(RETRO_LOG_INFO, "[Sandbox] Ruby terminated normally\n");
            deinit_sandbox();
        }
    } else if (!dupe_supported && mkxp_retro::sandbox.has_value()) {
        shState->graphics().repaint(sb().transitioning);
    }

    // We need to call `input_poll()` at least once every time `retro_run()` is called
    if (!input_polled) {
        input_poll();
    }

    void *fb;
    if (dupe_supported && !should_render) {
        fb = nullptr;
    } else if (hw_render.context_type != RETRO_HW_CONTEXT_NONE) {
        gl.UseProgram(0);
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, 0);
        if (gl.BindVertexArray != NULL) {
            gl.BindVertexArray(0);
        }
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        fb = RETRO_HW_FRAME_BUFFER_VALID;
    } else if (!retro_framebuffer_supported) {
        fb = frame_buf;
    } else {
        struct retro_framebuffer retro_framebuffer;
        if (environment(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &retro_framebuffer) && retro_framebuffer.format == RETRO_PIXEL_FORMAT_XRGB8888) {
            fb = retro_framebuffer.data;
        } else {
            retro_framebuffer_supported = false;
            fb = frame_buf;
        }
    }
    unsigned int width = shState->graphics().width();
    unsigned int height = shState->graphics().height();
    video_refresh(fb, width, height, width * 4);

    if (!threaded_audio_enabled && mkxp_retro::sandbox.has_value()) {
        audio_render((uint64_t)std::ceil((double)((uint64_t)SYNTH_SAMPLERATE * (retro_run_count + 1)) / av_info.timing.fps) - (uint64_t)std::ceil((double)((uint64_t)SYNTH_SAMPLERATE * retro_run_count) / av_info.timing.fps));
    }

    if (mkxp_retro::sandbox.has_value()) {
        retro_usec_t new_reference = 1000000 / shState->graphics().getFrameRate();
        if (new_reference != frame_time_callback.reference) {
            frame_time_callback.reference = new_reference;
            frame_time_callback_enabled = environment(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_time_callback);
        }
    }

    if (should_render) {
        ++frame_count;
    }
    ++retro_run_count;
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

    std::memset(&hw_render, 0, sizeof hw_render);
    hw_render.context_reset = initGLFunctions;
    hw_render.context_destroy = NULL;
    hw_render.cache_context = true;
    hw_render.bottom_left_origin = true;
    if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 6, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.6 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 5, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.5 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 4, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.4 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 3, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.3 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 4, hw_render.version_minor = 0, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 4.0 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 3.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES_VERSION, hw_render.version_major = 3, hw_render.version_minor = 2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL ES 3.2 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 3.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES_VERSION, hw_render.version_major = 3, hw_render.version_minor = 1, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL ES 3.1 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE, hw_render.version_major = 3, hw_render.version_minor = 0, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 3.0 graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES3, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL ES 3.x graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGL, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL 2.x graphics driver\n");
    } else if (hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2, environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        log_printf(RETRO_LOG_INFO, "Using OpenGL ES 2.0 graphics driver\n");
    } else {
        // TODO: Support software rendering again
        //log_printf(RETRO_LOG_WARN, "Hardware-accelerated graphics not supported; falling back to software rendering\n");
        //hw_render.context_type = RETRO_HW_CONTEXT_NONE;
        //environment(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render);
        log_printf(RETRO_LOG_ERROR, "Error: Hardware-accelerated graphics not supported\n");
        return false;
    }

#ifdef MKXPZ_HAVE_THREADED_AUDIO
    audio_callback.callback = []() {
        if (!shared_state_initialized) {
            return;
        }

        struct lock_guard guard(threaded_audio_mutex);

        if (!shared_state_initialized) {
            return;
        }

        audio_render(THREADED_AUDIO_SAMPLES);
    };
    audio_callback.set_state = nullptr;
    threaded_audio_enabled = environment(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_callback);
    log_printf(RETRO_LOG_INFO, threaded_audio_enabled ? "Using threaded audio driver\n" : "Not using threaded audio driver because the frontend does not support it\n");
#else
    log_printf(RETRO_LOG_INFO, "Not using threaded audio driver because multithreading is not supported on this platform\n");
#endif // MKXPZ_HAVE_THREADED_AUDIO

    {
        bool value;
        dupe_supported = environment(RETRO_ENVIRONMENT_GET_CAN_DUPE, &value) && value;
    }

    retro_framebuffer_supported = true;

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
