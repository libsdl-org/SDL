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

#if !defined(SDL_SVE2_EXTENSION_H) && (defined(__ARM_FEATURE_SVE2) && __ARM_FEATURE_SVE2)
#define SDL_SVE2_EXTENSION_H

#include "SDL_sve2_util.h"
#include <arm_sve.h>
#include <stdint.h>

/*!
 * \brief a wrapper for __attribute__((nonnull))
 */
#ifndef ARM_NONNULL
#define ARM_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#endif

#define svlenu8()  svcntb_pat(SV_ALL)
#define svlenu16() (svcntb_pat(SV_ALL) / sizeof(uint16_t))
#define svlenu32() (svcntb_pat(SV_ALL) / sizeof(uint32_t))
#define svlenu64() (svcntb_pat(SV_ALL) / sizeof(uint64_t))

#define svlens8()  svlenu8()
#define svlens16() svlenu16()
#define svlens32() svlenu32()
#define svlens64() svlenu64()

#define sdl_sve_stride_loop_accc8888(ma_stride_size, ma_pred_name)       \
    for (svbool_t ma_pred_name, *pTemp = &ma_pred_name;                  \
         pTemp != NULL;                                                  \
         pTemp = NULL)                                                   \
        for (size_t SVE_SAFE_NAME(n) = 0,                                \
                    sve_iteration_advance = svlenu32() * 4;              \
             ({                                                          \
                 ma_pred_name = svwhilelt_b8((int32_t)SVE_SAFE_NAME(n),  \
                                             (int32_t)(ma_stride_size)); \
                 SVE_SAFE_NAME(n) < (ma_stride_size);                    \
             });                                                         \
             SVE_SAFE_NAME(n) += sve_iteration_advance)

#define sdl_sve_stride_loop_rgb32(ma_stride_size, ma_pred_name) \
    sdl_sve_stride_loop_accc8888(ma_stride_size, ma_pred_name)

#define sdl_sve_stride_loop_rgb16(ma_stride_size, ma_pred_name)           \
    for (svbool_t ma_pred_name, *pTemp = &ma_pred_name;                   \
         pTemp != NULL;                                                   \
         pTemp = NULL)                                                    \
        for (size_t SVE_SAFE_NAME(n) = 0,                                 \
                    sve_iteration_advance = svlenu16();                   \
             ({                                                           \
                 ma_pred_name = svwhilelt_b16((int32_t)SVE_SAFE_NAME(n),  \
                                              (int32_t)(ma_stride_size)); \
                 SVE_SAFE_NAME(n) < (ma_stride_size);                     \
             });                                                          \
             SVE_SAFE_NAME(n) += sve_iteration_advance)

#define sdl_sve_pixel_ccc_foreach_chn(ma_source_u16x3,                    \
                                      ma_target_u16x3,                    \
                                      ...)                                \
    do {                                                                  \
        svuint16x3_t sve_source_u16x3 = ma_source_u16x3;                  \
        (void)sve_source_u16x3;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget3((ma_source_u16x3), 0);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget3((ma_source_u16x3), 1);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget3((ma_source_u16x3), 2);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_accc_foreach_chn012(ma_source_u16x4,                \
                                          ma_target_u16x4,                \
                                          ...)                            \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_accc_foreach_chn(ma_source_u16x4,                   \
                                       ma_target_u16x4,                   \
                                       ...)                               \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn(ma_source_u16x4,                  \
                                        ma_target_u16x4,                  \
                                        ...)                              \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_src_dst_rev(ma_source_u16x4,      \
                                                    ma_target_u16x4,      \
                                                    ...)                  \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_accc_ccca(ma_source_u16x4,        \
                                                  ma_target_u16x4,        \
                                                  ...)                    \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_ccca_accc(ma_source_u16x4,        \
                                                  ma_target_u16x4,        \
                                                  ...)                    \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_a123_a321(ma_source_u16x4,        \
                                                  ma_target_u16x4,        \
                                                  ...)                    \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_123a_321a(ma_source_u16x4,        \
                                                  ma_target_u16x4,        \
                                                  ...)                    \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 3;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 3);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 3, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 2, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget4((ma_target_u16x4), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x4 = svset4(ma_target_u16x4, 1, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_argb_rgb565(ma_source_u16x4,      \
                                                    ma_target_u16x3,      \
                                                    ...)                  \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_rgba_rgb565(ma_source_u16x4,      \
                                                    ma_target_u16x3,      \
                                                    ...)                  \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_bgra_rgb565(ma_source_u16x4,      \
                                                    ma_target_u16x3,      \
                                                    ...)                  \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 3;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 3);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

#define sdl_sve_pixel_u16x4_foreach_chn_abgr_rgb565(ma_source_u16x4,      \
                                                    ma_target_u16x3,      \
                                                    ...)                  \
    do {                                                                  \
        svuint16x4_t sve_source_u16x4 = ma_source_u16x4;                  \
        (void)sve_source_u16x4;                                           \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 2;                            \
            const uint8_t sve_dst_chn_idx = 0;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 2);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 0);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 0, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 1;                            \
            const uint8_t sve_dst_chn_idx = 1;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 1);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 1);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 1, sve_target_u16); \
        } while (0);                                                      \
        do {                                                              \
            const uint8_t sve_src_chn_idx = 0;                            \
            const uint8_t sve_dst_chn_idx = 2;                            \
            (void)sve_src_chn_idx;                                        \
            (void)sve_dst_chn_idx;                                        \
            svuint16_t sve_source_u16 = svget4((ma_source_u16x4), 0);     \
            svuint16_t sve_target_u16 = svget3((ma_target_u16x3), 2);     \
            (void)sve_source_u16;                                         \
            (void)sve_target_u16;                                         \
            __VA_ARGS__                                                   \
            ma_target_u16x3 = svset3(ma_target_u16x3, 2, sve_target_u16); \
        } while (0);                                                      \
    } while (0)

static inline svuint16x3_t sdl_sve_rgb565_unpack(svuint16_t vPixels)
{
    svuint16x3_t vRGB16x3 = svundef3_u16();

    /* extract and zero-exten blue channel */
    vRGB16x3 = svset3_u16(vRGB16x3, 0, (vPixels & 0x1F) << 3);

    /* extract and zero-exten green channel */
    vRGB16x3 = svset3_u16(vRGB16x3, 1, (vPixels & (0x3F << 5)) >> 3);

    /* extract and zero-exten green channel */
    vRGB16x3 = svset3_u16(vRGB16x3, 2, (vPixels & (0x1F << 11)) >> 8);

    return vRGB16x3;
}

static inline svuint16_t sdl_sve_rgb565_pack(svuint16x3_t vRGB16x3)
{
    return (svget3_u16(vRGB16x3, 0) >> 3) | ((svget3_u16(vRGB16x3, 1) & (0x3F << 2)) << 3) | ((svget3_u16(vRGB16x3, 2) & (0x1F << 3)) << 8);
}

static inline ARM_NONNULL(2, 3, 4) void svld3rgb565_u16(svbool_t vPredu8,
                                                        uint16_t *phwSource,
                                                        svuint16x3_t *pvLow,
                                                        svuint16x3_t *pvHigh)
{
    svuint8x2_t vInput8x2 = svld2_u8(vPredu8, (uint8_t *)phwSource);

    svuint16_t vLowByteLowHalf = svunpklo_u16(svget2_u8(vInput8x2, 0));
    svuint16_t vLowByteHighHalf = svunpkhi_u16(svget2_u8(vInput8x2, 0));

    svuint16_t vHighByteLowHalf = svunpklo_u16(svget2_u8(vInput8x2, 1));
    svuint16_t vHighByteHighHalf = svunpkhi_u16(svget2_u8(vInput8x2, 1));

    *pvLow = sdl_sve_rgb565_unpack(vLowByteLowHalf | (vHighByteLowHalf << 8));
    *pvHigh = sdl_sve_rgb565_unpack(vLowByteHighHalf | (vHighByteHighHalf << 8));
}

static inline ARM_NONNULL(2) void svst3rgb565_u16(svbool_t vPredu8,
                                                  uint16_t *phwTarget,
                                                  svuint16x3_t vLow,
                                                  svuint16x3_t vHigh)
{
    svuint16_t vLowByteLowHalf = svundef_u16();
    svuint16_t vHighByteLowHalf = svundef_u16();

    /* pack low half pixels */
    do {
        svuint16_t vPixel = sdl_sve_rgb565_pack(vLow);

        vLowByteLowHalf = vPixel & 0xFF;
        vHighByteLowHalf = vPixel >> 8;
    } while (0);

    svuint16_t vLowByteHighHalf = svundef_u16();
    svuint16_t vHighByteHighHalf = svundef_u16();

    /* pack high half pixels */
    do {
        svuint16_t vPixel = sdl_sve_rgb565_pack(vHigh);

        vLowByteHighHalf = vPixel & 0xFF;
        vHighByteHighHalf = vPixel >> 8;
    } while (0);

    /* save rgb565 pixels */
    svuint8_t vLowByte = svuzp1_u8(svreinterpret_u8(vLowByteLowHalf),
                                   svreinterpret_u8(vLowByteHighHalf));

    svuint8_t vHighByte = svuzp1_u8(svreinterpret_u8(vHighByteLowHalf),
                                    svreinterpret_u8(vHighByteHighHalf));

    svst2_u8(vPredu8, (uint8_t *)phwTarget, svcreate2_u8(vLowByte, vHighByte));
}

static inline ARM_NONNULL(2, 3, 4) void svld4ub_u16(svbool_t vPredu8,
                                                    uint8_t *pchSource,
                                                    svuint16x4_t *pvLow,
                                                    svuint16x4_t *pvHigh)
{
    svuint8x4_t vInput8x4 = svld4_u8(vPredu8, pchSource);

    *pvLow = svset4_u16(*pvLow, 0, svunpklo_u16(svget4_u8(vInput8x4, 0)));
    *pvLow = svset4_u16(*pvLow, 1, svunpklo_u16(svget4_u8(vInput8x4, 1)));
    *pvLow = svset4_u16(*pvLow, 2, svunpklo_u16(svget4_u8(vInput8x4, 2)));
    *pvLow = svset4_u16(*pvLow, 3, svunpklo_u16(svget4_u8(vInput8x4, 3)));

    *pvHigh = svset4_u16(*pvHigh, 0, svunpkhi_u16(svget4_u8(vInput8x4, 0)));
    *pvHigh = svset4_u16(*pvHigh, 1, svunpkhi_u16(svget4_u8(vInput8x4, 1)));
    *pvHigh = svset4_u16(*pvHigh, 2, svunpkhi_u16(svget4_u8(vInput8x4, 2)));
    *pvHigh = svset4_u16(*pvHigh, 3, svunpkhi_u16(svget4_u8(vInput8x4, 3)));
}

static inline ARM_NONNULL(2) void svst4ub_u16(svbool_t vPredu8,
                                              uint8_t *pchTarget,
                                              svuint16x4_t vLow,
                                              svuint16x4_t vHigh)
{

    svuint8_t vCH0u8 = svuzp1_u8(svreinterpret_u8(svget4_u16(vLow, 0)),
                                 svreinterpret_u8(svget4_u16(vHigh, 0)));

    svuint8_t vCH1u8 = svuzp1_u8(svreinterpret_u8(svget4_u16(vLow, 1)),
                                 svreinterpret_u8(svget4_u16(vHigh, 1)));

    svuint8_t vCH2u8 = svuzp1_u8(svreinterpret_u8(svget4_u16(vLow, 2)),
                                 svreinterpret_u8(svget4_u16(vHigh, 2)));

    svuint8_t vCH3u8 = svuzp1_u8(svreinterpret_u8(svget4_u16(vLow, 3)),
                                 svreinterpret_u8(svget4_u16(vHigh, 3)));

    svst4_u8(vPredu8, pchTarget, svcreate4_u8(vCH0u8, vCH1u8, vCH2u8, vCH3u8));
}

/*! \note the Element range of vMask is [0, 0xFF]
 */
static inline svuint16_t sdl_sve_chn_blend_with_mask(svuint16_t vSource, svuint16_t vTarget, svuint16_t vMask)
{
    vMask = svadd_u16_m(svcmpeq_n_u16(svptrue_b16(), vMask, 255),
                        vMask,
                        svdup_u16(1));

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

/*! \note the hwOpacity range [0, 0x100]
 */
static inline svuint16_t sdl_sve_chn_blend_with_opacity(svuint16_t vSource,
                                                        svuint16_t vTarget,
                                                        uint16_t hwOpacity)
{
    svuint16_t vOpacity = svdup_u16(hwOpacity);

    vTarget = vSource * vOpacity + vTarget * (256 - vOpacity);
    return vTarget >> 8;
}

/*! \note the Element range of vMask is [0, 0xFF]
 *  \note the hwOpacity range [0, 0x100]
 */
static inline svuint16_t sdl_sve_chn_blend_with_mask_and_opacity(svuint16_t vSource,
                                                                 svuint16_t vTarget,
                                                                 svuint16_t vMask,
                                                                 uint16_t hwOpacity)
{
    vMask = svsel(svcmpeq_n_u16(svptrue_b16(), vMask, 255),
                  svdup_u16(hwOpacity),
                  (vMask * hwOpacity) >> 8);

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

/*! \note the Element range of vMask0/1 is [0, 0xFF]
 */
static inline svuint16_t sdl_sve_chn_blend_with_masks(svuint16_t vSource,
                                                      svuint16_t vTarget,
                                                      svuint16_t vMask0,
                                                      svuint16_t vMask1)
{
    vMask1 = svadd_u16_m(svcmpeq_n_u16(svptrue_b16(), vMask1, 255),
                         vMask1,
                         svdup_u16(1));

    svuint16_t vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask0, 255),
              vMask1,
              (vMask0 * vMask1) >> 8);

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

/*! \note the Element range of vMask0/1 is [0, 0xFF]
 *  \note the hwOpacity range [0, 0x100]
 */
static inline svuint16_t sdl_sve_chn_blend_with_masks_and_opacity(
    svuint16_t vSource,
    svuint16_t vTarget,
    svuint16_t vMask0,
    svuint16_t vMask1,
    uint16_t hwOpacity)
{
    vMask0 = svadd_u16_m(svcmpeq_n_u16(svptrue_b16(), vMask0, 255),
                         vMask0,
                         svdup_u16(1));

    svuint16_t vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask1, 255), /* >= 255 */
              vMask0,
              (vMask0 * vMask1) >> 8);

    vMask = svsel(svcmpge_n_u16(svptrue_b16(), vMask, 255),
                  svdup_u16(hwOpacity),
                  (vMask * hwOpacity) >> 8);

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

/*! \note the Element range of vMask0/1 is [0, 0xFF]
 */
static inline svuint16_t sdl_sve_chn_blend_with_3masks(svuint16_t vSource,
                                                       svuint16_t vTarget,
                                                       svuint16_t vMask0,
                                                       svuint16_t vMask1,
                                                       svuint16_t vMask2)
{
    vMask0 = svadd_u16_m(svcmpeq_n_u16(svptrue_b16(), vMask0, 255),
                         vMask0,
                         svdup_u16(1));

    svuint16_t vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask1, 255),
              vMask0,
              (vMask0 * vMask1) >> 8);

    vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask2, 255),
              vMask,
              (vMask * vMask2) >> 8);

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

/*! \note the Element range of vMask0/1 is [0, 0xFF]
 *  \note the hwOpacity range [0, 0x100]
 */
static inline svuint16_t sdl_sve_chn_blend_with_3masks_and_opacity(
    svuint16_t vSource,
    svuint16_t vTarget,
    svuint16_t vMask0,
    svuint16_t vMask1,
    svuint16_t vMask2,
    uint16_t hwOpacity)
{
    vMask0 = svadd_u16_m(svcmpeq_n_u16(svptrue_b16(), vMask0, 255),
                         vMask0,
                         svdup_u16(1));

    svuint16_t vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask1, 255),
              vMask0,
              (vMask0 * vMask1) >> 8);

    vMask =
        svsel(svcmpge_n_u16(svptrue_b16(), vMask2, 255),
              vMask,
              (vMask * vMask2) >> 8);

    vMask = svsel(svcmpge_n_u16(svptrue_b16(), vMask, 255),
                  svdup_u16(hwOpacity),
                  (vMask * hwOpacity) >> 8);

    vTarget = vSource * vMask + vTarget * (256 - vMask);
    return vTarget >> 8;
}

#endif /* SDL_SVE2_EXTENSION_H */