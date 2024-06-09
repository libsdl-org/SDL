#ifndef SDL_SDL_BLIT_A_AVX2_H
#define SDL_SDL_BLIT_A_AVX2_H
#if !defined(_MSC_VER) || (defined(_MSC_VER) && defined(__clang__))
__attribute__((target("avx2")))
#endif
void BlitNtoNPixelAlpha_AVX2(SDL_BlitInfo *info);
#endif //SDL_SDL_BLIT_A_AVX2_H
