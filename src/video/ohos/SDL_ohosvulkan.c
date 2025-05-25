#include "SDL_internal.h"
#if defined(SDL_VIDEO_VULKAN)
#include "../SDL_vulkan_internal.h"

#include "SDL_ohosvideo.h"
// #include "SDL_ohoswindow.h"

#include "../core/ohos/SDL_ohos.h"
#include "SDL_loadso.h"
#include "SDL_ohosvulkan.h"

static int loadedCount = 0;
int OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    Uint32 i = 0;
    Uint32 extensionCount = 0;
    SDL_bool hasXComponentExtension = SDL_FALSE;
    SDL_bool hasOHOSXComponentExtension = SDL_FALSE;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    if (loadedCount == 1) {
        return 0;
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
        return -1;
    }
    loadedCount++;
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
    return 0;

fail:
    SDL_UnloadObject(_this->vulkan_config.loader_handle);
    _this->vulkan_config.loader_handle = NULL;
    loadedCount--;
    return -1;
}

void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this)
{
    loadedCount--;
    if (_this->vulkan_config.loader_handle && loadedCount == 0) {
        SDL_UnloadObject(_this->vulkan_config.loader_handle);
        _this->vulkan_config.loader_handle = NULL;
    }
}

SDL_bool OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, SDL_Window *window, unsigned *count,
                                           const char **names)
{
    static const char *const extensionsForOHOS[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_OHOS_XCOMPONENT_EXTENSION_NAME
    };
    if (!_this->vulkan_config.loader_handle) {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }
    return SDL_Vulkan_GetInstanceExtensions_Helper(
        count, names, SDL_arraysize(extensionsForOHOS), extensionsForOHOS);
}

SDL_bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, VkInstance instance,
                                   VkSurfaceKHR *xcomponent)
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr;
    PFN_vkCreateOHOSXComponentKHR vkCreateOHOSXComponentKHR =
        (PFN_vkCreateOHOSXComponentKHR)vkGetInstanceProcAddr(instance, "vkCreateSurfaceOHOS");
    VkOHOSXComponentCreateInfoKHR createInfo;
    VkResult result;

    if (!_this->vulkan_config.loader_handle) {
        SDL_SetError("Vulkan is not loaded");
        return SDL_FALSE;
    }

    if (!vkCreateOHOSXComponentKHR) {
        SDL_SetError(VK_KHR_OHOS_XCOMPONENT_EXTENSION_NAME
                     " extension is not enabled in the Vulkan instance.");
        return SDL_FALSE;
    }
    SDL_zero(createInfo);
    createInfo.sType = VK_STRUCTURE_TYPE_OHOS_XCOMPONENT_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = nativeWindow;
    result = vkCreateOHOSXComponentKHR(instance, &createInfo, NULL, xcomponent);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateOHOSXcomponentKHR failed: %s",
                     SDL_Vulkan_GetResultString(result));
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

void OHOS_Vulkan_DestroySurface(SDL_VideoDevice *_this,
                                VkInstance instance,
                                VkSurfaceKHR surface,
                                const struct VkAllocationCallbacks *allocator)
{
    if (_this->vulkan_config.loader_handle) {
        SDL_Vulkan_DestroySurface_Internal(_this->vulkan_config.vkGetInstanceProcAddr, instance, surface, allocator);
    }
}

#endif
