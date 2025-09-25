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

#ifndef SDL_VIDEO_DRIVER_X11
extern SDL_DECLSPEC void SDLCALL SDL_SetX11EventHook(SDL_X11EventHook callback, void *userdata);
#endif

#ifndef SDL_PLATFORM_LINUX
extern SDL_DECLSPEC bool SDLCALL SDL_SetLinuxThreadPriority(Sint64 threadID, int priority);
extern SDL_DECLSPEC bool SDLCALL SDL_SetLinuxThreadPriorityAndPolicy(Sint64 threadID, int sdlPriority, int schedPolicy);
#endif

#if !defined(SDL_PLATFORM_GDK)
typedef struct XUserHandle XUserHandle;

extern SDL_DECLSPEC void SDLCALL SDL_GDKSuspendComplete(void);
extern SDL_DECLSPEC bool SDLCALL SDL_GetGDKDefaultUser(XUserHandle *outUserHandle);
extern SDL_DECLSPEC void SDLCALL SDL_GDKSuspendGPU(SDL_GPUDevice *device);
extern SDL_DECLSPEC void SDLCALL SDL_GDKResumeGPU(SDL_GPUDevice *device);
#endif /* !SDL_PLATFORM_GDK */

#if !defined(SDL_PLATFORM_WINDOWS)
extern SDL_DECLSPEC bool SDLCALL SDL_RegisterApp(const char *name, Uint32 style, void *hInst);
extern SDL_DECLSPEC void SDLCALL SDL_SetWindowsMessageHook(void *callback, void *userdata);
extern SDL_DECLSPEC void SDLCALL SDL_UnregisterApp(void);
#endif /* !SDL_PLATFORM_WINDOWS */

#if !defined(SDL_PLATFORM_ANDROID)

typedef void *SDL_RequestAndroidPermissionCallback;
typedef void *JavaVM;

extern SDL_DECLSPEC void SDLCALL SDL_SendAndroidBackButton(void);
extern SDL_DECLSPEC void * SDLCALL SDL_GetAndroidActivity(void);
extern SDL_DECLSPEC const char * SDLCALL SDL_GetAndroidCachePath(void);
extern SDL_DECLSPEC const char * SDLCALL SDL_GetAndroidExternalStoragePath(void);
extern SDL_DECLSPEC Uint32 SDLCALL SDL_GetAndroidExternalStorageState(void);
extern SDL_DECLSPEC const char * SDLCALL SDL_GetAndroidInternalStoragePath(void);
extern SDL_DECLSPEC void * SDLCALL SDL_GetAndroidJNIEnv(void);
extern SDL_DECLSPEC bool SDLCALL SDL_RequestAndroidPermission(const char *permission, SDL_RequestAndroidPermissionCallback cb, void *userdata);
extern SDL_DECLSPEC bool SDLCALL SDL_SendAndroidMessage(Uint32 command, int param);
extern SDL_DECLSPEC bool SDLCALL SDL_ShowAndroidToast(const char *message, int duration, int gravity, int xoffset, int yoffset);
extern SDL_DECLSPEC int SDLCALL SDL_GetAndroidSDKVersion(void);
extern SDL_DECLSPEC bool SDLCALL SDL_IsChromebook(void);
extern SDL_DECLSPEC bool SDLCALL SDL_IsDeXMode(void);
extern SDL_DECLSPEC Sint32 SDLCALL JNI_OnLoad(JavaVM *vm, void *reserved);
#endif /* !SDL_PLATFORM_ANDROID */
