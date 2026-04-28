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

#include "SDL_sve2_blit_N.h"
#include <assert.h>

#ifdef SDL_SVE2_INTRINSICS

#ifdef __ARM_FEATURE_SVE2

#include "SDL_sve2_extension.h"


void SDLCALL Blit8888to8888PixelSwizzleSVE2(SDL_BlitInfo *info)
{

}

void SDLCALL Blit8888to565PixelSVE2(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;
    
    int srcstride = srcskip + srcbpp * width;
    int dststride = dstskip + dstbpp * width;

    while (height--) {

        uint8_t *srcline = src;
        uint8_t *dstline = dst;
        for (int c = width; c; --c) {
            Uint32 Pixel;
            unsigned sR, sG, sB, sA;
            DISEMBLE_RGBA(srcline, srcbpp, srcfmt, Pixel, sR, sG, sB, sA);
            ASSEMBLE_RGBA(dstline, dstbpp, dstfmt, sR, sG, sB, sA);
            dstline += dstbpp;
            srcline += srcbpp;
        }
        src += srcstride;
        dst += dststride;
    }
}


#endif /* __ARM_FEATURE_SVE2 */
#endif /* SDL_SVE2_INTRINSICS */