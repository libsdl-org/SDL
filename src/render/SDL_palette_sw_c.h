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

#ifndef SDL_palette_sw_c_h_
#define SDL_palette_sw_c_h_

#include "../SDL_internal.h"

#include "SDL_video.h"

/* This is the software implementation of the paletted texture support */

struct SDL_SW_PaletteTexture
{
    SDL_Surface *surface;
    SDL_Palette *palette;

    SDL_Surface *display;
};

typedef struct SDL_SW_PaletteTexture SDL_SW_PaletteTexture;

SDL_SW_PaletteTexture *SDL_SW_CreatePaletteTexture(Uint32 format, int w, int h);
int SDL_SW_UpdatePaletteTexture(SDL_SW_PaletteTexture *swdata, const SDL_Rect *rect,
                                const void *pixels, int pitch,
                                const SDL_Color *colors, int firstcolor, int ncolors);
int SDL_SW_LockPaletteTexture(SDL_SW_PaletteTexture *swdata, const SDL_Rect *rect,
                              void **pixels, int *pitch);
void SDL_SW_UnlockPaletteTexture(SDL_SW_PaletteTexture *swdata);
int SDL_SW_CopyPaletteToRGB(SDL_SW_PaletteTexture *swdata, const SDL_Rect *srcrect,
                            Uint32 target_format, int w, int h, void *pixels,
                            int pitch);
void SDL_SW_DestroyPaletteTexture(SDL_SW_PaletteTexture *swdata);

#endif /* SDL_palette_sw_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
