#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WINDOWS)
#include "SDL_windowsvideo.h"

// We have to add some random stuff here to appease Dawn.
// I don't know why, but we have to.
#if defined(_DEBUG)
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "OneCore")
#endif

#include "../SDL_wgpu_defs.h"
#include "SDL_windowswgpu.h"

bool WIN_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *surface)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    void *hwnd = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0);
    void *hinstance = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, 0);

    WGPUSurfaceDescriptor desc;
    WGPUSurfaceSourceWindowsHWND source;

    source.hwnd = hwnd;
    source.hinstance = hinstance;
    source.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    source.chain.next = NULL;

    desc.label = { NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    return wgpuInstanceCreateSurface(instance, &desc);
}

void WIN_WGPU_DestroySurface(WGPUSurface surface) { wgpuSurfaceRelease(surface); }

#endif
