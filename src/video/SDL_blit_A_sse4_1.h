#ifndef SDL_SDL_BLIT_A_SSE4_1_H
#define SDL_SDL_BLIT_A_SSE4_1_H

#ifdef SDL_SSE4_1_INTRINSICS
Uint32 AlignPixelToSDL_PixelFormat(Uint32 color, const SDL_PixelFormat* srcFormat);

__m128i SDL_TARGETING("sse4.1") AlignPixelToSDL_PixelFormat_x4(__m128i colors, const SDL_PixelFormat* srcFormat);

__m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(__m128i src, __m128i dst);

void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo *info);

#endif

#endif //SDL_SDL_BLIT_A_SSE4_1_H
