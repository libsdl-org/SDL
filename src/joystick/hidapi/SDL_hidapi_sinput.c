/*
  Simple DirectMedia Layer
  Copyright (C) 2025 Mitchell Cairns <mitch.cairns@handheldlegend.com>

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

/*****************************************************************************************************/

// Define this if you want to log all packets from the controller
#if 0
#define DEBUG_SINPUT_PROTOCOL
#endif

#if 0
#define DEBUG_SINPUT_INIT
#endif

#define SINPUT_DEVICE_REPORT_SIZE           64 // Size of input reports (And CMD Input reports)
#define SINPUT_DEVICE_REPORT_COMMAND_SIZE   48 // Size of command OUTPUT reports

#define SINPUT_DEVICE_REPORT_ID_JOYSTICK_INPUT  0x01
#define SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT    0x02
#define SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT   0x03

#define SINPUT_DEVICE_COMMAND_HAPTIC        0x01
#define SINPUT_DEVICE_COMMAND_FEATURES      0x02
#define SINPUT_DEVICE_COMMAND_PLAYERLED     0x03
#define SINPUT_DEVICE_COMMAND_JOYSTICKRGB   0x04

#define SINPUT_HAPTIC_TYPE_PRECISE          0x01
#define SINPUT_HAPTIC_TYPE_ERMSIMULATION    0x02

#define SINPUT_DEFAULT_GYRO_SENS  2000
#define SINPUT_DEFAULT_ACCEL_SENS 8

#define SINPUT_REPORT_IDX_BUTTONS_0         3
#define SINPUT_REPORT_IDX_BUTTONS_1         4
#define SINPUT_REPORT_IDX_BUTTONS_2         5
#define SINPUT_REPORT_IDX_BUTTONS_3         6
#define SINPUT_REPORT_IDX_LEFT_X            7
#define SINPUT_REPORT_IDX_LEFT_Y            9
#define SINPUT_REPORT_IDX_RIGHT_X           11
#define SINPUT_REPORT_IDX_RIGHT_Y           13
#define SINPUT_REPORT_IDX_LEFT_TRIGGER      15
#define SINPUT_REPORT_IDX_RIGHT_TRIGGER     17
#define SINPUT_REPORT_IDX_IMU_TIMESTAMP     19
#define SINPUT_REPORT_IDX_IMU_ACCEL_X       21
#define SINPUT_REPORT_IDX_IMU_ACCEL_Y       23
#define SINPUT_REPORT_IDX_IMU_ACCEL_Z       25
#define SINPUT_REPORT_IDX_IMU_GYRO_X        27
#define SINPUT_REPORT_IDX_IMU_GYRO_Y        29
#define SINPUT_REPORT_IDX_IMU_GYRO_Z        31
#define SINPUT_REPORT_IDX_IMU_COUNTER       33
#define SINPUT_REPORT_IDX_TOUCH1_X          34
#define SINPUT_REPORT_IDX_TOUCH1_Y          36
#define SINPUT_REPORT_IDX_TOUCH1_P          38
#define SINPUT_REPORT_IDX_TOUCH2_X          40
#define SINPUT_REPORT_IDX_TOUCH2_Y          42
#define SINPUT_REPORT_IDX_TOUCH2_P          44

#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_ID   1
#define SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK 2

#define SINPUT_REPORT_IDX_PLUG_STATUS     1
#define SINPUT_REPORT_IDX_CHARGE_LEVEL    2

#define SINPUT_MAX_ALLOWED_TOUCHPADS 2

#ifndef EXTRACTSINT16
#define EXTRACTSINT16(data, idx) ((Sint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif

#ifndef EXTRACTUINT16
#define EXTRACTUINT16(data, idx) ((Uint16)((data)[(idx)] | ((data)[(idx) + 1] << 8)))
#endif


typedef struct
{
    uint8_t type;

    union {
        // Frequency Amplitude pairs
        struct {
            struct {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } left;

            struct {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } right;

        } type_1;

        // Basic ERM simulation model
        struct {
            struct {
                uint8_t amplitude;
                bool brake;
            } left;

            struct {
                uint8_t amplitude;
                bool brake;
            } right;

        } type_2;
    };
} SINPUT_HAPTIC_S;

typedef struct
{
    SDL_HIDAPI_Device *device;
    bool sensors_enabled;

    Uint8 player_idx;

    bool player_leds_supported;
    bool joystick_rgb_supported;
    bool rumble_supported;
    bool accelerometer_supported;
    bool gyroscope_supported;
    bool left_analog_stick_supported;
    bool right_analog_stick_supported;
    bool left_analog_trigger_supported;
    bool right_analog_trigger_supported;
    bool touchpad_supported;

    Uint8 touchpad_count;        // 2 touchpads maximum
    Uint8 touchpad_finger_count; // 2 fingers for one touchpad, or 1 per touchpad (2 max)

    Uint8  polling_rate_ms;
    Uint8  sub_type;    // Subtype of the device, 0 in most cases

    Uint16 accelRange; // Example would be 2,4,8,16 +/- (g-force)
    Uint16 gyroRange;  // Example would be 1000,2000,4000 +/- (degrees per second)

    float accelScale; // Scale factor for accelerometer values
    float gyroScale;  // Scale factor for gyroscope values
    Uint8 last_state[USB_PACKET_LENGTH];

    Uint8 buttons_count;
    Uint8 usage_masks[4];

    Uint64 imu_timestamp; // Nanoseconds. We accumulate with received deltas
} SDL_DriverSInput_Context;

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

static void ProcessSDLFeaturesResponse(SDL_HIDAPI_Device *device, Uint8 *data)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    // Bitfields are not portable, so we unpack them into a struct value
    ctx->rumble_supported = (data[0] & 0x01) != 0;
    ctx->player_leds_supported = (data[0] & 0x02) != 0;
    ctx->accelerometer_supported = (data[0] & 0x04) != 0;
    ctx->gyroscope_supported = (data[0] & 0x08) != 0;

    ctx->left_analog_stick_supported = (data[0] & 0x10) != 0;
    ctx->right_analog_stick_supported = (data[0] & 0x20) != 0;
    ctx->left_analog_trigger_supported = (data[0] & 0x40) != 0;
    ctx->right_analog_trigger_supported = (data[0] & 0x80) != 0;

    ctx->touchpad_supported = (data[1] & 0x01) != 0;
    ctx->joystick_rgb_supported = (data[1] & 0x02) != 0;

    SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
    type = (SDL_GamepadType)SDL_clamp(data[2], SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_TYPE_COUNT);
    device->type = type;

    // The 4 MSB represent a button layout style SDL_GamepadFaceStyle
    // The 4 LSB represent a device sub-type
    device->guid.data[15] = data[3];

#if defined(DEBUG_SINPUT_INIT)
        SDL_Log("SInput Face Style: %d", (data[3] & 0xF0) >> 4);
        SDL_Log("SInput Sub-type: %d", (data[3] & 0xF));
#endif

    ctx->polling_rate_ms = data[4];

    ctx->accelRange = EXTRACTUINT16(data, 6);
    ctx->gyroRange = EXTRACTUINT16(data, 8);

    // Masks in LSB to MSB
    // South, East, West, North, DUp, DDown, DLeft, DRight
    ctx->usage_masks[0] = data[10];

    // Stick Left, Stick Right, L Shoulder, R Shoulder,
    // L Trigger, R Trigger, L Paddle 1, R Paddle 1
    ctx->usage_masks[1] = data[11];

    // Start, Back, Guide, Capture, L Paddle 2, R Paddle 2, Touchpad L, Touchpad R
    ctx->usage_masks[2] = data[12];

    // Power, Misc 4 to 10
    ctx->usage_masks[3] = data[13];

    // Derive button count from mask
    for (Uint8 byte = 0; byte < 4; ++byte) {
        for (Uint8 bit = 0; bit < 8; ++bit) {
            if ((ctx->usage_masks[byte] & (1 << bit)) != 0) {
                ++ctx->buttons_count;
            }
        }
    }

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Buttons count: %d", ctx->buttons_count);
#endif

    // Get and validate touchpad parameters
    ctx->touchpad_count = data[14];
    ctx->touchpad_finger_count = data[15];

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Accelerometer Range: %d", ctx->accelRange);
#endif

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Gyro Range: %d", ctx->gyroRange);
#endif

    ctx->accelScale = CalculateAccelScale(ctx->accelRange);
    ctx->gyroScale = CalculateGyroScale(ctx->gyroRange);
}

static bool RetrieveSDLFeatures(SDL_HIDAPI_Device *device)
{
    int written = 0;

    // Attempt to send the SDL features get command. 
    for (int attempt = 0; attempt < 8; ++attempt) {
        const Uint8 featuresGetCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_FEATURES };
        // This write will occasionally return -1, so ignore failure here and try again
        written = SDL_hid_write(device->dev, featuresGetCommand, sizeof(featuresGetCommand));

        if (written == SINPUT_DEVICE_REPORT_COMMAND_SIZE) {
            break;
        }
    }

    if (written < SINPUT_DEVICE_REPORT_COMMAND_SIZE) {
        SDL_SetError("SInput device SDL Features GET command could not write");
        return false;
    } 

    int read = 0;

    // Read the reply
    for (int i = 0; i < 100; ++i) {
        SDL_Delay(1);

        Uint8 data[USB_PACKET_LENGTH];
        read = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0);
        if (read < 0) {
            SDL_SetError("SInput device SDL Features GET command could not read");
            return false;
        }
        if (read == 0) {
            continue;
        }

#ifdef DEBUG_SINPUT_PROTOCOL
        HIDAPI_DumpPacket("SInput packet: size = %d", data, size);
#endif

        if ((read == USB_PACKET_LENGTH) && (data[0] == SINPUT_DEVICE_REPORT_ID_INPUT_CMDDAT) && (data[1] == SINPUT_DEVICE_COMMAND_FEATURES)) {
            ProcessSDLFeaturesResponse(device, &(data[SINPUT_REPORT_IDX_COMMAND_RESPONSE_BULK]));
#if defined(DEBUG_SINPUT_INIT)
            SDL_Log("Received SInput SDL Features command response");
#endif
            return true;
        }
    }

    return false;
}

// Type 2 haptics are for more traditional rumble such as
// ERM motors or simulated ERM motors
static inline void HapticsType2Pack(SINPUT_HAPTIC_S *in, Uint8 *out)
{
    // Type of haptics
    out[0] = 2;

    out[1] = in->type_2.left.amplitude;
    out[2] = in->type_2.left.brake;

    out[3] = in->type_2.right.amplitude;
    out[4] = in->type_2.right.brake;
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

    if (!RetrieveSDLFeatures(device)) {
        return false;
    }
    
    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverSInput_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSInput_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->player_leds_supported) {
        player_index = SDL_clamp(player_index + 1, 0, 255);
        Uint8 player_num = (Uint8)player_index;

        ctx->player_idx = player_num;

        // Set player number, finalizing the setup
        Uint8 playerLedCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_PLAYERLED, ctx->player_idx };
        int playerNumBytesWritten = SDL_hid_write(device->dev, playerLedCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

        if (playerNumBytesWritten < 0) {
            SDL_SetError("SInput device player led command could not write");
        }
    }
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

    joystick->nbuttons = ctx->buttons_count;

    SDL_zeroa(ctx->last_state);

    int axes = 0;
    if (ctx->left_analog_stick_supported) {
        axes += 2;
    }

    if (ctx->right_analog_stick_supported) {
        axes += 2;
    }

    if (ctx->left_analog_trigger_supported) {
        ++axes;
    }

    if (ctx->right_analog_trigger_supported) {
        ++axes;
    }

    joystick->naxes = axes;

    if (ctx->accelerometer_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, (float)1000.0f/ctx->polling_rate_ms);
    }

    if (ctx->gyroscope_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, (float)1000.0f / ctx->polling_rate_ms);
    }

    if (ctx->touchpad_supported) {
        // If touchpad is supported, minimum 1, max is capped
        ctx->touchpad_count = SDL_clamp(ctx->touchpad_count, 1, SINPUT_MAX_ALLOWED_TOUCHPADS);

        if (ctx->touchpad_count > 1) {
            // Support two separate touchpads with 1 finger each
            // or support one touchpad with 2 fingers max
            ctx->touchpad_finger_count = 1;
        }

        if (ctx->touchpad_count > 0) {
            SDL_PrivateJoystickAddTouchpad(joystick, ctx->touchpad_finger_count);
        }

        if (ctx->touchpad_count > 1) {
            SDL_PrivateJoystickAddTouchpad(joystick, ctx->touchpad_finger_count);
        }
    }

    return true;
}

static bool HIDAPI_DriverSInput_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{

    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->rumble_supported) {
        SINPUT_HAPTIC_S hapticData = { 0 };
        Uint8 hapticReport[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_HAPTIC };

        // Low Frequency  = Left
        // High Frequency = Right
        hapticData.type_2.left.amplitude = (Uint8) (low_frequency_rumble >> 8);
        hapticData.type_2.right.amplitude = (Uint8)(high_frequency_rumble >> 8);

        HapticsType2Pack(&hapticData, &(hapticReport[2]));

        SDL_HIDAPI_SendRumble(device, hapticReport, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

        return true;
    }

    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSInput_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    Uint32 caps = 0;
    if (ctx->rumble_supported) {
        caps |= SDL_JOYSTICK_CAP_RUMBLE;
    }

    if (ctx->player_leds_supported) {
        caps |= SDL_JOYSTICK_CAP_PLAYER_LED;
    }

    if (ctx->joystick_rgb_supported) {
        caps |= SDL_JOYSTICK_CAP_RGB_LED;
    }
    
    return caps;
}

static bool HIDAPI_DriverSInput_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->player_leds_supported) {

        // Set player number, finalizing the setup
        Uint8 joystickRGBCommand[SINPUT_DEVICE_REPORT_COMMAND_SIZE] = { SINPUT_DEVICE_REPORT_ID_OUTPUT_CMDDAT, SINPUT_DEVICE_COMMAND_JOYSTICKRGB, red, green, blue };
        int joystickRGBBytesWritten = SDL_hid_write(device->dev, joystickRGBCommand, SINPUT_DEVICE_REPORT_COMMAND_SIZE);

        if (joystickRGBBytesWritten < 0) {
            SDL_SetError("SInput device joystick rgb command could not write");
            return false;
        }

        return true;
    }
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSInput_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverSInput_Context *ctx = (SDL_DriverSInput_Context *)device->context;

    if (ctx->accelerometer_supported || ctx->gyroscope_supported) {
        ctx->sensors_enabled = enabled;
        return true;
    }
    return SDL_Unsupported();
}

static void HIDAPI_DriverSInput_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverSInput_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis = 0;
    Sint16 accel = 0;
    Sint16 gyro = 0;
    Uint64 timestamp = SDL_GetTicksNS();
    float imu_values[3] = { 0 };
    Uint8 output_idx = 0;

    // Process digital buttons according to the supplied
    // button mask to create a contiguous button input set
    for (Uint8 processes = 0; processes < 4; ++processes) {

        Uint8 button_idx = SINPUT_REPORT_IDX_BUTTONS_0 + processes;

        for (Uint8 buttons = 0; buttons < 8; ++buttons) {

            // If a button is enabled by our usage mask
            const Uint8 mask = (0x01 << buttons);
            if ((ctx->usage_masks[processes] & mask) != 0) {

                bool down = (data[button_idx] & mask) != 0;

                if ( (output_idx < SDL_GAMEPAD_BUTTON_COUNT) && (ctx->last_state[button_idx] != data[button_idx]) ) {
                    SDL_SendJoystickButton(timestamp, joystick, output_idx, down);
                }

                ++output_idx;
            }
        }
    }

    // Analog inputs map to a signed Sint16 range of -32768 to 32767 from the device.
    // Use an axis index because not all gamepads will have the same axis inputs.
    Uint8 axis_idx = 0;

    // Left Analog Stick
    axis = 0; // Reset axis value for joystick
    if (ctx->left_analog_stick_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_X);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;

        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_Y);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }
    
    // Right Analog Stick
    axis = 0; // Reset axis value for joystick
    if (ctx->right_analog_stick_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_X);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;

        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_Y);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }

    // Left Analog Trigger
    axis = SDL_MIN_SINT16; // Reset axis value for trigger
    if (ctx->left_analog_trigger_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_LEFT_TRIGGER);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
        ++axis_idx;
    }
    
    // Right Analog Trigger
    axis = SDL_MIN_SINT16; // Reset axis value for trigger
    if (ctx->right_analog_trigger_supported) {
        axis = EXTRACTSINT16(data, SINPUT_REPORT_IDX_RIGHT_TRIGGER);
        SDL_SendJoystickAxis(timestamp, joystick, axis_idx, axis);
    }

    // Battery/Power state handling
    if (ctx->last_state[SINPUT_REPORT_IDX_PLUG_STATUS]  != data[SINPUT_REPORT_IDX_PLUG_STATUS] ||
        ctx->last_state[SINPUT_REPORT_IDX_CHARGE_LEVEL] != data[SINPUT_REPORT_IDX_CHARGE_LEVEL]) {

        SDL_PowerState state = SDL_POWERSTATE_NO_BATTERY;
        Uint8 status = data[SINPUT_REPORT_IDX_PLUG_STATUS];
        int percent = data[SINPUT_REPORT_IDX_CHARGE_LEVEL];

        percent = SDL_clamp(percent, 0, 100); // Ensure percent is within valid range

        switch (status) {
        case 1:
            state = SDL_POWERSTATE_NO_BATTERY;
            percent = 0;
            break;
        case 2:
            state = SDL_POWERSTATE_CHARGING;
            break;
        case 3:
            state = SDL_POWERSTATE_CHARGED;
            percent = 100;
            break;
        case 4:
            state = SDL_POWERSTATE_ON_BATTERY;
            break;
        default: // Wired/No Battery Supported
            state = SDL_POWERSTATE_UNKNOWN;
            percent = 0;
            break;
        }

        if (state > 0) {
            SDL_SendJoystickPowerInfo(joystick, state, percent);
        } 
    }

    // Extract the IMU timestamp delta (in microseconds)
    Uint32 imu_timestamp_delta = (Uint32) EXTRACTUINT16(data, SINPUT_REPORT_IDX_IMU_TIMESTAMP);

    // Extract the IMU packet counter delta
    Uint8 imu_counter_delta = (Uint8)((data[SINPUT_REPORT_IDX_IMU_COUNTER] - ctx->last_state[SINPUT_REPORT_IDX_IMU_COUNTER]) & 0xFF);

    // If a packet is dropped, this delta will
    // be larger than 1
    // If the delta is 0 or 1, this is ignored
    if (imu_counter_delta > 1) {
        imu_timestamp_delta *= (Uint32)imu_counter_delta;
    }

    // Check if we should process IMU data and if sensors are enabled
    if ((imu_timestamp_delta > 0) && (ctx->sensors_enabled)) {

        // Process IMU timestamp by adding the delta to the accumulated timestamp and converting to nanoseconds
        ctx->imu_timestamp += ((Uint64) imu_timestamp_delta * 1000);

        // Process Accelerometer
        if (ctx->accelerometer_supported) {

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Y);
            imu_values[2] = -(float)accel * ctx->accelScale; // Y-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Z);
            imu_values[1] = (float)accel * ctx->accelScale; // Z-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_X);
            imu_values[0] = -(float)accel * ctx->accelScale; // X-axis acceleration

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, ctx->imu_timestamp, imu_values, 3);
        }

        // Process Gyroscope
        if (ctx->gyroscope_supported) {
            
            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Y);
            imu_values[2] = -(float)gyro * ctx->gyroScale; // Y-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Z);
            imu_values[1] = (float)gyro * ctx->gyroScale; // Z-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_X);
            imu_values[0] = -(float)gyro * ctx->gyroScale; // X-axis rotation

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, ctx->imu_timestamp, imu_values, 3);
        }
    }

    // Check if we should process touchpad
    if (ctx->touchpad_supported && ctx->touchpad_count > 0) {
        Uint8 touchpad = 0;
        Uint8 finger = 0;

        Sint16 touch1X = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH1_X);
        Sint16 touch1Y = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH1_Y);
        Uint16 touch1P = EXTRACTUINT16(data, SINPUT_REPORT_IDX_TOUCH1_P);

        Sint16 touch2X = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH2_X);
        Sint16 touch2Y = EXTRACTSINT16(data, SINPUT_REPORT_IDX_TOUCH2_Y);
        Uint16 touch2P = EXTRACTUINT16(data, SINPUT_REPORT_IDX_TOUCH2_P);

        SDL_SendJoystickTouchpad(timestamp, joystick, touchpad, finger,
            touch1P > 0,
            touch1X / 65536.0f + 0.5f,
            touch1Y / 65536.0f + 0.5f,
            touch1P / 32768.0f);

        if (ctx->touchpad_count > 1) {
            ++touchpad;
        } else if (ctx->touchpad_finger_count > 1) {
            ++finger;
        }

        if ((touchpad > 0) || (finger > 0)) {
            SDL_SendJoystickTouchpad(timestamp, joystick, touchpad, finger,
                                     touch2P > 0,
                                     touch2X / 65536.0f + 0.5f,
                                     touch2Y / 65536.0f + 0.5f,
                                     touch2P / 32768.0f);
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

        // Handle command response information
        if (data[0] == SINPUT_DEVICE_REPORT_ID_JOYSTICK_INPUT) {
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
