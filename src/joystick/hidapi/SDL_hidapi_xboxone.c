/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_log.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"


#ifdef SDL_JOYSTICK_HIDAPI_XBOXONE

/* Define this if you want to log all packets from the controller */
/*#define DEBUG_XBOX_PROTOCOL*/

#define USB_PACKET_LENGTH   64

/* The amount of time to wait after hotplug to send controller init sequence */
#define CONTROLLER_INIT_DELAY_MS    1500 /* 475 for Xbox One S, 1275 for the PDP Battlefield 1 */

/* Connect controller */
static const Uint8 xboxone_init0[] = {
    0x04, 0x20, 0x00, 0x00
};
/* Finish initialization? */
static const Uint8 xboxone_init1[] = {
    0x01, 0x20, 0x01, 0x09, 0x00, 0x04, 0x20, 0x3a,
    0x00, 0x00, 0x00, 0x80, 0x00
};
/* Start controller - extended? */
static const Uint8 xboxone_init2[] = {
    0x05, 0x20, 0x00, 0x0F, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x55, 0x53, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};
/* Start controller with input */
static const Uint8 xboxone_init3[] = {
    0x05, 0x20, 0x03, 0x01, 0x00
};
/* Enable LED */
static const Uint8 xboxone_init4[] = {
    0x0A, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14
};
/* Start input reports? */
static const Uint8 xboxone_init5[] = {
    0x06, 0x20, 0x00, 0x02, 0x01, 0x00
};
/* Start rumble? */
static const Uint8 xboxone_init6[] = {
    0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xEB
};

static const Uint8 xboxone_rumble_reset[] = {
    0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * This specifies the selection of init packets that a gamepad
 * will be sent on init *and* the order in which they will be
 * sent. The correct sequence number will be added when the
 * packet is going to be sent.
 */
typedef struct {
    Uint16 vendor_id;
    Uint16 product_id;
    Uint16 exclude_vendor_id;
    Uint16 exclude_product_id;
    const Uint8 *data;
    int size;
    const Uint8 response[2];
} SDL_DriverXboxOne_InitPacket;


static const SDL_DriverXboxOne_InitPacket xboxone_init_packets[] = {
    { 0x0000, 0x0000, 0x0000, 0x0000, xboxone_init0, sizeof(xboxone_init0), { 0x04, 0xf0 } },
    { 0x0000, 0x0000, 0x0000, 0x0000, xboxone_init1, sizeof(xboxone_init1), { 0x04, 0xb0 } },
    { 0x0000, 0x0000, 0x0000, 0x0000, xboxone_init2, sizeof(xboxone_init2), { 0x00, 0x00 } },
    { 0x0000, 0x0000, 0x0000, 0x0000, xboxone_init3, sizeof(xboxone_init3), { 0x00, 0x00 } },
    { 0x0000, 0x0000, 0x0000, 0x0000, xboxone_init4, sizeof(xboxone_init4), { 0x00, 0x00 } },

    /* These next packets are required for third party controllers (PowerA, PDP, HORI),
       but aren't the correct protocol for Microsoft Xbox controllers.
     */
    { 0x0000, 0x0000, 0x045e, 0x0000, xboxone_init5, sizeof(xboxone_init5), { 0x00, 0x00 } },
    { 0x0000, 0x0000, 0x045e, 0x0000, xboxone_init6, sizeof(xboxone_init6), { 0x00, 0x00 } },
};

typedef struct {
    Uint16 vendor_id;
    Uint16 product_id;
    Uint32 start_time;
    SDL_bool initialized;
    Uint8 sequence;
    Uint8 last_state[USB_PACKET_LENGTH];
    SDL_bool rumble_synchronized;
    Uint32 rumble_expiration;
    SDL_bool has_paddles;
} SDL_DriverXboxOne_Context;


#ifdef DEBUG_XBOX_PROTOCOL
static void
DumpPacket(const char *prefix, Uint8 *data, int size)
{
    int i;
    char buffer[5*USB_PACKET_LENGTH];

    SDL_snprintf(buffer, sizeof(buffer), prefix, size);
    for (i = 0; i < size; ++i) {
        if ((i % 8) == 0) {
            SDL_snprintf(&buffer[SDL_strlen(buffer)], sizeof(buffer) - SDL_strlen(buffer), "\n%.2d:      ", i);
        }
        SDL_snprintf(&buffer[SDL_strlen(buffer)], sizeof(buffer) - SDL_strlen(buffer), " 0x%.2x", data[i]);
    }
    SDL_strlcat(buffer, "\n", sizeof(buffer));
    SDL_Log("%s", buffer);
}
#endif /* DEBUG_XBOX_PROTOCOL */

static SDL_bool
IsBluetoothXboxOneController(Uint16 vendor_id, Uint16 product_id)
{
    /* Check to see if it's the Xbox One S or Xbox One Elite Series 2 in Bluetooth mode */
    const Uint16 USB_VENDOR_MICROSOFT = 0x045e;
    const Uint16 USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH = 0x02e0;
    const Uint16 USB_PRODUCT_XBOX_ONE_S_REV2_BLUETOOTH = 0x02fd;
    const Uint16 USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH = 0x0b05;

    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV2_BLUETOOTH ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_bool
ControllerHasPaddles(Uint16 vendor_id, Uint16 product_id)
{
    const Uint16 USB_VENDOR_MICROSOFT = 0x045e;
    const Uint16 USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2 = 0x0b00;

    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2) {
            return SDL_TRUE;
        }
        /* The original Elite controller probably works here, but I don't have one to test... */
    }
    return SDL_FALSE;
}

static SDL_bool
ControllerNeedsRumbleSequenceSynchronized(Uint16 vendor_id, Uint16 product_id)
{
    const Uint16 USB_VENDOR_MICROSOFT = 0x045e;

    if (vendor_id == USB_VENDOR_MICROSOFT) {
        /* All Xbox One controllers, from model 1537 through Elite Series 2, appear to need this */
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static SDL_bool
SynchronizeRumbleSequence(hid_device *dev, SDL_DriverXboxOne_Context *ctx)
{
    Uint16 vendor_id = ctx->vendor_id;
    Uint16 product_id = ctx->product_id;

    if (ctx->rumble_synchronized) {
        return SDL_TRUE;
    }

    if (ControllerNeedsRumbleSequenceSynchronized(vendor_id, product_id)) {
        int i;
        Uint8 init_packet[USB_PACKET_LENGTH];

        SDL_memcpy(init_packet, xboxone_rumble_reset, sizeof(xboxone_rumble_reset));
        for (i = 0; i < 255; ++i) {
            init_packet[2] = ((ctx->sequence + i) % 255);
            if (hid_write(dev, init_packet, sizeof(xboxone_rumble_reset)) != sizeof(xboxone_rumble_reset)) {
                SDL_SetError("Couldn't write Xbox One initialization packet");
                return SDL_FALSE;
            }
        }
    }
    ctx->rumble_synchronized = SDL_TRUE;

    return SDL_TRUE;
}

/* Return true if this controller sends the 0x02 "waiting for init" packet */
static SDL_bool
ControllerSendsWaitingForInit(Uint16 vendor_id, Uint16 product_id)
{
    const Uint16 USB_VENDOR_HYPERKIN = 0x2e24;
    const Uint16 USB_VENDOR_PDP = 0x0e6f;

    if (vendor_id == USB_VENDOR_HYPERKIN) {
        /* The Hyperkin controllers always send 0x02 when waiting for init,
           and the Hyperkin Duke plays an Xbox startup animation, so we want
           to make sure we don't send the init sequence if it isn't needed.
        */
        return SDL_TRUE;
    }
    if (vendor_id == USB_VENDOR_PDP) {
        /* The PDP Rock Candy (PID 0x0246) doesn't send 0x02 on Linux for some reason */
        return SDL_FALSE;
    }

    /* It doesn't hurt to reinit, especially if a driver has misconfigured the controller */
    /*return SDL_TRUE;*/
    return SDL_FALSE;
}

static SDL_bool
SendControllerInit(hid_device *dev, SDL_DriverXboxOne_Context *ctx)
{
    Uint16 vendor_id = ctx->vendor_id;
    Uint16 product_id = ctx->product_id;

    if (!IsBluetoothXboxOneController(vendor_id, product_id)) {
        int i;
        Uint8 init_packet[USB_PACKET_LENGTH];

        for (i = 0; i < SDL_arraysize(xboxone_init_packets); ++i) {
            const SDL_DriverXboxOne_InitPacket *packet = &xboxone_init_packets[i];

            if (packet->vendor_id && (vendor_id != packet->vendor_id)) {
                continue;
            }

            if (packet->product_id && (product_id != packet->product_id)) {
                continue;
            }

            if (packet->exclude_vendor_id && (vendor_id == packet->exclude_vendor_id)) {
                continue;
            }

            if (packet->exclude_product_id && (product_id == packet->exclude_product_id)) {
                continue;
            }

            SDL_memcpy(init_packet, packet->data, packet->size);
            if (init_packet[0] != 0x01) {
                init_packet[2] = ctx->sequence++;
            }
            if (hid_write(dev, init_packet, packet->size) != packet->size) {
                SDL_SetError("Couldn't write Xbox One initialization packet");
                return SDL_FALSE;
            }

            if (packet->response[0]) {
                const Uint32 RESPONSE_TIMEOUT_MS = 50;
                Uint32 start = SDL_GetTicks();
                SDL_bool got_response = SDL_FALSE;

                while (!got_response && !SDL_TICKS_PASSED(SDL_GetTicks(), start + RESPONSE_TIMEOUT_MS)) {
                    Uint8 data[USB_PACKET_LENGTH];
                    int size;

                    while ((size = hid_read_timeout(dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_XBOX_PROTOCOL
                        DumpPacket("Xbox One INIT packet: size = %d", data, size);
#endif
                        if (size >= 2 && data[0] == packet->response[0] && data[1] == packet->response[1]) {
                            got_response = SDL_TRUE;
                        }
                    }
                }
#ifdef DEBUG_XBOX_PROTOCOL
                SDL_Log("Init sequence %d got response: %s\n", i, got_response ? "TRUE" : "FALSE");
#endif
            }
        }
    }

    SynchronizeRumbleSequence(dev, ctx);

    return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverXboxOne_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, const char *name)
{
#ifdef __LINUX__
    if (IsBluetoothXboxOneController(vendor_id, product_id)) {
        /* We can't do rumble on this device, hid_write() fails, so don't try to open it here */
        return SDL_FALSE;
    }
    if (vendor_id == 0x24c6 && product_id == 0x541a) {
        /* The PowerA Mini controller, model 1240245-01, blocks while writing feature reports */
        return SDL_FALSE;
    }
#endif
    return (SDL_GetJoystickGameControllerType(vendor_id, product_id, name) == SDL_CONTROLLER_TYPE_XBOXONE);
}

static const char *
HIDAPI_DriverXboxOne_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    return NULL;
}

static SDL_bool
HIDAPI_DriverXboxOne_InitDevice(SDL_HIDAPI_Device *device)
{
    return HIDAPI_JoystickConnected(device, NULL);
}

static int
HIDAPI_DriverXboxOne_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void
HIDAPI_DriverXboxOne_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static SDL_bool
HIDAPI_DriverXboxOne_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXboxOne_Context *ctx;

    ctx = (SDL_DriverXboxOne_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }

    device->dev = hid_open_path(device->path, 0);
    if (!device->dev) {
        SDL_free(ctx);
        SDL_SetError("Couldn't open %s", device->path);
        return SDL_FALSE;
    }
    device->context = ctx;

    ctx->vendor_id = device->vendor_id;
    ctx->product_id = device->product_id;
    ctx->start_time = SDL_GetTicks();
    ctx->sequence = 1;
    ctx->has_paddles = ControllerHasPaddles(ctx->vendor_id, ctx->product_id);

    /* Initialize the joystick capabilities */
    joystick->nbuttons = ctx->has_paddles ? SDL_CONTROLLER_BUTTON_MAX : (SDL_CONTROLLER_BUTTON_MAX + 4);
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverXboxOne_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;
    Uint8 rumble_packet[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF };

    SynchronizeRumbleSequence(device->dev, ctx);

    /* Magnitude is 1..100 so scale the 16-bit input here */
    rumble_packet[2] = ctx->sequence++;
    rumble_packet[8] = low_frequency_rumble / 655;
    rumble_packet[9] = high_frequency_rumble / 655;

    if (hid_write(device->dev, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }

    if ((low_frequency_rumble || high_frequency_rumble) && duration_ms) {
        ctx->rumble_expiration = SDL_GetTicks() + SDL_min(duration_ms, SDL_MAX_RUMBLE_DURATION_MS);
        if (!ctx->rumble_expiration) {
            ctx->rumble_expiration = 1;
        }
    } else {
        ctx->rumble_expiration = 0;
    }
    return 0;
}

static void
HIDAPI_DriverXboxOne_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;

    if (ctx->last_state[4] != data[4]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[4] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[4] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[4] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[4] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[4] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[4] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[5] != data[5]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, (data[5] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, (data[5] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, (data[5] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (data[5] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[5] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[5] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[5] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[5] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->has_paddles && size >= 20) {
        /* data[19] is the paddle remapping mode, 0 = no remapping */
        if (data[19] != 0) {
            /* Respect that the paddles are being used for other controls and don't pass them on to the app */
            data[18] = 0;
        }
        if (ctx->last_state[18] != data[18]) {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+0, (data[18] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+1, (data[18] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+2, (data[18] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+3, (data[18] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        }
    }

    axis = ((int)*(Sint16*)(&data[6]) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)*(Sint16*)(&data[8]) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = *(Sint16*)(&data[10]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = *(Sint16*)(&data[12]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, ~axis);
    axis = *(Sint16*)(&data[14]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = *(Sint16*)(&data[16]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, ~axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static void
HIDAPI_DriverXboxOne_HandleModePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    if (data[1] == 0x30) {
        /* The Xbox One S controller needs acks for mode reports */
        const Uint8 seqnum = data[2];
        const Uint8 ack[] = { 0x01, 0x20, seqnum, 0x09, 0x00, 0x07, 0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
        hid_write(dev, ack, sizeof(ack));
    }

    SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[4] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
}

static SDL_bool
HIDAPI_DriverXboxOne_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_JoystickFromInstanceID(device->joysticks[0]);
    }
    if (!joystick) {
        return SDL_FALSE;
    }

    if (!ctx->initialized &&
        !ControllerSendsWaitingForInit(device->vendor_id, device->product_id)) {
        if (SDL_TICKS_PASSED(SDL_GetTicks(), ctx->start_time + CONTROLLER_INIT_DELAY_MS)) {
            if (!SendControllerInit(device->dev, ctx)) {
                HIDAPI_JoystickDisconnected(device, joystick->instance_id);
                return SDL_FALSE;
            }
            ctx->initialized = SDL_TRUE;
        }
    }

    while ((size = hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_XBOX_PROTOCOL
        DumpPacket("Xbox One packet: size = %d", data, size);
#endif
        switch (data[0]) {
        case 0x02:
            /* Controller is connected and waiting for initialization */
            if (!ctx->initialized) {
#ifdef DEBUG_XBOX_PROTOCOL
                SDL_Log("Delay after init: %ums\n", SDL_GetTicks() - ctx->start_time);
#endif
                if (!SendControllerInit(device->dev, ctx)) {
                    HIDAPI_JoystickDisconnected(device, joystick->instance_id);
                    return SDL_FALSE;
                }
                ctx->initialized = SDL_TRUE;
            }
            break;
        case 0x03:
            /* Controller heartbeat */
            break;
        case 0x20:
            HIDAPI_DriverXboxOne_HandleStatePacket(joystick, device->dev, ctx, data, size);
            break;
        case 0x07:
            HIDAPI_DriverXboxOne_HandleModePacket(joystick, device->dev, ctx, data, size);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown Xbox One packet: 0x%.2x\n", data[0]);
#endif
            break;
        }
    }

    if (ctx->rumble_expiration) {
        Uint32 now = SDL_GetTicks();
        if (SDL_TICKS_PASSED(now, ctx->rumble_expiration)) {
            HIDAPI_DriverXboxOne_RumbleJoystick(device, joystick, 0, 0, 0);
        }
    }

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, joystick->instance_id);
    }
    return (size >= 0);
}

static void
HIDAPI_DriverXboxOne_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    if (ctx->rumble_expiration) {
        HIDAPI_DriverXboxOne_RumbleJoystick(device, joystick, 0, 0, 0);
    }

    hid_close(device->dev);
    device->dev = NULL;

    SDL_free(device->context);
    device->context = NULL;
}

static void
HIDAPI_DriverXboxOne_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXboxOne =
{
    SDL_HINT_JOYSTICK_HIDAPI_XBOX,
    SDL_TRUE,
    HIDAPI_DriverXboxOne_IsSupportedDevice,
    HIDAPI_DriverXboxOne_GetDeviceName,
    HIDAPI_DriverXboxOne_InitDevice,
    HIDAPI_DriverXboxOne_GetDevicePlayerIndex,
    HIDAPI_DriverXboxOne_SetDevicePlayerIndex,
    HIDAPI_DriverXboxOne_UpdateDevice,
    HIDAPI_DriverXboxOne_OpenJoystick,
    HIDAPI_DriverXboxOne_RumbleJoystick,
    HIDAPI_DriverXboxOne_CloseJoystick,
    HIDAPI_DriverXboxOne_FreeDevice
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOXONE */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
