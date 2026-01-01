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

#include "SDL_core_unsupported.h"

#ifndef SDL_VIDEO_DRIVER_X11

void SDL_SetX11EventHook(SDL_X11EventHook callback, void *userdata)
{
}

#endif /* !SDL_VIDEO_DRIVER_X11 */

#ifndef SDL_PLATFORM_LINUX

bool SDL_SetLinuxThreadPriority(Sint64 threadID, int priority)
{
    (void)threadID;
    (void)priority;
    return SDL_Unsupported();
}

bool SDL_SetLinuxThreadPriorityAndPolicy(Sint64 threadID, int sdlPriority, int schedPolicy)
{
    (void)threadID;
    (void)sdlPriority;
    (void)schedPolicy;
    return SDL_Unsupported();
}

#endif /* !SDL_PLATFORM_LINUX */

#ifndef SDL_PLATFORM_GDK

void SDL_GDKSuspendComplete(void)
{
    SDL_Unsupported();
}

bool SDL_GetGDKDefaultUser(XUserHandle *outUserHandle)
{
    return SDL_Unsupported();
}

void SDL_GDKSuspendGPU(SDL_GPUDevice *device)
{
}

void SDL_GDKResumeGPU(SDL_GPUDevice *device)
{
}

#endif /* !SDL_PLATFORM_GDK */

#if !defined(SDL_PLATFORM_WINDOWS)

bool SDL_RegisterApp(const char *name, Uint32 style, void *hInst)
{
    (void)name;
    (void)style;
    (void)hInst;
    return SDL_Unsupported();
}

void SDL_SetWindowsMessageHook(void *callback, void *userdata)
{
    (void)callback;
    (void)userdata;
    SDL_Unsupported();
}

void SDL_UnregisterApp(void)
{
    SDL_Unsupported();
}

#endif /* !SDL_PLATFORM_WINDOWS */

#ifndef SDL_PLATFORM_ANDROID

void SDL_SendAndroidBackButton(void)
{
    SDL_Unsupported();
}

void *SDL_GetAndroidActivity(void)
{
    SDL_Unsupported();
    return NULL;
}

const char *SDL_GetAndroidCachePath(void)
{
    SDL_Unsupported();
    return NULL;
}


const char *SDL_GetAndroidExternalStoragePath(void)
{
    SDL_Unsupported();
    return NULL;
}

Uint32 SDL_GetAndroidExternalStorageState(void)
{
    SDL_Unsupported();
    return 0;
}
const char *SDL_GetAndroidInternalStoragePath(void)
{
    SDL_Unsupported();
    return NULL;
}

void *SDL_GetAndroidJNIEnv(void)
{
    SDL_Unsupported();
    return NULL;
}

bool SDL_RequestAndroidPermission(const char *permission, SDL_RequestAndroidPermissionCallback cb, void *userdata)
{
    (void)permission;
    (void)cb;
    (void)userdata;
    return SDL_Unsupported();
}

bool SDL_SendAndroidMessage(Uint32 command, int param)
{
    (void)command;
    (void)param;
    return SDL_Unsupported();
}

bool SDL_ShowAndroidToast(const char *message, int duration, int gravity, int xoffset, int yoffset)
{
    (void)message;
    (void)duration;
    (void)gravity;
    (void)xoffset;
    (void)yoffset;
    return SDL_Unsupported();
}

int SDL_GetAndroidSDKVersion(void)
{
    return SDL_Unsupported();
}

bool SDL_IsChromebook(void)
{
    SDL_Unsupported();
    return false;
}

bool SDL_IsDeXMode(void)
{
    SDL_Unsupported();
    return false;
}

Sint32 JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void)vm;
    (void)reserved;
    return 0x00010004; // JNI_VERSION_1_4
}
#endif
