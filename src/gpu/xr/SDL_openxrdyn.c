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

#if defined(SDL_PLATFORM_APPLE)
#define SDL_GPU_OPENXR_DYNAMIC "libopenxr_loader.dylib"
#elif defined(SDL_PLATFORM_WINDOWS)
#define SDL_GPU_OPENXR_DYNAMIC "openxr_loader.dll"
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
#define SDL_GPU_OPENXR_DYNAMIC "libopenxr_loader.so"
#else
#define SDL_GPU_OPENXR_DYNAMIC "libopenxr_loader.so.1,libopenxr_loader.so"
#endif

#define DEBUG_DYNAMIC_OPENXR 0

typedef struct
{
    SDL_SharedObject *lib;
    const char *libnames;
} openxrdynlib;

static openxrdynlib openxr_loader = { NULL, SDL_GPU_OPENXR_DYNAMIC };

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
    
    SDL_Log("SDL/OpenXR: Got xrGetInstanceProcAddr from loader: %p", (void*)loaderGetProcAddr);
    
    /* Get xrInitializeLoaderKHR via xrGetInstanceProcAddr */
    result = loaderGetProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&initializeLoader);
    if (XR_FAILED(result) || initializeLoader == NULL) {
        SDL_SetError("Failed to get xrInitializeLoaderKHR (result: %d)", (int)result);
        return false;
    }
    
    SDL_Log("SDL/OpenXR: Got xrInitializeLoaderKHR: %p", (void*)initializeLoader);

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
    
    SDL_Log("SDL/OpenXR: xrInitializeLoaderKHR succeeded");

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
    SDL_Log("SDL/OpenXR: UnloadLibrary called, current refcount=%d", openxr_load_refcount);
    // Don't actually unload if more than one module is using the libs...
    if (openxr_load_refcount > 0) {
        if (--openxr_load_refcount == 0) {
            SDL_Log("SDL/OpenXR: Refcount reached 0, unloading library");

#ifdef SDL_PLATFORM_ANDROID
            /* On Android/Quest, don't actually unload the library or reset the loader state.
             * The Quest OpenXR runtime doesn't support being re-initialized after teardown.
             * xrInitializeLoaderKHR and xrNegotiateLoaderRuntimeInterface must only be called once.
             * We keep the library loaded and the loader initialized. 
             * 
             * IMPORTANT: We also keep xrGetInstanceProcAddr intact so we can reload other
             * function pointers on the next LoadLibrary call. Only NULL out the other symbols. */
            SDL_Log("SDL/OpenXR: Android - keeping library loaded and loader initialized");
            
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
        } else {
            SDL_Log("SDL/OpenXR: Refcount is now %d, not unloading", openxr_load_refcount);
        }
    }
}

// returns non-zero if all needed symbols were loaded.
SDL_DECLSPEC bool SDLCALL SDL_OpenXR_LoadLibrary(void)
{
    bool result = true;

    SDL_Log("SDL/OpenXR: LoadLibrary called, current refcount=%d, lib=%p", openxr_load_refcount, (void*)openxr_loader.lib);

    // deal with multiple modules (gpu, openxr, etc) needing these symbols...
    if (openxr_load_refcount++ == 0) {
#ifdef SDL_PLATFORM_ANDROID
        /* On Android, the library may already be loaded if this is a reload after
         * unload (we don't actually unload on Android to preserve runtime state) */
        if (openxr_loader.lib == NULL) {
#endif
        const char *paths_hint = SDL_GetHint(SDL_HINT_OPENXR_SONAMES);

        // If no hint was specified, use the default
        if (!paths_hint)
            paths_hint = openxr_loader.libnames;

        // dupe for strtok
        char *paths = SDL_strdup(paths_hint);

        char *strtok_state;
        // go over all the passed paths
        char *path = SDL_strtok_r(paths, ",", &strtok_state);
        while (path) {
            openxr_loader.lib = SDL_LoadObject(path);
            // if we found the lib, break out
            if (openxr_loader.lib) {
                break;
            }

            path = SDL_strtok_r(NULL, ",", &strtok_state);
        }

        SDL_free(paths);

        if (!openxr_loader.lib) {
            SDL_SetError("Failed loading OpenXR library from: %s", paths_hint);
            openxr_load_refcount--;
            return false;
        }
#ifdef SDL_PLATFORM_ANDROID
        } else {
            SDL_Log("SDL/OpenXR: Library already loaded (Android reload), skipping SDL_LoadObject");
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
        
        SDL_Log("SDL/OpenXR: Loading global functions...");
        
        /* First try to get functions directly from the forwardloader library */
        OPENXR_xrEnumerateApiLayerProperties = (PFN_xrEnumerateApiLayerProperties)SDL_LoadFunction(openxr_loader.lib, "xrEnumerateApiLayerProperties");
        OPENXR_xrCreateInstance = (PFN_xrCreateInstance)SDL_LoadFunction(openxr_loader.lib, "xrCreateInstance");
        OPENXR_xrEnumerateInstanceExtensionProperties = (PFN_xrEnumerateInstanceExtensionProperties)SDL_LoadFunction(openxr_loader.lib, "xrEnumerateInstanceExtensionProperties");
        
        SDL_Log("SDL/OpenXR: Direct symbols - xrEnumerateApiLayerProperties=%p, xrCreateInstance=%p, xrEnumerateInstanceExtensionProperties=%p",
                (void*)OPENXR_xrEnumerateApiLayerProperties, 
                (void*)OPENXR_xrCreateInstance,
                (void*)OPENXR_xrEnumerateInstanceExtensionProperties);
        
        /* If direct loading failed, fall back to xrGetInstanceProcAddr(NULL, ...) */
        if (OPENXR_xrEnumerateApiLayerProperties == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateApiLayerProperties", (PFN_xrVoidFunction*)&OPENXR_xrEnumerateApiLayerProperties);
            if (XR_FAILED(xrResult) || OPENXR_xrEnumerateApiLayerProperties == NULL) {
                SDL_Log("SDL/OpenXR: Failed to get xrEnumerateApiLayerProperties via xrGetInstanceProcAddr");
                failed = true;
            }
        }
        
        if (OPENXR_xrCreateInstance == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrCreateInstance", (PFN_xrVoidFunction*)&OPENXR_xrCreateInstance);
            if (XR_FAILED(xrResult) || OPENXR_xrCreateInstance == NULL) {
                SDL_Log("SDL/OpenXR: Failed to get xrCreateInstance via xrGetInstanceProcAddr");
                failed = true;
            }
        }
        
        if (OPENXR_xrEnumerateInstanceExtensionProperties == NULL) {
            xrResult = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties", (PFN_xrVoidFunction*)&OPENXR_xrEnumerateInstanceExtensionProperties);
            if (XR_FAILED(xrResult) || OPENXR_xrEnumerateInstanceExtensionProperties == NULL) {
                SDL_Log("SDL/OpenXR: Failed to get xrEnumerateInstanceExtensionProperties via xrGetInstanceProcAddr");
                failed = true;
            }
        }
        
        SDL_Log("SDL/OpenXR: Final symbols - xrEnumerateApiLayerProperties=%p, xrCreateInstance=%p, xrEnumerateInstanceExtensionProperties=%p",
                (void*)OPENXR_xrEnumerateApiLayerProperties, 
                (void*)OPENXR_xrCreateInstance,
                (void*)OPENXR_xrEnumerateInstanceExtensionProperties);
        
        SDL_Log("SDL/OpenXR: Global functions loading %s", failed ? "FAILED" : "succeeded");
#else
#define SDL_OPENXR_SYM(name) OPENXR_##name = (PFN_##name)OPENXR_GetSym(#name, &failed);
#include "SDL_openxrsym.h"
#endif

        if (failed) {
            // in case something got loaded...
            SDL_OpenXR_UnloadLibrary();
            result = false;
        }
    } else {
        SDL_Log("SDL/OpenXR: Library already loaded (refcount=%d), skipping", openxr_load_refcount);
    }

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
    SDL_SetError("OpenXR is not enabled in this build of SDL");
    return false;
}

SDL_DECLSPEC void SDLCALL SDL_OpenXR_UnloadLibrary(void)
{
    SDL_SetError("OpenXR is not enabled in this build of SDL");
}

SDL_DECLSPEC PFN_xrGetInstanceProcAddr SDLCALL SDL_OpenXR_GetXrGetInstanceProcAddr(void)
{
    SDL_SetError("OpenXR is not enabled in this build of SDL");

    return NULL;
}

#endif // HAVE_GPU_OPENXR
