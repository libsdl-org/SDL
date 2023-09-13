#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_SSE4_1_INTRINSICS

#define SDL_blit_A_sse4_1_c

#include "SDL_blit.h"
#include "SDL_blit_A_sse4_1.h"

/**
 * A helper function to create an alpha splat mask for use with MixRGBA_SSE4_1 based on pixel format
 */
__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatAlphaSplatMask_SSE4_1(const SDL_PixelFormat* dstfmt) {
    const Uint8 index = dstfmt->Ashift / 8;
    return _mm_set_epi8(
            index + 12, index + 12, index + 12, index + 12,
            index + 8, index + 8, index + 8, index + 8,
            index + 4, index + 4, index + 4, index + 4,
            index, index, index, index);
}

/**
 * A helper function to create an alpha saturate mask for use with MixRGBA_SSE4_1 based on pixel format
 */
__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatAlphaSaturateMask_SSE4_1(const SDL_PixelFormat* dstfmt) {
    const Uint8 bin = dstfmt->Ashift / 8;
    return _mm_set_epi8(
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0,
            bin == 3 ? 0xFF : 0, bin == 2 ? 0xFF : 0, bin == 1 ? 0xFF : 0, bin == 0 ? 0xFF : 0);
}

/**
 * This helper function converts arbitrary pixel formats into a shuffle mask for _mm_shuffle_epi8
 */
__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatShuffleMask_SSE4_1(const SDL_PixelFormat* srcfmt,
                                                                     const SDL_PixelFormat* dstfmt) {
    /* Calculate shuffle indices based on the source and destination SDL_PixelFormat */
    Uint8 shuffleIndices[16];
    Uint8 dstAshift = dstfmt->Ashift / 8;
    Uint8 dstRshift = dstfmt->Rshift / 8;
    Uint8 dstGshift = dstfmt->Gshift / 8;
    Uint8 dstBshift = dstfmt->Bshift / 8;
    for (int i = 0; i < 4; ++i) {
        shuffleIndices[dstAshift + i * 4] = srcfmt->Ashift / 8 + i * 4;
        shuffleIndices[dstRshift + i * 4] = srcfmt->Rshift / 8 + i * 4;
        shuffleIndices[dstGshift + i * 4] = srcfmt->Gshift / 8 + i * 4;
        shuffleIndices[dstBshift + i * 4] = srcfmt->Bshift / 8 + i * 4;
    }

    /* Create shuffle mask based on the calculated indices */
    return _mm_set_epi8(
            shuffleIndices[15], shuffleIndices[14], shuffleIndices[13], shuffleIndices[12],
            shuffleIndices[11], shuffleIndices[10], shuffleIndices[9], shuffleIndices[8],
            shuffleIndices[7], shuffleIndices[6], shuffleIndices[5], shuffleIndices[4],
            shuffleIndices[3], shuffleIndices[2], shuffleIndices[1], shuffleIndices[0]
    );
}

/**
 * Using the SSE4.1 instruction set, blit eight pixels into four with alpha blending
 */
__m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(__m128i src, __m128i dst,
                                               const __m128i alpha_splat, const __m128i alpha_saturate) {
    // SIMD implementation of blend_mul2.
    // dstRGB                            = (srcRGB * srcA) + (dstRGB * (1-srcA))
    // dstA   = srcA + (dstA * (1-srcA)) = (1      * srcA) + (dstA   * (1-srcA))

    // Splat the alpha into all channels for each pixel
    __m128i srca = _mm_shuffle_epi8(src, alpha_splat);

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

    // dst += dst >> 8;
    dst_lo = _mm_srli_epi16(_mm_add_epi16(dst_lo, _mm_srli_epi16(dst_lo, 8)), 8);
    dst_hi = _mm_srli_epi16(_mm_add_epi16(dst_hi, _mm_srli_epi16(dst_hi, 8)), 8);

    dst = _mm_packus_epi16(dst_lo, dst_hi);
    return dst;
}

Uint32 AlignPixelToSDL_PixelFormat(Uint32 color, const SDL_PixelFormat* srcfmt, const SDL_PixelFormat* dstfmt) {
    Uint8 a = (color >> srcfmt->Ashift) & 0xFF;
    Uint8 r = (color >> srcfmt->Rshift) & 0xFF;
    Uint8 g = (color >> srcfmt->Gshift) & 0xFF;
    Uint8 b = (color >> srcfmt->Bshift) & 0xFF;

    return (a << dstfmt->Ashift) |
           (r << dstfmt->Rshift) |
           (g << dstfmt->Gshift) |
           (b << dstfmt->Bshift);
}


void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo* info) {
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;
    SDL_PixelFormat *dstfmt = info->dst_fmt;

    const int chunks = width / 4;
    int free_format = 0;
    /* Handle case when passed invalid format, assume ARGB destination */
    if (dstfmt->Ashift == 0 && dstfmt->Ashift == dstfmt->Bshift) {
        dstfmt = SDL_CreatePixelFormat(SDL_PIXELFORMAT_ARGB8888);
        free_format = 1;
    }
    const __m128i shift_mask = GetSDL_PixelFormatShuffleMask_SSE4_1(srcfmt, dstfmt);
    const __m128i splat_mask = GetSDL_PixelFormatAlphaSplatMask_SSE4_1(dstfmt);
    const __m128i saturate_mask = GetSDL_PixelFormatAlphaSaturateMask_SSE4_1(dstfmt);

    while (height--) {
        for (int i = 0; i < chunks; i += 1) {
            __m128i colors = _mm_loadu_si128((__m128i*)(src + i * 16));
            colors = _mm_shuffle_epi8(colors, shift_mask);
            colors = MixRGBA_SSE4_1(colors, _mm_loadu_si128((__m128i*)(dst + i * 16)),
                                    splat_mask, saturate_mask);
            _mm_storeu_si128((__m128i*)(dst + i * 16), colors);
        }

        /* Handle remaining pixels when width is not a multiple of 4 */
        if (width % 4 != 0) {
            int remaining_pixels = width % 4;
            int offset = width - remaining_pixels;
            if (remaining_pixels >= 2) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si64(src_ptr);
                c_src = _mm_shuffle_epi8(c_src, shift_mask);
                __m128i c_dst = _mm_loadu_si64(dst_ptr);
                __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst, splat_mask, saturate_mask);
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
                __m128i mixed_pixel = MixRGBA_SSE4_1(c_src, c_dst, splat_mask, saturate_mask);
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
