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

/*

 Based on automated SDL_Surface tests originally written by Edgar Simo 'bobbens'.

 Rewritten for test lib by Andreas Schiffler.

*/
#include <SDL3/SDL_test.h>

#include "../video/SDL_surface_pixel_impl.h"

#define FILENAME_SIZE 128

/* Counter for _CompareSurface calls; used for filename creation when comparisons fail */
static int _CompareSurfaceCount = 0;

int
SDLTest_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    return SDL_ReadSurfacePixel_impl(surface, x, y, r, g, b, a);
}

static void
LogErrorFormat(const char *name, const SDL_PixelFormat *format)
{
  SDLTest_LogError("%s: %08" SDL_PRIx32 " %s, %u bits/%u bytes per pixel", name, format->format, SDL_GetPixelFormatName(format->format), format->BitsPerPixel, format->BytesPerPixel);
  SDLTest_LogError("%s: R mask %08" SDL_PRIx32 ", loss %u, shift %u", name, format->Rmask, format->Rloss, format->Rshift);
  SDLTest_LogError("%s: G mask %08" SDL_PRIx32 ", loss %u, shift %u", name, format->Gmask, format->Gloss, format->Gshift);
  SDLTest_LogError("%s: B mask %08" SDL_PRIx32 ", loss %u, shift %u", name, format->Bmask, format->Bloss, format->Bshift);
  SDLTest_LogError("%s: A mask %08" SDL_PRIx32 ", loss %u, shift %u", name, format->Amask, format->Aloss, format->Ashift);
}

/* Compare surfaces */
int SDLTest_CompareSurfaces(SDL_Surface *surface, SDL_Surface *referenceSurface, int allowable_error)
{
    int ret;
    int i, j;
    int dist;
    int sampleErrorX = 0, sampleErrorY = 0, sampleDist = 0;
    SDL_Color sampleReference = { 0, 0, 0, 0 };
    SDL_Color sampleActual = { 0, 0, 0, 0 };
    Uint8 R, G, B, A;
    Uint8 Rd, Gd, Bd, Ad;
    char imageFilename[FILENAME_SIZE];
    char referenceFilename[FILENAME_SIZE];

    /* Validate input surfaces */
    if (!surface) {
        SDLTest_LogError("Cannot compare NULL surface");
        return -1;
    }

    if (!referenceSurface) {
        SDLTest_LogError("Cannot compare NULL reference surface");
        return -1;
    }

    /* Make sure surface size is the same. */
    if ((surface->w != referenceSurface->w) || (surface->h != referenceSurface->h)) {
        SDLTest_LogError("Expected %dx%d surface, got %dx%d", referenceSurface->w, referenceSurface->h, surface->w, surface->h);
        return -2;
    }

    /* Sanitize input value */
    if (allowable_error < 0) {
        allowable_error = 0;
    }

    SDL_LockSurface(surface);
    SDL_LockSurface(referenceSurface);

    ret = 0;
    /* Compare image - should be same format. */
    for (j = 0; j < surface->h; j++) {
        for (i = 0; i < surface->w; i++) {
            int temp;

            temp = SDLTest_ReadSurfacePixel(surface, i, j, &R, &G, &B, &A);
            if (temp != 0) {
                SDLTest_LogError("Failed to retrieve pixel (%d,%d): %s", i, j, SDL_GetError());
                ret++;
                continue;
            }

            temp = SDLTest_ReadSurfacePixel(referenceSurface, i, j, &Rd, &Gd, &Bd, &Ad);
            if (temp != 0) {
                SDLTest_LogError("Failed to retrieve reference pixel (%d,%d): %s", i, j, SDL_GetError());
                ret++;
                continue;
            }

            dist = 0;
            dist += (R - Rd) * (R - Rd);
            dist += (G - Gd) * (G - Gd);
            dist += (B - Bd) * (B - Bd);

            /* Allow some difference in blending accuracy */
            if (dist > allowable_error) {
                ret++;
                if (ret == 1) {
                    sampleErrorX = i;
                    sampleErrorY = j;
                    sampleDist = dist;
                    sampleReference.r = Rd;
                    sampleReference.g = Gd;
                    sampleReference.b = Bd;
                    sampleReference.a = Ad;
                    sampleActual.r = R;
                    sampleActual.g = G;
                    sampleActual.b = B;
                    sampleActual.a = A;
                }
            }
        }
    }

    SDL_UnlockSurface(surface);
    SDL_UnlockSurface(referenceSurface);

    /* Save test image and reference for analysis on failures */
    _CompareSurfaceCount++;
    if (ret != 0) {
        SDLTest_LogError("Comparison of pixels with allowable error of %i failed %i times.", allowable_error, ret);
        LogErrorFormat("Reference surface format", referenceSurface->format);
        LogErrorFormat("Actual surface format   ", surface->format);
        SDLTest_LogError("First detected occurrence at position %i,%i with a squared RGB-difference of %i.", sampleErrorX, sampleErrorY, sampleDist);
        SDLTest_LogError("Reference pixel: R=%u G=%u B=%u A=%u", sampleReference.r, sampleReference.g, sampleReference.b, sampleReference.a);
        SDLTest_LogError("Actual pixel   : R=%u G=%u B=%u A=%u", sampleActual.r, sampleActual.g, sampleActual.b, sampleActual.a);
        (void)SDL_snprintf(imageFilename, FILENAME_SIZE - 1, "CompareSurfaces%04d_TestOutput.bmp", _CompareSurfaceCount);
        SDL_SaveBMP(surface, imageFilename);
        (void)SDL_snprintf(referenceFilename, FILENAME_SIZE - 1, "CompareSurfaces%04d_Reference.bmp", _CompareSurfaceCount);
        SDL_SaveBMP(referenceSurface, referenceFilename);
        SDLTest_LogError("Surfaces from failed comparison saved as '%s' and '%s'", imageFilename, referenceFilename);
    }

    return ret;
}
