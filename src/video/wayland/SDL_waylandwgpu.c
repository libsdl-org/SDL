#include "SDL_internal.h"

#if defined(SDL_VIDEO_WGPU) && defined(SDL_VIDEO_DRIVER_WAYLAND)
#include "SDL_waylandvideo.h"

#include "../SDL_wgpu_defs.h"
#include "SDL_waylandwgpu.h"

#include <SDL3/SDL_wgpu.h>

SDL_ELF_NOTE_DLOPEN(
    "wayland-wgpu",
    "Support for wgpu-native on wayland backend",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_SUGGESTED,
    "libwgpu_native.so")

WGPUSurface
Wayland_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    struct wl_display *display = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, 0);
    struct wl_surface *surface = SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, 0);

    WGPUSurfaceDescriptor desc;
    WGPUSurfaceSourceWaylandSurface source;

    source.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    source.chain.next = NULL;
    source.surface = surface;
    source.display = display;

    // TODO: I don't know the convention on what label the surface should have?
    // I'll just leave it NULL for now. - TheStickmahn
    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    // FIXME: wgpuGetProcAddress isn't implemented, so we'll have to use SDL_LoadFunction instead and just hope nothing goes badly
    //
    // WGPUProcGetProcAddress getProcAddr = (WGPUProcGetProcAddress)SDL_LoadFunction(SDL_LoadObject("libwgpu_native.so"), "wgpuGetProcAddress");
    // WGPUProcInstanceCreateSurface proc = (WGPUProcInstanceCreateSurface)getProcAddr((WGPUStringView){"wgpuInstanceCreateSurface", 25});

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
}

#endif
