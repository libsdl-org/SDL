/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
/* This driver supports both simplified reports and the extended input reports enabled by Steam.
   Code and logic contributed by Valve Corporation under the SDL zlib license.
*/
#include "../../SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hints.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../../SDL_hints_c.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"


#ifdef SDL_JOYSTICK_HIDAPI_PS3

/* Define this if you want to log all packets from the controller */
/*#define DEBUG_PS3_PROTOCOL*/

#define LOAD16(A, B)  (Sint16)((Uint16)(A) | (((Uint16)(B)) << 8))

typedef enum
{
    k_EPS3ReportIdState = 1,
    k_EPS3ReportIdEffects = 1,
} EPS3ReportId;

typedef struct {
    SDL_HIDAPI_Device *device;
    SDL_Joystick *joystick;
    SDL_bool is_shanwan;
    SDL_bool report_sensors;
    SDL_bool effects_updated;
    Uint8 last_state[USB_PACKET_LENGTH];
    int player_index;
    Uint8 rumble_left;
    Uint8 rumble_right;
} SDL_DriverPS3_Context;


static int HIDAPI_DriverPS3_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *effect, int size);

static void
HIDAPI_DriverPS3_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_PS3, callback, userdata);
}

static void
HIDAPI_DriverPS3_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI_PS3, callback, userdata);
}

static SDL_bool
HIDAPI_DriverPS3_IsEnabled(void)
{
#if defined(__MACOSX__)
    /* This works well on macOS */
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_PS3,
               SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI,
                   SDL_HIDAPI_DEFAULT));
#elif defined(__WINDOWS__)
    /* You can't initialize the controller with the stock Windows drivers
     * See https://github.com/ViGEm/DsHidMini as an alternative driver
     */
    return SDL_FALSE;
#elif defined(__LINUX__)
    /* Linux drivers do a better job of managing the transition between
     * USB and Bluetooth. There are also some quirks in communicating
     * with PS3 controllers that have been implemented in SDL's hidapi
     * for libusb, but are not possible to support using hidraw if the
     * kernel doesn't already know about them.
     */
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_PS3, SDL_FALSE);
#else
    /* Untested, default off */
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_PS3, SDL_FALSE);
#endif
}

static SDL_bool
HIDAPI_DriverPS3_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GameControllerType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id == USB_VENDOR_SONY && product_id == USB_PRODUCT_SONY_DS3) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static const char *
HIDAPI_DriverPS3_GetDeviceName(const char *name, Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_SONY) {
        if (name && SDL_strncasecmp(name, "ShanWan", 7) == 0) {
            return "ShanWan PS3 Controller";
        } else {
            return "PS3 Controller";
        }
    }
    return NULL;
}

static int ReadFeatureReport(SDL_hid_device *dev, Uint8 report_id, Uint8 *report, size_t length)
{
    SDL_memset(report, 0, length);
    report[0] = report_id;
    return SDL_hid_get_feature_report(dev, report, length);
}

static int SendFeatureReport(SDL_hid_device *dev, Uint8 *report, size_t length)
{
    return SDL_hid_send_feature_report(dev, report, length);
}

static SDL_bool
HIDAPI_DriverPS3_InitDevice(SDL_HIDAPI_Device *device)
{
    return HIDAPI_JoystickConnected(device, NULL);
}

static int
HIDAPI_DriverPS3_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static int
HIDAPI_DriverPS3_UpdateEffects(SDL_HIDAPI_Device *device)
{
    SDL_DriverPS3_Context *ctx = (SDL_DriverPS3_Context *)device->context;

    Uint8 effects[] = {
        0x01, 0xff, 0x00, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00
    };

    effects[2] = ctx->rumble_right ? 1 : 0;
    effects[4] = ctx->rumble_left;

    effects[9] = (0x01 << (1+(ctx->player_index % 4)));

    return HIDAPI_DriverPS3_SendJoystickEffect(device, ctx->joystick, &effects, sizeof(effects));
}

static void
HIDAPI_DriverPS3_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
    SDL_DriverPS3_Context *ctx = (SDL_DriverPS3_Context *)device->context;

    if (!ctx) {
        return;
    }

    ctx->player_index = player_index;

    /* This will set the new LED state based on the new player index */
    HIDAPI_DriverPS3_UpdateEffects(device);
}

static SDL_bool
HIDAPI_DriverPS3_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverPS3_Context *ctx;

    ctx = (SDL_DriverPS3_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    ctx->device = device;
    ctx->joystick = joystick;

    device->dev = SDL_hid_open_path(device->path, 0);
    if (!device->dev) {
        SDL_free(ctx);
        SDL_SetError("Couldn't open %s", device->path);
        return SDL_FALSE;
    }
    device->context = ctx;

    if (SDL_strncasecmp(device->name, "ShanWan", 7) == 0) {
        ctx->is_shanwan = SDL_TRUE;
    }

    /* Set the controller into report mode over Bluetooth */
    {
        Uint8 data[] = { 0xf4, 0x42, 0x03, 0x00, 0x00 };

        SendFeatureReport(device->dev, data, sizeof(data));
    }

    /* Set the controller into report mode over USB */
    {
        Uint8 data[USB_PACKET_LENGTH];
        int size;

        if ((size = ReadFeatureReport(device->dev, 0xf2, data, 17)) < 0) {
            SDL_SetError("Couldn't read feature report 0xf2");
            return SDL_FALSE;
        }
#ifdef DEBUG_PS3_PROTOCOL
        HIDAPI_DumpPacket("PS3 0xF2 packet: size = %d", data, size);
#endif
        if ((size = ReadFeatureReport(device->dev, 0xf5, data, 8)) < 0) {
            SDL_SetError("Couldn't read feature report 0xf5");
            return SDL_FALSE;
        }
#ifdef DEBUG_PS3_PROTOCOL
        HIDAPI_DumpPacket("PS3 0xF5 packet: size = %d", data, size);
#endif
        if (!ctx->is_shanwan) {
            /* An output report could cause ShanWan controllers to rumble non-stop */
            SDL_hid_write(device->dev, data, 1);
        }
    }

    /* Initialize player index (needed for setting LEDs) */
    ctx->player_index = SDL_JoystickGetPlayerIndex(joystick);

    /* Initialize the joystick capabilities */
    joystick->nbuttons = 15;
    joystick->naxes = 16;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 100.0f);

    return SDL_TRUE;
}

static int
HIDAPI_DriverPS3_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverPS3_Context *ctx = (SDL_DriverPS3_Context *)device->context;

    ctx->rumble_left = (low_frequency_rumble >> 8);
    ctx->rumble_right = (high_frequency_rumble >> 8);

    return HIDAPI_DriverPS3_UpdateEffects(device);
}

static int
HIDAPI_DriverPS3_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32
HIDAPI_DriverPS3_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return SDL_JOYCAP_RUMBLE;
}

static int
HIDAPI_DriverPS3_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int
HIDAPI_DriverPS3_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *effect, int size)
{
    Uint8 data[49];
    int report_size, offset;

    SDL_zeroa(data);

    data[0] = k_EPS3ReportIdEffects;
    report_size = sizeof(data);
    offset = 1;
    SDL_memcpy(&data[offset], effect, SDL_min((sizeof(data) - offset), (size_t)size));

    if (SDL_HIDAPI_SendRumble(device, data, report_size) != report_size) {
        return SDL_SetError("Couldn't send rumble packet");
    }
    return 0;
}

static int
HIDAPI_DriverPS3_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, SDL_bool enabled)
{
    SDL_DriverPS3_Context *ctx = (SDL_DriverPS3_Context *)device->context;

    ctx->report_sensors = enabled;

    return 0;
}

static float
HIDAPI_DriverPS3_ScaleAccel(Sint16 value)
{
    /* Accelerometer values are in big endian order */
    value = SDL_SwapBE16(value);
    return (float)(value - 511) / 113.0f;
}

static void
HIDAPI_DriverPS3_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverPS3_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;

    if (ctx->last_state[2] != data[2]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[2] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[2] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[2] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[2] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, (data[2] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (data[2] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, (data[2] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, (data[2] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[3] != data[3]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[3] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[3] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[3] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[3] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[3] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[3] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[4] != data[4]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[4] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
    }

    axis = ((int)data[18] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)data[19] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = ((int)data[6] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = ((int)data[7] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = ((int)data[8] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = ((int)data[9] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    /* Buttons are mapped as axes in the order they appear in the button enumeration */
    {
        static int button_axis_offsets[] = {
            24, /* SDL_CONTROLLER_BUTTON_A */
            23, /* SDL_CONTROLLER_BUTTON_B */
            25, /* SDL_CONTROLLER_BUTTON_X */
            22, /* SDL_CONTROLLER_BUTTON_Y */
            0,  /* SDL_CONTROLLER_BUTTON_BACK */
            0,  /* SDL_CONTROLLER_BUTTON_GUIDE */
            0,  /* SDL_CONTROLLER_BUTTON_START */
            0,  /* SDL_CONTROLLER_BUTTON_LEFTSTICK */
            0,  /* SDL_CONTROLLER_BUTTON_RIGHTSTICK */
            20, /* SDL_CONTROLLER_BUTTON_LEFTSHOULDER */
            21, /* SDL_CONTROLLER_BUTTON_RIGHTSHOULDER */
            14, /* SDL_CONTROLLER_BUTTON_DPAD_UP */
            16, /* SDL_CONTROLLER_BUTTON_DPAD_DOWN */
            17, /* SDL_CONTROLLER_BUTTON_DPAD_LEFT */
            15, /* SDL_CONTROLLER_BUTTON_DPAD_RIGHT */
        };
        int i, axis_index = 6;

        for (i = 0; i < SDL_arraysize(button_axis_offsets); ++i) {
            int offset = button_axis_offsets[i];
            if (!offset) {
                /* This button doesn't report as an axis */
                continue;
            }

            axis = ((int)data[offset] * 257) - 32768;
            SDL_PrivateJoystickAxis(joystick, axis_index, axis);
            ++axis_index;
        }
    }

    if (ctx->report_sensors) {
        float sensor_data[3];

        sensor_data[0] = HIDAPI_DriverPS3_ScaleAccel(LOAD16(data[41], data[42]));
        sensor_data[1] = -HIDAPI_DriverPS3_ScaleAccel(LOAD16(data[45], data[46]));
        sensor_data[2] = -HIDAPI_DriverPS3_ScaleAccel(LOAD16(data[43], data[44]));
        SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_ACCEL, sensor_data, SDL_arraysize(sensor_data));
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static SDL_bool
HIDAPI_DriverPS3_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverPS3_Context *ctx = (SDL_DriverPS3_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
    }
    if (!joystick) {
        return SDL_FALSE;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_PS3_PROTOCOL
        HIDAPI_DumpPacket("PS3 packet: size = %d", data, size);
#endif

        switch (data[0]) {
        case k_EPS3ReportIdState:
            if (data[1] == 0xFF) {
                /* Invalid data packet, ignore */
                break;
            }
            HIDAPI_DriverPS3_HandleStatePacket(joystick, ctx, data, size);

            /* Wait for the first report to set the LED state after the controller stops blinking */
            if (!ctx->effects_updated) {
                HIDAPI_DriverPS3_UpdateEffects(device);
                ctx->effects_updated = SDL_TRUE;
            }
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown PS3 packet: 0x%.2x\n", data[0]);
#endif
            break;
        }
    }

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, joystick->instance_id);
    }
    return (size >= 0);
}

static void
HIDAPI_DriverPS3_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_LockMutex(device->dev_lock);
    {
        SDL_hid_close(device->dev);
        device->dev = NULL;

        SDL_free(device->context);
        device->context = NULL;
    }
    SDL_UnlockMutex(device->dev_lock);
}

static void
HIDAPI_DriverPS3_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS3 =
{
    SDL_HINT_JOYSTICK_HIDAPI_PS3,
    SDL_TRUE,
    HIDAPI_DriverPS3_RegisterHints,
    HIDAPI_DriverPS3_UnregisterHints,
    HIDAPI_DriverPS3_IsEnabled,
    HIDAPI_DriverPS3_IsSupportedDevice,
    HIDAPI_DriverPS3_GetDeviceName,
    HIDAPI_DriverPS3_InitDevice,
    HIDAPI_DriverPS3_GetDevicePlayerIndex,
    HIDAPI_DriverPS3_SetDevicePlayerIndex,
    HIDAPI_DriverPS3_UpdateDevice,
    HIDAPI_DriverPS3_OpenJoystick,
    HIDAPI_DriverPS3_RumbleJoystick,
    HIDAPI_DriverPS3_RumbleJoystickTriggers,
    HIDAPI_DriverPS3_GetJoystickCapabilities,
    HIDAPI_DriverPS3_SetJoystickLED,
    HIDAPI_DriverPS3_SendJoystickEffect,
    HIDAPI_DriverPS3_SetJoystickSensorsEnabled,
    HIDAPI_DriverPS3_CloseJoystick,
    HIDAPI_DriverPS3_FreeDevice,
};

#endif /* SDL_JOYSTICK_HIDAPI_PS3 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
