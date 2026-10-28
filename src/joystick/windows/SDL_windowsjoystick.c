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

#if defined(SDL_JOYSTICK_DINPUT) || defined(SDL_JOYSTICK_XINPUT)

/* Device change handling shared by the XInput and DirectInput joystick drivers.
 *
 * Both APIs see devices some time after Windows reports the device interface, so each driver looks again
 * 300 ms and 2 seconds after a device interface arrives or is removed. Each notification pushes the 300 ms
 * scan back, so a burst of notifications, e.g. the several interfaces of one controller, leads to one scan
 * after the burst. The 2 second scan isn't pushed back, so device changes that never stop (the notification
 * covers every interface class) can't postpone scanning forever.
 *
 * Drivers whose scan is cheap (XInput) can also look as soon as a notification arrives. DirectInput doesn't:
 * its scan takes hundreds of milliseconds, and it runs on the joystick device thread, so scanning as each
 * notification arrives would hold up the other drivers on that thread (HIDAPI in particular) while a
 * controller's interfaces are still arriving. Waiting until after the burst also gives HIDAPI time to claim
 * the controller, so DirectInput's view of it is hidden as soon as it's reported.
 */

#include "../SDL_sysjoystick.h"
#include "../../core/windows/SDL_windows.h"

#define INITGUID // Only set here, if set twice will cause mingw32 to break.
#include "SDL_windowsjoystick_c.h"

static int s_notification_refcount;
static WIN_DeviceInterfaceNotification *s_notification;
static SDL_AtomicU32 s_device_change_count;

static void WINDOWS_DeviceInterfaceChanged(void *userdata)
{
    SDL_AddAtomicU32(&s_device_change_count, 1);
    SDL_WakeJoystickDeviceThread();
}

void WINDOWS_InitJoystickDeviceNotification(void)
{
    if (s_notification_refcount++ == 0) {
        // This isn't available on Windows 7 or Xbox, so drivers poll if they need to
        s_notification = WIN_RegisterDeviceInterfaceNotification(WINDOWS_DeviceInterfaceChanged, NULL);
    }
}

void WINDOWS_QuitJoystickDeviceNotification(void)
{
    if (s_notification_refcount > 0 && --s_notification_refcount == 0) {
        WIN_UnregisterDeviceInterfaceNotification(s_notification);
        s_notification = NULL;
    }
}

bool WINDOWS_JoystickRescanNeeded(WINDOWS_JoystickRescan *rescan, bool scan_immediately, Uint32 poll_interval_ms, Sint32 *timeout)
{
    const Uint64 delays[SDL_arraysize(rescan->scan_ns)] = { SDL_MS_TO_NS(300), SDL_MS_TO_NS(2000) };
    const Uint64 now = SDL_GetTicksNS();
    const Uint32 change_count = SDL_GetAtomicU32(&s_device_change_count);
    const bool polling = (!s_notification && poll_interval_ms > 0);
    Uint64 next_ns = 0;
    bool scan = false;

    if (!rescan->initialized) {
        // Always look for devices the first time
        rescan->initialized = true;
        rescan->change_count = change_count;
        scan = true;
    } else if (change_count != rescan->change_count) {
        rescan->change_count = change_count;
        for (int i = 0; i < SDL_arraysize(rescan->scan_ns); ++i) {
            if (i == 0 || !rescan->scan_ns[i]) {
                rescan->scan_ns[i] = now + delays[i];
            }
        }
        if (scan_immediately) {
            scan = true;
        }
    }

    for (int i = 0; i < SDL_arraysize(rescan->scan_ns); ++i) {
        if (rescan->scan_ns[i] && now >= rescan->scan_ns[i]) {
            rescan->scan_ns[i] = 0;
            scan = true;
        }
    }

    if (polling && now >= rescan->last_scan_ns + SDL_MS_TO_NS(poll_interval_ms)) {
        scan = true;
    }

    if (scan) {
        rescan->last_scan_ns = now;
    }

    // Work out when the next scan is due
    for (int i = 0; i < SDL_arraysize(rescan->scan_ns); ++i) {
        if (rescan->scan_ns[i] && (!next_ns || rescan->scan_ns[i] < next_ns)) {
            next_ns = rescan->scan_ns[i];
        }
    }
    if (polling) {
        Uint64 poll_ns = rescan->last_scan_ns + SDL_MS_TO_NS(poll_interval_ms);
        if (!next_ns || poll_ns < next_ns) {
            next_ns = poll_ns;
        }
    }
    if (next_ns) {
        *timeout = (next_ns > now) ? (Sint32)SDL_NS_TO_MS(next_ns - now + SDL_NS_PER_MS - 1) : 0;
    } else {
        *timeout = -1;
    }

    return scan;
}

#endif // SDL_JOYSTICK_DINPUT || SDL_JOYSTICK_XINPUT
