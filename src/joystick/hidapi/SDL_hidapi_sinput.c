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

#ifdef SDL_JOYSTICK_HIDAPI_SINPUT

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_SINPUT_PROTOCOL
#endif

#if 1
#define DEBUG_SINPUT_INIT
#endif

#define SINPUT_DEVICE_NAME_SIZE     32
#define SINPUT_DEVICE_POLLING_RATE  1000 // 1000 Hz
#define SINPUT_DEVICE_REPORT_SIZE   64   // Size of input reports (And CMD Input reports)
#define SINPUT_DEVICE_REPORT_COMMAND_SIZE 48 // Size of command OUTPUT reports

#define SINPUT_DEVICE_REPORT_ID_JOYSTICK_INPUT  0x01
#define SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT    0x02
#define SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT   0x03


#define SINPUT_DEVICE_COMMAND_HAPTIC    0x01
#define SINPUT_DEVICE_COMMAND_FEATURES  0x02
#define SINPUT_DEVICE_COMMAND_PLAYERLED 0x03

#define SINPUT_HAPTIC_TYPE_PRECISE          0x01
#define SINPUT_HAPTIC_TYPE_ERMSIMULATION    0x02

#define SINPUT_DEFAULT_GYRO_SENS  2000
#define SINPUT_DEFAULT_ACCEL_SENS 8

#define SINPUT_REPORT_IDX_BUTTONS_0   3
#define SINPUT_REPORT_IDX_BUTTONS_1   4
#define SINPUT_REPORT_IDX_BUTTONS_2   5
#define SINPUT_REPORT_IDX_BUTTONS_3   6
#define SINPUT_REPORT_IDX_LEFT_X      7
#define SINPUT_REPORT_IDX_LEFT_Y      9
#define SINPUT_REPORT_IDX_RIGHT_X     11
#define SINPUT_REPORT_IDX_RIGHT_Y     13
#define SINPUT_REPORT_IDX_LEFT_TRIGGER    15
#define SINPUT_REPORT_IDX_RIGHT_TRIGGER   17
#define SINPUT_REPORT_IDX_IMU_TIMESTAMP   19
#define SINPUT_REPORT_IDX_IMU_ACCEL_X     21
#define SINPUT_REPORT_IDX_IMU_ACCEL_Y     23
#define SINPUT_REPORT_IDX_IMU_ACCEL_Z     25
#define SINPUT_REPORT_IDX_IMU_GYRO_X      27
#define SINPUT_REPORT_IDX_IMU_GYRO_Y      29
#define SINPUT_REPORT_IDX_IMU_GYRO_Z      31

#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_ID   1
#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK 2

#define SINPUT_REPORT_IDX_PLUG_STATUS     1
#define SINPUT_REPORT_IDX_CHARGE_LEVEL    2

#define SINPUT_RUMBLE_WRITE_FREQUENCY_MS 4

#ifndef EXTRACTSINT16
#define EXTRACTSINT16(data, idx) ((Sint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#ifndef EXTRACTUINT16
#define EXTRACTUINT16(data, idx) ((Uint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#pragma pack(push, 1) // Ensure byte alignment
// Input report (Report ID: 1)
typedef struct
{
    uint8_t plug_status;    // Plug Status (0 = unplug, 1 = plug, 2 = charge)
    uint8_t charge_percent; // 0-100

    union
    {
        struct
        {
            uint8_t button_b : 1;
            uint8_t button_a : 1;
            uint8_t button_y : 1;
            uint8_t button_x : 1;
            uint8_t dpad_up : 1;
            uint8_t dpad_down : 1;
            uint8_t dpad_left : 1;
            uint8_t dpad_right : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_stick_left : 1;
            uint8_t button_stick_right : 1;
            uint8_t button_l : 1;
            uint8_t button_r : 1;
            uint8_t button_zl : 1;
            uint8_t button_zr : 1;
            uint8_t button_gl : 1;
            uint8_t button_gr : 1;
        };
        uint8_t buttons_2;
    };

    union
    {
        struct
        {
            uint8_t button_plus : 1;
            uint8_t button_minus : 1;
            uint8_t button_home : 1;
            uint8_t button_capture : 1;
            uint8_t button_power : 1;
            uint8_t reserved_b3 : 3; // Reserved bits
        };
        uint8_t buttons_3;
    };

    uint8_t buttons_reserved;
    int16_t left_x;             // Left stick X
    int16_t left_y;             // Left stick Y
    int16_t right_x;            // Right stick X
    int16_t right_y;            // Right stick Y
    int16_t trigger_l;          // Left trigger
    int16_t trigger_r;          // Right trigger
    uint16_t gyro_elapsed_time; // Microseconds, 0 if unchanged
    int16_t accel_x;            // Accelerometer X
    int16_t accel_y;            // Accelerometer Y
    int16_t accel_z;            // Accelerometer Z
    int16_t gyro_x;             // Gyroscope X
    int16_t gyro_y;             // Gyroscope Y
    int16_t gyro_z;             // Gyroscope Z

    uint8_t reserved_bulk[31]; // Reserved for future input data types
} SINPUT_INPUT_S;
#pragma pack(pop)

#pragma pack(push, 1) // Ensure byte alignment
typedef struct
{
    uint8_t type;

    union
    {
        // Frequency Amplitude pairs
        struct
        {
            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } left;

            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } right;

        } type_1;

        // Basic ERM simulation model
        struct
        {
            struct
            {
                uint8_t amplitude;
                bool brake;
            } left;

            struct
            {
                uint8_t amplitude;
                bool brake;
            } right;

        } type_2;
    };
} SINPUT_HAPTIC_S;
#pragma pack(pop)

typedef union
{
    struct
    {
        uint8_t haptics_supported : 1;
        uint8_t player_leds_supported : 1;
        uint8_t accelerometer_supported : 1;
        uint8_t gyroscope_supported : 1;
        uint8_t left_analog_stick_supported : 1;
        uint8_t right_analog_stick_supported : 1;
        uint8_t left_analog_trigger_supported : 1;
        uint8_t right_analog_trigger_supported : 1;
    };
    uint8_t value;
} SINPUT_FEATURE_FLAGS_U;

typedef struct
{
    SDL_HIDAPI_Device *device;
    SDL_Joystick *joystick;
    SINPUT_FEATURE_FLAGS_U feature_flags;

    Uint8 player_idx;
    bool feature_flags_obtained;
    bool feature_flags_sent;

    Uint16 api_version; // Version of the API this device supports
    Uint8  sub_type;    // Subtype of the device, 0 in most cases

    Uint16 accelRange; // Example would be 2,4,8,16 +/- (g-force)
    Uint16 gyroRange;  // Example would be 125,250,500,1000,2000,4000 +/- (degrees per second)

    float accelScale; // Scale factor for accelerometer values
    float gyroScale;  // Scale factor for gyroscope values
    Uint8 last_state[USB_PACKET_LENGTH];

    Uint64 imu_timestamp; // Nanoseconds. We accumulate with received deltas
} SDL_DriverSInput_Context;

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
} SINPUT_IMU_S;

// Converts raw int16_t gyro scale setting
static inline float CalculateGyroScale(uint16_t dps_range)
{
    return SDL_PI_F / 180.0f / (32768.0f / (float)dps_range);
}

// Converts raw int16_t accel scale setting
static inline float CalculateAccelScale(uint16_t g_range)
{
    return SDL_STANDARD_GRAVITY / (32768.0f / (float)g_range);
}

static void HIDAPI_DriverSInput_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, callback, userdata);
}

static void HIDAPI_DriverSInput_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, callback, userdata);
}

static bool HIDAPI_DriverSInput_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SINPUT, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSInput_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return SDL_IsJoystickSInputController(vendor_id, product_id);
}

static void ProcessFeatureFlagResponse(SDL_HIDAPI_Device *device, Uint8 *data)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    ctx->feature_flags.value = data[0];

    // data[1] reserved byte

    ctx->sub_type = data[2];
    // data[3] reserved byte

    ctx->api_version = (Uint16)(data[4] | (data[5] << 8));

    ctx->accelRange = (Uint16)(data[6] | (data[7] << 8));
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Accelerometer Range: %d", ctx->accelRange);
#endif

    ctx->gyroRange = (Uint16)(data[8] | (data[9] << 8));
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Gyro Range: %d", ctx->gyroRange);
#endif

    ctx->accelScale = CalculateAccelScale(ctx->accelRange);
    ctx->gyroScale  = CalculateGyroScale(ctx->gyroRange);

    ctx->feature_flags_obtained = true;

    // Set player number, finalizing the setup
    Uint8 playerLedCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_PLAYERLED, ctx->player_idx };
    int playerNumBytesWritten = SDL_HIDAPI_SendRumble(device, playerLedCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

    if (playerNumBytesWritten < 0) {
        SDL_Log("SInput device player led command could not write");
    }
}

static bool HIDAPI_DriverSInput_InitDevice(SDL_HIDAPI_Device *device)
{
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput device Init");
#endif

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }

    ctx->device = device;
    device->context = ctx;

#if !defined(DEBUG_SINPUT_INIT)
    // Set default feature flags
    SINPUT_FEATURE_FLAGS_U flags = {
        .accelerometer_supported = 1,
        .gyroscope_supported = 1,
        .haptics_supported = 1,
        .left_analog_stick_supported = 1,
        .right_analog_stick_supported = 1,
        .player_leds_supported = 1,
        .left_analog_trigger_supported = 1,
        .right_analog_trigger_supported = 1,
    };


    ctx->feature_flags.value = flags.value;
    ctx->accelRange = SINPUT_DEFAULT_ACCEL_SENS;
    ctx->gyroRange  = SINPUT_DEFAULT_GYRO_SENS;

    ctx->accelScale = CalculateAccelScale(ctx->accelRange);
    ctx->gyroScale = CalculateGyroScale(ctx->gyroRange);
#endif 
    
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSInput_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSInput_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    player_index = SDL_clamp(player_index+1, 0, 255);
    Uint8 player_num = (Uint8)player_index;

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    ctx->player_idx = player_num;
}

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(SDL_PI_F / 180.f))
#endif


static bool HIDAPI_DriverSInput_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput device Open");
#endif

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    SDL_AssertJoysticksLocked();

    ctx->joystick = joystick;

    SDL_zeroa(ctx->last_state);

    joystick->nbuttons = 32; 
    joystick->naxes = SDL_GAMEPAD_AXIS_COUNT;
    joystick->nhats = 1;
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, (float)SINPUT_DEVICE_POLLING_RATE);
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, (float)SINPUT_DEVICE_POLLING_RATE);

    return true;
}

static bool HIDAPI_DriverSInput_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SINPUT_HAPTIC_S hapticData = { 0 };
    Uint8 hapticReport[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_HAPTIC };

    hapticData.type = 1;

    hapticData.type_1.left.frequency_1 = 85;
    hapticData.type_1.left.frequency_2 = 170;

    hapticData.type_1.right.frequency_1 = 85;
    hapticData.type_1.right.frequency_2 = 170;

    // It's really left and right amplitude, not low frequency and high frequency...
    hapticData.type_1.left.amplitude_1 = low_frequency_rumble;
    hapticData.type_1.left.amplitude_2 = low_frequency_rumble;

    hapticData.type_1.right.amplitude_1 = high_frequency_rumble;
    hapticData.type_1.right.amplitude_2 = high_frequency_rumble;


    SDL_memcpy( &(hapticReport[2]), &hapticData, sizeof(SINPUT_HAPTIC_S) );

    SDL_HIDAPI_SendRumble(device, hapticReport, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

    return true;
}

static bool HIDAPI_DriverSInput_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSInput_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    Uint32 caps = 0;
    caps |= SDL_JOYSTICK_CAP_RUMBLE;
    caps |= SDL_JOYSTICK_CAP_PLAYER_LED;
    return caps;
}

static bool HIDAPI_DriverSInput_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return true;
}

static void HIDAPI_DriverSInput_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSInput_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis = 0;
    Sint16 accel = 0;
    Sint16 gyro = 0;
    Uint64 timestamp = SDL_GetTicksNS();
    float imu_values[3] = { 0 };

    if (ctx->last_state[SINPUT_REPORT_IDX_BUTTONS_0] != data[SINPUT_REPORT_IDX_BUTTONS_0]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, (data[SINPUT_REPORT_IDX_BUTTONS_0] & 0x01));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST,  (data[SINPUT_REPORT_IDX_BUTTONS_0] & 0x02));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST,  (data[SINPUT_REPORT_IDX_BUTTONS_0] & 0x04));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, (data[SINPUT_REPORT_IDX_BUTTONS_0] & 0x08));

        Uint8 hat;
        switch ((data[SINPUT_REPORT_IDX_BUTTONS_0] & 0xF0) >> 4) {
        case 0b0001:
            hat = SDL_HAT_UP;
            break;
        case 0b1001:
            hat = SDL_HAT_RIGHTUP;
            break;
        case 0b1000:
            hat = SDL_HAT_RIGHT;
            break;
        case 0b1010:
            hat = SDL_HAT_RIGHTDOWN;
            break;
        case 0b0010:
            hat = SDL_HAT_DOWN;
            break;
        case 0b0110:
            hat = SDL_HAT_LEFTDOWN;
            break;
        case 0b0100:
            hat = SDL_HAT_LEFT;
            break;
        case 0b0101:
            hat = SDL_HAT_LEFTUP;
            break;
        default:
            hat = SDL_HAT_CENTERED;
            break;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
    }

    if (ctx->last_state[SINPUT_REPORT_IDX_BUTTONS_1] != data[SINPUT_REPORT_IDX_BUTTONS_1]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK,  (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x01));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x02));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x04));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x08));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,  (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x10));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x20));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,  (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x40));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, (data[SINPUT_REPORT_IDX_BUTTONS_1] & 0x80));
    }

    if (ctx->last_state[SINPUT_REPORT_IDX_BUTTONS_2] != data[SINPUT_REPORT_IDX_BUTTONS_2]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (data[SINPUT_REPORT_IDX_BUTTONS_2] & 0x01));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK,  (data[SINPUT_REPORT_IDX_BUTTONS_2] & 0x02));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (data[SINPUT_REPORT_IDX_BUTTONS_2] & 0x04));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[SINPUT_REPORT_IDX_BUTTONS_2] & 0x08));

        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC2, (data[SINPUT_REPORT_IDX_BUTTONS_2] & 0x10));
    }

    // Analog inputs map to a signed Sint16 range of -32768 to 32767 from the device.
    if (ctx->feature_flags.left_analog_stick_supported) {
        axis = (Sint16)(data[SINPUT_REPORT_IDX_LEFT_X] | (data[SINPUT_REPORT_IDX_LEFT_X + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);

        axis = (Sint16)(data[SINPUT_REPORT_IDX_LEFT_Y] | (data[SINPUT_REPORT_IDX_LEFT_Y + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    }

    if (ctx->feature_flags.right_analog_stick_supported) {
        axis = (Sint16)(data[SINPUT_REPORT_IDX_RIGHT_X] | (data[SINPUT_REPORT_IDX_RIGHT_X + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);

        axis = (Sint16)(data[SINPUT_REPORT_IDX_RIGHT_Y] | (data[SINPUT_REPORT_IDX_RIGHT_Y + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);
    }

    if (ctx->feature_flags.left_analog_trigger_supported) {
        axis = (Sint16)(data[SINPUT_REPORT_IDX_LEFT_TRIGGER] | (data[SINPUT_REPORT_IDX_LEFT_TRIGGER + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);
    }

    if (ctx->feature_flags.right_analog_trigger_supported) {
        axis = (Sint16)(data[SINPUT_REPORT_IDX_RIGHT_TRIGGER] | (data[SINPUT_REPORT_IDX_RIGHT_TRIGGER + 1] << 8));
        SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);
    }

    if (ctx->last_state[SINPUT_REPORT_IDX_PLUG_STATUS]  != data[SINPUT_REPORT_IDX_PLUG_STATUS] ||
        ctx->last_state[SINPUT_REPORT_IDX_CHARGE_LEVEL] != data[SINPUT_REPORT_IDX_CHARGE_LEVEL]) {

        SDL_PowerState state;
        Uint8 status = data[SINPUT_REPORT_IDX_PLUG_STATUS];
        int percent = data[SINPUT_REPORT_IDX_CHARGE_LEVEL];

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
    }

    // Extract the IMU timestamp delta (in microseconds)
    Uint16 imu_timestamp_delta = EXTRACTUINT16(data, SINPUT_REPORT_IDX_IMU_TIMESTAMP);

    // Check if we should process IMU data
    if (imu_timestamp_delta > 0) {

        // Process IMU timestamp by adding the delta to the accumulated timestamp and converting to nanoseconds
        ctx->imu_timestamp += ((Uint64) imu_timestamp_delta * 1000);

        // Process Accelerometer
        if (ctx->feature_flags.accelerometer_supported) {

            accel = (Sint16)(data[SINPUT_REPORT_IDX_IMU_ACCEL_Y] | (data[SINPUT_REPORT_IDX_IMU_ACCEL_Y + 1] << 8));
            imu_values[2] = -(float)accel * ctx->accelScale; // Y-axis acceleration

            accel = (Sint16)(data[SINPUT_REPORT_IDX_IMU_ACCEL_Z] | (data[SINPUT_REPORT_IDX_IMU_ACCEL_Z + 1] << 8));
            imu_values[1] = (float)accel * ctx->accelScale; // Z-axis acceleration

            accel = (Sint16)(data[SINPUT_REPORT_IDX_IMU_ACCEL_X] | (data[SINPUT_REPORT_IDX_IMU_ACCEL_X + 1] << 8));
            imu_values[0] = -(float)accel * ctx->accelScale; // X-axis acceleration

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, ctx->imu_timestamp, imu_values, 3);
        }

        // Process Gyroscope
        if (ctx->feature_flags.accelerometer_supported) {
            
            gyro = (Sint16)(data[SINPUT_REPORT_IDX_IMU_GYRO_Y] | (data[SINPUT_REPORT_IDX_IMU_GYRO_Y + 1] << 8));
            imu_values[2] = -(float)gyro * ctx->gyroScale; // Y-axis rotation

            gyro = (Sint16)(data[SINPUT_REPORT_IDX_IMU_GYRO_Z] | (data[SINPUT_REPORT_IDX_IMU_GYRO_Z + 1] << 8));
            imu_values[1] = (float)gyro * ctx->gyroScale; // Z-axis rotation

            gyro = (Sint16)(data[SINPUT_REPORT_IDX_IMU_GYRO_X] | (data[SINPUT_REPORT_IDX_IMU_GYRO_X + 1] << 8));
            imu_values[0] = -(float)gyro * ctx->gyroScale; // X-axis rotation

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, ctx->imu_timestamp, imu_values, 3);
        }
    }    

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static bool HIDAPI_DriverSInput_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SINPUT_PROTOCOL
        HIDAPI_DumpPacket("SInput packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        if (!ctx->feature_flags_obtained && !ctx->feature_flags_sent && ctx->player_idx > 0)
        {
            // Send feature get command
            // Set player number, finalizing the setup
            Uint8 featuresGetCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_FEATURES };
            int featureNumBytesWritten = SDL_HIDAPI_SendRumble(device, featuresGetCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

            if (featureNumBytesWritten < 0) {
                SDL_SetError("SInput device features get command could not write");
            }
            else
            {
                SDL_SetError("SInput device features get command written");
                ctx->feature_flags_sent = true;
            }
        }

        // Handle command response information
        if (data[0] == SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT)
        {
#if defined(DEBUG_SINPUT_INIT)
            SDL_Log("Got Input Command Data SInput Device");
#endif
            switch (data[SINPUT_REPORT_IDX_COMMAND_RESPONSE_ID])
            {
                default:
                break;

                case SINPUT_DEVICE_COMMAND_FEATURES:
#if defined(DEBUG_SINPUT_INIT)
                    SDL_Log("Got Feature Response Data SInput Device");
#endif
                ProcessFeatureFlagResponse(device, &(data[SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK]));
                ctx->feature_flags_obtained = true;
                break;
            }
        } else {
            HIDAPI_DriverSInput_HandleStatePacket(joystick, ctx, data, size);
        }
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static void HIDAPI_DriverSInput_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSInput_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSInput = {
    SDL_HINT_JOYSTICK_HIDAPI_SINPUT,
    true,
    HIDAPI_DriverSInput_RegisterHints,
    HIDAPI_DriverSInput_UnregisterHints,
    HIDAPI_DriverSInput_IsEnabled,
    HIDAPI_DriverSInput_IsSupportedDevice,
    HIDAPI_DriverSInput_InitDevice,
    HIDAPI_DriverSInput_GetDevicePlayerIndex,
    HIDAPI_DriverSInput_SetDevicePlayerIndex,
    HIDAPI_DriverSInput_UpdateDevice,
    HIDAPI_DriverSInput_OpenJoystick,
    HIDAPI_DriverSInput_RumbleJoystick,
    HIDAPI_DriverSInput_RumbleJoystickTriggers,
    HIDAPI_DriverSInput_GetJoystickCapabilities,
    HIDAPI_DriverSInput_SetJoystickLED,
    HIDAPI_DriverSInput_SendJoystickEffect,
    HIDAPI_DriverSInput_SetJoystickSensorsEnabled,
    HIDAPI_DriverSInput_CloseJoystick,
    HIDAPI_DriverSInput_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SINPUT

#endif // SDL_JOYSTICK_HIDAPI
