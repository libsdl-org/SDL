// NOTE: I don't have a Windows machine to test this on. - TheStickmahn

#ifndef SDL_windowswgpu_h_
#define SDL_windowswgpu_h_

#include <SDL3/SDL_wgpu.h>

#if defined(SDL_VIDEO_WGPU) && defined(SDL_VIDEO_DRIVER_WINDOWS)
extern WGPUSurface WIN_WGPU_CreateSurface(SDL_VSDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance);
#endif

#endif // SDL_windowswgpu_h_
