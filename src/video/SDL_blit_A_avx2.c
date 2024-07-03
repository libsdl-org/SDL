#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_AVX2_INTRINSICS

#include "SDL_blit.h"

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

    // The byte offsets for the start of each pixel
    const __m256i mask_offsets = _mm256_set_epi8(
        28, 28, 28, 28, 24, 24, 24, 24, 20, 20, 20, 20, 16, 16, 16, 16, 12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m256i convert_mask = _mm256_add_epi32(
        _mm256_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift)),
        mask_offsets);

    const __m256i alpha_splat_mask = _mm256_add_epi8(_mm256_set1_epi8(srcfmt->Ashift >> 3), mask_offsets);
    const __m256i alpha_fill_mask = _mm256_set1_epi32((int)dstfmt->Amask);

    while (height--) {
        int i = 0;

        for (; i + 8 <= width; i += 8) {
            // Load 8 src pixels
            __m256i src256 = _mm256_loadu_si256((__m256i *)src);

            // Load 8 dst pixels
            __m256i dst256 = _mm256_loadu_si256((__m256i *)dst);

            // Extract the alpha from each pixel and splat it into all the channels
            __m256i srcA = _mm256_shuffle_epi8(src256, alpha_splat_mask);

            // Convert to dst format
            src256 = _mm256_shuffle_epi8(src256, convert_mask);

            // Set the alpha channels of src to 255
            src256 = _mm256_or_si256(src256, alpha_fill_mask);

            __m256i src_lo = _mm256_unpacklo_epi8(src256, _mm256_setzero_si256());
            __m256i src_hi = _mm256_unpackhi_epi8(src256, _mm256_setzero_si256());

            __m256i dst_lo = _mm256_unpacklo_epi8(dst256, _mm256_setzero_si256());
            __m256i dst_hi = _mm256_unpackhi_epi8(dst256, _mm256_setzero_si256());

            __m256i srca_lo = _mm256_unpacklo_epi8(srcA, _mm256_setzero_si256());
            __m256i srca_hi = _mm256_unpackhi_epi8(srcA, _mm256_setzero_si256());

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

            // Blend the pixels together and save the result
            _mm256_storeu_si256((__m256i *)dst, _mm256_packus_epi16(dst_lo, dst_hi));

            src += 32;
            dst += 32;
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
