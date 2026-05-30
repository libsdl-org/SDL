#ifndef SDL_waylandwgpu_h_
#define SDL_waylandwgpu_h_

// NOTE: Not sure if I'm allowed to do this.

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WGPU) && defined(SDL_VIDEO_DRIVER_WAYLAND)
extern WGPUSurface Wayland_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance);

#endif

#endif // SDL_waylandwgpu_h_
