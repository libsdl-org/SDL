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

#ifdef SDL_JOYSTICK_HIDAPI

#include "../../SDL_hints_c.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"
#include "SDL_hidapi_xbox360.h"

#ifdef SDL_JOYSTICK_HIDAPI_XBOX360

// Define this if you want to log all packets from the controller
// #define DEBUG_XBOX_PROTOCOL

#define MAX_CONTROLLERS 4

typedef struct
{
    SDL_HIDAPI_Device *device;
    SDL_JoystickID joysticks[MAX_CONTROLLERS];
    Uint8 last_state[MAX_CONTROLLERS][USB_PACKET_LENGTH];
    Uint64 last_packet[MAX_CONTROLLERS];
} SDL_DriverXbox360BB_Context;

static void HIDAPI_DriverXBOX360BB_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX, callback, userdata);
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360, callback, userdata);
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_BIGBUTTON, callback, userdata);
}

static void HIDAPI_DriverXBOX360BB_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX, callback, userdata);
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360, callback, userdata);
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_BIGBUTTON, callback, userdata);
}

static bool HIDAPI_DriverXBOX360BB_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_BIGBUTTON,
                              SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_XBOX_360, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_XBOX, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT))));
}

static bool HIDAPI_DriverXBOX360BB_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return (vendor_id == USB_VENDOR_MICROSOFT) && (product_id == USB_PRODUCT_XBOX360_BIGBUTTON_RECEIVER);
}

static bool HIDAPI_DriverXBOX360BB_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXbox360BB_Context *ctx;
    Uint8 i;

    HIDAPI_SetDeviceName(device, "Xbox 360 Big Button Controller");

    ctx = (SDL_DriverXbox360BB_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    ctx->device = device;
    device->joystick_type = SDL_JOYSTICK_TYPE_UNKNOWN; // FIXME: Clicker? TriviaThing?

    device->context = ctx;

    for (i = 0; i < MAX_CONTROLLERS; ++i) {
        HIDAPI_JoystickConnected(device, &ctx->joysticks[i]);
    }

    device->type = SDL_GAMEPAD_TYPE_UNKNOWN; // FIXME: Clicker? TriviaThing?

    return true;
}

static int HIDAPI_DriverXBOX360BB_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverXBOX360BB_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverXBOX360BB_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXbox360BB_Context *ctx = (SDL_DriverXbox360BB_Context *)device->context;
    Uint8 i;

    SDL_AssertJoysticksLocked();

    for (i = 0; i < MAX_CONTROLLERS; i += 1) {
        if (joystick->instance_id == ctx->joysticks[i]) {
            joystick->nbuttons = 12;
            joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
            return true;
        }
    }
    return false; // Should never get here!
}

static bool HIDAPI_DriverXBOX360BB_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverXBOX360BB_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverXBOX360BB_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverXBOX360BB_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverXBOX360BB_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverXBOX360BB_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void HIDAPI_DriverXBOX360BB_HandleStatePacket(SDL_DriverXbox360BB_Context *ctx, Uint8 *data, int size)
{
    Uint64 timestamp = SDL_GetTicksNS();
    SDL_Joystick *joystick;
    Uint8 i;

    i = data[2];
    SDL_assert(i < MAX_CONTROLLERS);
    joystick = SDL_GetJoystickFromID(ctx->joysticks[i]);
    ctx->last_packet[i] = timestamp;

    if (ctx->last_state[i][3] != data[3]) {
        SDL_SendJoystickButton(timestamp, joystick, 0, ((data[3] & 0x01) != 0)); // bigbutton up
        SDL_SendJoystickButton(timestamp, joystick, 1, ((data[3] & 0x02) != 0)); // bigbutton down
        SDL_SendJoystickButton(timestamp, joystick, 2, ((data[3] & 0x04) != 0)); // bigbutton left
        SDL_SendJoystickButton(timestamp, joystick, 3, ((data[3] & 0x08) != 0)); // bigbutton right
        SDL_SendJoystickButton(timestamp, joystick, 4, ((data[3] & 0x10) != 0)); // start
        SDL_SendJoystickButton(timestamp, joystick, 5, ((data[3] & 0x20) != 0)); // back
    }

    if (ctx->last_state[i][4] != data[4]) {
        SDL_SendJoystickButton(timestamp, joystick, 6, ((data[4] & 0x04) != 0)); // guide
        SDL_SendJoystickButton(timestamp, joystick, 7, ((data[4] & 0x08) != 0)); // bigbutton
        SDL_SendJoystickButton(timestamp, joystick, 8, ((data[4] & 0x10) != 0)); // A
        SDL_SendJoystickButton(timestamp, joystick, 9, ((data[4] & 0x20) != 0)); // B
        SDL_SendJoystickButton(timestamp, joystick, 10, ((data[4] & 0x40) != 0)); // X
        SDL_SendJoystickButton(timestamp, joystick, 11, ((data[4] & 0x80) != 0)); // Y
    }

    SDL_memcpy(ctx->last_state[i], data, SDL_min(size, sizeof(ctx->last_state[i])));
}

static void HIDAPI_DriverXBOX360BB_HandleReleaseEvents(SDL_DriverXbox360BB_Context *ctx)
{
    Uint64 timestamp = SDL_GetTicksNS();
    Uint8 i;

    // The receiver only sends packets when a button is down, handle our own release logic
    for (i = 0; i < MAX_CONTROLLERS; i += 1) {
        // FIXME: Even when a button is continuously pressed, we get _huge_
        // delays between packets. This is the smallest number I could get
        // without erroneous button releases. Yeesh.
        // -flibit
        if (SDL_NS_TO_MS(timestamp - ctx->last_packet[i]) > 117) {
            SDL_Joystick *joystick = SDL_GetJoystickFromID(ctx->joysticks[i]);
            if (joystick) {
                SDL_SendJoystickButton(timestamp, joystick, 0, 0); // bigbutton up
                SDL_SendJoystickButton(timestamp, joystick, 1, 0); // bigbutton down
                SDL_SendJoystickButton(timestamp, joystick, 2, 0); // bigbutton left
                SDL_SendJoystickButton(timestamp, joystick, 3, 0); // bigbutton right
                SDL_SendJoystickButton(timestamp, joystick, 4, 0); // start
                SDL_SendJoystickButton(timestamp, joystick, 5, 0); // back
                SDL_SendJoystickButton(timestamp, joystick, 6, 0); // guide
                SDL_SendJoystickButton(timestamp, joystick, 7, 0); // bigbutton
                SDL_SendJoystickButton(timestamp, joystick, 8, 0); // A
                SDL_SendJoystickButton(timestamp, joystick, 9, 0); // B
                SDL_SendJoystickButton(timestamp, joystick, 10, 0); // X
                SDL_SendJoystickButton(timestamp, joystick, 11, 0); // Y
            }
            SDL_zero(ctx->last_state[i]);
            ctx->last_packet[i] = timestamp;
        }
    }
}

static bool HIDAPI_DriverXBOX360BB_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXbox360BB_Context *ctx = (SDL_DriverXbox360BB_Context *)device->context;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_XBOX_PROTOCOL
        HIDAPI_DumpPacket("Xbox 360 BigButton packet: size = %d", data, size);
#endif
        if (size == 5) {
            HIDAPI_DriverXBOX360BB_HandleStatePacket(ctx, data, size);
        }
    }

    HIDAPI_DriverXBOX360BB_HandleReleaseEvents(ctx);

    return (size >= 0);
}

static void HIDAPI_DriverXBOX360BB_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverXBOX360BB_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360BB = {
    SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_BIGBUTTON,
    true,
    HIDAPI_DriverXBOX360BB_RegisterHints,
    HIDAPI_DriverXBOX360BB_UnregisterHints,
    HIDAPI_DriverXBOX360BB_IsEnabled,
    HIDAPI_DriverXBOX360BB_IsSupportedDevice,
    HIDAPI_DriverXBOX360BB_InitDevice,
    HIDAPI_DriverXBOX360BB_GetDevicePlayerIndex,
    HIDAPI_DriverXBOX360BB_SetDevicePlayerIndex,
    HIDAPI_DriverXBOX360BB_UpdateDevice,
    HIDAPI_DriverXBOX360BB_OpenJoystick,
    HIDAPI_DriverXBOX360BB_RumbleJoystick,
    HIDAPI_DriverXBOX360BB_RumbleJoystickTriggers,
    HIDAPI_DriverXBOX360BB_GetJoystickCapabilities,
    HIDAPI_DriverXBOX360BB_SetJoystickLED,
    HIDAPI_DriverXBOX360BB_SendJoystickEffect,
    HIDAPI_DriverXBOX360BB_SetJoystickSensorsEnabled,
    HIDAPI_DriverXBOX360BB_CloseJoystick,
    HIDAPI_DriverXBOX360BB_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_XBOX360

#endif // SDL_JOYSTICK_HIDAPI
