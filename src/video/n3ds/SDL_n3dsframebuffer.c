/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_N3DS

#include "../SDL_sysvideo.h"
#include "../../SDL_properties_c.h"
#include "SDL_n3dsframebuffer_c.h"
#include "SDL_n3dsvideo.h"

#define N3DS_SURFACE "SDL.internal.window.surface"

typedef struct
{
    int width, height;
} Dimensions;

static void CopyFramebuffertoN3DS(u32 *dest, const Dimensions dest_dim, const u32 *source, const Dimensions source_dim);
static int GetDestOffset(int x, int y, int dest_width);
static int GetSourceOffset(int x, int y, int source_width);
static void FlushN3DSBuffer(const void *buffer, u32 bufsize, gfxScreen_t screen);


int SDL_N3DS_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *framebuffer;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);
    framebuffer = SDL_CreateSurface(w, h, FRAMEBUFFER_FORMAT);

    if (!framebuffer) {
        return -1;
    }

    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), N3DS_SURFACE, framebuffer);
    *format = FRAMEBUFFER_FORMAT;
    *pixels = framebuffer->pixels;
    *pitch = framebuffer->pitch;
    return 0;
}

int SDL_N3DS_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *drv_data = window->driverdata;
    SDL_Surface *surface;
    u16 width, height;
    u32 *framebuffer;
    u32 bufsize;

    surface = (SDL_Surface *)SDL_GetProperty(SDL_GetWindowProperties(window), N3DS_SURFACE, NULL);
    if (!surface) {
        return SDL_SetError("%s: Unable to get the window surface.", __func__);
    }

    /* Get the N3DS internal framebuffer and its size */
    framebuffer = (u32 *)gfxGetFramebuffer(drv_data->screen, GFX_LEFT, &width, &height);
    bufsize = width * height * 4;

    CopyFramebuffertoN3DS(framebuffer, (Dimensions){ width, height },
                          surface->pixels, (Dimensions){ surface->w, surface->h });
    FlushN3DSBuffer(framebuffer, bufsize, drv_data->screen);

    return 0;
}

static void CopyFramebuffertoN3DS(u32 *dest, const Dimensions dest_dim, const u32 *source, const Dimensions source_dim)
{
    int rows = SDL_min(dest_dim.width, source_dim.height);
    int cols = SDL_min(dest_dim.height, source_dim.width);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            SDL_memcpy(
                dest + GetDestOffset(x, y, dest_dim.width),
                source + GetSourceOffset(x, y, source_dim.width),
                4);
        }
    }
}

static int GetDestOffset(int x, int y, int dest_width)
{
    return dest_width - y - 1 + dest_width * x;
}

static int GetSourceOffset(int x, int y, int source_width)
{
    return x + y * source_width;
}

static void FlushN3DSBuffer(const void *buffer, u32 bufsize, gfxScreen_t screen)
{
    GSPGPU_FlushDataCache(buffer, bufsize);
    gfxScreenSwapBuffers(screen, false);
}

void SDL_N3DS_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_ClearProperty(SDL_GetWindowProperties(window), N3DS_SURFACE);
}

#endif /* SDL_VIDEO_DRIVER_N3DS */
