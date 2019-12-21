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
    Uint8 wireless[4];
    Uint8 rumbleAllowed[4];
    Uint8 rumble[5];
    Uint32 rumbleExpiration[4];
    /* Without this variable, hid_write starts to lag a TON */
    SDL_bool rumbleUpdate;
} SDL_DriverGameCube_Context;

static SDL_bool
HIDAPI_DriverGameCube_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, const char *name)
{
    if (vendor_id == 0x057e && product_id == 0x0337) {
        /* Nintendo Co., Ltd.  Wii U GameCube Controller Adapter */
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static const char *
HIDAPI_DriverGameCube_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    return "Nintendo GameCube Controller";
}

static SDL_bool
HIDAPI_DriverGameCube_InitDevice(SDL_HIDAPI_Device *device)
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

    device->dev = hid_open_path(device->path, 0);
    if (!device->dev) {
        SDL_free(ctx);
        SDL_SetError("Couldn't open %s", device->path);
        return SDL_FALSE;
    }
    device->context = ctx;

    ctx->joysticks[0] = -1;
    ctx->joysticks[1] = -1;
    ctx->joysticks[2] = -1;
    ctx->joysticks[3] = -1;
    ctx->rumble[0] = rumbleMagic;

    /* This is all that's needed to initialize the device. Really! */
    if (hid_write(device->dev, &initMagic, sizeof(initMagic)) != sizeof(initMagic)) {
        SDL_SetError("Couldn't initialize WUP-028");
        goto error;
    }

    /* Add all the applicable joysticks */
    while ((size = hid_read_timeout(device->dev, packet, sizeof(packet), 0)) > 0) {
        if (size < 37 || packet[0] != 0x21) {
            continue; /* Nothing to do yet...? */
        }

        /* Go through all 4 slots */
        curSlot = packet + 1;
        for (i = 0; i < 4; i += 1, curSlot += 9) {
            ctx->wireless[i] = (curSlot[0] & 0x20) != 0;

            /* Only allow rumble if the adapter's second USB cable is connected */
            ctx->rumbleAllowed[i] = (curSlot[0] & 0x04) != 0 && !ctx->wireless[i];

            if (curSlot[0] & 0x30) { /* 0x10 - Wired, 0x20 - Wireless */
                if (ctx->joysticks[i] == -1) {
                    HIDAPI_JoystickConnected(device, &ctx->joysticks[i]);
                }
            } else {
                if (ctx->joysticks[i] != -1) {
                    HIDAPI_JoystickDisconnected(device, ctx->joysticks[i]);
                    ctx->joysticks[i] = -1;
                }
                continue;
            }
        }
    }

    return SDL_TRUE;

error:
    if (device->dev) {
        hid_close(device->dev);
        device->dev = NULL;
    }
    if (device->context) {
        SDL_free(device->context);
        device->context = NULL;
    }
    return SDL_FALSE;
}

static int
HIDAPI_DriverGameCube_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)device->context;
    Uint8 i;

    for (i = 0; i < 4; ++i) {
        if (instance_id == ctx->joysticks[i]) {
            return i;
        }
    }
    return -1;
}

static void
HIDAPI_DriverGameCube_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static SDL_bool
HIDAPI_DriverGameCube_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)device->context;
    SDL_Joystick *joystick;
    Uint8 packet[37];
    Uint8 *curSlot;
    Uint8 i;
    int size;

    /* Read input packet */
    while ((size = hid_read_timeout(device->dev, packet, sizeof(packet), 0)) > 0) {
        if (size < 37 || packet[0] != 0x21) {
            continue; /* Nothing to do right now...? */
        }

        /* Go through all 4 slots */
        curSlot = packet + 1;
        for (i = 0; i < 4; i += 1, curSlot += 9) {
            ctx->wireless[i] = (curSlot[0] & 0x20) != 0;

            /* Only allow rumble if the adapter's second USB cable is connected */
            ctx->rumbleAllowed[i] = (curSlot[0] & 0x04) != 0 && !ctx->wireless[i];

            if (curSlot[0] & 0x30) { /* 0x10 - Wired, 0x20 - Wireless */
                if (ctx->joysticks[i] == -1) {
                    HIDAPI_JoystickConnected(device, &ctx->joysticks[i]);
                }
                joystick = SDL_JoystickFromInstanceID(ctx->joysticks[i]);

                /* Hasn't been opened yet, skip */
                if (joystick == NULL) {
                    continue;
                }
            } else {
                if (ctx->joysticks[i] != -1) {
                    HIDAPI_JoystickDisconnected(device, ctx->joysticks[i]);
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
    for (i = 0; i < 4; i += 1) {
        if (ctx->rumbleExpiration[i] || (ctx->rumble[1 + i] && !ctx->rumbleAllowed[i])) {
            Uint32 now = SDL_GetTicks();
            if (SDL_TICKS_PASSED(now, ctx->rumbleExpiration[i]) || !ctx->rumbleAllowed[i]) {
                ctx->rumble[1 + i] = 0;
                ctx->rumbleExpiration[i] = 0;
                ctx->rumbleUpdate = SDL_TRUE;
            }
        }
    }
    if (ctx->rumbleUpdate) {
        hid_write(device->dev, ctx->rumble, sizeof(ctx->rumble));
        ctx->rumbleUpdate = SDL_FALSE;
    }

    /* If we got here, nothing bad happened! */
    return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverGameCube_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)device->context;
    Uint8 i;
    for (i = 0; i < 4; i += 1) {
        if (joystick->instance_id == ctx->joysticks[i]) {
            joystick->nbuttons = 12;
            joystick->naxes = 6;
            joystick->epowerlevel = ctx->wireless[i] ? SDL_JOYSTICK_POWER_UNKNOWN : SDL_JOYSTICK_POWER_WIRED;
            return SDL_TRUE;
        }
    }
    return SDL_FALSE; /* Should never get here! */
}

static int
HIDAPI_DriverGameCube_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)device->context;
    Uint8 i, val;
    for (i = 0; i < 4; i += 1) {
        if (joystick->instance_id == ctx->joysticks[i]) {
            if (ctx->wireless[i]) {
                return SDL_SetError("Ninteno GameCube WaveBird controllers do not support rumble");
            }
            if (!ctx->rumbleAllowed[i]) {
                return SDL_SetError("Second USB cable for WUP-028 not connected");
            }
            val = (low_frequency_rumble > 0 || high_frequency_rumble > 0);
            if (val != ctx->rumble[i + 1]) {
                ctx->rumble[i + 1] = val;
                ctx->rumbleUpdate = SDL_TRUE;
            }
            if (val && duration_ms) {
                ctx->rumbleExpiration[i] = SDL_GetTicks() + SDL_min(duration_ms, SDL_MAX_RUMBLE_DURATION_MS);
                if (!ctx->rumbleExpiration[i]) {
                    ctx->rumbleExpiration[i] = 1;
                }
            } else {
                ctx->rumbleExpiration[i] = 0;
            }
            return 0;
        }
    }

    /* Should never get here! */
    SDL_SetError("Couldn't find joystick");
    return -1;
}

static void
HIDAPI_DriverGameCube_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverGameCube_Context *ctx = (SDL_DriverGameCube_Context *)device->context;
    Uint8 i;

    /* Stop rumble activity */
    for (i = 0; i < 4; i += 1) {
        if (joystick->instance_id == ctx->joysticks[i]) {
            if (!ctx->wireless[i] && ctx->rumbleAllowed[i] && ctx->rumble[1 + i] != 0) {
                ctx->rumble[1 + i] = 0;
                ctx->rumbleExpiration[i] = 0;
                hid_write(device->dev, ctx->rumble, sizeof(ctx->rumble));
            }
            break;
        }
    }
}

static void
HIDAPI_DriverGameCube_FreeDevice(SDL_HIDAPI_Device *device)
{
    hid_close(device->dev);
    device->dev = NULL;

    SDL_free(device->context);
    device->context = NULL;
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverGameCube =
{
    SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE,
    SDL_TRUE,
    HIDAPI_DriverGameCube_IsSupportedDevice,
    HIDAPI_DriverGameCube_GetDeviceName,
    HIDAPI_DriverGameCube_InitDevice,
    HIDAPI_DriverGameCube_GetDevicePlayerIndex,
    HIDAPI_DriverGameCube_SetDevicePlayerIndex,
    HIDAPI_DriverGameCube_UpdateDevice,
    HIDAPI_DriverGameCube_OpenJoystick,
    HIDAPI_DriverGameCube_RumbleJoystick,
    HIDAPI_DriverGameCube_CloseJoystick,
    HIDAPI_DriverGameCube_FreeDevice
};

#endif /* SDL_JOYSTICK_HIDAPI_GAMECUBE */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
