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
/* This driver supports the Nintendo Switch Pro controller.
   Code and logic contributed by Valve Corporation under the SDL zlib license.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_HIDAPI

#include "../../SDL_hints_c.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"
#include "SDL_hidapi_nintendo.h"

#ifdef SDL_JOYSTICK_HIDAPI_SWITCH2

static void HIDAPI_DriverSwitch2_RegisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, callback, userdata);
}

static void HIDAPI_DriverSwitch2_UnregisterHints(SDL_HintCallback callback, void *userdata)
{
    SDL_RemoveHintCallback(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, callback, userdata);
}

static bool HIDAPI_DriverSwitch2_IsEnabled(void)
{
    return SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI_SWITCH2, SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, SDL_HIDAPI_DEFAULT));
}

static bool HIDAPI_DriverSwitch2_IsSupportedDevice(SDL_HIDAPI_Device *device, const char *name, SDL_GamepadType type, Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, int interface_class, int interface_subclass, int interface_protocol)
{
    if (vendor_id == USB_VENDOR_NINTENDO) {
        if (product_id == USB_PRODUCT_NINTENDO_SWITCH2_GAMECUBE_CONTROLLER) {
            return true;
        }
    }

    return false;
}

static bool HIDAPI_DriverSwitch2_InitDevice(SDL_HIDAPI_Device *device)
{
    return SDL_Unsupported();
}

static int HIDAPI_DriverSwitch2_GetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id)
{
    return -1;
}

static void HIDAPI_DriverSwitch2_SetDevicePlayerIndex(SDL_HIDAPI_Device *device, SDL_JoystickID instance_id, int player_index)
{
}

static bool HIDAPI_DriverSwitch2_UpdateDevice(SDL_HIDAPI_Device *device)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_OpenJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_RumbleJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_RumbleJoystickTriggers(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 HIDAPI_DriverSwitch2_GetJoystickCapabilities(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
    return 0;
}

static bool HIDAPI_DriverSwitch2_SetJoystickLED(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_SendJoystickEffect(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool HIDAPI_DriverSwitch2_SetJoystickSensorsEnabled(SDL_HIDAPI_Device *device, SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void HIDAPI_DriverSwitch2_CloseJoystick(SDL_HIDAPI_Device *device, SDL_Joystick *joystick)
{
}

static void HIDAPI_DriverSwitch2_FreeDevice(SDL_HIDAPI_Device *device)
{
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSwitch2 = {
    SDL_HINT_JOYSTICK_HIDAPI_SWITCH2,
    true,
    HIDAPI_DriverSwitch2_RegisterHints,
    HIDAPI_DriverSwitch2_UnregisterHints,
    HIDAPI_DriverSwitch2_IsEnabled,
    HIDAPI_DriverSwitch2_IsSupportedDevice,
    HIDAPI_DriverSwitch2_InitDevice,
    HIDAPI_DriverSwitch2_GetDevicePlayerIndex,
    HIDAPI_DriverSwitch2_SetDevicePlayerIndex,
    HIDAPI_DriverSwitch2_UpdateDevice,
    HIDAPI_DriverSwitch2_OpenJoystick,
    HIDAPI_DriverSwitch2_RumbleJoystick,
    HIDAPI_DriverSwitch2_RumbleJoystickTriggers,
    HIDAPI_DriverSwitch2_GetJoystickCapabilities,
    HIDAPI_DriverSwitch2_SetJoystickLED,
    HIDAPI_DriverSwitch2_SendJoystickEffect,
    HIDAPI_DriverSwitch2_SetJoystickSensorsEnabled,
    HIDAPI_DriverSwitch2_CloseJoystick,
    HIDAPI_DriverSwitch2_FreeDevice,
};

#endif // SDL_JOYSTICK_HIDAPI_SWITCH2

#endif // SDL_JOYSTICK_HIDAPI
