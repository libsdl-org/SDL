#ifndef SDL_OHOSVULKAN_H
#define SDL_OHOSVULKAN_H

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../SDL_sysvideo.h"

bool OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path);
void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this);
char const* const* OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count);
#endif

#endif
