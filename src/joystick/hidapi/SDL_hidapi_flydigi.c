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

#ifdef SDL_JOYSTICK_HIDAPI_FLYDIGI

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_FLYDIGI_PROTOCOL
#endif

enum
{
    SDL_GAMEPAD_BUTTON_FLYDIGI_M1 = 11,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M2,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M3,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M4,
    SDL_GAMEPAD_BUTTON_FLYDIGI_FN,
    SDL_GAMEPAD_BUTTON_FLYDIGI_C,
    SDL_GAMEPAD_BUTTON_FLYDIGI_Z,
    SDL_GAMEPAD_NUM_FLYDIGI_BUTTONS_WITH_CZ,
};
#define SDL_GAMEPAD_NUM_FLYDIGI_BUTTONS_WITHOUT_CZ SDL_GAMEPAD_BUTTON_FLYDIGI_C

#define FLYDIGI_ACCEL_SCALE 256.f
#define SENSOR_INTERVAL_NS 8000000ULL
#define FLYDIGI_CMD_REPORT_ID 0x05
#define FLYDIGI_HAPTIC_COMMAND 0x0F
#define FLYDIGI_GET_CONFIG_COMMAND 0xEB

#define LOAD16(A, B)       (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))

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
    bool has_cz;
    Uint8 serial[6];
    Uint16 version;
    Uint16 version_beta;
    float accelScale;
    float gyroScale;
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint64 sensor_timestamp; // Microseconds. Simulate onboard clock. Advance by known rate: SENSOR_INTERVAL_NS == 8ms = 125 Hz
} SDL_DriverFlydigi_Context;

#pragma pack(push,1)
typedef struct
{
    bool sensors_supported;
    bool touchpad_01_supported;
    bool touchpad_02_supported;
    bool rumble_supported;
    bool rumble_type;
    bool rgb_supported;
    Uint8 device_type;
    Uint8 serial[6];
    Uint16 version;
    Uint16 version_beta;
    Uint16 pid;
} FLYDIGI_DEVICE_INFO;

#pragma pack(pop)


static void HIDAPI_DriverFlydigi_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_FLYDIGI, callback, userdata);
}

static void HIDAPI_DriverFlydigi_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_FLYDIGI, callback, userdata);
}

static bool HIDAPI_DriverFlydigi_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_FLYDIGI, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverFlydigi_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return SDL_IsJoystickFlydigiController(vendor_id, product_id) && interface_number == 2;
}

static bool HIDAPI_DriverFlydigi_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    device->context = ctx;

    if (device->product_id == USB_PRODUCT_FLYDIGI_VADER4_PRO) {
        const int VADER4PRO_REPORT_SIZE = 32;
        Uint8 data[USB_PACKET_LENGTH];
        int size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 80);
        if (size == VADER4PRO_REPORT_SIZE) {
            ctx->sensors_supported = true;
            ctx->rumble_supported = true;
        }
        const char VADER3_NAME[] = "Flydigi VADER3";
        const char VADER4_NAME[] = "Flydigi VADER4";
        ctx->has_cz = SDL_strncmp(device->name, VADER3_NAME, sizeof(VADER3_NAME)) == 0 || SDL_strncmp(device->name, VADER4_NAME, sizeof(VADER4_NAME)) == 0;
    }

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverFlydigi_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverFlydigi_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif

static bool HIDAPI_DriverFlydigi_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);

    // Initialize the joystick capabilities
    joystick->nbuttons = ctx->has_cz ? SDL_GAMEPAD_NUM_FLYDIGI_BUTTONS_WITH_CZ : SDL_GAMEPAD_NUM_FLYDIGI_BUTTONS_WITHOUT_CZ;
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    if (ctx->sensors_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 125.0f);
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 125.0f);


        ctx->accelScale = SDL_STANDARD_GRAVITY / FLYDIGI_ACCEL_SCALE;
    }

    return true;
}

static bool HIDAPI_DriverFlydigi_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;
    if (ctx->rumble_supported) {
        Uint8 rumble_packet[4] = { FLYDIGI_CMD_REPORT_ID, FLYDIGI_HAPTIC_COMMAND, 0x00, 0x00 };
        rumble_packet[2] = low_frequency_rumble >> 8;
        rumble_packet[3] = high_frequency_rumble >> 8;

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
        return true;
    } else {
        return SDL_Unsupported();
    }
}

static bool HIDAPI_DriverFlydigi_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverFlydigi_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;
    Uint32 caps = 0;
    if (ctx->rumble_supported) {
        caps |= SDL_JOYSTICK_CAP_RUMBLE;
    }
    return caps;
}

static bool HIDAPI_DriverFlydigi_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverFlydigi_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverFlydigi_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;
    if (ctx->sensors_supported) {
        ctx->sensors_enabled = enabled;
        return true;
    }
    return SDL_Unsupported();
}
static void HIDAPI_DriverFlydigi_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverFlydigi_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();
    if (data[0] != 0x04 && data[0] != 0xFE) {
        // We don't know how to handle this report
        return;
    }

    if (ctx->last_state[8] != data[8]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_FN, ((data[8] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[8] & 0x08) != 0));
    }

    if (ctx->last_state[9] != data[9]) {
        Uint8 hat;

        switch (data[9] & 0x0F) {
        case 0b0001:
            hat = SDL_HAT_UP;
            break;
        case 0b0011:
            hat = SDL_HAT_RIGHTUP;
            break;
        case 0b0010:
            hat = SDL_HAT_RIGHT;
            break;
        case 0b0110:
            hat = SDL_HAT_RIGHTDOWN;
            break;
        case 0b0100:
            hat = SDL_HAT_DOWN;
            break;
        case 0b1100:
            hat = SDL_HAT_LEFTDOWN;
            break;
        case 0b1000:
            hat = SDL_HAT_LEFT;
            break;
        case 0b1001:
            hat = SDL_HAT_LEFTUP;
            break;
        default:
            hat = SDL_HAT_CENTERED;
            break;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((data[9] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((data[9] & 0x20) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[9] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((data[9] & 0x80) != 0));
    }

    if (ctx->last_state[10] != data[10]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((data[10] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((data[10] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ((data[10] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ((data[10] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((data[10] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ((data[10] & 0x80) != 0));
    }

    if (ctx->last_state[7] != data[7]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_C, ((data[7] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_Z, ((data[7] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M1, ((data[7] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M2, ((data[7] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M3, ((data[7] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M4, ((data[7] & 0x20) != 0));
    }

#define READ_STICK_AXIS(offset) \
    (data[offset] == 0x7f ? 0 : (Sint16)HIDAPI_RemapVal((float)((int)data[offset] - 0x7f), -0x7f, 0xff - 0x7f, SDL_MIN_SINT16, SDL_MAX_SINT16))
    {
        axis = READ_STICK_AXIS(17);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
        axis = READ_STICK_AXIS(19);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
        axis = READ_STICK_AXIS(21);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
        axis = READ_STICK_AXIS(22);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }
#undef READ_STICK_AXIS

#define READ_TRIGGER_AXIS(offset) \
    (Sint16)(((int)data[offset] * 257) - 32768)
    {
        axis = READ_TRIGGER_AXIS(23);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
        axis = READ_TRIGGER_AXIS(24);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
    }
#undef READ_TRIGGER_AXIS

    if (ctx->sensors_enabled) {
        Uint64 sensor_timestamp;
        float values[3];

        // Note: we cannot use the time stamp of the receiving computer due to packet delay creating "spiky" timings.
        // The imu time stamp is intended to be the sample time of the on-board hardware.
        // In the absence of time stamp data from the data[], we can simulate that by
        // advancing a time stamp by the observed/known imu clock rate. This is 8ms = 125 Hz
        sensor_timestamp = ctx->sensor_timestamp;
        ctx->sensor_timestamp += SENSOR_INTERVAL_NS;

        // This device's IMU values are reported differently from SDL
        // Thus we perform a rotation of the coordinate system to match the SDL standard.
        values[0] = -LOAD16(data[26], data[27]) * DEG2RAD(65536) / INT16_MAX;  // Rotation around pitch axis
        values[1] = -LOAD16(data[18], data[20]) * DEG2RAD(65536) / INT16_MAX;   // Rotation around yaw axis
        values[2] = -LOAD16(data[29], data[30]) * DEG2RAD(1024) / INT16_MAX;  // Rotation around roll axis
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, sensor_timestamp, values, 3);

        values[0] = -LOAD16(data[11], data[12])  * ctx->accelScale; // Acceleration along pitch axis
        values[1] = LOAD16(data[15], data[16]) * ctx->accelScale;  // Acceleration along yaw axis
        values[2] = LOAD16(data[13], data[14]) * ctx->accelScale; // Acceleration along roll axis
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, sensor_timestamp, values, 3);
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static bool HIDAPI_DriverFlydigi_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_FLYDIGI_PROTOCOL
        HIDAPI_DumpPacket("Flydigi packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        HIDAPI_DriverFlydigi_HandleStatePacket(joystick, ctx, data, size);
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverFlydigi_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverFlydigi_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverFlydigi = {
    SDL_HINT_JOYSTICK_HIDAPI_FLYDIGI,
    true,
    HIDAPI_DriverFlydigi_RegisterHints,
    HIDAPI_DriverFlydigi_UnregisterHints,
    HIDAPI_DriverFlydigi_IsEnabled,
    HIDAPI_DriverFlydigi_IsSupportedDevice,
    HIDAPI_DriverFlydigi_InitDevice,
    HIDAPI_DriverFlydigi_GetDevicePlayerIndex,
    HIDAPI_DriverFlydigi_SetDevicePlayerIndex,
    HIDAPI_DriverFlydigi_UpdateDevice,
    HIDAPI_DriverFlydigi_OpenJoystick,
    HIDAPI_DriverFlydigi_RumbleJoystick,
    HIDAPI_DriverFlydigi_RumbleJoystickTriggers,
    HIDAPI_DriverFlydigi_GetJoystickCapabilities,
    HIDAPI_DriverFlydigi_SetJoystickLED,
    HIDAPI_DriverFlydigi_SendJoystickEffect,
    HIDAPI_DriverFlydigi_SetJoystickSensorsEnabled,
    HIDAPI_DriverFlydigi_CloseJoystick,
    HIDAPI_DriverFlydigi_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_FLYDIGI

#endif // SDL_JOYSTICK_HIDAPI
