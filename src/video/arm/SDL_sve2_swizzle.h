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

#if !defined(SD_SVE2_SWIZZLE_H) //&& (defined(__ARM_FEATURE_SVE2) && __ARM_FEATURE_SVE2)
#define SD_SVE2_SWIZZLE_H

#include "SDL_sve2_extension.h"

#define sdl_sve_rgb32_stride_impl(ma_sve_chn_iterator, ...)   \
    sdl_sve_stride_loop_rgb32(uStride, vTailPred)             \
    {                                                         \
                                                              \
        svuint16x4_t vSourceLow16x4 = svundef4_u16();         \
        svuint16x4_t vSourceHigh16x4 = svundef4_u16();        \
                                                              \
        svuint16x4_t vTargetLow16x4 = svundef4_u16();         \
        svuint16x4_t vTargetHigh16x4 = svundef4_u16();        \
                                                              \
        svld4ub_u16(vTailPred,                                \
                    (uint8_t *)pwSource,                      \
                    &vSourceLow16x4,                          \
                    &vSourceHigh16x4);                        \
                                                              \
        svld4ub_u16(vTailPred,                                \
                    (uint8_t *)pwTarget,                      \
                    &vTargetLow16x4,                          \
                    &vTargetHigh16x4);                        \
                                                              \
        /* process low half */                                \
        ma_sve_chn_iterator(vSourceLow16x4, vTargetLow16x4,   \
                            __VA_ARGS__);                     \
                                                              \
        /* process high half */                               \
        ma_sve_chn_iterator(vSourceHigh16x4, vTargetHigh16x4, \
                            __VA_ARGS__);                     \
                                                              \
        svst4ub_u16(vTailPred,                                \
                    (uint8_t *)pwTarget,                      \
                    vTargetLow16x4,                           \
                    vTargetHigh16x4);                         \
                                                              \
        pwSource += sve_iteration_advance;                    \
        pwTarget += sve_iteration_advance;                    \
    }

#define sdl_sve_rgb32_no_alpha_stride_impl(                   \
    ma_alpha_idx,                                             \
    ma_sve_chn_iterator,                                      \
    ...)                                                      \
    sdl_sve_stride_loop_rgb32(uStride, vTailPred)             \
    {                                                         \
                                                              \
        svuint16x4_t vSourceLow16x4 = svundef4_u16();         \
        svuint16x4_t vSourceHigh16x4 = svundef4_u16();        \
                                                              \
        svuint16x4_t vTargetLow16x4 = svundef4_u16();         \
        svuint16x4_t vTargetHigh16x4 = svundef4_u16();        \
                                                              \
        svld4ub_u16(vTailPred,                                \
                    (uint8_t *)pwSource,                      \
                    &vSourceLow16x4,                          \
                    &vSourceHigh16x4);                        \
                                                              \
        svld4ub_u16(vTailPred,                                \
                    (uint8_t *)pwTarget,                      \
                    &vTargetLow16x4,                          \
                    &vTargetHigh16x4);                        \
                                                              \
        vSourceLow16x4 = svset4(vSourceLow16x4,               \
                                (ma_alpha_idx),               \
                                svdup_u16(0xFF));             \
        vSourceHigh16x4 = svset4(vSourceHigh16x4,             \
                                 (ma_alpha_idx),              \
                                 svdup_u16(0xFF));            \
                                                              \
        /* process low half */                                \
        ma_sve_chn_iterator(vSourceLow16x4, vTargetLow16x4,   \
                            __VA_ARGS__);                     \
                                                              \
        /* process high half */                               \
        ma_sve_chn_iterator(vSourceHigh16x4, vTargetHigh16x4, \
                            __VA_ARGS__);                     \
                                                              \
        svst4ub_u16(vTailPred,                                \
                    (uint8_t *)pwTarget,                      \
                    vTargetLow16x4,                           \
                    vTargetHigh16x4);                         \
                                                              \
        pwSource += sve_iteration_advance;                    \
        pwTarget += sve_iteration_advance;                    \
    }

#define sdl_sve_rgb32_to_rgb565_stride_impl(ma_sve_chn_iterator, ...) \
    sdl_sve_stride_loop_rgb32(uStride, vTailPred)                     \
    {                                                                 \
                                                                      \
        svuint16x4_t vSourceLow16x4 = svundef4_u16();                 \
        svuint16x4_t vSourceHigh16x4 = svundef4_u16();                \
                                                                      \
        svuint16x3_t vTargetLow16x3 = svundef3_u16();                 \
        svuint16x3_t vTargetHigh16x3 = svundef3_u16();                \
                                                                      \
        svld4ub_u16(vTailPred,                                        \
                    (uint8_t *)pwSource,                              \
                    &vSourceLow16x4,                                  \
                    &vSourceHigh16x4);                                \
                                                                      \
        svld3rgb565_u16(vTailPred,                                    \
                        phwTarget,                                    \
                        &vTargetLow16x3,                              \
                        &vTargetHigh16x3);                            \
                                                                      \
        ma_sve_chn_iterator(vSourceLow16x4, vTargetLow16x3,           \
                            __VA_ARGS__);                             \
                                                                      \
        ma_sve_chn_iterator(vSourceHigh16x4, vTargetHigh16x3,         \
                            __VA_ARGS__);                             \
                                                                      \
        svst3rgb565_u16(vTailPred,                                    \
                        phwTarget,                                    \
                        vTargetLow16x3,                               \
                        vTargetHigh16x3);                             \
                                                                      \
        pwSource += sve_iteration_advance;                            \
        phwTarget += sve_iteration_advance;                           \
    }

#define sdl_sve_rgb32_no_alpha_to_rgb565_stride_impl(         \
    ma_alpha_idx,                                             \
    ma_sve_chn_iterator,                                      \
    ...)                                                      \
    sdl_sve_stride_loop_rgb32(uStride, vTailPred)             \
    {                                                         \
                                                              \
        svuint16x4_t vSourceLow16x4 = svundef4_u16();         \
        svuint16x4_t vSourceHigh16x4 = svundef4_u16();        \
                                                              \
        svuint16x3_t vTargetLow16x3 = svundef3_u16();         \
        svuint16x3_t vTargetHigh16x3 = svundef3_u16();        \
                                                              \
        svld4ub_u16(vTailPred,                                \
                    (uint8_t *)pwSource,                      \
                    &vSourceLow16x4,                          \
                    &vSourceHigh16x4);                        \
                                                              \
        vSourceLow16x4 = svset4(vSourceLow16x4,               \
                                (ma_alpha_idx),               \
                                svdup_u16(0xFF));             \
        vSourceHigh16x4 = svset4(vSourceHigh16x4,             \
                                 (ma_alpha_idx),              \
                                 svdup_u16(0xFF));            \
                                                              \
        svld3rgb565_u16(vTailPred,                            \
                        phwTarget,                            \
                        &vTargetLow16x3,                      \
                        &vTargetHigh16x3);                    \
                                                              \
        ma_sve_chn_iterator(vSourceLow16x4, vTargetLow16x3,   \
                            __VA_ARGS__);                     \
                                                              \
        ma_sve_chn_iterator(vSourceHigh16x4, vTargetHigh16x3, \
                            __VA_ARGS__);                     \
                                                              \
        svst3rgb565_u16(vTailPred,                            \
                        phwTarget,                            \
                        vTargetLow16x3,                       \
                        vTargetHigh16x3);                     \
                                                              \
        pwSource += sve_iteration_advance;                    \
        phwTarget += sve_iteration_advance;                   \
    }

#ifndef sdl_sve_rgb32_blend_op_fill_alpha
#define sdl_sve_rgb32_blend_op_fill_alpha(ma_alpha_chn_idx)
#endif

#ifndef sdl_sve_rgb32_blend_op_copy_alpha
#define sdl_sve_rgb32_blend_op_copy_alpha(ma_alpha_chn_idx)
#endif

/*
 * Source: ACCC and CCCA
 */
SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_accc_stride_blend_to_accc_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,

                              sdl_sve_rgb32_blend_op_fill_alpha(3);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_accc_stride_blend_to_accc_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
                              sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_ccca_stride_blend_to_ccca_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,
                              sdl_sve_rgb32_blend_op_fill_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_ccca_stride_blend_to_ccca_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn,

                              sdl_sve_rgb32_blend_op_copy_alpha(0);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_accc_blend_to_accc_fill_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_accc_stride_blend_to_accc_fill_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_accc_blend_to_accc_copy_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_accc_stride_blend_to_accc_copy_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_ccca_blend_to_ccca_fill_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_ccca_stride_blend_to_ccca_fill_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_ccca_blend_to_ccca_copy_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_ccca_stride_blend_to_ccca_copy_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_a123_stride_blend_to_321a_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                              sdl_sve_rgb32_blend_op_fill_alpha(3);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_a123_stride_blend_to_321a_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                              sdl_sve_rgb32_blend_op_copy_alpha(3);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_a123_blend_to_321a_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_a123_stride_blend_to_321a_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_a123_blend_to_321a_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_a123_stride_blend_to_321a_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123a_stride_blend_to_a321_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                              sdl_sve_rgb32_blend_op_fill_alpha(0);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123a_stride_blend_to_a321_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                              sdl_sve_rgb32_blend_op_copy_alpha(0);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123a_blend_to_a321_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123a_stride_blend_to_a321_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123a_blend_to_a321_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123a_stride_blend_to_a321_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_accc_stride_blend_to_ccca_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
                              sdl_sve_rgb32_blend_op_fill_alpha(3);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_accc_stride_blend_to_ccca_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
                              sdl_sve_rgb32_blend_op_copy_alpha(3);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_accc_blend_to_ccca_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_accc_stride_blend_to_ccca_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_accc_blend_to_ccca_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_accc_stride_blend_to_ccca_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_ccca_stride_blend_to_accc_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
                              sdl_sve_rgb32_blend_op_fill_alpha(0);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_ccca_stride_blend_to_accc_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
                              sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_ccca_blend_to_accc_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_ccca_stride_blend_to_accc_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_ccca_blend_to_accc_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_ccca_stride_blend_to_accc_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_a123_stride_blend_to_a321_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
                              sdl_sve_rgb32_blend_op_fill_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_a123_stride_blend_to_a321_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
                              sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_a123_blend_to_a321_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_a123_stride_blend_to_a321_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_a123_blend_to_a321_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_a123_stride_blend_to_a321_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123a_stride_blend_to_321a_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
                              sdl_sve_rgb32_blend_op_fill_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123a_stride_blend_to_321a_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{
    sdl_sve_rgb32_stride_impl(sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
                              sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123a_blend_to_321a_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123a_stride_blend_to_321a_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123a_blend_to_321a_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123a_stride_blend_to_321a_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

/*
 * Source: XCCC and CCCX
 */

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xccc_stride_blend_to_accc_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn,
                                       sdl_sve_rgb32_blend_op_fill_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xccc_stride_blend_to_accc_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn,
                                       sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_cccx_stride_blend_to_ccca_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn,
                                       sdl_sve_rgb32_blend_op_fill_alpha(0);

    );
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_cccx_stride_blend_to_ccca_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn,
                                       sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xccc_blend_to_accc_fill_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_xccc_stride_blend_to_accc_fill_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xccc_blend_to_accc_copy_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_xccc_stride_blend_to_accc_copy_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_cccx_blend_to_ccca_fill_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_cccx_stride_blend_to_ccca_fill_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_cccx_blend_to_ccca_copy_alpha(
    uint8_t *SDL_RESTRICT pchSource,
    size_t uSourceStride,
    uint8_t *SDL_RESTRICT pchTarget,
    size_t uTargetStride,
    int nWidth,
    int nHeight)
{
    while (nHeight--) {

        sdl_sve_cccx_stride_blend_to_ccca_copy_alpha(
            (uint32_t *)pchSource,
            (uint32_t *)pchTarget,
            nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_x123_stride_blend_to_321a_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                                       sdl_sve_rgb32_blend_op_fill_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_x123_stride_blend_to_321a_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                                       sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_x123_blend_to_321a_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_x123_stride_blend_to_321a_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_x123_blend_to_321a_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_x123_stride_blend_to_321a_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123x_stride_blend_to_a321_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                                       sdl_sve_rgb32_blend_op_fill_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123x_stride_blend_to_a321_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev,
                                       sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123x_blend_to_a321_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123x_stride_blend_to_a321_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123x_blend_to_a321_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123x_stride_blend_to_a321_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xccc_stride_blend_to_ccca_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
                                       sdl_sve_rgb32_blend_op_fill_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xccc_stride_blend_to_ccca_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_accc_ccca,
                                       sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xccc_blend_to_ccca_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_xccc_stride_blend_to_ccca_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xccc_blend_to_ccca_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_xccc_stride_blend_to_ccca_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_cccx_stride_blend_to_accc_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
                                       sdl_sve_rgb32_blend_op_fill_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_cccx_stride_blend_to_accc_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_ccca_accc,
                                       sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_cccx_blend_to_accc_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_cccx_stride_blend_to_accc_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_cccx_blend_to_accc_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_cccx_stride_blend_to_accc_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_x123_stride_blend_to_a321_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
                                       sdl_sve_rgb32_blend_op_fill_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_x123_stride_blend_to_a321_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(3,
                                       sdl_sve_pixel_u16x4_foreach_chn_a123_a321,
                                       sdl_sve_rgb32_blend_op_copy_alpha(3););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_x123_blend_to_a321_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_x123_stride_blend_to_a321_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_x123_blend_to_a321_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_x123_stride_blend_to_a321_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123x_stride_blend_to_321a_fill_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{

    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
                                       sdl_sve_rgb32_blend_op_fill_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_123x_stride_blend_to_321a_copy_alpha(
    uint32_t *SDL_RESTRICT pwSource,
    uint32_t *SDL_RESTRICT pwTarget,
    size_t uStride)
{
    sdl_sve_rgb32_no_alpha_stride_impl(0,
                                       sdl_sve_pixel_u16x4_foreach_chn_123a_321a,
                                       sdl_sve_rgb32_blend_op_copy_alpha(0););
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123x_blend_to_321a_fill_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123x_stride_blend_to_321a_fill_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_123x_blend_to_321a_copy_alpha(uint8_t *SDL_RESTRICT pchSource,
                                                         size_t uSourceStride,
                                                         uint8_t *SDL_RESTRICT pchTarget,
                                                         size_t uTargetStride,
                                                         int nWidth,
                                                         int nHeight)
{
    while (nHeight--) {

        sdl_sve_123x_stride_blend_to_321a_copy_alpha((uint32_t *)pchSource,
                                                     (uint32_t *)pchTarget,
                                                     nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1)
static inline void sdl_sve_8888_to_8888_swizzle_dispatcher(SDL_BlitInfo *info)
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

    assert((srcbpp == 4) && (dstbpp == 4));

    bool fill_alpha = (!dstfmt->Amask);

    int srcstride = srcskip + srcbpp * width;
    int dststride = dstskip + dstbpp * width;

    switch (srcfmt->format) {
    case SDL_PIXELFORMAT_XRGB8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_xccc_blend_to_accc_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_xccc_blend_to_accc_copy_alpha(
                    src,
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
                sdl_sve_xccc_blend_to_ccca_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_xccc_blend_to_ccca_copy_alpha(src,
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
                sdl_sve_x123_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_x123_blend_to_a321_copy_alpha(src,
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
                sdl_sve_x123_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_x123_blend_to_321a_copy_alpha(src,
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

    case SDL_PIXELFORMAT_ARGB8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_accc_blend_to_accc_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_accc_blend_to_accc_copy_alpha(
                    src,
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
                sdl_sve_accc_blend_to_ccca_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_accc_blend_to_ccca_copy_alpha(src,
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
                sdl_sve_a123_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_a123_blend_to_a321_copy_alpha(src,
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
                sdl_sve_a123_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_a123_blend_to_321a_copy_alpha(src,
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

    case SDL_PIXELFORMAT_RGBX8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_cccx_blend_to_accc_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_cccx_blend_to_accc_copy_alpha(src,
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
                sdl_sve_cccx_blend_to_ccca_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_cccx_blend_to_ccca_copy_alpha(
                    src,
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
                sdl_sve_123x_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123x_blend_to_a321_copy_alpha(src,
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
                sdl_sve_123x_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123x_blend_to_321a_copy_alpha(src,
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
                sdl_sve_ccca_blend_to_accc_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_ccca_blend_to_accc_copy_alpha(src,
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
                sdl_sve_ccca_blend_to_ccca_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_ccca_blend_to_ccca_copy_alpha(
                    src,
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
                sdl_sve_123a_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123a_blend_to_a321_copy_alpha(src,
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
                sdl_sve_123a_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123a_blend_to_321a_copy_alpha(src,
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

    case SDL_PIXELFORMAT_XBGR8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_x123_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_x123_blend_to_a321_copy_alpha(src,
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
                sdl_sve_x123_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_x123_blend_to_321a_copy_alpha(src,
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
                sdl_sve_xccc_blend_to_accc_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_xccc_blend_to_accc_copy_alpha(
                    src,
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
                sdl_sve_xccc_blend_to_ccca_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_xccc_blend_to_ccca_copy_alpha(src,
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
                sdl_sve_a123_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_a123_blend_to_a321_copy_alpha(src,
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
                sdl_sve_a123_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_a123_blend_to_321a_copy_alpha(src,
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
                sdl_sve_accc_blend_to_accc_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_accc_blend_to_accc_copy_alpha(
                    src,
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
                sdl_sve_accc_blend_to_ccca_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_accc_blend_to_ccca_copy_alpha(src,
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

    case SDL_PIXELFORMAT_BGRX8888:
        switch (dstfmt->format) {
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
            if (fill_alpha) {
                sdl_sve_123x_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123x_blend_to_a321_copy_alpha(src,
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
                sdl_sve_123x_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123x_blend_to_321a_copy_alpha(src,
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
                sdl_sve_cccx_blend_to_accc_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_cccx_blend_to_accc_copy_alpha(src,
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
                sdl_sve_cccx_blend_to_ccca_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_cccx_blend_to_ccca_copy_alpha(
                    src,
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
                sdl_sve_123a_blend_to_a321_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123a_blend_to_a321_copy_alpha(src,
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
                sdl_sve_123a_blend_to_321a_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_123a_blend_to_321a_copy_alpha(src,
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
                sdl_sve_ccca_blend_to_accc_fill_alpha(src,
                                                      srcstride,
                                                      dst,
                                                      dststride,
                                                      width,
                                                      height);
            } else {
                sdl_sve_ccca_blend_to_accc_copy_alpha(src,
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
                sdl_sve_ccca_blend_to_ccca_fill_alpha(
                    src,
                    srcstride,
                    dst,
                    dststride,
                    width,
                    height);
            } else {
                sdl_sve_ccca_blend_to_ccca_copy_alpha(
                    src,
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

    default:
        assert(false);
        break;
    }
}

#ifndef sdl_sve_rgb32_blend_to_rgb565_op
#define sdl_sve_rgb32_blend_to_rgb565_op(ma_alpha_chn_idx)
#endif

/*
 * ACCC or CCCA
 */
SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_argb8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_to_rgb565_stride_impl(
        sdl_sve_pixel_u16x4_foreach_chn_argb_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(3);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_argb8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_argb8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_rgba8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_to_rgb565_stride_impl(
        sdl_sve_pixel_u16x4_foreach_chn_rgba_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(0);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_rgba8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_rgba8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_bgra8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_to_rgb565_stride_impl(
        sdl_sve_pixel_u16x4_foreach_chn_bgra_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(0);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_bgra8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_bgra8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_abgr8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_to_rgb565_stride_impl(
        sdl_sve_pixel_u16x4_foreach_chn_abgr_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(3);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_abgr8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_abgr8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

/*
 * XCCC or CCCX
 */
SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xrgb8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_no_alpha_to_rgb565_stride_impl(
        3,
        sdl_sve_pixel_u16x4_foreach_chn_argb_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(3);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xrgb8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_xrgb8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_rgbx8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_no_alpha_to_rgb565_stride_impl(
        0,
        sdl_sve_pixel_u16x4_foreach_chn_rgba_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(0);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_rgbx8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_rgbx8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_bgrx8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_no_alpha_to_rgb565_stride_impl(
        0,
        sdl_sve_pixel_u16x4_foreach_chn_bgra_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(0);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_bgrx8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_bgrx8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 2)
static inline void sdl_sve_xbgr8888_stride_blend_to_rgb565(uint32_t *SDL_RESTRICT pwSource,
                                                           uint16_t *SDL_RESTRICT phwTarget,
                                                           size_t uStride)
{
    sdl_sve_rgb32_no_alpha_to_rgb565_stride_impl(
        3,
        sdl_sve_pixel_u16x4_foreach_chn_abgr_rgb565,
        {
            sdl_sve_rgb32_blend_to_rgb565_op(3);
        });
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1, 3)
static inline void sdl_sve_xbgr8888_blend_to_rgb565(uint8_t *SDL_RESTRICT pchSource,
                                                    size_t uSourceStride,
                                                    uint8_t *SDL_RESTRICT pchTarget,
                                                    size_t uTargetStride,
                                                    int nWidth,
                                                    int nHeight)
{
    while (nHeight--) {

        sdl_sve_xbgr8888_stride_blend_to_rgb565((uint32_t *)pchSource,
                                                (uint16_t *)pchTarget,
                                                nWidth);

        pchSource += uSourceStride;
        pchTarget += uTargetStride;
    }
}

SDL_TARGETING("arch=armv8-a+sve2")
ARM_NONNULL(1)
static inline void sdl_sve_rgb32_to_rgb565_swizzle_dispatcher(SDL_BlitInfo *info)
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

    assert(srcbpp == 4);
    assert(dstbpp == 2);

    int srcstride = srcskip + srcbpp * width;
    int dststride = dstskip + dstbpp * width;

    switch (srcfmt->format) {
    case SDL_PIXELFORMAT_XRGB8888:
        sdl_sve_xrgb8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_ARGB8888:
        sdl_sve_argb8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_RGBX8888:
        sdl_sve_rgbx8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_RGBA8888:
        sdl_sve_rgba8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_XBGR8888:
        sdl_sve_xbgr8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_ABGR8888:
        sdl_sve_abgr8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_BGRX8888:
        sdl_sve_bgrx8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    case SDL_PIXELFORMAT_BGRA8888:
        sdl_sve_bgra8888_blend_to_rgb565(src,
                                         srcstride,
                                         dst,
                                         dststride,
                                         width,
                                         height);
        break;

    default:
        assert(false);
        break;
    }
}

#endif /* SD_SVE2_SWIZZLE_H */