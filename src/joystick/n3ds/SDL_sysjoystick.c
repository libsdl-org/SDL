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

#ifdef SDL_JOYSTICK_N3DS

// This is the N3DS implementation of the SDL joystick API

#include <3ds.h>

#include "../SDL_sysjoystick.h"

#define NB_BUTTONS 23   // physical buttons on the device (although we treat four of these, the dpad, as a hat switch).
#define N3DS_HAT_MASK 0xF0   // 0xF0==d-pad bits, which we handle elsewhere, so mask them out here.

/*
  N3DS sticks values are roughly within +/-160
  which is too small to pass the jitter tolerance.
  This correction is applied to axis values
  so they fit better in SDL's value range.
*/
static inline int Correct_Axis_X(int X) {
    if (X > 160) {
        return SDL_JOYSTICK_AXIS_MAX;
    }
    else if (X < -160) {
        return -SDL_JOYSTICK_AXIS_MAX;
    }
    return (X * SDL_JOYSTICK_AXIS_MAX) / 160;
}

/*
  The Y axis needs to be flipped because SDL's "up"
  is reversed compared to libctru's "up"
*/
static inline int Correct_Axis_Y(int Y) {
    return Correct_Axis_X(-Y);
}

struct joystick_hwdata
{
    u32 last_button_state;
    circlePosition last_circle_state;
    circlePosition last_cstick_state;
};

static void UpdateN3DSButtons(Uint64 timestamp, SDL_Joystick *joystick, u32 previous_button_state, u32 current_button_state);
static void UpdateN3DSHat(Uint64 timestamp, SDL_Joystick *joystick, u32 previous_button_state, u32 current_button_state);
static void UpdateN3DSCircle(Uint64 timestamp, SDL_Joystick *joystick);
static void UpdateN3DSCStick(Uint64 timestamp, SDL_Joystick *joystick);

static bool N3DS_JoystickInit(void)
{
    const char *name = "Nintendo 3DS";

    hidInit();

    // There is always one builtin controller
    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_N3DS_JoystickDriver, 0);
    if (!device) {
        hidExit();
        return false;
    }
    device->name = SDL_strdup(name);
    device->guid = SDL_CreateJoystickGUIDForName(name);

    SDL_AddJoystickDevice(device, false);
    return true;
}

static bool N3DS_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }

    joystick->nbuttons = NB_BUTTONS - 4;  // -4 for the dpad (which we treat as a hat).
    joystick->naxes = 4;
    joystick->nhats = 1;  // treat the dpad as a hat.

    // Make sure we send fresh state when the controller is opened
    joystick->hwdata->last_circle_state.dx = SDL_MAX_SINT16;
    joystick->hwdata->last_circle_state.dy = SDL_MAX_SINT16;
    joystick->hwdata->last_cstick_state.dx = SDL_MAX_SINT16;
    joystick->hwdata->last_cstick_state.dy = SDL_MAX_SINT16;

    return true;
}

static void N3DS_JoystickUpdate(SDL_Joystick *joystick)
{
    const Uint64 timestamp = SDL_GetTicksNS();
    const u32 current_button_state = hidKeysHeld();
    struct joystick_hwdata *hwdata = joystick->hwdata;

    UpdateN3DSButtons(timestamp, joystick, hwdata->last_button_state, current_button_state);
    UpdateN3DSHat(timestamp, joystick, hwdata->last_button_state, current_button_state);
    UpdateN3DSCircle(timestamp, joystick);
    UpdateN3DSCStick(timestamp, joystick);

    hwdata->last_button_state = current_button_state;
}

static void UpdateN3DSButtons(Uint64 timestamp, SDL_Joystick *joystick, u32 previous_button_state, u32 current_button_state)
{
    const u32 updated_buttons = (previous_button_state ^ current_button_state) & ~N3DS_HAT_MASK;
    if (updated_buttons) {
        Uint8 buttonidx = 0;
        Uint8 i = 0;
        while (i < joystick->nbuttons) {
            if ((buttonidx < 4) || (buttonidx > 7)) {  // skip dpad (we treat it as a hat).
                if (updated_buttons & BIT(buttonidx)) {
                    const bool down = ((current_button_state & BIT(buttonidx)) != 0);
                    SDL_SendJoystickButton(timestamp, joystick, i, down);
                }
                i++;
            }
            buttonidx++;
        }
    }
}

static void UpdateN3DSHat(Uint64 timestamp, SDL_Joystick *joystick, u32 previous_button_state, u32 current_button_state)
{
    // The 3DS dpad looks like 4 buttons at this level, but we treat it as a hat switch, so apps that are talking to SDL_Joystick can hope to do basic directional things without a configuration step.
    // (but they should _really_ be using the gamepad API.)
    const u32 updated_hat = ((previous_button_state ^ current_button_state) & N3DS_HAT_MASK);  // did bits 4 through 7 change?
    if (updated_hat) {
        Uint8 hat = SDL_HAT_CENTERED;

        #define HATSTATE(n3dsbit, sdlenum) if (current_button_state & BIT(n3dsbit)) { hat |= SDL_HAT_##sdlenum; }
        HATSTATE(4, RIGHT);
        HATSTATE(5, LEFT);
        HATSTATE(6, UP);
        HATSTATE(7, DOWN);
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


static void UpdateN3DSCircle(Uint64 timestamp, SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    circlePosition current_state;
    hidCircleRead(&current_state);
    if (hwdata->last_circle_state.dx != current_state.dx) {
        SDL_SendJoystickAxis(timestamp, joystick,
                                0,
                                Correct_Axis_X(current_state.dx));
    }
    if (hwdata->last_circle_state.dy != current_state.dy) {
        SDL_SendJoystickAxis(timestamp, joystick,
                                1,
                                Correct_Axis_Y(current_state.dy));
    }
    hwdata->last_circle_state = current_state;
}

static void UpdateN3DSCStick(Uint64 timestamp, SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    circlePosition current_state;
    hidCstickRead(&current_state);
    if (hwdata->last_cstick_state.dx != current_state.dx) {
        SDL_SendJoystickAxis(timestamp, joystick,
                                2,
                                Correct_Axis_X(current_state.dx));
    }
    if (hwdata->last_cstick_state.dy != current_state.dy) {
        SDL_SendJoystickAxis(timestamp, joystick,
                                3,
                                Correct_Axis_Y(current_state.dy));
    }
    hwdata->last_cstick_state = current_state;
}

static void N3DS_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

static void N3DS_JoystickQuit(void)
{
    hidExit();
}

static bool N3DS_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    // There is only one possible mapping.
    *out = (SDL_GamepadMapping){
        .a = { EMappingKind_Button, 0 },
        .b = { EMappingKind_Button, 1 },
        .x = { EMappingKind_Button, 6 },
        .y = { EMappingKind_Button, 7 },
        .back = { EMappingKind_Button, 2 },
        .guide = { EMappingKind_None, 255 },
        .start = { EMappingKind_Button, 3 },
        .leftstick = { EMappingKind_None, 255 },
        .rightstick = { EMappingKind_None, 255 },
        .leftshoulder = { EMappingKind_Button, 5 },
        .rightshoulder = { EMappingKind_Button, 4 },
        .dpup = { EMappingKind_Hat, SDL_HAT_UP },
        .dpdown = { EMappingKind_Hat, SDL_HAT_DOWN },
        .dpleft = { EMappingKind_Hat, SDL_HAT_LEFT },
        .dpright = { EMappingKind_Hat, SDL_HAT_RIGHT },
        .misc1 = { EMappingKind_None, 255 },
        .right_paddle1 = { EMappingKind_None, 255 },
        .left_paddle1 = { EMappingKind_None, 255 },
        .right_paddle2 = { EMappingKind_None, 255 },
        .left_paddle2 = { EMappingKind_None, 255 },
        .leftx = { EMappingKind_Axis, 0 },
        .lefty = { EMappingKind_Axis, 1 },
        .rightx = { EMappingKind_Axis, 2 },
        .righty = { EMappingKind_Axis, 3 },
        .lefttrigger = { EMappingKind_Button, 10 },
        .righttrigger = { EMappingKind_Button, 11 },
    };
    return true;
}

SDL_JoystickDriver SDL_N3DS_JoystickDriver = {
    .Init = N3DS_JoystickInit,
    .GetGamepadMapping = N3DS_JoystickGetGamepadMapping,
    .Open = N3DS_JoystickOpen,
    .Update = N3DS_JoystickUpdate,
    .Close = N3DS_JoystickClose,
    .Quit = N3DS_JoystickQuit
};

#endif // SDL_JOYSTICK_N3DS
