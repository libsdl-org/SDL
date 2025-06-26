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
    SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS
};

/* Rate of IMU Sensor Packets over wireless Dongle observed in testcontroller tool at 1000hz */
#define SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ 1000
#define SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS    (SDL_NS_PER_SECOND / SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ)
/* Rate of IMU Sensor Packets over wired observed in testcontroller tool connection at 500hz */
#define SENSOR_INTERVAL_VADER_PRO4_WIRED_RATE_HZ  500
#define SENSOR_INTERVAL_VADER_PRO4_WIRED_NS     (SDL_NS_PER_SECOND / SENSOR_INTERVAL_VADER_PRO4_WIRED_RATE_HZ)

#define FLYDIGI_CMD_REPORT_ID 0x05
#define FLYDIGI_HAPTIC_COMMAND 0x0F
#define FLYDIGI_GET_CONFIG_COMMAND 0xEB
#define FLYDIGI_GET_INFO_COMMAND 0xEC

#define LOAD16(A, B)       (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))

typedef struct
{
    Uint8 deviceID;
    bool has_cz;
    bool wireless;
    bool sensors_supported;
    bool sensors_enabled;
    Uint16 firmware_version;
    Uint64 sensor_timestamp_ns; // Simulate onboard clock. Advance by known time step. Nanoseconds.
    Uint64 sensor_timestamp_step_ns; // Based on observed rate of receipt of IMU sensor packets.
    float accelScale;
    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverFlydigi_Context;


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

static void UpdateDeviceIdentity(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;

    // Detecting the Vader 2 can take over 1000 read retries, so be generous here
    for (int attempt = 0; ctx->deviceID == 0 && attempt < 30; ++attempt) {
        const Uint8 request[] = { FLYDIGI_CMD_REPORT_ID, FLYDIGI_GET_INFO_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        // This write will occasionally return -1, so ignore failure here and try again
        (void)SDL_hid_write(device->dev, request, sizeof(request));

        // Read the reply
        for (int i = 0; i < 100; ++i) {
            SDL_Delay(1);

            Uint8 data[USB_PACKET_LENGTH];
            int size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0);
            if (size < 0) {
                break;
            }
            if (size == 0) {
                continue;
            }

#ifdef DEBUG_FLYDIGI_PROTOCOL
            HIDAPI_DumpPacket("Flydigi packet: size = %d", data, size);
#endif
            if (size == 32 && data[15] == 236) {
                ctx->deviceID = data[3];
                ctx->firmware_version = data[9] | (data[10] << 8);

                char serial[9];
                (void)SDL_snprintf(serial, sizeof(serial), "%.2x%.2x%.2x%.2x", data[5], data[6], data[7], data[8]);
                HIDAPI_SetDeviceSerial(device, serial);

                // The Vader 2 with firmware 6.0.4.9 doesn't report the connection state
                if (ctx->firmware_version >= 0x6400) {
                    switch (data[13]) {
                    case 0:
                        // Wireless connection
                        ctx->wireless = true;
                        break;
                    case 1:
                        // Wired connection
                        ctx->wireless = false;
                        break;
                    default:
                        break;
                    }
                }

                // Done!
                break;
            }
        }
    }

    if (ctx->deviceID == 0) {
        // Try to guess from the name of the controller
        if (SDL_strstr(device->name, "VADER") != NULL) {
            if (SDL_strstr(device->name, "VADER2") != NULL) {
                ctx->deviceID = 20;
            } else if (SDL_strstr(device->name, "VADER3") != NULL) {
                ctx->deviceID = 28;
            } else if (SDL_strstr(device->name, "VADER4") != NULL) {
                ctx->deviceID = 85;
            }
        } else if (SDL_strstr(device->name, "APEX") != NULL) {
            if (SDL_strstr(device->name, "APEX2") != NULL) {
                ctx->deviceID = 19;
            } else if (SDL_strstr(device->name, "APEX3") != NULL) {
                ctx->deviceID = 24;
            } else if (SDL_strstr(device->name, "APEX4") != NULL) {
                ctx->deviceID = 84;
            }
        }
    }
    device->guid.data[15] = ctx->deviceID;

    // This is the previous sensor default of 125hz.
    // Override this in the switch statement below based on observed sensor packet rate.
    ctx->sensor_timestamp_step_ns = SDL_NS_PER_SECOND / 125;

    switch (ctx->deviceID) {
    case 19:
        HIDAPI_SetDeviceName(device, "Flydigi Apex 2");
        break;
    case 24:
    case 26:
    case 29:
        HIDAPI_SetDeviceName(device, "Flydigi Apex 3");
        break;
    case 84:
        // The Apex 4 controller has sensors, but they're only reported when gyro mouse is enabled
        HIDAPI_SetDeviceName(device, "Flydigi Apex 4");
        break;
    case 20:
    case 21:
    case 23:
        // The Vader 2 controller has sensors, but they're only reported when gyro mouse is enabled
        HIDAPI_SetDeviceName(device, "Flydigi Vader 2");
        ctx->has_cz = true;
        break;
    case 22:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 2 Pro");
        ctx->has_cz = true;
        break;
    case 28:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 3");
        ctx->has_cz = true;
        break;
    case 80:
    case 81:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 3 Pro");
        ctx->has_cz = true;
        ctx->sensors_supported = true;
        ctx->accelScale = SDL_STANDARD_GRAVITY / 256.0f;
        ctx->sensor_timestamp_step_ns = ctx->wireless ? SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS : SENSOR_INTERVAL_VADER_PRO4_WIRED_NS;
        break;
    case 85:
    case 105:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 4 Pro");
        ctx->has_cz = true;
        ctx->sensors_supported = true;
        ctx->accelScale = SDL_STANDARD_GRAVITY / 256.0f;
        ctx->sensor_timestamp_step_ns = ctx->wireless ? SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS : SENSOR_INTERVAL_VADER_PRO4_WIRED_NS;
        break;
    default:
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "Unknown FlyDigi controller with ID %d, name '%s'", ctx->deviceID, device->name);
        break;
    }
}

static bool HIDAPI_DriverFlydigi_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    device->context = ctx;

    UpdateDeviceIdentity(device);

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
    joystick->nbuttons = SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS;
    if (ctx->has_cz) {
        joystick->nbuttons += 2;
    }
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    if (ctx->wireless) {
        joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
    }

    if (ctx->sensors_supported) {

        const float flSensorRate = ctx->wireless ? (float)SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ : (float)SENSOR_INTERVAL_VADER_PRO4_WIRED_RATE_HZ;
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, flSensorRate);
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, flSensorRate);
    }

    return true;
}

static bool HIDAPI_DriverFlydigi_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    Uint8 rumble_packet[4] = { FLYDIGI_CMD_REPORT_ID, FLYDIGI_HAPTIC_COMMAND, 0x00, 0x00 };
    rumble_packet[2] = low_frequency_rumble >> 8;
    rumble_packet[3] = high_frequency_rumble >> 8;

    if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }
    return true;
}

static bool HIDAPI_DriverFlydigi_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverFlydigi_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return SDL_JOYSTICK_CAP_RUMBLE;
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

    Uint8 extra_button_index = SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS;

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
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M1, ((data[7] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M2, ((data[7] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M3, ((data[7] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M4, ((data[7] & 0x20) != 0));
        if (ctx->has_cz) {
            SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[7] & 0x01) != 0));
            SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[7] & 0x02) != 0));
        }
    }

    if (ctx->last_state[8] != data[8]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[8] & 0x08) != 0));
        // The '+' button is used to toggle gyro mouse mode, so don't pass that to the application
        //SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[8] & 0x01) != 0));
        // The '-' button is only available on the Vader 2, for simplicity let's ignore that
        //SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[8] & 0x10) != 0));
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

        // Advance the imu sensor time stamp based on the observed rate of receipt of packets in the testcontroller app.
        // This varies between Product ID and connection type.
        sensor_timestamp = ctx->sensor_timestamp_ns;
        ctx->sensor_timestamp_ns += ctx->sensor_timestamp_step_ns;

        // Pitch and yaw scales may be receiving extra filtering for the sake of bespoke direct mouse output.
        // As result, roll has a different scaling factor than pitch and yaw.
        // These values were estimated using the testcontroller tool in lieux of hard data sheet references.
        const float flPitchAndYawScale = DEG2RAD(72000.0f);
        const float flRollScale = DEG2RAD(1200.0f);
        values[0] = HIDAPI_RemapVal(-1.0f * LOAD16(data[26], data[27]), INT16_MIN, INT16_MAX, -flPitchAndYawScale, flPitchAndYawScale);
        values[1] = HIDAPI_RemapVal(-1.0f * LOAD16(data[18], data[20]), INT16_MIN, INT16_MAX, -flPitchAndYawScale, flPitchAndYawScale);
        values[2] = HIDAPI_RemapVal(-1.0f * LOAD16(data[29], data[30]), INT16_MIN, INT16_MAX, -flRollScale, flRollScale);

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
