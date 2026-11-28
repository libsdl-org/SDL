// NOTE: We had to remove SDL_x11video.h since that uses the register keyword which was removed in C++17.
// That means we can't use Display, but that's not an issue since we're using a pointer to Display, which is a struct.
// We'll just make it a generic struct pointer.

#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_X11)

#include "../SDL_sysvideo.h"
#include "../SDL_wgpu_defs.h"

#include "SDL_x11wgpu.h"

bool X11_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *surface)
{
    if (!surface) {
        SDL_SetError("Surface pointer is NULL!");
        return false;
    }

    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    struct Display *x11_display = (Display *)SDL_GetPointerProperty(windowProperties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, 0);
    int x11_window = SDL_GetNumberProperty(windowProperties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    WGPUSurfaceDescriptor desc;
    WGPUSurfaceSourceXlibWindow source;

    source.window = x11_window;
    source.display = x11_display;
    source.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    source.chain.next = NULL;

    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    *surface = wgpuInstanceCreateSurface(instance, &desc);
    
    if(!surface){
        SDL_SetError("Failed to create WebGPU surface!");
        return false;
    }

    return true;
}

void X11_WGPU_DestroySurface(WGPUSurface surface) { wgpuSurfaceRelease(surface); }

#endif
