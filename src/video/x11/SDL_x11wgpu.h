#ifndef SDL_x11wgpu_h_
#define SDL_x11wgpu_h_

// NOTE: Again, Not sure if I'm allowed to do this.
#include "../webgpu/webgpu.h"

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WGPU) && defined(SDL_VIDEO_DRIVER_X11)
extern WGPUSurface X11_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance);
#endif

#endif // SDL_x11wgpu_h_
