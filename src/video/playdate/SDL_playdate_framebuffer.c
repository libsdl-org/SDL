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
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PLAYDATE

#include <math.h>

#include "../SDL_sysvideo.h"
#include "SDL_playdate_framebuffer_c.h"

#include "pd_api.h"

#define PLAYDATE_SURFACE   "_SDL_PlaydateSurface"

static const uint8_t bayer2[2][2] = {
        {     51, 206    },
        {    153, 102    }
};

typedef struct {
    uint16_t i;
    uint8_t black_mask;
    uint8_t white_mask;
    int x;
    int y;
} SDL_PD_Pixel;

static const int number_of_bytes = LCD_ROWS * LCD_COLUMNS;

static uint8_t bayer2_rows[LCD_ROWS];
static uint8_t bayer2_cols[LCD_COLUMNS];

static SDL_PD_Pixel SDL_pd_pixels[LCD_ROWS * LCD_COLUMNS];

static const float r_d = 0.212671f * 2;
static const float g_d = 0.715160f * 2;
static const float b_d = 0.072169f * 2;

int SDL_PLAYDATE_CreateWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format, void ** pixels, int *pitch)
{
    SDL_Surface *surface;
    const Uint32 surface_format = SDL_PIXELFORMAT_RGB888;

    for(int y = 0; y < LCD_ROWS; y++){
        bayer2_rows[y] = y % 2;
        for(int x = 0; x < LCD_COLUMNS; x++){
            bayer2_cols[x] = x % 2;

            int i = y * LCD_COLUMNS + x;
            int block_x = x / 8;
            int block_i = y * LCD_ROWSIZE + block_x;
            int n = 7 - x % 8;
            SDL_PD_Pixel pixel = {
                    .i = block_i,
                    .black_mask = ~(1 << n),
                    .white_mask = (1 << n),
                    .x = x,
                    .y = y,
            };

            SDL_pd_pixels[i] = pixel;
        }
    }

    /* Free the old framebuffer surface */
    SDL_PLAYDATE_DestroyWindowFramebuffer(_this, window);

    /* Create a new one */
    surface = SDL_CreateRGBSurfaceWithFormat(0, LCD_COLUMNS, LCD_ROWS, 0, surface_format);

    if (!surface) {
        return -1;
    }

    /* Save the info and return! */
    SDL_SetWindowData(window, PLAYDATE_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

    return 0;
}

int SDL_PLAYDATE_UpdateWindowFramebuffer(_THIS, SDL_Window * window, const SDL_Rect * rects, int numrects)
{
    if (!window->surface) {
        return SDL_SetError("Couldn't find surface for window");
    }

    Uint32* pixels = window->surface->pixels;
    Uint8* frame = pd->graphics->getFrame();

    for (int i = 0;i<number_of_bytes;i++) {
        SDL_PD_Pixel pixel = SDL_pd_pixels[i];
        Uint8 d_row = bayer2_rows[pixel.y];
        Uint8 d_col = bayer2_cols[pixel.x];

        Uint8 r; Uint8 g; Uint8 b;
        SDL_GetRGB(pixels[i], window->surface->format, &r, &g, &b);
        Uint8 intensity = r_d * r + g_d * g + b_d * b;
        Uint8 previous = frame[pixel.i];
        if (intensity < bayer2[d_col][d_row]) {
            frame[pixel.i] &= pixel.black_mask;
        } else {
            frame[pixel.i] |= pixel.white_mask;
        }
        int isDirty = previous != frame[pixel.i];
        if (isDirty) {
            pd->graphics->markUpdatedRows(pixel.y, pixel.y);
        }
    }

    return 0;
}

void SDL_PLAYDATE_DestroyWindowFramebuffer(_THIS, SDL_Window * window)
{
    SDL_Surface *surface;
    surface = (SDL_Surface *) SDL_SetWindowData(window, PLAYDATE_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_PLAYDATE */

/* vi: set ts=4 sw=4 expandtab: */
