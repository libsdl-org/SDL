#include "SDL_internal.h"

#if defined(SDL_VIDEO_WGPU) && defined(SDL_VIDEO_DRIVER_X11) && defined(WGPU_NATIVE)

#include "SDL_x11video.h"

#include "../SDL_wgpu_defs.h"
#include "SDL_x11wgpu.h"

SDL_ELF_NOTE_DLOPEN(
    "x11-wgpu",
    "Support for for wgpu-native on X11 backend",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    "libwgpu_native.so")

WGPUSurface X11_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    Display *x11_display = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, 0);
    Window x11_window = SDL_GetNumberProperty(windowProperties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    WGPUSurfaceDescriptor desc;
    WGPUSurfaceSourceXlibWindow source;

    source.window = x11_window;
    source.display = x11_display;
    source.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    source.chain.next = NULL;

    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

#if defined(WGPU_STATIC)
    return wgpuInstanceCreateSurface(instance, &desc);
#else
    SDL_SharedObject *wgpuLib = SDL_LoadObject("libwgpu_native.so");

    if (wgpuLib == NULL) {
        SDL_SetError("Failed to load wgpu-native shared library 'libwgpu_native.so'");
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
