#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_SSE4_1_INTRINSICS

#include "SDL_blit.h"

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

    // The byte offsets for the start of each pixel
    const __m128i mask_offsets = _mm_set_epi8(
        12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m128i convert_mask = _mm_add_epi32(
        _mm_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift)),
        mask_offsets);

    const __m128i alpha_splat_mask = _mm_add_epi8(_mm_set1_epi8(srcfmt->Ashift >> 3), mask_offsets);
    const __m128i alpha_fill_mask = _mm_set1_epi32((int)dstfmt->Amask);

    while (height--) {
        int i = 0;

        for (; i + 4 <= width; i += 4) {
            // Load 4 src pixels
            __m128i src128 = _mm_loadu_si128((__m128i *)src);

            // Load 4 dst pixels
            __m128i dst128 = _mm_loadu_si128((__m128i *)dst);

            // Extract the alpha from each pixel and splat it into all the channels
            __m128i srcA = _mm_shuffle_epi8(src128, alpha_splat_mask);

            // Convert to dst format
            src128 = _mm_shuffle_epi8(src128, convert_mask);

            // Set the alpha channels of src to 255
            src128 = _mm_or_si128(src128, alpha_fill_mask);

            __m128i src_lo = _mm_unpacklo_epi8(src128, _mm_setzero_si128());
            __m128i src_hi = _mm_unpackhi_epi8(src128, _mm_setzero_si128());

            __m128i dst_lo = _mm_unpacklo_epi8(dst128, _mm_setzero_si128());
            __m128i dst_hi = _mm_unpackhi_epi8(dst128, _mm_setzero_si128());

            __m128i srca_lo = _mm_unpacklo_epi8(srcA, _mm_setzero_si128());
            __m128i srca_hi = _mm_unpackhi_epi8(srcA, _mm_setzero_si128());

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

            // Blend the pixels together and save the result
            _mm_storeu_si128((__m128i *)dst, _mm_packus_epi16(dst_lo, dst_hi));

            src += 16;
            dst += 16;
        }

        for (; i < width; ++i) {
            Uint32 src32 = *(Uint32 *)src;
            Uint32 dst32 = *(Uint32 *)dst;

            Uint32 srcA = (src32 >> srcfmt->Ashift) & 0xFF;

            src32 = (((src32 >> srcfmt->Rshift) & 0xFF) << dstfmt->Rshift) |
                    (((src32 >> srcfmt->Gshift) & 0xFF) << dstfmt->Gshift) |
                    (((src32 >> srcfmt->Bshift) & 0xFF) << dstfmt->Bshift) |
                    dstfmt->Amask;

            Uint32 srcRB = src32 & 0x00FF00FF;
            Uint32 dstRB = dst32 & 0x00FF00FF;

            Uint32 srcGA = (src32 >> 8) & 0x00FF00FF;
            Uint32 dstGA = (dst32 >> 8) & 0x00FF00FF;

            Uint32 resRB = ((srcRB - dstRB) * srcA) + (dstRB << 8) - dstRB;
            resRB += 0x00010001;
            resRB += (resRB >> 8) & 0x00FF00FF;
            resRB = (resRB >> 8) & 0x00FF00FF;

            Uint32 resGA = ((srcGA - dstGA) * srcA) + (dstGA << 8) - dstGA;
            resGA += 0x00010001;
            resGA += (resGA >> 8) & 0x00FF00FF;
            resGA &= 0xFF00FF00;
            dst32 = resRB | resGA;

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
