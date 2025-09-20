/*
** sharedmidistate.h
**
** This file is part of mkxp.
**
** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#ifndef SHAREDMIDISTATE_H
#define SHAREDMIDISTATE_H

#include "config.h"
#include "debugwriter.h"
#include "fluid-fun.h"

#ifdef MKXPZ_RETRO
#  include "core.h"
#endif // MKXPZ_RETRO

#include <assert.h>
#include <vector>
#include <string>

#define SYNTH_INIT_COUNT 2
#define SYNTH_SAMPLERATE 44100

struct Synth
{
	fluid_synth_t *synth;
	bool inUse;
};

struct SharedMidiState
{
	bool inited;
	std::vector<Synth> synths;
#ifndef MKXPZ_RETRO
	const std::string &soundFont;
#endif // MKXPZ_RETRO
	fluid_settings_t *flSettings;

#ifdef MKXPZ_RETRO
	SharedMidiState()
	    : inited(false)
	{}
#else
	SharedMidiState(const Config &conf)
	    : inited(false),
	      soundFont(conf.midi.soundFont)
	{}
#endif // MKXPZ_RETRO

	~SharedMidiState()
	{
		/* We might have initialized, but if the consecutive libfluidsynth
		 * load failed, no resources will have been allocated */
		if (!inited || !HAVE_FLUID)
			return;

		for (size_t i = 0; i < synths.size(); ++i)
		{
			assert(!synths[i].inUse);
			fluid.delete_synth(synths[i].synth);
		}

		fluid.delete_settings(flSettings);
	}

#ifdef MKXPZ_RETRO
	void initIfNeeded(const Config &conf)
	{
		if (inited)
			return;

		inited = true;

		initFluidFunctions();

		if (!HAVE_FLUID)
			return;

		flSettings = fluid.new_settings();
		fluid.settings_setnum(flSettings, "synth.gain", 1.0f);
		fluid.settings_setnum(flSettings, "synth.sample-rate", mkxp_retro::sample_rate);
		fluid.settings_setint(flSettings, "synth.chorus.active", mkxp_retro::midi_chorus_override == 1 || (mkxp_retro::midi_chorus_override != 0 && conf.midi.chorus));
		fluid.settings_setint(flSettings, "synth.reverb.active", mkxp_retro::midi_reverb_override == 1 || (mkxp_retro::midi_reverb_override != 0 && conf.midi.reverb));

		for (size_t i = 0; i < SYNTH_INIT_COUNT; ++i)
			addSynth(false);
	}
#else
	void initIfNeeded(const Config &conf)
	{
		if (inited)
			return;

		inited = true;

		initFluidFunctions();

		if (!HAVE_FLUID)
			return;

		flSettings = fluid.new_settings();
		fluid.settings_setnum(flSettings, "synth.gain", 1.0f);
		fluid.settings_setnum(flSettings, "synth.sample-rate", SYNTH_SAMPLERATE);
		fluid.settings_setint(flSettings, "synth.chorus.active", conf.midi.chorus);
		fluid.settings_setint(flSettings, "synth.reverb.active", conf.midi.reverb);

		for (size_t i = 0; i < SYNTH_INIT_COUNT; ++i)
			addSynth(false);
	}
#endif // MKXPZ_RETRO

	fluid_synth_t *allocateSynth()
	{
		assert(HAVE_FLUID);
		assert(inited);

		size_t i;

		for (i = 0; i < synths.size(); ++i)
			if (!synths[i].inUse)
				break;

		if (i < synths.size())
		{
			fluid_synth_t *syn = synths[i].synth;
			fluid.synth_system_reset(syn);
			synths[i].inUse = true;

			return syn;
		}
		else
		{
			return addSynth(true);
		}
	}

	void releaseSynth(fluid_synth_t *synth)
	{
		size_t i;

		for (i = 0; i < synths.size(); ++i)
			if (synths[i].synth == synth)
				break;

		assert(i < synths.size());

		synths[i].inUse = false;
	}

private:
	fluid_synth_t *addSynth(bool usedNow)
	{
		fluid_synth_t *syn = fluid.new_synth(flSettings);

#ifdef MKXPZ_RETRO
		extern const uint8_t mkxp_gmgsx_sf2[];
		extern const size_t mkxp_gmgsx_sf2_len;

		fluid_sfloader_t *loader = new_fluid_defsfloader(flSettings);
		fluid_sfloader_set_callbacks(
			loader,
			[](const char *filename) {
				return std::strcmp(filename, "/GMGSx.sf2") ? NULL : std::calloc(1, sizeof(fluid_long_long_t));
			},
			[](void *buf, fluid_long_long_t count, void *handle) {
				assert((size_t)(*(fluid_long_long_t *)handle + count) < mkxp_gmgsx_sf2_len);
				std::memcpy(buf, mkxp_gmgsx_sf2 + *(fluid_long_long_t *)handle, count);
				*(fluid_long_long_t *)handle += count;
				return (int)FLUID_OK;
			},
			[](void *handle, fluid_long_long_t offset, int origin) {
				switch (origin) {
					case SEEK_CUR:
						*(fluid_long_long_t *)handle += offset;
						break;
					case SEEK_END:
						*(fluid_long_long_t *)handle = mkxp_gmgsx_sf2_len + offset;
						break;
					default:
						*(fluid_long_long_t *)handle = offset;
						break;
				}
				return (int)FLUID_OK;
			},
			[](void *handle) {
				return *(fluid_long_long_t *)handle;
			},
			[](void *handle) {
				std::free(handle);
				return (int)FLUID_OK;
			}
		);

		fluid_synth_add_sfloader(syn, loader);
		fluid.synth_sfload(syn, "/GMGSx.sf2", 1);
#else
		if (!soundFont.empty())
			fluid.synth_sfload(syn, soundFont.c_str(), 1);
		else
			Debug() << "Warning: No soundfont specified, sound might be mute";
#endif // MKXPZ_RETRO

		Synth synth;
		synth.inUse = usedNow;
		synth.synth = syn;
		synths.push_back(synth);

		return syn;
	}
};

#endif // SHAREDMIDISTATE_H
