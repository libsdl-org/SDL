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
    SDL_WindowData *windata = window->driverdata;
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
    windata->surface = surface;
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
    SDL_WindowData *windata = window->driverdata;
    SDL_Surface *surface = windata->surface;
    size_t surface_size;

    Uint8 *buf;
    __dpmi_meminfo mapping;

    if (!surface) {
        return SDL_SetError("Missing SVGA surface");
    }

    if (SDL_GetWindowDisplayMode(window, &mode)) {
        return -1;
    }

    modedata = mode.driverdata;

    /* TODO: Support case when pitch includes off-screen padding. */
    surface_size = surface->pitch * surface->h;

    mapping.address = *(Uint32 *)&modedata->framebuffer_phys_addr;
    mapping.size = devdata->vbe_info.total_memory << 16;

    if (__dpmi_physical_address_mapping(&mapping)) {
        return -1;
    }

    if (!__djgpp_nearptr_enable()) {
        return -1;
    }

    buf = (Uint8 *)(mapping.address + __djgpp_conventional_base);

    /* TODO: Copy to back buffer and swap to screen. */
    SDL_memcpy(buf, surface->pixels, surface_size);

    return 0;
}

void
SDL_SVGA_DestroyFramebuffer(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = window->driverdata;
    SDL_FreeSurface(windata->surface);
    windata->surface = NULL;
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
