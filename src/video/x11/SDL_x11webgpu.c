/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2021 NVIDIA Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"
#include "SDL_x11webgpu.h"

#if defined(SDL_VIDEO_WEBGPU) && defined(SDL_VIDEO_DRIVER_X11)

bool X11_WebGPU_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, WGPUInstance instance, WGPUSurface *retSurface)
{
    SDL_WindowData *data = window->internal;
    
    WGPUSurfaceSourceXlibWindow canvas_desc = {
        .chain.sType = WGPUSType_SurfaceSourceXlibWindow,
        .display = data->videodata->display,
        .window = (Uint64)data->xwindow,
    };

    const char *surface_label = "SDL_GPU Swapchain Surface";
    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = &canvas_desc.chain,
        .label = { surface_label, SDL_strlen(surface_label) },
    };

    *retSurface = wgpuInstanceCreateSurface(instance, &surf_desc);

    // Should probably return false somewhere here.
    return true;
}

#endif
