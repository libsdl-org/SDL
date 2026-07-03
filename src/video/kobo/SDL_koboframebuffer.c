/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

/* Kobo (MediaTek hwtcon) e-ink video driver -- ADDED for the Kobo port. */

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_KOBO

#include <string.h>
#include <sys/ioctl.h>

#include "SDL_hints.h"
#include "../SDL_sysvideo.h"
#include "SDL_kobovideo.h"
#include "SDL_koboframebuffer_c.h"
#include "SDL_kobo_hwtcon.h"

#define KOBO_SURFACE "_SDL_KoboSurface"

/* Resolve the waveform mode from the SDL_KOBO_WAVEFORM hint (env or runtime).
   Default is GC16: a full-quality 16-level update that is guaranteed to render
   correctly for a first light. Callers can switch to A2/DU for speed. */
static Uint32 KOBO_WaveformFromHint(void)
{
    const char *h = SDL_GetHint(KOBO_HINT_WAVEFORM);

    if (!h || !*h) {
        return HWTCON_WAVEFORM_MODE_GC16;
    }
    if (SDL_strcasecmp(h, "AUTO") == 0)   return HWTCON_WAVEFORM_MODE_AUTO;
    if (SDL_strcasecmp(h, "INIT") == 0)   return HWTCON_WAVEFORM_MODE_INIT;
    if (SDL_strcasecmp(h, "DU") == 0)     return HWTCON_WAVEFORM_MODE_DU;
    if (SDL_strcasecmp(h, "GC16") == 0)   return HWTCON_WAVEFORM_MODE_GC16;
    if (SDL_strcasecmp(h, "GL16") == 0)   return HWTCON_WAVEFORM_MODE_GL16;
    if (SDL_strcasecmp(h, "GLR16") == 0)  return HWTCON_WAVEFORM_MODE_GLR16;
    if (SDL_strcasecmp(h, "REAGL") == 0)  return HWTCON_WAVEFORM_MODE_REAGL;
    if (SDL_strcasecmp(h, "A2") == 0)     return HWTCON_WAVEFORM_MODE_A2;
    if (SDL_strcasecmp(h, "GCK16") == 0)  return HWTCON_WAVEFORM_MODE_GCK16;
    if (SDL_strcasecmp(h, "GLKW16") == 0) return HWTCON_WAVEFORM_MODE_GLKW16;
    if (SDL_strcasecmp(h, "GCC16") == 0)  return HWTCON_WAVEFORM_MODE_GCC16;
    if (SDL_strcasecmp(h, "GLRC16") == 0) return HWTCON_WAVEFORM_MODE_GLRC16;

    return HWTCON_WAVEFORM_MODE_GC16;
}

/* Issue one hwtcon update over [left,top,w,h] (panel coords). */
static void KOBO_SendUpdate(KOBO_VideoData *data, int left, int top, int w, int h,
                            Uint32 waveform_mode)
{
    struct hwtcon_update_data upd;

    SDL_zero(upd);
    upd.update_region.left   = (Uint32)left;
    upd.update_region.top    = (Uint32)top;
    upd.update_region.width  = (Uint32)w;
    upd.update_region.height = (Uint32)h;
    upd.waveform_mode = waveform_mode;
    upd.update_mode   = SDL_GetHintBoolean(KOBO_HINT_FULL, SDL_FALSE)
                            ? HWTCON_UPDATE_MODE_FULL
                            : HWTCON_UPDATE_MODE_PARTIAL;
    upd.update_marker = ++data->marker;
    upd.flags         = 0;
    upd.dither_mode   = 0;

    if (ioctl(data->fd, HWTCON_SEND_UPDATE, &upd) < 0) {
        /* Non-fatal: pixels are already in the fb, only the refresh failed. */
        return;
    }

    if (SDL_GetHintBoolean(KOBO_HINT_WAIT, SDL_FALSE)) {
        struct hwtcon_update_marker_data m;
        SDL_zero(m);
        m.update_marker = upd.update_marker;
        (void)ioctl(data->fd, HWTCON_WAIT_FOR_UPDATE_COMPLETE, &m);
    }
}

int SDL_KOBO_RefreshFull(KOBO_VideoData *data, Uint32 waveform_mode)
{
    if (!data || !data->map) {
        return SDL_SetError("Kobo framebuffer not mapped");
    }
    KOBO_SendUpdate(data, 0, 0, data->width, data->height, waveform_mode);
    return 0;
}

int SDL_KOBO_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format,
                                     void **pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_ARGB8888;
    int w, h;

    /* Free any old framebuffer surface for this window. */
    SDL_KOBO_DestroyWindowFramebuffer(_this, window);

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 0, surface_format);
    if (!surface) {
        return -1;
    }

    SDL_SetWindowData(window, KOBO_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch  = surface->pitch;
    return 0;
}

int SDL_KOBO_UpdateWindowFramebuffer(_THIS, SDL_Window *window,
                                     const SDL_Rect *rects, int numrects)
{
    KOBO_VideoData *data = (KOBO_VideoData *)_this->driverdata;
    SDL_Surface *surface;
    int minx, miny, maxx, maxy;
    int y;

    surface = (SDL_Surface *)SDL_GetWindowData(window, KOBO_SURFACE);
    if (!surface) {
        return SDL_SetError("Couldn't find kobo framebuffer surface for window");
    }
    if (!data || !data->map) {
        return SDL_SetError("Kobo framebuffer not mapped");
    }

    /* Union of the dirty rects, clamped to both the surface and the panel. */
    if (numrects <= 0 || !rects) {
        minx = 0;
        miny = 0;
        maxx = surface->w;
        maxy = surface->h;
    } else {
        int i;
        minx = surface->w;
        miny = surface->h;
        maxx = 0;
        maxy = 0;
        for (i = 0; i < numrects; ++i) {
            int rx = rects[i].x, ry = rects[i].y;
            int rr = rx + rects[i].w, rb = ry + rects[i].h;
            if (rx < minx) minx = rx;
            if (ry < miny) miny = ry;
            if (rr > maxx) maxx = rr;
            if (rb > maxy) maxy = rb;
        }
    }

    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > surface->w) maxx = surface->w;
    if (maxy > surface->h) maxy = surface->h;
    if (maxx > data->width) maxx = data->width;
    if (maxy > data->height) maxy = data->height;
    if (maxx <= minx || maxy <= miny) {
        return 0;
    }

    /* Format-agnostic grayscale path (a): compute BT.601 luma and replicate it
       into every byte of the 32bpp pixel. Because the panel is grayscale, this
       renders correctly regardless of the fb's (undiscoverable) channel order.
       TODO (path b): once FBInk's MTK pixel order is confirmed, write real RGB
       so the hwtcon hardware dithering can operate on true color. */
    for (y = miny; y < maxy; ++y) {
        const Uint32 *src = (const Uint32 *)((const Uint8 *)surface->pixels + (size_t)y * surface->pitch);
        Uint32 *dst = (Uint32 *)(data->map + (size_t)y * data->pitch);
        int x;
        for (x = minx; x < maxx; ++x) {
            Uint32 p = src[x];
            Uint32 r = (p >> 16) & 0xFF;
            Uint32 g = (p >> 8) & 0xFF;
            Uint32 b = p & 0xFF;
            Uint32 luma = (77 * r + 150 * g + 29 * b) >> 8; /* 0.299/0.587/0.114 */
            dst[x] = luma * 0x01010101u;
        }
    }

    KOBO_SendUpdate(data, minx, miny, maxx - minx, maxy - miny, KOBO_WaveformFromHint());
    return 0;
}

void SDL_KOBO_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;

    surface = (SDL_Surface *)SDL_SetWindowData(window, KOBO_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_KOBO */

/* vi: set ts=4 sw=4 expandtab: */
