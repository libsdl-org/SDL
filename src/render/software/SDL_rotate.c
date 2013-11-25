/*

SDL_rotate.c: rotates 32bit or 8bit surfaces

Shamelessly stolen from SDL_gfx by Andreas Schiffler. Original copyright follows:

Copyright (C) 2001-2011  Andreas Schiffler

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
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

   3. This notice may not be removed or altered from any source
   distribution.

Andreas Schiffler -- aschiffler at ferzkopp dot net

*/
#include "../../SDL_internal.h"

#if defined(__WIN32__)
#include "../../core/windows/SDL_windows.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_rotate.h"

/* ---- Internally used structures */

/* !
\brief A 32 bit RGBA pixel.
*/
typedef struct tColorRGBA {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} tColorRGBA;

/* !
\brief A 8bit Y/palette pixel.
*/
typedef struct tColorY {
    Uint8 y;
} tColorY;

/* !
\brief Returns maximum of two numbers a and b.
*/
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))

/* !
\brief Number of guard rows added to destination surfaces.

This is a simple but effective workaround for observed issues.
These rows allocate extra memory and are then hidden from the surface.
Rows are added to the end of destination surfaces when they are allocated.
This catches any potential overflows which seem to happen with
just the right src image dimensions and scale/rotation and can lead
to a situation where the program can segfault.
*/
#define GUARD_ROWS (2)

/* !
\brief Lower limit of absolute zoom factor or rotation degrees.
*/
#define VALUE_LIMIT 0.001

/* !
\brief Returns colorkey info for a surface
*/
static Uint32
_colorkey(SDL_Surface *src)
{
    Uint32 key = 0;
    SDL_GetColorKey(src, &key);
    return key;
}


/* !
\brief Internal target surface sizing function for rotations with trig result return.

\param width The source surface width.
\param height The source surface height.
\param angle The angle to rotate in degrees.
\param dstwidth The calculated width of the destination surface.
\param dstheight The calculated height of the destination surface.
\param cangle The sine of the angle
\param sangle The cosine of the angle

*/
void
SDLgfx_rotozoomSurfaceSizeTrig(int width, int height, double angle,
                               int *dstwidth, int *dstheight,
                               double *cangle, double *sangle)
{
    double x, y, cx, cy, sx, sy;
    double radangle;
    int dstwidthhalf, dstheighthalf;

    /*
    * Determine destination width and height by rotating a centered source box
    */
    radangle = angle * (M_PI / 180.0);
    *sangle = SDL_sin(radangle);
    *cangle = SDL_cos(radangle);
    x = (double)(width / 2);
    y = (double)(height / 2);
    cx = *cangle * x;
    cy = *cangle * y;
    sx = *sangle * x;
    sy = *sangle * y;

    dstwidthhalf = MAX((int)
        SDL_ceil(MAX(MAX(MAX(SDL_fabs(cx + sy), SDL_fabs(cx - sy)), SDL_fabs(-cx + sy)), SDL_fabs(-cx - sy))), 1);
    dstheighthalf = MAX((int)
        SDL_ceil(MAX(MAX(MAX(SDL_fabs(sx + cy), SDL_fabs(sx - cy)), SDL_fabs(-sx + cy)), SDL_fabs(-sx - cy))), 1);
    *dstwidth = 2 * dstwidthhalf;
    *dstheight = 2 * dstheighthalf;
}


/* !
\brief Internal 32 bit rotozoomer with optional anti-aliasing.

Rotates and zooms 32 bit RGBA/ABGR 'src' surface to 'dst' surface based on the control
parameters by scanning the destination surface and applying optionally anti-aliasing
by bilinear interpolation.
Assumes src and dst surfaces are of 32 bit depth.
Assumes dst surface was allocated with the correct dimensions.

\param src Source surface.
\param dst Destination surface.
\param cx Horizontal center coordinate.
\param cy Vertical center coordinate.
\param isin Integer version of sine of angle.
\param icos Integer version of cosine of angle.
\param flipx Flag indicating horizontal mirroring should be applied.
\param flipy Flag indicating vertical mirroring should be applied.
\param smooth Flag indicating anti-aliasing should be used.
*/
static void
_transformSurfaceRGBA(SDL_Surface * src, SDL_Surface * dst, int cx, int cy, int isin, int icos, int flipx, int flipy, int smooth)
{
    int x, y, t1, t2, dx, dy, xd, yd, sdx, sdy, ax, ay, ex, ey, sw, sh;
    tColorRGBA c00, c01, c10, c11, cswap;
    tColorRGBA *pc, *sp;
    int gap;

    /*
    * Variable setup
    */
    xd = ((src->w - dst->w) << 15);
    yd = ((src->h - dst->h) << 15);
    ax = (cx << 16) - (icos * cx);
    ay = (cy << 16) - (isin * cx);
    sw = src->w - 1;
    sh = src->h - 1;
    pc = (tColorRGBA*) dst->pixels;
    gap = dst->pitch - dst->w * 4;

    /*
    * Switch between interpolating and non-interpolating code
    */
    if (smooth) {
        for (y = 0; y < dst->h; y++) {
            dy = cy - y;
            sdx = (ax + (isin * dy)) + xd;
            sdy = (ay - (icos * dy)) + yd;
            for (x = 0; x < dst->w; x++) {
                dx = (sdx >> 16);
                dy = (sdy >> 16);
                if (flipx) dx = sw - dx;
                if (flipy) dy = sh - dy;
                if ((dx > -1) && (dy > -1) && (dx < (src->w-1)) && (dy < (src->h-1))) {
                    sp = (tColorRGBA *)src->pixels;;
                    sp += ((src->pitch/4) * dy);
                    sp += dx;
                    c00 = *sp;
                    sp += 1;
                    c01 = *sp;
                    sp += (src->pitch/4);
                    c11 = *sp;
                    sp -= 1;
                    c10 = *sp;
                    if (flipx) {
                        cswap = c00; c00=c01; c01=cswap;
                        cswap = c10; c10=c11; c11=cswap;
                    }
                    if (flipy) {
                        cswap = c00; c00=c10; c10=cswap;
                        cswap = c01; c01=c11; c11=cswap;
                    }
                    /*
                    * Interpolate colors
                    */
                    ex = (sdx & 0xffff);
                    ey = (sdy & 0xffff);
                    t1 = ((((c01.r - c00.r) * ex) >> 16) + c00.r) & 0xff;
                    t2 = ((((c11.r - c10.r) * ex) >> 16) + c10.r) & 0xff;
                    pc->r = (((t2 - t1) * ey) >> 16) + t1;
                    t1 = ((((c01.g - c00.g) * ex) >> 16) + c00.g) & 0xff;
                    t2 = ((((c11.g - c10.g) * ex) >> 16) + c10.g) & 0xff;
                    pc->g = (((t2 - t1) * ey) >> 16) + t1;
                    t1 = ((((c01.b - c00.b) * ex) >> 16) + c00.b) & 0xff;
                    t2 = ((((c11.b - c10.b) * ex) >> 16) + c10.b) & 0xff;
                    pc->b = (((t2 - t1) * ey) >> 16) + t1;
                    t1 = ((((c01.a - c00.a) * ex) >> 16) + c00.a) & 0xff;
                    t2 = ((((c11.a - c10.a) * ex) >> 16) + c10.a) & 0xff;
                    pc->a = (((t2 - t1) * ey) >> 16) + t1;
                }
                sdx += icos;
                sdy += isin;
                pc++;
            }
            pc = (tColorRGBA *) ((Uint8 *) pc + gap);
        }
    } else {
        for (y = 0; y < dst->h; y++) {
            dy = cy - y;
            sdx = (ax + (isin * dy)) + xd;
            sdy = (ay - (icos * dy)) + yd;
            for (x = 0; x < dst->w; x++) {
                dx = (short) (sdx >> 16);
                dy = (short) (sdy >> 16);
                if (flipx) dx = (src->w-1)-dx;
                if (flipy) dy = (src->h-1)-dy;
                if ((dx >= 0) && (dy >= 0) && (dx < src->w) && (dy < src->h)) {
                    sp = (tColorRGBA *) ((Uint8 *) src->pixels + src->pitch * dy);
                    sp += dx;
                    *pc = *sp;
                }
                sdx += icos;
                sdy += isin;
                pc++;
            }
            pc = (tColorRGBA *) ((Uint8 *) pc + gap);
        }
    }
}

/* !

\brief Rotates and zooms 8 bit palette/Y 'src' surface to 'dst' surface without smoothing.

Rotates and zooms 8 bit RGBA/ABGR 'src' surface to 'dst' surface based on the control
parameters by scanning the destination surface.
Assumes src and dst surfaces are of 8 bit depth.
Assumes dst surface was allocated with the correct dimensions.

\param src Source surface.
\param dst Destination surface.
\param cx Horizontal center coordinate.
\param cy Vertical center coordinate.
\param isin Integer version of sine of angle.
\param icos Integer version of cosine of angle.
\param flipx Flag indicating horizontal mirroring should be applied.
\param flipy Flag indicating vertical mirroring should be applied.
*/
static void
transformSurfaceY(SDL_Surface * src, SDL_Surface * dst, int cx, int cy, int isin, int icos, int flipx, int flipy)
{
    int x, y, dx, dy, xd, yd, sdx, sdy, ax, ay;
    tColorY *pc, *sp;
    int gap;

    /*
    * Variable setup
    */
    xd = ((src->w - dst->w) << 15);
    yd = ((src->h - dst->h) << 15);
    ax = (cx << 16) - (icos * cx);
    ay = (cy << 16) - (isin * cx);
    pc = (tColorY*) dst->pixels;
    gap = dst->pitch - dst->w;
    /*
    * Clear surface to colorkey
    */
    SDL_memset(pc, (int)(_colorkey(src) & 0xff), dst->pitch * dst->h);
    /*
    * Iterate through destination surface
    */
    for (y = 0; y < dst->h; y++) {
        dy = cy - y;
        sdx = (ax + (isin * dy)) + xd;
        sdy = (ay - (icos * dy)) + yd;
        for (x = 0; x < dst->w; x++) {
            dx = (short) (sdx >> 16);
            dy = (short) (sdy >> 16);
            if (flipx) dx = (src->w-1)-dx;
            if (flipy) dy = (src->h-1)-dy;
            if ((dx >= 0) && (dy >= 0) && (dx < src->w) && (dy < src->h)) {
                sp = (tColorY *) (src->pixels);
                sp += (src->pitch * dy + dx);
                *pc = *sp;
            }
            sdx += icos;
            sdy += isin;
            pc++;
        }
        pc += gap;
    }
}


/* !
\brief Rotates and zooms a surface with different horizontal and vertival scaling factors and optional anti-aliasing.

Rotates a 32bit or 8bit 'src' surface to newly created 'dst' surface.
'angle' is the rotation in degrees, 'centerx' and 'centery' the rotation center. If 'smooth' is set
then the destination 32bit surface is anti-aliased. If the surface is not 8bit
or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.

\param src The surface to rotozoom.
\param angle The angle to rotate in degrees.
\param centerx The horizontal coordinate of the center of rotation
\param zoomy The vertical coordinate of the center of rotation
\param smooth Antialiasing flag; set to SMOOTHING_ON to enable.
\param flipx Set to 1 to flip the image horizontally
\param flipy Set to 1 to flip the image vertically
\param dstwidth The destination surface width
\param dstheight The destination surface height
\param cangle The angle cosine
\param sangle The angle sine
\return The new rotated surface.

*/

SDL_Surface *
SDLgfx_rotateSurface(SDL_Surface * src, double angle, int centerx, int centery, int smooth, int flipx, int flipy, int dstwidth, int dstheight, double cangle, double sangle)
{
    SDL_Surface *rz_src;
    SDL_Surface *rz_dst;
    int is32bit;
    int i, src_converted;
    Uint8 r,g,b;
    Uint32 colorkey = 0;
    int colorKeyAvailable = 0;
    double sangleinv, cangleinv;

    /*
    * Sanity check
    */
    if (src == NULL)
        return (NULL);

    if (src->flags & SDL_TRUE/* SDL_SRCCOLORKEY */)
    {
        colorkey = _colorkey(src);
        SDL_GetRGB(colorkey, src->format, &r, &g, &b);
        colorKeyAvailable = 1;
    }
    /*
    * Determine if source surface is 32bit or 8bit
    */
    is32bit = (src->format->BitsPerPixel == 32);
    if ((is32bit) || (src->format->BitsPerPixel == 8)) {
        /*
        * Use source surface 'as is'
        */
        rz_src = src;
        src_converted = 0;
    } else {
        /*
        * New source surface is 32bit with a defined RGBA ordering
        */
        rz_src =
            SDL_CreateRGBSurface(SDL_SWSURFACE, src->w, src->h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#else
            0xff000000,  0x00ff0000, 0x0000ff00, 0x000000ff
#endif
            );
        if(colorKeyAvailable)
            SDL_SetColorKey(src, 0, 0);

        SDL_BlitSurface(src, NULL, rz_src, NULL);

        if(colorKeyAvailable)
            SDL_SetColorKey(src, SDL_TRUE /* SDL_SRCCOLORKEY */, colorkey);
        src_converted = 1;
        is32bit = 1;
    }


    /* Determine target size */
    /* _rotozoomSurfaceSizeTrig(rz_src->w, rz_src->h, angle, &dstwidth, &dstheight, &cangle, &sangle); */

    /*
    * Calculate target factors from sin/cos and zoom
    */
    sangleinv = sangle*65536.0;
    cangleinv = cangle*65536.0;

    /*
    * Alloc space to completely contain the rotated surface
    */
    rz_dst = NULL;
    if (is32bit) {
        /*
        * Target surface is 32bit with source RGBA/ABGR ordering
        */
        rz_dst =
            SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight + GUARD_ROWS, 32,
            rz_src->format->Rmask, rz_src->format->Gmask,
            rz_src->format->Bmask, rz_src->format->Amask);
    } else {
        /*
        * Target surface is 8bit
        */
        rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight + GUARD_ROWS, 8, 0, 0, 0, 0);
    }

    /* Check target */
    if (rz_dst == NULL)
        return NULL;

    /* Adjust for guard rows */
    rz_dst->h = dstheight;

    if (colorKeyAvailable == 1){
        colorkey = SDL_MapRGB(rz_dst->format, r, g, b);

        SDL_FillRect(rz_dst, NULL, colorkey );
    }

    /*
    * Lock source surface
    */
    if (SDL_MUSTLOCK(rz_src)) {
        SDL_LockSurface(rz_src);
    }

    /*
    * Check which kind of surface we have
    */
    if (is32bit) {
        /*
        * Call the 32bit transformation routine to do the rotation (using alpha)
        */
        _transformSurfaceRGBA(rz_src, rz_dst, centerx, centery,
            (int) (sangleinv), (int) (cangleinv),
            flipx, flipy,
            smooth);
        /*
        * Turn on source-alpha support
        */
        /* SDL_SetAlpha(rz_dst, SDL_SRCALPHA, 255); */
        SDL_SetColorKey(rz_dst, /* SDL_SRCCOLORKEY */ SDL_TRUE | SDL_RLEACCEL, _colorkey(rz_src));
    } else {
        /*
        * Copy palette and colorkey info
        */
        for (i = 0; i < rz_src->format->palette->ncolors; i++) {
            rz_dst->format->palette->colors[i] = rz_src->format->palette->colors[i];
        }
        rz_dst->format->palette->ncolors = rz_src->format->palette->ncolors;
        /*
        * Call the 8bit transformation routine to do the rotation
        */
        transformSurfaceY(rz_src, rz_dst, centerx, centery,
            (int) (sangleinv), (int) (cangleinv),
            flipx, flipy);
        SDL_SetColorKey(rz_dst, /* SDL_SRCCOLORKEY */ SDL_TRUE | SDL_RLEACCEL, _colorkey(rz_src));
    }
    /*
    * Unlock source surface
    */
    if (SDL_MUSTLOCK(rz_src)) {
        SDL_UnlockSurface(rz_src);
    }

    /*
    * Cleanup temp surface
    */
    if (src_converted) {
        SDL_FreeSurface(rz_src);
    }

    /*
    * Return destination surface
    */
    return (rz_dst);
}
