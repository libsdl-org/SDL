/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_HIDAPI_XBOXONE

/* Define this if you want verbose logging of the init sequence */
/*#define DEBUG_JOYSTICK*/

/* Define this if you want to log all packets from the controller */
/*#define DEBUG_XBOX_PROTOCOL*/

#if defined(__WIN32__) || defined(__WINGDK__)
#define XBOX_ONE_DRIVER_ACTIVE  1
#else
#define XBOX_ONE_DRIVER_ACTIVE  0
#endif

#define CONTROLLER_IDENTIFY_TIMEOUT_MS      100
#define CONTROLLER_PREPARE_INPUT_TIMEOUT_MS 50

/* Deadzone thresholds */
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    -25058 /* Uint8 30 scaled to Sint16 full range */

/* Power on */
static const Uint8 xbox_init_power_on[] = {
    0x05, 0x20, 0x00, 0x01, 0x00
};
/* Enable LED */
static const Uint8 xbox_init_enable_led[] = {
    0x0A, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14
};
/* This controller passed security check */
static const Uint8 xbox_init_security_passed[] = {
    0x06, 0x20, 0x00, 0x02, 0x01, 0x00
};
/* Some PowerA controllers need to actually start the rumble motors */
static const Uint8 xbox_init_powera_rumble[] = {
    0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
    0x1D, 0x1D, 0xFF, 0x00, 0x00
};
/* Setup rumble (not needed for Microsoft controllers, but it doesn't hurt) */
static const Uint8 xbox_init_rumble[] = {
    0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xEB
};

/*
 * This specifies the selection of init packets that a gamepad
 * will be sent on init *and* the order in which they will be
 * sent. The correct sequence number will be added when the
 * packet is going to be sent.
 */
typedef struct
{
    Uint16 vendor_id;
    Uint16 product_id;
    const Uint8 *data;
    int size;
} SDL_DriverXboxOne_InitPacket;

static const SDL_DriverXboxOne_InitPacket xboxone_init_packets[] = {
    { 0x0000, 0x0000, xbox_init_power_on, sizeof(xbox_init_power_on) },
    { 0x0000, 0x0000, xbox_init_enable_led, sizeof(xbox_init_enable_led) },
    { 0x0000, 0x0000, xbox_init_security_passed, sizeof(xbox_init_security_passed) },
    { 0x24c6, 0x541a, xbox_init_powera_rumble, sizeof(xbox_init_powera_rumble) },
    { 0x24c6, 0x542a, xbox_init_powera_rumble, sizeof(xbox_init_powera_rumble) },
    { 0x24c6, 0x543a, xbox_init_powera_rumble, sizeof(xbox_init_powera_rumble) },
    { 0x0000, 0x0000, xbox_init_rumble, sizeof(xbox_init_rumble) },
};

typedef enum
{
    XBOX_ONE_INIT_STATE_ANNOUNCED,
    XBOX_ONE_INIT_STATE_IDENTIFYING,
    XBOX_ONE_INIT_STATE_STARTUP,
    XBOX_ONE_INIT_STATE_PREPARE_INPUT,
    XBOX_ONE_INIT_STATE_COMPLETE,
} SDL_XboxOneInitState;

typedef enum
{
    XBOX_ONE_RUMBLE_STATE_IDLE,
    XBOX_ONE_RUMBLE_STATE_QUEUED,
    XBOX_ONE_RUMBLE_STATE_BUSY
} SDL_XboxOneRumbleState;

typedef struct
{
    SDL_HIDAPI_Device *device;
    Uint16 vendor_id;
    Uint16 product_id;
    SDL_XboxOneInitState init_state;
    Uint64 start_time;
    Uint8 sequence;
    Uint64 send_time;
    SDL_bool has_guide_packet;
    SDL_bool has_color_led;
    SDL_bool has_paddles;
    SDL_bool has_unmapped_state;
    SDL_bool has_trigger_rumble;
    SDL_bool has_share_button;
    Uint8 last_paddle_state;
    Uint8 low_frequency_rumble;
    Uint8 high_frequency_rumble;
    Uint8 left_trigger_rumble;
    Uint8 right_trigger_rumble;
    SDL_XboxOneRumbleState rumble_state;
    Uint64 rumble_time;
    SDL_bool rumble_pending;
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint8 *chunk_buffer;
    Uint32 chunk_length;
} SDL_DriverXboxOne_Context;

static SDL_bool ControllerHasColorLED(Uint16 vendor_id, Uint16 product_id)
{
    return vendor_id == USB_VENDOR_MICROSOFT && product_id == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2;
}

static SDL_bool ControllerHasPaddles(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_IsJoystickXboxOneElite(vendor_id, product_id);
}

static SDL_bool ControllerHasTriggerRumble(Uint16 vendor_id, Uint16 product_id)
{
    /* All the Microsoft Xbox One controllers have trigger rumble */
    if (vendor_id == USB_VENDOR_MICROSOFT) {
        return SDL_TRUE;
    }

    /* It turns out other controllers a mixed bag as to whether they support
       trigger rumble or not, and when they do it's often a buzz rather than
       the vibration of the Microsoft trigger rumble, so for now just pretend
       that it is not available.
     */
    return SDL_FALSE;
}

static SDL_bool ControllerHasShareButton(Uint16 vendor_id, Uint16 product_id)
{
    return SDL_IsJoystickXboxSeriesX(vendor_id, product_id);
}

static int GetHomeLEDBrightness(const char *hint)
{
    const int MAX_VALUE = 50;
    int value = 20;

    if (hint && *hint) {
        if (SDL_strchr(hint, '.') != NULL) {
            value = (int)(MAX_VALUE * SDL_atof(hint));
        } else if (!SDL_GetStringBoolean(hint, SDL_TRUE)) {
            value = 0;
        }
    }
    return value;
}

static void SetHomeLED(SDL_DriverXboxOne_Context *ctx, int value)
{
    Uint8 led_packet[] = { 0x0A, 0x20, 0x00, 0x03, 0x00, 0x00, 0x00 };

    if (value > 0) {
        led_packet[5] = 0x01;
        led_packet[6] = (Uint8)value;
    }
    SDL_HIDAPI_SendRumble(ctx->device, led_packet, sizeof(led_packet));
}

static void SDLCALL SDL_HomeLEDHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)userdata;

    if (hint && *hint) {
        SetHomeLED(ctx, GetHomeLEDBrightness(hint));
    }
}

static void SetInitState(SDL_DriverXboxOne_Context *ctx, SDL_XboxOneInitState state)
{
#ifdef DEBUG_JOYSTICK
    SDL_Log("Setting init state %d\n", state);
#endif
    ctx->init_state = state;
}

static Uint8 GetNextPacketSequence(SDL_DriverXboxOne_Context *ctx)
{
    ++ctx->sequence;
    if (!ctx->sequence) {
        ctx->sequence = 1;
    }
    return ctx->sequence;
}

static SDL_bool SendProtocolPacket(SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
#ifdef DEBUG_XBOX_PROTOCOL
    HIDAPI_DumpPacket("Xbox One sending packet: size = %d", data, size);
#endif

    ctx->send_time = SDL_GetTicks();

    if (SDL_HIDAPI_LockRumble() != 0) {
        return SDL_FALSE;
    }
    if (SDL_HIDAPI_SendRumbleAndUnlock(ctx->device, data, size) != size) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

#if 0
static SDL_bool SendSerialRequest(SDL_DriverXboxOne_Context *ctx)
{
    Uint8 packet[] = { 0x1E, 0x20, 0x00, 0x01, 0x04 };

    packet[2] = GetNextPacketSequence(ctx);

    /* Request the serial number
     * Sending this should be done only after startup is complete.
     * It will cancel the announce packet if sent before that, and will be
     * ignored if sent during the startup sequence.
     */
    if (!SendProtocolPacket(ctx, packet, sizeof(packet))) {
        SDL_SetError("Couldn't send serial request packet");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}
#endif

static SDL_bool ControllerSendsAnnouncement(Uint16 vendor_id, Uint16 product_id)
{
    if (vendor_id == USB_VENDOR_PDP && product_id == 0x0246) {
        /* The PDP Rock Candy (PID 0x0246) doesn't send the announce packet on Linux for some reason */
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool SendIdentificationRequest(SDL_DriverXboxOne_Context *ctx)
{
    /* Request identification, sent in response to announce packet */
    Uint8 packet[] = {
        0x04, 0x20, 0x00, 0x00
    };

    packet[2] = GetNextPacketSequence(ctx);

    if (!SendProtocolPacket(ctx, packet, sizeof(packet))) {
        SDL_SetError("Couldn't send identification request packet");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool SendControllerStartup(SDL_DriverXboxOne_Context *ctx)
{
    Uint16 vendor_id = ctx->vendor_id;
    Uint16 product_id = ctx->product_id;
    Uint8 init_packet[USB_PACKET_LENGTH];
    size_t i;

    for (i = 0; i < SDL_arraysize(xboxone_init_packets); ++i) {
        const SDL_DriverXboxOne_InitPacket *packet = &xboxone_init_packets[i];

        if (packet->vendor_id && (vendor_id != packet->vendor_id)) {
            continue;
        }

        if (packet->product_id && (product_id != packet->product_id)) {
            continue;
        }

        SDL_memcpy(init_packet, packet->data, packet->size);
        init_packet[2] = GetNextPacketSequence(ctx);

        if (init_packet[0] == 0x0A) {
            /* Get the initial brightness value */
            int brightness = GetHomeLEDBrightness(SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED));
            init_packet[5] = (brightness > 0) ? 0x01 : 0x00;
            init_packet[6] = (Uint8)brightness;
        }

        if (!SendProtocolPacket(ctx, init_packet, packet->size)) {
            SDL_SetError("Couldn't send initialization packet");
            return SDL_FALSE;
        }

        /* Wait to process the rumble packet */
        if (packet->data == xbox_init_powera_rumble) {
            SDL_Delay(10);
        }
    }
    return SDL_TRUE;
}

static void HIDAPI_DriverXboxOne_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX, callback, userdata);
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE, callback, userdata);
}

static void HIDAPI_DriverXboxOne_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX, callback, userdata);
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE, callback, userdata);
}

static SDL_bool HIDAPI_DriverXboxOne_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE,
                              SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_XBOX, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT)));
}

static SDL_bool HIDAPI_DriverXboxOne_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
#ifdef __MACOS__
    /* Wired Xbox One controllers are handled by the 360Controller driver */
    if (!SDL_IsJoystickBluetoothXboxOne(vendor_id, product_id)) {
        return SDL_FALSE;
    }
#endif
    return (type == SDL_GAMEPAD_TYPE_XBOXONE);
}

static SDL_bool HIDAPI_DriverXboxOne_InitDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXboxOne_Context *ctx;

    ctx = (SDL_DriverXboxOne_Context *)SDL_calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    ctx->device = device;

    device->context = ctx;

    ctx->vendor_id = device->vendor_id;
    ctx->product_id = device->product_id;
    ctx->start_time = SDL_GetTicks();
    ctx->sequence = 0;
    ctx->has_color_led = ControllerHasColorLED(ctx->vendor_id, ctx->product_id);
    ctx->has_paddles = ControllerHasPaddles(ctx->vendor_id, ctx->product_id);
    ctx->has_trigger_rumble = ControllerHasTriggerRumble(ctx->vendor_id, ctx->product_id);
    ctx->has_share_button = ControllerHasShareButton(ctx->vendor_id, ctx->product_id);

    /* Assume that the controller is correctly initialized when we start */
    if (!ControllerSendsAnnouncement(device->vendor_id, device->product_id)) {
        /* Jump into the startup sequence for this controller */
        ctx->init_state = XBOX_ONE_INIT_STATE_STARTUP;
    } else {
        ctx->init_state = XBOX_ONE_INIT_STATE_COMPLETE;
    }

#ifdef DEBUG_JOYSTICK
    SDL_Log("Controller version: %d (0x%.4x)\n", device->version, device->version);
#endif

    device->type = SDL_GAMEPAD_TYPE_XBOXONE;

    return HIDAPI_JoystickConnected(device, NULL);
}

static int HIDAPI_DriverXboxOne_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverXboxOne_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static SDL_bool HIDAPI_DriverXboxOne_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    SDL_AssertJoysticksLocked();

    ctx->low_frequency_rumble = 0;
    ctx->high_frequency_rumble = 0;
    ctx->left_trigger_rumble = 0;
    ctx->right_trigger_rumble = 0;
    ctx->rumble_state = XBOX_ONE_RUMBLE_STATE_IDLE;
    ctx->rumble_time = 0;
    ctx->rumble_pending = SDL_FALSE;
    SDL_zeroa(ctx->last_state);

    /* Initialize the joystick capabilities */
    joystick->nbuttons = 15;
    if (ctx->has_share_button) {
        joystick->nbuttons += 1;
    }
    if (ctx->has_paddles) {
        joystick->nbuttons += 4;
    }
    joystick->naxes = SDL_GAMEPAD_AXIS_MAX;

    if (!device->is_bluetooth) {
        joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;
    }

    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED,
                        SDL_HomeLEDHintChanged, ctx);
    return SDL_TRUE;
}

static void HIDAPI_DriverXboxOne_RumbleSent(void *userdata)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)userdata;
    ctx->rumble_time = SDL_GetTicks();
}

static int HIDAPI_DriverXboxOne_UpdateRumble(SDL_DriverXboxOne_Context *ctx)
{
    if (ctx->rumble_state == XBOX_ONE_RUMBLE_STATE_QUEUED) {
        if (ctx->rumble_time) {
            ctx->rumble_state = XBOX_ONE_RUMBLE_STATE_BUSY;
        }
    }

    if (ctx->rumble_state == XBOX_ONE_RUMBLE_STATE_BUSY) {
        const int RUMBLE_BUSY_TIME_MS = ctx->device->is_bluetooth ? 50 : 10;
        if (SDL_GetTicks() >= (ctx->rumble_time + RUMBLE_BUSY_TIME_MS)) {
            ctx->rumble_time = 0;
            ctx->rumble_state = XBOX_ONE_RUMBLE_STATE_IDLE;
        }
    }

    if (!ctx->rumble_pending) {
        return 0;
    }

    if (ctx->rumble_state != XBOX_ONE_RUMBLE_STATE_IDLE) {
        return 0;
    }

    /* We're no longer pending, even if we fail to send the rumble below */
    ctx->rumble_pending = SDL_FALSE;

    if (SDL_HIDAPI_LockRumble() != 0) {
        return -1;
    }

    if (ctx->device->is_bluetooth) {
        Uint8 rumble_packet[] = { 0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEB };

        rumble_packet[2] = ctx->left_trigger_rumble;
        rumble_packet[3] = ctx->right_trigger_rumble;
        rumble_packet[4] = ctx->low_frequency_rumble;
        rumble_packet[5] = ctx->high_frequency_rumble;

        if (SDL_HIDAPI_SendRumbleWithCallbackAndUnlock(ctx->device, rumble_packet, sizeof(rumble_packet), HIDAPI_DriverXboxOne_RumbleSent, ctx) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    } else {
        Uint8 rumble_packet[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEB };

        rumble_packet[6] = ctx->left_trigger_rumble;
        rumble_packet[7] = ctx->right_trigger_rumble;
        rumble_packet[8] = ctx->low_frequency_rumble;
        rumble_packet[9] = ctx->high_frequency_rumble;

        if (SDL_HIDAPI_SendRumbleWithCallbackAndUnlock(ctx->device, rumble_packet, sizeof(rumble_packet), HIDAPI_DriverXboxOne_RumbleSent, ctx) != sizeof(rumble_packet)) {
            return SDL_SetError("Couldn't send rumble packet");
        }
    }

    ctx->rumble_state = XBOX_ONE_RUMBLE_STATE_QUEUED;

    return 0;
}

static int HIDAPI_DriverXboxOne_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    /* Magnitude is 1..100 so scale the 16-bit input here */
    ctx->low_frequency_rumble = (Uint8)(low_frequency_rumble / 655);
    ctx->high_frequency_rumble = (Uint8)(high_frequency_rumble / 655);
    ctx->rumble_pending = SDL_TRUE;

    return HIDAPI_DriverXboxOne_UpdateRumble(ctx);
}

static int HIDAPI_DriverXboxOne_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    if (!ctx->has_trigger_rumble) {
        return SDL_Unsupported();
    }

    /* Magnitude is 1..100 so scale the 16-bit input here */
    ctx->left_trigger_rumble = (Uint8)(left_rumble / 655);
    ctx->right_trigger_rumble = (Uint8)(right_rumble / 655);
    ctx->rumble_pending = SDL_TRUE;

    return HIDAPI_DriverXboxOne_UpdateRumble(ctx);
}

static Uint32 HIDAPI_DriverXboxOne_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;
    Uint32 result = 0;

    result |= SDL_JOYCAP_RUMBLE;
    if (ctx->has_trigger_rumble) {
        result |= SDL_JOYCAP_RUMBLE_TRIGGERS;
    }

    if (ctx->has_color_led) {
        result |= SDL_JOYCAP_LED;
    }

    return result;
}

static int HIDAPI_DriverXboxOne_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    if (ctx->has_color_led) {
        Uint8 led_packet[] = { 0x0E, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };

        led_packet[5] = 0x00; /* Whiteness? Sets white intensity when RGB is 0, seems additive */
        led_packet[6] = red;
        led_packet[7] = green;
        led_packet[8] = blue;

        if (SDL_HIDAPI_SendRumble(device, led_packet, sizeof(led_packet)) != sizeof(led_packet)) {
            return SDL_SetError("Couldn't send LED packet");
        }
        return 0;
    } else {
        return SDL_Unsupported();
    }
}

static int HIDAPI_DriverXboxOne_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static int HIDAPI_DriverXboxOne_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, SDL_bool enabled)
{
    return SDL_Unsupported();
}

/*
 * The Xbox One Elite controller with 5.13+ firmware sends the unmapped state in a separate packet.
 * We can use this to send the paddle state when they aren't mapped
 */
static void HIDAPI_DriverXboxOne_HandleUnmappedStatePacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    Uint8 profile;
    int paddle_index;
    int button1_bit;
    int button2_bit;
    int button3_bit;
    int button4_bit;
    SDL_bool paddles_mapped;
    Uint64 timestamp = SDL_GetTicksNS();

    if (size == 17) {
        /* XBox One Elite Series 2 */
        paddle_index = 14;
        button1_bit = 0x01;
        button2_bit = 0x02;
        button3_bit = 0x04;
        button4_bit = 0x08;
        profile = data[15];

        if (profile == 0) {
            paddles_mapped = SDL_FALSE;
        } else if (SDL_memcmp(&data[0], &ctx->last_state[0], 14) == 0) {
            /* We're using a profile, but paddles aren't mapped */
            paddles_mapped = SDL_FALSE;
        } else {
            /* Something is mapped, we can't use the paddles */
            paddles_mapped = SDL_TRUE;
        }

    } else {
        /* Unknown format */
        return;
    }
#ifdef DEBUG_XBOX_PROTOCOL
    SDL_Log(">>> Paddles: %d,%d,%d,%d mapped = %s\n",
            (data[paddle_index] & button1_bit) ? 1 : 0,
            (data[paddle_index] & button2_bit) ? 1 : 0,
            (data[paddle_index] & button3_bit) ? 1 : 0,
            (data[paddle_index] & button4_bit) ? 1 : 0,
            paddles_mapped ? "TRUE" : "FALSE");
#endif

    if (paddles_mapped) {
        /* Respect that the paddles are being used for other controls and don't pass them on to the app */
        data[paddle_index] = 0;
    }

    if (ctx->last_paddle_state != data[paddle_index]) {
        Uint8 nButton = (Uint8)(SDL_GAMEPAD_BUTTON_MISC1 + ctx->has_share_button); /* Next available button */
        SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button1_bit) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button2_bit) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button3_bit) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button4_bit) ? SDL_PRESSED : SDL_RELEASED);
        ctx->last_paddle_state = data[paddle_index];
    }
    ctx->has_unmapped_state = SDL_TRUE;
}

static void HIDAPI_DriverXboxOne_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    /* Enable paddles on the Xbox Elite controller when connected over USB */
    if (ctx->has_paddles && !ctx->has_unmapped_state && size == 46) {
        Uint8 packet[] = { 0x4d, 0x00, 0x00, 0x02, 0x07, 0x00 };

#ifdef DEBUG_JOYSTICK
        SDL_Log("Enabling paddles on XBox Elite 2\n");
#endif
        SDL_HIDAPI_SendRumble(ctx->device, packet, sizeof(packet));
    }

    if (ctx->last_state[0] != data[0]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (data[0] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, (data[0] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_A, (data[0] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_B, (data[0] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_X, (data[0] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_Y, (data[0] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[1] != data[1]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, (data[1] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, (data[1] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, (data[1] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, (data[1] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        if (ctx->vendor_id == USB_VENDOR_RAZER && ctx->product_id == USB_PRODUCT_RAZER_ATROX) {
            /* The Razer Atrox has the right and left shoulder bits reversed */
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (data[1] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (data[1] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        } else {
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (data[1] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (data[1] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        }
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, (data[1] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (data[1] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->has_share_button) {
        /* Xbox Series X firmware version 5.0, report is 32 bytes, share button is in byte 14
         * Xbox Series X firmware version 5.1, report is 40 bytes, share button is in byte 14
         * Xbox Series X firmware version 5.5, report is 44 bytes, share button is in byte 18
         * Victrix Gambit Tournament Controller, report is 46 bytes, share button is in byte 28
         * ThrustMaster eSwap PRO Controller Xbox, report is 60 bytes, share button is in byte 42
         */
        if (size < 44) {
            if (ctx->last_state[14] != data[14]) {
                SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[14] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            }
        } else if (size == 44) {
            if (ctx->last_state[18] != data[18]) {
                SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[18] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            }
        } else if (size == 46) {
            if (ctx->last_state[28] != data[28]) {
                SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[28] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            }
        } else if (size == 60) {
            if (ctx->last_state[42] != data[42]) {
                SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[42] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
            }
        }
    }

    /* Xbox One S report is 14 bytes
       Xbox One Elite Series 1 report is 29 bytes, paddles in data[28], mode in data[28] & 0x10, both modes have mapped paddles by default
        Paddle bits:
            P3: 0x01 (A)    P1: 0x02 (B)
            P4: 0x04 (X)    P2: 0x08 (Y)
       Xbox One Elite Series 2 4.x firmware report is 34 bytes, paddles in data[14], mode in data[15], mode 0 has no mapped paddles by default
        Paddle bits:
            P3: 0x04 (A)    P1: 0x01 (B)
            P4: 0x08 (X)    P2: 0x02 (Y)
       Xbox One Elite Series 2 5.x firmware report is 46 bytes, paddles in data[18], mode in data[19], mode 0 has no mapped paddles by default
        Paddle bits:
            P3: 0x04 (A)    P1: 0x01 (B)
            P4: 0x08 (X)    P2: 0x02 (Y)
       Xbox One Elite Series 2 5.17+ firmware report is 47 bytes, paddles in data[14], mode in data[20], mode 0 has no mapped paddles by default
        Paddle bits:
            P3: 0x04 (A)    P1: 0x01 (B)
            P4: 0x08 (X)    P2: 0x02 (Y)
    */
    if (ctx->has_paddles && !ctx->has_unmapped_state && (size == 29 || size == 34 || size == 46 || size == 47)) {
        int paddle_index;
        int button1_bit;
        int button2_bit;
        int button3_bit;
        int button4_bit;
        SDL_bool paddles_mapped;

        if (size == 29) {
            /* XBox One Elite Series 1 */
            paddle_index = 28;
            button1_bit = 0x02;
            button2_bit = 0x08;
            button3_bit = 0x01;
            button4_bit = 0x04;

            /* The mapped controller state is at offset 0, the raw state is at offset 14, compare them to see if the paddles are mapped */
            paddles_mapped = (SDL_memcmp(&data[0], &data[14], 2) != 0);

        } else if (size == 34) {
            /* XBox One Elite Series 2 */
            paddle_index = 14;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[15] != 0);

        } else if (size == 46) {
            /* XBox One Elite Series 2 */
            paddle_index = 18;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[19] != 0);
        } else /* if (size == 47) */ {
            /* XBox One Elite Series 2 */
            paddle_index = 14;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[20] != 0);
        }
#ifdef DEBUG_XBOX_PROTOCOL
        SDL_Log(">>> Paddles: %d,%d,%d,%d mapped = %s\n",
                (data[paddle_index] & button1_bit) ? 1 : 0,
                (data[paddle_index] & button2_bit) ? 1 : 0,
                (data[paddle_index] & button3_bit) ? 1 : 0,
                (data[paddle_index] & button4_bit) ? 1 : 0,
                paddles_mapped ? "TRUE" : "FALSE");
#endif

        if (paddles_mapped) {
            /* Respect that the paddles are being used for other controls and don't pass them on to the app */
            data[paddle_index] = 0;
        }

        if (ctx->last_paddle_state != data[paddle_index]) {
            Uint8 nButton = (Uint8)(SDL_GAMEPAD_BUTTON_MISC1 + ctx->has_share_button); /* Next available button */
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button1_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button2_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button3_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button4_bit) ? SDL_PRESSED : SDL_RELEASED);
            ctx->last_paddle_state = data[paddle_index];
        }
    }

    axis = ((int)SDL_SwapLE16(*(Sint16 *)(&data[2])) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    if (axis == -32768 && size == 26 && (data[18] & 0x80)) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);

    axis = ((int)SDL_SwapLE16(*(Sint16 *)(&data[4])) * 64) - 32768;
    if (axis == -32768 && size == 26 && (data[18] & 0x40)) {
        axis = 32767;
    }
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);

    axis = SDL_SwapLE16(*(Sint16 *)(&data[6]));
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
    axis = SDL_SwapLE16(*(Sint16 *)(&data[8]));
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, ~axis);
    axis = SDL_SwapLE16(*(Sint16 *)(&data[10]));
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
    axis = SDL_SwapLE16(*(Sint16 *)(&data[12]));
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, ~axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));

    /* We don't have the unmapped state for this packet */
    ctx->has_unmapped_state = SDL_FALSE;
}

static void HIDAPI_DriverXboxOne_HandleStatusPacket(SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    if (ctx->init_state < XBOX_ONE_INIT_STATE_COMPLETE) {
        SetInitState(ctx, XBOX_ONE_INIT_STATE_COMPLETE);
    }
}

static void HIDAPI_DriverXboxOne_HandleModePacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    Uint64 timestamp = SDL_GetTicksNS();

    SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (data[0] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
}

/*
 * Xbox One S with firmware 3.1.1221 uses a 16 byte packet and the GUIDE button in a separate packet
 */
static void HIDAPI_DriverXboxOneBluetooth_HandleButtons16(Uint64 timestamp, SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    if (ctx->last_state[14] != data[14]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_A, (data[14] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_B, (data[14] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_X, (data[14] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_Y, (data[14] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (data[14] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (data[14] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, (data[14] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (data[14] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[15] != data[15]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, (data[15] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (data[15] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
    }
}

/*
 * Xbox One S with firmware 4.8.1923 uses a 17 byte packet with BACK button in byte 16 and the GUIDE button in a separate packet (on Windows), or in byte 15 (on Linux)
 * Xbox One S with firmware 5.x uses a 17 byte packet with BACK and GUIDE buttons in byte 15
 * Xbox One Elite Series 2 with firmware 4.7.1872 uses a 55 byte packet with BACK button in byte 16, paddles starting at byte 33, and the GUIDE button in a separate packet
 * Xbox One Elite Series 2 with firmware 4.8.1908 uses a 33 byte packet with BACK button in byte 16, paddles starting at byte 17, and the GUIDE button in a separate packet
 * Xbox One Elite Series 2 with firmware 5.11.3112 uses a 19 byte packet with BACK and GUIDE buttons in byte 15
 * Xbox Series X with firmware 5.5.2641 uses a 17 byte packet with BACK and GUIDE buttons in byte 15, and SHARE button in byte 17
 */
static void HIDAPI_DriverXboxOneBluetooth_HandleButtons(Uint64 timestamp, SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    if (ctx->last_state[14] != data[14]) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_A, (data[14] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_B, (data[14] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_X, (data[14] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_Y, (data[14] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, (data[14] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, (data[14] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[15] != data[15]) {
        if (!ctx->has_guide_packet) {
            SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (data[15] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        }
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_START, (data[15] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_LEFT_STICK, (data[15] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_RIGHT_STICK, (data[15] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->has_share_button) {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, (data[15] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_MISC1, (data[16] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
    } else {
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_BACK, ((data[15] & 0x04) || (data[16] & 0x01)) ? SDL_PRESSED : SDL_RELEASED);
    }

    /*
        Paddle bits:
            P3: 0x04 (A)    P1: 0x01 (B)
            P4: 0x08 (X)    P2: 0x02 (Y)
    */
    if (ctx->has_paddles && (size == 20 || size == 39 || size == 55)) {
        int paddle_index;
        int button1_bit;
        int button2_bit;
        int button3_bit;
        int button4_bit;
        SDL_bool paddles_mapped;

        if (size == 55) {
            /* Initial firmware for the Xbox Elite Series 2 controller */
            paddle_index = 33;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[35] != 0);
        } else if (size == 39) {
            /* Updated firmware for the Xbox Elite Series 2 controller */
            paddle_index = 17;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[19] != 0);
        } else /* if (size == 20) */ {
            /* Updated firmware for the Xbox Elite Series 2 controller (5.13+) */
            paddle_index = 19;
            button1_bit = 0x01;
            button2_bit = 0x02;
            button3_bit = 0x04;
            button4_bit = 0x08;
            paddles_mapped = (data[17] != 0);
        }

#ifdef DEBUG_XBOX_PROTOCOL
        SDL_Log(">>> Paddles: %d,%d,%d,%d mapped = %s\n",
                (data[paddle_index] & button1_bit) ? 1 : 0,
                (data[paddle_index] & button2_bit) ? 1 : 0,
                (data[paddle_index] & button3_bit) ? 1 : 0,
                (data[paddle_index] & button4_bit) ? 1 : 0,
                paddles_mapped ? "TRUE" : "FALSE");
#endif

        if (paddles_mapped) {
            /* Respect that the paddles are being used for other controls and don't pass them on to the app */
            data[paddle_index] = 0;
        }

        if (ctx->last_paddle_state != data[paddle_index]) {
            Uint8 nButton = SDL_GAMEPAD_BUTTON_MISC1; /* Next available button */
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button1_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button2_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button3_bit) ? SDL_PRESSED : SDL_RELEASED);
            SDL_SendJoystickButton(timestamp, joystick, nButton++, (data[paddle_index] & button4_bit) ? SDL_PRESSED : SDL_RELEASED);
            ctx->last_paddle_state = data[paddle_index];
        }
    }
}

static void HIDAPI_DriverXboxOneBluetooth_HandleStatePacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
    Uint64 timestamp = SDL_GetTicksNS();

    if (size == 16) {
        /* Original Xbox One S, with separate report for guide button */
        HIDAPI_DriverXboxOneBluetooth_HandleButtons16(timestamp, joystick, ctx, data, size);
    } else if (size > 16) {
        HIDAPI_DriverXboxOneBluetooth_HandleButtons(timestamp, joystick, ctx, data, size);
    } else {
#ifdef DEBUG_XBOX_PROTOCOL
        SDL_Log("Unknown Bluetooth state packet format\n");
#endif
        return;
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
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, dpad_down);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, dpad_up);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, dpad_right);
        SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, dpad_left);
    }

    axis = ((int)SDL_SwapLE16(*(Sint16 *)(&data[9])) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, axis);

    axis = ((int)SDL_SwapLE16(*(Sint16 *)(&data[11])) * 64) - 32768;
    if (axis == 32704) {
        axis = 32767;
    }
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, axis);

    axis = (int)SDL_SwapLE16(*(Uint16 *)(&data[1])) - 0x8000;
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTX, axis);
    axis = (int)SDL_SwapLE16(*(Uint16 *)(&data[3])) - 0x8000;
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_LEFTY, axis);
    axis = (int)SDL_SwapLE16(*(Uint16 *)(&data[5])) - 0x8000;
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTX, axis);
    axis = (int)SDL_SwapLE16(*(Uint16 *)(&data[7])) - 0x8000;
    SDL_SendJoystickAxis(timestamp, joystick, SDL_GAMEPAD_AXIS_RIGHTY, axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static void HIDAPI_DriverXboxOneBluetooth_HandleGuidePacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    Uint64 timestamp = SDL_GetTicksNS();

    ctx->has_guide_packet = SDL_TRUE;
    SDL_SendJoystickButton(timestamp, joystick, SDL_GAMEPAD_BUTTON_GUIDE, (data[1] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
}

static void HIDAPI_DriverXboxOneBluetooth_HandleBatteryPacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    Uint8 flags = data[1];
    SDL_bool on_usb = (((flags & 0x0C) >> 2) == 0);

    if (on_usb) {
        /* Does this ever happen? */
        SDL_SendJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_WIRED);
    } else {
        switch ((flags & 0x03)) {
        case 0:
            SDL_SendJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_LOW);
            break;
        case 1:
            SDL_SendJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_MEDIUM);
            break;
        default: /* 2, 3 */
            SDL_SendJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_FULL);
            break;
        }
    }
}

static void HIDAPI_DriverXboxOne_HandleSerialIDPacket(SDL_DriverXboxOne_Context *ctx, const Uint8 *data, int size)
{
    char serial[29];
    int i;

    for (i = 0; i < 14; ++i) {
        SDL_uitoa(data[2 + i], &serial[i * 2], 16);
    }
    serial[i * 2] = '\0';

#ifdef DEBUG_JOYSTICK
    SDL_Log("Setting serial number to %s\n", serial);
#endif
    HIDAPI_SetDeviceSerial(ctx->device, serial);
}

static SDL_bool HIDAPI_DriverXboxOne_UpdateInitState(SDL_DriverXboxOne_Context *ctx)
{
    SDL_XboxOneInitState prev_state;
    do {
        prev_state = ctx->init_state;

        switch (ctx->init_state) {
        case XBOX_ONE_INIT_STATE_ANNOUNCED:
            if (XBOX_ONE_DRIVER_ACTIVE) {
                /* The driver is taking care of identification */
                SetInitState(ctx, XBOX_ONE_INIT_STATE_COMPLETE);
            } else {
                SendIdentificationRequest(ctx);
                SetInitState(ctx, XBOX_ONE_INIT_STATE_IDENTIFYING);
            }
            break;
        case XBOX_ONE_INIT_STATE_IDENTIFYING:
            if (SDL_GetTicks() >= (ctx->send_time + CONTROLLER_IDENTIFY_TIMEOUT_MS)) {
                /* We haven't heard anything, let's move on */
#ifdef DEBUG_JOYSTICK
                SDL_Log("Identification request timed out after %llu ms\n", (SDL_GetTicks() - ctx->send_time));
#endif
                SetInitState(ctx, XBOX_ONE_INIT_STATE_STARTUP);
            }
            break;
        case XBOX_ONE_INIT_STATE_STARTUP:
            if (XBOX_ONE_DRIVER_ACTIVE) {
                /* The driver is taking care of startup */
                SetInitState(ctx, XBOX_ONE_INIT_STATE_COMPLETE);
            } else {
                SendControllerStartup(ctx);
                SetInitState(ctx, XBOX_ONE_INIT_STATE_PREPARE_INPUT);
            }
            break;
        case XBOX_ONE_INIT_STATE_PREPARE_INPUT:
            if (SDL_GetTicks() >= (ctx->send_time + CONTROLLER_PREPARE_INPUT_TIMEOUT_MS)) {
#ifdef DEBUG_JOYSTICK
                SDL_Log("Prepare input complete after %llu ms\n", (SDL_GetTicks() - ctx->send_time));
#endif
                SetInitState(ctx, XBOX_ONE_INIT_STATE_COMPLETE);
            }
            break;
        case XBOX_ONE_INIT_STATE_COMPLETE:
            break;
        }

    } while (ctx->init_state != prev_state);

    return SDL_TRUE;
}

#define GIP_HEADER_MIN_LENGTH 3

/* Internal commands */
#define GIP_CMD_ACKNOWLEDGE     0x01
#define GIP_CMD_ANNOUNCE        0x02
#define GIP_CMD_STATUS          0x03
#define GIP_CMD_IDENTIFY        0x04
#define GIP_CMD_POWER           0x05
#define GIP_CMD_AUTHENTICATE    0x06
#define GIP_CMD_VIRTUAL_KEY     0x07
#define GIP_CMD_AUDIO_CONTROL   0x08
#define GIP_CMD_LED             0x0A
#define GIP_CMD_HID_REPORT      0x0B
#define GIP_CMD_FIRMWARE        0x0C
#define GIP_CMD_SERIAL_NUMBER   0x1E
#define GIP_CMD_AUDIO_SAMPLES   0x60

/* External commands */
#define GIP_CMD_RUMBLE          0x09
#define GIP_CMD_UNMAPPED_STATE  0x0C
#define GIP_CMD_INPUT           0x20

/* Header option flags */
#define GIP_OPT_ACKNOWLEDGE     0x10
#define GIP_OPT_INTERNAL        0x20
#define GIP_OPT_CHUNK_START     0x40
#define GIP_OPT_CHUNK           0x80

#pragma pack(push, 1)

struct gip_header {
    Uint8 command;
    Uint8 options;
    Uint8 sequence;
    Uint32 packet_length;
    Uint32 chunk_offset;
};

struct gip_pkt_acknowledge {
    Uint8 unknown;
    Uint8 command;
    Uint8 options;
    Uint16 length;
    Uint8 padding[2];
    Uint16 remaining;
};

#pragma pack(pop)

static int EncodeVariableInt(Uint8 *buf, Uint32 val)
{
    int i;

    for (i = 0; i < sizeof(val); i++) {
        buf[i] = (Uint8)val;
        if (val > 0x7F) {
            buf[i] |= 0x80;
        }

        val >>= 7;
        if (!val) {
            break;
        }
    }
    return i + 1;
}

static int DecodeVariableInt(const Uint8 *data, int len, Uint32 *val)
{
    int i;

    for (i = 0; i < sizeof(*val) && i < len; i++) {
        *val |= (data[i] & 0x7F) << (i * 7);

        if (!(data[i] & 0x80)) {
            break;
        }
    }
    return i + 1;
}

static int HIDAPI_GIP_GetActualHeaderLength(struct gip_header *hdr)
{
    Uint32 pkt_len = hdr->packet_length;
    Uint32 chunk_offset = hdr->chunk_offset;
    int len = GIP_HEADER_MIN_LENGTH;

    do {
        len++;
        pkt_len >>= 7;
    } while (pkt_len);

    if (hdr->options & GIP_OPT_CHUNK) {
        while (chunk_offset) {
            len++;
            chunk_offset >>= 7;
        }
    }

    return len;
}

static int HIDAPI_GIP_GetHeaderLength(struct gip_header *hdr)
{
    int len = HIDAPI_GIP_GetActualHeaderLength(hdr);

    /* Header length must be even */
    return len + (len % 2);
}

static void HIDAPI_GIP_EncodeHeader(struct gip_header *hdr, Uint8 *buf)
{
    int hdr_len = 0;

    buf[hdr_len++] = hdr->command;
    buf[hdr_len++] = hdr->options;
    buf[hdr_len++] = hdr->sequence;

    hdr_len += EncodeVariableInt(buf + hdr_len, hdr->packet_length);

    /* Header length must be even */
    if (HIDAPI_GIP_GetActualHeaderLength(hdr) % 2) {
        buf[hdr_len - 1] |= 0x80;
        buf[hdr_len++] = 0;
    }

    if (hdr->options & GIP_OPT_CHUNK) {
        EncodeVariableInt(buf + hdr_len, hdr->chunk_offset);
    }
}

static int HIDAPI_GIP_DecodeHeader(struct gip_header *hdr, const Uint8 *data, int len)
{
    int hdr_len = 0;

    hdr->command = data[hdr_len++];
    hdr->options = data[hdr_len++];
    hdr->sequence = data[hdr_len++];
    hdr->packet_length = 0;
    hdr->chunk_offset = 0;

    hdr_len += DecodeVariableInt(data + hdr_len, len - hdr_len, &hdr->packet_length);

    if (hdr->options & GIP_OPT_CHUNK) {
        hdr_len += DecodeVariableInt(data + hdr_len, len - hdr_len, &hdr->chunk_offset);
    }
    return hdr_len;
}

static SDL_bool HIDAPI_GIP_SendPacket(SDL_DriverXboxOne_Context *ctx, struct gip_header *hdr, const void *data)
{
    Uint8 packet[USB_PACKET_LENGTH];
    int hdr_len, size;

    hdr_len = HIDAPI_GIP_GetHeaderLength(hdr);
    size = (hdr_len + hdr->packet_length);
    if (size > sizeof(packet)) {
        SDL_SetError("Couldn't send GIP packet, size (%d) too large\n", size);
        return SDL_FALSE;
    }

    if (!hdr->sequence) {
        hdr->sequence = GetNextPacketSequence(ctx);
    }

    HIDAPI_GIP_EncodeHeader(hdr, packet);
    if (data) {
        SDL_memcpy(&packet[hdr_len], data, hdr->packet_length);
    }

    if (!SendProtocolPacket(ctx, packet, size)) {
        SDL_SetError("Couldn't send protocol packet");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool HIDAPI_GIP_AcknowledgePacket(SDL_DriverXboxOne_Context *ctx, struct gip_header *ack)
{
    if (XBOX_ONE_DRIVER_ACTIVE) {
        /* The driver is taking care of acks */
        return SDL_TRUE;
    } else {
        struct gip_header hdr;
        struct gip_pkt_acknowledge pkt;

        SDL_zero(hdr);
        hdr.command = GIP_CMD_ACKNOWLEDGE;
        hdr.options = GIP_OPT_INTERNAL;
        hdr.sequence = ack->sequence;
        hdr.packet_length = sizeof(pkt);

        SDL_zero(pkt);
        pkt.command = ack->command;
        pkt.options = GIP_OPT_INTERNAL;
        pkt.length = SDL_SwapLE16((Uint16)(ack->chunk_offset + ack->packet_length));

        if ((ack->options & GIP_OPT_CHUNK) && ctx->chunk_buffer) {
            pkt.remaining = SDL_SwapLE16((Uint16)(ctx->chunk_length - pkt.length));
        }

        return HIDAPI_GIP_SendPacket(ctx, &hdr, &pkt);
    }
}

static SDL_bool HIDAPI_GIP_DispatchPacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, struct gip_header *hdr, Uint8 *data, Uint32 size)
{
    if ((hdr->options & 0x0F) != 0) {
        /* This is a packet for a device plugged into the controller, skip it */
        return SDL_TRUE;
    }

    if (hdr->options & GIP_OPT_INTERNAL) {
        switch (hdr->command) {
        case GIP_CMD_ACKNOWLEDGE:
            /* Ignore this packet */
            break;
        case GIP_CMD_ANNOUNCE:
            /* Controller is connected and waiting for initialization */
            /* The data bytes are:
               0x02 0x20 NN 0x1c, where NN is the packet sequence
               then 6 bytes of wireless MAC address
               then 2 bytes padding
               then 16-bit VID
               then 16-bit PID
               then 16-bit firmware version quartet AA.BB.CC.DD
                    e.g. 0x05 0x00 0x05 0x00 0x51 0x0a 0x00 0x00
                         is firmware version 5.5.2641.0, and product version 0x0505 = 1285
               then 8 bytes of unknown data
            */
#ifdef DEBUG_JOYSTICK
            SDL_Log("Controller announce after %llu ms\n", (SDL_GetTicks() - ctx->start_time));
#endif
            SetInitState(ctx, XBOX_ONE_INIT_STATE_ANNOUNCED);
            break;
        case GIP_CMD_STATUS:
            /* Controller status update */
            HIDAPI_DriverXboxOne_HandleStatusPacket(ctx, data, size);
            break;
        case GIP_CMD_IDENTIFY:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Identification request completed after %llu ms\n", (SDL_GetTicks() - ctx->send_time));
#endif
#ifdef DEBUG_XBOX_PROTOCOL
            HIDAPI_DumpPacket("Xbox One identification data: size = %d", data, size);
#endif
            SetInitState(ctx, XBOX_ONE_INIT_STATE_STARTUP);
            break;
        case GIP_CMD_POWER:
            /* Ignore this packet */
            break;
        case GIP_CMD_AUTHENTICATE:
            /* Ignore this packet */
            break;
        case GIP_CMD_VIRTUAL_KEY:
            if (joystick == NULL) {
                break;
            }
            HIDAPI_DriverXboxOne_HandleModePacket(joystick, ctx, data, size);
            break;
        case GIP_CMD_SERIAL_NUMBER:
            /* If the packet starts with this:
                0x1E 0x30 0x00 0x10 0x04 0x00
                then the next 14 bytes are the controller serial number
                    e.g. 0x30 0x39 0x37 0x31 0x32 0x33 0x33 0x32 0x33 0x35 0x34 0x30 0x33 0x36
                    is serial number "3039373132333332333534303336"

               The controller sends that in response to this request:
                0x1E 0x20 0x00 0x01 0x04
            */
            HIDAPI_DriverXboxOne_HandleSerialIDPacket(ctx, data, size);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown Xbox One packet: 0x%.2x\n", hdr->command);
#endif
            break;
        }
    } else {
        switch (hdr->command) {
        case GIP_CMD_INPUT:
            if (ctx->init_state < XBOX_ONE_INIT_STATE_COMPLETE) {
                SetInitState(ctx, XBOX_ONE_INIT_STATE_COMPLETE);

                /* Ignore the first input, it may be spurious */
#ifdef DEBUG_JOYSTICK
                SDL_Log("Controller ignoring spurious input\n");
#endif
                break;
            }
            if (joystick == NULL) {
                break;
            }
            HIDAPI_DriverXboxOne_HandleStatePacket(joystick, ctx, data, size);
            break;
        case GIP_CMD_UNMAPPED_STATE:
            if (joystick == NULL) {
                break;
            }
            HIDAPI_DriverXboxOne_HandleUnmappedStatePacket(joystick, ctx, data, size);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown Xbox One packet: 0x%.2x\n", hdr->command);
#endif
            break;
        }
    }
    return SDL_TRUE;
}

static void HIDAPI_GIP_DestroyChunkBuffer(SDL_DriverXboxOne_Context *ctx)
{
    if (ctx->chunk_buffer) {
        SDL_free(ctx->chunk_buffer);
        ctx->chunk_buffer = NULL;
        ctx->chunk_length = 0;
    }
}

static SDL_bool HIDAPI_GIP_CreateChunkBuffer(SDL_DriverXboxOne_Context *ctx, Uint32 size)
{
    HIDAPI_GIP_DestroyChunkBuffer(ctx);

    ctx->chunk_buffer = (Uint8 *)SDL_malloc(size);
    if (ctx->chunk_buffer) {
        ctx->chunk_length = size;
        return SDL_TRUE;
    } else {
        return SDL_FALSE;
    }
}

static SDL_bool HIDAPI_GIP_ProcessPacketChunked(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, struct gip_header *hdr, Uint8 *data)
{
    SDL_bool result;

    if (!ctx->chunk_buffer) {
        return SDL_FALSE;
    }

    if ((hdr->chunk_offset + hdr->packet_length) > ctx->chunk_length) {
        return SDL_FALSE;
    }

    if (hdr->packet_length) {
        SDL_memcpy(ctx->chunk_buffer + hdr->chunk_offset, data, hdr->packet_length);
        return SDL_TRUE;
    }

    result = HIDAPI_GIP_DispatchPacket(joystick, ctx, hdr, ctx->chunk_buffer, ctx->chunk_length);

    HIDAPI_GIP_DestroyChunkBuffer(ctx);

    return result;
}

static SDL_bool HIDAPI_GIP_ProcessPacket(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, struct gip_header *hdr, Uint8 *data)
{
    if (hdr->options & GIP_OPT_CHUNK_START) {
        if (!HIDAPI_GIP_CreateChunkBuffer(ctx, hdr->chunk_offset)) {
            return SDL_FALSE;
        }
        ctx->chunk_length = hdr->chunk_offset;

        hdr->chunk_offset = 0;
    }

    if (hdr->options & GIP_OPT_ACKNOWLEDGE) {
        if (!HIDAPI_GIP_AcknowledgePacket(ctx, hdr)) {
            return SDL_FALSE;
        }
    }

    if (hdr->options & GIP_OPT_CHUNK) {
        return HIDAPI_GIP_ProcessPacketChunked(joystick, ctx, hdr, data);
    } else {
        return HIDAPI_GIP_DispatchPacket(joystick, ctx, hdr, data, hdr->packet_length);
    }
}

static SDL_bool HIDAPI_GIP_ProcessData(SDL_Joystick *joystick, SDL_DriverXboxOne_Context *ctx, Uint8 *data, int size)
{
    struct gip_header hdr;
    int hdr_len;

    while (size > GIP_HEADER_MIN_LENGTH) {
        hdr_len = HIDAPI_GIP_DecodeHeader(&hdr, data, size);
        if ((hdr_len + hdr.packet_length) > (size_t)size) {
            return SDL_FALSE;
        }

        if (!HIDAPI_GIP_ProcessPacket(joystick, ctx, &hdr, data + hdr_len)) {
            return SDL_FALSE;
        }

        data += hdr_len + hdr.packet_length;
        size -= hdr_len + hdr.packet_length;
    }
    return SDL_TRUE;
}

static SDL_bool HIDAPI_DriverXboxOne_UpdateDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;
    SDL_Joystick *joystick = NULL;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    if (device->num_joysticks > 0) {
        joystick = SDL_GetJoystickFromInstanceID(device->joysticks[0]);
    } else {
        return SDL_FALSE;
    }

    while ((size = SDL_hid_read_timeout(device->dev, data, sizeof(data), 0)) > 0) {
#ifdef DEBUG_XBOX_PROTOCOL
        HIDAPI_DumpPacket("Xbox One packet: size = %d", data, size);
#endif
        if (device->is_bluetooth) {
            switch (data[0]) {
            case 0x01:
                if (joystick == NULL) {
                    break;
                }
                if (size >= 16) {
                    HIDAPI_DriverXboxOneBluetooth_HandleStatePacket(joystick, ctx, data, size);
                } else {
#ifdef DEBUG_JOYSTICK
                    SDL_Log("Unknown Xbox One Bluetooth packet size: %d\n", size);
#endif
                }
                break;
            case 0x02:
                if (joystick == NULL) {
                    break;
                }
                HIDAPI_DriverXboxOneBluetooth_HandleGuidePacket(joystick, ctx, data, size);
                break;
            case 0x04:
                if (joystick == NULL) {
                    break;
                }
                HIDAPI_DriverXboxOneBluetooth_HandleBatteryPacket(joystick, ctx, data, size);
                break;
            default:
#ifdef DEBUG_JOYSTICK
                SDL_Log("Unknown Xbox One packet: 0x%.2x\n", data[0]);
#endif
                break;
            }
        } else {
            HIDAPI_GIP_ProcessData(joystick, ctx, data, size);
        }
    }

    HIDAPI_DriverXboxOne_UpdateInitState(ctx);
    HIDAPI_DriverXboxOne_UpdateRumble(ctx);

    if (size < 0) {
        /* Read error, device is disconnected */
        HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
    }
    return size >= 0;
}

static void HIDAPI_DriverXboxOne_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED,
                        SDL_HomeLEDHintChanged, ctx);
}

static void HIDAPI_DriverXboxOne_FreeDevice(SDL_HIDAPI_Device *device)
{
    SDL_DriverXboxOne_Context *ctx = (SDL_DriverXboxOne_Context *)device->context;

    HIDAPI_GIP_DestroyChunkBuffer(ctx);
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXboxOne = {
    SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE,
    SDL_TRUE,
    HIDAPI_DriverXboxOne_RegisterHints,
    HIDAPI_DriverXboxOne_UnregisterHints,
    HIDAPI_DriverXboxOne_IsEnabled,
    HIDAPI_DriverXboxOne_IsSupportedDevice,
    HIDAPI_DriverXboxOne_InitDevice,
    HIDAPI_DriverXboxOne_GetDevicePlayerIndex,
    HIDAPI_DriverXboxOne_SetDevicePlayerIndex,
    HIDAPI_DriverXboxOne_UpdateDevice,
    HIDAPI_DriverXboxOne_OpenJoystick,
    HIDAPI_DriverXboxOne_RumbleJoystick,
    HIDAPI_DriverXboxOne_RumbleJoystickTriggers,
    HIDAPI_DriverXboxOne_GetJoystickCapabilities,
    HIDAPI_DriverXboxOne_SetJoystickLED,
    HIDAPI_DriverXboxOne_SendJoystickEffect,
    HIDAPI_DriverXboxOne_SetJoystickSensorsEnabled,
    HIDAPI_DriverXboxOne_CloseJoystick,
    HIDAPI_DriverXboxOne_FreeDevice,
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOXONE */

#endif /* SDL_JOYSTICK_HIDAPI */
