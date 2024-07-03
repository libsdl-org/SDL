#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_SSE4_1_INTRINSICS

#include "SDL_blit.h"

// Using the SSE4.1 instruction set, blit eight pixels into four with alpha blending
SDL_FORCE_INLINE __m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(
    __m128i src, __m128i dst,
    const __m128i alpha_shuffle, const __m128i alpha_saturate)
{
    // SIMD implementation of blend_mul2.
    // dstRGB                            = (srcRGB * srcA) + (dstRGB * (1-srcA))
    // dstA   = srcA + (dstA * (1-srcA)) = (1      * srcA) + (dstA   * (1-srcA))

    // Splat the alpha into all channels for each pixel
    __m128i srca = _mm_shuffle_epi8(src, alpha_shuffle);

    // Set the alpha channels of src to 255
    src = _mm_or_si128(src, alpha_saturate);

    __m128i src_lo = _mm_unpacklo_epi8(src, _mm_setzero_si128());
    __m128i src_hi = _mm_unpackhi_epi8(src, _mm_setzero_si128());

    __m128i dst_lo = _mm_unpacklo_epi8(dst, _mm_setzero_si128());
    __m128i dst_hi = _mm_unpackhi_epi8(dst, _mm_setzero_si128());

    __m128i srca_lo = _mm_unpacklo_epi8(srca, _mm_setzero_si128());
    __m128i srca_hi = _mm_unpackhi_epi8(srca, _mm_setzero_si128());

    // dst = ((src - dst) * srcA) + ((dst << 8) - dst)
    dst_lo = _mm_add_epi16(_mm_mullo_epi16(_mm_sub_epi16(src_lo, dst_lo), srca_lo),
                           _mm_sub_epi16(_mm_slli_epi16(dst_lo, 8), dst_lo));
    dst_hi = _mm_add_epi16(_mm_mullo_epi16(_mm_sub_epi16(src_hi, dst_hi), srca_hi),
                           _mm_sub_epi16(_mm_slli_epi16(dst_hi, 8), dst_hi));

    // dst += 0x1U (use 0x80 to round instead of floor)
    dst_lo = _mm_add_epi16(dst_lo, _mm_set1_epi16(1));
    dst_hi = _mm_add_epi16(dst_hi, _mm_set1_epi16(1));

    // dst += dst >> 8
    dst_lo = _mm_srli_epi16(_mm_add_epi16(dst_lo, _mm_srli_epi16(dst_lo, 8)), 8);
    dst_hi = _mm_srli_epi16(_mm_add_epi16(dst_hi, _mm_srli_epi16(dst_hi, 8)), 8);

    dst = _mm_packus_epi16(dst_lo, dst_hi);
    return dst;
}

void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;
    SDL_PixelFormat *dstfmt = info->dst_fmt;

    const __m128i mask_offsets = _mm_set_epi8(
        12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m128i shift_mask = _mm_add_epi32(
        _mm_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift) |
            ((srcfmt->Ashift >> 3) << dstfmt->Ashift)),
        mask_offsets);

    const __m128i splat_mask = _mm_add_epi8(_mm_set1_epi8(dstfmt->Ashift >> 3), mask_offsets);
    const __m128i saturate_mask = _mm_set1_epi32((int)dstfmt->Amask);

    while (height--) {
        int i = 0;

        for (; i + 4 <= width; i += 4) {
            // Load 4 src pixels and shuffle into the dst format
            __m128i c_src = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)src), shift_mask);

            // Load 4 dst pixels
            __m128i c_dst = _mm_loadu_si128((__m128i *)dst);

            // Blend the pixels together and save the result
            _mm_storeu_si128((__m128i *)dst, MixRGBA_SSE4_1(c_src, c_dst, splat_mask, saturate_mask));

            src += 16;
            dst += 16;
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
