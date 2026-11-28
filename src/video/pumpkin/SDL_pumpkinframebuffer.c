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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PUMPKINOS

#include "../SDL_sysvideo.h"
#include "SDL_pumpkinvideo.h"
#include "SDL_pumpkinframebuffer_c.h"
#include "../../core/pumpkin/SDL_pumpkin.h"

bool PUMPKIN_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata = window->internal;
    SDL_Surface *surface;
    int w, h;

    if (wdata->shadow) {
        SDL_DestroySurface(wdata->shadow);
        wdata->shadow = NULL;
    }

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurfaceZeroed(w, h, data->format);
    if (!surface) {
        return false;
    }

    wdata->shadow = surface;
    *format = data->format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return true;
}

// Copy one rectangle of the shadow surface into the task screen surface.
// dst is the linear pixel buffer of the screen, or NULL to go through the
// surface callbacks (needed for palette, gray and mono screens).
static void PUMPKIN_CopyRect(SDL_Surface *shadow, surface_t *screen, Uint8 *dst, int dst_pitch, const SDL_Rect *r)
{
    const int bpp = SDL_BYTESPERPIXEL(shadow->format);
    int x, y;

    if (dst && screen->encoding == SURFACE_ENCODING_RGB565) {
        for (y = 0; y < r->h; y++) {
            const Uint8 *src = (const Uint8 *)shadow->pixels + (r->y + y) * shadow->pitch + r->x * bpp;
            SDL_memcpy(dst + (r->y + y) * dst_pitch + r->x * 2, src, (size_t)r->w * 2);
        }
    } else if (dst && (screen->encoding == SURFACE_ENCODING_ARGB || screen->encoding == SURFACE_ENCODING_ABGR)) {
        // same layout as the shadow surface; force the alpha channel to opaque,
        // the desktop composites task screens over the background
        for (y = 0; y < r->h; y++) {
            const Uint32 *src = (const Uint32 *)((const Uint8 *)shadow->pixels + (r->y + y) * shadow->pitch) + r->x;
            Uint32 *d = (Uint32 *)(dst + (r->y + y) * dst_pitch) + r->x;
            for (x = 0; x < r->w; x++) {
                d[x] = src[x] | 0xFF000000u;
            }
        }
    } else {
        // the shadow surface is XRGB8888 here (see PUMPKIN_PixelFormatForEncoding)
        for (y = 0; y < r->h; y++) {
            const Uint32 *src = (const Uint32 *)((const Uint8 *)shadow->pixels + (r->y + y) * shadow->pitch) + r->x;
            for (x = 0; x < r->w; x++) {
                const Uint32 p = src[x];
                const uint32_t c = screen->color_rgb(screen->data, (p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF, 0xFF);
                screen->setpixel(screen->data, r->x + x, r->y + y, c);
            }
        }
    }
}

bool PUMPKIN_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata = window->internal;
    SDL_Surface *shadow = wdata ? wdata->shadow : NULL;
    surface_t *screen;
    void *scr = NULL;
    Uint8 *dst;
    int dst_len, dst_pitch;
    SDL_Rect bounds, dirty;
    bool have_dirty = false;
    int i;

    if (!shadow) {
        return SDL_SetError("PumpkinOS: window has no framebuffer");
    }

    // Work out the dirty area first: the screen lock must only be released
    // with a non-empty rectangle, so do not take it when there is nothing to do.
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = SDL_min(shadow->w, data->screen_w);
    bounds.h = SDL_min(shadow->h, data->screen_h);
    for (i = 0; i < numrects; i++) {
        SDL_Rect r;
        if (SDL_GetRectIntersection(&rects[i], &bounds, &r)) {
            if (have_dirty) {
                SDL_GetRectUnion(&dirty, &r, &dirty);
            } else {
                dirty = r;
                have_dirty = true;
            }
        }
    }
    if (!have_dirty) {
        return true;
    }

    screen = pumpkin_screen_lock(&scr);
    if (!screen) {
        return SDL_SetError("PumpkinOS: cannot lock the task screen");
    }

    // the screen may have been reallocated with another size behind our back
    bounds.w = SDL_min(bounds.w, screen->width);
    bounds.h = SDL_min(bounds.h, screen->height);
    SDL_GetRectIntersection(&dirty, &bounds, &dirty);

    dst = (Uint8 *)surface_buffer(screen, &dst_len);
    dst_pitch = screen->height > 0 ? dst_len / screen->height : 0;
    if (dst_pitch <= 0) {
        dst = NULL;
    }

    for (i = 0; i < numrects; i++) {
        SDL_Rect r;
        if (SDL_GetRectIntersection(&rects[i], &bounds, &r)) {
            PUMPKIN_CopyRect(shadow, screen, dst, dst_pitch, &r);
        }
    }

    if (dirty.w > 0 && dirty.h > 0) {
        pumpkin_screen_unlock(scr, dirty.x, dirty.y, dirty.x + dirty.w - 1, dirty.y + dirty.h - 1);
    } else {
        pumpkin_screen_unlock(scr, 0, 0, 0, 0);
    }
    // In single application mode the screen is only uploaded at the end of a
    // dirty region batch; an empty batch flushes what was just marked.
    pumpkin_dirty_region_mode(dirtyRegionBegin);
    pumpkin_dirty_region_mode(dirtyRegionEnd);
    return true;
}

void PUMPKIN_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wdata = window->internal;

    if (wdata && wdata->shadow) {
        SDL_DestroySurface(wdata->shadow);
        wdata->shadow = NULL;
    }
}

#endif // SDL_VIDEO_DRIVER_PUMPKINOS
