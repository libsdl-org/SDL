#ifndef SDL_x11wgpu_h_
#define SDL_x11wgpu_h_

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_X11)

#ifdef __cplusplus
extern "C" {
#else
extern
#endif

bool X11_WGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *surface);
void X11_WGPU_DestroySurface(WGPUSurface surface);

#ifdef __cplusplus
}
#endif

#endif

#endif // SDL_x11wgpu_h_
