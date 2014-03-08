/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

/*
  Contributed by Brandon Schaefer, <brandon.schaefer@canonical.com>
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MIR

#include "SDL_mirevents.h"
#include "SDL_mirframebuffer.h"
#include "SDL_mirwindow.h"

#include "SDL_mirdyn.h"

static const Uint32 mir_pixel_format_to_sdl_format[] = {
    SDL_PIXELFORMAT_UNKNOWN,  /* mir_pixel_format_invalid   */
    SDL_PIXELFORMAT_ABGR8888, /* mir_pixel_format_abgr_8888 */
    SDL_PIXELFORMAT_BGR888,   /* mir_pixel_format_xbgr_8888 */
    SDL_PIXELFORMAT_ARGB8888, /* mir_pixel_format_argb_8888 */
    SDL_PIXELFORMAT_RGB888,   /* mir_pixel_format_xrgb_8888 */
    SDL_PIXELFORMAT_BGR24     /* mir_pixel_format_bgr_888   */
};

Uint32
MIR_GetSDLPixelFormat(MirPixelFormat format)
{
    return mir_pixel_format_to_sdl_format[format];
}

int
MIR_CreateWindowFramebuffer(_THIS, SDL_Window* window, Uint32* format,
                            void** pixels, int* pitch)
{
    MIR_Data* mir_data = _this->driverdata;
    MIR_Window* mir_window;
    MirSurfaceParameters surfaceparm;

    if (MIR_CreateWindow(_this, window) < 0)
        return SDL_SetError("Failed to created a mir window.");

    mir_window = window->driverdata;

    MIR_mir_surface_get_parameters(mir_window->surface, &surfaceparm);

    *format = MIR_GetSDLPixelFormat(surfaceparm.pixel_format);
    if (*format == SDL_PIXELFORMAT_UNKNOWN)
        return SDL_SetError("Unknown pixel format");

    *pitch = (((window->w * SDL_BYTESPERPIXEL(*format)) + 3) & ~3);

    *pixels = SDL_malloc(window->h*(*pitch));
    if (*pixels == NULL)
        return SDL_OutOfMemory();

    mir_window->surface = MIR_mir_connection_create_surface_sync(mir_data->connection, &surfaceparm);
    if (!MIR_mir_surface_is_valid(mir_window->surface)) {
        const char* error = MIR_mir_surface_get_error_message(mir_window->surface);
        return SDL_SetError("Failed to created a mir surface: %s", error);
    }

    return 0;
}

int
MIR_UpdateWindowFramebuffer(_THIS, SDL_Window* window,
                            const SDL_Rect* rects, int numrects)
{
    MIR_Window* mir_window = window->driverdata;

    MirGraphicsRegion region;
    int i, j, x, y, w, h, start;
    int bytes_per_pixel, bytes_per_row, s_stride, d_stride;
    char* s_dest;
    char* pixels;

    MIR_mir_surface_get_graphics_region(mir_window->surface, &region);

    s_dest = region.vaddr;
    pixels = (char*)window->surface->pixels;

    s_stride = window->surface->pitch;
    d_stride = region.stride;
    bytes_per_pixel = window->surface->format->BytesPerPixel;

    for (i = 0; i < numrects; i++) {
        s_dest = region.vaddr;
        pixels = (char*)window->surface->pixels;

        x = rects[i].x;
        y = rects[i].y;
        w = rects[i].w;
        h = rects[i].h;

        if (w <= 0 || h <= 0 || (x + w) <= 0 || (y + h) <= 0)
            continue;

        if (x < 0) {
            x += w;
            w += rects[i].x;
        }

        if (y < 0) {
            y += h;
            h += rects[i].y;
        }

        if (x + w > window->w)
            w = window->w - x;
        if (y + h > window->h)
            h = window->h - y;

        start = y * s_stride + x;
        pixels += start;
        s_dest += start;

        bytes_per_row =  bytes_per_pixel * w;
        for (j = 0; j < h; j++) {
            memcpy(s_dest, pixels, bytes_per_row);
            pixels += s_stride;
            s_dest += d_stride;
        }
    }

    MIR_mir_surface_swap_buffers_sync(mir_window->surface);

    return 0;
}

void
MIR_DestroyWindowFramebuffer(_THIS, SDL_Window* window)
{
}

#endif /* SDL_VIDEO_DRIVER_MIR */

/* vi: set ts=4 sw=4 expandtab: */
