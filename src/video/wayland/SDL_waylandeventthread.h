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

#ifndef SDL_waylandeventthread_h_
#define SDL_waylandeventthread_h_

#include "../SDL_sysvideo.h"

typedef struct Wayland_EventThreadContext Wayland_EventThreadContext;

extern Wayland_EventThreadContext *Wayland_CreateEventThread(SDL_VideoData *data, const char *queue_name);
extern void Wayland_DestroyEventThread(Wayland_EventThreadContext *context);
extern void *Wayland_CreateEventThreadProxyWrapper(Wayland_EventThreadContext *context, void *obj);
extern void Wayland_LockEventThread(Wayland_EventThreadContext *context);
extern void Wayland_UnlockEventThread(Wayland_EventThreadContext *context);

#endif // SDL_waylandeventthread_h_
