#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_SSE4_1_INTRINSICS

#define SDL_blit_A_sse4_1_c

#include "SDL_blit.h"
#include "SDL_blit_A_sse4_1.h"

/**
 * A helper function to create an alpha mask for use with MixRGBA_SSE4_1 based on pixel format
 */
__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatAlphaMask_SSE4_1(const SDL_PixelFormat* dstfmt) {
    Uint8 index = dstfmt->Ashift / 4;
    /* Handle case where bad input sent */
    if (dstfmt->Ashift == dstfmt->Bshift && dstfmt->Ashift == 0) {
        index = 6;
    }
    return _mm_set_epi8(
            -1, index + 8, -1, index + 8, -1, index + 8, -1, index + 8,
            -1, index, -1, index, -1, index, -1, index);
}

/**
 * Using the SSE4.1 instruction set, blit four pixels with alpha blending
 * @param src_color A pointer to two 32-bit pixels of ARGB format to blit into dst
 * @param dst_color A pointer to two 32-bit pixels of ARGB format to retain visual data for while alpha blending
 * @return A 128-bit wide vector of two alpha-blended pixels in ARGB format
 */
__m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(const __m128i src_color, const __m128i dst_color, const __m128i alphaMask) {
    /**
     * Combines a shuffle and an _mm_cvtepu8_epi16 operation into one operation by moving the lower 8 bits of the alpha
     * channel around to create 16-bit integers.
     */
    __m128i alpha = _mm_shuffle_epi8(src_color, alphaMask);
    __m128i sub = _mm_sub_epi16(src_color, dst_color);
    __m128i mul = _mm_mullo_epi16(sub, alpha);
    mul = _mm_srli_epi16(mul, 8);

    return _mm_add_epi8(mul, dst_color);
}

Uint32 AlignPixelToSDL_PixelFormat(Uint32 color, const SDL_PixelFormat* srcfmt, const SDL_PixelFormat* dstfmt) {
    Uint8 a = (color >> srcfmt->Ashift) & 0xFF;
    Uint8 r = (color >> srcfmt->Rshift) & 0xFF;
    Uint8 g = (color >> srcfmt->Gshift) & 0xFF;
    Uint8 b = (color >> srcfmt->Bshift) & 0xFF;

    /* Handle case where bad input sent */
    Uint8 aShift = dstfmt->Ashift;
    if (aShift == dstfmt->Bshift && aShift == 0) {
        aShift = 24;
    }
    return (a << aShift) |
           (r << dstfmt->Rshift) |
           (g << dstfmt->Gshift) |
           (b << dstfmt->Bshift);
}

/*
 * This helper function converts arbitrary pixel formats into a shuffle mask for _mm_shuffle_epi8
 */
__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatShuffleMask(const SDL_PixelFormat* srcfmt,
                                                              const SDL_PixelFormat* dstfmt) {
    /* Calculate shuffle indices based on the source and destination SDL_PixelFormat */
    Uint8 shuffleIndices[16];
    Uint8 dstAshift = dstfmt->Ashift / 8;
    Uint8 dstRshift = dstfmt->Rshift / 8;
    Uint8 dstGshift = dstfmt->Gshift / 8;
    Uint8 dstBshift = dstfmt->Bshift / 8;
    /* Handle case where bad input sent */
    if (dstAshift == dstBshift && dstAshift == 0) {
        dstAshift = 3;
    }
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


void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo* info) {
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;
    SDL_PixelFormat *dstfmt = info->dst_fmt;

    int chunks = width / 4;
    const __m128i colorShiftMask = GetSDL_PixelFormatShuffleMask(srcfmt, dstfmt);
    const __m128i alphaMask = GetSDL_PixelFormatAlphaMask_SSE4_1(dstfmt);

    while (height--) {
        /* Process 4-wide chunks of source color data that may be in wrong format into buffer */
        for (int i = 0; i < chunks; i += 1) {
            __m128i c_src = _mm_loadu_si128((__m128i*)(src + i * 16));
            c_src = _mm_shuffle_epi8(c_src, colorShiftMask);
            __m128i c_dst = _mm_loadu_si128((__m128i*)(dst + i * 16));
            __m128i src_lo = _mm_unpacklo_epi8(c_src, _mm_setzero_si128());
            __m128i dst_lo = _mm_unpacklo_epi8(c_dst, _mm_setzero_si128());
            __m128i mix_lo = MixRGBA_SSE4_1(src_lo, dst_lo, alphaMask);
            __m128i src_hi = _mm_unpackhi_epi8(c_src, _mm_setzero_si128());
            __m128i dst_hi = _mm_unpackhi_epi8(c_dst, _mm_setzero_si128());
            __m128i mix_hi = MixRGBA_SSE4_1(src_hi, dst_hi, alphaMask);
            _mm_storeu_si128((__m128i*)(dst + i * 16), _mm_packus_epi16(mix_lo, mix_hi));
        }

        /* Handle remaining pixels when width is not a multiple of 4 */
        if (width % 4 != 0) {
            int remaining_pixels = width % 4;
            int offset = width - remaining_pixels;
            if (remaining_pixels >= 2) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si64(src_ptr);
                c_src = _mm_unpacklo_epi8(_mm_shuffle_epi8(c_src, colorShiftMask), _mm_setzero_si128());
                __m128i c_dst = _mm_unpacklo_epi8(_mm_loadu_si64(dst_ptr), _mm_setzero_si128());
                __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst, alphaMask);
                c_mix = _mm_packus_epi16(c_mix, _mm_setzero_si128());
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
                c_src = _mm_unpacklo_epi8(c_src, _mm_setzero_si128());
                c_dst = _mm_unpacklo_epi8(c_dst, _mm_setzero_si128());
                __m128i mixed_pixel = MixRGBA_SSE4_1(c_src, c_dst, alphaMask);
                mixed_pixel = _mm_srli_epi16(mixed_pixel, 8);
                mixed_pixel = _mm_unpacklo_epi8(mixed_pixel, _mm_setzero_si128());
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
}

#endif

#endif
