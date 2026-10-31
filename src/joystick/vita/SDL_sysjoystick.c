/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_VITA

// This is the PSVita implementation of the SDL joystick API
#include <psp2/types.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>

#include <stdio.h> // For the definition of NULL
#include <stdlib.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"


struct SDL_JoystickDeviceData
{
    int index;
};

struct joystick_hwdata
{
    int index;

    // Current pad state
    SceCtrlData pad;

    // Previous pad state
    unsigned int old_buttons;
    unsigned char old_lx;
    unsigned char old_ly;
    unsigned char old_rx;
    unsigned char old_ry;
    unsigned char old_lt;
    unsigned char old_rt;
};

static int ext_port_map[4] = { 1, 2, 3, 4 }; // index: SDL joy number, entry: Vita port number. For external controllers

#define VITA_HAT_MASK (SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_UP | SCE_CTRL_RIGHT)  // we offer the dpad as a hat switch.

static const unsigned int ext_button_map[] = {
    SCE_CTRL_TRIANGLE,
    SCE_CTRL_CIRCLE,
    SCE_CTRL_CROSS,
    SCE_CTRL_SQUARE,
    SCE_CTRL_L1,
    SCE_CTRL_R1,
    SCE_CTRL_SELECT,
    SCE_CTRL_START,
    SCE_CTRL_L2,
    SCE_CTRL_R2,
    SCE_CTRL_L3,
    SCE_CTRL_R3
};

static int analog_map[256]; // Map analog inputs to -32768 -> 32767

// 4 points define the bezier-curve.
// The Vita has a good amount of analog travel, so use a linear curve
static SDL_Point a = { 0, 0 };
static SDL_Point b = { 0, 0 };
static SDL_Point c = { 128, 32767 };
static SDL_Point d = { 128, 32767 };

// simple linear interpolation between two points
static SDL_INLINE void lerp(SDL_Point *dest, const SDL_Point *first, const SDL_Point *second, float t)
{
    dest->x = first->x + (int)((second->x - first->x) * t);
    dest->y = first->y + (int)((second->y - first->y) * t);
}

// evaluate a point on a bezier-curve. t goes from 0 to 1.0
static int calc_bezier_y(float t)
{
    SDL_Point ab, bc, cd, abbc, bccd, dest;
    lerp(&ab, &a, &b, t);         // point between a and b
    lerp(&bc, &b, &c, t);         // point between b and c
    lerp(&cd, &c, &d, t);         // point between c and d
    lerp(&abbc, &ab, &bc, t);     // point between ab and bc
    lerp(&bccd, &bc, &cd, t);     // point between bc and cd
    lerp(&dest, &abbc, &bccd, t); // point on the bezier-curve
    return dest.y;
}

static bool VITA_AddJoystick(int index)
{
    const char *name = "PSVita Controller";

    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_VITA_JoystickDriver, sizeof(SDL_JoystickDeviceData));
    if (!device) {
        return false;
    }
    device->internal = (SDL_JoystickDeviceData *)(device + 1);
    device->internal->index = index;
    device->name = SDL_strdup(name);
    device->guid = SDL_CreateJoystickGUIDForName(name);

    SDL_AddJoystickDevice(device, false);
    return true;
}

static bool VITA_JoystickInit(void)
{
    int i;
    int index = 0;
    SceCtrlPortInfo myPortInfo;

    // Setup input
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);

    /* Create an accurate map from analog inputs (0 to 255)
       to SDL joystick positions (-32768 to 32767) */
    for (i = 0; i < 128; i++) {
        float t = (float)i / 127.0f;
        analog_map[i + 128] = calc_bezier_y(t);
        analog_map[127 - i] = -1 * analog_map[i + 128];
    }

    // Assume we have at least one controller, even when nothing is paired
    // This way the user can jump in, pair a controller
    // and control things immediately even if it is paired
    // after the app has already started.
    if (!VITA_AddJoystick(index++)) {
        return false;
    }

    // How many additional paired controllers are there?
    sceCtrlGetControllerPortInfo(&myPortInfo);

    // On Vita TV, port 0 and 1 are the same controller
    // and that is the first one, so start at port 2
    for (i = 2; i <= 4; i++) {
        if (myPortInfo.port[i] != SCE_CTRL_TYPE_UNPAIRED) {
            if (!VITA_AddJoystick(index++)) {
                return false;
            }
        }
    }
    return true;
}

static bool VITA_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->hwdata->index = device->internal->index;

    joystick->nbuttons = SDL_arraysize(ext_button_map);
    joystick->naxes = 6;
    joystick->nhats = 1;  // we treat the dpad as a hat switch, even though its just buttons.

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, true);
    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);

    return true;
}

static void VITA_JoystickUpdate(SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    int i;
    unsigned int buttons;
    unsigned int changed;
    unsigned char lx, ly, rx, ry, lt, rt;
    SceCtrlData *pad = &hwdata->pad;
    Uint64 timestamp = SDL_GetTicksNS();

    int index = hwdata->index;

    if (index == 0) {
        if (sceCtrlPeekBufferPositive2(ext_port_map[index], pad, 1) < 0) {
            // on vita fallback to port 0
            sceCtrlPeekBufferPositive2(0, pad, 1);
        }
    } else {
        sceCtrlPeekBufferPositive2(ext_port_map[index], pad, 1);
    }

    buttons = pad->buttons;

    lx = pad->lx;
    ly = pad->ly;
    rx = pad->rx;
    ry = pad->ry;
    lt = pad->lt;
    rt = pad->rt;

    // Axes

    if (hwdata->old_lx != lx) {
        SDL_SendJoystickAxis(timestamp, joystick, 0, analog_map[lx]);
        hwdata->old_lx = lx;
    }
    if (hwdata->old_ly != ly) {
        SDL_SendJoystickAxis(timestamp, joystick, 1, analog_map[ly]);
        hwdata->old_ly = ly;
    }
    if (hwdata->old_rx != rx) {
        SDL_SendJoystickAxis(timestamp, joystick, 2, analog_map[rx]);
        hwdata->old_rx = rx;
    }
    if (hwdata->old_ry != ry) {
        SDL_SendJoystickAxis(timestamp, joystick, 3, analog_map[ry]);
        hwdata->old_ry = ry;
    }

    if (hwdata->old_lt != lt) {
        SDL_SendJoystickAxis(timestamp, joystick, 4, analog_map[lt]);
        hwdata->old_lt = lt;
    }
    if (hwdata->old_rt != rt) {
        SDL_SendJoystickAxis(timestamp, joystick, 5, analog_map[rt]);
        hwdata->old_rt = rt;
    }

    // Buttons
    changed = hwdata->old_buttons ^ buttons;
    hwdata->old_buttons = buttons;

    if (changed & ~VITA_HAT_MASK) {
        for (i = 0; i < SDL_arraysize(ext_button_map); i++) {
            if (changed & ext_button_map[i]) {
                const bool down = ((buttons & ext_button_map[i]) != 0);
                SDL_SendJoystickButton(timestamp, joystick, i, down);
            }
        }
    }

    if (changed & VITA_HAT_MASK) {
        // The Vita dpad looks like 4 buttons at this level, but we treat it as a hat switch, so apps that are talking to SDL_Joystick can hope to do basic directional things without a configuration step.
        // (but they should _really_ be using the gamepad API.)
        Uint8 hat = SDL_HAT_CENTERED;
        #define HATSTATE(name) if (buttons & SCE_CTRL_##name) { hat |= SDL_HAT_##name; }
        HATSTATE(UP);
        HATSTATE(RIGHT);
        HATSTATE(DOWN);
        HATSTATE(LEFT);
        #undef HATSTATE

        // this is a physical d-pad on the device, so it probably _can't_ send opposing buttons at the same time, but just in case, cancel them out.
        if ((hat & (SDL_HAT_UP|SDL_HAT_DOWN)) == (SDL_HAT_UP|SDL_HAT_DOWN)) {
            hat &= ~(SDL_HAT_UP|SDL_HAT_DOWN);
        }
        if ((hat & (SDL_HAT_LEFT|SDL_HAT_RIGHT)) == (SDL_HAT_LEFT|SDL_HAT_RIGHT)) {
            hat &= ~(SDL_HAT_LEFT|SDL_HAT_RIGHT);
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
    }
}

static void VITA_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

static bool VITA_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    int index = joystick->hwdata->index;
    SceCtrlActuator act;

    if (index < 0 || index > 3) {
        return false;
    }
    SDL_zero(act);
    act.small = high_frequency_rumble / 256;
    act.large = low_frequency_rumble / 256;
    if (sceCtrlSetActuator(ext_port_map[index], &act) < 0) {
        return SDL_Unsupported();
    }
    return true;
}

static bool VITA_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    int index = joystick->hwdata->index;
    if (index < 0 || index > 3) {
        return false;
    }
    if (sceCtrlSetLightBar(ext_port_map[index], red, green, blue) < 0) {
        return SDL_Unsupported();
    }
    return true;
}

SDL_JoystickDriver SDL_VITA_JoystickDriver = {
    .Init = VITA_JoystickInit,
    .Open = VITA_JoystickOpen,
    .Rumble = VITA_JoystickRumble,
    .SetLED = VITA_JoystickSetLED,
    .Update = VITA_JoystickUpdate,
    .Close = VITA_JoystickClose
};

#endif // SDL_JOYSTICK_VITA
