/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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


/* Return true if the HIDAPI should ignore a device during enumeration */
extern bool SDL_HIDAPI_ShouldIgnoreDevice(int bus_type, Uint16 vendor_id, Uint16 product_id, Uint16 usage_page, Uint16 usage, bool libusb, bool is_xbox);

/* Called by platform backends when devices may have changed, this is safe to call from any thread */
extern void SDL_HIDAPI_DeviceChanged(void);

// Set a function to call whenever devices may have changed. It's called from whichever thread noticed the
// change, so it must be safe to call from any thread.
typedef void (*SDL_HIDAPI_DeviceChangeCallback)(void);
extern void SDL_HIDAPI_SetDeviceChangeCallback(SDL_HIDAPI_DeviceChangeCallback callback);

// Return true if the platform notifies hidapi of device changes. Otherwise, SDL_hid_device_change_count()
// reports a change every few seconds, so callers should poll it.
extern bool SDL_HIDAPI_CanGetDeviceNotifications(void);

// Return a file descriptor that becomes readable when devices may have changed, or -1 if there isn't one.
// When it's readable, call SDL_hid_device_change_count() to process it.
extern int SDL_HIDAPI_GetDiscoveryFD(void);

#ifdef SDL_PLATFORM_ANDROID
// Whether the user has been asked for permission to open a device. Devices are left out of enumeration
// from when the user is asked until they're enumerated again after the answer.
typedef enum
{
    SDL_HIDAPI_OPEN_PERMISSION_NONE,    // The device isn't connected, or the user wasn't asked
    SDL_HIDAPI_OPEN_PERMISSION_PENDING, // Waiting for the user's answer
    SDL_HIDAPI_OPEN_PERMISSION_GRANTED, // Granted, the device will be enumerated again
    SDL_HIDAPI_OPEN_PERMISSION_DENIED   // Denied, the device won't be enumerated until it's reconnected
} SDL_HIDAPI_OpenPermission;

extern SDL_HIDAPI_OpenPermission SDL_HIDAPI_GetOpenPermission(const char *path);
#endif

