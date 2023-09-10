#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_AVX2_INTRINSICS

#define SDL_blit_A_avx2_c

#include "SDL_blit.h"
#include "SDL_blit_A_sse4_1.h"

__m256i SDL_TARGETING("avx2") GetSDL_PixelFormatAlphaSplatMask_AVX2(const SDL_PixelFormat* dstfmt) {
    Uint8 index = dstfmt->Ashift / 8;
    return _mm256_set_epi8(
            index + 28, index + 28, index + 28, index + 28, index + 24, index + 24, index + 24, index + 24,
            index + 20, index + 20, index + 20, index + 20, index + 16, index + 16, index + 16, index + 16,
            index + 12, index + 12, index + 12, index + 12, index + 8, index + 8, index + 8, index + 8,
            index + 4, index + 4, index + 4, index + 4, index, index, index, index);
}

__m256i SDL_TARGETING("avx2") GetSDL_PixelFormatAlphaSaturateMask_AVX2(const SDL_PixelFormat* dstfmt) {
    const Uint8 bin = dstfmt->Ashift / 8;
    return _mm256_set_epi8(
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0);
}

__m256i SDL_TARGETING("avx2") GetSDL_PixelFormatShuffleMask_AVX2(const SDL_PixelFormat* srcfmt,
                                                              const SDL_PixelFormat* dstfmt) {
    /* Calculate shuffle indices based on the source and destination SDL_PixelFormat */
    Uint8 shuffleIndices[32];
    Uint8 dstAshift = dstfmt->Ashift / 8;
    Uint8 dstRshift = dstfmt->Rshift / 8;
    Uint8 dstGshift = dstfmt->Gshift / 8;
    Uint8 dstBshift = dstfmt->Bshift / 8;
    for (int i = 0; i < 8; ++i) {
        shuffleIndices[dstAshift + i * 4] = srcfmt->Ashift / 8 + i * 4;
        shuffleIndices[dstRshift + i * 4] = srcfmt->Rshift / 8 + i * 4;
        shuffleIndices[dstGshift + i * 4] = srcfmt->Gshift / 8 + i * 4;
        shuffleIndices[dstBshift + i * 4] = srcfmt->Bshift / 8 + i * 4;
    }

    /* Create shuffle mask based on the calculated indices */
    return _mm256_set_epi8(
            shuffleIndices[31], shuffleIndices[30], shuffleIndices[29], shuffleIndices[28],
            shuffleIndices[27], shuffleIndices[26], shuffleIndices[25], shuffleIndices[24],
            shuffleIndices[23], shuffleIndices[22], shuffleIndices[21], shuffleIndices[20],
            shuffleIndices[19], shuffleIndices[18], shuffleIndices[17], shuffleIndices[16],
            shuffleIndices[15], shuffleIndices[14], shuffleIndices[13], shuffleIndices[12],
            shuffleIndices[11], shuffleIndices[10], shuffleIndices[9], shuffleIndices[8],
            shuffleIndices[7], shuffleIndices[6], shuffleIndices[5], shuffleIndices[4],
            shuffleIndices[3], shuffleIndices[2], shuffleIndices[1], shuffleIndices[0]
    );
}

/**
 * Using the AVX2 instruction set, blit sixteen pixels into eight with alpha blending
 */
__m256i SDL_TARGETING("avx2") MixRGBA_AVX2(__m256i src, __m256i dst, const __m256i alpha_shuffle,
                                           const __m256i alpha_saturate) {
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

    // dst = (dst * 0x8081) >> 23
    dst_lo = _mm256_srli_epi16(_mm256_mulhi_epu16(dst_lo, _mm256_set1_epi16(-0x7F7F)), 7);
    dst_hi = _mm256_srli_epi16(_mm256_mulhi_epu16(dst_hi, _mm256_set1_epi16(-0x7F7F)), 7);

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

    int chunks = width / 8;
    int free_format = 0;
    /* Handle case when passed invalid format, assume ARGB destination */
    if (dstfmt->Ashift == 0 && dstfmt->Ashift == dstfmt->Bshift) {
        dstfmt = SDL_CreatePixelFormat(SDL_PIXELFORMAT_ARGB8888);
        free_format = 1;
    }
    const __m256i shift_mask = GetSDL_PixelFormatShuffleMask_AVX2(srcfmt, dstfmt);
    const __m256i splat_mask = GetSDL_PixelFormatAlphaSplatMask_AVX2(dstfmt);
    const __m256i saturate_mask = GetSDL_PixelFormatAlphaSaturateMask_AVX2(dstfmt);
    const __m128i sse4_1_shift_mask = GetSDL_PixelFormatShuffleMask_SSE4_1(srcfmt, dstfmt);
    const __m128i sse4_1_splat_mask = GetSDL_PixelFormatAlphaSplatMask_SSE4_1(dstfmt);
    const __m128i sse4_1_saturate_mask = GetSDL_PixelFormatAlphaSaturateMask_SSE4_1(dstfmt);

    while (height--) {
        /* Process 8-wide chunks of source color data that may be in wrong format */
        for (int i = 0; i < chunks; i += 1) {
            __m256i c_src = _mm256_shuffle_epi8(_mm256_loadu_si256((__m256i *) (src + i * 32)), shift_mask);
            /* Alpha-blend in 8-wide chunk from src into destination */
            __m256i c_dst = _mm256_loadu_si256((__m256i*) (dst + i * 32));
            __m256i c_mix = MixRGBA_AVX2(c_src, c_dst, splat_mask, saturate_mask);
            _mm256_storeu_si256((__m256i*) (dst + i * 32), c_mix);
        }

        /* Handle remaining pixels when width is not a multiple of 4 */
        if (width % 8 != 0) {
            int remaining_pixels = width % 8;
            int offset = width - remaining_pixels;
            if (remaining_pixels >= 4) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si128((__m128i*)src_ptr);
                c_src = _mm_shuffle_epi8(c_src, sse4_1_shift_mask);
                __m128i c_dst = _mm_loadu_si128((__m128i*)dst_ptr);
                __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst, sse4_1_splat_mask, sse4_1_saturate_mask);
                _mm_storeu_si128((__m128i*)dst_ptr, c_mix);
                remaining_pixels -= 4;
                offset += 4;
            }
            if (remaining_pixels >= 2) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si64(src_ptr);
                c_src = _mm_shuffle_epi8(c_src, sse4_1_shift_mask);
                __m128i c_dst = _mm_loadu_si64(dst_ptr);
                __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst, sse4_1_splat_mask, sse4_1_saturate_mask);
                _mm_storeu_si64(dst_ptr, c_mix);
                remaining_pixels -= 2;
                offset += 2;
            }
            if (remaining_pixels == 1) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                Uint32 pixel = AlignPixelToSDL_PixelFormat(*src_ptr, srcfmt, dstfmt);
                /* Old GCC has bad or no _mm_loadu_si32 */
                #if defined(__GNUC__) && (__GNUC__ < 11)
                __m128i c_src = _mm_set_epi32(0, 0, 0, pixel);
                __m128i c_dst = _mm_set_epi32(0, 0, 0, *dst_ptr);
                #else
                __m128i c_src = _mm_loadu_si32(&pixel);
                __m128i c_dst = _mm_loadu_si32(dst_ptr);
                #endif
                __m128i mixed_pixel = MixRGBA_SSE4_1(c_src, c_dst, sse4_1_splat_mask, sse4_1_saturate_mask);
                /* Old GCC has bad or no _mm_storeu_si32 */
                #if defined(__GNUC__) && (__GNUC__ < 11)
                *dst_ptr = _mm_extract_epi32(mixed_pixel, 0);
                #else
                _mm_storeu_si32(dst_ptr, mixed_pixel);
                #endif
            }
        }

        src += 4 * width;
        dst += 4 * width;

        src += srcskip;
        dst += dstskip;
    }
    if (free_format) {
        SDL_DestroyPixelFormat(dstfmt);
    }
}

#endif

#endif
