#ifndef SDL_emscriptenwgpu_h_
#define SDL_emscriptenwgpu_h_

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_EMSCRIPTEN)

// Dawn's a C++ lib so we'll have to do this to prevent any name mangling.
#ifdef __cplusplus
extern "C" {
#else
extern
#endif

WGPUSurface Emscripten_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance);

#ifdef __cplusplus
}
#endif

#endif

#endif // SDL_emscriptenwgpu_h_
