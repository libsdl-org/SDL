/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"

#ifdef SDL_JOYSTICK_HIDAPI_ZUIKI

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_ZUIKI_PROTOCOL
#endif

typedef struct
{
    bool sensors_supported;
    bool sensors_enabled;
    bool touchpad_01_supported;
    bool touchpad_02_supported;
    bool rumble_supported;
    bool rumble_type;
    bool rgb_supported;
    bool player_led_supported;
    bool powerstate_supported;
    Uint8 serial[6];
    Uint16 version;
    Uint16 version_beta;
    float accelScale;
    float gyroScale;
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint64 sensor_timestamp; // Nanoseconds.  Simulate onboard clock. Different models have different rates vs different connection styles.
    Uint64 sensor_timestamp_interval;
} SDL_DriverZUIKI_Context;

#pragma pack(push, 1)

typedef struct
{
    // Accelerometer values
    short sAccelX;
    short sAccelY;
    short sAccelZ;

    // Gyroscope values
    short sGyroX;
    short sGyroY;
    short sGyroZ;
} ABITDO_SENSORS;

#pragma pack(pop)

static void HIDAPI_DriverZUIKI_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_ZUIKI, callback, userdata);
}

static void HIDAPI_DriverZUIKI_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_ZUIKI, callback, userdata);
}

static bool HIDAPI_DriverZUIKI_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_ZUIKI, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverZUIKI_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id == USB_VENDOR_ZUIKI) {
        switch (product_id) {
        case USB_PRODUCT_ZUIKI_MASCON_PRO:
            return true;
        case USB_PRODUCT_ZUIKI_EVOTOP:
            return true;
        default:
            break;
        }
    }
    return false;
}

static bool HIDAPI_DriverZUIKI_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverZUIKI_Context *ctx = (SDL_DriverZUIKI_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    device->context = ctx;

    if (device->product_id == USB_PRODUCT_ZUIKI_MASCON_PRO) {
        HIDAPI_SetDeviceName(device, "ZUIKI MASCON PRO");
    }

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverZUIKI_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverZUIKI_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif

static bool HIDAPI_DriverZUIKI_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverZUIKI_Context *ctx = (SDL_DriverZUIKI_Context *)device->context;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);

    joystick->nbuttons = 11;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    return true;
}

static bool HIDAPI_DriverZUIKI_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    Uint8 rumble_packet[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    rumble_packet[4] = low_frequency_rumble >> 8;
    rumble_packet[5] = high_frequency_rumble >> 8;
    if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }
    return true;
}

static bool HIDAPI_DriverZUIKI_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverZUIKI_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    Uint32 caps = 0;
    caps |= SDL_JOYSTICK_CAP_RUMBLE;
    return caps;
}

static bool HIDAPI_DriverZUIKI_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverZUIKI_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverZUIKI_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void HIDAPI_DriverZUIKI_HandleOldStatePacket(SDL_Joystick *joystick, SDL_DriverZUIKI_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    if (ctx->last_state[2] != data[2]) {
        Uint8 hat;

        switch (data[2]) {
        case 0:
            hat = SDL_HAT_UP;
            break;
        case 1:
            hat = SDL_HAT_RIGHTUP;
            break;
        case 2:
            hat = SDL_HAT_RIGHT;
            break;
        case 3:
            hat = SDL_HAT_RIGHTDOWN;
            break;
        case 4:
            hat = SDL_HAT_DOWN;
            break;
        case 5:
            hat = SDL_HAT_LEFTDOWN;
            break;
        case 6:
            hat = SDL_HAT_LEFT;
            break;
        case 7:
            hat = SDL_HAT_LEFTUP;
            break;
        default:
            hat = SDL_HAT_CENTERED;
            break;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
    }

    if (ctx->last_state[0] != data[0]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((data[0] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((data[0] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((data[0] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((data[0] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ((data[0] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ((data[0] & 0x20) != 0));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, (data[0] & 0x40) ? SDL_MAX_SINT16 : SDL_MIN_SINT16);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, (data[0] & 0x80) ? SDL_MAX_SINT16 : SDL_MIN_SINT16);
    }

    if (ctx->last_state[1] != data[1]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[1] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((data[1] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((data[1] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ((data[1] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[1] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, ((data[1] & 0x20) != 0));
        /* todo for switch C key */
    }

#define READ_STICK_AXIS(offset) \
    (data[offset] == 0x7f ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)data[offset] - 0x7f), -0x7f, 0xff - 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16))
    {
        axis = READ_STICK_AXIS(3);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
        axis = READ_STICK_AXIS(4);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
        axis = READ_STICK_AXIS(5);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
        axis = READ_STICK_AXIS(6);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }
#undef READ_STICK_AXIS

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static bool HIDAPI_DriverZUIKI_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverZUIKI_Context *ctx = (SDL_DriverZUIKI_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_ZUIKI_PROTOCOL
        HIDAPI_DumpPacket("ZUIKI packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        if (size == 8) {
            // Old firmware USB report for the SF30 Pro and SN30 Pro controllers
            HIDAPI_DriverZUIKI_HandleOldStatePacket(joystick, ctx, data, size);
        }
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverZUIKI_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverZUIKI_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverZUIKI = {
    SDL_HINT_JOYSTICK_HIDAPI_ZUIKI,
    true,
    HIDAPI_DriverZUIKI_RegisterHints,
    HIDAPI_DriverZUIKI_UnregisterHints,
    HIDAPI_DriverZUIKI_IsEnabled,
    HIDAPI_DriverZUIKI_IsSupportedDevice,
    HIDAPI_DriverZUIKI_InitDevice,
    HIDAPI_DriverZUIKI_GetDevicePlayerIndex,
    HIDAPI_DriverZUIKI_SetDevicePlayerIndex,
    HIDAPI_DriverZUIKI_UpdateDevice,
    HIDAPI_DriverZUIKI_OpenJoystick,
    HIDAPI_DriverZUIKI_RumbleJoystick,
    HIDAPI_DriverZUIKI_RumbleJoystickTriggers,
    HIDAPI_DriverZUIKI_GetJoystickCapabilities,
    HIDAPI_DriverZUIKI_SetJoystickLED,
    HIDAPI_DriverZUIKI_SendJoystickEffect,
    HIDAPI_DriverZUIKI_SetJoystickSensorsEnabled,
    HIDAPI_DriverZUIKI_CloseJoystick,
    HIDAPI_DriverZUIKI_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_ZUIKI

#endif // SDL_JOYSTICK_HIDAPI
