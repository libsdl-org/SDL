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
#include "../../SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hints.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"


#ifdef SDL_JOYSTICK_HIDAPI_SHIELD

/* Define this if you want to log all packets from the controller */
/*#define DEBUG_SHIELD_PROTOCOL*/

#define CMD_BATTERY_STATE 0x07
#define CMD_RUMBLE 0x39
#define CMD_CHARGE_STATE 0x3A

/* Milliseconds between polls of battery state */
#define BATTERY_POLL_INTERVAL_MS 60000

/* Milliseconds between retransmission of rumble to keep motors running */
#define RUMBLE_REFRESH_INTERVAL_MS 500

/* Reports that are too small are dropped over Bluetooth */
#define HID_REPORT_SIZE 33

typedef enum {
    k_ShieldReportIdControllerState = 0x01,
    k_ShieldReportIdCommandResponse = 0x03,
    k_ShieldReportIdCommandRequest = 0x04,
} EShieldReportId;

/* This same report structure is used for both requests and responses */
typedef struct {
    Uint8 report_id;
    Uint8 cmd;
    Uint8 seq_num;
    Uint8 payload[HID_REPORT_SIZE - 3];
} ShieldCommandReport_t;
SDL_COMPILE_TIME_ASSERT(ShieldCommandReport_t, sizeof(ShieldCommandReport_t) == HID_REPORT_SIZE);

typedef struct {
    Uint8 seq_num;

    SDL_JoystickPowerLevel battery_level;
    SDL_bool charging;
    Uint32 last_battery_query_time;

    SDL_bool rumble_report_pending;
    SDL_bool rumble_update_pending;
    Uint8 left_motor_amplitude;
    Uint8 right_motor_amplitude;
    Uint32 last_rumble_time;

    Uint8 last_state[USB_PACKET_LENGTH];
} SDL_DriverShield_Context;


static SDL_bool
HIDAPI_DriverShield_IsSupportedDevice(const char *name, SDL_GameControllerType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    return (type == SDL_CONTROLLER_TYPE_NVIDIA_SHIELD) ? SDL_TRUE : SDL_FALSE;
}

static const char *
HIDAPI_DriverShield_GetDeviceName(const char *name, Uint16 vendor_id, Uint16 product_id)
{
    return "NVIDIA SHIELD Controller";
}

static SDL_bool
HIDAPI_DriverShield_InitDevice(SDL_HIDAPI_Device *device)
{
    return HIDAPI_JoystickConnected(device, NULL);
}

static int
HIDAPI_DriverShield_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void
HIDAPI_DriverShield_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static int
HIDAPI_DriverShield_SendCommand(SDL_HIDAPI_Device *device, Uint8 cmd, const void *data, int size)
{
    SDL_DriverShield_Context *ctx = device->context;
    ShieldCommandReport_t cmd_pkt;

    if (size > sizeof(cmd_pkt.payload)) {
        return SDL_SetError("Command data exceeds HID report size");
    }

    if (SDL_HIDAPI_LockRumble() < 0) {
        return -1;
    }

    cmd_pkt.report_id = k_ShieldReportIdCommandRequest;
    cmd_pkt.cmd = cmd;
    cmd_pkt.seq_num = ctx->seq_num++;
    if (data) {
        SDL_memcpy(cmd_pkt.payload, data, size);
    }

    /* Zero unused data in the payload */
    SDL_memset(&cmd_pkt.payload[size], 0, sizeof(cmd_pkt.payload) - size);

    if (SDL_HIDAPI_SendRumbleAndUnlock(device, (Uint8*)&cmd_pkt, sizeof(cmd_pkt)) != sizeof(cmd_pkt)) {
        return SDL_SetError("Couldn't send command packet");
    }

    return 0;
}

static SDL_bool
HIDAPI_DriverShield_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverShield_Context *ctx;

    ctx = (SDL_DriverShield_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }

    device->dev = SDL_hid_open_path(device->path, 0);
    if (!device->dev) {
        SDL_SetError("Couldn't open %s", device->path);
        SDL_free(ctx);
        return SDL_FALSE;
    }
    device->context = ctx;

    /* Initialize the joystick capabilities */
    joystick->nbuttons = 16;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_UNKNOWN;

    /* Request battery and charging info */
    ctx->battery_level = SDL_JOYSTICK_POWER_UNKNOWN;
    ctx->last_battery_query_time = SDL_GetTicks();
    HIDAPI_DriverShield_SendCommand(device, CMD_CHARGE_STATE, NULL, 0);
    HIDAPI_DriverShield_SendCommand(device, CMD_BATTERY_STATE, NULL, 0);

    return SDL_TRUE;
}

static int
HIDAPI_DriverShield_SendNextRumble(SDL_HIDAPI_Device *device)
{
    SDL_DriverShield_Context *ctx = device->context;
    Uint8 rumble_data[3];

    if (!ctx->rumble_update_pending) {
        return 0;
    }

    rumble_data[0] = 0x01; /* enable */
    rumble_data[1] = ctx->left_motor_amplitude;
    rumble_data[2] = ctx->right_motor_amplitude;

    ctx->rumble_update_pending = SDL_FALSE;
    ctx->last_rumble_time = SDL_GetTicks();

    return HIDAPI_DriverShield_SendCommand(device, CMD_RUMBLE, rumble_data, sizeof(rumble_data));
}

static int
HIDAPI_DriverShield_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverShield_Context *ctx = device->context;

    /* The rumble motors are quite intense, so tone down the intensity like the official driver does */
    ctx->left_motor_amplitude = low_frequency_rumble >> 11;
    ctx->right_motor_amplitude = high_frequency_rumble >> 11;
    ctx->rumble_update_pending = SDL_TRUE;

    if (ctx->rumble_report_pending) {
        /* We will service this after the hardware acknowledges the previous request */
        return 0;
    }

    return HIDAPI_DriverShield_SendNextRumble(device);
}

static int
HIDAPI_DriverShield_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32
HIDAPI_DriverShield_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return SDL_JOYCAP_RUMBLE;
}

static int
HIDAPI_DriverShield_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int
HIDAPI_DriverShield_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    const Uint8 *data_bytes = data;

    if (size > 1) {
        /* Single command byte followed by a variable length payload */
        return HIDAPI_DriverShield_SendCommand(device, data_bytes[0], &data_bytes[1], size - 1);
    } else if (size == 1) {
        /* Single command byte with no payload */
        return HIDAPI_DriverShield_SendCommand(device, data_bytes[0], NULL, 0);
    } else {
        return SDL_SetError("Effect data must at least contain a command byte");
    }
}

static int
HIDAPI_DriverShield_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, SDL_bool enabled)
{
    return SDL_Unsupported();
}

static void
HIDAPI_DriverShield_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverShield_Context *ctx, Uint8 *data, int size)
{
    if (size < 23) {
        return;
    }

    if (ctx->last_state[2] != data[2]) {
        SDL_bool dpad_up = SDL_FALSE;
        SDL_bool dpad_down = SDL_FALSE;
        SDL_bool dpad_left = SDL_FALSE;
        SDL_bool dpad_right = SDL_FALSE;

        switch (data[2]) {
        case 0:
            dpad_up = SDL_TRUE;
            break;
        case 1:
            dpad_up = SDL_TRUE;
            dpad_right = SDL_TRUE;
            break;
        case 2:
            dpad_right = SDL_TRUE;
            break;
        case 3:
            dpad_right = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 4:
            dpad_down = SDL_TRUE;
            break;
        case 5:
            dpad_left = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 6:
            dpad_left = SDL_TRUE;
            break;
        case 7:
            dpad_up = SDL_TRUE;
            dpad_left = SDL_TRUE;
            break;
        default:
            break;
        }
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, dpad_down);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, dpad_up);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, dpad_right);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, dpad_left);
    }

    if (ctx->last_state[3] != data[3]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[3] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[3] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[3] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[3] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[3] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[3] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[3] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[3] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[4] != data[4]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[4] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
    }

    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, SDL_SwapLE16(*(Sint16*)&data[9]) - 0x8000);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, SDL_SwapLE16(*(Sint16*)&data[11]) - 0x8000);

    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, SDL_SwapLE16(*(Sint16*)&data[13]) - 0x8000);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, SDL_SwapLE16(*(Sint16*)&data[15]) - 0x8000);

    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_SwapLE16(*(Sint16*)&data[19]) - 0x8000);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_SwapLE16(*(Sint16*)&data[21]) - 0x8000);

    if (ctx->last_state[17] != data[17]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MISC1, (data[17] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[17] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[17] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static SDL_bool
HIDAPI_DriverShield_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverShield_Context *ctx = (SDL_DriverShield_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size = 0;
    ShieldCommandReport_t *cmd_resp_report;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
    }
    if (!joystick) {
        return SDL_FALSE;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_SHIELD_PROTOCOL
        HIDAPI_DumpPacket("NVIDIA SHIELD packet: size = %d", data, size);
#endif
        /* Byte 0 is HID report ID */
        switch (data[0]) {
            case k_ShieldReportIdControllerState:
                HIDAPI_DriverShield_HandleStatePacket(joystick, ctx, data, size);
                break;
            case k_ShieldReportIdCommandResponse:
                cmd_resp_report = (ShieldCommandReport_t*)data;
                switch (cmd_resp_report->cmd) {
                    case CMD_RUMBLE:
                        ctx->rumble_report_pending = SDL_FALSE;
                        HIDAPI_DriverShield_SendNextRumble(device);
                        break;
                    case CMD_CHARGE_STATE:
                        ctx->charging = cmd_resp_report->payload[0] != 0;
                        SDL_PrivateJoystickBatteryLevel(joystick, ctx->charging ? SDL_JOYSTICK_POWER_WIRED : ctx->battery_level);
                        break;
                    case CMD_BATTERY_STATE:
                        switch (cmd_resp_report->payload[2]) {
                            case 0:
                                ctx->battery_level = SDL_JOYSTICK_POWER_EMPTY;
                                break;
                            case 1:
                                ctx->battery_level = SDL_JOYSTICK_POWER_LOW;
                                break;
                            case 2: /* 40% */
                            case 3: /* 60% */
                            case 4: /* 80% */
                                ctx->battery_level = SDL_JOYSTICK_POWER_MEDIUM;
                                break;
                            case 5:
                                ctx->battery_level = SDL_JOYSTICK_POWER_FULL;
                                break;
                            default:
                                ctx->battery_level = SDL_JOYSTICK_POWER_UNKNOWN;
                                break;
                        }
                        SDL_PrivateJoystickBatteryLevel(joystick, ctx->charging ? SDL_JOYSTICK_POWER_WIRED : ctx->battery_level);
                        break;
                }
                break;
        }
    }

    /* Ask for battery state again if we're due for an update */
    if (SDL_TICKS_PASSED(SDL_GetTicks(), ctx->last_battery_query_time + BATTERY_POLL_INTERVAL_MS)) {
        ctx->last_battery_query_time = SDL_GetTicks();
        HIDAPI_DriverShield_SendCommand(device, CMD_BATTERY_STATE, NULL, 0);
    }

    /* Retransmit rumble packets if they've lasted longer than the hardware supports */
    if ((ctx->left_motor_amplitude != 0 || ctx->right_motor_amplitude != 0) &&
        SDL_TICKS_PASSED(SDL_GetTicks(), ctx->last_rumble_time + RUMBLE_REFRESH_INTERVAL_MS)) {
        ctx->rumble_update_pending = SDL_TRUE;
        HIDAPI_DriverShield_SendNextRumble(device);
    }

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, joystick->instance_id);
    }
    return (size >= 0);
}

static void
HIDAPI_DriverShield_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_LockMutex(device->dev_lock);
    {
        if (device->dev) {
            SDL_hid_close(device->dev);
            device->dev = NULL;
        }

        SDL_free(device->context);
        device->context = NULL;
    }
    SDL_UnlockMutex(device->dev_lock);
}

static void
HIDAPI_DriverShield_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverShield =
{
    SDL_HINT_JOYSTICK_HIDAPI_SHIELD,
    SDL_TRUE,
    SDL_TRUE,
    HIDAPI_DriverShield_IsSupportedDevice,
    HIDAPI_DriverShield_GetDeviceName,
    HIDAPI_DriverShield_InitDevice,
    HIDAPI_DriverShield_GetDevicePlayerIndex,
    HIDAPI_DriverShield_SetDevicePlayerIndex,
    HIDAPI_DriverShield_UpdateDevice,
    HIDAPI_DriverShield_OpenJoystick,
    HIDAPI_DriverShield_RumbleJoystick,
    HIDAPI_DriverShield_RumbleJoystickTriggers,
    HIDAPI_DriverShield_GetJoystickCapabilities,
    HIDAPI_DriverShield_SetJoystickLED,
    HIDAPI_DriverShield_SendJoystickEffect,
    HIDAPI_DriverShield_SetJoystickSensorsEnabled,
    HIDAPI_DriverShield_CloseJoystick,
    HIDAPI_DriverShield_FreeDevice,
};

#endif /* SDL_JOYSTICK_HIDAPI_SHIELD */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
