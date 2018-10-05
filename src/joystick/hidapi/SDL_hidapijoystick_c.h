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

#ifndef SDL_JOYSTICK_HIDAPI_H
#define SDL_JOYSTICK_HIDAPI_H

#include "../../hidapi/hidapi/hidapi.h"

/* This is the full set of HIDAPI drivers available */
#define SDL_JOYSTICK_HIDAPI_PS4
#define SDL_JOYSTICK_HIDAPI_SWITCH
#define SDL_JOYSTICK_HIDAPI_XBOX360
#define SDL_JOYSTICK_HIDAPI_XBOXONE

#ifdef __WINDOWS__
/* On Windows, Xbox One controllers are handled by the Xbox 360 driver */
#undef SDL_JOYSTICK_HIDAPI_XBOXONE
/* It turns out HIDAPI for Xbox controllers doesn't allow background input */
#undef SDL_JOYSTICK_HIDAPI_XBOX360
#endif

#ifdef __MACOSX__
/* On Mac OS X, Xbox One controllers are handled by the Xbox 360 driver */
#undef SDL_JOYSTICK_HIDAPI_XBOXONE
#endif

typedef struct _SDL_HIDAPI_DeviceDriver
{
    const char *hint;
    SDL_bool enabled;
    SDL_bool (*IsSupportedDevice)(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number);
    const char *(*GetDeviceName)(Uint16 vendor_id, Uint16 product_id);
    SDL_bool (*Init)(SDL_Joystick *joystick, hid_device *dev, Uint16 vendor_id, Uint16 product_id, void **context);
    int (*Rumble)(SDL_Joystick *joystick, hid_device *dev, void *context, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms);
    SDL_bool (*Update)(SDL_Joystick *joystick, hid_device *dev, void *context);
    void (*Quit)(SDL_Joystick *joystick, hid_device *dev, void *context);

} SDL_HIDAPI_DeviceDriver;

/* HIDAPI device support */
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverPS4;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSteam;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverSwitch;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360;
extern SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXboxOne;

/* Return true if a HID device is present and supported as a joystick */
extern SDL_bool HIDAPI_IsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version);

/* Return the name of an Xbox 360 or Xbox One controller */
extern const char *HIDAPI_XboxControllerName(Uint16 vendor_id, Uint16 product_id);

#endif /* SDL_JOYSTICK_HIDAPI_H */

/* vi: set ts=4 sw=4 expandtab: */
