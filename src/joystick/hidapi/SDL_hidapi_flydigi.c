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
#include "SDL_hidapi_flydigi.h"

#ifdef SDL_JOYSTICK_HIDAPI_FLYDIGI

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_FLYDIGI_PROTOCOL
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif

enum
{
    SDL_GAMEPAD_BUTTON_FLYDIGI_M1 = 11,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M2,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M3,
    SDL_GAMEPAD_BUTTON_FLYDIGI_M4,
    SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS
};

/* Rate of IMU Sensor Packets over wireless dongle observed in testcontroller at 1000hz */
#define SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ 1000
#define SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS    (SDL_NS_PER_SECOND / SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ)
/* Rate of IMU Sensor Packets over wired connection observed in testcontroller at 500hz */
#define SENSOR_INTERVAL_VADER4_PRO_WIRED_RATE_HZ  500
#define SENSOR_INTERVAL_VADER4_PRO_WIRED_NS      (SDL_NS_PER_SECOND / SENSOR_INTERVAL_VADER4_PRO_WIRED_RATE_HZ)

/* Rate of IMU Sensor Packets over wireless dongle observed in testcontroller at 295hz */
#define SENSOR_INTERVAL_APEX5_DONGLE_RATE_HZ     295
#define SENSOR_INTERVAL_APEX5_DONGLE_NS         (SDL_NS_PER_SECOND / SENSOR_INTERVAL_APEX5_DONGLE_RATE_HZ)
/* Rate of IMU Sensor Packets over wired connection observed in testcontroller at 970hz */
#define SENSOR_INTERVAL_APEX5_WIRED_RATE_HZ      970
#define SENSOR_INTERVAL_APEX5_WIRED_NS          (SDL_NS_PER_SECOND / SENSOR_INTERVAL_APEX5_WIRED_RATE_HZ)

#define FLYDIGI_ACQUIRE_CONTROLLER_HEARTBEAT_TIME  1000 * 60

#define FLYDIGI_V1_CMD_REPORT_ID    0x05
#define FLYDIGI_V1_HAPTIC_COMMAND   0x0F
#define FLYDIGI_V1_GET_INFO_COMMAND 0xEC

#define FLYDIGI_V2_CMD_REPORT_ID    0x03
#define FLYDIGI_V2_MAGIC1           0x5A
#define FLYDIGI_V2_MAGIC2           0xA5
#define FLYDIGI_V2_GET_INFO_COMMAND 0x01
#define FLYDIGI_V2_HAPTIC_COMMAND   0x12
#define FLYDIGI_V2_ACQUIRE_CONTROLLER_COMMAND 0x1c

#define LOAD16(A, B)       (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))

typedef struct
{
    Uint8 deviceID;
    bool has_cz;
    bool has_lmrm;
    bool wireless;
    bool sensors_supported;
    bool sensors_enabled;
    Uint16 firmware_version;
    Uint64 sensor_timestamp_ns; // Simulate onboard clock. Advance by known time step. Nanoseconds.
    Uint64 sensor_timestamp_step_ns; // Based on observed rate of receipt of IMU sensor packets.
    float accelScale;
    float gyroScale;
    Uint64 last_heartbeat;
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

static bool HIDAPI_DriverFlydigi_InitControllerV1(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;

    // Detecting the Vader 2 can take over 1000 read retries, so be generous here
    for (int attempt = 0; ctx->deviceID == 0 && attempt < 30; ++attempt) {
        const Uint8 request[] = { FLYDIGI_V1_CMD_REPORT_ID, FLYDIGI_V1_GET_INFO_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
                ctx->firmware_version = LOAD16(data[9], data[10]);

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
    return true;
}

static bool GetReply(SDL_HIDAPI_Device* device, Uint8 command, Uint8* data, size_t length)
{
    for (int i = 0; i < 100; ++i) {
        SDL_Delay(1);

        int size = SDL_hid_read_timeout(device->dev, data, length, 0);
        if (size < 0) {
            break;
        }
        if (size == 0) {
            continue;
        }

#ifdef DEBUG_FLYDIGI_PROTOCOL
        HIDAPI_DumpPacket("Flydigi packet: size = %d", data, size);
#endif

        if (size == 32 && data[1] == FLYDIGI_V2_MAGIC1 && data[2] == FLYDIGI_V2_MAGIC2 && data[3] == command) {
            return true;
        }
    }
    return false;
}

static bool SDL_HIDAPI_Flydigi_SendHeartbeat(SDL_HIDAPI_Device *device)
{
    const Uint8 acquireControllerCmd[] = { FLYDIGI_V2_CMD_REPORT_ID, FLYDIGI_V2_MAGIC1, FLYDIGI_V2_MAGIC2, FLYDIGI_V2_ACQUIRE_CONTROLLER_COMMAND, 23, 1, 83, 68, 76, 0 };
    if (SDL_hid_write(device->dev, acquireControllerCmd, sizeof(acquireControllerCmd)) < 0) {
        return SDL_SetError("Couldn't enable input reports");
    }

    Uint8 acquireControllerData[USB_PACKET_LENGTH];
    if (!GetReply(device, FLYDIGI_V2_ACQUIRE_CONTROLLER_COMMAND, acquireControllerData, sizeof(acquireControllerData))) {
        return SDL_SetError("Controller acquiring is not supported");
    }
    if (acquireControllerData[6] != 1) {
        return SDL_SetError("Controller acquiring is disabled");
    }
    return true;
}

static bool HIDAPI_DriverFlydigi_InitControllerV2(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;

    const Uint8 query_info[] = { FLYDIGI_V2_CMD_REPORT_ID, FLYDIGI_V2_MAGIC1, FLYDIGI_V2_MAGIC2, FLYDIGI_V2_GET_INFO_COMMAND, 2, 0 };
    if (SDL_hid_write(device->dev, query_info, sizeof(query_info)) < 0) {
        return SDL_SetError("Couldn't query controller info");
    }

    Uint8 data[USB_PACKET_LENGTH];
    if (GetReply(device, FLYDIGI_V2_GET_INFO_COMMAND, data, sizeof(data))) {
        ctx->deviceID = data[6];
        ctx->firmware_version = LOAD16(data[17], data[16]);

        switch (data[7]) {
        case 1:
            // Wired connection
            ctx->wireless = false;
            break;
        case 2:
            // Wireless connection
            ctx->wireless = true;
            break;
        default:
            break;
        }
    }

    ctx->last_heartbeat = SDL_GetTicks();

    return SDL_HIDAPI_Flydigi_SendHeartbeat(device);
}

static void HIDAPI_DriverFlydigi_UpdateDeviceIdentity(SDL_HIDAPI_Device *device)
{
    SDL_DriverFlydigi_Context *ctx = (SDL_DriverFlydigi_Context *)device->context;

    Uint8 controller_type = SDL_FLYDIGI_UNKNOWN;
    switch (ctx->deviceID) {
    case 19:
        controller_type = SDL_FLYDIGI_APEX2;
        break;
    case 24:
    case 26:
    case 29:
        controller_type = SDL_FLYDIGI_APEX3;
        break;
    case 84:
        controller_type = SDL_FLYDIGI_APEX4;
        break;
    case 20:
    case 21:
    case 23:
        controller_type = SDL_FLYDIGI_VADER2;
        break;
    case 22:
        controller_type = SDL_FLYDIGI_VADER2_PRO;
        break;
    case 28:
        controller_type = SDL_FLYDIGI_VADER3;
        break;
    case 80:
    case 81:
        controller_type = SDL_FLYDIGI_VADER3_PRO;
        break;
    case 85:
    case 91:
    case 105:
        controller_type = SDL_FLYDIGI_VADER4_PRO;
        break;
    case 128:
    case 129:
    case 133:
    case 134:
        controller_type = SDL_FLYDIGI_APEX5;
        break;
    default:
        // Try to guess from the name of the controller
        if (SDL_strstr(device->name, "VADER") != NULL) {
            if (SDL_strstr(device->name, "VADER2") != NULL) {
                controller_type = SDL_FLYDIGI_VADER2;
            } else if (SDL_strstr(device->name, "VADER3") != NULL) {
                controller_type = SDL_FLYDIGI_VADER3;
            } else if (SDL_strstr(device->name, "VADER4") != NULL) {
                controller_type = SDL_FLYDIGI_VADER4;
            }
        } else if (SDL_strstr(device->name, "APEX") != NULL) {
            if (SDL_strstr(device->name, "APEX2") != NULL) {
                controller_type = SDL_FLYDIGI_APEX2;
            } else if (SDL_strstr(device->name, "APEX3") != NULL) {
                controller_type = SDL_FLYDIGI_APEX3;
            } else if (SDL_strstr(device->name, "APEX4") != NULL) {
                controller_type = SDL_FLYDIGI_APEX4;
            } else if (SDL_strstr(device->name, "APEX5") != NULL) {
                controller_type = SDL_FLYDIGI_APEX5;
            }
        }
        break;
    }
    device->guid.data[15] = controller_type;

    // This is the previous sensor default of 125hz.
    // Override this in the switch statement below based on observed sensor packet rate.
    ctx->sensor_timestamp_step_ns = SDL_NS_PER_SECOND / 125;

    switch (controller_type) {
    case SDL_FLYDIGI_APEX2:
        HIDAPI_SetDeviceName(device, "Flydigi Apex 2");
        break;
    case SDL_FLYDIGI_APEX3:
        HIDAPI_SetDeviceName(device, "Flydigi Apex 3");
        break;
    case SDL_FLYDIGI_APEX4:
        // The Apex 4 controller has sensors, but they're only reported when gyro mouse is enabled
        HIDAPI_SetDeviceName(device, "Flydigi Apex 4");
        break;
    case SDL_FLYDIGI_APEX5:
        HIDAPI_SetDeviceName(device, "Flydigi Apex 5");
        ctx->has_lmrm = true;

        // The Apex 5 gyro values are only usable on firmware 7.0.3.0 and newer
        if (ctx->firmware_version >= 0x7030) {
            ctx->sensors_supported = true;
            ctx->accelScale = SDL_STANDARD_GRAVITY / 4096.0f;
            ctx->gyroScale = DEG2RAD(2000.0f);
            ctx->sensor_timestamp_step_ns = ctx->wireless ? SENSOR_INTERVAL_APEX5_DONGLE_NS : SENSOR_INTERVAL_APEX5_WIRED_NS;
        }
        break;
    case SDL_FLYDIGI_VADER2:
        // The Vader 2 controller has sensors, but they're only reported when gyro mouse is enabled
        HIDAPI_SetDeviceName(device, "Flydigi Vader 2");
        ctx->has_cz = true;
        break;
    case SDL_FLYDIGI_VADER2_PRO:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 2 Pro");
        ctx->has_cz = true;
        break;
    case SDL_FLYDIGI_VADER3:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 3");
        ctx->has_cz = true;
        break;
    case SDL_FLYDIGI_VADER3_PRO:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 3 Pro");
        ctx->has_cz = true;
        ctx->sensors_supported = true;
        ctx->accelScale = SDL_STANDARD_GRAVITY / 256.0f;
        ctx->sensor_timestamp_step_ns = ctx->wireless ? SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS : SENSOR_INTERVAL_VADER4_PRO_WIRED_NS;
        break;
    case SDL_FLYDIGI_VADER4:
    case SDL_FLYDIGI_VADER4_PRO:
        HIDAPI_SetDeviceName(device, "Flydigi Vader 4 Pro");
        ctx->has_cz = true;
        ctx->sensors_supported = true;
        ctx->accelScale = SDL_STANDARD_GRAVITY / 256.0f;
        ctx->sensor_timestamp_step_ns = ctx->wireless ? SENSOR_INTERVAL_VADER4_PRO_DONGLE_NS : SENSOR_INTERVAL_VADER4_PRO_WIRED_NS;
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

    bool initialized;
    if (device->vendor_id == USB_VENDOR_FLYDIGI_V1) {
        initialized = HIDAPI_DriverFlydigi_InitControllerV1(device);
    } else {
        initialized = HIDAPI_DriverFlydigi_InitControllerV2(device);
    }
    if (!initialized) {
        return false;
    }

    HIDAPI_DriverFlydigi_UpdateDeviceIdentity(device);

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverFlydigi_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverFlydigi_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

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
    if (ctx->has_lmrm) {
        joystick->nbuttons += 2;
    }
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;

    if (ctx->wireless) {
        joystick->connection_state = SDL_JOYSTICK_CONNECTION_WIRELESS;
    }

    if (ctx->sensors_supported) {
        const float flSensorRate = ctx->wireless ? (float)SENSOR_INTERVAL_VADER4_PRO_DONGLE_RATE_HZ : (float)SENSOR_INTERVAL_VADER4_PRO_WIRED_RATE_HZ;
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, flSensorRate);
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, flSensorRate);
    }
    return true;
}

static bool HIDAPI_DriverFlydigi_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    if (device->vendor_id == USB_VENDOR_FLYDIGI_V1) {
        Uint8 rumble_packet[] = { FLYDIGI_V1_CMD_REPORT_ID, FLYDIGI_V1_HAPTIC_COMMAND, 0x00, 0x00 };
        rumble_packet[2] = low_frequency_rumble >> 8;
        rumble_packet[3] = high_frequency_rumble >> 8;

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    } else {
        Uint8 rumble_packet[] = { FLYDIGI_V2_CMD_REPORT_ID, FLYDIGI_V2_MAGIC1, FLYDIGI_V2_MAGIC2, FLYDIGI_V2_HAPTIC_COMMAND, 6, 0, 0, 0, 0, 0 };
        rumble_packet[5] = low_frequency_rumble >> 8;
        rumble_packet[6] = high_frequency_rumble >> 8;

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
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

static void HIDAPI_DriverFlydigi_HandleStatePacketV1(SDL_Joystick *joystick, SDL_DriverFlydigi_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();
    if (data[0] != 0x04 || data[1] != 0xFE) {
        // We don't know how to handle this report
        return;
    }

    Uint8 extra_button_index = SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS;

    if (ctx->last_state[9] != data[9]) {
        Uint8 hat;

        switch (data[9] & 0x0F) {
        case 0x01u:
            hat = SDL_HAT_UP;
            break;
        case 0x02u | 0x01u:
            hat = SDL_HAT_RIGHTUP;
            break;
        case 0x02u:
            hat = SDL_HAT_RIGHT;
            break;
        case 0x02u | 0x04u:
            hat = SDL_HAT_RIGHTDOWN;
            break;
        case 0x04u:
            hat = SDL_HAT_DOWN;
            break;
        case 0x08u | 0x04u:
            hat = SDL_HAT_LEFTDOWN;
            break;
        case 0x08u:
            hat = SDL_HAT_LEFT;
            break;
        case 0x08u | 0x01u:
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
        // SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[8] & 0x01) != 0));
        // The '-' button is only available on the Vader 2, for simplicity let's ignore that
        // SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[8] & 0x10) != 0));
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

        const float flAccelScale = ctx->accelScale;
        values[0] = -LOAD16(data[11], data[12]) * flAccelScale; // Acceleration along pitch axis
        values[1] = LOAD16(data[15], data[16]) * flAccelScale;  // Acceleration along yaw axis
        values[2] = LOAD16(data[13], data[14]) * flAccelScale;  // Acceleration along roll axis
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, sensor_timestamp, values, 3);
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static void HIDAPI_DriverFlydigi_HandleStatePacketV2(SDL_Joystick *joystick, SDL_DriverFlydigi_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    if (size > 0 && data[0] != 0x5A) {
        // If first byte is not 0x5A, it must be REPORT_ID, we need to remove it.
        ++data;
        --size;
    }
    if (size < 31 || data[0] != 0x5A || data[1] != 0xA5 || data[2] != 0xEF) {
        // We don't know how to handle this report
        return;
    }

    Uint8 extra_button_index = SDL_GAMEPAD_NUM_BASE_FLYDIGI_BUTTONS;

    if (ctx->last_state[11] != data[11]) {
        Uint8 hat;

        switch (data[11] & 0x0F) {
        case 0x01u:
            hat = SDL_HAT_UP;
            break;
        case 0x02u | 0x01u:
            hat = SDL_HAT_RIGHTUP;
            break;
        case 0x02u:
            hat = SDL_HAT_RIGHT;
            break;
        case 0x02u | 0x04u:
            hat = SDL_HAT_RIGHTDOWN;
            break;
        case 0x04u:
            hat = SDL_HAT_DOWN;
            break;
        case 0x08u | 0x04u:
            hat = SDL_HAT_LEFTDOWN;
            break;
        case 0x08u:
            hat = SDL_HAT_LEFT;
            break;
        case 0x08u | 0x01u:
            hat = SDL_HAT_LEFTUP;
            break;
        default:
            hat = SDL_HAT_CENTERED;
            break;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((data[11] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((data[11] & 0x20) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[11] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((data[11] & 0x80) != 0));
    }

    if (ctx->last_state[12] != data[12]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((data[12] & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((data[12] & 0x02) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ((data[12] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, ((data[12] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((data[12] & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ((data[12] & 0x80) != 0));
    }

    if (ctx->last_state[13] != data[13]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M1, ((data[13] & 0x04) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M2, ((data[13] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M3, ((data[13] & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_FLYDIGI_M4, ((data[13] & 0x20) != 0));
        if (ctx->has_lmrm) {
            SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[13] & 0x40) != 0));
            SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[13] & 0x80) != 0));
        }
    }

    if (ctx->last_state[14] != data[14]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((data[14] & 0x08) != 0));
        SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[14] & 0x01) != 0));
        // The '-' button is only available on the Vader 2, for simplicity let's ignore that
        SDL_SendJoystickButton(timestamp, joystick, extra_button_index++, ((data[8] & 0x10) != 0));
    }

    axis = LOAD16(data[3], data[4]);
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
    axis = -LOAD16(data[5], data[6]);
    if (axis <= -32768) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    axis = LOAD16(data[7], data[8]);
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
    axis = -LOAD16(data[9], data[10]);
    if (axis <= -32768) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);

#define READ_TRIGGER_AXIS(offset) \
    (Sint16)(((int)data[offset] * 257) - 32768)
    {
        axis = READ_TRIGGER_AXIS(15);
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
        axis = READ_TRIGGER_AXIS(16);
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

        const float flGyroScale = ctx->gyroScale;
        values[0] = HIDAPI_RemapVal((float)LOAD16(data[17], data[18]), INT16_MIN, INT16_MAX, -flGyroScale, flGyroScale);
        values[1] = HIDAPI_RemapVal((float)LOAD16(data[21], data[22]), INT16_MIN, INT16_MAX, -flGyroScale, flGyroScale);
        values[2] = HIDAPI_RemapVal(-(float)LOAD16(data[19], data[20]), INT16_MIN, INT16_MAX, -flGyroScale, flGyroScale);
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, sensor_timestamp, values, 3);

        const float flAccelScale = ctx->accelScale;
        values[0] = LOAD16(data[23], data[24]) * flAccelScale;  // Acceleration along pitch axis
        values[1] = LOAD16(data[27], data[28]) * flAccelScale;  // Acceleration along yaw axis
        values[2] = -LOAD16(data[25], data[26]) * flAccelScale; // Acceleration along roll axis
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
        if (device->vendor_id == USB_VENDOR_FLYDIGI_V1) {
            HIDAPI_DriverFlydigi_HandleStatePacketV1(joystick, ctx, data, size);
        } else {
            HIDAPI_DriverFlydigi_HandleStatePacketV2(joystick, ctx, data, size);
        }
    }

    if (device->vendor_id == USB_VENDOR_FLYDIGI_V2) {
        Uint64 now = SDL_GetTicks();
        if (now >= (ctx->last_heartbeat + FLYDIGI_ACQUIRE_CONTROLLER_HEARTBEAT_TIME)) {
            if (!SDL_HIDAPI_Flydigi_SendHeartbeat(device)) {
                // We can no longer acquire the device, mark it as disconnected
                HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
                return false;
            }
            ctx->last_heartbeat = now;
        }
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverFlydigi_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    const Uint8 acquireControllerCmd[] = { FLYDIGI_V2_CMD_REPORT_ID, FLYDIGI_V2_MAGIC1, FLYDIGI_V2_MAGIC2, FLYDIGI_V2_ACQUIRE_CONTROLLER_COMMAND, 23, 0, 83, 68, 76, 0 };
    SDL_hid_write(device->dev, acquireControllerCmd, sizeof(acquireControllerCmd));
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
