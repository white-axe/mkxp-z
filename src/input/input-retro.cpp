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

#include "input.h"
#include "binding-sandbox/core.h"

#define JOYPAD_BUTTON_MAX 16
#define REPEAT_NONE 255
#define REPEAT_START 0.4 // TODO: should be 0.375 when RGSS version >= 2
#define REPEAT_DELAY 0.1
#define FPS 60.0 // TODO: use the actual FPS

static std::unordered_map<int, uint8_t> codeToJoypadId = {
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

struct InputPrivate
{
    uint32_t repeatCount;
    uint16_t currJoypadState;
    uint16_t prevJoypadState;
    uint8_t repeat;
    uint8_t currDir4;
    uint8_t prevDir4;
    uint8_t dir8;
    bool joypadMaskSupported;

    InputPrivate() :
        repeatCount(0),
        currJoypadState(0),
        prevJoypadState(0),
        repeat(-1),
        currDir4(0),
        prevDir4(0),
        dir8(0),
        joypadMaskSupported(mkxp_retro::environment(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
    {}

    void updateJoypad()
    {
        prevJoypadState = currJoypadState;

        if (joypadMaskSupported) {
            currJoypadState = (uint16_t)mkxp_retro::input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        } else {
            currJoypadState = 0;
            for (uint8_t i = 0; i < JOYPAD_BUTTON_MAX; ++i) {
                if (mkxp_retro::input_state(0, RETRO_DEVICE_JOYPAD, 0, i)) {
                    currJoypadState |= (1 << i);
                }
            }
        }

        if (repeat == REPEAT_NONE) {
            if (currJoypadState != 0) {
                for (uint8_t i = 0; i < JOYPAD_BUTTON_MAX; ++i) {
                    if (currJoypadState & (1 << i)) {
                        repeat = i;
                        repeatCount = 0;
                        break;
                    }
                }
            }
        } else if (currJoypadState & (1 << repeat)) {
            __builtin_add_overflow(repeatCount, 1, &repeatCount);
        } else {
            repeat = REPEAT_NONE;
            repeatCount = 0;
        }
    }

    void updateDir4()
    {
        /* Check for dead keys */
        if (((currJoypadState & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN)) && (currJoypadState & (1 << RETRO_DEVICE_ID_JOYPAD_UP))) || ((currJoypadState & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT)) && (currJoypadState & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))))
        {
            currDir4 = 0;
            return;
        }

        if (prevDir4 != 0)
        {
            /* Check if prev still pressed */
            if (currJoypadState & (1 << prevDir4))
            {
                for (size_t i = 0; i < 3; ++i)
                {
                    uint8_t other = otherDirs[prevDir4 - RETRO_DEVICE_ID_JOYPAD_UP][i];

                    if (!(currJoypadState & (1 << other)))
                        continue;

                    currDir4 = other;
                    return;
                }
            }
        }

        for (size_t i = 0; i < 4; ++i)
        {
            if (!(currJoypadState & (1 << (RETRO_DEVICE_ID_JOYPAD_UP + i))))
                continue;

            currDir4 = prevDir4 = RETRO_DEVICE_ID_JOYPAD_UP + i;
            return;
        }

        currDir4 = prevDir4 = 0;
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
            if (!(currJoypadState & (1 << (RETRO_DEVICE_ID_JOYPAD_UP + i))))
                continue;

            for (int j = 0; j < 3; ++j)
            {
                uint8_t other = otherDirs[i][j];

                if (!(currJoypadState & (1 << other)))
                    continue;

                dir8 = combos[i][other - RETRO_DEVICE_ID_JOYPAD_UP];
                return;
            }

            dir8 = RETRO_DEVICE_ID_JOYPAD_UP + i;
            return;
        }
    }

    bool getCurrJoypadState(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && (currJoypadState & (1 << it->second));
    }

    bool getPrevJoypadState(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && (prevJoypadState & (1 << it->second));
    }

    bool isRepeat(int button)
    {
        auto it = codeToJoypadId.find(button);
        return it != codeToJoypadId.end() && repeat == it->second;
    }
};

Input::Input()
{
    p = new InputPrivate();
}

double Input::getDelta()
{
    return 0.0; // TODO
}

void Input::update()
{
    p->updateJoypad();
    p->updateDir4();
    p->updateDir8();
}

bool Input::isPressed(int button)
{
    return p->getCurrJoypadState(button);
}

bool Input::isTriggered(int button)
{
    return p->getCurrJoypadState(button) && !p->getPrevJoypadState(button);
}

bool Input::isReleased(int button)
{
    return p->getPrevJoypadState(button) && !p->getCurrJoypadState(button);
}

bool Input::isRepeated(int button)
{
    return p->isRepeat(button) && (p->repeatCount == 0 || (p->repeatCount >= (size_t)std::ceil(REPEAT_START * FPS) && (p->repeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * FPS) == 0));
}

unsigned int Input::count(int button)
{
    return p->isRepeat(button) ? p->repeatCount : 0;
}

double Input::repeatTime(int button)
{
    return p->isRepeat(button) ? (double)p->repeatCount / FPS : 0;
}

bool Input::isPressedEx(int button, bool isVKey)
{
    return false; // TODO
}

bool Input::isTriggeredEx(int button, bool isVKey)
{
    return false; // TODO
}

bool Input::isReleasedEx(int button, bool isVKey)
{
    return false; // TODO
}

bool Input::isRepeatedEx(int button, bool isVKey)
{
    return false; // TODO
}

unsigned int Input::repeatcount(int button, bool isVKey)
{
    return 0; // TODO
}

double Input::repeatTimeEx(int button, bool isVKey)
{
    return 0.0; // TODO
}

bool Input::controllerIsPressedEx(int button)
{
    return false; // TODO
}

bool Input::controllerIsTriggeredEx(int button)
{
    return false; // TODO
}

bool Input::controllerIsReleasedEx(int button)
{
    return false; // TODO
}

bool Input::controllerIsRepeatedEx(int button)
{
    return false; // TODO
}

double Input::controllerRepeatTimeEx(int button)
{
    return 0.0; // TODO
}

int Input::dir4Value()
{
    switch (p->currDir4) {
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

Input::~Input()
{
    delete p;
}
