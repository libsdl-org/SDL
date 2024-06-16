/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_VIDEO_DRIVER_X11

SDL_DECLSPEC void SDLCALL SDL_SetX11EventHook(SDL_X11EventHook callback, void *userdata);
void SDL_SetX11EventHook(SDL_X11EventHook callback, void *userdata)
{
}

#endif

#ifndef SDL_PLATFORM_LINUX

SDL_DECLSPEC int SDLCALL SDL_LinuxSetThreadPriority(Sint64 threadID, int priority);
int SDL_LinuxSetThreadPriority(Sint64 threadID, int priority)
{
    (void)threadID;
    (void)priority;
    return SDL_Unsupported();
}

SDL_DECLSPEC int SDLCALL SDL_LinuxSetThreadPriorityAndPolicy(Sint64 threadID, int sdlPriority, int schedPolicy);
int SDL_LinuxSetThreadPriorityAndPolicy(Sint64 threadID, int sdlPriority, int schedPolicy)
{
    (void)threadID;
    (void)sdlPriority;
    (void)schedPolicy;
    return SDL_Unsupported();
}

#endif

#ifndef SDL_PLATFORM_GDK

SDL_DECLSPEC void SDLCALL SDL_GDKSuspendComplete(void);
void SDL_GDKSuspendComplete(void)
{
    SDL_Unsupported();
}

SDL_DECLSPEC int SDLCALL SDL_GDKGetDefaultUser(void *outUserHandle); /* XUserHandle *outUserHandle */
int SDL_GDKGetDefaultUser(void *outUserHandle)
{
    return SDL_Unsupported();
}

#endif

#if !(defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINRT) || defined(SDL_PLATFORM_GDK))

SDL_DECLSPEC int SDLCALL SDL_RegisterApp(const char *name, Uint32 style, void *hInst);
int SDL_RegisterApp(const char *name, Uint32 style, void *hInst)
{
    (void)name;
    (void)style;
    (void)hInst;
    return SDL_Unsupported();
}

SDL_DECLSPEC void SDLCALL SDL_SetWindowsMessageHook(void *callback, void *userdata); /* SDL_WindowsMessageHook callback */
void SDL_SetWindowsMessageHook(void *callback, void *userdata)
{
    (void)callback;
    (void)userdata;
    SDL_Unsupported();
}

SDL_DECLSPEC void SDLCALL SDL_UnregisterApp(void);
void SDL_UnregisterApp(void)
{
    SDL_Unsupported();
}

#endif

#ifndef SDL_PLATFORM_WINRT

/* Returns SDL_WinRT_DeviceFamily enum */
SDL_DECLSPEC int SDLCALL SDL_WinRTGetDeviceFamily(void);
int SDL_WinRTGetDeviceFamily()
{
    SDL_Unsupported();
    return 0; /* SDL_WINRT_DEVICEFAMILY_UNKNOWN */
}

SDL_DECLSPEC const char *SDLCALL SDL_WinRTGetFSPath(int pathType); /* SDL_WinRT_Path pathType */
const char *SDL_WinRTGetFSPath(int pathType)
{
    (void)pathType;
    SDL_Unsupported();
    return NULL;
}
#endif

#ifndef SDL_PLATFORM_ANDROID

SDL_DECLSPEC void SDLCALL SDL_AndroidBackButton(void);
void SDL_AndroidBackButton()
{
    SDL_Unsupported();
}

SDL_DECLSPEC void *SDLCALL SDL_AndroidGetActivity(void);
void *SDL_AndroidGetActivity()
{
    SDL_Unsupported();
    return NULL;
}

SDL_DECLSPEC const char *SDLCALL SDL_AndroidGetExternalStoragePath(void);
const char* SDL_AndroidGetExternalStoragePath()
{
    SDL_Unsupported();
    return NULL;
}

SDL_DECLSPEC int SDLCALL SDL_AndroidGetExternalStorageState(Uint32 *state);
int SDL_AndroidGetExternalStorageState(Uint32 *state)
{
    (void)state;
    return SDL_Unsupported();
}
SDL_DECLSPEC const char *SDLCALL SDL_AndroidGetInternalStoragePath(void);
const char *SDL_AndroidGetInternalStoragePath(void)
{
    SDL_Unsupported();
    return NULL;
}

SDL_DECLSPEC void *SDLCALL SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetJNIEnv()
{
    SDL_Unsupported();
    return NULL;
}

typedef void (SDLCALL *SDL_AndroidRequestPermissionCallback)(void *userdata, const char *permission, SDL_bool granted);
SDL_DECLSPEC int SDLCALL SDL_AndroidRequestPermission(const char *permission, SDL_AndroidRequestPermissionCallback cb, void *userdata);
int SDL_AndroidRequestPermission(const char *permission, SDL_AndroidRequestPermissionCallback cb, void *userdata)
{
    (void)permission;
    (void)cb;
    (void)userdata;
    return SDL_Unsupported();
}

SDL_DECLSPEC int SDLCALL SDL_AndroidSendMessage(Uint32 command, int param);
int SDL_AndroidSendMessage(Uint32 command, int param)
{
    (void)command;
    (void)param;
    return SDL_Unsupported();
}

SDL_DECLSPEC int SDLCALL SDL_AndroidShowToast(const char* message, int duration, int gravity, int xoffset, int yoffset);
int SDL_AndroidShowToast(const char* message, int duration, int gravity, int xoffset, int yoffset)
{
    (void)message;
    (void)duration;
    (void)gravity;
    (void)xoffset;
    (void)yoffset;
    return SDL_Unsupported();
}

SDL_DECLSPEC int SDLCALL SDL_GetAndroidSDKVersion(void);
int SDL_GetAndroidSDKVersion()
{
    return SDL_Unsupported();
}

SDL_DECLSPEC SDL_bool SDLCALL SDL_IsAndroidTV(void);
SDL_bool SDL_IsAndroidTV()
{
    SDL_Unsupported();
    return SDL_FALSE;
}

SDL_DECLSPEC SDL_bool SDLCALL SDL_IsChromebook(void);
SDL_bool SDL_IsChromebook()
{
    SDL_Unsupported();
    return SDL_FALSE;
}

SDL_DECLSPEC SDL_bool SDLCALL SDL_IsDeXMode(void);
SDL_bool SDL_IsDeXMode(void)
{
    SDL_Unsupported();
    return SDL_FALSE;
}

SDL_DECLSPEC Sint32 SDLCALL JNI_OnLoad(void *vm, void *reserved);
Sint32 JNI_OnLoad(void *vm, void *reserved)
{
    (void)vm;
    (void)reserved;
    SDL_Unsupported();
    return -1; /* JNI_ERR */
}
#endif

#if defined(SDL_PLATFORM_XBOXONE) || defined(SDL_PLATFORM_XBOXSERIES)
char *SDL_GetUserFolder(SDL_Folder folder)
{
    (void)folder;
    SDL_Unsupported();
    return NULL;
}
#endif
