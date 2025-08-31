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

#ifdef SDL_VIDEO_DRIVER_SVGA

#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/segments.h>

#include "SDL_mouse.h"
#include "SDL_svga_video.h"
#include "SDL_svga_framebuffer.h"

int
SDL_SVGA_CreateFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_DeviceData *devdata = _this->driverdata;
    SDL_DisplayMode mode;
    SDL_DisplayModeData *modedata;
    SDL_Surface *surface;
    SDL_WindowData *windata = window->driverdata;
    __dpmi_meminfo meminfo;
    int w, h;

    /* Free the old framebuffer surface. */
    SDL_SVGA_DestroyFramebuffer(_this, window);

    /* Get data for current mode. */
    if (SDL_GetWindowDisplayMode(window, &mode)) {
        return -1;
    }
    modedata = mode.driverdata;

    /* Map framebuffer's physical address to linear address. */
    meminfo.address = modedata->framebuffer_phys_addr.segment << 16;
    meminfo.address += modedata->framebuffer_phys_addr.offset;
    meminfo.size = devdata->vbe_info.total_memory << 16;
    if (__dpmi_physical_address_mapping(&meminfo)) {
        SDL_SVGA_DestroyFramebuffer(_this, window);
        return -1;
    }
    windata->framebuffer_linear_addr = meminfo.address;

    /* Allocate local descriptor to access memory-mapped framebuffer. */
    windata->framebuffer_selector = __dpmi_allocate_ldt_descriptors(1);
    if (windata->framebuffer_selector == -1) {
        SDL_SVGA_DestroyFramebuffer(_this, window);
        return -1;
    }

    /* Setup framebuffer descriptor. */
    if (__dpmi_set_segment_base_address(windata->framebuffer_selector, meminfo.address) ||
        __dpmi_set_segment_limit(windata->framebuffer_selector, meminfo.size - 1)) {
        SDL_SVGA_DestroyFramebuffer(_this, window);
        return -1;
    }

    /* Create a new surface. */
    SDL_GetWindowSize(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, mode.format);
    if (!surface) {
        SDL_SVGA_DestroyFramebuffer(_this, window);
        return -1;
    }

    /* Populate color palette for indexed pixel formats. */
    if (surface->format->palette) {
        SDL_Palette *palette = surface->format->palette;
        if (SVGA_GetPaletteData(palette->colors, palette->ncolors)) {
            SDL_SVGA_DestroyFramebuffer(_this, window);
            return -1;
        }
    }

    /* Save data and set output parameters. */
    window->surface = surface;
    *format = mode.format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

    return 0;
}

/* TODO: Draw a real pointer. */
static void
CopyCursorPixels(SDL_Window * window)
{
    SDL_Surface *surface = window->surface;
    SDL_WindowData *windata = window->driverdata;
    size_t surface_size = surface->pitch * surface->h;
    size_t framebuffer_offset = windata->framebuffer_page ? surface_size : 0;
    Uint32 color = SDL_MapRGB(surface->format, 0xFF, 0, 0);
    int i, k, x, y;

    SDL_GetMouseState(&x, &y);
    x = SDL_max(x, 0);
    y = SDL_max(y, 0);
    x = SDL_min(x, surface->w - 4);
    y = SDL_min(y, surface->h - 4);

    for (i = 0; i < 4; i++) {
        for (k = 0; k < 4; k++) {
            movedata(_my_ds(), (uintptr_t)&color, windata->framebuffer_selector,
                framebuffer_offset + surface->pitch * (y + i) + (x + k) * surface->format->BytesPerPixel,
                surface->format->BytesPerPixel);
        }
    }
}

int
SDL_SVGA_UpdateFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    SDL_WindowData *windata = window->driverdata;
    SDL_Surface *surface = window->surface;
    size_t framebuffer_offset, surface_size;

    if (!surface) {
        return SDL_SetError("Missing SVGA surface");
    }

    surface_size = surface->pitch * surface->h;

    /* Flip the active page flag. */
    windata->framebuffer_page = !windata->framebuffer_page;
    framebuffer_offset = windata->framebuffer_page ? surface_size : 0;

    /* Copy surface pixels to hidden framebuffer. */
    movedata(_my_ds(), (uintptr_t)surface->pixels, windata->framebuffer_selector,
        framebuffer_offset, surface_size);

    /* Copy cursor pixels to hidden framebuffer. */
    CopyCursorPixels(window);

    /* Display fresh page to screen. */
    SVGA_SetDisplayStart(0, windata->framebuffer_page ? surface->h : 0);

    return 0;
}

void
SDL_SVGA_DestroyFramebuffer(_THIS, SDL_Window * window)
{
    SDL_WindowData *windata = window->driverdata;

    /* Destroy surface. */
    SDL_FreeSurface(window->surface);
    window->surface = NULL;
    window->surface_valid = SDL_FALSE;

    /* Deallocate local descriptor for framebuffer. */
    if (windata->framebuffer_selector != -1) {
        __dpmi_free_ldt_descriptor(windata->framebuffer_selector);
        windata->framebuffer_selector = -1;
    }

    /* Unmap framebuffer physical address. */
    if (windata->framebuffer_linear_addr) {
        __dpmi_meminfo meminfo;

        meminfo.address = windata->framebuffer_linear_addr;
        __dpmi_free_physical_address_mapping(&meminfo);
        windata->framebuffer_linear_addr = 0;
    }
}

#endif /* SDL_VIDEO_DRIVER_SVGA */

/* vi: set ts=4 sw=4 expandtab: */
