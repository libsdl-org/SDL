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

#ifdef __ARM_FEATURE_SVE2

#include "SDL_sve2_extension.h"

/* *INDENT-OFF* */ // clang-format off
#define sdl_sve_rgb32_stride_impl(ma_sve_chn_iterator, ...)                     \
    sdl_sve_stride_loop_rgb32(uStride, vTailPred) {                             \
                                                                                \
        svuint16x4_t vSourceLow16x4 = svundef4_u16();                           \
        svuint16x4_t vSourceHigh16x4 = svundef4_u16();                          \
                                                                                \
        svuint16x4_t vTargetLow16x4 = svundef4_u16();                           \
        svuint16x4_t vTargetHigh16x4 = svundef4_u16();                          \
                                                                                \
        svld4ub_u16(vTailPred,                                                  \
                    (uint8_t *)pwSource,                                        \
                    &vSourceLow16x4,                                            \
                    &vSourceHigh16x4);                                          \
                                                                                \
        svld4ub_u16(vTailPred,                                                  \
                    (uint8_t *)pwTarget,                                        \
                    &vTargetLow16x4,                                            \
                    &vTargetHigh16x4);                                          \
                                                                                \
        /* process low half */                                                  \
        ma_sve_chn_iterator(vSourceLow16x4, vTargetLow16x4,                     \
            __VA_ARGS__                                                         \
        );                                                                      \
                                                                                \
        /* process high half */                                                 \
        ma_sve_chn_iterator( vSourceHigh16x4,vTargetHigh16x4,                   \
            __VA_ARGS__                                                         \
        );                                                                      \
                                                                                \
        svst4ub_u16(vTailPred,                                                  \
                    (uint8_t *)pwTarget,                                        \
                    vTargetLow16x4,                                             \
                    vTargetHigh16x4);                                           \
                                                                                \
        pwSource += sve_iteration_advance;                                      \
        pwTarget += sve_iteration_advance;                                      \
    }

/*-----------------------------------------------------------------------------*
 * Normal Blend with Alpha                                                     *
 *-----------------------------------------------------------------------------*/

static inline
ARM_NONNULL(1,2)
void sdl_sve_accc8888_stride_blend_to_nccc888_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
        
        if (sve_chn_idx == 3) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_accc8888_stride_blend_to_nccc888_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
        
        if (sve_chn_idx != 3){
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_ccca8888_stride_blend_to_cccn888_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
        
        if (sve_chn_idx == 0) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_ccca8888_stride_blend_to_cccn888_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
        
        if (sve_chn_idx != 0){
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}


/* *INDENT-ON* */ // clang-format off

static inline
ARM_NONNULL(1,3)
void sdl_sve_accc8888_blend_to_nccc888_fill_alpha(  
                                            uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_accc8888_stride_blend_to_nccc888_fill_alpha(
                                                        (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_accc8888_blend_to_nccc888_copy_alpha(  
                                            uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_accc8888_stride_blend_to_nccc888_copy_alpha(
                                                        (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_ccca8888_blend_to_cccn888_fill_alpha(  
                                            uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_ccca8888_stride_blend_to_cccn888_fill_alpha(
                                                        (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_ccca8888_blend_to_cccn888_copy_alpha(  
                                            uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_ccca8888_stride_blend_to_cccn888_copy_alpha(
                                                        (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}


void SDLCALL Blit8888to8888PixelAlphaSVE2(SDL_BlitInfo *info)
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
            sdl_sve_accc8888_blend_to_nccc888_fill_alpha(   src, 
                                                            srcstride, 
                                                            dst, 
                                                            dststride, 
                                                            width, 
                                                            height);
        } else {
            /* CCCA */
            assert(0 == (dstfmt->Ashift >> 3));
            sdl_sve_ccca8888_blend_to_cccn888_fill_alpha(   src, 
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
            sdl_sve_accc8888_blend_to_nccc888_copy_alpha(   src, 
                                                            srcstride, 
                                                            dst, 
                                                            dststride, 
                                                            width, 
                                                            height);
        } else {
            /* CCCA */
            assert(0 == (dstfmt->Ashift >> 3));
            sdl_sve_ccca8888_blend_to_cccn888_copy_alpha(   src, 
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

static inline
ARM_NONNULL(1,2)
void sdl_sve_a123_stride_blend_to_321a_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
        
        if (sve_src_chn_idx == 3) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_a123_stride_blend_to_321a_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
        
        if (sve_src_chn_idx != 3) {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_a123_blend_to_321a_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_a123_stride_blend_to_321a_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_a123_blend_to_321a_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_a123_stride_blend_to_321a_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_123a_stride_blend_to_a321_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
        
        if (sve_src_chn_idx == 0) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_123a_stride_blend_to_a321_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
        
        if (sve_src_chn_idx != 0) {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_123a_blend_to_a321_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_123a_stride_blend_to_a321_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_123a_blend_to_a321_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_123a_stride_blend_to_a321_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_accc_stride_blend_to_ccca_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
        
        if (sve_src_chn_idx == 3) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_accc_stride_blend_to_ccca_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
        
        if (sve_src_chn_idx != 3) {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_accc_blend_to_ccca_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_accc_stride_blend_to_ccca_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_accc_blend_to_ccca_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_accc_stride_blend_to_ccca_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_ccca_stride_blend_to_accc_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
        
        if (sve_src_chn_idx == 0) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_ccca_stride_blend_to_accc_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
        
        if (sve_src_chn_idx != 0) {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_ccca_blend_to_accc_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_ccca_stride_blend_to_accc_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_ccca_blend_to_accc_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_ccca_stride_blend_to_accc_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_a123_stride_blend_to_a321_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
        
        if (sve_src_chn_idx == 3) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_a123_stride_blend_to_a321_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
        
        if (sve_src_chn_idx != 3) {
            svuint16_t vMask = svget4(vSourceLow16x4, 3);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_a123_blend_to_a321_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_a123_stride_blend_to_a321_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_a123_blend_to_a321_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_a123_stride_blend_to_a321_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_123a_stride_blend_to_321a_fill_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
        
        if (sve_src_chn_idx == 0) {
            /* fill alpha */
            sve_target_u16 = svdup_u16(0xFF);
        } else {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,2)
void sdl_sve_123a_stride_blend_to_321a_copy_alpha(   
                                            uint32_t * SDL_RESTRICT pwSource,
                                            uint32_t * SDL_RESTRICT pwTarget,
                                            size_t uStride)
{
    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
        
        if (sve_src_chn_idx != 0) {
            svuint16_t vMask = svget4(vSourceLow16x4, 0);
            sve_target_u16 
                = sdl_sve_chn_blend_with_mask(  sve_source_u16, 
                                                sve_target_u16, 
                                                vMask);
        }
    );
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_123a_blend_to_321a_fill_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_123a_stride_blend_to_321a_fill_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

static inline
ARM_NONNULL(1,3)
void sdl_sve_123a_blend_to_321a_copy_alpha( uint8_t * SDL_RESTRICT pchSource, 
                                            size_t uSourceStride,
                                            uint8_t * SDL_RESTRICT pchTarget,
                                            size_t uTargetStride,
                                            int nWidth,
                                            int nHeight)
{
    while (nHeight--) {
        
        sdl_sve_123a_stride_blend_to_321a_copy_alpha(   (uint32_t *)pchSource,
                                                        (uint32_t *)pchTarget,
                                                        nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

void SDLCALL Blit8888to8888PixelAlphaSwizzleSVE2(SDL_BlitInfo *info)
{
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

#if 0
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
    switch (srcfmt->format) {
    case SDL_PIXELFORMAT_ARGB8888:

        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            Blit8888to8888PixelAlphaSVE2(info);
            break;
        
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888:
            if (fill_alpha) {
                sdl_sve_accc_blend_to_ccca_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_accc_blend_to_ccca_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_XBGR8888:
            if (fill_alpha) {
                sdl_sve_a123_blend_to_a321_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_a123_blend_to_a321_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888:
            if (fill_alpha) {
                sdl_sve_a123_blend_to_321a_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_a123_blend_to_321a_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        default:
            assert(false);
            break;
        }
        break;

    case SDL_PIXELFORMAT_RGBA8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_ccca_blend_to_accc_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_ccca_blend_to_accc_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888:
            Blit8888to8888PixelAlphaSVE2(info);
            break;

        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_XBGR8888:
            if (fill_alpha) {
                sdl_sve_123a_blend_to_a321_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_123a_blend_to_a321_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888:
            if (fill_alpha) {
                sdl_sve_123a_blend_to_321a_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_123a_blend_to_321a_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        default:
            assert(false);
            break;
        }
        break;

    case SDL_PIXELFORMAT_ABGR8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_a123_blend_to_a321_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_a123_blend_to_a321_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888:
            if (fill_alpha) {
                sdl_sve_a123_blend_to_321a_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_a123_blend_to_321a_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_XBGR8888:
            Blit8888to8888PixelAlphaSVE2(info);
            break;

        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888:
            if (fill_alpha) {
                sdl_sve_accc_blend_to_ccca_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_accc_blend_to_ccca_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        default:
            assert(false);
            break;
        }
        break;

    case SDL_PIXELFORMAT_BGRA8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_123a_blend_to_a321_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_123a_blend_to_a321_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;
        
        case SDL_PIXELFORMAT_RGBA8888:
        case SDL_PIXELFORMAT_RGBX8888:
            if (fill_alpha) {
                sdl_sve_123a_blend_to_321a_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_123a_blend_to_321a_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_ABGR8888:
        case SDL_PIXELFORMAT_XBGR8888:
            if (fill_alpha) {
                sdl_sve_ccca_blend_to_accc_fill_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            } else {
                sdl_sve_ccca_blend_to_accc_copy_alpha(  src, 
                                                        srcstride, 
                                                        dst, 
                                                        dststride, 
                                                        width, 
                                                        height);
            }
            break;

        case SDL_PIXELFORMAT_BGRA8888:
        case SDL_PIXELFORMAT_BGRX8888:
            Blit8888to8888PixelAlphaSVE2(info);
            break;
        default:
            assert(false);
            break;
        }
        break;

    default:
        assert(false);
        break;
    }


#endif
}


#endif /* __ARM_FEATURE_SVE2 */
#endif /* SDL_SVE2_INTRINSICS */