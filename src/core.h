#ifndef MKXPZ_CORE_H
#define MKXPZ_CORE_H

#include <libretro.h>

namespace mkxp_retro {
    extern retro_log_printf_t log_printf;
    extern retro_video_refresh_t video_refresh;
    extern retro_audio_sample_t audio_sample;
    extern retro_audio_sample_batch_t audio_sample_batch;
    extern retro_environment_t environment;
    extern retro_input_poll_t input_poll;
    extern retro_input_state_t input_state;
    extern struct retro_perf_callback perf;
}

#endif // MKXPZ_CORE_H
