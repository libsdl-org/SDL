#ifndef SDL_OHOSVULKAN_H
#define SDL_OHOSVULKAN_H

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../SDL_sysvideo.h"

bool OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path);
void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this);
char const* const* OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count);
bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this,
    SDL_Window *window,
    VkInstance instance,
    const struct VkAllocationCallbacks *allocator,
    VkSurfaceKHR *surface);
void OHOS_Vulkan_DestroySurface(SDL_VideoDevice *_this,
    VkInstance instance,
    VkSurfaceKHR surface,
    const struct VkAllocationCallbacks *allocator);
#endif

#endif
