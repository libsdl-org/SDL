#include "SDL_internal.h"

#if SDL_HAVE_BLIT_A

#ifdef SDL_SSE4_1_INTRINSICS

#define SDL_blit_A_sse4_1_c

#include "SDL_blit.h"
#include "SDL_blit_A_sse4_1.h"

/**
 * Using the SSE4.1 instruction set, blit four pixels with alpha blending
 * @param src A pointer to two 32-bit pixels of ARGB format to blit into dst
 * @param dst A pointer to two 32-bit pixels of ARGB format to retain visual data for while alpha blending
 * @return A 128-bit wide vector of two alpha-blended pixels in ARGB format
 */
__m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(__m128i src, __m128i dst) {
    __m128i src_color = _mm_cvtepu8_epi16(src);
    __m128i dst_color = _mm_cvtepu8_epi16(dst);
    /**
     * Combines a shuffle and an _mm_cvtepu8_epi16 operation into one operation by moving the lower 8 bits of the alpha
     * channel around to create 16-bit integers.
     */
    const __m128i SHUFFLE_ALPHA = _mm_set_epi8(
         -1, 7, -1, 7, -1, 7, -1, 7,
         -1, 3, -1, 3, -1, 3, -1, 3);
    __m128i alpha = _mm_shuffle_epi8(src, SHUFFLE_ALPHA);
    __m128i sub = _mm_sub_epi16(src_color, dst_color);
    __m128i mul = _mm_mullo_epi16(sub, alpha);
    const __m128i SHUFFLE_REDUCE = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1,
        15, 13, 11, 9, 7, 5, 3, 1);
    __m128i reduced = _mm_shuffle_epi8(mul, SHUFFLE_REDUCE);

    return _mm_add_epi8(reduced, dst);
}

Uint32 AlignPixelToSDL_PixelFormat(Uint32 color, const SDL_PixelFormat* srcFormat) {
    Uint8 a = (color >> srcFormat->Ashift) & 0xFF;
    Uint8 r = (color >> srcFormat->Rshift) & 0xFF;
    Uint8 g = (color >> srcFormat->Gshift) & 0xFF;
    Uint8 b = (color >> srcFormat->Bshift) & 0xFF;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

/*
 * This helper function converts arbitrary pixel format data into ARGB form with a 4 pixel-wide shuffle
 */
__m128i SDL_TARGETING("sse4.1") AlignPixelToSDL_PixelFormat_x4(__m128i colors, const SDL_PixelFormat* srcFormat) {
    // Create shuffle masks based on the source SDL_PixelFormat to ARGB
    __m128i srcShuffleMask = _mm_set_epi8(
        srcFormat->Ashift / 8 + 12, srcFormat->Rshift / 8 + 12, srcFormat->Gshift / 8 + 12, srcFormat->Bshift / 8 + 12,
        srcFormat->Ashift / 8 + 8, srcFormat->Rshift / 8 + 8, srcFormat->Gshift / 8 + 8, srcFormat->Bshift / 8 + 8,
        srcFormat->Ashift / 8 + 4, srcFormat->Rshift / 8 + 4, srcFormat->Gshift / 8 + 4, srcFormat->Bshift / 8 + 4,
        srcFormat->Ashift / 8, srcFormat->Rshift / 8, srcFormat->Gshift / 8, srcFormat->Bshift / 8
    );

    // Shuffle the colors
    return _mm_shuffle_epi8(colors, srcShuffleMask);
}

void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo* info) {
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;

    int chunks = width / 4;
    Uint8 *buffer = (Uint8*)SDL_malloc(chunks * 16 * sizeof(Uint8));

    while (height--) {
        /* Process 4-wide chunks of source color data that may be in wrong format into buffer */
        for (int i = 0; i < chunks; i += 1) {
            __m128i colors = _mm_loadu_si128((__m128i*)(src + i * 16));
            _mm_storeu_si128((__m128i*)(buffer + i * 16), AlignPixelToSDL_PixelFormat_x4(colors, srcfmt));
        }

        /* Alpha-blend in 2-wide chunks from buffer into destination */
        for (int i = 0; i < chunks * 2; i += 1) {
            __m128i c_src = _mm_loadu_si64((buffer + (i * 8)));
            __m128i c_dst = _mm_loadu_si64((dst + i * 8));
            __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst);
            _mm_storeu_si64(dst + i * 8, c_mix);
        }

        /* Handle remaining pixels when width is not a multiple of 4 */
        if (width % 4 != 0) {
            int remaining_pixels = width % 4;
            int offset = width - remaining_pixels;
            if (remaining_pixels >= 2) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si64(src_ptr);
                c_src = AlignPixelToSDL_PixelFormat_x4(c_src, srcfmt);
                __m128i c_dst = _mm_loadu_si64(dst_ptr);
                __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst);
                _mm_storeu_si64(dst_ptr, c_mix);
                remaining_pixels -= 2;
                offset += 2;
            }
            if (remaining_pixels == 1) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                Uint32 pixel = AlignPixelToSDL_PixelFormat(*src_ptr, srcfmt);
                /* Old GCC has bad or no _mm_loadu_si32 */
                #if defined(__GNUC__) && (__GNUC__ < 11)
                __m128i c_src = _mm_set_epi32(0, 0, 0, pixel);
                __m128i c_dst = _mm_set_epi32(0, 0, 0, *dst_ptr);
                #else
                __m128i c_src = _mm_loadu_si32(&pixel);
                __m128i c_dst = _mm_loadu_si32(dst_ptr);
                #endif
                __m128i mixed_pixel = MixRGBA_SSE4_1(c_src, c_dst);
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

    SDL_free(buffer);
}

#endif

#endif
