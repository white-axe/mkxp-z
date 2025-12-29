/*
** core-options.h
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

#ifndef MKXPZ_CORE_OPTIONS_H
#define MKXPZ_CORE_OPTIONS_H

#include <libretro.h>

static const struct retro_core_option_v2_category core_option_categories[] = {
    {
        "runtime",
        "Runtime",
        nullptr,
    },
    {
        "video",
        "Video",
        nullptr,
    },
    {
        "audio",
        "Audio",
        nullptr,
    },
    {
        "text",
        "Text",
        nullptr,
    },
    {
        nullptr,
        nullptr,
        nullptr,
    },
};

static const struct retro_core_option_v2_definition core_option_definitions[] = {
    {
        "mkxp-z_rgssVersion",
        "RGSS Version",
        nullptr,
        (
            "Specify the RGSS version to run under."
            " By default, mkxp will try to guess the required version"
            " based on the game files."
            " If this fails, the version defaults to 1."
            " Changes will take effect after the core is reset."
        ),
        nullptr,
        "runtime",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"1", "1 (RPG Maker XP)"},
            {"2", "2 (RPG Maker VX)"},
            {"3", "3 (RPG Maker VX Ace)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_saveStateSize",
        "Save State Size",
        nullptr,
        (
            "Maximum size of each save state, in mebibytes."
            " If the game uses more than this much memory, save state creation will fail."
            " Changes to this setting will not take effect until the core is unloaded."
        ),
        nullptr,
        "runtime",
        {
            {"64", "64"},
            {"66", "66"},
            {"68", "68"},
            {"70", "70"},
            {"72", "72"},
            {"74", "74"},
            {"76", "76"},
            {"78", "78"},
            {"80", "80"},
            {"82", "82"},
            {"84", "84"},
            {"86", "86"},
            {"88", "88"},
            {"90", "90"},
            {"92", "92"},
            {"94", "94"},
            {"96", "96"},
            {"98", "98"},
            {"100", "100"},
            {"102", "102"},
            {"104", "104"},
            {"106", "106"},
            {"108", "108"},
            {"110", "110"},
            {"112", "112"},
            {"114", "114"},
            {"116", "116"},
            {"118", "118"},
            {"120", "120"},
            {"122", "122"},
            {"124", "124"},
            {"126", "126"},
            {"128", "128"},
            {"132", "132"},
            {"136", "136"},
            {"140", "140"},
            {"144", "144"},
            {"148", "148"},
            {"152", "152"},
            {"156", "156"},
            {"160", "160"},
            {"164", "164"},
            {"168", "168"},
            {"172", "172"},
            {"176", "176"},
            {"180", "180"},
            {"184", "184"},
            {"188", "188"},
            {"192", "192"},
            {"196", "196"},
            {"200", "200"},
            {"204", "204"},
            {"208", "208"},
            {"212", "212"},
            {"216", "216"},
            {"220", "220"},
            {"224", "224"},
            {"228", "228"},
            {"232", "232"},
            {"236", "236"},
            {"240", "240"},
            {"244", "244"},
            {"248", "248"},
            {"252", "252"},
            {"256", "256"},
            {"264", "264"},
            {"272", "272"},
            {"280", "280"},
            {"288", "288"},
            {"296", "296"},
            {"304", "304"},
            {"312", "312"},
            {"320", "320"},
            {"328", "328"},
            {"336", "336"},
            {"344", "344"},
            {"352", "352"},
            {"360", "360"},
            {"368", "368"},
            {"376", "376"},
            {"384", "384"},
            {"392", "392"},
            {"400", "400"},
            {"408", "408"},
            {"416", "416"},
            {"424", "424"},
            {"432", "432"},
            {"440", "440"},
            {"448", "448"},
            {"456", "456"},
            {"464", "464"},
            {"472", "472"},
            {"480", "480"},
            {"488", "488"},
            {"496", "496"},
            {"504", "504"},
            {"512", "512"},
            {"528", "528"},
            {"544", "544"},
            {"560", "560"},
            {"576", "576"},
            {"592", "592"},
            {"608", "608"},
            {"624", "624"},
            {"640", "640"},
            {"656", "656"},
            {"672", "672"},
            {"688", "688"},
            {"704", "704"},
            {"720", "720"},
            {"736", "736"},
            {"752", "752"},
            {"768", "768"},
            {"784", "784"},
            {"800", "800"},
            {"816", "816"},
            {"832", "832"},
            {"848", "848"},
            {"864", "864"},
            {"880", "880"},
            {"896", "896"},
            {"912", "912"},
            {"928", "928"},
            {"944", "944"},
            {"960", "960"},
            {"976", "976"},
            {"992", "992"},
            {nullptr, nullptr},
        },
        "100",
    },
    {
        "mkxp-z_frameSkip",
        "Frame Skip",
        nullptr,
        (
            "Skip (don't draw) frames when behind."
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "disabled",
    },
    {
        "mkxp-z_subImageFix",
        "Subimage Fix",
        nullptr,
        (
            "Work around buggy graphics drivers which don't"
            " properly synchronize texture access, most"
            " apparent when text doesn't show up or the map"
            " tileset doesn't render at all."
            " (default: enabled for systems using OpenGL ES, disabled on other systems)"
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_enableBlitting",
        "Framebuffer Blitting",
        nullptr,
        (
            "Enable framebuffer blitting if the driver is"
            " capable of it. Some drivers carry buggy"
            " implementations of this functionality, so"
            " disabling it can be used as a workaround."
            " (default: disabled on Windows, enabled on other systems)"
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_threadedAudio",
        "Threaded Audio",
        nullptr,
        (
            "Use a worker thread for rendering the audio instead of"
            " rendering in the main thread, if possible. Reduces audio"
            " crackling, especially on systems with slow file system"
            " access speed. Changes to this setting will not take effect"
            " until the game is closed."
        ),
        nullptr,
        "audio",
        {
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "enabled",
    },
    {
        "mkxp-z_midiChorus",
        "MIDI Chorus",
        nullptr,
        (
            "Activate \"chorus\" effect for midi playback."
        ),
        nullptr,
        "audio",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_midiReverb",
        "MIDI Reverb",
        nullptr,
        (
            "Activate \"reverb\" effect for midi playback."
        ),
        nullptr,
        "audio",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_SESourceCount",
        "SE Source Count",
        nullptr,
        (
            "Number of OpenAL sources to allocate for SE playback."
            " If there are a lot of sounds playing at the same time"
            " and audibly cutting each other off, try increasing"
            " this number."
            " Changes will take effect after the core is reset."
            " (if this value is also set in the game's mkxp.json,"
            " the maximum of the value set here and the value in"
            " mkxp.json will be used)"
        ),
        nullptr,
        "audio",
        {
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
        "6",
    },
    {
        "mkxp-z_fontScale",
        "Font Scale",
        nullptr,
        (

            "Scales the sizes of all fonts."
            " If you think text tends to be too large or too small,"
            " try fiddling with this."
            " (if this value is also set in the game's mkxp.json,"
            " the product of the value set here and the value in"
            " mkxp.json will be used)"
        ),
        nullptr,
        "text",
        {
            {"0.2", "0.2"},
            {"0.25", "0.25"},
            {"0.3", "0.3"},
            {"0.35", "0.35"},
            {"0.4", "0.4"},
            {"0.45", "0.45"},
            {"0.5", "0.5"},
            {"0.55", "0.55"},
            {"0.6", "0.6"},
            {"0.65", "0.65"},
            {"0.7", "0.7"},
            {"0.75", "0.75"},
            {"0.8", "0.8"},
            {"0.85", "0.85"},
            {"0.9", "0.9"},
            {"0.95", "0.95"},
            {"1.0", "1.0"},
            {"1.05", "1.05"},
            {"1.1", "1.1"},
            {"1.15", "1.15"},
            {"1.2", "1.2"},
            {"1.25", "1.25"},
            {"1.3", "1.3"},
            {"1.35", "1.35"},
            {"1.4", "1.4"},
            {"1.45", "1.45"},
            {"1.5", "1.5"},
            {"1.55", "1.55"},
            {"1.6", "1.6"},
            {"1.65", "1.65"},
            {"1.7", "1.7"},
            {"1.75", "1.75"},
            {"1.8", "1.8"},
            {"1.85", "1.85"},
            {"1.9", "1.9"},
            {"1.95", "1.95"},
            {"2.0", "2.0"},
            {"2.05", "2.05"},
            {"2.1", "2.1"},
            {"2.15", "2.15"},
            {"2.2", "2.2"},
            {"2.25", "2.25"},
            {"2.3", "2.3"},
            {"2.35", "2.35"},
            {"2.4", "2.4"},
            {"2.45", "2.45"},
            {"2.5", "2.5"},
            {"2.55", "2.55"},
            {"2.6", "2.6"},
            {"2.65", "2.65"},
            {"2.7", "2.7"},
            {"2.75", "2.75"},
            {"2.8", "2.8"},
            {"2.85", "2.85"},
            {"2.9", "2.9"},
            {"2.95", "2.95"},
            {"3.0", "3.0"},
            {"3.05", "3.05"},
            {"3.1", "3.1"},
            {"3.15", "3.15"},
            {"3.2", "3.2"},
            {"3.25", "3.25"},
            {"3.3", "3.3"},
            {"3.35", "3.35"},
            {"3.4", "3.4"},
            {"3.45", "3.45"},
            {"3.5", "3.5"},
            {"3.55", "3.55"},
            {"3.6", "3.6"},
            {"3.65", "3.65"},
            {"3.7", "3.7"},
            {"3.75", "3.75"},
            {"3.8", "3.8"},
            {"3.85", "3.85"},
            {"3.9", "3.9"},
            {"3.95", "3.95"},
            {"4.0", "4.0"},
            {"4.05", "4.05"},
            {"4.1", "4.1"},
            {"4.15", "4.15"},
            {"4.2", "4.2"},
            {"4.25", "4.25"},
            {"4.3", "4.3"},
            {"4.35", "4.35"},
            {"4.4", "4.4"},
            {"4.45", "4.45"},
            {"4.5", "4.5"},
            {"4.55", "4.55"},
            {"4.6", "4.6"},
            {"4.65", "4.65"},
            {"4.7", "4.7"},
            {"4.75", "4.75"},
            {"4.8", "4.8"},
            {"4.85", "4.85"},
            {"4.9", "4.9"},
            {"4.95", "4.95"},
            {"5.0", "5.0"},
            {nullptr, nullptr},
        },
        "1.0",
    },
    {
        "mkxp-z_fontKerning",
        "Kerning",
        nullptr,
        (
            "Kerning adjusts the spacing between individual letters or characters."
            " Enabling it generally looks nicer, but RGSS doesn't use it,"
            " so disabling it should make text appearance more accurate."
            " (default: enabled)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontHinting",
        "Font Hinting",
        nullptr,
        (
            "Hinting adjusts the rendering of individual letters or characters."
            " Enabling it may look nicer (especially on low-resolution displays), but"
            " RGSS doesn't use it, so disabling it should make text appearance more"
            " accurate. Documentation can be found at:"
            " https://pysdl2.readthedocs.io/en/latest/modules/sdl2_sdlttf.html#sdl2.sdlttf.TTF_HINTING_NORMAL"
            " (default: 3)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"0", "0 (Normal)"},
            {"1", "1 (Light)"},
            {"2", "2 (Mono)"},
            {"3", "3 (None)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontHeightReporting",
        "Font Height Reporting",
        nullptr,
        (
            "Controls the algorithm for reporting the height of rendered text."
            " 0: Nominal (TTF_FontHeight); matches RGSS behavior; may cut off bottoms of some characters."
            " 1: Rendered (TTF_SizeUTF8); deviates from RGSS; may look better."
            " (default: 0)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"0", "0 (Nominal)"},
            {"1", "1 (Rendered)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontOutlineCrop",
        "Outline Crop",
        nullptr,
        (
            "Crops top row and left column of text that has an outline."
            " Disabling it generally looks nicer, but RGSS enables it, so enabling it"
            " should make text appearance more accurate."
            " (default: enabled)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        {{nullptr, nullptr}},
        nullptr,
    },
};

#endif // MKXPZ_CORE_OPTIONS_H
