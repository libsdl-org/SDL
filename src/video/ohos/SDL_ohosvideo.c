#include "SDL_internal.h"
#ifdef SDL_VIDEO_DRIVER_OHOS
#include "../SDL_sysvideo.h"
#include "SDL_ohosvideo.h"

static SDL_VideoDevice *OHOS_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    // Initialize all variables that we clean on shutdown
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_free(device);
        return NULL;
    }

    device->internal = data;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = OHOS_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = OHOS_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = OHOS_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = OHOS_Vulkan_CreateSurface;
    device->Vulkan_DestroySurface = Android_Vulkan_DestroySurface;
#endif
    return device;
}

VideoBootStrap OHOS_bootstrap = {
    "OHOS",
    "SDL OHOS video driver",
    OHOS_CreateDevice,
    NULL,
    false
};
#endif
