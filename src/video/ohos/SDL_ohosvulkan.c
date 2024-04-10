/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SDL_ohosvulkan.h"

#if SDL_VIDEO_VULKAN && SDL_VIDEO_DRIVER_OHOS

#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_assert.h"

#include "SDL_loadso.h"
#include "SDL_syswm.h"

#include "../SDL_vulkan_internal.h"
#include "../SDL_sysvideo.h"

#include "../khronos/vulkan/vulkan_ohos.h"

int OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    Uint32 i = 0;
    Uint32 extensionCount = 0;
    SDL_bool hasSurfaceExtension = SDL_FALSE;
    SDL_bool hasOHOSSurfaceExtension = SDL_FALSE;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    if (_this->vulkan_config.loader_handle)
        return SDL_SetError("Vulkan already loaded");

    /* Load the Vulkan loader library */
    if (!path)
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    if (!path)
        path = "libvulkan.so";
    _this->vulkan_config.loader_handle = SDL_LoadObject(path);
    if (!_this->vulkan_config.loader_handle)
        return -1;
    SDL_strlcpy(_this->vulkan_config.loader_path, path,
                SDL_arraysize(_this->vulkan_config.loader_path));
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
        _this->vulkan_config.loader_handle, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr)
        goto fail;
    _this->vulkan_config.vkGetInstanceProcAddr = (void *)vkGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (void *)((PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr)(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!_this->vulkan_config.vkEnumerateInstanceExtensionProperties)
        goto fail;
    return 0;

fail:
    SDL_UnloadObject(_this->vulkan_config.loader_handle);
    _this->vulkan_config.loader_handle = NULL;
    return -1;
}

void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this)
{
    if (_this->vulkan_config.loader_handle)
    {
        SDL_UnloadObject(_this->vulkan_config.loader_handle);
        _this->vulkan_config.loader_handle = NULL;
    }
}

SDL_bool OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this,
                                           SDL_Window *window,
                                           unsigned *count,
                                           const char **names)
{
    static const char *const extensionsForOHOS[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_OHOS_SURFACE_EXTENSION_NAME
    };
    if (!_this->vulkan_config.loader_handle) {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }
    return SDL_Vulkan_GetInstanceExtensions_Helper(
            count, names, SDL_arraysize(extensionsForOHOS),
            extensionsForOHOS);
}

SDL_bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this,
                                   SDL_Window *window,
                                   VkInstance instance,
                                   VkSurfaceKHR *xcomponent)
{
    SDL_WindowData *windowData = (SDL_WindowData *)window->driverdata;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr;
    PFN_vkCreateOHOSSurfaceKHR vkCreateOHOSSurfaceKHR =
        (PFN_vkCreateOHOSSurfaceKHR)vkGetInstanceProcAddr(
                                           instance,
                                           "vkCreateSurfaceOHOS");
    VkOHOSSurfaceCreateInfoKHR createInfo;
    VkResult result;

    if (!_this->vulkan_config.loader_handle)
    {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }

    if (!vkCreateOHOSSurfaceKHR) {
        SDL_SetError(VK_KHR_OHOS_SURFACE_EXTENSION_NAME
                     " extension is not enabled in the Vulkan instance.");
        return SDL_FALSE;
    }
    SDL_zero(createInfo);
    createInfo.sType = VK_STRUCTURE_TYPE_OSOS_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = windowData->native_window;
    result = vkCreateOHOSSurfaceKHR(instance, &createInfo,
                                      NULL, xcomponent);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateOHOSSurfaceKHR failed: %s",
                     SDL_Vulkan_GetResultString(result));
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

#endif /* SDL_VIDEO_VULKAN && SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
