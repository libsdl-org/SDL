#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_EMSCRIPTEN)

#include "../SDL_sysvideo.h"
#include "../SDL_wgpu_defs.h"

#include "SDL_emscriptenwgpu.h"

bool
Emscripten_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *surface)
{
    if (!surface) {
        SDL_SetError("Surface pointer is NULL!");
        return false;
    }

    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    const char *canvas = SDL_GetStringProperty(windowProperties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, 0);

    WGPUSurfaceDescriptor desc;
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source;

    source.selector.data = canvas;
    source.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    source.chain.next = NULL;

    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    *surface = wgpuInstanceCreateSurface(instance, &desc);
    
    if (!surface) {
        SDL_SetError("Failed to create WebGPU surface!");
        return false;
    }

    return true;
}

void Emscripten_WGPU_DestroySurface(WGPUSurface surface) { wgpuSurfaceRelease(surface); }

#endif
