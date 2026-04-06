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

#ifdef SDL_JOYSTICK_PS3

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include <io/pad.h>

#define NAMESIZE 10

typedef struct {
    int       word;
    u16       mask;
} PS3ButtonDef;

static const PS3ButtonDef ps3_buttons[] = {
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_TRIANGLE },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_CIRCLE   },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_CROSS    },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_SQUARE   },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_L1       },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_R1       },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_DOWN     },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_LEFT     },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_UP       },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_RIGHT    },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_SELECT   },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_START    },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_L2       },
    { PAD_BUTTON_OFFSET_DIGITAL2, PAD_CTRL_R2       },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_L3       },
    { PAD_BUTTON_OFFSET_DIGITAL1, PAD_CTRL_R3       },
};

typedef struct {
    int word;
} PS3AxisDef;

static const PS3AxisDef ps3_axes[] = {
    { PAD_BUTTON_OFFSET_ANALOG_LEFT_X },
    { PAD_BUTTON_OFFSET_ANALOG_LEFT_Y },
    { PAD_BUTTON_OFFSET_ANALOG_RIGHT_X },
    { PAD_BUTTON_OFFSET_ANALOG_RIGHT_Y },
};

struct joystick_hwdata
{
    padData old_pad_data;
};

int numberOfJoysticks = MAX_PORT_NUM;

static bool PS3_JoystickInit(void)
{
    int result = ioPadInit(MAX_PORT_NUM);

    // No gamepads present.
    if (result != 0) {
        SDL_SetError("PS3_JoystickInit() : Couldn't initialize PS3 pads");
        return false;
    }

    SDL_PrivateJoystickAdded(1);

    return true;
}

static int PS3_JoystickGetCount(void)
{
    return numberOfJoysticks;
}

static void PS3_JoystickDetect(void)
{
}

static const char * PS3_JoystickGetDeviceName(int device_index)
{
    if (device_index < numberOfJoysticks) {
        return "PS3 Controller";
    }

    SDL_SetError("No joystick available with that index");
    return NULL;
}

static int PS3_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void PS3_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{

}

static SDL_JoystickID PS3_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index + 1;
}

static SDL_GUID PS3_JoystickGetDeviceGUID(int device_index)
{
    const char *name = PS3_JoystickGetDeviceName(device_index);
    return SDL_CreateJoystickGUIDForName(name);
}

static bool PS3_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    if (!(joystick->hwdata = (struct joystick_hwdata*)SDL_malloc(sizeof(struct joystick_hwdata))))
    {
        return false;
    }

    joystick->naxes = SDL_arraysize(ps3_axes);
    joystick->nhats = 0;
    joystick->nballs = 0;
    joystick->nbuttons = SDL_arraysize(ps3_buttons);

    // TODO: using padinfo check if rumble available

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);

    return true;
}

static bool PS3_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    padActParam act = {0};
    int joystickIndex = (int)(joystick->instance_id - 1);

    // low frequency, analog [0x00, 0xFF]
    act.large_motor = (Uint8)(low_frequency_rumble);
    // small motor — high frequency, digital on/off only
    act.small_motor = (high_frequency_rumble > 0) ? 1 : 0;

    int result = ioPadSetActDirect(joystickIndex, &act);

    return (result == 0);
}

static bool PSP_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static SDL_INLINE Sint16 PS3_AxisScale(u16 raw)
{
    return (Sint16)(((int)(raw & 0xFF) - 128) * 257);
}

static void PS3_JoystickUpdate(SDL_Joystick * joystick)
{

    padData new_pad_data;
    int joystickIndex = (int)(joystick->instance_id - 1);
    Uint64 timestamp = SDL_GetTicksNS();

    if (ioPadGetData(joystickIndex, (padData*)&new_pad_data) != 0)
    {
        return;
    }
    
    if (new_pad_data.len >= 8) {
        // Update buttons
        for (int i = 0; i < SDL_arraysize(ps3_buttons); i++) {
            const PS3ButtonDef *b = &ps3_buttons[i];
            int cur  = (new_pad_data.button[b->word] & b->mask) != 0;
            int prev = (joystick->hwdata->old_pad_data.button[b->word] & b->mask) != 0;

            if (cur != prev) {
                SDL_SendJoystickButton(timestamp, joystick, i, cur);
            }
        }

        // Update axes
        for (int i = 0; i < SDL_arraysize(ps3_axes); i++) {
            const PS3AxisDef *a = &ps3_axes[i];
            u16 cur  = new_pad_data.button[a->word];
            u16 prev = joystick->hwdata->old_pad_data.button[a->word];

            if (cur != prev) {
                SDL_SendJoystickAxis(timestamp, joystick, i, PS3_AxisScale(cur));
            }
        }

        joystick->hwdata->old_pad_data = new_pad_data;
    }
}

static void PS3_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata)
        SDL_free(joystick->hwdata);
}

static void PS3_JoystickQuit(void)
{
    numberOfJoysticks = 0;
}

static bool PS3_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return false;
}

static bool PS3_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    // We don't override any other drivers
    return false;
}

static const char *PS3_JoystickGetDevicePath(int index)
{
    return NULL;
}

static int PS3_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

static bool PS3_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool PS3_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool PS3_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool PS3_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

SDL_JoystickDriver SDL_PS3_JoystickDriver =
{
    PS3_JoystickInit,
    PS3_JoystickGetCount,
    PS3_JoystickDetect,
    PS3_JoystickIsDevicePresent,
    PS3_JoystickGetDeviceName,
    PS3_JoystickGetDevicePath,
    PS3_JoystickGetDeviceSteamVirtualGamepadSlot,
    PS3_JoystickGetDevicePlayerIndex,
    PS3_JoystickSetDevicePlayerIndex,
    PS3_JoystickGetDeviceGUID,
    PS3_JoystickGetDeviceInstanceID,
    PS3_JoystickOpen,
    PS3_JoystickRumble,
    PS3_JoystickRumbleTriggers,
    PS3_JoystickSetLED,
    PS3_JoystickSendEffect,
    PS3_JoystickSetSensorsEnabled,
    PS3_JoystickUpdate,
    PS3_JoystickClose,
    PS3_JoystickQuit,
    PS3_JoystickGetGamepadMapping
};

#endif // SDL_JOYSTICK_PS3

/* vi: set ts=4 sw=4 expandtab: */
