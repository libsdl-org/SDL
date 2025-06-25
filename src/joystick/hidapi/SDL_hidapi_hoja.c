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

#include "../../SDL_hints_c.h"
#include "../SDL_sysjoystick.h"

#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"

#ifdef SDL_JOYSTICK_HIDAPI_HOJA

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_HOJA_PROTOCOL
#endif

#define HOJA_ACCEL_SCALE                 4096.f
#define HOJA_GYRO_MAX_DEGREES_PER_SECOND 2000.f

#define HOJA_DEVICE_NAME_SIZE 32
#define HOJA_DEVICE_POLLING_RATE 1000 // 1000 Hz
#define HOJA_DEVICE_REPORT_SIZE  64   // Size of all reports

#define HOJA_DEVICE_REPORT_ID_JOYSTICK_INPUT    0x01
#define HOJA_DEVICE_REPORT_ID_HAPTIC_OUTPUT     0x02
#define HOJA_DEVICE_REPORT_ID_COMMAND_OUTPUT    0x03
#define HOJA_DEVICE_REPORT_ID_COMMAND_INPUT     0x04

#define HOJA_DEVICE_COMMAND_GETINFO         0x01
#define HOJA_DEVICE_COMMAND_SETPLAYERNUM    0x02

#define HOJA_REPORT_IDX_BUTTONS_0   3
#define HOJA_REPORT_IDX_BUTTONS_1   4
#define HOJA_REPORT_IDX_BUTTONS_2   5
#define HOJA_REPORT_IDX_BUTTONS_3   6
#define HOJA_REPORT_IDX_LEFT_X      7
#define HOJA_REPORT_IDX_LEFT_Y      9
#define HOJA_REPORT_IDX_RIGHT_X     11
#define HOJA_REPORT_IDX_RIGHT_Y     13
#define HOJA_REPORT_IDX_LEFT_TRIGGER    15
#define HOJA_REPORT_IDX_RIGHT_TRIGGER   17
#define HOJA_REPORT_IDX_IMU_TIMESTAMP   19
#define HOJA_REPORT_IDX_IMU_ACCEL_X     21
#define HOJA_REPORT_IDX_IMU_ACCEL_Y     23
#define HOJA_REPORT_IDX_IMU_ACCEL_Z     25
#define HOJA_REPORT_IDX_IMU_GYRO_X      27
#define HOJA_REPORT_IDX_IMU_GYRO_Y      29
#define HOJA_REPORT_IDX_IMU_GYRO_Z      31

#define HOJA_REPORT_IDX_PLUG_STATUS     1
#define HOJA_REPORT_IDX_CHARGE_LEVEL    2

#define HOJA_OVERRIDE_SUPPORTED_DEBUG 1

#ifndef EXTRACTSINT16
#define EXTRACTSINT16(data, idx) ((Sint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#ifndef EXTRACTUINT16
#define EXTRACTUINT16(data, idx) ((Uint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif


typedef struct
{
    bool gyro_supported;
    bool accel_supported;
    bool haptics_supported;

    bool left_joystick_supported;
    bool right_joystick_supported;

    bool left_analog_trigger_supported;
    bool right_analog_trigger_supported;

    bool player_led_supported;
    bool powerstate_supported;

    char device_name[HOJA_DEVICE_NAME_SIZE];

    Uint16 api_version; // Version of the API this device supports

    Uint16 accelRange; // Example would be 2,4,8,16 +/- (g-force)
    Uint16 gyroRange;  // Example would be 125,250,500,1000,2000,4000 +/- (degrees per second)

    float accelScale; // Scale factor for accelerometer values
    float gyroScale;  // Scale factor for gyroscope values
    Uint8 last_state[USB_PACKET_LENGTH];

    Uint64 imu_timestamp; // Nanoseconds. We accumulate with received deltas
} SDL_DriverHoja_Context;

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
} HOJA_SENSORS;

typedef union
{
    struct
    {
        Uint8 haptics_supported : 1;
        Uint8 player_led_supported : 1;
        Uint8 accel_supported : 1;
        Uint8 gyro_supported : 1;

        Uint8 left_joystick_supported : 1;
        Uint8 right_joystick_supported : 1;

        Uint8 left_analog_trigger_supported : 1;
        Uint8 right_analog_trigger_supported : 1;
    };
    Uint8 value;
} HOJA_FEATURE_FLAGS;

static void HIDAPI_DriverHoja_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_HOJA, callback, userdata);
}

static void HIDAPI_DriverHoja_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_HOJA, callback, userdata);
}

static bool HIDAPI_DriverHoja_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_HOJA, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverHoja_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id == USB_VENDOR_RASPBERRYPI) {
        switch (product_id) {
        case USB_PRODUCT_HANDHELDLEGEND_HOJA_GAMEPAD:
            return true;
        default:
            break;
        }
    }
    return false;
}

static bool HIDAPI_DriverHoja_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    device->context = ctx;

    // Write our command to get device info
    Uint8 commandBuffer[HOJA_DEVICE_REPORT_SIZE-1] = { HOJA_DEVICE_REPORT_ID_COMMAND_OUTPUT, HOJA_DEVICE_COMMAND_GETINFO};

    //if (SDL_hid_write(device->dev, commandBuffer, sizeof(commandBuffer)) != sizeof(commandBuffer)) {
    //    SDL_free(ctx);
    //    return SDL_SetError("Couldn't send command to Hoja device");
    //}

    // Read the response from the device
    Uint8 deviceInfoData[HOJA_DEVICE_REPORT_SIZE];

    int deviceInfoAttempts = 6;

    while (deviceInfoAttempts-- > 0) {
        // Read the device info report
        int size = SDL_hid_read_timeout(device->dev, deviceInfoData, HOJA_DEVICE_REPORT_SIZE, 1000);
        if (size == HOJA_DEVICE_REPORT_SIZE && deviceInfoData[0] == HOJA_DEVICE_REPORT_ID_COMMAND_INPUT && deviceInfoData[1] == HOJA_DEVICE_COMMAND_GETINFO) {
            break; // We got a valid response
        }
        SDL_Delay(100); // Wait before retrying
    }

    if (!deviceInfoAttempts) {
        SDL_free(ctx);
        return SDL_SetError("Couldn't read device info from Hoja device");
    }

    // Extract information
    HOJA_FEATURE_FLAGS features = { .value = 0 };
    features.value = deviceInfoData[2];

    ctx->haptics_supported      = features.haptics_supported;
    ctx->player_led_supported   = features.player_led_supported;
    ctx->accel_supported        = features.accel_supported;
    ctx->gyro_supported         = features.gyro_supported;
    ctx->left_joystick_supported    = features.left_joystick_supported;
    ctx->right_joystick_supported   = features.right_joystick_supported;
    ctx->left_analog_trigger_supported  = features.left_analog_trigger_supported;
    ctx->right_analog_trigger_supported = features.right_analog_trigger_supported;

    #if defined(HOJA_OVERRIDE_SUPPORTED_DEBUG) && (HOJA_OVERRIDE_SUPPORTED_DEBUG==1)
    ctx->haptics_supported      = 1;
    ctx->player_led_supported   = 1;
    ctx->accel_supported    = 1;
    ctx->gyro_supported     = 1;
    ctx->left_joystick_supported    = 1;
    ctx->right_joystick_supported   = 1;
    ctx->left_analog_trigger_supported  = 1;
    ctx->right_analog_trigger_supported = 1;
    #endif

    ctx->api_version    = EXTRACTUINT16(deviceInfoData, 4);
    ctx->accelRange     = EXTRACTUINT16(deviceInfoData, 6); // Range in g-force
    ctx->gyroRange      = EXTRACTUINT16(deviceInfoData, 8); // Range in degrees per second

    // Determine device name
    const char name[HOJA_DEVICE_NAME_SIZE] = "Hoja Gamepad";

    // Use the name from the device if it isn't blank
    if (deviceInfoData[10] != 0) {
        // Copy the device name from the report
        SDL_utf8strlcpy(ctx->device_name, (const char *)&deviceInfoData[10], HOJA_DEVICE_NAME_SIZE);
    } else {
        // If the name is blank, we use the default name
        SDL_utf8strlcpy(ctx->device_name, name, HOJA_DEVICE_NAME_SIZE);
    }

    // Set the device name
    HIDAPI_SetDeviceName(device, ctx->device_name);

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverHoja_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverHoja_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    player_index = SDL_clamp(player_index, 0, 255); // Hoja supports 8 players, but up to 255 is allowed
    Uint8 player_num = (Uint8)player_index;
    Uint8 playerCommandBuffer[HOJA_DEVICE_REPORT_SIZE-1] = { HOJA_DEVICE_REPORT_ID_COMMAND_OUTPUT, HOJA_DEVICE_COMMAND_SETPLAYERNUM, player_num };

    SDL_hid_write(device->dev, playerCommandBuffer, sizeof(playerCommandBuffer));
}


#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif

static bool HIDAPI_DriverHoja_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)device->context;

    SDL_AssertJoysticksLocked();

    SDL_zeroa(ctx->last_state);

    joystick->nbuttons = 32; 
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;

    if (ctx->accel_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, (float)HOJA_DEVICE_POLLING_RATE);

        ctx->accelScale = SDL_STANDARD_GRAVITY / ctx->accelRange;
    }

    if (ctx->gyro_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, (float)HOJA_DEVICE_POLLING_RATE);
        // Hardware senses +/- N Degrees per second mapped to +/- INT16_MAX
        ctx->gyroScale = DEG2RAD(ctx->gyroRange) / INT16_MAX;
    }

    return true;
}

static bool HIDAPI_DriverHoja_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)device->context;

    return SDL_Unsupported();
}

static bool HIDAPI_DriverHoja_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverHoja_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)device->context;

    Uint32 caps = 0;
    if (ctx->haptics_supported) {
        caps |= SDL_JOYSTICK_CAP_RUMBLE;
    }
    if (ctx->player_led_supported) {
        caps |= SDL_JOYSTICK_CAP_PLAYER_LED;
    }
    return caps;
}

static bool HIDAPI_DriverHoja_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverHoja_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverHoja_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    // Our sensors are always enabled if supported, so we just set the flag
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)device->context;
    return true;
}

static void HIDAPI_DriverHoja_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverHoja_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis = 0;
    Sint16 accel = 0;
    Sint16 gyro = 0;
    Uint64 timestamp = SDL_GetTicksNS();
    float imu_values[3] = { 0 };

    if (ctx->last_state[HOJA_REPORT_IDX_BUTTONS_0] != data[HOJA_REPORT_IDX_BUTTONS_0]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x01)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x02)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x04)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x08)));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x10)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x20)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x40)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, (!(data[HOJA_REPORT_IDX_BUTTONS_0] & 0x80)));
    }

    if (ctx->last_state[HOJA_REPORT_IDX_BUTTONS_1] != data[HOJA_REPORT_IDX_BUTTONS_1]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x01)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x02)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x04)));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x08)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x10)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x20)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x40)));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (!(data[HOJA_REPORT_IDX_BUTTONS_1] & 0x80)));
    }

    if (ctx->last_state[HOJA_REPORT_IDX_BUTTONS_2] != data[HOJA_REPORT_IDX_BUTTONS_2]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, (!(data[HOJA_REPORT_IDX_BUTTONS_2] & 0x01)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, (!(data[HOJA_REPORT_IDX_BUTTONS_2] & 0x02)));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, (!(data[HOJA_REPORT_IDX_BUTTONS_2] & 0x04)));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, (!(data[HOJA_REPORT_IDX_BUTTONS_2] & 0x08)));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_TOUCHPAD, (!(data[HOJA_REPORT_IDX_BUTTONS_2] & 0x10)));
    }

    // Our analog inputs map to a signed Sint16 range of -32768 to 32767 from the device.

    if (ctx->left_joystick_supported) {
        axis = (Sint16)(data[HOJA_REPORT_IDX_LEFT_X] | (data[HOJA_REPORT_IDX_LEFT_X + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);

        axis = (Sint16)(data[HOJA_REPORT_IDX_LEFT_Y] | (data[HOJA_REPORT_IDX_LEFT_Y + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    }

    if (ctx->right_joystick_supported) {
        axis = (Sint16)(data[HOJA_REPORT_IDX_RIGHT_X] | (data[HOJA_REPORT_IDX_RIGHT_X + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);

        axis = (Sint16)(data[HOJA_REPORT_IDX_RIGHT_Y] | (data[HOJA_REPORT_IDX_RIGHT_Y + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }

    if (ctx->left_analog_trigger_supported) {
        axis = (Sint16)(data[HOJA_REPORT_IDX_LEFT_TRIGGER] | (data[HOJA_REPORT_IDX_LEFT_TRIGGER + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
    }

    if (ctx->right_analog_trigger_supported) {
        axis = (Sint16)(data[HOJA_REPORT_IDX_RIGHT_TRIGGER] | (data[HOJA_REPORT_IDX_RIGHT_TRIGGER + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
    }

    if (ctx->last_state[HOJA_REPORT_IDX_PLUG_STATUS] != data[HOJA_REPORT_IDX_PLUG_STATUS] ||
        ctx->last_state[HOJA_REPORT_IDX_CHARGE_LEVEL] != data[HOJA_REPORT_IDX_CHARGE_LEVEL]) {

        if (ctx->powerstate_supported) {
            SDL_PowerState state;
            Uint8 status    = data[HOJA_REPORT_IDX_PLUG_STATUS];
            int percent     = data[HOJA_REPORT_IDX_CHARGE_LEVEL];

            percent = SDL_clamp(percent, 0, 100); // Ensure percent is within valid range

            switch (status) {
            case 0:
                state = SDL_POWERSTATE_ON_BATTERY;
                break;
            case 2:
                state = SDL_POWERSTATE_CHARGING;
                break;
            case 3:
                state = SDL_POWERSTATE_CHARGED;
                percent = 100;
                break;
            default:
                state = SDL_POWERSTATE_UNKNOWN;
                percent = 0;
                break;
            }
            SDL_SendJoystickPowerInfo(joystick, state, percent);
        } else {
            SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_NO_BATTERY, 100);
        }
    }

    // Extract the IMU timestamp delta (in microseconds)
    Uint16 imu_timestamp_delta = EXTRACTUINT16(data, HOJA_REPORT_IDX_IMU_TIMESTAMP);

    // Check if we should process IMU data
    if (imu_timestamp_delta > 0) {

        // Process IMU timestamp by adding the delta to the accumulated timestamp and converting to nanoseconds
        ctx->imu_timestamp += ((Uint64) imu_timestamp_delta * 1000);

        // Process Accelerometer
        if (ctx->accel_supported) {

            accel = (Sint16)(data[HOJA_REPORT_IDX_IMU_ACCEL_X] | (data[HOJA_REPORT_IDX_IMU_ACCEL_X + 1] << 8));
            imu_values[0] = (float)accel * ctx->accelScale; // X-axis acceleration

            accel = (Sint16)(data[HOJA_REPORT_IDX_IMU_ACCEL_Y] | (data[HOJA_REPORT_IDX_IMU_ACCEL_Y + 1] << 8));
            imu_values[1] = (float)accel * ctx->accelScale; // Y-axis acceleration

            accel = (Sint16)(data[HOJA_REPORT_IDX_IMU_ACCEL_Z] | (data[HOJA_REPORT_IDX_IMU_ACCEL_Z + 1] << 8));
            imu_values[2] = (float)accel * ctx->accelScale; // Z-axis acceleration

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, ctx->imu_timestamp, imu_values, 3);
        }

        // Process Gyroscope
        if (ctx->gyro_supported) {
            gyro = (Sint16)(data[HOJA_REPORT_IDX_IMU_GYRO_X] | (data[HOJA_REPORT_IDX_IMU_GYRO_X + 1] << 8));
            imu_values[0] = (float)gyro * ctx->gyroScale; // X-axis rotation

            gyro = (Sint16)(data[HOJA_REPORT_IDX_IMU_GYRO_Y] | (data[HOJA_REPORT_IDX_IMU_GYRO_Y + 1] << 8));
            imu_values[1] = (float)gyro * ctx->gyroScale; // Y-axis rotation

            gyro = (Sint16)(data[HOJA_REPORT_IDX_IMU_GYRO_Z] | (data[HOJA_REPORT_IDX_IMU_GYRO_Z + 1] << 8));
            imu_values[2] = (float)gyro * ctx->gyroScale; // Z-axis rotation

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, ctx->imu_timestamp, imu_values, 3);
        }
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static bool HIDAPI_DriverHoja_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverHoja_Context *ctx = (SDL_DriverHoja_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_HOJA_PROTOCOL
        HIDAPI_DumpPacket("Hoja packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        HIDAPI_DriverHoja_HandleStatePacket(joystick, ctx, data, size);
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverHoja_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverHoja_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverHoja = {
    SDL_HINT_JOYSTICK_HIDAPI_HOJA,
    true,
    HIDAPI_DriverHoja_RegisterHints,
    HIDAPI_DriverHoja_UnregisterHints,
    HIDAPI_DriverHoja_IsEnabled,
    HIDAPI_DriverHoja_IsSupportedDevice,
    HIDAPI_DriverHoja_InitDevice,
    HIDAPI_DriverHoja_GetDevicePlayerIndex,
    HIDAPI_DriverHoja_SetDevicePlayerIndex,
    HIDAPI_DriverHoja_UpdateDevice,
    HIDAPI_DriverHoja_OpenJoystick,
    HIDAPI_DriverHoja_RumbleJoystick,
    HIDAPI_DriverHoja_RumbleJoystickTriggers,
    HIDAPI_DriverHoja_GetJoystickCapabilities,
    HIDAPI_DriverHoja_SetJoystickLED,
    HIDAPI_DriverHoja_SendJoystickEffect,
    HIDAPI_DriverHoja_SetJoystickSensorsEnabled,
    HIDAPI_DriverHoja_CloseJoystick,
    HIDAPI_DriverHoja_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_HOJA

#endif // SDL_JOYSTICK_HIDAPI
