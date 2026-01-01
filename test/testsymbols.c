/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test for availability of ALL SDL3 symbols */

#define SDL_DISABLE_ANALYZE_MACROS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#if !defined(SDL_PLATFORM_ANDROID)
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidActivity(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidCachePath(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidExternalStoragePath(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidExternalStorageState(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidInternalStoragePath(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidJNIEnv(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetAndroidSDKVersion(void);
extern SDL_DECLSPEC void SDLCALL SDL_IsChromebook(void);
extern SDL_DECLSPEC void SDLCALL SDL_IsDeXMode(void);
extern SDL_DECLSPEC void SDLCALL SDL_RequestAndroidPermission(void);
extern SDL_DECLSPEC void SDLCALL SDL_SendAndroidBackButton(void);
extern SDL_DECLSPEC void SDLCALL SDL_SendAndroidMessage(void);
extern SDL_DECLSPEC void SDLCALL SDL_ShowAndroidToast(void);
#endif

#if !defined(SDL_PLATFORM_GDK)
extern SDL_DECLSPEC void SDLCALL SDL_GDKResumeGPU(void);
extern SDL_DECLSPEC void SDLCALL SDL_GDKSuspendGPU(void);
extern SDL_DECLSPEC void SDLCALL SDL_GDKSuspendComplete(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetGDKDefaultUser(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetGDKTaskQueue(void);
#endif

#if !defined(SDL_PLATFORM_IOS)
extern SDL_DECLSPEC void SDLCALL SDL_OnApplicationDidChangeStatusBarOrientation(void);
extern SDL_DECLSPEC void SDLCALL SDL_SetiOSAnimationCallback(void);
extern SDL_DECLSPEC void SDLCALL SDL_SetiOSEventPump(void);
#endif

#if !defined(SDL_PLATFORM_LINUX)
extern SDL_DECLSPEC void SDLCALL SDL_SetLinuxThreadPriority(void);
extern SDL_DECLSPEC void SDLCALL SDL_SetLinuxThreadPriorityAndPolicy(void);
#endif

#if !(defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK))
extern SDL_DECLSPEC void SDLCALL SDL_GetDXGIOutputInfo(void);
extern SDL_DECLSPEC void SDLCALL SDL_GetDirect3D9AdapterIndex(void);
#endif

#if !defined(SDL_PLATFORM_WINDOWS)
extern SDL_DECLSPEC void SDLCALL SDL_RegisterApp(void);
extern SDL_DECLSPEC void SDLCALL SDL_UnregisterApp(void);
extern SDL_DECLSPEC void SDLCALL SDL_SetWindowsMessageHook(void);
#endif

extern SDL_DECLSPEC void SDLCALL JNI_OnLoad(void);

const static struct {
    const char *name;
    SDL_FunctionPointer address;
} sdl_symbols[] = {
    #define SDL_DYNAPI_PROC(rc, fn, params, args, ret) { #fn, (SDL_FunctionPointer)fn },
    #include "../src/dynapi/SDL_dynapi_procs.h"
    #undef SDL_DYNAPI_PROC
    { NULL, NULL }
};

static void print_usage(const char *argv0)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s [number [number] ...]\n", argv0);
}

int main(int argc, char *argv[])
{
    static const int count_sdl_symbols = (int)SDL_arraysize(sdl_symbols) - 1;
    int i;
    int result = 0;
    SDL_Log("There are %d SDL3 symbols", count_sdl_symbols);
    for (i = 1; i < argc; i++) {
        Sint64 symbol_index = -1;
        char *endp = NULL;
        symbol_index = (Sint64)SDL_strtol(argv[i], &endp, 10);
        if (*endp != '\0') {
            print_usage(argv[0]);
            return 1;
        }
        if (symbol_index < 0 || symbol_index >= count_sdl_symbols) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Index %" SDL_PRIs64 " is out of range", symbol_index);
            result = 1;
            continue;
        }
        SDL_Log("Address of %s is %p", sdl_symbols[symbol_index].name, sdl_symbols[symbol_index].address);
    }
    return result;
}
