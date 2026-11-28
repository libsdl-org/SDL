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

#ifdef SDL_JOYSTICK_PUMPKINOS

/* PumpkinOS joystick driver.

   PalmOS devices have a handful of hard keys: four application buttons, the
   up/down (page) keys, left/right and the power key. PumpkinOS reports their
   state in the key mask of pumpkin_status(), driven by the host keyboard
   (F1..F4, arrows, End) or by the device buttons on embedded targets. This
   driver exposes them as one gamepad: the four application buttons are
   A/B/X/Y, the power key is Start and the direction keys form the d-pad. */

#include "../SDL_sysjoystick.h"
#include "../../core/pumpkin/SDL_pumpkin.h"

#define PUMPKIN_JOYSTICK_NAME "PumpkinOS Hard Keys"

#define PUMPKIN_BUTTON_MASK (keyBitHard1 | keyBitHard2 | keyBitHard3 | keyBitHard4 | keyBitPower)
#define PUMPKIN_HAT_MASK (keyBitPageUp | keyBitPageDown | keyBitLeft | keyBitRight)

static const Uint32 pumpkin_button_bits[] = {
    keyBitHard1, keyBitHard2, keyBitHard3, keyBitHard4, keyBitPower
};

struct joystick_hwdata
{
    Uint32 last_key_mask;
};

static bool PUMPKIN_JoystickInit(void)
{
    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_PUMPKIN_JoystickDriver, 0);
    if (!device) {
        return false;
    }
    device->name = SDL_strdup(PUMPKIN_JOYSTICK_NAME);
    device->guid = SDL_CreateJoystickGUIDForName(PUMPKIN_JOYSTICK_NAME);
    SDL_AddJoystickDevice(device);
    return true;
}

static bool PUMPKIN_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->nbuttons = SDL_arraysize(pumpkin_button_bits);
    joystick->naxes = 0;
    joystick->nhats = 1;
    return true;
}

static void PUMPKIN_JoystickUpdate(SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    const Uint64 timestamp = SDL_GetTicksNS();
    uint32_t key_mask = 0;
    Uint32 diff;
    int i;

    pumpkin_status(NULL, NULL, &key_mask, NULL, NULL, NULL);
    diff = key_mask ^ hwdata->last_key_mask;
    if (!diff) {
        return;
    }

    if (diff & PUMPKIN_BUTTON_MASK) {
        for (i = 0; i < (int)SDL_arraysize(pumpkin_button_bits); i++) {
            if (diff & pumpkin_button_bits[i]) {
                SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, (key_mask & pumpkin_button_bits[i]) != 0);
            }
        }
    }

    if (diff & PUMPKIN_HAT_MASK) {
        Uint8 hat = SDL_HAT_CENTERED;
        if (key_mask & keyBitPageUp) {
            hat |= SDL_HAT_UP;
        }
        if (key_mask & keyBitPageDown) {
            hat |= SDL_HAT_DOWN;
        }
        if (key_mask & keyBitLeft) {
            hat |= SDL_HAT_LEFT;
        }
        if (key_mask & keyBitRight) {
            hat |= SDL_HAT_RIGHT;
        }
        // a keyboard can hold both; a hat cannot
        if ((hat & (SDL_HAT_UP | SDL_HAT_DOWN)) == (SDL_HAT_UP | SDL_HAT_DOWN)) {
            hat &= ~(SDL_HAT_UP | SDL_HAT_DOWN);
        }
        if ((hat & (SDL_HAT_LEFT | SDL_HAT_RIGHT)) == (SDL_HAT_LEFT | SDL_HAT_RIGHT)) {
            hat &= ~(SDL_HAT_LEFT | SDL_HAT_RIGHT);
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
    }

    hwdata->last_key_mask = key_mask;
}

static void PUMPKIN_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

static void PUMPKIN_JoystickQuit(void)
{
}

static bool PUMPKIN_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    SDL_zerop(out);
    out->a = (SDL_InputMapping){ EMappingKind_Button, 0 };
    out->b = (SDL_InputMapping){ EMappingKind_Button, 1 };
    out->x = (SDL_InputMapping){ EMappingKind_Button, 2 };
    out->y = (SDL_InputMapping){ EMappingKind_Button, 3 };
    out->start = (SDL_InputMapping){ EMappingKind_Button, 4 };
    out->dpup = (SDL_InputMapping){ EMappingKind_Hat, SDL_HAT_UP };
    out->dpdown = (SDL_InputMapping){ EMappingKind_Hat, SDL_HAT_DOWN };
    out->dpleft = (SDL_InputMapping){ EMappingKind_Hat, SDL_HAT_LEFT };
    out->dpright = (SDL_InputMapping){ EMappingKind_Hat, SDL_HAT_RIGHT };
    return true;
}

SDL_JoystickDriver SDL_PUMPKIN_JoystickDriver = {
    .Init = PUMPKIN_JoystickInit,
    .GetGamepadMapping = PUMPKIN_JoystickGetGamepadMapping,
    .Open = PUMPKIN_JoystickOpen,
    .Update = PUMPKIN_JoystickUpdate,
    .Close = PUMPKIN_JoystickClose,
    .Quit = PUMPKIN_JoystickQuit
};

#endif // SDL_JOYSTICK_PUMPKINOS
