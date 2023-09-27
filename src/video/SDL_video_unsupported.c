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

#ifndef SDL_VIDEO_DRIVER_WINDOWS

DECLSPEC SDL_bool SDLCALL SDL_DXGIGetOutputInfo(SDL_DisplayID displayID, int *adapterIndex, int *outputIndex);
SDL_bool SDL_DXGIGetOutputInfo(SDL_DisplayID displayID, int *adapterIndex, int *outputIndex)
{
    (void)displayID;
    (void)adapterIndex;
    (void)outputIndex;
    SDL_Unsupported();
    return SDL_FALSE;
}

DECLSPEC int SDLCALL SDL_Direct3D9GetAdapterIndex(SDL_DisplayID displayID);
int SDL_Direct3D9GetAdapterIndex(SDL_DisplayID displayID)
{
    (void)displayID;
    return SDL_Unsupported();
}

#endif

#ifndef SDL_GDK_TEXTINPUT

DECLSPEC int SDLCALL SDL_GDKGetTaskQueue(void *outTaskQueue);
int SDL_GDKGetTaskQueue(void *outTaskQueue)
{
    (void)outTaskQueue;
    return SDL_Unsupported();
}

#endif

#ifndef SDL_VIDEO_DRIVER_UIKIT

DECLSPEC void SDLCALL SDL_OnApplicationDidChangeStatusBarOrientation(void);
void SDL_OnApplicationDidChangeStatusBarOrientation(void)
{
    SDL_Unsupported();
}

#endif

#ifndef SDL_VIDEO_DRIVER_UIKIT

DECLSPEC int SDLCALL SDL_iPhoneSetAnimationCallback(SDL_Window *window, int interval, void (*callback)(void *), void *callbackParam);
int SDL_iPhoneSetAnimationCallback(SDL_Window *window, int interval, void (*callback)(void *), void *callbackParam)
{
    (void)window;
    (void)interval;
    (void)callback;
    (void)callbackParam;
    return SDL_Unsupported();
}

DECLSPEC void SDLCALL SDL_iPhoneSetEventPump(SDL_bool enabled);
void SDL_iPhoneSetEventPump(SDL_bool enabled)
{
    (void)enabled;
    SDL_Unsupported();
}
#endif
