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

#ifndef SDL_devicewatcher_h_
#define SDL_devicewatcher_h_

// Platform specific notification of devices being added and removed

typedef struct SDL_DeviceWatcher SDL_DeviceWatcher;

extern SDL_DeviceWatcher *SDL_CreateDeviceWatcher(void);

// Wait until a device changes, the watcher is woken, or the timeout expires
// The timeout is in milliseconds, or -1 to wait indefinitely
// Returns the time in milliseconds of the most recent device change since the last call, or 0 if devices haven't changed
extern Uint64 SDL_WaitDeviceWatcher(SDL_DeviceWatcher *watcher, Sint32 timeoutMS);

// Wake up a thread waiting on the watcher, this is safe to call from any thread
extern void SDL_WakeDeviceWatcher(SDL_DeviceWatcher *watcher, bool device_changed);

// Get a counter that changes whenever devices change, this is safe to call from any thread
extern Uint32 SDL_GetDeviceWatcherChangeCount(SDL_DeviceWatcher *watcher);

extern void SDL_DestroyDeviceWatcher(SDL_DeviceWatcher *watcher);

#endif // SDL_devicewatcher_h_
