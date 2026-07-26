#include "SDL_ohosvulkan.h"
#include "SDL_internal.h"
#include <vulkan/vulkan_core.h>

#ifdef SDL_VIDEO_DRIVER_OHOS
#define VK_USE_PLATFORM_OHOS 1
#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_sysvideo.h"
#include "SDL_ohosvideo.h"
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_ohos.h"
#include <native_window/external_window.h>

static int loadedCount = 0;
bool OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    if (_this->vulkan_config.loader_handle) {
        return SDL_SetError("Vulkan already loaded");
    }

    /* Load the Vulkan loader library */
    if (!path) {
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    }
    if (!path) {
        path = "libvulkan.so";
    }
    _this->vulkan_config.loader_handle = SDL_LoadObject(path);
    if (!_this->vulkan_config.loader_handle) {
        return false;
    }
    SDL_strlcpy(_this->vulkan_config.loader_path, path,
                SDL_arraysize(_this->vulkan_config.loader_path));
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
        _this->vulkan_config.loader_handle, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) {
        goto fail;
    }
    _this->vulkan_config.vkGetInstanceProcAddr = (void *)vkGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (void *)((PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr)(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!_this->vulkan_config.vkEnumerateInstanceExtensionProperties) {
        goto fail;
    }
    loadedCount++;
    return true;

fail:
    SDL_UnloadObject(_this->vulkan_config.loader_handle);
    _this->vulkan_config.loader_handle = NULL;
    return false;
}

void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this)
{
    if (loadedCount == 0) {
        return;
    }
    loadedCount--;
    if (_this->vulkan_config.loader_handle && loadedCount == 0) {
        SDL_UnloadObject(_this->vulkan_config.loader_handle);
        _this->vulkan_config.loader_handle = NULL;
    }
}

char const *const *OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count)
{
    static const char *const extensionsForOHOS[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_OHOS_SURFACE_EXTENSION_NAME
    };
    if (count) {
        *count = SDL_arraysize(extensionsForOHOS);
    }
    return extensionsForOHOS;
}

bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this,
                               SDL_Window *window,
                               VkInstance instance,
                               const struct VkAllocationCallbacks *allocator,
                               VkSurfaceKHR *surface)
{
    VkResult result;

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr;
    PFN_vkCreateSurfaceOHOS vkCreateSurfaceOHOS =
        (PFN_vkCreateSurfaceOHOS)vkGetInstanceProcAddr(instance, "vkCreateSurfaceOHOS");
    VkSurfaceCreateInfoOHOS createInfo;

    if (!_this->vulkan_config.loader_handle) {
        SDL_SetError("Vulkan is not loaded");
        return false;
    }

    if (!vkCreateSurfaceOHOS) {
        SDL_SetError(VK_OHOS_SURFACE_EXTENSION_NAME
                     " extension is not enabled in the Vulkan instance.");
        return false;
    }

    SDL_zero(createInfo);
    createInfo.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = window->internal->native_window;
    result = vkCreateSurfaceOHOS(instance, &createInfo, NULL, surface);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateSurfaceOHOS failed: %d", result);
        return false;
    }
    return true;
}

void OHOS_Vulkan_DestroySurface(SDL_VideoDevice *_this,
                                VkInstance instance,
                                VkSurfaceKHR surface,
                                const struct VkAllocationCallbacks *allocator)
{
    if (_this->vulkan_config.loader_handle) {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
            (PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr;
        PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
        vkDestroySurfaceKHR(instance, surface, allocator);
    }
}

#endif
