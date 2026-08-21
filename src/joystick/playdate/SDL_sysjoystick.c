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

#ifdef SDL_JOYSTICK_PLAYDATE

/* This is the playdate implementation of the SDL joystick API */

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../events/SDL_keyboard_c.h"

#include "pd_api.h"

static int running = 0;
static const unsigned int button_map[] = {
        kButtonLeft,
        kButtonRight,
        kButtonUp,
        kButtonDown,
        kButtonA,
        kButtonB
};

static void
PLAYDATE_JoystickUpdate(SDL_Joystick *joystick)
{
    int i;
    PDButtons buttons;
    PDButtons changed;
    static PDButtons old_buttons;

    if (running) {
        PDButtons current;
        pd->system->getButtonState(&current, NULL, NULL);
        buttons = current;

        changed = old_buttons ^ buttons;
        old_buttons = buttons;
        if (changed) {
            for(i=0;i<6;i++) {
                if (changed & button_map[i]) {
                    SDL_PrivateJoystickButton(
                        joystick, i,
                        (buttons & button_map[i]) ?
                        SDL_PRESSED : SDL_RELEASED);
                }
            }
        }
    }
}

static int
PLAYDATE_JoystickInit(void)
{
    running = 1;
    SDL_PrivateJoystickAdded(0);
    return 0;
}

static int
PLAYDATE_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = 6;
    joystick->naxes = 0; // maybe support...
    joystick->nhats = 0;

    return 0;
}

static int
PLAYDATE_JoystickGetCount(void)
{
    return 1;
}

static void
PLAYDATE_JoystickDetect(void)
{
}

static const char *
PLAYDATE_JoystickGetDeviceName(int device_index)
{
    if (device_index == 0)
        return "Playdate Controller";

    SDL_SetError("No joystick available with that index");
    return(NULL);
}

static int
PLAYDATE_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void
PLAYDATE_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_JoystickGUID
PLAYDATE_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    const char *name = PLAYDATE_JoystickGetDeviceName(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

static SDL_JoystickID
PLAYDATE_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index;
}

static int
PLAYDATE_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static int
PLAYDATE_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32
PLAYDATE_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    return 0;
}

static int
PLAYDATE_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int
PLAYDATE_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static int
PLAYDATE_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    // i think we may need this
    return SDL_Unsupported();
}

static void
PLAYDATE_JoystickClose(SDL_Joystick *joystick)
{
}

static void
PLAYDATE_JoystickQuit(void)
{
    running = 0;
}

static SDL_bool
PLAYDATE_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return SDL_FALSE;
}

SDL_JoystickDriver SDL_PLAYDATE_JoystickDriver =
{
    PLAYDATE_JoystickInit,
    PLAYDATE_JoystickGetCount,
    PLAYDATE_JoystickDetect,
    PLAYDATE_JoystickGetDeviceName,
    PLAYDATE_JoystickGetDevicePlayerIndex,
    PLAYDATE_JoystickSetDevicePlayerIndex,
    PLAYDATE_JoystickGetDeviceGUID,
    PLAYDATE_JoystickGetDeviceInstanceID,
    PLAYDATE_JoystickOpen,
    PLAYDATE_JoystickRumble,
    PLAYDATE_JoystickRumbleTriggers,
    PLAYDATE_JoystickGetCapabilities,
    PLAYDATE_JoystickSetLED,
    PLAYDATE_JoystickSendEffect,
    PLAYDATE_JoystickSetSensorsEnabled,
    PLAYDATE_JoystickUpdate,
    PLAYDATE_JoystickClose,
    PLAYDATE_JoystickQuit,
    PLAYDATE_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_PLAYDATE */

/* vi: set ts=4 sw=4 expandtab: */
