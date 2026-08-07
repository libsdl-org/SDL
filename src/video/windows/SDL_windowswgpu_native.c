
#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WINDOWS) && defined(WGPU_NATIVE)
#include "SDL_windowsvideo.h"

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

    desc.label = { NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

#if defined(WGPU_STATIC)
    return wgpuInstanceCreateSurface(instance, &desc);
#else
    SDL_SharedObject *wgpuLib = SDL_LoadObject("wgpu_native.dll");

    if (wgpuLib == NULL) {
        SDL_SetError("Failed to load wgpu-native shared library 'wgpu_native.dll'");
        goto fail;
    }

    WGPUProcInstanceCreateSurface proc = (WGPUProcInstanceCreateSurface)SDL_LoadFunction(wgpuLib, "wgpuInstanceCreateSurface");

    if (proc == NULL) {
        SDL_SetError("Failed to load function 'wgpuInstanceCreateSurface' from loaded wgpu-native library!");
        goto fail;
    }

    return proc(instance, &desc);
fail:
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create WGPU surface: %s", SDL_GetError());
    return NULL;
#endif
}

#endif
