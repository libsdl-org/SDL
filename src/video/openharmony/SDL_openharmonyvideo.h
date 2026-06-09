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

#ifndef SDL_openharmonyvideo_h_
#define SDL_openharmonyvideo_h_

#include "../SDL_sysvideo.h"

// these are fired from XComponent callbacks registered in src/core/openharmony/SDL_openharmony.c
extern void SDL_OpenHarmonyVideoSurfaceDestroyed(void *component, void *window);
extern void SDL_OpenHarmonyVideoSurfaceChanged(void *component, void *window);
extern void SDL_OpenHarmonyVideoSurfaceCreated(void *component, void *window);

// Get the app's current native window.
extern void SDL_OpenHarmonyGetNativeWindowPointers(void **xcomponent, void **window);

struct SDL_VideoData
{
    void *oh_pasteboard;
    void *oh_pasteboard_observer;
    bool clipboard_set;

// !!! FIXME: Android stuff.
    int isPaused;
    int isPausing;
};

#endif // SDL_openharmonyvideo_h_
