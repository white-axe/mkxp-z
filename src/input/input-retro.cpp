/*
 ** input-retro.cpp
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

#include <cstring>
#include "input.h"
#include "core.h"
#include "sharedstate.h"
#include "graphics.h"
#include "mkxp-polyfill.h" // std::lround

#define JOYPAD_BUTTON_MAX 16
#define REPEATING_NONE 255
#define REPEAT_START (rgssVer >= 2 ? 0.375 : 0.400)
#define REPEAT_DELAY 0.1

static const std::unordered_map<int, uint8_t> codeToJoypadId = {
    {Input::Down, RETRO_DEVICE_ID_JOYPAD_DOWN},
    {Input::Left, RETRO_DEVICE_ID_JOYPAD_LEFT},
    {Input::Right, RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {Input::Up, RETRO_DEVICE_ID_JOYPAD_UP},
    {Input::A, RETRO_DEVICE_ID_JOYPAD_X},
    {Input::B, RETRO_DEVICE_ID_JOYPAD_B},
    {Input::C, RETRO_DEVICE_ID_JOYPAD_A},
    {Input::X, RETRO_DEVICE_ID_JOYPAD_Y},
    {Input::Y, RETRO_DEVICE_ID_JOYPAD_L3},
    {Input::Z, RETRO_DEVICE_ID_JOYPAD_R3},
    {Input::L, RETRO_DEVICE_ID_JOYPAD_L},
    {Input::R, RETRO_DEVICE_ID_JOYPAD_R},
    {Input::Shift, RETRO_DEVICE_ID_JOYPAD_L2},
    {Input::Ctrl, RETRO_DEVICE_ID_JOYPAD_R2},
    {Input::Alt, RETRO_DEVICE_ID_JOYPAD_SELECT},
};

static const uint8_t otherDirs[4][3] = {
    { RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_DOWN  }, /* Up    */
    { RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_UP    }, /* Down  */
    { RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_RIGHT }, /* Left  */
    { RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_LEFT  }, /* Right */
};

static const enum retro_key scancodeToRetrok[] = {
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_g,
    RETROK_h,
    RETROK_i,
    RETROK_j,
    RETROK_k,
    RETROK_l,
    RETROK_m,
    RETROK_n,
    RETROK_o,
    RETROK_p,
    RETROK_q,
    RETROK_r,
    RETROK_s,
    RETROK_t,
    RETROK_u,
    RETROK_v,
    RETROK_w,
    RETROK_x,
    RETROK_y,
    RETROK_z,
    RETROK_1,
    RETROK_2,
    RETROK_3,
    RETROK_4,
    RETROK_5,
    RETROK_6,
    RETROK_7,
    RETROK_8,
    RETROK_9,
    RETROK_0,
    RETROK_RETURN,
    RETROK_ESCAPE,
    RETROK_BACKSPACE,
    RETROK_TAB,
    RETROK_SPACE,
    RETROK_MINUS,
    RETROK_EQUALS,
    RETROK_LEFTBRACKET,
    RETROK_RIGHTBRACKET,
    RETROK_BACKSLASH,
    RETROK_BACKSLASH,
    RETROK_SEMICOLON,
    RETROK_QUOTE,
    RETROK_BACKQUOTE,
    RETROK_COMMA,
    RETROK_PERIOD,
    RETROK_SLASH,
    RETROK_CAPSLOCK,
    RETROK_F1,
    RETROK_F2,
    RETROK_F3,
    RETROK_F4,
    RETROK_F5,
    RETROK_F6,
    RETROK_F7,
    RETROK_F8,
    RETROK_F9,
    RETROK_F10,
    RETROK_F11,
    RETROK_F12,
    RETROK_UNKNOWN,
    RETROK_SCROLLOCK,
    RETROK_PAUSE,
    RETROK_INSERT,
    RETROK_HOME,
    RETROK_PAGEUP,
    RETROK_DELETE,
    RETROK_END,
    RETROK_PAGEDOWN,
    RETROK_RIGHT,
    RETROK_LEFT,
    RETROK_DOWN,
    RETROK_UP,
    RETROK_NUMLOCK,
    RETROK_KP_DIVIDE,
    RETROK_KP_MULTIPLY,
    RETROK_KP_MINUS,
    RETROK_KP_PLUS,
    RETROK_KP_ENTER,
    RETROK_KP1,
    RETROK_KP2,
    RETROK_KP3,
    RETROK_KP4,
    RETROK_KP5,
    RETROK_KP6,
    RETROK_KP7,
    RETROK_KP8,
    RETROK_KP9,
    RETROK_KP0,
    RETROK_KP_PERIOD,
    RETROK_BACKSLASH,
    RETROK_COMPOSE,
    RETROK_POWER,
    RETROK_KP_EQUALS,
    RETROK_F13,
    RETROK_F14,
    RETROK_F15,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_HELP,
    RETROK_MENU,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNDO,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_VOLUME_MUTE,
    RETROK_VOLUME_UP,
    RETROK_VOLUME_DOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SYSREQ,
    RETROK_UNKNOWN,
    RETROK_CLEAR,
    RETROK_PAGEUP,
    RETROK_UNKNOWN,
    RETROK_KP_PERIOD,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_LEFTPAREN,
    RETROK_RIGHTPAREN,
    RETROK_LEFTBRACE,
    RETROK_RIGHTBRACE,
    RETROK_TAB,
    RETROK_BACKSPACE,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_UNKNOWN,
    RETROK_CARET,
    RETROK_UNKNOWN,
    RETROK_LESS,
    RETROK_GREATER,
    RETROK_AMPERSAND,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_COLON,
    RETROK_HASH,
    RETROK_SPACE,
    RETROK_AT,
    RETROK_EXCLAIM,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LCTRL,
    RETROK_LSHIFT,
    RETROK_LALT,
    RETROK_LSUPER,
    RETROK_RCTRL,
    RETROK_RSHIFT,
    RETROK_RALT,
    RETROK_RSUPER,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_MEDIA_NEXT,
    RETROK_MEDIA_PREV,
    RETROK_MEDIA_STOP,
    RETROK_MEDIA_PLAY_PAUSE,
    RETROK_VOLUME_MUTE,
    RETROK_LAUNCH_MEDIA,
    RETROK_UNKNOWN,
    RETROK_LAUNCH_MAIL,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_BROWSER_SEARCH,
    RETROK_BROWSER_HOME,
    RETROK_BROWSER_BACK,
    RETROK_BROWSER_FORWARD,
    RETROK_BROWSER_STOP,
    RETROK_BROWSER_REFRESH,
    RETROK_BROWSER_FAVORITES,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LAUNCH_APP1,
    RETROK_LAUNCH_APP2,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
};

#define NUM_SCANCODES (sizeof scancodeToRetrok / sizeof *scancodeToRetrok)

static const enum retro_key vkeyToRetrok[] = {
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // left mouse button; specially handled
    RETROK_UNKNOWN, // right mouse button; specially handled
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // middle mouse button; specially handled
    RETROK_UNKNOWN, // mouse button 4; specially handled
    RETROK_UNKNOWN, // mouse button 5; specially handled
    RETROK_UNKNOWN,
    RETROK_BACKSPACE,
    RETROK_TAB,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_CLEAR,
    RETROK_RETURN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // shift; specially handled to map to both RETROK_LSHIFT and RETROK_RSHIFT
    RETROK_UNKNOWN, // ctrl; specially handled to map to both RETROK_LCTRL and RETROK_RCTRL
    RETROK_UNKNOWN, // alt; specially handled to map to both RETROK_LALT and RETROK_RALT
    RETROK_PAUSE,
    RETROK_CAPSLOCK,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_ESCAPE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SPACE,
    RETROK_PAGEUP,
    RETROK_PAGEDOWN,
    RETROK_END,
    RETROK_HOME,
    RETROK_LEFT,
    RETROK_UP,
    RETROK_RIGHT,
    RETROK_DOWN,
    RETROK_UNKNOWN,
    RETROK_PRINT,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_INSERT,
    RETROK_DELETE,
    RETROK_HELP,
    RETROK_0,
    RETROK_1,
    RETROK_2,
    RETROK_3,
    RETROK_4,
    RETROK_5,
    RETROK_6,
    RETROK_7,
    RETROK_8,
    RETROK_9,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_g,
    RETROK_h,
    RETROK_i,
    RETROK_j,
    RETROK_k,
    RETROK_l,
    RETROK_m,
    RETROK_n,
    RETROK_o,
    RETROK_p,
    RETROK_q,
    RETROK_r,
    RETROK_s,
    RETROK_t,
    RETROK_u,
    RETROK_v,
    RETROK_w,
    RETROK_x,
    RETROK_y,
    RETROK_z,
    RETROK_LSUPER,
    RETROK_RSUPER,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_KP0,
    RETROK_KP1,
    RETROK_KP2,
    RETROK_KP3,
    RETROK_KP4,
    RETROK_KP5,
    RETROK_KP6,
    RETROK_KP7,
    RETROK_KP8,
    RETROK_KP9,
    RETROK_KP_MULTIPLY,
    RETROK_KP_PLUS,
    RETROK_KP_PERIOD,
    RETROK_KP_MINUS,
    RETROK_KP_PERIOD,
    RETROK_KP_DIVIDE,
    RETROK_F1,
    RETROK_F2,
    RETROK_F3,
    RETROK_F4,
    RETROK_F5,
    RETROK_F6,
    RETROK_F7,
    RETROK_F8,
    RETROK_F9,
    RETROK_F10,
    RETROK_F11,
    RETROK_F12,
    RETROK_F13,
    RETROK_F14,
    RETROK_F15,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_NUMLOCK,
    RETROK_SCROLLOCK,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LSHIFT,
    RETROK_RSHIFT,
    RETROK_LCTRL,
    RETROK_RCTRL,
    RETROK_LALT,
    RETROK_RALT,
    RETROK_BROWSER_BACK,
    RETROK_BROWSER_FORWARD,
    RETROK_BROWSER_REFRESH,
    RETROK_BROWSER_STOP,
    RETROK_BROWSER_SEARCH,
    RETROK_BROWSER_FAVORITES,
    RETROK_BROWSER_HOME,
    RETROK_VOLUME_MUTE,
    RETROK_VOLUME_DOWN,
    RETROK_VOLUME_UP,
    RETROK_MEDIA_NEXT,
    RETROK_MEDIA_PREV,
    RETROK_MEDIA_STOP,
    RETROK_MEDIA_PLAY_PAUSE,
    RETROK_LAUNCH_MAIL,
    RETROK_LAUNCH_MEDIA,
    RETROK_LAUNCH_APP1,
    RETROK_LAUNCH_APP2,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SEMICOLON,
    RETROK_PLUS,
    RETROK_COMMA,
    RETROK_MINUS,
    RETROK_PERIOD,
    RETROK_SLASH,
    RETROK_BACKQUOTE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LEFTBRACKET,
    RETROK_BACKSLASH,
    RETROK_RIGHTBRACKET,
    RETROK_QUOTE,
    RETROK_EXCLAIM,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_OEM_102,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_COMPOSE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
};

#define NUM_VKEYS (sizeof vkeyToRetrok / sizeof *vkeyToRetrok)

static const uint8_t controllerButtonToJoypadId[] = {
    RETRO_DEVICE_ID_JOYPAD_A,
    RETRO_DEVICE_ID_JOYPAD_B,
    RETRO_DEVICE_ID_JOYPAD_X,
    RETRO_DEVICE_ID_JOYPAD_Y,
    REPEATING_NONE,
    RETRO_DEVICE_ID_JOYPAD_SELECT,
    RETRO_DEVICE_ID_JOYPAD_START,
    RETRO_DEVICE_ID_JOYPAD_L3,
    RETRO_DEVICE_ID_JOYPAD_R3,
    RETRO_DEVICE_ID_JOYPAD_L,
    RETRO_DEVICE_ID_JOYPAD_R,
    RETRO_DEVICE_ID_JOYPAD_UP,
    RETRO_DEVICE_ID_JOYPAD_DOWN,
    RETRO_DEVICE_ID_JOYPAD_LEFT,
    RETRO_DEVICE_ID_JOYPAD_RIGHT,
    REPEATING_NONE,
    REPEATING_NONE,
    REPEATING_NONE,
    REPEATING_NONE,
    REPEATING_NONE,
    REPEATING_NONE,
};

#define NUM_CONTROLLER_BUTTONS (sizeof controllerButtonToJoypadId / sizeof *controllerButtonToJoypadId)

struct InputPrivate
{
    uint16_t joypadState;
    uint16_t joypadStateOld;
    uint8_t dir4;
    uint8_t dir4Old;
    uint8_t dir8;

    bool keyboardStates[RETROK_LAST];
    bool keyboardStatesOld[RETROK_LAST];

    bool mouseStates[5];
    bool mouseStatesOld[5];
    int16_t mouseX;
    int16_t mouseY;
    uint32_t scrollV;
    bool mouseInWindow;

    uint8_t rawKeyStates[NUM_SCANCODES];
    uint8_t rawButtonStates[NUM_CONTROLLER_BUTTONS];

    uint8_t repeating;
    double repeatTime;
    uint32_t repeatCount;

    enum {
        RAW_REPEATING_NONE,
        RAW_REPEATING_KEYBOARD,
        RAW_REPEATING_MOUSE,
    } rawRepeatingType;
    uint16_t rawRepeating;
    uint32_t rawRepeatCount;
    double rawRepeatTime;

    double last_update;

    bool joypadMaskSupported;

    InputPrivate() :
        joypadState(0),
        joypadStateOld(0),
        dir4(0),
        dir4Old(0),
        dir8(0),
        mouseX(0),
        mouseY(0),
        scrollV(0),
        mouseInWindow(false),
        repeating(REPEATING_NONE),
        repeatTime(0),
        repeatCount(0),
        rawRepeatingType(RAW_REPEATING_NONE),
        rawRepeatCount(0),
        rawRepeatTime(0),
        last_update(0),
        joypadMaskSupported(mkxp_retro::environment(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr))
    {
        std::memset(keyboardStates, 0, sizeof keyboardStates);
        std::memset(keyboardStatesOld, 0, sizeof keyboardStates);
        std::memset(mouseStates, 0, sizeof mouseStates);
        std::memset(mouseStatesOld, 0, sizeof mouseStates);
        std::memset(rawKeyStates, 0, sizeof rawKeyStates);
        std::memset(rawButtonStates, 0, sizeof rawButtonStates);
    }

    void updateJoypad()
    {
        joypadStateOld = joypadState;

        if (joypadMaskSupported) {
            joypadState = (uint16_t)mkxp_retro::input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        } else {
            joypadState = 0;
            for (uint8_t i = 0; i < JOYPAD_BUTTON_MAX; ++i) {
                if (mkxp_retro::input_state(0, RETRO_DEVICE_JOYPAD, 0, i)) {
                    joypadState |= (1 << i);
                }
            }
        }
    }

    void updateDir4()
    {
        /* Check for dead keys */
        if (((joypadState & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)) && (joypadState & (1 << RETRO_DEVICE_ID_JOYPAD_UP))) || ((joypadState & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) && (joypadState & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))))
        {
            dir4 = 0;
            return;
        }

        if (dir4Old != 0)
        {
            /* Check if prev still pressed */
            if (joypadState & (1 << dir4Old))
            {
                for (size_t i = 0; i < 3; ++i)
                {
                    uint8_t other = otherDirs[dir4Old - RETRO_DEVICE_ID_JOYPAD_UP][i];

                    if (!(joypadState & (1 << other)))
                        continue;

                    dir4 = other;
                    return;
                }
            }
        }

        for (size_t i = 0; i < 4; ++i)
        {
            if (!(joypadState & (1 << (RETRO_DEVICE_ID_JOYPAD_UP + i))))
                continue;

            dir4 = dir4Old = RETRO_DEVICE_ID_JOYPAD_UP + i;
            return;
        }

        dir4 = dir4Old = 0;
    }

    void updateDir8()
    {
        static const int combos[4][4] =
        {
            { 8, 0, 7, 9 },
            { 0, 2, 1, 3 },
            { 7, 1, 4, 0 },
            { 9, 3, 0, 6 },
        };

        dir8 = 0;

        for (size_t i = 0; i < 4; ++i)
        {
            if (!(joypadState & (1 << (RETRO_DEVICE_ID_JOYPAD_UP + i))))
                continue;

            for (int j = 0; j < 3; ++j)
            {
                uint8_t other = otherDirs[i][j];

                if (!(joypadState & (1 << other)))
                    continue;

                dir8 = combos[i][other - RETRO_DEVICE_ID_JOYPAD_UP];
                return;
            }

            dir8 = RETRO_DEVICE_ID_JOYPAD_UP + i;
            return;
        }
    }

    void updateKeyboard()
    {
        std::memcpy(keyboardStatesOld, keyboardStates, sizeof keyboardStates);
        std::memcpy(keyboardStates, mkxp_retro::keyboard_state, sizeof keyboardStates);
    }

    void updateMouse()
    {
        std::memcpy(mouseStatesOld, mouseStates, sizeof mouseStates);

        mouseStates[0] = mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
        mouseStates[1] = mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
        mouseStates[2] = mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
        mouseStates[3] = mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4);
        mouseStates[4] = mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_5);

        mouseX = (int16_t)mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        mouseY = (int16_t)mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
        mouseInWindow = !mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN);
        scrollV += mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
        scrollV -= mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
    }

    void updateRawKeyStates()
    {
        for (uint16_t i = 0; i < NUM_SCANCODES; ++i)
        {
            rawKeyStates[i] = keyboardStates[scancodeToRetrok[i]];
        }
    }

    void updateRawButtonStates()
    {
        for (uint8_t i = 0; i < NUM_CONTROLLER_BUTTONS; ++i)
        {
            rawButtonStates[i] = controllerButtonToJoypadId[i] != REPEATING_NONE && joypadState & (1 << controllerButtonToJoypadId[i]);
        }
    }

    void updateRepeat()
    {
        for (uint8_t i = 0; i < JOYPAD_BUTTON_MAX; ++i)
        {
            /* Check for new repeating joypad key */
            if (joypadState & (1 << i) && !(joypadStateOld & (1 << i)) && i != repeating)
            {
                repeating = i;
                repeatCount = 0;
                repeatTime = shState->runTime();
                return;
            }
        }

        /* Check if repeating joypad key still pressed */
        if (repeating < JOYPAD_BUTTON_MAX && joypadState & (1 << repeating))
        {
            ++repeatCount;
            return;
        }

        repeating = REPEATING_NONE;
    }

    void updateRawRepeat()
    {
        for (uint16_t i = 0; i < RETROK_LAST; ++i)
        {
            /* Check for new repeating keyboard key */
            if (keyboardStates[i] && !keyboardStatesOld[i] && (rawRepeatingType != RAW_REPEATING_KEYBOARD || i != rawRepeating))
            {
                rawRepeatingType = RAW_REPEATING_KEYBOARD;
                rawRepeating = i;
                rawRepeatCount = 0;
                rawRepeatTime = shState->runTime();
                return;
            }
        }

        for (uint8_t i = 0; i < 5; ++i)
        {
            /* Check for new repeating mouse button */
            if (mouseStates[i] && !mouseStatesOld[i] && (rawRepeatingType != RAW_REPEATING_MOUSE || i != rawRepeating))
            {
                rawRepeatingType = RAW_REPEATING_MOUSE;
                rawRepeating = i;
                rawRepeatCount = 0;
                rawRepeatTime = shState->runTime();
                return;
            }
        }

        /* Check if repeating keyboard key still pressed */
        if (rawRepeatingType == RAW_REPEATING_KEYBOARD && keyboardStates[rawRepeating])
        {
            ++rawRepeatCount;
            return;
        }

        /* Check if repeating mouse button still pressed */
        if (rawRepeatingType == RAW_REPEATING_MOUSE && mouseStates[rawRepeating])
        {
            ++rawRepeatCount;
            return;
        }

        rawRepeatingType = RAW_REPEATING_NONE;
    }

    bool getState(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && (joypadState & (1 << it->second));
    }

    bool getStateOld(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && (joypadStateOld & (1 << it->second));
    }

    bool isRepeat(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && repeating == it->second;
    }

    bool getRawState(int button, bool isVKey)
    {
        if (button < 0 || button >= (isVKey ? NUM_VKEYS : NUM_SCANCODES))
            return false;
        else if (!isVKey)
            return keyboardStates[scancodeToRetrok[button]];
        else if (button == 1)
            return mouseStates[0];
        else if (button == 2)
            return mouseStates[1];
        else if (button == 4)
            return mouseStates[2];
        else if (button == 5)
            return mouseStates[3];
        else if (button == 6)
            return mouseStates[4];
        else if (button == 16)
            return keyboardStates[RETROK_LSHIFT] || keyboardStates[RETROK_RSHIFT];
        else if (button == 17)
            return keyboardStates[RETROK_LCTRL] || keyboardStates[RETROK_RCTRL];
        else if (button == 18)
            return keyboardStates[RETROK_LALT] || keyboardStates[RETROK_RALT];
        else
            return keyboardStates[vkeyToRetrok[button]];
    }

    bool getRawStateOld(int button, bool isVKey)
    {
        if (button < 0 || button >= (isVKey ? NUM_VKEYS : NUM_SCANCODES))
            return false;
        else if (!isVKey)
            return keyboardStatesOld[scancodeToRetrok[button]];
        else if (button == 1)
            return mouseStatesOld[0];
        else if (button == 2)
            return mouseStatesOld[1];
        else if (button == 4)
            return mouseStatesOld[2];
        else if (button == 5)
            return mouseStatesOld[3];
        else if (button == 6)
            return mouseStatesOld[4];
        else if (button == 16)
            return keyboardStatesOld[RETROK_LSHIFT] || keyboardStatesOld[RETROK_RSHIFT];
        else if (button == 17)
            return keyboardStatesOld[RETROK_LCTRL] || keyboardStatesOld[RETROK_RCTRL];
        else if (button == 18)
            return keyboardStatesOld[RETROK_LALT] || keyboardStatesOld[RETROK_RALT];
        else
            return keyboardStatesOld[vkeyToRetrok[button]];
    }

    bool isRawRepeat(int button, bool isVKey)
    {
        if (rawRepeatingType == RAW_REPEATING_NONE || button < 0 || button >= (isVKey ? NUM_VKEYS : NUM_SCANCODES))
            return false;
        else if (!isVKey)
            return rawRepeatingType == RAW_REPEATING_KEYBOARD && rawRepeating == scancodeToRetrok[button];
        else if (button == 1)
            return rawRepeatingType == RAW_REPEATING_MOUSE && rawRepeating == 0;
        else if (button == 2)
            return rawRepeatingType == RAW_REPEATING_MOUSE && rawRepeating == 1;
        else if (button == 4)
            return rawRepeatingType == RAW_REPEATING_MOUSE && rawRepeating == 2;
        else if (button == 5)
            return rawRepeatingType == RAW_REPEATING_MOUSE && rawRepeating == 3;
        else if (button == 6)
            return rawRepeatingType == RAW_REPEATING_MOUSE && rawRepeating == 4;
        else if (button == 16)
            return rawRepeatingType == RAW_REPEATING_KEYBOARD && (rawRepeating == RETROK_LSHIFT || rawRepeating == RETROK_RSHIFT);
        else if (button == 17)
            return rawRepeatingType == RAW_REPEATING_KEYBOARD && (rawRepeating == RETROK_LCTRL || rawRepeating == RETROK_RCTRL);
        else if (button == 18)
            return rawRepeatingType == RAW_REPEATING_KEYBOARD && (rawRepeating == RETROK_LALT || rawRepeating == RETROK_RALT);
        else
            return rawRepeatingType == RAW_REPEATING_KEYBOARD && rawRepeating == vkeyToRetrok[button];
    }

    bool getControllerState(int button)
    {
        if (button < 0 || button >= NUM_CONTROLLER_BUTTONS || controllerButtonToJoypadId[button] == REPEATING_NONE)
            return false;
        else
            return joypadState & (1 << controllerButtonToJoypadId[button]);
    }

    bool getControllerStateOld(int button)
    {
        if (button < 0 || button >= NUM_CONTROLLER_BUTTONS || controllerButtonToJoypadId[button] == REPEATING_NONE)
            return false;
        else
            return joypadStateOld & (1 << controllerButtonToJoypadId[button]);
    }

    bool isControllerRepeat(int button)
    {
        if (button < 0 || button >= NUM_CONTROLLER_BUTTONS || controllerButtonToJoypadId[button] == REPEATING_NONE)
            return false;
        else
            return repeating == controllerButtonToJoypadId[button];
    }
};

Input::Input()
{
    p = new InputPrivate();
}

Input::~Input()
{
    delete p;
}

void Input::recalcRepeat(unsigned int fps)
{

}

double Input::getDelta()
{
    return shState->runTime() - p->last_update;
}

void Input::update()
{
    mkxp_retro::input_poll();
    mkxp_retro::input_polled = true;
    p->updateJoypad();
    p->updateDir4();
    p->updateDir8();
    p->updateKeyboard();
    p->updateMouse();
    p->updateRawKeyStates();
    p->updateRawButtonStates();
    p->updateRepeat();
    p->updateRawRepeat();
    p->last_update = shState->runTime();
}

bool Input::isPressed(int button)
{
    return p->getState(button);
}

bool Input::isTriggered(int button)
{
    return p->getState(button) && !p->getStateOld(button);
}

bool Input::isReleased(int button)
{
    return p->getStateOld(button) && !p->getState(button);
}

bool Input::isRepeated(int button)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isRepeat(button) && (p->repeatCount == 0 || (p->repeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->repeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::count(int button)
{
    return p->isRepeat(button) ? p->repeatCount : 0;
}

double Input::repeatTime(int button)
{
    return p->isRepeat(button) ? shState->runTime() - p->repeatTime : 0;
}

bool Input::isPressedEx(int button, bool isVKey)
{
    return p->getRawState(button, isVKey);
}

bool Input::isTriggeredEx(int button, bool isVKey)
{
    return p->getRawState(button, isVKey) && !p->getRawStateOld(button, isVKey);
}

bool Input::isReleasedEx(int button, bool isVKey)
{
    return p->getRawStateOld(button, isVKey) && !p->getRawState(button, isVKey);
}

bool Input::isRepeatedEx(int button, bool isVKey)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isRawRepeat(button, isVKey) && (p->rawRepeatCount == 0 || (p->rawRepeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->rawRepeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::repeatcount(int button, bool isVKey)
{
    return p->isRawRepeat(button, isVKey) ? p->rawRepeatCount : 0;
}

double Input::repeatTimeEx(int button, bool isVKey)
{
    return p->isRawRepeat(button, isVKey) ? shState->runTime() - p->rawRepeatTime : 0;
}

bool Input::controllerIsPressedEx(int button)
{
    return p->getControllerState(button);
}

bool Input::controllerIsTriggeredEx(int button)
{
    return p->getControllerState(button) && !p->getControllerStateOld(button);
}

bool Input::controllerIsReleasedEx(int button)
{
    return p->getControllerStateOld(button) && !p->getControllerState(button);
}

bool Input::controllerIsRepeatedEx(int button)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isControllerRepeat(button) && (p->repeatCount == 0 || (p->repeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->repeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::controllerRepeatcount(int button)
{
    return p->isControllerRepeat(button) ? p->repeatCount : 0;
}

double Input::controllerRepeatTimeEx(int button)
{
    return p->isControllerRepeat(button) ? shState->runTime() - p->repeatTime : 0;
}

uint8_t *Input::rawKeyStates()
{
    return p->rawKeyStates;
}

unsigned int Input::rawKeyStatesLength()
{
    return NUM_SCANCODES;
}

uint8_t *Input::rawButtonStates()
{
    return p->rawButtonStates;
}

unsigned int Input::rawButtonStatesLength()
{
    return NUM_CONTROLLER_BUTTONS;
}

int16_t *Input::rawAxes()
{
    return nullptr; // TODO
}

unsigned int Input::rawAxesLength()
{
    return 0; // TODO
}

short Input::getControllerAxisValue(SDL_GameControllerAxis axis)
{
    return 0; // TODO
}

int Input::dir4Value()
{
    switch (p->dir4) {
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return Input::Down;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return Input::Left;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return Input::Right;
        case RETRO_DEVICE_ID_JOYPAD_UP:
            return Input::Up;
        default:
            return Input::None;
    }
}

int Input::dir8Value()
{
    return p->dir8;
}

int Input::mouseX()
{
    uint16_t x = clamp(p->mouseX, (int16_t)-32767, (int16_t)32767);
    x += 32767;
    return (int)std::lround(((float)x / 65534.0f) * shState->graphics().width());
}

int Input::mouseY()
{
    uint16_t y = clamp(p->mouseY, (int16_t)-32767, (int16_t)32767);
    y += 32767;
    return (int)std::lround(((float)y / 65534.0f) * shState->graphics().height());
}

int Input::scrollV()
{
    return (int)p->scrollV;
}

bool Input::mouseInWindow()
{
    return p->mouseInWindow;
}

bool Input::getControllerConnected()
{
    return true;
}

const char *Input::getControllerName()
{
    return "RetroPad";
}

int Input::getControllerPowerLevel()
{
    return (int)SDL_JOYSTICK_POWER_UNKNOWN;
}

bool Input::getTextInputMode()
{
    return false;
}

void Input::setTextInputMode(bool mode)
{

}

const char *Input::getText()
{
    return "";
}

void Input::clearText()
{

}

std::string Input::getClipboardText()
{
    return {};
}

void Input::setClipboardText(const char *text)
{

}
