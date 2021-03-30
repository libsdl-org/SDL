/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#ifdef SDL_JOYSTICK_N3DS

/* This is the N3DS implementation of the SDL joystick API */

#include <3ds.h>

#include "../SDL_sysjoystick.h"
#include "SDL_events.h"

#define NB_BUTTONS 23

/*
  N3DS sticks values are roughly within +/-160
  which is too small to pass the jitter tolerance.
  This correction factor is applied to axis values
  so they fit better in SDL's value range.
*/
#define CORRECTION_FACTOR_X SDL_JOYSTICK_AXIS_MAX / 160

/*
  The Y axis needs to be flipped because SDL's "up"
  is reversed compared to libctru's "up"
*/
#define CORRECTION_FACTOR_Y -CORRECTION_FACTOR_X

/*
  Factors used to convert touchscreen coordinates to
  SDL's 0-1 values. Note that the N3DS's screen is
  internally in a portrait disposition so the
  GSP_SCREEN constants are flipped.
*/
#define TOUCHPAD_SCALE_X 1.0f / GSP_SCREEN_HEIGHT_BOTTOM
#define TOUCHPAD_SCALE_Y 1.0f / GSP_SCREEN_WIDTH

typedef struct N3DSJoystickState
{
    u32 kDown;
    u32 kUp;
    circlePosition circlePos;
    circlePosition cStickPos;
    accelVector acceleration;
    angularRate rate;
} N3DSJoystickState;

SDL_FORCE_INLINE void UpdateAxis(SDL_Joystick *joystick, N3DSJoystickState *previous_state);
SDL_FORCE_INLINE void UpdateButtons(SDL_Joystick *joystick, N3DSJoystickState *previous_state);
SDL_FORCE_INLINE void UpdateSensors(SDL_Joystick *joystick, N3DSJoystickState *previous_state);
SDL_FORCE_INLINE void UpdateTouch(SDL_Joystick *joystick);

static N3DSJoystickState current_state;
static SDL_bool sensors_enabled = SDL_FALSE;

static int
N3DS_JoystickInit(void)
{
    hidInit();
    HIDUSER_EnableAccelerometer();
    HIDUSER_EnableGyroscope();
    return 0;
}

static const char *
N3DS_JoystickGetDeviceName(int device_index)
{
    return "Nintendo 3DS";
}

static int
N3DS_JoystickGetCount(void)
{
    return 1;
}

static SDL_JoystickGUID
N3DS_JoystickGetDeviceGUID(int device_index)
{
    /* GUID corresponds to the name "Nintendo 3DS map" */
    SDL_JoystickGUID guid = { { 0x4e, 0x69, 0x6e, 0x74, 0x65, 0x6e, 0x64, 0x6f, 0x20, 0x33, 0x44, 0x53, 0x20, 0x6d, 0x61, 0x70 } };
    return guid;
}

static SDL_JoystickID
N3DS_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index;
}

static int
N3DS_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = NB_BUTTONS;
    joystick->naxes = 4;
    joystick->nhats = 0;
    joystick->instance_id = device_index;

    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 0.0f);
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 0.0f);
    SDL_PrivateJoystickAddTouchpad(joystick, 1);

    return 0;
}

static int
N3DS_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    sensors_enabled = enabled;
    return 0;
}

static void
N3DS_JoystickUpdate(SDL_Joystick *joystick)
{
    N3DSJoystickState previous_state = current_state;
    hidScanInput();

    UpdateAxis(joystick, &previous_state);
    UpdateButtons(joystick, &previous_state);
    UpdateTouch(joystick);

    if (sensors_enabled) {
        UpdateSensors(joystick, &previous_state);
    }
}

SDL_FORCE_INLINE void
UpdateAxis(SDL_Joystick *joystick, N3DSJoystickState *previous_state)
{
    hidCircleRead(&current_state.circlePos);
    if (previous_state->circlePos.dx != current_state.circlePos.dx) {
        SDL_PrivateJoystickAxis(joystick,
                                0,
                                current_state.circlePos.dx * CORRECTION_FACTOR_X);
    }
    if (previous_state->circlePos.dy != current_state.circlePos.dy) {
        SDL_PrivateJoystickAxis(joystick,
                                1,
                                current_state.circlePos.dy * CORRECTION_FACTOR_Y);
    }

    hidCstickRead(&current_state.cStickPos);
    if (previous_state->cStickPos.dx != current_state.cStickPos.dx) {
        SDL_PrivateJoystickAxis(joystick,
                                2,
                                current_state.cStickPos.dx * CORRECTION_FACTOR_X);
    }
    if (previous_state->cStickPos.dy != current_state.cStickPos.dy) {
        SDL_PrivateJoystickAxis(joystick,
                                3,
                                current_state.cStickPos.dy * CORRECTION_FACTOR_Y);
    }
}

SDL_FORCE_INLINE void
UpdateButtons(SDL_Joystick *joystick, N3DSJoystickState *previous_state)
{
    u32 updated_down, updated_up;

    current_state.kDown = hidKeysDown();
    updated_down = previous_state->kDown ^ current_state.kDown;
    if (updated_down) {
        for (Uint8 i = 0; i < joystick->nbuttons; i++) {
            if (current_state.kDown & BIT(i) & updated_down) {
                SDL_PrivateJoystickButton(joystick, i, SDL_PRESSED);
            }
        }
    }

    current_state.kUp = hidKeysUp();
    updated_up = previous_state->kUp ^ current_state.kUp;
    if (updated_up) {
        for (Uint8 i = 0; i < joystick->nbuttons; i++) {
            if (current_state.kUp & BIT(i) & updated_up) {
                SDL_PrivateJoystickButton(joystick, i, SDL_RELEASED);
            }
        }
    }
}

SDL_FORCE_INLINE void
UpdateTouch(SDL_Joystick *joystick)
{
    touchPosition touch;
    Uint8 state;
    hidTouchRead(&touch);
    state = (touch.px == 0 && touch.py == 0) ? SDL_RELEASED : SDL_PRESSED;

    SDL_PrivateJoystickTouchpad(joystick,
                                0,
                                0,
                                state,
                                touch.px * TOUCHPAD_SCALE_X,
                                touch.py * TOUCHPAD_SCALE_Y,
                                state == SDL_PRESSED ? 1.0f : 0.0f);
}

SDL_FORCE_INLINE void
UpdateSensors(SDL_Joystick *joystick, N3DSJoystickState *previous_state)
{
    float data[3];

    hidAccelRead(&current_state.acceleration);
    if (SDL_memcmp(&previous_state->acceleration, &current_state.acceleration, sizeof(accelVector)) != 0) {
        SDL_memcpy(&previous_state->acceleration, &current_state.acceleration, sizeof(accelVector));
        data[0] = (float) current_state.acceleration.x * SDL_STANDARD_GRAVITY;
        data[1] = (float) current_state.acceleration.y * SDL_STANDARD_GRAVITY;
        data[2] = (float) current_state.acceleration.z * SDL_STANDARD_GRAVITY;
        SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_ACCEL, data, sizeof data);
    }

    hidGyroRead(&current_state.rate);
    if (SDL_memcmp(&previous_state->rate, &current_state.rate, sizeof(angularRate)) != 0) {
        SDL_memcpy(&previous_state->rate, &current_state.rate, sizeof(angularRate));
        data[0] = (float) current_state.rate.y;
        data[1] = (float) current_state.rate.z;
        data[2] = (float) current_state.rate.x;
        SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_GYRO, data, sizeof data);
    }
}

static void
N3DS_JoystickClose(SDL_Joystick *joystick)
{
}

static void
N3DS_JoystickQuit(void)
{
    HIDUSER_DisableGyroscope();
    HIDUSER_DisableAccelerometer();
    hidExit();
}

static SDL_bool
N3DS_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    /* There is only one possible mapping. */
    *out = (SDL_GamepadMapping){
        .a = { EMappingKind_Button, 0 },
        .b = { EMappingKind_Button, 1 },
        .x = { EMappingKind_Button, 10 },
        .y = { EMappingKind_Button, 11 },
        .back = { EMappingKind_Button, 2 },
        .guide = { EMappingKind_None, 255 },
        .start = { EMappingKind_Button, 3 },
        .leftstick = { EMappingKind_None, 255 },
        .rightstick = { EMappingKind_None, 255 },
        .leftshoulder = { EMappingKind_Button, 9 },
        .rightshoulder = { EMappingKind_Button, 8 },
        .dpup = { EMappingKind_Button, 6 },
        .dpdown = { EMappingKind_Button, 7 },
        .dpleft = { EMappingKind_Button, 5 },
        .dpright = { EMappingKind_Button, 4 },
        .misc1 = { EMappingKind_None, 255 },
        .paddle1 = { EMappingKind_None, 255 },
        .paddle2 = { EMappingKind_None, 255 },
        .paddle3 = { EMappingKind_None, 255 },
        .paddle4 = { EMappingKind_None, 255 },
        .leftx = { EMappingKind_Axis, 0 },
        .lefty = { EMappingKind_Axis, 1 },
        .rightx = { EMappingKind_Axis, 2 },
        .righty = { EMappingKind_Axis, 3 },
        .lefttrigger = { EMappingKind_Button, 14 },
        .righttrigger = { EMappingKind_Button, 15 },
    };
    return SDL_TRUE;
}

static void
N3DS_JoystickDetect(void)
{
}

static const char *
N3DS_JoystickGetDevicePath(int device_index)
{
    return NULL;
}

static int
N3DS_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void
N3DS_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static Uint32
N3DS_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    return 0;
}

static int
N3DS_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static int
N3DS_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static int
N3DS_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int
N3DS_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

SDL_JoystickDriver SDL_N3DS_JoystickDriver = {
    N3DS_JoystickInit,
    N3DS_JoystickGetCount,
    N3DS_JoystickDetect,
    N3DS_JoystickGetDeviceName,
    N3DS_JoystickGetDevicePath,
    N3DS_JoystickGetDevicePlayerIndex,
    N3DS_JoystickSetDevicePlayerIndex,
    N3DS_JoystickGetDeviceGUID,
    N3DS_JoystickGetDeviceInstanceID,
    N3DS_JoystickOpen,
    N3DS_JoystickRumble,
    N3DS_JoystickRumbleTriggers,
    N3DS_JoystickGetCapabilities,
    N3DS_JoystickSetLED,
    N3DS_JoystickSendEffect,
    N3DS_JoystickSetSensorsEnabled,
    N3DS_JoystickUpdate,
    N3DS_JoystickClose,
    N3DS_JoystickQuit,
    N3DS_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_N3DS */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
