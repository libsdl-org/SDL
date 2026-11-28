#ifndef SDL_windowswgpu_h_
#define SDL_windowswgpu_h_

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WINDOWS)

// Dawn's a C++ lib so we'll have to do this to prevent any name mangling.
#ifdef __cplusplus
extern "C" {
#else
extern
#endif

bool WIN_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *surface);
void WIN_WGPU_DestroySurface(WGPUSurface surface);

#ifdef __cplusplus
}
#endif

#endif

#endif // SDL_windowswgpu_h_
