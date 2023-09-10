#ifndef SDL_SDL_BLIT_A_SSE4_1_H
#define SDL_SDL_BLIT_A_SSE4_1_H

#ifdef SDL_SSE4_1_INTRINSICS
Uint32 AlignPixelToSDL_PixelFormat(Uint32 color, const SDL_PixelFormat* srcfmt, const SDL_PixelFormat* dstfmt);

__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatAlphaSplatMask_SSE4_1(const SDL_PixelFormat* dstfmt);

__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatAlphaSaturateMask_SSE4_1(const SDL_PixelFormat* dstfmt);

__m128i SDL_TARGETING("sse4.1") GetSDL_PixelFormatShuffleMask_SSE4_1(const SDL_PixelFormat* srcfmt, const SDL_PixelFormat* dstfmt);

__m128i SDL_TARGETING("sse4.1") MixRGBA_SSE4_1(__m128i src, __m128i dst, __m128i alpha_splat, __m128i alpha_saturate);

void SDL_TARGETING("sse4.1") BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo *info);

#endif

/* for compatibility with older compilers: */
#if defined(SDL_blit_A_sse4_1_c) || defined(SDL_blit_A_avx2_c)
/* _mm_loadu_si64 : missing in clang < 3.9, missing in gcc < 9
 * _mm_storeu_si64: missing in clang < 8.0, missing in gcc < 9
 * __m128i_u type (to be used to define the missing two above):
 *		      missing in gcc < 7, missing in clang < 9
 */
#if defined(__clang__)
#if (__clang_major__ < 9)
#define MISSING__m128i_u
#endif
#if (__clang_major__ < 8)
#define MISSING__mm_storeu_si64
#endif
#elif defined(__GNUC__)
#if (__GNUC__ < 7)
#define MISSING__m128i_u
#endif
#if (__GNUC__ < 9)
#define MISSING__mm_storeu_si64
#endif
#endif

#ifdef MISSING__m128i_u
typedef long long __m128i_u __attribute__((__vector_size__(16), __may_alias__, __aligned__(1)));
#endif
#ifdef MISSING__mm_storeu_si64
#define _mm_loadu_si64(_x) _mm_loadl_epi64((__m128i_u*)(_x))
#define _mm_storeu_si64(_x,_y)  _mm_storel_epi64((__m128i_u*)(_x),(_y))
#endif
#endif /**/

#endif //SDL_SDL_BLIT_A_SSE4_1_H
