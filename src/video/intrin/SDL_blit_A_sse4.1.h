#ifndef SDL_SDL_BLIT_A_SSE4_1_H
#define SDL_SDL_BLIT_A_SSE4_1_H

#ifdef SDL_SSE4_1_INTRINSICS
Uint32 convertPixelFormat(Uint32 color, const SDL_PixelFormat* srcFormat);

#if !defined(_MSC_VER) || (defined(_MSC_VER) && defined(__clang__))
__attribute__((target("sse4.1")))
#endif
__m128i convertPixelFormatsx4(__m128i colors, const SDL_PixelFormat* srcFormat);

#if !defined(_MSC_VER) || (defined(_MSC_VER) && defined(__clang__))
__attribute__((target("sse4.1")))
#endif
__m128i MixRGBA_SSE4_1(__m128i src, __m128i dst);

#if !defined(_MSC_VER) || (defined(_MSC_VER) && defined(__clang__))
__attribute__((target("sse4.1")))
#endif
void BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo *info);

#endif

#endif //SDL_SDL_BLIT_A_SSE4_1_H
