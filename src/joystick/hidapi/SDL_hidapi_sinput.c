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
#include "SDL_hidapi_sinput.h"

#ifdef SDL_JOYSTICK_HIDAPI_SINPUT

/*****************************************************************************************************/
// This protocol is documented at:
// https://docs.handheldlegend.com/s/sinput/doc/sinput-hid-protocol-TkPYWlDMAg
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

#ifndef EXTRACTUINT32
#define EXTRACTUINT32(data, idx) ((Uint32)((data)[(idx)] | ((data)[(idx) + 1] << 8) | ((data)[(idx) + 2] << 16) | ((data)[(idx) + 3] << 24)))
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
    Uint16 protocol_version;
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
    bool dpad_supported;
    bool touchpad_supported;
    bool is_handheld;

    Uint8 touchpad_count;        // 2 touchpads maximum
    Uint8 touchpad_finger_count; // 2 fingers for one touchpad, or 1 per touchpad (2 max)

    Uint8  polling_rate_ms;
    Uint8  subtype;
    struct SDL_SInputFeatures features;

    Uint16 accelRange; // Example would be 2,4,8,16 +/- (g-force)
    Uint16 gyroRange;  // Example would be 1000,2000,4000 +/- (degrees per second)

    float accelScale; // Scale factor for accelerometer values
    float gyroScale;  // Scale factor for gyroscope values
    Uint8 last_state[USB_PACKET_LENGTH];

    Uint8 buttons_count;
    Uint8 usage_masks[4];

    Uint32 last_imu_timestamp_us;

    Uint64 imu_timestamp_ns; // Nanoseconds. We accumulate with received deltas
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

    // Obtain protocol version
    ctx->protocol_version = EXTRACTUINT16(data, 0);
    Uint8 *fflags = data + 2;
    Uint8 *buttons = data + 12;
    
    //
    // Unpack feature flags into context
    //
    ctx->rumble_supported = (fflags[0] & 0x01) != 0;
    ctx->player_leds_supported = (fflags[0] & 0x02) != 0;
    ctx->accelerometer_supported = (fflags[0] & 0x04) != 0;
    ctx->gyroscope_supported = (fflags[0] & 0x08) != 0;

    // Axes cannot be dynamic, so we only sanity check them
    bool left_analog_stick_supported = (fflags[0] & 0x10) != 0;
    bool right_analog_stick_supported = (fflags[0] & 0x20) != 0;
    bool left_analog_trigger_supported = (fflags[0] & 0x40) != 0;
    bool right_analog_trigger_supported = (fflags[0] & 0x80) != 0;

    ctx->touchpad_supported = (fflags[1] & 0x01) != 0;
    ctx->joystick_rgb_supported = (fflags[1] & 0x02) != 0;
    ctx->is_handheld = (fflags[1] & 0x04) != 0;

    //
    // Gamepad Info
    //
    SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
    type = (SDL_GamepadType)SDL_clamp(data[4], SDL_GAMEPAD_TYPE_UNKNOWN, SDL_GAMEPAD_TYPE_COUNT);
    device->type = type;
    
    // Update version to the custom one
    Uint16 version = HIDAPI_DriverSInput_DeriveVersion(buttons, left_analog_stick_supported, right_analog_stick_supported,
        left_analog_trigger_supported, right_analog_trigger_supported);
    device->guid.data[6] = version & 0xFF;
    device->guid.data[7] = (version >> 8) & 0xFF;

    // The 3 MSB represent a button layout style SDL_GamepadFaceStyle
    // The 5 LSB represent a device sub-type
    device->guid.data[15] = data[5];
    ctx->subtype = data[5];

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("SInput Face Style: %d", (data[5] & 0xE0) >> 5);
    SDL_Log("SInput Sub-type: %d", (data[5] & 0x1F));
#endif

    //
    // IMU Info
    //
    ctx->polling_rate_ms = data[6];
    ctx->accelRange = EXTRACTUINT16(data, 8);
    ctx->gyroRange = EXTRACTUINT16(data, 10);

    //
    // Get mappings based on SDL subtype and assert that they match.
    //
    HIDAPI_DriverSInput_GetControllerType(device->vendor_id, device->product_id, version, ctx->subtype, &ctx->features);
    struct SDL_SInputFeatures *f = &ctx->features;

    switch (f->controller_type) {
    case k_eSInputControllerType_HHL_PROGCC:
    case k_eSInputControllerType_HHL_GCCULT:
    case k_eSInputControllerType_LoadFirmware:
        ctx->usage_masks[0] = buttons[0];
        ctx->usage_masks[1] = buttons[1];
        ctx->usage_masks[2] = buttons[2];
        ctx->usage_masks[3] = buttons[3];
        ctx->left_analog_stick_supported = left_analog_stick_supported;
        ctx->right_analog_stick_supported = right_analog_stick_supported;
        ctx->left_analog_trigger_supported = left_analog_trigger_supported;
        ctx->right_analog_trigger_supported = right_analog_trigger_supported;
        break;
    case k_eSInputControllerType_Dynamic:
    default:
        ctx->usage_masks[0] = 0xFF;
        ctx->usage_masks[1] = 0x00;
        if (ctx->left_analog_stick_supported)
            ctx->usage_masks[1] |= 0x01 << (SINPUT_BTN_IDX_LEFT_STICK % 8);
        if (ctx->right_analog_stick_supported)
            ctx->usage_masks[1] |= 0x01 << (SINPUT_BTN_IDX_RIGHT_STICK % 8);
        if (f->trigger_style == k_eSInputMiscTriggerStyle_DigitalBumpers ||
            f->trigger_style == k_eSInputMiscTriggerStyle_AnalogBumpers)
            ctx->usage_masks[1] |= (0x01 << (SINPUT_BTN_IDX_LEFT_BUMPER % 8)) |
                                   (0x01 << (SINPUT_BTN_IDX_RIGHT_BUMPER % 8));
        if (f->trigger_style == k_eSInputMiscTriggerStyle_Digital ||
            f->trigger_style == k_eSInputMiscTriggerStyle_DigitalBumpers)
            ctx->usage_masks[1] |= (0x01 << (SINPUT_BTN_IDX_LEFT_TRIGGER % 8)) |
                                   (0x01 << (SINPUT_BTN_IDX_RIGHT_TRIGGER % 8));
        if (f->paddle_style != k_eSInputPaddleStyle_None)
            ctx->usage_masks[1] |= (0x01 << (SINPUT_BTN_IDX_LEFT_PADDLE1 % 8)) |
                                   (0x01 << (SINPUT_BTN_IDX_RIGHT_PADDLE1 % 8));

        ctx->usage_masks[2] = 0x03;
        if (f->guide_style == k_eSInputHasButtonStyle_Yes)
            ctx->usage_masks[2] |= (0x01 << (SINPUT_BTN_IDX_GUIDE % 8));        
        if (f->guide_style == k_eSInputHasButtonStyle_Yes)
            ctx->usage_masks[2] |= (0x01 << (SINPUT_BTN_IDX_GUIDE % 8));
        if (f->misc_button_style != k_eSInputMiscButtonStyle_None)
            ctx->usage_masks[2] |= (0x01 << (SINPUT_BTN_IDX_CAPTURE % 8));
        if (f->paddle_style == k_eSInputPaddleStyle_Four)
            ctx->usage_masks[2] |= (0x01 << (SINPUT_BTN_IDX_LEFT_PADDLE2 % 8)) |
                                   (0x01 << (SINPUT_BTN_IDX_RIGHT_PADDLE2 % 8));
        if (f->misc_button_style != k_eSInputMiscButtonStyle_Two && f->misc_button_style != k_eSInputMiscButtonStyle_None)
            ctx->usage_masks[2] |= 0x01 << (SINPUT_BTN_IDX_TOUCHPAD2 % 8);
        
        ctx->usage_masks[3] = 0x00;
        for (int i = k_eSInputMiscButtonStyle_Two; i < k_eSInputMiscButtonStyle_Max; ++i)
            ctx->usage_masks[3] |= 0x01 << (i - 2);

        ctx->left_analog_stick_supported = f->analog_style != k_eSInputanalog_None;
        ctx->right_analog_stick_supported = f->analog_style == k_eSInputanalog_Two;

        ctx->left_analog_trigger_supported = (f->trigger_style == k_eSInputMiscTriggerStyle_Analog &&
                                              f->trigger_style == k_eSInputMiscTriggerStyle_DigitalBumpers);
        ctx->right_analog_trigger_supported = ctx->left_analog_trigger_supported;
        break;
    }

    // Since SDL uses fixed mappings, unfortunately we cannot use the
    // button mask from the protocol. SInput defines a set of predefined
    // sub-types for this use. However, we can check it matches our expectations.
    if (
        // Masks in LSB to MSB
        // South, East, West, North, DUp, DDown, DLeft, DRight
        ctx->usage_masks[0] != buttons[0] ||
        // Left Stick, Right Stick, L Shoulder, R Shoulder,
        // L Trigger, R Trigger, L Paddle 1, R Paddle 1
        ctx->usage_masks[1] != buttons[1] ||
        // Start, Back, Guide, Capture, L Paddle 2, R Paddle 2, Touchpad L, Touchpad R
        ctx->usage_masks[2] != buttons[2] ||
        // Power, Misc 4 to 10
        ctx->usage_masks[3] != buttons[3] ||
        // Check Axes
        ctx->left_analog_stick_supported != left_analog_stick_supported ||
        ctx->right_analog_stick_supported != right_analog_stick_supported ||
        ctx->left_analog_trigger_supported != left_analog_trigger_supported ||
        ctx->right_analog_trigger_supported != right_analog_trigger_supported
    ) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_INPUT,
            "SInput device %s has different button mask than controller type 0x%.8x or that type is uknown. Found: %.2x%.2x%.2x%.2x-%d%d%d%d and will use: %.2x%.2x%.2x%.2x-%d%d%d%d",
            device->name, f->controller_type,
            buttons[0], buttons[1], buttons[2], buttons[3],
            left_analog_stick_supported, right_analog_stick_supported,
            left_analog_trigger_supported, right_analog_trigger_supported,
            ctx->usage_masks[0], ctx->usage_masks[1], ctx->usage_masks[2], ctx->usage_masks[3],
            ctx->left_analog_stick_supported, ctx->right_analog_stick_supported,
            ctx->left_analog_trigger_supported, ctx->right_analog_trigger_supported
        );
    }

    // Derive button count from mask
    for (Uint8 byte = 0; byte < 4; ++byte) {
        for (Uint8 bit = 0; bit < 8; ++bit) {
            if ((ctx->usage_masks[byte] & (1 << bit)) != 0) {
                ++ctx->buttons_count;
            }
        }
    }

    // Convert DPAD to hat
    const int DPAD_MASK = (1 << SINPUT_BTN_IDX_DPAD_UP) |
                          (1 << SINPUT_BTN_IDX_DPAD_DOWN) |
                          (1 << SINPUT_BTN_IDX_DPAD_LEFT) |
                          (1 << SINPUT_BTN_IDX_DPAD_RIGHT);
    if ((ctx->usage_masks[0] & DPAD_MASK) == DPAD_MASK) {
        ctx->dpad_supported = true;
        ctx->usage_masks[0] &= ~DPAD_MASK;
        ctx->buttons_count -= 4;
    }

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Buttons count: %d", ctx->buttons_count);
#endif

    // Get and validate touchpad parameters
    ctx->touchpad_count = data[16];
    ctx->touchpad_finger_count = data[17];

    // Get device Serial - MAC address
    char serial[18];
    (void)SDL_snprintf(serial, sizeof(serial), "%.2x-%.2x-%.2x-%.2x-%.2x-%.2x",
                       data[18], data[19], data[20], data[21], data[22], data[23]);

#if defined(DEBUG_SINPUT_INIT)
    SDL_Log("Serial num: %s", serial);
#endif
    HIDAPI_SetDeviceSerial(device, serial);

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
        HIDAPI_DumpPacket("SInput packet: size = %d", data, read);
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

    switch (device->product_id) {
    case USB_PRODUCT_HANDHELDLEGEND_GCULTIMATE:
        HIDAPI_SetDeviceName(device, "HHL GC Ultimate");
        break;
    case USB_PRODUCT_HANDHELDLEGEND_PROGCC:
        HIDAPI_SetDeviceName(device, "HHL ProGCC");
        break;
    case USB_PRODUCT_BONZIRICHANNEL_FIREBIRD:
        HIDAPI_SetDeviceName(device, "Bonziri Firebird");
        break;
    case USB_PRODUCT_HANDHELDLEGEND_SINPUT_GENERIC:
    default:
        // Use the USB product name
        break;
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

    if (ctx->dpad_supported) {
        joystick->nhats = 1;
    }

    if (ctx->accelerometer_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 1000.0f / ctx->polling_rate_ms);
    }

    if (ctx->gyroscope_supported) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 1000.0f / ctx->polling_rate_ms);
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

    if (ctx->joystick_rgb_supported) {
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

    if (ctx->dpad_supported) {
        Uint8 hat = SDL_HAT_CENTERED;

        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BTN_IDX_DPAD_UP)) {
            hat |= SDL_HAT_UP;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BTN_IDX_DPAD_DOWN)) {
            hat |= SDL_HAT_DOWN;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BTN_IDX_DPAD_LEFT)) {
            hat |= SDL_HAT_LEFT;
        }
        if (data[SINPUT_REPORT_IDX_BUTTONS_0] & (1 << SINPUT_BTN_IDX_DPAD_RIGHT)) {
            hat |= SDL_HAT_RIGHT;
        }
        SDL_SendJoystickHat(timestamp, joystick, 0, hat);
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

        SDL_PowerState state = SDL_POWERSTATE_UNKNOWN;
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
        default:
            break;
        }

        if (state != SDL_POWERSTATE_UNKNOWN) {
            SDL_SendJoystickPowerInfo(joystick, state, percent);
        }
    }

    // Extract the IMU timestamp delta (in microseconds)
    Uint32 imu_timestamp_us = EXTRACTUINT32(data, SINPUT_REPORT_IDX_IMU_TIMESTAMP);
    Uint32 imu_time_delta_us = 0;

    // Check if we should process IMU data and if sensors are enabled
    if (ctx->sensors_enabled) {

        if (imu_timestamp_us >= ctx->last_imu_timestamp_us) {
            imu_time_delta_us = (imu_timestamp_us - ctx->last_imu_timestamp_us);
        } else {
            // Handle rollover case
            imu_time_delta_us = (UINT32_MAX - ctx->last_imu_timestamp_us) + imu_timestamp_us + 1;
        }

        // Convert delta to nanoseconds and update running timestamp
        ctx->imu_timestamp_ns += (Uint64)imu_time_delta_us * 1000;

        // Update last timestamp
        ctx->last_imu_timestamp_us = imu_timestamp_us;

        // Process Accelerometer
        if (ctx->accelerometer_supported) {

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Y);
            imu_values[2] = -(float)accel * ctx->accelScale; // Y-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_Z);
            imu_values[1] = (float)accel * ctx->accelScale; // Z-axis acceleration

            accel = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_ACCEL_X);
            imu_values[0] = -(float)accel * ctx->accelScale; // X-axis acceleration

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, ctx->imu_timestamp_ns, imu_values, 3);
        }

        // Process Gyroscope
        if (ctx->gyroscope_supported) {

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Y);
            imu_values[2] = -(float)gyro * ctx->gyroScale; // Y-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_Z);
            imu_values[1] = (float)gyro * ctx->gyroScale; // Z-axis rotation

            gyro = EXTRACTSINT16(data, SINPUT_REPORT_IDX_IMU_GYRO_X);
            imu_values[0] = -(float)gyro * ctx->gyroScale; // X-axis rotation

            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, ctx->imu_timestamp_ns, imu_values, 3);
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
