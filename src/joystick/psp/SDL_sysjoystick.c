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

#ifdef SDL_JOYSTICK_PSP

// This is the PSP implementation of the SDL joystick API
#include <pspctrl.h>

#include <stdio.h> // For the definition of NULL
#include <stdlib.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#define PSP_HAT_MASK (PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_UP | PSP_CTRL_RIGHT )

struct joystick_hwdata
{
    // Current pad state
    SceCtrlData pad;

    // Pad state from the last update
    enum PspCtrlButtons last_buttons;
    int last_x, last_y;
};

static const enum PspCtrlButtons button_map[] = {
    PSP_CTRL_TRIANGLE, PSP_CTRL_CIRCLE, PSP_CTRL_CROSS, PSP_CTRL_SQUARE,
    PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER,
    PSP_CTRL_SELECT, PSP_CTRL_START, PSP_CTRL_HOME, PSP_CTRL_HOLD
};
static int analog_map[256]; // Map analog inputs to -32768 -> 32767

// 4 points define the bezier-curve.
static SDL_Point a = { 0, 0 };
static SDL_Point b = { 50, 0 };
static SDL_Point c = { 78, 32767 };
static SDL_Point d = { 128, 32767 };

// simple linear interpolation between two points
static SDL_INLINE void lerp(SDL_Point *dest, const SDL_Point *pt_a, const SDL_Point *pt_b, float t)
{
    dest->x = pt_a->x + (int)((pt_b->x - pt_a->x) * t);
    dest->y = pt_a->y + (int)((pt_b->y - pt_a->y) * t);
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

static bool PSP_JoystickInit(void)
{
    const char *name = "PSP builtin joypad";

    // Setup input
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    /* Create an accurate map from analog inputs (0 to 255)
       to SDL joystick positions (-32768 to 32767) */
    for (int i = 0; i < 128; i++) {
        float t = (float)i / 127.0f;
        analog_map[i + 128] = calc_bezier_y(t);
        analog_map[127 - i] = -1 * analog_map[i + 128];
    }

    // There is always one builtin joypad
    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_PSP_JoystickDriver, 0);
    if (!device) {
        return false;
    }
    device->name = SDL_strdup(name);
    device->guid = SDL_CreateJoystickGUIDForName(name);

    SDL_AddJoystickDevice(device, false);
    return true;
}

static bool PSP_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }

    joystick->nbuttons = SDL_arraysize(button_map);
    joystick->naxes = 2;
    joystick->nhats = 1;  // we treat the d-pad as a hat.

    // Make sure we send fresh state when the controller is opened
    joystick->hwdata->last_x = -1;
    joystick->hwdata->last_y = -1;

    return true;
}

static void PSP_JoystickUpdate(SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    int i;
    enum PspCtrlButtons buttons;
    enum PspCtrlButtons changed;
    unsigned char x, y;
    Uint64 timestamp = SDL_GetTicksNS();

    if (sceCtrlPeekBufferPositive(&hwdata->pad, 1) <= 0) {
        return;
    }
    buttons = hwdata->pad.Buttons;
    x = hwdata->pad.Lx;
    y = hwdata->pad.Ly;

    // Axes
    if (hwdata->last_x != x) {
        SDL_SendJoystickAxis(timestamp, joystick, 0, analog_map[x]);
        hwdata->last_x = x;
    }
    if (hwdata->last_y != y) {
        SDL_SendJoystickAxis(timestamp, joystick, 1, analog_map[y]);
        hwdata->last_y = y;
    }

    // Buttons
    changed = hwdata->last_buttons ^ buttons;
    hwdata->last_buttons = buttons;
    if (changed & ~PSP_HAT_MASK) {
        for (i = 0; i < SDL_arraysize(button_map); i++) {
            if (changed & button_map[i]) {
                const bool down = ((buttons & button_map[i]) != 0);
                SDL_SendJoystickButton(timestamp, joystick, i, down);
            }
        }
    }

    if (changed & PSP_HAT_MASK) {
        // The PSP dpad looks like 4 buttons at this level, but we treat it as a hat switch, so apps that are talking to SDL_Joystick can hope to do basic directional things without a configuration step.
        // (but they should _really_ be using the gamepad API.)
        Uint8 hat = SDL_HAT_CENTERED;
        #define HATSTATE(name) if (buttons & PSP_CTRL_##name) { hat |= SDL_HAT_##name; }
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

static void PSP_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

SDL_JoystickDriver SDL_PSP_JoystickDriver = {
    .Init = PSP_JoystickInit,
    .Open = PSP_JoystickOpen,
    .Update = PSP_JoystickUpdate,
    .Close = PSP_JoystickClose
};

#endif // SDL_JOYSTICK_PSP
