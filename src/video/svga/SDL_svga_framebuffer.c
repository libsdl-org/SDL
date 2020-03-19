/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_SVGA

#include <dpmi.h>
#include <sys/nearptr.h>

#include "SDL_svga_video.h"
#include "SDL_svga_framebuffer.h"

int
SDL_SVGA_CreateFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_DeviceData *devdata = _this->driverdata;
    SDL_Surface *surface;
    Uint32 surface_format = SDL_GetWindowPixelFormat(window);
    int w, h;

    /* Free the old framebuffer surface */
    SDL_SVGA_DestroyFramebuffer(_this, window);

    /* Create a new surface */
    SDL_GetWindowSize(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    devdata->surface = surface;
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

int
SDL_SVGA_UpdateFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    SDL_DeviceData *devdata = _this->driverdata;
    SDL_DisplayMode mode;
    SDL_DisplayModeData *modedata;
    SDL_Surface *surface = devdata->surface;

    Uint8 *buf;
    __dpmi_meminfo mapping;

    if (!surface) {
        return SDL_SetError("Missing SVGA surface");
    }

    /* TODO: Copy to back buffer and swap to screen */

    if (SDL_GetWindowDisplayMode(window, &mode)) {
        return -1;
    }

    modedata = mode.driverdata;
    mapping.address = (uintptr_t)modedata->framebuffer_phys_addr;
    mapping.size = modedata->framebuffer_size;
    
    if (__dpmi_physical_address_mapping(&mapping)) {
        return -1;
    }

    if (!__djgpp_nearptr_enable()) {
        return -1;
    }

    buf = (Uint8 *)(mapping.address + __djgpp_conventional_base);

    /* TODO: Use a blit function? */
    SDL_memcpy(buf, surface->pixels, surface->w * surface->h * surface->format->BytesPerPixel);

    return 0;
}

void
SDL_SVGA_DestroyFramebuffer(_THIS, SDL_Window * window)
{
    SDL_DeviceData *devdata = _this->driverdata;
    SDL_FreeSurface(devdata->surface);
    devdata->surface = NULL;
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
