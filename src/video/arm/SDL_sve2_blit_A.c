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

#include "SDL_sve2_blit_A.h"
#include <assert.h>

#ifdef SDL_SVE2_INTRINSICS

#undef sdl_sve_rgb32_blend_op_fill_alpha
#define sdl_sve_rgb32_blend_op_fill_alpha(ma_alpha_chn_idx)              \
    if (sve_src_chn_idx == (ma_alpha_chn_idx)) {                         \
        /* fill alpha */                                                 \
        sve_target_u16 = svdup_u16(0xFF);                                \
    } else {                                                             \
        svuint16_t vMask = svget4(sve_source_u16x4, (ma_alpha_chn_idx)); \
        sve_target_u16 = sdl_sve_chn_blend_with_mask(sve_source_u16,     \
                                                     sve_target_u16,     \
                                                     vMask);             \
    }

#undef sdl_sve_rgb32_blend_op_copy_alpha
#define sdl_sve_rgb32_blend_op_copy_alpha(ma_alpha_chn_idx)              \
    if (sve_src_chn_idx == (ma_alpha_chn_idx)) {                         \
        svuint16_t vMask = svget4(sve_source_u16x4, (ma_alpha_chn_idx)); \
        sve_target_u16 = sdl_sve_chn_blend_with_mask(svdup_u16(0xFF),    \
                                                     sve_target_u16,     \
                                                     vMask);             \
    } else {                                                             \
        svuint16_t vMask = svget4(sve_source_u16x4, (ma_alpha_chn_idx)); \
        sve_target_u16 = sdl_sve_chn_blend_with_mask(sve_source_u16,     \
                                                     sve_target_u16,     \
                                                     vMask);             \
    }

#undef sdl_sve_rgb32_blend_to_rgb565_op
#define sdl_sve_rgb32_blend_to_rgb565_op(ma_alpha_chn_idx)               \
    do {                                                                 \
        svuint16_t vMask = svget4(sve_source_u16x4, (ma_alpha_chn_idx)); \
        sve_target_u16 = sdl_sve_chn_blend_with_mask(sve_source_u16,     \
                                                     sve_target_u16,     \
                                                     vMask);             \
    } while (0)

#include "SDL_sve2_swizzle.h"

/*-----------------------------------------------------------------------------*
 * Normal Blend with Alpha                                                     *
 *-----------------------------------------------------------------------------*/
#if defined(SDL_PLATFORM_ANDROID)
__attribute__((target("arch=armv8-a+sve2")))
#endif
void Blit8888to8888PixelAlphaSVE2(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int srcbpp;
    int dstbpp;

    // Set up some basic variables
    srcbpp = srcfmt->bytes_per_pixel;
    dstbpp = dstfmt->bytes_per_pixel;

    assert(0 != srcfmt->Amask);

    int srcstride = srcskip + srcbpp * width;
    int dststride = dstskip + dstbpp * width;

#if 0 /* keep a default C implementation for reference and tests */
    while (height--) {

        Uint32 Pixel;
        unsigned sR, sG, sB, sA;
        unsigned dR, dG, dB, dA;
        uint8_t *srcline = src;
        uint8_t *dstline = dst;
        DUFFS_LOOP(
        {
        DISEMBLE_RGBA(srcline, srcbpp, srcfmt, Pixel, sR, sG, sB, sA);
        if (sA) {
            DISEMBLE_RGBA(dstline, dstbpp, dstfmt, Pixel, dR, dG, dB, dA);
            ALPHA_BLEND_RGBA(sR, sG, sB, sA, dR, dG, dB, dA);
            ASSEMBLE_RGBA(dstline, dstbpp, dstfmt, dR, dG, dB, dA);
        }
        srcline += srcbpp;
        dstline += dstbpp;
        },
        width);

        src += srcstride;
        dst += dststride;
    }
#else
    if (!dstfmt->Amask) {
        /* fill alpha */
        if (3 == dstfmt->Ashift >> 3) {
            /* ACCC */
            sdl_sve_accc_blend_to_accc_fill_alpha(  src,
                                                    srcstride,
                                                    dst,
                                                    dststride,
                                                    width,
                                                    height);
        } else {
            /* CCCA */
            assert(0 == (dstfmt->Ashift >> 3));
            sdl_sve_ccca_blend_to_ccca_fill_alpha(  src,
                                                    srcstride,
                                                    dst,
                                                    dststride,
                                                    width,
                                                    height);
        }
    } else {
        /* copy alpha */
        if (3 == dstfmt->Ashift >> 3) {
            /* ACCC */
            sdl_sve_accc_blend_to_accc_copy_alpha(  src,
                                                    srcstride,
                                                    dst,
                                                    dststride,
                                                    width,
                                                    height);
        } else {
            /* CCCA */
            assert(0 == (dstfmt->Ashift >> 3));
            sdl_sve_ccca_blend_to_ccca_copy_alpha(  src,
                                                    srcstride,
                                                    dst,
                                                    dststride,
                                                    width,
                                                    height);
        }
    }
#endif
}

/*-----------------------------------------------------------------------------*
 * Swizzle Blend with Alpha                                                    *
 *-----------------------------------------------------------------------------*/
#if defined(SDL_PLATFORM_ANDROID)
__attribute__((target("arch=armv8-a+sve2")))
#endif
void Blit8888to8888PixelAlphaSwizzleSVE2(SDL_BlitInfo *info)
{
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    assert(0 != srcfmt->Amask);
    (void)srcfmt;

#if 0

    int width = info->dst_w;
    int height = info->dst_h;
    uint8_t *src = info->src;
    int srcskip = info->src_skip;
    uint8_t *dst = info->dst;
    int dstskip = info->dst_skip;

    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;

    // Set up some basic variables
    int srcbpp = srcfmt->bytes_per_pixel;
    int dstbpp = dstfmt->bytes_per_pixel;

    assert(srcbpp == dstbpp == 4);
    assert(0 != srcfmt->Amask);
    bool fill_alpha = (!dstfmt->Amask);

    int srcstride = srcskip + srcbpp * width;
    int dststride = dstskip + dstbpp * width;


    while (height--) {

        uint32_t Pixel;
        unsigned sR, sG, sB, sA;
        unsigned dR, dG, dB, dA;
        uint8_t *srcline = src;
        uint8_t *dstline = dst;
        DUFFS_LOOP(
        {
        DISEMBLE_RGBA(srcline, srcbpp, srcfmt, Pixel, sR, sG, sB, sA);
        if (sA) {
            DISEMBLE_RGBA(dstline, dstbpp, dstfmt, Pixel, dR, dG, dB, dA);
            ALPHA_BLEND_RGBA(sR, sG, sB, sA, dR, dG, dB, dA);
            ASSEMBLE_RGBA(dstline, dstbpp, dstfmt, dR, dG, dB, dA);
        }
        srcline += srcbpp;
        dstline += dstbpp;
        },
        width);

        src += srcstride;
        dst += dststride;
    }
#else
    sdl_sve_8888_to_8888_swizzle_dispatcher(info);
#endif
}

#if defined(SDL_PLATFORM_ANDROID)
__attribute__((target("arch=armv8-a+sve2")))
#endif
void Blit8888to565PixelAlphaSwizzleSVE2(SDL_BlitInfo *info)
{
    sdl_sve_rgb32_to_rgb565_swizzle_dispatcher(info);
}

size_t SDL_GetSVEVectorSize(void)
{
    return svlen(svundef_u8()) * 8;
}

#endif /* SDL_SVE2_INTRINSICS */