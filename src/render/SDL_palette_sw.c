/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* This is the software implementation of the paletted texture support */

#include "SDL_palette_sw_c.h"

SDL_SW_PaletteTexture *SDL_SW_CreatePaletteTexture(Uint32 format, int w, int h)
{
    SDL_SW_PaletteTexture *swdata;

    switch (format) {
    /*
    case SDL_PIXELFORMAT_INDEX1LSB:
    case SDL_PIXELFORMAT_INDEX1MSB:
    case SDL_PIXELFORMAT_INDEX4LSB:
    case SDL_PIXELFORMAT_INDEX4MSB:
    */
    case SDL_PIXELFORMAT_INDEX8:
        break;
    default:
        SDL_SetError("Unsupported palette format");
        return NULL;
    }

    swdata = (SDL_SW_PaletteTexture *)SDL_calloc(1, sizeof(*swdata));
    if (!swdata) {
        SDL_OutOfMemory();
        return NULL;
    }

    swdata->surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(format), format);
    if (!swdata->surface) {
        SDL_SW_DestroyPaletteTexture(swdata);
        return NULL;
    }

    swdata->palette = SDL_AllocPalette(256);
    if (!swdata->palette) {
        SDL_SW_DestroyPaletteTexture(swdata);
        return NULL;
    }

    SDL_SetSurfacePalette(swdata->surface, swdata->palette);

    /* We're all done.. */
    return (swdata);
}

int SDL_SW_UpdatePaletteTexture(SDL_SW_PaletteTexture *swdata, const SDL_Rect *rect,
                                const void *pixels, int pitch,
                                const SDL_Color *colors, int firstcolor, int ncolors)
{
    const Uint8 *src;
    Uint8 *dst;
    int row;
    size_t length;

    if (pixels) {
        if (SDL_LockSurface(swdata->surface) < 0) {
            return -1;
        }

        src = pixels;
        dst = (Uint8 *)swdata->surface->pixels + rect->y * swdata->surface->pitch + rect->x;
        length = rect->w;
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += swdata->surface->pitch;
        }

        SDL_UnlockSurface(swdata->surface);
    }
    if (colors) {
        SDL_SetPaletteColors(swdata->palette, colors, firstcolor, ncolors);
    }
    return 0;
}

int SDL_SW_LockPaletteTexture(SDL_SW_PaletteTexture *swdata, const SDL_Rect *rect,
                              void **pixels, int *pitch)
{
    if (SDL_LockSurface(swdata->surface) < 0) {
        return -1;
    }

    if (rect) {
        *pixels = (Uint8 *)swdata->surface->pixels + rect->y * swdata->surface->pitch + rect->x;
    } else {
        *pixels = (Uint8 *)swdata->surface->pixels;
    }
    *pitch = swdata->surface->pitch;
    return 0;
}

void SDL_SW_UnlockPaletteTexture(SDL_SW_PaletteTexture *swdata)
{
    SDL_UnlockSurface(swdata->surface);
}

int SDL_SW_CopyPaletteToRGB(SDL_SW_PaletteTexture *swdata, const SDL_Rect *srcrect,
                            Uint32 target_format, int w, int h, void *pixels,
                            int pitch)
{
    SDL_Rect rect = *srcrect;

    /* Make sure we're set up to display in the desired format */
    if (swdata->display && target_format != swdata->display->format->format) {
        SDL_FreeSurface(swdata->display);
        swdata->display = NULL;
    }

    if (swdata->display) {
        swdata->display->w = w;
        swdata->display->h = h;
        swdata->display->pixels = pixels;
        swdata->display->pitch = pitch;
    } else {
        swdata->display =
            SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, SDL_BITSPERPIXEL(target_format),
                                               pitch, target_format);
        if (!swdata->display) {
            return (-1);
        }
    }

    return SDL_BlitSurface(swdata->surface, &rect, swdata->display, NULL);
}

void SDL_SW_DestroyPaletteTexture(SDL_SW_PaletteTexture *swdata)
{
    if (swdata) {
        SDL_FreeSurface(swdata->display);
        SDL_FreeSurface(swdata->surface);
        SDL_FreePalette(swdata->palette);
        SDL_free(swdata);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
