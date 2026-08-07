#include "SDL_internal.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_EMSCRIPTEN) && defined(WGPU_DAWN)

#include "../SDL_sysvideo.h"
#include "../SDL_wgpu_defs.h"

#include "SDL_emscriptenwgpu.h"

#include <SDL3/SDL_wgpu.h>

WGPUSurface
Emscripten_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance)
{
    SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);

    const char *canvas = SDL_GetStringProperty(windowProperties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, 0);

    WGPUSurfaceDescriptor desc;
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector source;

    source.selector.data = canvas;
    source.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    source.chain.next = NULL;

    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    desc.nextInChain = &source.chain;

    return wgpuInstanceCreateSurface(instance, &desc);
}

#endif
