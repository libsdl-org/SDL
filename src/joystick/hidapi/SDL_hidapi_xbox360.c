/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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


#ifdef SDL_JOYSTICK_HIDAPI_XBOX360

#ifdef __WIN32__
#include "../../core/windows/SDL_xinput.h"
#endif

#define USB_PACKET_LENGTH   64


typedef struct {
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint32 rumble_expiration;
#ifdef __WIN32__
    SDL_bool xinput_enabled;
    Uint8 xinput_slot;
#endif
} SDL_DriverXbox360_Context;


#ifdef __WIN32__
static Uint8 xinput_slots;

static void
HIDAPI_DriverXbox360_MarkXInputSlotUsed(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_slots |= (0x01 << xinput_slot);
    }
}

static void
HIDAPI_DriverXbox360_MarkXInputSlotFree(Uint8 xinput_slot)
{
    if (xinput_slot != XUSER_INDEX_ANY) {
        xinput_slots &= ~(0x01 << xinput_slot);
    }
}

static SDL_bool
HIDAPI_DriverXbox360_MissingXInputSlot()
{
    return xinput_slots != 0x0F;
}

static Uint8
HIDAPI_DriverXbox360_GuessXInputSlot(WORD wButtons)
{
    DWORD user_index;
    int match_count;
    Uint8 match_slot;

    if (!XINPUTGETSTATE) {
        return XUSER_INDEX_ANY;
    }

    match_count = 0;
    for (user_index = 0; user_index < XUSER_MAX_COUNT; ++user_index) {
        XINPUT_STATE_EX xinput_state;

        if (XINPUTGETSTATE(user_index, &xinput_state) == ERROR_SUCCESS) {
            if (xinput_state.Gamepad.wButtons == wButtons) {
                ++match_count;
                match_slot = (Uint8)user_index;
            }
        }
    }
    if (match_count == 1) {
        return match_slot;
    }
    return XUSER_INDEX_ANY;
}

#endif /* __WIN32__ */

static SDL_bool
HIDAPI_DriverXbox360_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, Uint16 usage_page, Uint16 usage)
{
#if defined(__MACOSX__) || defined(__WIN32__)
    if (vendor_id == 0x045e && product_id == 0x028e && version == 1) {
        /* This is the Steam Virtual Gamepad, which isn't supported by this driver */
        return SDL_FALSE;
    }
    return SDL_IsJoystickXbox360(vendor_id, product_id) || SDL_IsJoystickXboxOne(vendor_id, product_id);
#else
    return SDL_IsJoystickXbox360(vendor_id, product_id);
#endif
}

static const char *
HIDAPI_DriverXbox360_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    return HIDAPI_XboxControllerName(vendor_id, product_id);
}

static SDL_bool SetSlotLED(hid_device *dev, Uint8 slot)
{
    const Uint8 led_packet[] = { 0x01, 0x03, (2 + slot) };

    if (hid_write(dev, led_packet, sizeof(led_packet)) != sizeof(led_packet)) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverXbox360_Init(SDL_Joystick *joystick, hid_device *dev, Uint16 vendor_id, Uint16 product_id, void **context)
{
    SDL_DriverXbox360_Context *ctx;

    ctx = (SDL_DriverXbox360_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
#ifdef __WIN32__
    ctx->xinput_enabled = SDL_GetHintBoolean(SDL_HINT_XINPUT_ENABLED, SDL_TRUE);
    if (ctx->xinput_enabled && WIN_LoadXInputDLL() < 0) {
        ctx->xinput_enabled = SDL_FALSE;
    }
    ctx->xinput_slot = XUSER_INDEX_ANY;
#endif
    *context = ctx;

    /* Set the controller LED */
    SetSlotLED(dev, (joystick->instance_id % 4));

    /* Initialize the joystick capabilities */
    joystick->nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverXbox360_Rumble(SDL_Joystick *joystick, hid_device *dev, void *context, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)context;
#ifdef __WIN32__
    if (ctx->xinput_slot != XUSER_INDEX_ANY) {
        XINPUT_VIBRATION XVibration;

        if (!XINPUTSETSTATE) {
            return SDL_Unsupported();
        }

        XVibration.wLeftMotorSpeed = low_frequency_rumble;
        XVibration.wRightMotorSpeed = high_frequency_rumble;
        if (XINPUTSETSTATE(ctx->xinput_slot, &XVibration) != ERROR_SUCCESS) {
            return SDL_SetError("XInputSetState() failed");
        }
    }
#else
#ifdef __MACOSX__
    /* On Mac OS X the 360Controller driver uses this short report,
       and we need to prefix it with a magic token so hidapi passes it through untouched
     */
    Uint8 rumble_packet[] = { 'M', 'A', 'G', 'I', 'C', '0', 0x00, 0x04, 0x00, 0x00 };

    rumble_packet[6+2] = (low_frequency_rumble >> 8);
    rumble_packet[6+3] = (high_frequency_rumble >> 8);
#else
    Uint8 rumble_packet[] = { 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    rumble_packet[3] = (low_frequency_rumble >> 8);
    rumble_packet[4] = (high_frequency_rumble >> 8);
#endif

    if (hid_write(dev, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }
#endif /* __WIN32__ */

    if ((low_frequency_rumble || high_frequency_rumble) && duration_ms) {
        ctx->rumble_expiration = SDL_GetTicks() + duration_ms;
    } else {
        ctx->rumble_expiration = 0;
    }
    return 0;
}

#ifdef __WIN32__
 /* This is the packet format for Xbox 360 and Xbox One controllers on Windows,
    however with this interface there is no rumble support, no guide button,
    and the left and right triggers are tied together as a single axis.
  */
static void
HIDAPI_DriverXbox360_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;

    if (ctx->last_state[10] != data[10]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[10] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[10] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[10] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[10] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[10] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[10] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[10] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[10] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[11] != data[11]) {
        SDL_bool dpad_up = SDL_FALSE;
        SDL_bool dpad_down = SDL_FALSE;
        SDL_bool dpad_left = SDL_FALSE;
        SDL_bool dpad_right = SDL_FALSE;

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[11] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[11] & 0x02) ? SDL_PRESSED : SDL_RELEASED);

        switch (data[11] & 0x3C) {
        case 4:
            dpad_up = SDL_TRUE;
            break;
        case 8:
            dpad_up = SDL_TRUE;
            dpad_right = SDL_TRUE;
            break;
        case 12:
            dpad_right = SDL_TRUE;
            break;
        case 16:
            dpad_right = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 20:
            dpad_down = SDL_TRUE;
            break;
        case 24:
            dpad_left = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 28:
            dpad_left = SDL_TRUE;
            break;
        case 32:
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

    axis = (int)*(Uint16*)(&data[0]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = (int)*(Uint16*)(&data[2]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = (int)*(Uint16*)(&data[4]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = (int)*(Uint16*)(&data[6]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    if (ctx->xinput_slot == XUSER_INDEX_ANY && HIDAPI_DriverXbox360_MissingXInputSlot()) {
        WORD wButtons = 0;

        if (data[10] & 0x01) {
            wButtons |= XINPUT_GAMEPAD_A;
        }
        if (data[10] & 0x02) {
            wButtons |= XINPUT_GAMEPAD_B;
        }
        if (data[10] & 0x04) {
            wButtons |= XINPUT_GAMEPAD_X;
        }
        if (data[10] & 0x08) {
            wButtons |= XINPUT_GAMEPAD_Y;
        }
        if (wButtons != 0) {
            Uint8 xinput_slot = HIDAPI_DriverXbox360_GuessXInputSlot(wButtons);
            if (xinput_slot != XUSER_INDEX_ANY) {
                HIDAPI_DriverXbox360_MarkXInputSlotUsed(xinput_slot);
                ctx->xinput_slot = xinput_slot;
            }
        }
    }

    if (ctx->xinput_slot == XUSER_INDEX_ANY) {
        axis = (data[9] * 257) - 32768;
        if (data[9] < 0x80) {
            axis = -axis * 2 - 32769;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
        } else if (data[9] > 0x80) {
            axis = axis * 2 - 32767;
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
        } else {
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_MIN_SINT16);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_MIN_SINT16);
        }
    } else {
        XINPUT_STATE_EX xinput_state;

        if (XINPUTGETSTATE(ctx->xinput_slot, &xinput_state) == ERROR_SUCCESS) {
            SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) ? SDL_PRESSED : SDL_RELEASED);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, ((int)xinput_state.Gamepad.bLeftTrigger * 257) - 32768);
            SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, ((int)xinput_state.Gamepad.bRightTrigger * 257) - 32768);
        }
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}
#else

static void
HIDAPI_DriverXbox360_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
#ifdef __MACOSX__
    const SDL_bool invert_y_axes = SDL_FALSE;
#else
    const SDL_bool invert_y_axes = SDL_TRUE;
#endif

    if (ctx->last_state[2] != data[2]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, (data[2] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, (data[2] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, (data[2] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (data[2] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[2] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[2] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[2] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[2] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[3] != data[3]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[3] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[3] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[3] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[3] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[3] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[3] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[3] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    axis = ((int)data[4] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)data[5] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = *(Sint16*)(&data[6]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = *(Sint16*)(&data[8]);
    if (invert_y_axes) {
        axis = ~axis;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = *(Sint16*)(&data[10]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = *(Sint16*)(&data[12]);
    if (invert_y_axes) {
        axis = ~axis;
    }
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}
#endif /* __WIN32__ */

static SDL_bool
HIDAPI_DriverXbox360_Update(SDL_Joystick *joystick, hid_device *dev, void *context)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)context;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    while ((size = hid_read_timeout(dev, data, sizeof(data), 0)) > 0) {
#ifdef __WIN32__
        HIDAPI_DriverXbox360_HandleStatePacket(joystick, dev, ctx, data, size);
#else
        switch (data[0]) {
        case 0x00:
            HIDAPI_DriverXbox360_HandleStatePacket(joystick, dev, ctx, data, size);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown Xbox 360 packet: 0x%.2x\n", data[0]);
#endif
            break;
        }
#endif /* __WIN32__ */
    }

    if (ctx->rumble_expiration) {
        Uint32 now = SDL_GetTicks();
        if (SDL_TICKS_PASSED(now, ctx->rumble_expiration)) {
            HIDAPI_DriverXbox360_Rumble(joystick, dev, context, 0, 0, 0);
        }
    }

    return (size >= 0);
}

static void
HIDAPI_DriverXbox360_Quit(SDL_Joystick *joystick, hid_device *dev, void *context)
{
#ifdef __WIN32__
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)context;

    if (ctx->xinput_enabled) {
        HIDAPI_DriverXbox360_MarkXInputSlotFree(ctx->xinput_slot);
        WIN_UnloadXInputDLL();
    }
#endif
    SDL_free(context);
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360 =
{
    SDL_HINT_JOYSTICK_HIDAPI_XBOX360,
    SDL_TRUE,
    HIDAPI_DriverXbox360_IsSupportedDevice,
    HIDAPI_DriverXbox360_GetDeviceName,
    HIDAPI_DriverXbox360_Init,
    HIDAPI_DriverXbox360_Rumble,
    HIDAPI_DriverXbox360_Update,
    HIDAPI_DriverXbox360_Quit
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOX360 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
