// NOTE: Again, Dawn's a C++ library.

#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WINDOWS) && defined(WGPU_DAWN)
#include "SDL_windowsvideo.h"

// Death to MSVC.
#if defined(_DEBUG)
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "OneCore")
#endif

#include "../SDL_wgpu_defs.h"
#include "SDL_windowswgpu.h"

WGPUSurface WIN_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    void *hwnd = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, 0);
    void *hinstance = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, 0);

    WGPUSurfaceDescriptor desc;
    WGPUSurfaceSourceWindowsHWND source;

    source.hwnd = hwnd;
    source.hinstance = hinstance;
    source.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    source.chain.next = NULL;

    desc.label = {NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    return wgpuInstanceCreateSurface(instance, &desc);
}

#endif
