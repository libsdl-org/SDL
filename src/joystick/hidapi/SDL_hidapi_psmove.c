/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_HIDAPI_PSMOVE

#define LOAD16(A, B)       (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))
#define PSMOVE_ACCEL_SCALE       (SDL_STANDARD_GRAVITY / 8192.0f)
#define PSMOVE_GYRO_SCALE        (SDL_PI_F / 180.0f / 16.4f)
#define PSMOVE_BUFFER_SIZE       9
#define PSMOVE_EXT_DATA_BUF_SIZE 5

typedef struct
{
    Uint8 type;                             /* message type, must be PSMove_Req_SetLEDs */
    Uint8 _zero;                            /* must be zero */
    Uint8 r;                                /* red value, 0x00..0xff */
    Uint8 g;                                /* green value, 0x00..0xff */
    Uint8 b;                                /* blue value, 0x00..0xff */
    Uint8 _zero2;                           /* must be zero */
    Uint8 rumble;                           /* rumble value, 0x00..0xff */
    Uint8 _padding[PSMOVE_BUFFER_SIZE - 7]; /* must be zero */
} PSMove_Data_LEDs;

typedef struct
{
    Uint8 type; /* message type, must be PSMove_Req_GetInput */
    Uint8 buttons1;
    Uint8 buttons2;
    Uint8 buttons3;
    Uint8 buttons4;
    Uint8 trigger;  /* trigger value; 0..255 */
    Uint8 trigger2; /* trigger value, 2nd frame */
    Uint32 _unk;
    Uint8 timehigh; /* high byte of timestamp */
    Uint8 battery;  /* battery level; 0x05 = max, 0xEE = USB charging */
    Uint8 aX[2];    /* 1st frame */
    Uint8 aY[2];
    Uint8 aZ[2];
    Uint8 aX2[2]; /* 2nd half-frame */
    Uint8 aY2[2];
    Uint8 aZ2[2];
    Uint8 gX[2]; /* 1st half-frame */
    Uint8 gY[2];
    Uint8 gZ[2];
    Uint8 gX2[2]; /* 2nd half-frame */
    Uint8 gY2[2];
    Uint8 gZ2[2];
    Uint8 temphigh;       /* temperature (bits 12-5) */
    Uint8 templow_mXhigh; /* temp (bits 4-1); magneto X (bits 12-9) (magneto only in ZCM1) */
} PSMove_Data_Input_Common;

typedef struct
{
    PSMove_Data_Input_Common common;

    Uint8 mXlow;                             /* magnetometer X (bits 8-1) */
    Uint8 mYhigh;                            /* magnetometer Y (bits 12-5) */
    Uint8 mYlow_mZhigh;                      /* magnetometer: Y (bits 4-1), Z (bits 12-9) */
    Uint8 mZlow;                             /* magnetometer Z (bits 8-1) */
    Uint8 timelow;                           /* low byte of timestamp */
    Uint8 extdata[PSMOVE_EXT_DATA_BUF_SIZE]; /* external device data (EXT port) */
} PSMove_ZCM1_Data_Input;

typedef struct
{
    PSMove_Data_Input_Common common;

    Uint8 timehigh2; /* same as timestamp at offsets 0x0B */
    Uint8 timelow;   /* same as timestamp at offsets 0x2B */
    Uint8 _unk41;
    Uint8 _unk42;
    Uint8 timelow2;
} PSMove_ZCM2_Data_Input;

typedef union
{
    PSMove_Data_Input_Common common;
    PSMove_ZCM1_Data_Input zcm1;
    PSMove_ZCM2_Data_Input zcm2;
} PSMove_Data_Input;

typedef enum
{
    Model_Unknown = 0,
    Model_ZCM1,
    Model_ZCM2,
    Model_Count
} PSMove_Model_Type;

typedef struct
{
    SDL_HIDAPI_Device *device;
    SDL_Joystick *joystick;

    PSMove_Model_Type model;
    PSMove_Data_LEDs leds;
    PSMove_Data_Input input;
    PSMove_Data_Input last_state;

    bool report_sensors;
    bool effects_updated;
} SDL_DriverPSMove_Context;

static void HIDAPI_DriverPSMove_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_PSMOVE, callback, userdata);
}

static void HIDAPI_DriverPSMove_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_PSMOVE, callback, userdata);
}

static bool HIDAPI_DriverPSMove_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_PSMOVE, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverPSMove_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return SDL_IsJoystickPSMove(vendor_id, product_id);
}

static bool HIDAPI_DriverPSMove_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverPSMove_Context *ctx;

    ctx = (SDL_DriverPSMove_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return false;
    }
    ctx->device = device;
    device->context = ctx;

    ctx->leds.type = 0x06; // PSMove_Req_SetLEDs
    ctx->model = (device->product_id == USB_PRODUCT_SONY_PSMOVE) ? Model_ZCM1 : Model_ZCM2;

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverPSMove_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverPSMove_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverPSMove_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    if (SDL_HIDAPI_SendRumble(device, (Uint8 *)data, size) != size) {
        return SDL_SetError("Couldn't send rumble packet");
    }
    return true;
}

static bool HIDAPI_DriverPSMove_UpdateEffects(SDL_HIDAPI_Device *device)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;

    return HIDAPI_DriverPSMove_SendJoystickEffect(device, ctx->joystick, &(ctx->leds), sizeof(ctx->leds));
}

static inline int
psmove_decode_16bit(Uint8 a, Uint8 b)
{
    return LOAD16(a & 0xFF, b & 0xFF) - 0x8000;
}

static inline int
psmove_decode_16bit_twos_complement(Uint8 a, Uint8 b)
{
    Uint16 value = LOAD16(a & 0xFF, b & 0xFF);
    return (value & 0x8000) ? (-(~value & 0xFFFF) + 1) : value;
}

static void HIDAPI_DriverPSMove_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverPSMove_Context *ctx)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    if (ctx->last_state.common.buttons1 != ctx->input.common.buttons1) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((ctx->input.common.buttons1 & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, ((ctx->input.common.buttons1 & 0x08) != 0));
    }

    if (ctx->last_state.common.buttons2 != ctx->input.common.buttons2) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_NORTH, ((ctx->input.common.buttons2 & 0x10) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_EAST, ((ctx->input.common.buttons2 & 0x20) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_SOUTH, ((ctx->input.common.buttons2 & 0x40) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_WEST, ((ctx->input.common.buttons2 & 0x80) != 0));
    }

    if (ctx->last_state.common.buttons3 != ctx->input.common.buttons3) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, ((ctx->input.common.buttons3 & 0x01) != 0));
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, ((ctx->input.common.buttons3 & 0x08) != 0));
    }

    if (ctx->model == Model_ZCM1)
        axis = (((ctx->input.common.trigger + ctx->input.common.trigger2) / 2) * 257) - 32768;
    else
        axis = (ctx->input.common.trigger * 257) - 32768;
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);

    if (ctx->report_sensors) {

        float sensor_data[3];
        if (ctx->model == Model_ZCM1) {
            sensor_data[0] = (float)psmove_decode_16bit(ctx->input.common.aX2[0], ctx->input.common.aX2[1]);
            sensor_data[1] = (float)psmove_decode_16bit(ctx->input.common.aY2[0], ctx->input.common.aY2[1]);
            sensor_data[2] = (float)psmove_decode_16bit(ctx->input.common.aZ2[0], ctx->input.common.aZ2[1]);
        } else {
            sensor_data[0] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.aX[0], ctx->input.common.aX[1]);
            sensor_data[1] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.aY[0], ctx->input.common.aY[1]);
            sensor_data[2] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.aZ[0], ctx->input.common.aZ[1]);
        }
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, timestamp, sensor_data, SDL_arraysize(sensor_data));

        if (ctx->model == Model_ZCM1) {
            sensor_data[0] = (float)psmove_decode_16bit(ctx->input.common.gX2[0], ctx->input.common.gX2[1]);
            sensor_data[1] = (float)psmove_decode_16bit(ctx->input.common.gY2[0], ctx->input.common.gY2[1]);
            sensor_data[2] = (float)psmove_decode_16bit(ctx->input.common.gZ2[0], ctx->input.common.gZ2[1]);
        } else {
            sensor_data[0] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.gX[0], ctx->input.common.gX[1]);
            sensor_data[1] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.gY[0], ctx->input.common.gY[1]);
            sensor_data[2] = (float)psmove_decode_16bit_twos_complement(ctx->input.common.gZ[0], ctx->input.common.gZ[1]);
        }
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, timestamp, sensor_data, SDL_arraysize(sensor_data));
    }
    SDL_memcpy(&ctx->last_state, &ctx->input, sizeof(ctx->last_state));
}

static bool HIDAPI_DriverPSMove_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromID(device->joysticks[0]);
    } else {
        return false;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_PSMOVE_PROTOCOL
        HIDAPI_DumpPacket("PSMove packet: size = %d", data, size);
#endif
        if (!joystick) {
            continue;
        }

        switch (data[0]) {
        case 0x01: /*PSMove_Req_GetInput*/
            if (data[1] == 0xFF) {
                // Invalid data packet, ignore
                break;
            }
            if (ctx->model == Model_ZCM1) {
                SDL_memcpy(&(ctx->input.zcm1), data, sizeof(ctx->input.zcm1));
            } else {
                SDL_memcpy(&(ctx->input.zcm2), data, sizeof(ctx->input.zcm2));
            }
            HIDAPI_DriverPSMove_HandleStatePacket(joystick, ctx);
            if (!ctx->effects_updated) {
                HIDAPI_DriverPSMove_UpdateEffects(device);
                ctx->effects_updated = true;
            }
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown PSMove packet: 0x%.2x", data[0]);
#endif
            break;
        }
    }

    if (size < 0) {
        // Read error, device is disconnected
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return (size >= 0);
}

static bool HIDAPI_DriverPSMove_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;

    SDL_AssertJoysticksLocked();

    ctx->joystick = joystick;
    ctx->effects_updated = false;
    ctx->leds.rumble = 0;

    // Initialize the joystick capabilities
    joystick->nbuttons = 8;
    joystick->naxes = 1;
    joystick->nhats = 0;

    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 75.0f);
    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 75.0f);

    return true;
}

static bool HIDAPI_DriverPSMove_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;

    Uint8 rumble = (high_frequency_rumble >> 8);

    ctx->leds.rumble = rumble;

    return HIDAPI_DriverPSMove_UpdateEffects(device);
}

static bool HIDAPI_DriverPSMove_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverPSMove_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return (SDL_JOYSTICK_CAP_RGB_LED | SDL_JOYSTICK_CAP_RUMBLE);
}

static bool HIDAPI_DriverPSMove_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;

    ctx->leds.r = red;
    ctx->leds.g = green;
    ctx->leds.b = blue;

    return HIDAPI_DriverPSMove_UpdateEffects(device);
}

static bool HIDAPI_DriverPSMove_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;
    ctx->report_sensors = enabled;
    return true;
}

static void HIDAPI_DriverPSMove_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverPSMove_Context *ctx = (SDL_DriverPSMove_Context *)device->context;

    ctx->joystick = NULL;
}

static void HIDAPI_DriverPSMove_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPSMove = {
    SDL_HINT_JOYSTICK_HIDAPI_PSMOVE,
    true,
    HIDAPI_DriverPSMove_RegisterHints,
    HIDAPI_DriverPSMove_UnregisterHints,
    HIDAPI_DriverPSMove_IsEnabled,
    HIDAPI_DriverPSMove_IsSupportedDevice,
    HIDAPI_DriverPSMove_InitDevice,
    HIDAPI_DriverPSMove_GetDevicePlayerIndex,
    HIDAPI_DriverPSMove_SetDevicePlayerIndex,
    HIDAPI_DriverPSMove_UpdateDevice,
    HIDAPI_DriverPSMove_OpenJoystick,
    HIDAPI_DriverPSMove_RumbleJoystick,
    HIDAPI_DriverPSMove_RumbleJoystickTriggers,
    HIDAPI_DriverPSMove_GetJoystickCapabilities,
    HIDAPI_DriverPSMove_SetJoystickLED,
    HIDAPI_DriverPSMove_SendJoystickEffect,
    HIDAPI_DriverPSMove_SetJoystickSensorsEnabled,
    HIDAPI_DriverPSMove_CloseJoystick,
    HIDAPI_DriverPSMove_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_PSMOVE

#endif // SDL_JOYSTICK_HIDAPI
