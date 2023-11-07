/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_OFFSCREEN

#include "../SDL_sysvideo.h"
#include "SDL_offscreenframebuffer_c.h"

#define OFFSCREEN_SURFACE "SDL.internal.window.surface"

static void CleanupSurface(void *userdata, void *value)
{
    SDL_Surface *surface = (SDL_Surface *)value;

    SDL_DestroySurface(surface);
}

int SDL_OFFSCREEN_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_XRGB8888;
    int w, h;

    /* Create a new framebuffer */
    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurface(w, h, surface_format);
    if (surface == NULL) {
        return -1;
    }

    /* Save the info and return! */
    SDL_SetPropertyWithCleanup(SDL_GetWindowProperties(window), OFFSCREEN_SURFACE, surface, CleanupSurface, NULL);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

    return 0;
}

int SDL_OFFSCREEN_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    static int frame_number;
    SDL_Surface *surface;

    surface = (SDL_Surface *)SDL_GetProperty(SDL_GetWindowProperties(window), OFFSCREEN_SURFACE);
    if (surface == NULL) {
        return SDL_SetError("Couldn't find offscreen surface for window");
    }

    /* Send the data to the display */
    if (SDL_getenv("SDL_VIDEO_OFFSCREEN_SAVE_FRAMES")) {
        char file[128];
        (void)SDL_snprintf(file, sizeof(file), "SDL_window%" SDL_PRIu32 "-%8.8d.bmp",
                           SDL_GetWindowID(window), ++frame_number);
        SDL_SaveBMP(surface, file);
    }
    return 0;
}

void SDL_OFFSCREEN_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_ClearProperty(SDL_GetWindowProperties(window), OFFSCREEN_SURFACE);
}

#endif /* SDL_VIDEO_DRIVER_OFFSCREEN */
