#include "SDL_ohosvulkan.h"
#include "SDL_internal.h"
#include "../khronos/vulkan/vulkan.h"

#ifdef SDL_VIDEO_DRIVER_OHOS

static int loadedCount = 0;
bool OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = NULL;
    if (_this->vulkan_config.loader_handle)
    {
        return SDL_SetError("Vulkan already loaded");
    }

    /* Load the Vulkan loader library */
    if (!path)
    {
        path = SDL_getenv("SDL_VULKAN_LIBRARY");
    }
    if (!path)
    {
        path = "libvulkan.so";
    }
    _this->vulkan_config.loader_handle = SDL_LoadObject(path);
    if (!_this->vulkan_config.loader_handle)
    {
        return false;
    }
    SDL_strlcpy(_this->vulkan_config.loader_path, path,
                SDL_arraysize(_this->vulkan_config.loader_path));
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(
        _this->vulkan_config.loader_handle, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr)
    {
        goto fail;
    }
    _this->vulkan_config.vkGetInstanceProcAddr = (void *)vkGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (void *)((PFN_vkGetInstanceProcAddr)_this->vulkan_config.vkGetInstanceProcAddr)(
            VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    if (!_this->vulkan_config.vkEnumerateInstanceExtensionProperties)
    {
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
    if (loadedCount == 0)
    {
        return;
    }
    loadedCount--;
    if (_this->vulkan_config.loader_handle && loadedCount == 0) {
        SDL_UnloadObject(_this->vulkan_config.loader_handle);
        _this->vulkan_config.loader_handle = NULL;
    }
}

/*bool OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, SDL_Window *window, unsigned *count,
    const char **names)
{
    static const char *const extensionsForOHOS[] = {
        VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_OHOS_XCOMPONENT_EXTENSION_NAME
    };
    if (!_this->vulkan_config.loader_handle) {
        SDL_SetError("Vulkan is not loaded");
        return false;
    }
    return SDL_Vulkan_GetInstanceExtensions_Helper(
        count, names, SDL_arraysize(extensionsForOHOS), extensionsForOHOS);
}*/

#endif
