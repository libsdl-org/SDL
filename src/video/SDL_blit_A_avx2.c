#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_AVX2_INTRINSICS

#define SDL_blit_A_avx2_c

#include "SDL_blit.h"

/**
 * Using the AVX2 instruction set, blit sixteen pixels into eight with alpha blending
 */
SDL_FORCE_INLINE __m256i SDL_TARGETING("avx2") MixRGBA_AVX2(
    __m256i src, __m256i dst,
    const __m256i alpha_shuffle, const __m256i alpha_saturate)
{
    // SIMD implementation of blend_mul2.
    // dstRGB                            = (srcRGB * srcA) + (dstRGB * (1-srcA))
    // dstA   = srcA + (dstA * (1-srcA)) = (1      * srcA) + (dstA   * (1-srcA))

    // Splat the alpha into all channels for each pixel
    __m256i srca = _mm256_shuffle_epi8(src, alpha_shuffle);

    // Set the alpha channels of src to 255
    src = _mm256_or_si256(src, alpha_saturate);

    __m256i src_lo = _mm256_unpacklo_epi8(src, _mm256_setzero_si256());
    __m256i src_hi = _mm256_unpackhi_epi8(src, _mm256_setzero_si256());

    __m256i dst_lo = _mm256_unpacklo_epi8(dst, _mm256_setzero_si256());
    __m256i dst_hi = _mm256_unpackhi_epi8(dst, _mm256_setzero_si256());

    __m256i srca_lo = _mm256_unpacklo_epi8(srca, _mm256_setzero_si256());
    __m256i srca_hi = _mm256_unpackhi_epi8(srca, _mm256_setzero_si256());

    // dst = ((src - dst) * srcA) + ((dst << 8) - dst)
    dst_lo = _mm256_add_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(src_lo, dst_lo), srca_lo),
                              _mm256_sub_epi16(_mm256_slli_epi16(dst_lo, 8), dst_lo));
    dst_hi = _mm256_add_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(src_hi, dst_hi), srca_hi),
                              _mm256_sub_epi16(_mm256_slli_epi16(dst_hi, 8), dst_hi));

    // dst += 0x1U (use 0x80 to round instead of floor)
    dst_lo = _mm256_add_epi16(dst_lo, _mm256_set1_epi16(1));
    dst_hi = _mm256_add_epi16(dst_hi, _mm256_set1_epi16(1));

    // dst += dst >> 8
    dst_lo = _mm256_srli_epi16(_mm256_add_epi16(dst_lo, _mm256_srli_epi16(dst_lo, 8)), 8);
    dst_hi = _mm256_srli_epi16(_mm256_add_epi16(dst_hi, _mm256_srli_epi16(dst_hi, 8)), 8);

    dst = _mm256_packus_epi16(dst_lo, dst_hi);
    return dst;
}

void SDL_TARGETING("avx2") BlitNtoNPixelAlpha_AVX2(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;
    SDL_PixelFormat *dstfmt = info->dst_fmt;

    const __m256i mask_offsets = _mm256_set_epi8(
        28, 28, 28, 28, 24, 24, 24, 24, 20, 20, 20, 20, 16, 16, 16, 16, 12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m256i shift_mask = _mm256_add_epi32(
        _mm256_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift) |
            ((srcfmt->Ashift >> 3) << dstfmt->Ashift)),
        mask_offsets);

    const __m256i splat_mask = _mm256_add_epi8(_mm256_set1_epi8(dstfmt->Ashift >> 3), mask_offsets);
    const __m256i saturate_mask = _mm256_set1_epi32((int)dstfmt->Amask);

    while (height--) {
        int i = 0;

        for (; i + 8 <= width; i += 8) {
            // Load 8 src pixels and shuffle into the dst format
            __m256i c_src = _mm256_shuffle_epi8(_mm256_loadu_si256((__m256i *)src), shift_mask);

            // Load 8 dst pixels
            __m256i c_dst = _mm256_loadu_si256((__m256i *)dst);

            // Blend the pixels together and save the result
            _mm256_storeu_si256((__m256i *)dst, MixRGBA_AVX2(c_src, c_dst, splat_mask, saturate_mask));

            src += 32;
            dst += 32;
        }

        for (; i < width; ++i) {
            Uint32 src32 = *(Uint32 *)src;
            Uint32 dst32 = *(Uint32 *)dst;

            src32 = (((src32 >> srcfmt->Rshift) & 0xFF) << dstfmt->Rshift) |
                    (((src32 >> srcfmt->Gshift) & 0xFF) << dstfmt->Gshift) |
                    (((src32 >> srcfmt->Bshift) & 0xFF) << dstfmt->Bshift) |
                    (((src32 >> srcfmt->Ashift) & 0xFF) << dstfmt->Ashift);

            ALPHA_BLEND_RGBA_4(src32, dst32, dstfmt->Ashift);

            *(Uint32 *)dst = dst32;

            src += 4;
            dst += 4;
        }

        src += srcskip;
        dst += dstskip;
    }
}

#endif

#endif
