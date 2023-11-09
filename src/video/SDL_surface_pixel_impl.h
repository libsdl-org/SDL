/*
  Copyright 1997-2023 Sam Lantinga <slouken@libsdl.org>
  Copyright 2023 Collabora Ltd.

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

#include "SDL3/SDL.h"

/* Internal implementation of SDL_ReadSurfacePixel, shared between SDL_shape
 * and SDLTest */
static int SDL_ReadSurfacePixel_impl(SDL_Surface *surface, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    Uint32 pixel = 0;
    size_t bytes_per_pixel;
    void *p;

    if (!surface || !surface->format || !surface->pixels) {
        return SDL_InvalidParamError("surface");
    }

    if (x < 0 || x >= surface->w) {
        return SDL_InvalidParamError("x");
    }

    if (y < 0 || y >= surface->h) {
        return SDL_InvalidParamError("y");
    }

    if (!r) {
        return SDL_InvalidParamError("r");
    }

    if (!g) {
        return SDL_InvalidParamError("g");
    }

    if (!b) {
        return SDL_InvalidParamError("b");
    }

    if (!a) {
        return SDL_InvalidParamError("a");
    }

    bytes_per_pixel = surface->format->BytesPerPixel;

    if (bytes_per_pixel > sizeof(pixel)) {
        return SDL_InvalidParamError("surface->format->BytesPerPixel");
    }

    SDL_LockSurface(surface);

    p = (Uint8 *)surface->pixels + y * surface->pitch + x * bytes_per_pixel;
    /* Fill the appropriate number of least-significant bytes of pixel,
     * leaving the most-significant bytes set to zero */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    SDL_memcpy(((Uint8 *) &pixel) + (sizeof(pixel) - bytes_per_pixel), p, bytes_per_pixel);
#else
    SDL_memcpy(&pixel, p, bytes_per_pixel);
#endif
    SDL_GetRGBA(pixel, surface->format, r, g, b, a);

    SDL_UnlockSurface(surface);
    return 0;
}
