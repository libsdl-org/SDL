// NOTE:
// Dawn's a C++ library.
// So, if you want to use C++, you have to have a .cpp file.
// The code in this file is largely identical to the native one.
// A couple changes though:
// C++ isn't a fan of forward declaring enums, so we can't use SDL_waylandvideo.h,
// but that's fine; we're not using anything that isn't in SDL_sysvideo.h.
// I also had to add a cast for SDL_GetPointerProperty to wl_display/surface.

#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(WGPU_DAWN)

#include "../SDL_sysvideo.h"
#include "../SDL_wgpu_defs.h"

#include "SDL_waylandwgpu.h"

#include <SDL3/SDL_wgpu.h>

WGPUSurface
Wayland_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    struct wl_display *display = (wl_display *)SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, 0);
    struct wl_surface *surface = (wl_surface *)SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, 0);

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

    // Dawn doesn't support dynamically loading, so we're just always using wgpuInstanceCreateSurface.
    return wgpuInstanceCreateSurface(instance, &desc);
}

#endif
