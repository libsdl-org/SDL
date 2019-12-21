/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

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

#define USB_PACKET_LENGTH   64

/* The amount of time to wait after hotplug to send controller init sequence */
#define CONTROLLER_INIT_DELAY_MS    100

/* This is the full init sequence for the Xbox One Elite Series 2 controller.
   Normally it isn't needed, but this switches the controller back to wired report mode after being in Bluetooth mode.
*/
static const Uint8 xboxone_elite_init0[] = {
    0x04, 0x20, 0x01, 0x00
};
static const Uint8 xboxone_elite_init1[] = {
    0x01, 0x20, 0x28, 0x09, 0x00, 0x04, 0x20, 0x3A,
    0x00, 0x00, 0x00, 0x31, 0x01
};
static const Uint8 xboxone_elite_init2[] = {
    0x01, 0x20, 0x28, 0x09, 0x00, 0x04, 0x20, 0x6B,
    0x01, 0x00, 0x00, 0x00, 0x00
};
static const Uint8 xboxone_elite_init3[] = {
    0x05, 0x20, 0x02, 0x0F, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x55, 0x53, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};
static const Uint8 xboxone_elite_init4[] = {
    0x05, 0x20, 0x03, 0x01, 0x00
};
static const Uint8 xboxone_elite_init5[] = {
    0x0A, 0x20, 0x04, 0x03, 0x00, 0x01, 0x14
};

/*
 * This packet is required for all Xbox One pads with 2015
 * or later firmware installed (or present from the factory).
 */
static const Uint8 xboxone_fw2015_init[] = {
    0x05, 0x20, 0x00, 0x01, 0x00
};

/*
 * This packet turns on the LED on some controllers, including PowerA
 */
static const Uint8 xboxone_led_enable[] = {
    0x0A, 0x20, 0x04, 0x03, 0x00, 0x01, 0x14
};

/*
 * This packet is required for the Titanfall 2 Xbox One pads
 * (0x0e6f:0x0165) to finish initialization and for Hori pads
 * (0x0f0d:0x0067) to make the analog sticks work.
 */
static const Uint8 xboxone_hori_init[] = {
    0x01, 0x20, 0x00, 0x09, 0x00, 0x04, 0x20, 0x3a,
    0x00, 0x00, 0x00, 0x80, 0x00
};

/*
 * This packet is required for some of the PDP pads to start
 * sending input reports. These pads include: (0x0e6f:0x02ab),
 * (0x0e6f:0x02a4).
 */
static const Uint8 xboxone_pdp_init1[] = {
    0x0a, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14
};

/*
 * This packet is required for some of the PDP pads to start
 * sending input reports. These pads include: (0x0e6f:0x02ab),
 * (0x0e6f:0x02a4).
 */
static const Uint8 xboxone_pdp_init2[] = {
    0x06, 0x20, 0x00, 0x02, 0x01, 0x00
};

/*
 * A specific rumble packet is required for some PowerA pads to start
 * sending input reports. One of those pads is (0x24c6:0x543a).
 */
static const Uint8 xboxone_rumblebegin_init[] = {
    0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
    0x1D, 0x1D, 0xFF, 0x00, 0x00
};

/*
 * A rumble packet with zero FF intensity will immediately
 * terminate the rumbling required to init PowerA pads.
 * This should happen fast enough that the motors don't
 * spin up to enough speed to actually vibrate the gamepad.
 */
static const Uint8 xboxone_rumbleend_init[] = {
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
    const Uint8 *data;
    int size;
} SDL_DriverXboxOne_InitPacket;


static const SDL_DriverXboxOne_InitPacket xboxone_init_packets[] = {
    { 0x0e6f, 0x0165, xboxone_hori_init, sizeof(xboxone_hori_init) },
    { 0x0f0d, 0x0067, xboxone_hori_init, sizeof(xboxone_hori_init) },
    { 0x045e, 0x0b00, xboxone_elite_init0, sizeof(xboxone_elite_init0) },
    { 0x045e, 0x0b00, xboxone_elite_init1, sizeof(xboxone_elite_init1) },
    { 0x045e, 0x0b00, xboxone_elite_init2, sizeof(xboxone_elite_init2) },
    { 0x045e, 0x0b00, xboxone_elite_init3, sizeof(xboxone_elite_init3) },
    { 0x045e, 0x0b00, xboxone_elite_init4, sizeof(xboxone_elite_init4) },
    { 0x045e, 0x0b00, xboxone_elite_init5, sizeof(xboxone_elite_init5) },
    { 0x0000, 0x0000, xboxone_fw2015_init, sizeof(xboxone_fw2015_init) },
    { 0x0000, 0x0000, xboxone_led_enable, sizeof(xboxone_led_enable) },
    { 0x0e6f, 0x0000, xboxone_pdp_init1, sizeof(xboxone_pdp_init1) },
    { 0x0e6f, 0x0000, xboxone_pdp_init2, sizeof(xboxone_pdp_init2) },
    { 0x24c6, 0x0000, xboxone_rumblebegin_init, sizeof(xboxone_rumblebegin_init) },
    { 0x24c6, 0x0000, xboxone_rumbleend_init, sizeof(xboxone_rumbleend_init) },
};

typedef struct {
    Uint16 vendor_id;
    Uint16 product_id;
    Uint32 start_time;
    SDL_bool initialized;
    Uint8 sequence;
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint32 rumble_expiration;
} SDL_DriverXboxOne_Context;


static SDL_bool
IsBluetoothXboxOneController(Uint16 vendor_id, Uint16 product_id)
{
    /* Check to see if it's the Xbox One S or Xbox One Elite Series 2 in Bluetooth mode */
    const Uint16 USB_VENDOR_MICROSOFT = 0x045e;
    const Uint16 USB_PRODUCT_XBOX_ONE_S_REV1 = 0x02e0;
    const Uint16 USB_PRODUCT_XBOX_ONE_S_REV2 = 0x02fd;
    const Uint16 USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2 = 0x0b05;

    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_S_REV1 ||
            product_id == USB_PRODUCT_XBOX_ONE_S_REV2 ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_bool
SendControllerInit(hid_device *dev, SDL_DriverXboxOne_Context *ctx)
{
    Uint16 vendor_id = ctx->vendor_id;
    Uint16 product_id = ctx->product_id;

    if (!IsBluetoothXboxOneController(vendor_id, product_id)) {
        int i, j;
        Uint8 init_packet[USB_PACKET_LENGTH];

        for (i = 0; i < SDL_arraysize(xboxone_init_packets); ++i) {
            const SDL_DriverXboxOne_InitPacket *packet = &xboxone_init_packets[i];

            if (packet->vendor_id && (vendor_id != packet->vendor_id)) {
                continue;
            }

            if (packet->product_id && (product_id != packet->product_id)) {
                continue;
            }

            SDL_memcpy(init_packet, packet->data, packet->size);
            init_packet[2] = ctx->sequence++;
            if (hid_write(dev, init_packet, packet->size) != packet->size) {
                SDL_SetError("Couldn't write Xbox One initialization packet");
                return SDL_FALSE;
            }

            /* After the init we need to sync up the rumble sequence */
            if (packet->data == xboxone_fw2015_init) {
                for (j = 0; j < 255; ++j) {
                    if (hid_write(dev, xboxone_rumbleend_init, sizeof(xboxone_rumbleend_init)) != sizeof(xboxone_rumbleend_init)) {
                        SDL_SetError("Couldn't write Xbox One initialization packet");
                        return SDL_FALSE;
                    }
                }
            }
        }
    }
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

    /* Initialize the joystick capabilities */
    joystick->nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverXboxOne_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;
    Uint8 rumble_packet[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF };

    if (!ctx->initialized) {
        return 0;
    }

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

    if (!ctx->initialized) {
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
        SDL_Log("Xbox One packet: size = %d\n"
                "                 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n"
                "                 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n"
                "                 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n",
                    size,
                    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15],
                    data[16], data[17], data[18], data[19]);
#endif
        switch (data[0]) {
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
