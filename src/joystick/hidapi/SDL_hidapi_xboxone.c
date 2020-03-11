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
#include "SDL_hidapi_rumble.h"


#ifdef SDL_JOYSTICK_HIDAPI_XBOXONE

/* Define this if you want to log all packets from the controller */
/*#define DEBUG_XBOX_PROTOCOL*/

/* The amount of time to wait after hotplug to send controller init sequence */
#define CONTROLLER_INIT_DELAY_MS    1500 /* 475 for Xbox One S, 1275 for the PDP Battlefield 1 */

/* Connect controller */
static const Uint8 xboxone_init0[] = {
    0x04, 0x20, 0x00, 0x00
};
/* Initial ack */
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

typedef enum {
    XBOX_ONE_WIRELESS_PROTOCOL_UNKNOWN,
    XBOX_ONE_WIRELESS_PROTOCOL_V1,
    XBOX_ONE_WIRELESS_PROTOCOL_V2,
} SDL_XboxOneWirelessProtocol;

typedef struct {
    Uint16 vendor_id;
    Uint16 product_id;
    SDL_bool bluetooth;
    SDL_XboxOneWirelessProtocol wireless_protocol;
    SDL_bool initialized;
    Uint32 start_time;
    Uint8 sequence;
    Uint8 last_state[USB_PACKET_LENGTH];
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
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        if (product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_1 ||
            product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

/* Return true if this controller sends the 0x02 "waiting for init" packet */
static SDL_bool
ControllerSendsWaitingForInit(Uint16 vendor_id, Uint16 product_id)
{
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
SendControllerInit(SDL_HIDAPI_Device *device, SDL_DriverXboxOne_Context *ctx)
{
    Uint16 vendor_id = ctx->vendor_id;
    Uint16 product_id = ctx->product_id;
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
        if (hid_write(device->dev, init_packet, packet->size) != packet->size) {
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

                while ((size = hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
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
    return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverXboxOne_IsSupportedDevice(const char *name, SDL_GameControllerType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
#ifdef __LINUX__
    if (vendor_id == USB_VENDOR_POWERA && product_id == 0x541a) {
        /* The PowerA Mini controller, model 1240245-01, blocks while writing feature reports */
        return SDL_FALSE;
    }
#endif
#ifdef __MACOSX__
    /* Wired Xbox One controllers are handled by the 360Controller driver */
    if (!IsBluetoothXboxOneController(vendor_id, product_id)) {
        return SDL_FALSE;
    }
#endif
    return (type == SDL_CONTROLLER_TYPE_XBOXONE);
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
    ctx->bluetooth = IsBluetoothXboxOneController(device->vendor_id, device->product_id);
    ctx->initialized = ctx->bluetooth ? SDL_TRUE : SDL_FALSE;
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
HIDAPI_DriverXboxOne_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    if (ctx->bluetooth) {
        Uint8 rumble_packet[] = { 0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00 };

        rumble_packet[4] = (low_frequency_rumble >> 8);
        rumble_packet[5] = (high_frequency_rumble >> 8);

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    } else {
        Uint8 rumble_packet[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF };

        /* Magnitude is 1..100 so scale the 16-bit input here */
        rumble_packet[8] = low_frequency_rumble / 655;
        rumble_packet[9] = high_frequency_rumble / 655;

        if (SDL_HIDAPI_SendRumble(device, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
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

    /* Xbox One S report is 18 bytes
       Xbox One Elite Series 1 report is 33 bytes, paddles in data[32], mode in data[32] & 0x10, both modes have mapped paddles by default
        Paddle bits:
            UL: 0x01 (A)    UR: 0x02 (B)
            LL: 0x04 (X)    LR: 0x08 (Y)
       Xbox One Elite Series 2 report is 38 bytes, paddles in data[18], mode in data[19], mode 0 has no mapped paddles by default
        Paddle bits:
            UL: 0x04 (A)    UR: 0x01 (B)
            LL: 0x08 (X)    LR: 0x02 (Y)
    */
    if (ctx->has_paddles && (size == 33 || size == 38)) {
        int paddle_index;
        int button1_bit;
        int button2_bit;
        int button3_bit;
        int button4_bit;
        SDL_bool paddles_mapped;

        if (size == 33) {
            /* XBox One Elite Series 1 */
            paddle_index = 32;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;

            /* The mapped controller state is at offset 4, the raw state is at offset 18, compare them to see if the paddles are mapped */
            paddles_mapped = (SDL_memcmp(&data[4], &data[18], 14) != 0);

        } else /* if (size == 38) */ {
            /* XBox One Elite Series 2 */
            paddle_index = 18;
            button1_bit = 0x04;
            button2_bit = 0x01;
            button3_bit = 0x08;
            button4_bit = 0x02;
            paddles_mapped = (data[19] != 0);
        }
#ifdef DEBUG_XBOX_PROTOCOL
        SDL_Log(">>> Paddles: %d,%d,%d,%d mapped = %s\n",
            (data[paddle_index] & button1_bit) ? 1 : 0,
            (data[paddle_index] & button2_bit) ? 1 : 0,
            (data[paddle_index] & button3_bit) ? 1 : 0,
            (data[paddle_index] & button4_bit) ? 1 : 0,
            paddles_mapped ? "TRUE" : "FALSE"
        );
#endif

        if (paddles_mapped) {
            /* Respect that the paddles are being used for other controls and don't pass them on to the app */
            data[paddle_index] = 0;
        }

        if (ctx->last_state[paddle_index] != data[paddle_index]) {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+0, (data[paddle_index] & button1_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+1, (data[paddle_index] & button2_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+2, (data[paddle_index] & button3_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_MAX+3, (data[paddle_index] & button4_bit) ? SDL_PRESSED : SDL_RELEASED);
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

static void
HIDAPI_DriverXboxOneBluetooth_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;

    if (ctx->last_state[14] != data[14]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[14] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[14] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[14] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[14] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[14] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[14] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[15] != data[15]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[15] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[15] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[15] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        if (ctx->wireless_protocol == XBOX_ONE_WIRELESS_PROTOCOL_UNKNOWN)
        {
            if (data[15] & 0x10) {
                ctx->wireless_protocol = XBOX_ONE_WIRELESS_PROTOCOL_V2;
            }
        }
        if (ctx->wireless_protocol == XBOX_ONE_WIRELESS_PROTOCOL_V2)
        {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[15] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        }
    }

    if (ctx->last_state[16] != data[16]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[16] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[13] != data[13]) {
        SDL_bool dpad_up = SDL_FALSE;
        SDL_bool dpad_down = SDL_FALSE;
        SDL_bool dpad_left = SDL_FALSE;
        SDL_bool dpad_right = SDL_FALSE;

        switch (data[13]) {
        case 1:
            dpad_up = SDL_TRUE;
            break;
        case 2:
            dpad_up = SDL_TRUE;
            dpad_right = SDL_TRUE;
            break;
        case 3:
            dpad_right = SDL_TRUE;
            break;
        case 4:
            dpad_right = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 5:
            dpad_down = SDL_TRUE;
            break;
        case 6:
            dpad_left = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 7:
            dpad_left = SDL_TRUE;
            break;
        case 8:
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

    axis = (int)*(Uint16*)(&data[1]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = (int)*(Uint16*)(&data[3]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = (int)*(Uint16*)(&data[5]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = (int)*(Uint16*)(&data[7]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    axis = ((int)*(Sint16*)(&data[9]) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);

    axis = ((int)*(Sint16*)(&data[11]) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static void
HIDAPI_DriverXboxOneBluetooth_HandleGuidePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    ctx->wireless_protocol = XBOX_ONE_WIRELESS_PROTOCOL_V1;
    SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[1] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
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
            if (!SendControllerInit(device, ctx)) {
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
        if (ctx->bluetooth) {
            switch (data[0]) {
            case 0x01:
                HIDAPI_DriverXboxOneBluetooth_HandleStatePacket(joystick, device->dev, ctx, data, size);
                break;
            case 0x02:
                HIDAPI_DriverXboxOneBluetooth_HandleGuidePacket(joystick, device->dev, ctx, data, size);
                break;
            default:
#ifdef DEBUG_JOYSTICK
                SDL_Log("Unknown Xbox One packet: 0x%.2x\n", data[0]);
#endif
                break;
            }
        } else {
            switch (data[0]) {
            case 0x02:
                /* Controller is connected and waiting for initialization */
                if (!ctx->initialized) {
#ifdef DEBUG_XBOX_PROTOCOL
                    SDL_Log("Delay after init: %ums\n", SDL_GetTicks() - ctx->start_time);
#endif
                    if (!SendControllerInit(device, ctx)) {
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
