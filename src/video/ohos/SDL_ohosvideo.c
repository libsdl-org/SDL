#include "SDL_internal.h"
#include "../SDL_sysvideo.h"

#ifdef SDL_VIDEO_DRIVER_OHOS
#include "SDL_ohosvulkan.h"

bool OHOS_VideoInit(SDL_VideoDevice *_this)
{
    return true;
}
void OHOS_VideoQuit(SDL_VideoDevice *_this)
{
}
static SDL_VideoDevice *OHOS_CreateDevice(void)
{
    SDL_VideoDevice *device;

    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }

    device->VideoInit = OHOS_VideoInit;
    device->VideoQuit = OHOS_VideoQuit;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = OHOS_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = OHOS_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = OHOS_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = OHOS_Vulkan_CreateSurface;
    device->Vulkan_DestroySurface = OHOS_Vulkan_DestroySurface;
#endif

    return device;
}
VideoBootStrap OHOS_bootstrap = {
    "ohos", "OpenHarmony video driver",
    OHOS_CreateDevice,
    NULL,
    false
};
#endif
