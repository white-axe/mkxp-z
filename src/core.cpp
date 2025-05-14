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
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <string>

#include <boost/optional.hpp>
#include <alc.h>
#include <alext.h>
#include <fluidsynth.h>

#include "mkxp-polyfill.h" // std::mutex, std::strtoul
#include "git-hash.h"

#include "core.h"
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

static const struct retro_core_option_v2_category core_option_categories[] = {
    {
        .key = "runtime",
        .desc = "Runtime",
        .info = nullptr,
    },
    {
        .key = "video",
        .desc = "Video",
        .info = nullptr,
    },
    {
        .key = "audio",
        .desc = "Audio",
        .info = nullptr,
    },
    {
        .key = nullptr,
        .desc = nullptr,
        .info = nullptr,
    },
};

static const struct retro_core_option_v2_definition core_option_definitions[] = {
    {
        .key = "mkxp-z_rgssVersion",
        .desc = "RGSS Version",
        .desc_categorized = nullptr,
        .info = (
            "Specify the RGSS version to run under."
            " By default, mkxp will try to guess the required version"
            " based on the game files."
            " If this fails, the version defaults to 1."
        ),
        .info_categorized = nullptr,
        .category_key = "runtime",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"1", "1 (RPG Maker XP)"},
            {"2", "2 (RPG Maker VX)"},
            {"3", "3 (RPG Maker VX Ace)"},
            {nullptr, nullptr},
        },
        .default_value = "inherit",
    },
    {
        .key = "mkxp-z_frameSkip",
        .desc = "Frame Skip",
        .desc_categorized = nullptr,
        .info = (
            "Skip (don't draw) frames when behind."
        ),
        .info_categorized = nullptr,
        .category_key = "video",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "disabled",
    },
    {
        .key = "mkxp-z_subImageFix",
        .desc = "Subimage Fix",
        .desc_categorized = nullptr,
        .info = (
            "Work around buggy graphics drivers which don't"
            " properly synchronize texture access, most"
            " apparent when text doesn't show up or the map"
            " tileset doesn't render at all."
            " (default: enabled for systems using OpenGL ES, disabled on other systems)"
        ),
        .info_categorized = nullptr,
        .category_key = "video",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "default",
    },
    {
        .key = "mkxp-z_enableBlitting",
        .desc = "Framebuffer Blitting",
        .desc_categorized = nullptr,
        .info = (
            "Enable framebuffer blitting if the driver is"
            " capable of it. Some drivers carry buggy"
            " implementations of this functionality, so"
            " disabling it can be used as a workaround."
            " (default: disabled on Windows, enabled on other systems)"
        ),
        .info_categorized = nullptr,
        .category_key = "video",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "default",
    },
    {
        .key = "mkxp-z_threadedAudio",
        .desc = "Threaded Audio",
        .desc_categorized = nullptr,
        .info = (
            "Use a worker thread for rendering the audio instead of"
            " rendering in the main thread, if possible. Reduces audio"
            " crackling, especially on systems with slow file system"
            " access speed. Changes to this setting will not take effect"
            " until the game is closed."
        ),
        .info_categorized = nullptr,
        .category_key = "audio",
        .values = {
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "enabled",
    },
    {
        .key = "mkxp-z_midiChorus",
        .desc = "MIDI Chorus",
        .desc_categorized = nullptr,
        .info = (
            "Activate \"chorus\" effect for midi playback."
        ),
        .info_categorized = nullptr,
        .category_key = "audio",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "inherit",
    },
    {
        .key = "mkxp-z_midiReverb",
        .desc = "MIDI Reverb",
        .desc_categorized = nullptr,
        .info = (
            "Activate \"reverb\" effect for midi playback."
        ),
        .info_categorized = nullptr,
        .category_key = "audio",
        .values = {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        .default_value = "inherit",
    },
    {
        .key = "mkxp-z_SESourceCount",
        .desc = "SE Source Count",
        .desc_categorized = nullptr,
        .info = (
            "Number of OpenAL sources to allocate for SE playback."
            " If there are a lot of sounds playing at the same time"
            " and audibly cutting each other off, try increasing"
            " this number."
            " Changes will take effect after the core is reset."
            " (if this value is also set in the game's mkxp.json,"
            " the maximum of the value set here and the value in"
            " mkxp.json will be used)"
        ),
        .info_categorized = nullptr,
        .category_key = "audio",
        .values = {
            {"6", "6"},
            {"7", "7"},
            {"8", "8"},
            {"9", "9"},
            {"10", "10"},
            {"11", "11"},
            {"12", "12"},
            {"13", "13"},
            {"14", "14"},
            {"15", "15"},
            {"16", "16"},
            {"17", "17"},
            {"18", "18"},
            {"19", "19"},
            {"20", "20"},
            {"21", "21"},
            {"22", "22"},
            {"23", "23"},
            {"24", "24"},
            {"25", "25"},
            {"26", "26"},
            {"27", "27"},
            {"28", "28"},
            {"29", "29"},
            {"30", "30"},
            {"31", "31"},
            {"32", "32"},
            {"33", "33"},
            {"34", "34"},
            {"35", "35"},
            {"36", "36"},
            {"37", "37"},
            {"38", "38"},
            {"39", "39"},
            {"40", "40"},
            {"41", "41"},
            {"42", "42"},
            {"43", "43"},
            {"44", "44"},
            {"45", "45"},
            {"46", "46"},
            {"47", "47"},
            {"48", "48"},
            {"49", "49"},
            {"50", "50"},
            {"51", "51"},
            {"52", "52"},
            {"53", "53"},
            {"54", "54"},
            {"55", "55"},
            {"56", "56"},
            {"57", "57"},
            {"58", "58"},
            {"59", "59"},
            {"60", "60"},
            {"61", "61"},
            {"62", "62"},
            {"63", "63"},
            {"64", "64"},
            {nullptr, nullptr},
        },
        .default_value = "6",
    },
    {
        .key = nullptr,
        .desc = nullptr,
        .desc_categorized = nullptr,
        .info = nullptr,
        .info_categorized = nullptr,
        .category_key = nullptr,
        .values = {{nullptr, nullptr}},
        .default_value = nullptr,
    },
};

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
#ifndef MKXPZ_NO_THREADED_AUDIO
    std::atomic<T> atom;
#else
    T atom;
#endif // MKXPZ_NO_THREADED_AUDIO

    atomic() {}

    atomic(T value) : atom(value) {}

    atomic(const struct atomic &guard) = delete;

    atomic(struct atomic &&guard) noexcept = delete;

    struct atomic &operator=(const struct atomic &guard) = delete;

    struct atomic &operator=(struct atomic &&guard) noexcept = delete;

    T load_relaxed() const noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        return atom.load(std::memory_order_relaxed);
#else
        return atom;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    operator T() const noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        return atom.load(std::memory_order_seq_cst);
#else
        return atom;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    void operator=(T value) noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        atom.store(value, std::memory_order_seq_cst);
#else
        atom = value;
#endif // MKXPZ_NO_THREADED_AUDIO
    }

    void operator+=(T value) noexcept {
#ifndef MKXPZ_NO_THREADED_AUDIO
        atom.fetch_add(value, std::memory_order_seq_cst);
#else
        atom += value;
#endif // MKXPZ_NO_THREADED_AUDIO
    }
};

#if !defined(MKXPZ_NO_THREADED_AUDIO) && defined(MKXPZ_NO_STD_ATOMIC_UINT64_T)
template <> struct atomic<uint64_t> {
    mutable std::mutex mutex;
    uint64_t atom;

    atomic() {}

    atomic(uint64_t value) : atom(value) {}

    atomic(const struct atomic &guard) = delete;

    atomic(struct atomic &&guard) noexcept = delete;

    struct atomic &operator=(const struct atomic &guard) = delete;

    struct atomic &operator=(struct atomic &&guard) noexcept = delete;

    uint64_t load_relaxed() const noexcept {
        return atom;
    }

    operator uint64_t() const noexcept {
        struct lock_guard guard(mutex);
        return atom;
    }

    void operator=(uint64_t value) noexcept {
        struct lock_guard guard(mutex);
        atom = value;
    }

    void operator+=(uint64_t value) noexcept {
        struct lock_guard guard(mutex);
        atom += value;
    }
};
#endif // !defined(MKXPZ_NO_THREADED_AUDIO) && defined(MKXPZ_NO_STD_ATOMIC_UINT64_T)

int mkxp_physfs_allow_duplicates = false;

struct physfs_allow_duplicates_guard {
    bool old_value;

    physfs_allow_duplicates_guard() : old_value(mkxp_physfs_allow_duplicates) {
        mkxp_physfs_allow_duplicates = true;
    }

    physfs_allow_duplicates_guard(const struct physfs_allow_duplicates_guard &guard) = delete;

    physfs_allow_duplicates_guard(struct physfs_allow_duplicates_guard &&guard) noexcept = delete;

    struct physfs_allow_duplicates_guard &operator=(const struct physfs_allow_duplicates_guard &guard) = delete;

    struct physfs_allow_duplicates_guard &operator=(struct physfs_allow_duplicates_guard &&guard) noexcept = delete;

    ~physfs_allow_duplicates_guard() {
        mkxp_physfs_allow_duplicates = old_value;
    }
};

static uint64_t frame_count;
static struct atomic<uint64_t> frame_time;
static uint64_t frame_time_remainder;
static uint64_t retro_run_count;

extern const uint8_t dist_zip[];
extern const size_t dist_zip_len;

static bool initialized = false;
static ALCdevice *al_device = nullptr;
static ALCcontext *al_context = nullptr;
static LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT = nullptr;
static LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT = nullptr;
static int16_t *sound_buf = nullptr;
static bool retro_framebuffer_supported;
static bool dupe_supported;
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
static std::string previous_frame_skip_value;

namespace mkxp_retro {
    retro_log_printf_t log_printf;
    retro_video_refresh_t video_refresh;
    retro_audio_sample_batch_t audio_sample_batch;
    retro_environment_t environment;
    retro_input_poll_t input_poll;
    retro_input_state_t input_state;
    struct retro_perf_callback perf;
    retro_hw_render_callback hw_render;
    bool keyboard_state[RETROK_LAST];
    bool input_polled;

    uint8_t sub_image_fix_override;
    uint8_t enable_blitting_override;
    uint8_t midi_chorus_override;
    uint8_t midi_reverb_override;

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

static const char *get_core_option(const char *key) {
    struct retro_variable variable = {
        .key = key,
        .value = "",
    };
    return environment(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) ? variable.value : "";
}

static VALUE func(VALUE arg) {
    struct coro : boost::asio::coroutine {
        VALUE operator()() {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(sandbox_binding_init);
            }

            return SANDBOX_TRUE;
        }
    };

    return sb()->bind<struct coro>()()();
}

static VALUE rescue(VALUE arg, VALUE exception) {
    struct coro : boost::asio::coroutine {
        VALUE operator()(VALUE exception) {
            BOOST_ASIO_CORO_REENTER (this) {
                SANDBOX_AWAIT(log_backtrace, exception);
            }

            return SANDBOX_FALSE;
        }
    };

    return sb()->bind<struct coro>()()(exception);
}

struct main : boost::asio::coroutine {
    typedef decl_slots<VALUE> slots;

    void operator()() {
        BOOST_ASIO_CORO_REENTER (this) {
            SANDBOX_AWAIT_S(0, rb_rescue2, func, SANDBOX_NIL, rescue, SANDBOX_NIL, sb()->rb_eException(), 0);
            if (SANDBOX_VALUE_TO_BOOL(SANDBOX_SLOT(0))) {
                log_printf(RETRO_LOG_INFO, "Game exited; terminating\n");
            } else {
                log_printf(RETRO_LOG_ERROR, "Game threw an exception; terminating\n");
            }
        }
    }
};

static void deinit_sandbox() {
    shared_state_initialized = false;
    struct lock_guard guard(threaded_audio_mutex);

    if (sound_buf != nullptr) {
        mkxp_aligned_free(sound_buf);
        sound_buf = nullptr;
    }
    mkxp_retro::sandbox.reset();
    thread_data.reset();
    input.reset();
    audio.reset();
    if (al_context != nullptr) {
        alcDestroyContext(al_context);
        al_context = nullptr;
    }
    if (al_device != nullptr) {
        alcCloseDevice(al_device);
        al_device = nullptr;
    }
    conf.reset();
    fs.reset();
}

static bool init_sandbox() {
    deinit_sandbox();

    log_printf(RETRO_LOG_INFO, "Initializing FS\n");

    fs.emplace(nullptr, false);

    log_printf(RETRO_LOG_INFO, "Initializing config\n");

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

        fs->addPath(parsed_game_path.c_str(), "/Game");

        conf.emplace();
        {
            const char *value = get_core_option("mkxp-z_rgssVersion");
            if (!std::strcmp(value, "default")) {
                conf->read(0, nullptr, 0);
            } else {
                unsigned long value_num = std::strtoul(value, nullptr, 10);
                if (value_num == 1 || value_num == 2 || value_num == 3) {
                    conf->read(0, nullptr, value_num);
                } else {
                    conf->read(0, nullptr);
                }
            }
        }

        {
            const char *value = get_core_option("mkxp-z_frameSkip");
            previous_frame_skip_value = value;
            if (!std::strcmp(value, "enabled")) {
                conf->frameSkip = true;
            } else if (!std::strcmp(value, "disabled")) {
                conf->frameSkip = false;
            }
        }

        {
            const char *value = get_core_option("mkxp-z_subImageFix");
            if (!std::strcmp(value, "default")) {
                sub_image_fix_override = hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION ? 1 : 0;
            } else if (!std::strcmp(value, "enabled")) {
                sub_image_fix_override = 1;
            } else if (!std::strcmp(value, "disabled")) {
                sub_image_fix_override = 0;
            } else {
                sub_image_fix_override = -1;
            }
        }

        {
            const char *value = get_core_option("mkxp-z_enableBlitting");
            if (!std::strcmp(value, "default")) {
#ifdef _WIN32
                enable_blitting_override = 0;
#else
                enable_blitting_override = 1;
#endif // _WIN32
            } else if (!std::strcmp(value, "enabled")) {
                enable_blitting_override = 1;
            } else if (!std::strcmp(value, "disabled")) {
                enable_blitting_override = 0;
            } else {
                enable_blitting_override = -1;
            }
        }

        {
            const char *value = get_core_option("mkxp-z_midiChorus");
            if (!std::strcmp(value, "enabled")) {
                midi_chorus_override = 1;
            } else if (!std::strcmp(value, "disabled")) {
                midi_chorus_override = 0;
            } else {
                midi_chorus_override = -1;
            }
        }

        {
            const char *value = get_core_option("mkxp-z_midiReverb");
            if (!std::strcmp(value, "enabled")) {
                midi_reverb_override = 1;
            } else if (!std::strcmp(value, "disabled")) {
                midi_reverb_override = 0;
            } else {
                midi_reverb_override = -1;
            }
        }

        {
            unsigned long value_num = std::strtoul(get_core_option("mkxp-z_SESourceCount"), nullptr, 10);
            if (value_num >= 6 && value_num <= 64) {
                conf->SE.sourceCount = std::max(conf->SE.sourceCount, (int)value_num);
            }
        }

        SharedState::rgssVersion = conf->rgssVersion;
        thread_data.emplace(nullptr, nullptr, nullptr, nullptr, 60, 1, *conf);

        PHYSFS_File *rgssad;
        if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgssad").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgssad").c_str(), "/Game", 1);
        } else if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgss2a").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgss2a").c_str(), "/Game", 1);
        } else if ((rgssad = PHYSFS_openRead(("/Game/" + conf->execName + ".rgss3a").c_str())) != nullptr) {
            PHYSFS_mountHandle(rgssad, ('/' + conf->execName + ".rgss3a").c_str(), "/Game", 1);
        }

        PHYSFS_mountMemory(dist_zip, dist_zip_len, nullptr, "/dist.zip", "/Dist", 1);
    }

    log_printf(RETRO_LOG_INFO, "Initializing system directory\n");

    {
        const char *system_path;
        if (environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_path) && system_path != nullptr) {
            std::string system_root_path(system_path);
#ifdef _WIN32
            system_root_path.append("\\mkxp-z");
#else
            system_root_path.append("/mkxp-z");
#endif // _WIN32

            std::string rtp_root_path(system_root_path);
#ifdef _WIN32
            rtp_root_path.append("\\RTP");
#else
            rtp_root_path.append("/RTP");
#endif // _WIN32

            // Create the RTP root directory if needed
            PHYSFS_setWriteDir(system_path);
            if (!PHYSFS_mkdir(rtp_root_path.c_str() + std::strlen(system_path) + 1)) {
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", rtp_root_path.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                deinit_sandbox();
                return false;
            }

            PHYSFS_mount(system_root_path.c_str(), "/System", true);

            // Mount each RTP declared in mkxp.json to the game directory
            for (const std::string &rtp : conf->rtps) {
                std::string path("/System" + fs->normalize(rtp.c_str(), false, true, "/RTP"));

                std::string rtp_path(system_root_path.c_str());
#ifdef _WIN32
                rtp_path.push_back('\\');
#else
                rtp_path.push_back('/');
#endif // _WIN32
                rtp_path.append(path.c_str() + sizeof "/System/" - 1);

                // Check if this is a file or directory
                PHYSFS_Stat stat;
                if (!PHYSFS_stat(path.c_str(), &stat) || (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                    goto fail;
                }

                if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                    // If it's a directory, just mount the path directly
                    if (!PHYSFS_mount(rtp_path.c_str(), "/Game", true)) {
                        goto fail;
                    }
                } else {
                    // If it's a file, try to open it as an archive and then mount it
                    PHYSFS_File *file = PHYSFS_openRead(path.c_str());
                    if (file == nullptr) {
                        goto fail;
                    }
                    if (!PHYSFS_mountHandle(file, path.c_str(), "/Game", true)) {
                        PHYSFS_close(file);
                        goto fail;
                    }
                }

                log_printf(RETRO_LOG_INFO, "Mounted RTP \"%s\" from \"%s\"\n", rtp.c_str(), rtp_path.c_str());
                continue;

            fail:
                log_printf(RETRO_LOG_ERROR, "Failed to mount RTP \"%s\" because \"%s\" was not found\n", rtp.c_str(), rtp_path.c_str());
                continue;
            }

            // Mount each RTP declared in Game.ini to the game directory
            for (const std::string &rtp : conf->game.rtps) {
                struct data {
                    std::string rtp_root_path;
                    std::string rtp;
                    std::string rtp_lowercase;
                    bool found;
                } data = {
                    .rtp_root_path = rtp_root_path,
                    .rtp = rtp,
                    .rtp_lowercase = rtp,
                    .found = false,
                };
                for (char &c : data.rtp_lowercase) {
                    c = std::tolower(c);
                }

                PHYSFS_enumerate("/System/RTP", [](void *data_, const char *origdir, const char *fname) {
                    struct data &data = *(struct data *)data_;
                    std::string rtp(fname);
                    for (char &c : rtp) {
                        c = std::tolower(c);
                    }

                    // Make sure this file/directory has a filename that matches the one we're looking for (case-insensitive)
                    if (std::strncmp(rtp.c_str(), data.rtp_lowercase.c_str(), data.rtp_lowercase.length()) || (rtp[data.rtp_lowercase.length()] != '.' && rtp[data.rtp_lowercase.length()] != 0)) {
                        return PHYSFS_ENUM_OK;
                    }

                    // Check if this is a file or directory
                    std::string fullpath(origdir);
                    fullpath.push_back('/');
                    fullpath.append(fname);
                    PHYSFS_Stat stat;
                    if (!PHYSFS_stat(fullpath.c_str(), &stat) || (stat.filetype != PHYSFS_FILETYPE_DIRECTORY && stat.filetype != PHYSFS_FILETYPE_REGULAR)) {
                        return PHYSFS_ENUM_OK;
                    }

                    std::string rtp_path(data.rtp_root_path);
#ifdef _WIN32
                    rtp_path.push_back('\\');
#else
                    rtp_path.push_back('/');
#endif // _WIN32
                    rtp_path.append(fname);

                    if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
                        // If it's a directory, just mount the path directly
                        if (!PHYSFS_mount(rtp_path.c_str(), "/Game", true)) {
                            return PHYSFS_ENUM_OK;
                        }
                    } else {
                        // If it's a file, try to open it as an archive and then mount it
                        std::string path(origdir);
                        path.push_back('/');
                        path.append(fname);
                        PHYSFS_File *file = PHYSFS_openRead(path.c_str());
                        if (file == nullptr) {
                            return PHYSFS_ENUM_OK;
                        }
                        if (!PHYSFS_mountHandle(file, path.c_str(), "/Game", true)) {
                            PHYSFS_close(file);
                            return PHYSFS_ENUM_OK;
                        }
                    }

                    data.found = true;
                    log_printf(RETRO_LOG_INFO, "Mounted RTP \"%s\" from \"%s\"\n", data.rtp.c_str(), rtp_path.c_str());
                    return PHYSFS_ENUM_STOP;
                }, &data);

                if (!data.found) {
                    log_printf(
                        RETRO_LOG_ERROR,
                        (
                            "Failed to mount RTP \"%s\" because \"%s"
#ifdef _WIN32
                            "\\"
#else
                            "/"
#endif // _WIN32
                            "%s\" was not found\n"
                        ),
                        rtp.c_str(),
                        rtp_root_path.c_str(),
                        rtp.c_str()
                    );
                }
            }
        }
    }

    log_printf(RETRO_LOG_INFO, "Loading path cache\n");

    fs->createPathCache();

    log_printf(RETRO_LOG_INFO, "Initializing save directory\n");

    {
        const char *save_path;
        if (environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_path) && save_path != nullptr) {
            // Save to the subdirectory of the save directory corresponding to the game's name set in Game.ini
            std::string save_path_subdir(save_path);
#ifdef _WIN32
            save_path_subdir.append("\\mkxp-z\\Saves\\");
#else
            save_path_subdir.append("/mkxp-z/Saves/");
#endif // _WIN32
            if (!conf->windowTitle.empty()) {
                save_path_subdir.append(conf->windowTitle);
            } else if (!conf->game.title.empty()) {
                save_path_subdir.append(conf->game.title);
            } else {
                save_path_subdir.append("Game");
            }

            // Sanitize forbidden characters in the game name
            for (size_t i = std::strlen(save_path) + (sizeof "/mkxp-z/Saves/" - 1); i < save_path_subdir.length(); ++i) {
                if (save_path_subdir[i] < 32 || save_path_subdir[i] == '/' || save_path_subdir[i] == '\\' || save_path_subdir[i] == '*' || save_path_subdir[i] == '?' || save_path_subdir[i] == '|' || ((save_path_subdir[i] == ' ' || save_path_subdir[i] == '.') && i + 1 == save_path_subdir.length())) {
                    save_path_subdir[i] = '_';
                } else if (save_path_subdir[i] == '"') {
                    save_path_subdir[i] = '\"';
                } else if (save_path_subdir[i] == ':') {
                    save_path_subdir[i] = ';';
                } else if (save_path_subdir[i] == '<') {
                    save_path_subdir[i] = '(';
                } else if (save_path_subdir[i] == '>') {
                    save_path_subdir[i] = ')';
                }
            }

            // Create the subdirectory if needed
            PHYSFS_setWriteDir(save_path);
            if (!PHYSFS_mkdir(save_path_subdir.c_str() + std::strlen(save_path) + 1)) {
                mkxp_retro::log_printf(RETRO_LOG_ERROR, "Failed to create directory at \"%s\": %s\n", save_path_subdir.c_str(), PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
                deinit_sandbox();
                return false;
            }

            // Mount the subdirectory
            PHYSFS_setWriteDir(save_path_subdir.c_str());
            fs->addPath(save_path_subdir.c_str(), "/Save");
            {
                // PhysFS won't normally allow us to mount the save directory in two locations at once,
                // so we temporarily disable the duplicate detection here
                struct physfs_allow_duplicates_guard guard;
                fs->addPath(save_path_subdir.c_str(), "/Game");
            }
        }
    }

    alcLoopbackOpenDeviceSOFT = (LPALCLOOPBACKOPENDEVICESOFT)alcGetProcAddress(nullptr, "alcLoopbackOpenDeviceSOFT");
    if (alcLoopbackOpenDeviceSOFT == nullptr) {
        log_printf(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcLoopbackOpenDeviceSOFT`\n");
        deinit_sandbox();
        return false;
    }

    alcRenderSamplesSOFT = (LPALCRENDERSAMPLESSOFT)alcGetProcAddress(nullptr, "alcRenderSamplesSOFT");
    if (alcRenderSamplesSOFT == nullptr) {
        log_printf(RETRO_LOG_ERROR, "OpenAL implementation does not support `alcRenderSamplesSOFT`\n");
        deinit_sandbox();
        return false;
    }

    al_device = alcLoopbackOpenDeviceSOFT(nullptr);
    if (al_device == nullptr) {
        log_printf(RETRO_LOG_ERROR, "Failed to initialize OpenAL loopback device\n");
        deinit_sandbox();
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
    if (al_context == nullptr || alcMakeContextCurrent(al_context) == AL_FALSE) {
        log_printf(RETRO_LOG_ERROR, "Failed to create OpenAL context\n");
        deinit_sandbox();
        return false;
    }

    fluid_set_log_function(FLUID_PANIC, fluid_log, nullptr);
    fluid_set_log_function(FLUID_ERR, fluid_log, nullptr);
    fluid_set_log_function(FLUID_WARN, fluid_log, nullptr);
    fluid_set_log_function(FLUID_INFO, fluid_log, nullptr);
    fluid_set_log_function(FLUID_DBG, fluid_log, nullptr);

    audio.emplace(*thread_data);

    input.emplace();

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
    if (sound_buf == nullptr) {
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

    static const struct retro_keyboard_callback keyboard = {
        .callback = [](bool down, unsigned int keycode, uint32_t character, uint16_t key_modifiers) {
            if (keycode < RETROK_LAST) {
                keyboard_state[keycode] = down;
            }
        }
    };
    std::memset(keyboard_state, 0, sizeof keyboard_state);
    cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, (void *)&keyboard);

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

    unsigned int core_options_version;
    if (!cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &core_options_version)) {
        core_options_version = 0;
    }
    switch (core_options_version) {
        default:
            {
                const struct retro_core_options_v2 core_options = {
                    .categories = (struct retro_core_option_v2_category *)core_option_categories,
                    .definitions = (struct retro_core_option_v2_definition *)core_option_definitions,
                };
                if (cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, (void *)&core_options)) {
                    break;
                }
            }

        case 1:
            {
                struct retro_core_option_definition core_options[sizeof core_option_definitions / sizeof *core_option_definitions];
                for (size_t i = 0; i < sizeof core_options / sizeof *core_options; ++i) {
                    core_options[i].key = core_option_definitions[i].key;
                    core_options[i].desc = core_option_definitions[i].desc;
                    core_options[i].info = core_option_definitions[i].info;
                    size_t num_values = 0;
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        ++num_values;
                    }
                    std::memcpy(core_options[i].values, core_option_definitions[i].values, (1 + num_values) * sizeof *core_option_definitions[i].values);
                    core_options[i].default_value = core_option_definitions[i].default_value;
                }
                if (cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, (void *)&core_options)) {
                    break;
                }
            }

        case 0:
            {
                struct retro_variable core_options[sizeof core_option_definitions / sizeof *core_option_definitions];
                std::string values[sizeof core_options / sizeof *core_options];
                size_t i;
                for (i = 0; i < sizeof core_options / sizeof *core_options - 1; ++i) {
                    core_options[i].key = core_option_definitions[i].key;
                    size_t values_length = 0;
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        values_length += 1 + std::strlen(value->value);
                    }
                    values[i].reserve(std::strlen(core_option_definitions[i].desc) + 1 + values_length);
                    values[i] = core_option_definitions[i].desc;
                    values[i].append("; ");
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        if (std::strcmp(value->value, core_option_definitions[i].default_value)) {
                            continue;
                        }
                        values[i].append(value->value);
                        break;
                    }
                    for (const struct retro_core_option_value *value = core_option_definitions[i].values; value->value != nullptr; ++value) {
                        if (!std::strcmp(value->value, core_option_definitions[i].default_value)) {
                            continue;
                        }
                        values[i].push_back('|');
                        values[i].append(value->value);
                    }
                    core_options[i].value = values[i].c_str();
                }
                core_options[i].key = nullptr;
                core_options[i].value = nullptr;
                cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)&core_options);
            }
    }
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

    {
        bool core_options_updated;
        if (environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &core_options_updated) && core_options_updated) {
            {
                const char *value = get_core_option("mkxp-z_frameSkip");
                if (previous_frame_skip_value != value) {
                    previous_frame_skip_value = value;
                    if (!std::strcmp(value, "enabled")) {
                        shState->graphics().setFrameskip(true);
                    } else if (!std::strcmp(value, "disabled")) {
                        shState->graphics().setFrameskip(false);
                    }
                }
            }

            {
                const char *value = get_core_option("mkxp-z_subImageFix");
                if (!std::strcmp(value, "default")) {
                    sub_image_fix_override = hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 || hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION ? 1 : 0;
                } else if (!std::strcmp(value, "enabled")) {
                    sub_image_fix_override = 1;
                } else if (!std::strcmp(value, "disabled")) {
                    sub_image_fix_override = 0;
                } else {
                    sub_image_fix_override = -1;
                }
            }

            {
                const char *value = get_core_option("mkxp-z_enableBlitting");
                if (!std::strcmp(value, "default")) {
    #ifdef _WIN32
                    enable_blitting_override = 0;
    #else
                    enable_blitting_override = 1;
    #endif // _WIN32
                } else if (!std::strcmp(value, "enabled")) {
                    enable_blitting_override = 1;
                } else if (!std::strcmp(value, "disabled")) {
                    enable_blitting_override = 0;
                } else {
                    enable_blitting_override = -1;
                }
            }

            {
                const char *value = get_core_option("mkxp-z_midiChorus");
                if (!std::strcmp(value, "enabled")) {
                    midi_chorus_override = true;
                    if (shState->midiState().inited) {
                        fluid.settings_setint(shState->midiState().flSettings, "synth.chorus.active", midi_chorus_override == 1 || (midi_chorus_override != 0 && conf->midi.chorus));
                    }
                } else if (!std::strcmp(value, "disabled")) {
                    midi_chorus_override = false;
                    if (shState->midiState().inited) {
                        fluid.settings_setint(shState->midiState().flSettings, "synth.chorus.active", midi_chorus_override == 1 || (midi_chorus_override != 0 && conf->midi.chorus));
                    }
                }
            }

            {
                const char *value = get_core_option("mkxp-z_midiReverb");
                if (!std::strcmp(value, "enabled")) {
                    midi_reverb_override = true;
                    if (shState->midiState().inited) {
                        fluid.settings_setint(shState->midiState().flSettings, "synth.reverb.active", midi_reverb_override == 1 || (midi_reverb_override != 0 && conf->midi.reverb));
                    }
                } else if (!std::strcmp(value, "disabled")) {
                    midi_reverb_override = false;
                    if (shState->midiState().inited) {
                        fluid.settings_setint(shState->midiState().flSettings, "synth.reverb.active", midi_reverb_override == 1 || (midi_reverb_override != 0 && conf->midi.reverb));
                    }
                }
            }
        }
    }

    if (should_render) {
        if (sb().run<struct main>()) {
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
        if (gl.BindVertexArray != nullptr) {
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
        retro_usec_t new_reference = 1000000 / (sb().get_movie_from_main_thread() != nullptr ? av_info.timing.fps : shState->graphics().getFrameRate());
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
    if (info == nullptr || info->path == nullptr) {
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
    hw_render.context_destroy = nullptr;
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

#ifndef MKXPZ_NO_THREADED_AUDIO
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
    bool threaded_audio_allowed;
    {
        const char *value = get_core_option("mkxp-z_threadedAudio");
        if (!std::strcmp(value, "disabled")) {
            threaded_audio_allowed = false;
        } else {
            threaded_audio_allowed = true;
        }
    }
    if (threaded_audio_allowed) {
        threaded_audio_enabled = environment(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &audio_callback);
        log_printf(RETRO_LOG_INFO, threaded_audio_enabled ? "Using threaded audio driver\n" : "Not using threaded audio driver because the frontend does not support it\n");
    } else {
        threaded_audio_enabled = false;
        log_printf(RETRO_LOG_INFO, "Not using threaded audio driver because threaded audio is disabled in the core options\n");
    }
#else
    log_printf(RETRO_LOG_INFO, "Not using threaded audio driver because multithreading is not supported on this platform\n");
#endif // MKXPZ_NO_THREADED_AUDIO

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
    return nullptr;
}

extern "C" RETRO_API size_t retro_get_memory_size(unsigned int id) {
    return 0;
}
