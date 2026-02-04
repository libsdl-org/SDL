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

#include "SDL_openxrdyn.h"

#ifdef HAVE_GPU_OPENXR

#include <SDL3/SDL_dlopennote.h>
#include <SDL3/SDL_openxr.h>

#if defined(SDL_PLATFORM_APPLE)
static const char *openxr_library_names[] = { "libopenxr_loader.dylib", NULL };
#elif defined(SDL_PLATFORM_WINDOWS)
static const char *openxr_library_names[] = { "openxr_loader.dll", NULL };
#elif defined(SDL_PLATFORM_ANDROID)
/* On Android, use the Khronos OpenXR loader (libopenxr_loader.so) which properly
 * exports xrGetInstanceProcAddr. This is bundled via the Gradle dependency:
 *   implementation 'org.khronos.openxr:openxr_loader_for_android:X.Y.Z'
 * 
 * The Khronos loader handles runtime discovery internally via the Android broker
 * pattern and properly supports all pre-instance global functions.
 * 
 * Note: Do NOT use Meta's forwardloader (libopenxr_forwardloader.so) - it doesn't
 * export xrGetInstanceProcAddr directly and the function obtained via runtime
 * negotiation crashes on pre-instance calls (e.g., xrEnumerateApiLayerProperties). */
static const char *openxr_library_names[] = { "libopenxr_loader.so", NULL };
#else
static const char *openxr_library_names[] = { "libopenxr_loader.so.1", "libopenxr_loader.so", NULL };
SDL_ELF_NOTE_DLOPEN(
    "gpu-openxr",
    "Support for OpenXR with SDL_GPU rendering",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    "libopenxr_loader.so.1", "libopenxr_loader.so"
)
#endif

#define DEBUG_DYNAMIC_OPENXR 0

typedef struct
{
    SDL_SharedObject *lib;
} openxrdynlib;

static openxrdynlib openxr_loader = { NULL };

#ifndef SDL_PLATFORM_ANDROID
static void *OPENXR_GetSym(const char *fnname, bool *failed)
{
    void *fn = SDL_LoadFunction(openxr_loader.lib, fnname);

#if DEBUG_DYNAMIC_OPENXR
    if (fn) {
        SDL_Log("OPENXR: Found '%s' in %s (%p)\n", fnname, dynlib->libname, fn);
    } else {
        SDL_Log("OPENXR: Symbol '%s' NOT FOUND!\n", fnname);
    }
#endif

    return fn;
}
#endif

// Define all the function pointers and wrappers...
#define SDL_OPENXR_SYM(name) PFN_##name OPENXR_##name = NULL;
#include "SDL_openxrsym.h"

static int openxr_load_refcount = 0;

#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#include "../../video/khronos/openxr/openxr_platform.h"

/* On Android, we need to initialize the loader with JNI context before use */
static bool openxr_android_loader_initialized = false;

static bool OPENXR_InitializeAndroidLoader(void)
{
    XrResult result;
    PFN_xrInitializeLoaderKHR initializeLoader = NULL;
    PFN_xrGetInstanceProcAddr loaderGetProcAddr = NULL;
    JNIEnv *env = NULL;
    JavaVM *vm = NULL;
    jobject activity = NULL;
    
    if (openxr_android_loader_initialized) {
        return true;
    }

    /* The Khronos OpenXR loader (libopenxr_loader.so) properly exports xrGetInstanceProcAddr.
     * Get it directly from the library - this is the standard approach. */
    loaderGetProcAddr = (PFN_xrGetInstanceProcAddr)SDL_LoadFunction(openxr_loader.lib, "xrGetInstanceProcAddr");
    
    if (loaderGetProcAddr == NULL) {
        SDL_SetError("Failed to get xrGetInstanceProcAddr from OpenXR loader. "
                     "Make sure you're using the Khronos loader (libopenxr_loader.so), "
                     "not Meta's forwardloader.");
        return false;
    }

#if DEBUG_DYNAMIC_OPENXR
    SDL_Log("SDL/OpenXR: Got xrGetInstanceProcAddr from loader: %p", (void*)loaderGetProcAddr);
#endif

    /* Get xrInitializeLoaderKHR via xrGetInstanceProcAddr */
    result = loaderGetProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&initializeLoader);
    if (XR_FAILED(result) || initializeLoader == NULL) {
        SDL_SetError("Failed to get xrInitializeLoaderKHR (result: %d)", (int)result);
        return false;
    }

#if DEBUG_DYNAMIC_OPENXR
    SDL_Log("SDL/OpenXR: Got xrInitializeLoaderKHR: %p", (void*)initializeLoader);
#endif

    /* Get Android environment info from SDL */
    env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (!env) {
        SDL_SetError("Failed to get Android JNI environment");
        return false;
    }

    if ((*env)->GetJavaVM(env, &vm) != 0) {
        SDL_SetError("Failed to get JavaVM from JNIEnv");
        return false;
    }

    activity = (jobject)SDL_GetAndroidActivity();
    if (!activity) {
        SDL_SetError("Failed to get Android activity");
        return false;
    }

    XrLoaderInitInfoAndroidKHR loaderInitInfo = {
        .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
        .next = NULL,
        .applicationVM = vm,
        .applicationContext = activity
    };

    result = initializeLoader((XrLoaderInitInfoBaseHeaderKHR *)&loaderInitInfo);
    if (XR_FAILED(result)) {
        SDL_SetError("xrInitializeLoaderKHR failed with result %d", (int)result);
        return false;
    }

#if DEBUG_DYNAMIC_OPENXR
    SDL_Log("SDL/OpenXR: xrInitializeLoaderKHR succeeded");
#endif

    /* Store the xrGetInstanceProcAddr function - this one properly handles
     * all pre-instance calls (unlike Meta's forwardloader runtime negotiation) */
    OPENXR_xrGetInstanceProcAddr = loaderGetProcAddr;
    xrGetInstanceProcAddr = loaderGetProcAddr;
    
    openxr_android_loader_initialized = true;
    return true;
}
#endif /* SDL_PLATFORM_ANDROID */

SDL_DECLSPEC void SDLCALL SDL_OpenXR_UnloadLibrary(void)
{
#if DEBUG_DYNAMIC_OPENXR
    SDL_Log("SDL/OpenXR: UnloadLibrary called, current refcount=%d", openxr_load_refcount);
#endif

    // Don't actually unload if more than one module is using the libs...
    if (openxr_load_refcount > 0) {
        if (--openxr_load_refcount == 0) {

#if DEBUG_DYNAMIC_OPENXR
            SDL_Log("SDL/OpenXR: Refcount reached 0, unloading library");
#endif

#ifdef SDL_PLATFORM_ANDROID
            /* On Android/Quest, don't actually unload the library or reset the loader state.
             * The Quest OpenXR runtime doesn't support being re-initialized after teardown.
             * xrInitializeLoaderKHR and xrNegotiateLoaderRuntimeInterface must only be called once.
             * We keep the library loaded and the loader initialized. 
             * 
             * IMPORTANT: We also keep xrGetInstanceProcAddr intact so we can reload other
             * function pointers on the next LoadLibrary call. Only NULL out the other symbols. */
#if DEBUG_DYNAMIC_OPENXR
            SDL_Log("SDL/OpenXR: Android - keeping library loaded and loader initialized");
#endif
            
            // Only NULL out non-essential function pointers, keep xrGetInstanceProcAddr
#define SDL_OPENXR_SYM(name) \
    if (SDL_strcmp(#name, "xrGetInstanceProcAddr") != 0) { \
        OPENXR_##name = NULL; \
    }
#include "SDL_openxrsym.h"
#else
            // On non-Android, NULL everything and unload
#define SDL_OPENXR_SYM(name) OPENXR_##name = NULL;
#include "SDL_openxrsym.h"

            SDL_UnloadObject(openxr_loader.lib);
            openxr_loader.lib = NULL;
#endif
        }
#if DEBUG_DYNAMIC_OPENXR
        else {
            SDL_Log("SDL/OpenXR: Refcount is now %d, not unloading", openxr_load_refcount);
        }
#endif
    }
}

// returns non-zero if all needed symbols were loaded.
SDL_DECLSPEC bool SDLCALL SDL_OpenXR_LoadLibrary(void)
{
    bool result = true;

#if DEBUG_DYNAMIC_OPENXR
    SDL_Log("SDL/OpenXR: LoadLibrary called, current refcount=%d, lib=%p", openxr_load_refcount, (void*)openxr_loader.lib);
#endif

    // deal with multiple modules (gpu, openxr, etc) needing these symbols...
    if (openxr_load_refcount++ == 0) {
#ifdef SDL_PLATFORM_ANDROID
        /* On Android, the library may already be loaded if this is a reload after
         * unload (we don't actually unload on Android to preserve runtime state) */
        if (openxr_loader.lib == NULL) {
#endif
        const char *path_hint = SDL_GetHint(SDL_HINT_OPENXR_LIBRARY);

        // If a hint was specified, try that first
        if (path_hint && *path_hint) {
            openxr_loader.lib = SDL_LoadObject(path_hint);
        }

        // If no hint or hint failed, try the default library names
        if (!openxr_loader.lib) {
            for (int i = 0; openxr_library_names[i] != NULL; i++) {
                openxr_loader.lib = SDL_LoadObject(openxr_library_names[i]);
                if (openxr_loader.lib) {
                    break;
                }
            }
        }

        if (!openxr_loader.lib) {
            SDL_SetError("Failed to load OpenXR loader library. "
                         "On Windows, ensure openxr_loader.dll is in your application directory or PATH. "
                         "On Linux, install the OpenXR loader package (libopenxr-loader) or set LD_LIBRARY_PATH. "
                         "You can also use the SDL_HINT_OPENXR_LIBRARY hint to specify the loader path.");
            openxr_load_refcount--;
            return false;
        }
#if defined(SDL_PLATFORM_ANDROID)
        } else {
#if DEBUG_DYNAMIC_OPENXR
            SDL_Log("SDL/OpenXR: Library already loaded (Android reload), skipping SDL_LoadObject");
#endif
        }
#endif

#ifdef SDL_PLATFORM_ANDROID
        /* On Android, we need to initialize the loader before other functions work.
         * OPENXR_InitializeAndroidLoader() will return early if already initialized. */
        if (!OPENXR_InitializeAndroidLoader()) {
            SDL_UnloadObject(openxr_loader.lib);
            openxr_loader.lib = NULL;
            openxr_load_refcount--;
            return false;
        }
#endif

        bool failed = false;

#ifdef SDL_PLATFORM_ANDROID
        /* On Android with Meta's forwardloader, we need special handling.
         * After calling xrInitializeLoaderKHR, the global functions should be available
         * either as direct exports from the forwardloader or via xrGetInstanceProcAddr(NULL, ...).
         * 
         * Try getting functions directly from the forwardloader first since they'll go
         * through the proper forwarding path. */
        XrResult xrResult;

#if DEBUG_DYNAMIC_OPENXR
        SDL_Log("SDL/OpenXR: Loading global functions...");
#endif
        
        /* First try to get functions directly from the forwardloader library */
        OPENXR_xrEnumerateApiLayerProperties = (PFN_xrEnumerateApiLayerProperties)SDL_LoadFunction(openxr_loader.lib, "xrEnumerateApiLayerProperties");
        OPENXR_xrCreateInstance = (PFN_xrCreateInstance)SDL_LoadFunction(openxr_loader.lib, "xrCreateInstance");
        OPENXR_xrEnumerateInstanceExtensionProperties = (PFN_xrEnumerateInstanceExtensionProperties)SDL_LoadFunction(openxr_loader.lib, "xrEnumerateInstanceExtensionProperties");

#if DEBUG_DYNAMIC_OPENXR
        SDL_Log("SDL/OpenXR: Direct symbols - xrEnumerateApiLayerProperties=%p, xrCreateInstance=%p, xrEnumerateInstanceExtensionProperties=%p",
                (void*)OPENXR_xrEnumerateApiLayerProperties, 
                (void*)OPENXR_xrCreateInstance,
                (void*)OPENXR_xrEnumerateInstanceExtensionProperties);
#endif
        
        /* If direct loading failed, fall back to xrGetInstanceProcAddr(NULL, ...) */
        if (OPENXR_xrEnumerateApiLayerProperties == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateApiLayerProperties", (PFN_xrVoidFunction*)&OPENXR_xrEnumerateApiLayerProperties);
            if (XR_FAILED(xrResult) || OPENXR_xrEnumerateApiLayerProperties == NULL) {
#if DEBUG_DYNAMIC_OPENXR
                SDL_Log("SDL/OpenXR: Failed to get xrEnumerateApiLayerProperties via xrGetInstanceProcAddr");
#endif
                failed = true;
            }
        }
        
        if (OPENXR_xrCreateInstance == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction*)&OPENXR_xrCreateInstance);
            if (XR_FAILED(xrResult) || OPENXR_xrCreateInstance == NULL) {
#if DEBUG_DYNAMIC_OPENXR
                SDL_Log("SDL/OpenXR: Failed to get xrCreateInstance via xrGetInstanceProcAddr");
#endif
                failed = true;
            }
        }
        
        if (OPENXR_xrEnumerateInstanceExtensionProperties == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction*)&OPENXR_xrEnumerateInstanceExtensionProperties);
            if (XR_FAILED(xrResult) || OPENXR_xrEnumerateInstanceExtensionProperties == NULL) {
#if DEBUG_DYNAMIC_OPENXR
                SDL_Log("SDL/OpenXR: Failed to get xrEnumerateInstanceExtensionProperties via xrGetInstanceProcAddr");
#endif
                failed = true;
            }
        }

#if DEBUG_DYNAMIC_OPENXR
        SDL_Log("SDL/OpenXR: Final symbols - xrEnumerateApiLayerProperties=%p, xrCreateInstance=%p, xrEnumerateInstanceExtensionProperties=%p",
                (void*)OPENXR_xrEnumerateApiLayerProperties, 
                (void*)OPENXR_xrCreateInstance,
                (void*)OPENXR_xrEnumerateInstanceExtensionProperties);
        
        SDL_Log("SDL/OpenXR: Global functions loading %s", failed ? "FAILED" : "succeeded");
#endif
#else
#define SDL_OPENXR_SYM(name) OPENXR_##name = (PFN_##name)OPENXR_GetSym(#name, &failed);
#include "SDL_openxrsym.h"
#endif

        if (failed) {
            // in case something got loaded...
            SDL_OpenXR_UnloadLibrary();
            result = false;
        }
    }
#if DEBUG_DYNAMIC_OPENXR
    else {
        SDL_Log("SDL/OpenXR: Library already loaded (refcount=%d), skipping", openxr_load_refcount);
    }
#endif

    return result;
}

SDL_DECLSPEC PFN_xrGetInstanceProcAddr SDLCALL SDL_OpenXR_GetXrGetInstanceProcAddr(void)
{
    if (xrGetInstanceProcAddr == NULL) {
        SDL_SetError("The OpenXR loader has not been loaded");
    }

    return xrGetInstanceProcAddr;
}

XrInstancePfns *SDL_OPENXR_LoadInstanceSymbols(XrInstance instance)
{
    XrResult result;

    XrInstancePfns *pfns = SDL_calloc(1, sizeof(XrInstancePfns));

#define SDL_OPENXR_INSTANCE_SYM(name)                                                   \
    result = xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction *)&pfns->name); \
    if (result != XR_SUCCESS) {                                                         \
        SDL_free(pfns);                                                                 \
        return NULL;                                                                    \
    }
#include "SDL_openxrsym.h"

    return pfns;
}

#else

SDL_DECLSPEC bool SDLCALL SDL_OpenXR_LoadLibrary(void)
{
    return SDL_SetError("OpenXR is not enabled in this build of SDL");
}

SDL_DECLSPEC void SDLCALL SDL_OpenXR_UnloadLibrary(void)
{
    SDL_SetError("OpenXR is not enabled in this build of SDL");
}

SDL_DECLSPEC PFN_xrGetInstanceProcAddr SDLCALL SDL_OpenXR_GetXrGetInstanceProcAddr(void)
{
    return (PFN_xrGetInstanceProcAddr)SDL_SetError("OpenXR is not enabled in this build of SDL");
}

#endif // HAVE_GPU_OPENXR
