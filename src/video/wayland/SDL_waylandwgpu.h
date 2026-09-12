#ifndef SDL_waylandwgpu_h_
#define SDL_waylandwgpu_h_

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_WAYLAND)

// Dawn's a C++ lib so we'll have to do this to prevent any name mangling.
#ifdef __cplusplus
extern "C" {
#else
extern
#endif

WGPUSurface Wayland_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance);

#ifdef __cplusplus
}
#endif

#endif

#endif // SDL_waylandwgpu_h_
