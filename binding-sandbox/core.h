/*
** core.h
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

#ifndef MKXPZ_CORE_H
#define MKXPZ_CORE_H

#include <libretro.h>
#include "sandbox.h"
#include "audio.h"
#include "filesystem.h"
#include "input.h"

namespace mkxp_retro {
    extern boost::optional<struct mkxp_sandbox::sandbox> sandbox;
    extern boost::optional<Audio> audio;
    extern boost::optional<Input> input;
    extern boost::optional<FileSystem> fs;

    extern retro_log_printf_t log_printf;
    extern retro_video_refresh_t video_refresh;
    extern retro_audio_sample_batch_t audio_sample_batch;
    extern retro_environment_t environment;
    extern retro_input_poll_t input_poll;
    extern retro_input_state_t input_state;
    extern struct retro_perf_callback perf;
}

#endif // MKXPZ_CORE_H
