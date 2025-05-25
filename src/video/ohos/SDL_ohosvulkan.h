#include "SDL_internal.h"

#ifndef SDL_OHOSVULKAN_H
#define SDL_OHOSVULKAN_H

#include "../SDL_sysvideo.h"
#include "../SDL_vulkan_internal.h"

#if defined(SDL_VIDEO_VULKAN)

extern bool OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path);
extern void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this);
extern char const *const *OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count);
extern bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this,
                                      SDL_Window *window,
                                      VkInstance instance,
                                      const struct VkAllocationCallbacks *allocator,
                                      VkSurfaceKHR *surface);
extern void OHOS_Vulkan_DestroySurface(SDL_VideoDevice *_this,
                                       VkInstance instance,
                                       VkSurfaceKHR surface,
                                       const struct VkAllocationCallbacks *allocator);

#endif

#endif
