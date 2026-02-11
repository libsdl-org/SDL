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

#ifdef HAVE_GPU_OPENXR

#include "SDL_gpu_openxr.h"

#ifdef SDL_PLATFORM_ANDROID
#include "../../core/android/SDL_android.h"
#endif

#define VALIDATION_LAYER_API_NAME "XR_APILAYER_LUNARG_core_validation"

/* On Android, the OpenXR loader is initialized by SDL_OpenXR_LoadLibrary() in SDL_openxrdyn.c
 * which must be called before this. That function handles the complex initialization using
 * direct SDL_LoadFunction calls to avoid issues with xrGetInstanceProcAddr from runtime
 * negotiation not supporting pre-instance calls. This stub is kept for API compatibility. */
#ifdef SDL_PLATFORM_ANDROID
static bool SDL_OPENXR_INTERNAL_InitializeAndroidLoader(void)
{
    /* The loader should already be initialized by SDL_OpenXR_LoadLibrary().
     * We just verify that xrGetInstanceProcAddr is available. */
    if (xrGetInstanceProcAddr == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "xrGetInstanceProcAddr is NULL - SDL_OpenXR_LoadLibrary was not called first");
        return false;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Android OpenXR loader verified (was initialized by SDL_OpenXR_LoadLibrary)");
    return true;
}
#endif /* SDL_PLATFORM_ANDROID */

static bool SDL_OPENXR_INTERNAL_ValidationLayerAvailable(void)
{
#ifdef SDL_PLATFORM_ANDROID
    /* On Android/Quest, the xrGetInstanceProcAddr obtained through runtime negotiation
     * crashes when used for pre-instance global functions. Skip validation layer check. */
    return false;
#else

    Uint32 apiLayerCount;
    if (XR_FAILED(xrEnumerateApiLayerProperties(0, &apiLayerCount, NULL))) {
        return false;
    }

    if (apiLayerCount <= 0) {
        return false;
    }

    XrApiLayerProperties *apiLayerProperties = SDL_stack_alloc(XrApiLayerProperties, apiLayerCount);
    if (XR_FAILED(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties))) {
        SDL_stack_free(apiLayerProperties);
        return false;
    }

    bool found = false;
    for (Uint32 i = 0; i < apiLayerCount; i++) {
        XrApiLayerProperties apiLayer = apiLayerProperties[i];
        SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "api layer available: %s", apiLayer.layerName);
        if (SDL_strcmp(apiLayer.layerName, VALIDATION_LAYER_API_NAME) == 0) {
            found = true;
            break;
        }
    }

    SDL_stack_free(apiLayerProperties);
    return found;
#endif
}

XrResult SDL_OPENXR_INTERNAL_GPUInitOpenXR(
    bool debugMode,
    XrExtensionProperties gpuExtension,
    SDL_PropertiesID props,
    XrInstance *instance,
    XrSystemId *systemId,
    XrInstancePfns **xr)
{
    XrResult xrResult;

#ifdef SDL_PLATFORM_ANDROID
    // Android requires loader initialization before any other XR calls
    if (!SDL_OPENXR_INTERNAL_InitializeAndroidLoader()) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Failed to initialize Android OpenXR loader");
        return XR_ERROR_INITIALIZATION_FAILED;
    }
#endif

    bool validationLayersAvailable = SDL_OPENXR_INTERNAL_ValidationLayerAvailable();

    Uint32 userApiLayerCount = (Uint32)SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_LAYER_COUNT_NUMBER, 0);
    const char *const *userApiLayerNames = SDL_GetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_LAYER_NAMES_POINTER, NULL);

    Uint32 userExtensionCount = (Uint32)SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_COUNT_NUMBER, 0);
    const char *const *userExtensionNames = SDL_GetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_EXTENSION_NAMES_POINTER, NULL);

    // allocate enough space for the validation layer + the user's api layers
    const char **apiLayerNames = SDL_stack_alloc(const char *, userApiLayerCount + 1);
    SDL_memcpy((void *)apiLayerNames, userApiLayerNames, sizeof(const char *) * (userApiLayerCount));
    apiLayerNames[userApiLayerCount] = VALIDATION_LAYER_API_NAME;

    // On Android, we need an extra extension for android_create_instance
#ifdef SDL_PLATFORM_ANDROID
    const Uint32 platformExtensionCount = 2; // GPU extension + Android create instance
#else
    const Uint32 platformExtensionCount = 1; // GPU extension only
#endif

    const char **extensionNames = SDL_stack_alloc(const char *, userExtensionCount + platformExtensionCount);
    SDL_memcpy((void *)extensionNames, userExtensionNames, sizeof(const char *) * (userExtensionCount));
    extensionNames[userExtensionCount] = gpuExtension.extensionName;
#ifdef SDL_PLATFORM_ANDROID
    extensionNames[userExtensionCount + 1] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
#endif

    XrInstanceCreateInfo xrInstanceCreateInfo;
    SDL_zero(xrInstanceCreateInfo);
    xrInstanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
    xrInstanceCreateInfo.applicationInfo.apiVersion = SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_VERSION_NUMBER, XR_API_VERSION_1_0);
    xrInstanceCreateInfo.enabledApiLayerCount = userApiLayerCount + ((debugMode && validationLayersAvailable) ? 1 : 0); // in debug mode, we enable the validation layer
    xrInstanceCreateInfo.enabledApiLayerNames = apiLayerNames;
    xrInstanceCreateInfo.enabledExtensionCount = userExtensionCount + platformExtensionCount;
    xrInstanceCreateInfo.enabledExtensionNames = extensionNames;

#ifdef SDL_PLATFORM_ANDROID
    // Get JNI environment and JavaVM for Android instance creation
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    JavaVM *vm = NULL;
    if (env) {
        (*env)->GetJavaVM(env, &vm);
    }
    void *activity = SDL_GetAndroidActivity();

    XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid = {};
    instanceCreateInfoAndroid.type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR;
    instanceCreateInfoAndroid.applicationVM = vm;
    instanceCreateInfoAndroid.applicationActivity = activity;
    xrInstanceCreateInfo.next = &instanceCreateInfoAndroid;
#endif

    const char *applicationName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME_STRING, "SDL Application");
    Uint32 applicationVersion = (Uint32)SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_VERSION_NUMBER, 0);

    SDL_strlcpy(xrInstanceCreateInfo.applicationInfo.applicationName, applicationName, XR_MAX_APPLICATION_NAME_SIZE);
    xrInstanceCreateInfo.applicationInfo.applicationVersion = applicationVersion;

    const char *engineName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENGINE_NAME_STRING, "SDLGPU");
    uint32_t engineVersion = (uint32_t)SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENGINE_VERSION_NUMBER, SDL_VERSION);

    SDL_strlcpy(xrInstanceCreateInfo.applicationInfo.engineName, engineName, XR_MAX_APPLICATION_NAME_SIZE);
    xrInstanceCreateInfo.applicationInfo.engineVersion = engineVersion;

    if ((xrResult = xrCreateInstance(&xrInstanceCreateInfo, instance)) != XR_SUCCESS) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Failed to create OpenXR instance");
        SDL_stack_free(apiLayerNames);
        SDL_stack_free(extensionNames);
        return false;
    }

    SDL_stack_free(apiLayerNames);
    SDL_stack_free(extensionNames);

    *xr = SDL_OPENXR_LoadInstanceSymbols(*instance);
    if (!*xr) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Failed to load required OpenXR instance symbols");
        /* NOTE: we can't actually destroy the created OpenXR instance here,
                as we only get that function pointer by loading the instance symbols...
                let's just hope that doesn't happen. */
        return false;
    }

    XrSystemGetInfo systemGetInfo;
    SDL_zero(systemGetInfo);
    systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
    systemGetInfo.formFactor = (XrFormFactor)SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_FORM_FACTOR_NUMBER, XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);
    if ((xrResult = (*xr)->xrGetSystem(*instance, &systemGetInfo, systemId)) != XR_SUCCESS) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Failed to get OpenXR system");
        (*xr)->xrDestroyInstance(*instance);
        SDL_free(*xr);
        return false;
    }

    return true;
}

#endif /* HAVE_GPU_OPENXR */
