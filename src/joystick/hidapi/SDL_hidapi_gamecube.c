/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_haptic.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"


#ifdef SDL_JOYSTICK_HIDAPI_GAMECUBE

typedef struct {
    SDL_JoystickID joysticks[4];
    Uint8 rumble[5];
    Uint32 rumbleExpiration[4];
    /* Without this variable, hid_write starts to lag a TON */
    Uint8 rumbleUpdate;
} SDL_DriverGameCube_Context;

static SDL_bool
HIDAPI_DriverGameCube_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number)
{
    return SDL_IsJoystickGameCube(vendor_id, product_id);
}

static const char *
HIDAPI_DriverGameCube_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    /* Give a user friendly name for this controller */
    if (SDL_IsJoystickGameCube(vendor_id, product_id)) {
        return "Nintendo GameCube Controller";
    }
    return NULL;
}

static SDL_bool
HIDAPI_DriverGameCube_InitDriver(SDL_HIDAPI_DriverData *context, Uint16 vendor_id, Uint16 product_id, int *num_joysticks)
{
    SDL_DriverGameCube_Context *ctx;
    Uint8 packet[37];
    Uint8 *curSlot;
    Uint8 i;
    int size;
    Uint8 initMagic = 0x13;
    Uint8 rumbleMagic = 0x11;

    ctx = (SDL_DriverGameCube_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    ctx->joysticks[0] = -1;
    ctx->joysticks[1] = -1;
    ctx->joysticks[2] = -1;
    ctx->joysticks[3] = -1;
    ctx->rumble[0] = rumbleMagic;

    context->context = ctx;

    /* This is all that's needed to initialize the device. Really! */
    if (hid_write(context->device, &initMagic, sizeof(initMagic)) <= 0) {
        SDL_SetError("Couldn't initialize WUP-028");
        SDL_free(ctx);
        return SDL_FALSE;
    }

    /* Add all the applicable joysticks */
    while ((size = hid_read_timeout(context->device, packet, sizeof(packet), 0)) > 0) {
        if (size < 37 || packet[0] != 0x21) {
            continue; /* Nothing to do yet...? */
        }

        /* Go through all 4 slots */
        curSlot = packet + 1;
        for (i = 0; i < 4; i += 1, curSlot += 9) {
            if (curSlot[0] & 0x30) { /* 0x10 - Wired, 0x20 - Wireless */
                if (ctx->joysticks[i] == -1) {
                    ctx->joysticks[i] = SDL_GetNextJoystickInstanceID();

                    *num_joysticks += 1;

                    SDL_PrivateJoystickAdded(ctx->joysticks[i]);
                }
            } else {
                if (ctx->joysticks[i] != -1) {
                    SDL_PrivateJoystickRemoved(ctx->joysticks[i]);

                    *num_joysticks -= 1;

                    ctx->joysticks[i] = -1;
                }
                continue;
            }
        }
    }

    return SDL_TRUE;
}

static void
HIDAPI_DriverGameCube_QuitDriver(SDL_HIDAPI_DriverData *context, SDL_bool send_event, int *num_joysticks)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    Uint8 i;

    /* Stop all rumble activity */
    for (i = 1; i < 5; i += 1) {
        ctx->rumble[i] = 0;
    }
    hid_write(context->device, ctx->rumble, sizeof(ctx->rumble));

    /* Remove all joysticks! */
    for (i = 0; i < 4; i += 1) {
        if (ctx->joysticks[i] != -1) {
            *num_joysticks -= 1;
            if (send_event) {
                SDL_PrivateJoystickRemoved(ctx->joysticks[i]);
            }
        }
    }

    SDL_free(context->context);
}

static SDL_bool
HIDAPI_DriverGameCube_UpdateDriver(SDL_HIDAPI_DriverData *context, int *num_joysticks)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    SDL_Joystick *joystick;
    Uint8 packet[37];
    Uint8 *curSlot;
    Uint32 now;
    Uint8 i;
    int size;

    /* Read input packet */
    while ((size = hid_read_timeout(context->device, packet, sizeof(packet), 0)) > 0) {
        if (size < 37 || packet[0] != 0x21) {
            continue; /* Nothing to do right now...? */
        }

        /* Go through all 4 slots */
        curSlot = packet + 1;
        for (i = 0; i < 4; i += 1, curSlot += 9) {
            if (curSlot[0] & 0x30) { /* 0x10 - Wired, 0x20 - Wireless */
                if (ctx->joysticks[i] == -1) {
                    ctx->joysticks[i] = SDL_GetNextJoystickInstanceID();

                    *num_joysticks += 1;

                    SDL_PrivateJoystickAdded(ctx->joysticks[i]);
                }
                joystick = SDL_JoystickFromInstanceID(ctx->joysticks[i]);

                /* Hasn't been opened yet, skip */
                if (joystick == NULL) {
                    continue;
                }
            } else {
                if (ctx->joysticks[i] != -1) {
                    SDL_PrivateJoystickRemoved(ctx->joysticks[i]);

                    *num_joysticks -= 1;

                    ctx->joysticks[i] = -1;
                }
                continue;
            }

            #define READ_BUTTON(off, flag, button) \
                SDL_PrivateJoystickButton( \
                    joystick, \
                    button, \
                    (curSlot[off] & flag) ? SDL_PRESSED : SDL_RELEASED \
                );
            READ_BUTTON(1, 0x01, 0) /* A */
            READ_BUTTON(1, 0x02, 1) /* B */
            READ_BUTTON(1, 0x04, 2) /* X */
            READ_BUTTON(1, 0x08, 3) /* Y */
            READ_BUTTON(1, 0x10, 4) /* DPAD_LEFT */
            READ_BUTTON(1, 0x20, 5) /* DPAD_RIGHT */
            READ_BUTTON(1, 0x40, 6) /* DPAD_DOWN */
            READ_BUTTON(1, 0x80, 7) /* DPAD_UP */
            READ_BUTTON(2, 0x01, 8) /* START */
            READ_BUTTON(2, 0x02, 9) /* RIGHTSHOULDER */
            /* These two buttons are for the bottoms of the analog triggers.
             * More than likely, you're going to want to read the axes instead!
             * -flibit
             */
            READ_BUTTON(2, 0x04, 10) /* TRIGGERRIGHT */
            READ_BUTTON(2, 0x08, 11) /* TRIGGERLEFT */
            #undef READ_BUTTON

            /* Axis math taken from SDL_xinputjoystick.c */
            #define READ_AXIS(off, axis) \
                SDL_PrivateJoystickAxis( \
                    joystick, \
                    axis, \
                    (Sint16)(((int)curSlot[off] * 257) - 32768) \
                );
            READ_AXIS(3, 0) /* LEFTX */
            READ_AXIS(4, 1) /* LEFTY */
            READ_AXIS(5, 2) /* RIGHTX */
            READ_AXIS(6, 3) /* RIGHTY */
            READ_AXIS(7, 4) /* TRIGGERLEFT */
            READ_AXIS(8, 5) /* TRIGGERRIGHT */
            #undef READ_AXIS
        }
    }

    /* Write rumble packet */
    now = SDL_GetTicks();
    for (i = 0; i < 4; i += 1) {
        if (ctx->rumbleExpiration[i] > 0) {
            if (SDL_TICKS_PASSED(now, ctx->rumbleExpiration[i])) {
                ctx->rumble[i + 1] = 0;
                ctx->rumbleExpiration[i] = 0;
                ctx->rumbleUpdate = 1;
            }
        }
    }
    if (ctx->rumbleUpdate) {
        hid_write(context->device, ctx->rumble, sizeof(ctx->rumble));
        ctx->rumbleUpdate = 0;
    }

    /* If we got here, nothing bad happened! */
    return SDL_TRUE;
}

static int
HIDAPI_DriverGameCube_NumJoysticks(SDL_HIDAPI_DriverData *context)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    int i, joysticks = 0;
    for (i = 0; i < 4; i += 1) {
        if (ctx->joysticks[i] != -1) {
            joysticks += 1;
        }
    }
    return joysticks;
}

static int
HIDAPI_DriverGameCube_PlayerIndexForIndex(SDL_HIDAPI_DriverData *context, int index)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    Uint8 i;
    for (i = 0; i < 4; i += 1) {
        if (ctx->joysticks[i] != -1) {
            if (index == 0) {
                return i;
            }
            index -= 1;
        }
    }
    return -1; /* Should never get here! */
}

static SDL_JoystickID
HIDAPI_DriverGameCube_InstanceIDForIndex(SDL_HIDAPI_DriverData *context, int index)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    Uint8 i;
    for (i = 0; i < 4; i += 1) {
        if (ctx->joysticks[i] != -1) {
            if (index == 0) {
                return ctx->joysticks[i];
            }
            index -= 1;
        }
    }
    return -1; /* Should never get here! */
}

static SDL_bool
HIDAPI_DriverGameCube_OpenJoystick(SDL_HIDAPI_DriverData *context, SDL_Joystick *joystick)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    SDL_JoystickID instance = SDL_JoystickInstanceID(joystick);
    Uint8 i;
    for (i = 0; i < 4; i += 1) {
        if (instance == ctx->joysticks[i]) {
            joystick->nbuttons = 12;
            joystick->naxes = 6;
            joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;
            joystick->player_index = i;
            return SDL_TRUE;
        }
    }
    return SDL_FALSE; /* Should never get here! */
}

static int
HIDAPI_DriverGameCube_Rumble(SDL_HIDAPI_DriverData *context, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)context->context;
    SDL_JoystickID instance = SDL_JoystickInstanceID(joystick);
    Uint8 i, val;
    for (i = 0; i < 4; i += 1) {
        if (instance == ctx->joysticks[i]) {
            val = (low_frequency_rumble > 0 || high_frequency_rumble > 0);
            if (val != ctx->rumble[i + 1]) {
                ctx->rumble[i + 1] = val;
                ctx->rumbleUpdate = 1;
            }
            if (val && duration_ms < SDL_HAPTIC_INFINITY) {
                ctx->rumbleExpiration[i] = SDL_GetTicks() + duration_ms;
            } else {
                ctx->rumbleExpiration[i] = 0;
            }
            return 0;
        }
    }
    return -1;
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGameCube =
{
    SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE,
    SDL_TRUE,
    HIDAPI_DriverGameCube_IsSupportedDevice,
    HIDAPI_DriverGameCube_GetDeviceName,
    HIDAPI_DriverGameCube_InitDriver,
    HIDAPI_DriverGameCube_QuitDriver,
    HIDAPI_DriverGameCube_UpdateDriver,
    HIDAPI_DriverGameCube_NumJoysticks,
    HIDAPI_DriverGameCube_PlayerIndexForIndex,
    HIDAPI_DriverGameCube_InstanceIDForIndex,
    HIDAPI_DriverGameCube_OpenJoystick,
    HIDAPI_DriverGameCube_Rumble
};

#endif /* SDL_JOYSTICK_HIDAPI_GAMECUBE */

#endif /* SDL_JOYSTICK_HIDAPI */
