/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#include "SDL_audio_c.h"

#ifndef SDL_CPUINFO_DISABLED
#if defined(__x86_64__) && defined(SDL_SSE2_INTRINSICS)
#define NEED_SCALAR_CONVERTER_FALLBACKS 0 /* x86_64 guarantees SSE2. */
#elif defined(__MACOS__) && defined(SDL_SSE2_INTRINSICS)
#define NEED_SCALAR_CONVERTER_FALLBACKS 0 /* macOS/Intel guarantees SSE2. */
#elif defined(__ARM_ARCH) && (__ARM_ARCH >= 8) && defined(SDL_NEON_INTRINSICS)
#define NEED_SCALAR_CONVERTER_FALLBACKS 0 /* ARMv8+ promise NEON. */
#elif defined(__APPLE__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 7) && defined(SDL_NEON_INTRINSICS)
#define NEED_SCALAR_CONVERTER_FALLBACKS 0 /* All Apple ARMv7 chips promise NEON support. */
#endif
#endif

/* Set to zero if platform is guaranteed to use a SIMD codepath here. */
#if !defined(NEED_SCALAR_CONVERTER_FALLBACKS) || defined(SDL_CPUINFO_DISABLED)
#define NEED_SCALAR_CONVERTER_FALLBACKS 1
#endif

#define DIVBY128     0.0078125f
#define DIVBY32768   0.000030517578125f
#define DIVBY8388607 0.00000011920930376163766f

#if NEED_SCALAR_CONVERTER_FALLBACKS

/* these all convert backwards because (currently) float32 is >= to the size of anything it converts to, so it lets us safely convert in-place. */
#define AUDIOCVT_TOFLOAT_SCALAR(from, fromtype, equation) \
    static void SDL_Convert_##from##_to_F32_Scalar(float *dst, const fromtype *src, int num_samples) { \
        int i; \
        LOG_DEBUG_AUDIO_CONVERT(#from, "F32"); \
        for (i = num_samples - 1; i >= 0; --i) { \
            dst[i] = equation; \
        } \
    }

AUDIOCVT_TOFLOAT_SCALAR(S8, Sint8, ((float)src[i]) * DIVBY128)
AUDIOCVT_TOFLOAT_SCALAR(U8, Uint8, (((float)src[i]) * DIVBY128) - 1.0f)
AUDIOCVT_TOFLOAT_SCALAR(S16, Sint16, ((float)src[i]) * DIVBY32768)
AUDIOCVT_TOFLOAT_SCALAR(S32, Sint32, ((float)(src[i] >> 8)) * DIVBY8388607)
#undef AUDIOCVT_FROMFLOAT_SCALAR

/* these all convert forwards because (currently) float32 is >= to the size of anything it converts from, so it lets us safely convert in-place. */
#define AUDIOCVT_FROMFLOAT_SCALAR(to, totype, clampmin, clampmax, equation) \
    static void SDL_Convert_F32_to_##to##_Scalar(totype *dst, const float *src, int num_samples) { \
        int i; \
        LOG_DEBUG_AUDIO_CONVERT("F32", #to); \
        for (i = 0; i < num_samples; i++) { \
            const float sample = src[i]; \
            if (sample >= 1.0f) { \
                dst[i] = (totype) (clampmax); \
            } else if (sample <= -1.0f) { \
                dst[i] = (totype) (clampmin); \
            } else { \
                dst[i] = (totype) (equation); \
            } \
        } \
    }

AUDIOCVT_FROMFLOAT_SCALAR(S8, Sint8, -128, 127, sample * 127.0f);
AUDIOCVT_FROMFLOAT_SCALAR(U8, Uint8, 0, 255, (sample + 1.0f) * 127.0f);
AUDIOCVT_FROMFLOAT_SCALAR(S16, Sint16, -32768, 32767, sample * 32767.0f);
AUDIOCVT_FROMFLOAT_SCALAR(S32, Sint32, -2147483648LL, 2147483647, ((Sint32)(sample * 8388607.0f)) << 8);
#undef AUDIOCVT_FROMFLOAT_SCALAR

#endif /* NEED_SCALAR_CONVERTER_FALLBACKS */

#ifdef SDL_SSE2_INTRINSICS
static void SDL_TARGETING("sse2") SDL_Convert_S8_to_F32_SSE2(float *dst, const Sint8 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S8", "F32 (using SSE2)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 15)) & 15); --i, --src, --dst) {
        *dst = ((float)*src) * DIVBY128;
    }

    src -= 15;
    dst -= 15; /* adjust to read SSE blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128i *mmsrc = (const __m128i *)src;
        const __m128i zero = _mm_setzero_si128();
        const __m128 divby128 = _mm_set1_ps(DIVBY128);
        while (i >= 16) {                                /* 16 * 8-bit */
            const __m128i bytes = _mm_load_si128(mmsrc); /* get 16 sint8 into an XMM register. */
            /* treat as int16, shift left to clear every other sint16, then back right with sign-extend. Now sint16. */
            const __m128i shorts1 = _mm_srai_epi16(_mm_slli_epi16(bytes, 8), 8);
            /* right-shift-sign-extend gets us sint16 with the other set of values. */
            const __m128i shorts2 = _mm_srai_epi16(bytes, 8);
            /* unpack against zero to make these int32, shift to make them sign-extend, convert to float, multiply. Whew! */
            const __m128 floats1 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srai_epi32(_mm_slli_epi32(_mm_unpacklo_epi16(shorts1, zero), 16), 16)), divby128);
            const __m128 floats2 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srai_epi32(_mm_slli_epi32(_mm_unpacklo_epi16(shorts2, zero), 16), 16)), divby128);
            const __m128 floats3 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srai_epi32(_mm_slli_epi32(_mm_unpackhi_epi16(shorts1, zero), 16), 16)), divby128);
            const __m128 floats4 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_srai_epi32(_mm_slli_epi32(_mm_unpackhi_epi16(shorts2, zero), 16), 16)), divby128);
            /* Interleave back into correct order, store. */
            _mm_store_ps(dst, _mm_unpacklo_ps(floats1, floats2));
            _mm_store_ps(dst + 4, _mm_unpackhi_ps(floats1, floats2));
            _mm_store_ps(dst + 8, _mm_unpacklo_ps(floats3, floats4));
            _mm_store_ps(dst + 12, _mm_unpackhi_ps(floats3, floats4));
            i -= 16;
            mmsrc--;
            dst -= 16;
        }

        src = (const Sint8 *)mmsrc;
    }

    src += 15;
    dst += 15; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)*src) * DIVBY128;
        i--;
        src--;
        dst--;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_U8_to_F32_SSE2(float *dst, const Uint8 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("U8", "F32 (using SSE2)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 15)) & 15); --i, --src, --dst) {
        *dst = (((float)*src) * DIVBY128) - 1.0f;
    }

    src -= 15;
    dst -= 15; /* adjust to read SSE blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128i *mmsrc = (const __m128i *)src;
        const __m128i zero = _mm_setzero_si128();
        const __m128 divby128 = _mm_set1_ps(DIVBY128);
        const __m128 minus1 = _mm_set1_ps(-1.0f);
        while (i >= 16) {                                /* 16 * 8-bit */
            const __m128i bytes = _mm_load_si128(mmsrc); /* get 16 uint8 into an XMM register. */
            /* treat as int16, shift left to clear every other sint16, then back right with zero-extend. Now uint16. */
            const __m128i shorts1 = _mm_srli_epi16(_mm_slli_epi16(bytes, 8), 8);
            /* right-shift-zero-extend gets us uint16 with the other set of values. */
            const __m128i shorts2 = _mm_srli_epi16(bytes, 8);
            /* unpack against zero to make these int32, convert to float, multiply, add. Whew! */
            /* Note that AVX2 can do floating point multiply+add in one instruction, fwiw. SSE2 cannot. */
            const __m128 floats1 = _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(shorts1, zero)), divby128), minus1);
            const __m128 floats2 = _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(shorts2, zero)), divby128), minus1);
            const __m128 floats3 = _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(shorts1, zero)), divby128), minus1);
            const __m128 floats4 = _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(shorts2, zero)), divby128), minus1);
            /* Interleave back into correct order, store. */
            _mm_store_ps(dst, _mm_unpacklo_ps(floats1, floats2));
            _mm_store_ps(dst + 4, _mm_unpackhi_ps(floats1, floats2));
            _mm_store_ps(dst + 8, _mm_unpacklo_ps(floats3, floats4));
            _mm_store_ps(dst + 12, _mm_unpackhi_ps(floats3, floats4));
            i -= 16;
            mmsrc--;
            dst -= 16;
        }

        src = (const Uint8 *)mmsrc;
    }

    src += 15;
    dst += 15; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = (((float)*src) * DIVBY128) - 1.0f;
        i--;
        src--;
        dst--;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_S16_to_F32_SSE2(float *dst, const Sint16 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S16", "F32 (using SSE2)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 7)) & 15); --i, --src, --dst) {
        *dst = ((float)*src) * DIVBY32768;
    }

    src -= 7;
    dst -= 7; /* adjust to read SSE blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 divby32768 = _mm_set1_ps(DIVBY32768);
        while (i >= 8) {                                               /* 8 * 16-bit */
            const __m128i ints = _mm_load_si128((__m128i const *)src); /* get 8 sint16 into an XMM register. */
            /* treat as int32, shift left to clear every other sint16, then back right with sign-extend. Now sint32. */
            const __m128i a = _mm_srai_epi32(_mm_slli_epi32(ints, 16), 16);
            /* right-shift-sign-extend gets us sint32 with the other set of values. */
            const __m128i b = _mm_srai_epi32(ints, 16);
            /* Interleave these back into the right order, convert to float, multiply, store. */
            _mm_store_ps(dst, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi32(a, b)), divby32768));
            _mm_store_ps(dst + 4, _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi32(a, b)), divby32768));
            i -= 8;
            src -= 8;
            dst -= 8;
        }
    }

    src += 7;
    dst += 7; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)*src) * DIVBY32768;
        i--;
        src--;
        dst--;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_S32_to_F32_SSE2(float *dst, const Sint32 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S32", "F32 (using SSE2)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        *dst = ((float)(*src >> 8)) * DIVBY8388607;
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 divby8388607 = _mm_set1_ps(DIVBY8388607);
        const __m128i *mmsrc = (const __m128i *)src;
        while (i >= 4) { /* 4 * sint32 */
            /* shift out lowest bits so int fits in a float32. Small precision loss, but much faster. */
            _mm_store_ps(dst, _mm_mul_ps(_mm_cvtepi32_ps(_mm_srai_epi32(_mm_load_si128(mmsrc), 8)), divby8388607));
            i -= 4;
            mmsrc++;
            dst += 4;
        }
        src = (const Sint32 *)mmsrc;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)(*src >> 8)) * DIVBY8388607;
        i--;
        src++;
        dst++;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S8_SSE2(Sint8 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S8 (using SSE2)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 127;
        } else if (sample <= -1.0f) {
            *dst = -128;
        } else {
            *dst = (Sint8)(sample * 127.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 negone = _mm_set1_ps(-1.0f);
        const __m128 mulby127 = _mm_set1_ps(127.0f);
        __m128i *mmdst = (__m128i *)dst;
        while (i >= 16) {                                                                                                            /* 16 * float32 */
            const __m128i ints1 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src)), one), mulby127));      /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints2 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 4)), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints3 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 8)), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints4 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 12)), one), mulby127)); /* load 4 floats, clamp, convert to sint32 */
            _mm_store_si128(mmdst, _mm_packs_epi16(_mm_packs_epi32(ints1, ints2), _mm_packs_epi32(ints3, ints4)));                   /* pack down, store out. */
            i -= 16;
            src += 16;
            mmdst++;
        }
        dst = (Sint8 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 127;
        } else if (sample <= -1.0f) {
            *dst = -128;
        } else {
            *dst = (Sint8)(sample * 127.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_U8_SSE2(Uint8 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "U8 (using SSE2)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 255;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint8)((sample + 1.0f) * 127.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 negone = _mm_set1_ps(-1.0f);
        const __m128 mulby127 = _mm_set1_ps(127.0f);
        __m128i *mmdst = (__m128i *)dst;
        while (i >= 16) {                                                                                                                             /* 16 * float32 */
            const __m128i ints1 = _mm_cvtps_epi32(_mm_mul_ps(_mm_add_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src)), one), one), mulby127));      /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints2 = _mm_cvtps_epi32(_mm_mul_ps(_mm_add_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 4)), one), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints3 = _mm_cvtps_epi32(_mm_mul_ps(_mm_add_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 8)), one), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints4 = _mm_cvtps_epi32(_mm_mul_ps(_mm_add_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 12)), one), one), mulby127)); /* load 4 floats, clamp, convert to sint32 */
            _mm_store_si128(mmdst, _mm_packus_epi16(_mm_packs_epi32(ints1, ints2), _mm_packs_epi32(ints3, ints4)));                                   /* pack down, store out. */
            i -= 16;
            src += 16;
            mmdst++;
        }
        dst = (Uint8 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 255;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint8)((sample + 1.0f) * 127.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S16_SSE2(Sint16 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S16 (using SSE2)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 32767;
        } else if (sample <= -1.0f) {
            *dst = -32768;
        } else {
            *dst = (Sint16)(sample * 32767.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 negone = _mm_set1_ps(-1.0f);
        const __m128 mulby32767 = _mm_set1_ps(32767.0f);
        __m128i *mmdst = (__m128i *)dst;
        while (i >= 8) {                                                                                                              /* 8 * float32 */
            const __m128i ints1 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src)), one), mulby32767));     /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints2 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src + 4)), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            _mm_store_si128(mmdst, _mm_packs_epi32(ints1, ints2));                                                                    /* pack to sint16, store out. */
            i -= 8;
            src += 8;
            mmdst++;
        }
        dst = (Sint16 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 32767;
        } else if (sample <= -1.0f) {
            *dst = -32768;
        } else {
            *dst = (Sint16)(sample * 32767.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_TARGETING("sse2") SDL_Convert_F32_to_S32_SSE2(Sint32 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S32 (using SSE2)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 2147483647;
        } else if (sample <= -1.0f) {
            *dst = (Sint32)-2147483648LL;
        } else {
            *dst = ((Sint32)(sample * 8388607.0f)) << 8;
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));
    SDL_assert(!i || !(((size_t)src) & 15));

    {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 negone = _mm_set1_ps(-1.0f);
        const __m128 mulby8388607 = _mm_set1_ps(8388607.0f);
        __m128i *mmdst = (__m128i *)dst;
        while (i >= 4) {                                                                                                                                 /* 4 * float32 */
            _mm_store_si128(mmdst, _mm_slli_epi32(_mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_load_ps(src)), one), mulby8388607)), 8)); /* load 4 floats, clamp, convert to sint32 */
            i -= 4;
            src += 4;
            mmdst++;
        }
        dst = (Sint32 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 2147483647;
        } else if (sample <= -1.0f) {
            *dst = (Sint32)-2147483648LL;
        } else {
            *dst = ((Sint32)(sample * 8388607.0f)) << 8;
        }
        i--;
        src++;
        dst++;
    }
}
#endif

#ifdef SDL_NEON_INTRINSICS
static void SDL_Convert_S8_to_F32_NEON(float *dst, const Sint8 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S8", "F32 (using NEON)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 15)) & 15); --i, --src, --dst) {
        *dst = ((float)*src) * DIVBY128;
    }

    src -= 15;
    dst -= 15; /* adjust to read NEON blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const int8_t *mmsrc = (const int8_t *)src;
        const float32x4_t divby128 = vdupq_n_f32(DIVBY128);
        while (i >= 16) {                                            /* 16 * 8-bit */
            const int8x16_t bytes = vld1q_s8(mmsrc);                 /* get 16 sint8 into a NEON register. */
            const int16x8_t int16hi = vmovl_s8(vget_high_s8(bytes)); /* convert top 8 bytes to 8 int16 */
            const int16x8_t int16lo = vmovl_s8(vget_low_s8(bytes));  /* convert bottom 8 bytes to 8 int16 */
            /* split int16 to two int32, then convert to float, then multiply to normalize, store. */
            vst1q_f32(dst, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(int16lo))), divby128));
            vst1q_f32(dst + 4, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(int16lo))), divby128));
            vst1q_f32(dst + 8, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(int16hi))), divby128));
            vst1q_f32(dst + 12, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(int16hi))), divby128));
            i -= 16;
            mmsrc -= 16;
            dst -= 16;
        }

        src = (const Sint8 *)mmsrc;
    }

    src += 15;
    dst += 15; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)*src) * DIVBY128;
        i--;
        src--;
        dst--;
    }
}

static void SDL_Convert_U8_to_F32_NEON(float *dst, const Uint8 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("U8", "F32 (using NEON)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 15)) & 15); --i, --src, --dst) {
        *dst = (((float)*src) * DIVBY128) - 1.0f;
    }

    src -= 15;
    dst -= 15; /* adjust to read NEON blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const uint8_t *mmsrc = (const uint8_t *)src;
        const float32x4_t divby128 = vdupq_n_f32(DIVBY128);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        while (i >= 16) {                                              /* 16 * 8-bit */
            const uint8x16_t bytes = vld1q_u8(mmsrc);                  /* get 16 uint8 into a NEON register. */
            const uint16x8_t uint16hi = vmovl_u8(vget_high_u8(bytes)); /* convert top 8 bytes to 8 uint16 */
            const uint16x8_t uint16lo = vmovl_u8(vget_low_u8(bytes));  /* convert bottom 8 bytes to 8 uint16 */
            /* split uint16 to two uint32, then convert to float, then multiply to normalize, subtract to adjust for sign, store. */
            vst1q_f32(dst, vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_low_u16(uint16lo))), divby128));
            vst1q_f32(dst + 4, vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_high_u16(uint16lo))), divby128));
            vst1q_f32(dst + 8, vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_low_u16(uint16hi))), divby128));
            vst1q_f32(dst + 12, vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_high_u16(uint16hi))), divby128));
            i -= 16;
            mmsrc -= 16;
            dst -= 16;
        }

        src = (const Uint8 *)mmsrc;
    }

    src += 15;
    dst += 15; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = (((float)*src) * DIVBY128) - 1.0f;
        i--;
        src--;
        dst--;
    }
}

static void SDL_Convert_S16_to_F32_NEON(float *dst, const Sint16 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S16", "F32 (using NEON)");

    src += num_samples - 1;
    dst += num_samples - 1;

    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for (i = num_samples; i && (((size_t)(dst - 7)) & 15); --i, --src, --dst) {
        *dst = ((float)*src) * DIVBY32768;
    }

    src -= 7;
    dst -= 7; /* adjust to read NEON blocks from the start. */
    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby32768 = vdupq_n_f32(DIVBY32768);
        while (i >= 8) {                                            /* 8 * 16-bit */
            const int16x8_t ints = vld1q_s16((int16_t const *)src); /* get 8 sint16 into a NEON register. */
            /* split int16 to two int32, then convert to float, then multiply to normalize, store. */
            vst1q_f32(dst, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(ints))), divby32768));
            vst1q_f32(dst + 4, vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(ints))), divby32768));
            i -= 8;
            src -= 8;
            dst -= 8;
        }
    }

    src += 7;
    dst += 7; /* adjust for any scalar finishing. */

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)*src) * DIVBY32768;
        i--;
        src--;
        dst--;
    }
}

static void SDL_Convert_S32_to_F32_NEON(float *dst, const Sint32 *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("S32", "F32 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        *dst = ((float)(*src >> 8)) * DIVBY8388607;
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby8388607 = vdupq_n_f32(DIVBY8388607);
        const int32_t *mmsrc = (const int32_t *)src;
        while (i >= 4) { /* 4 * sint32 */
            /* shift out lowest bits so int fits in a float32. Small precision loss, but much faster. */
            vst1q_f32(dst, vmulq_f32(vcvtq_f32_s32(vshrq_n_s32(vld1q_s32(mmsrc), 8)), divby8388607));
            i -= 4;
            mmsrc += 4;
            dst += 4;
        }
        src = (const Sint32 *)mmsrc;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = ((float)(*src >> 8)) * DIVBY8388607;
        i--;
        src++;
        dst++;
    }
}

static void SDL_Convert_F32_to_S8_NEON(Sint8 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S8 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 127;
        } else if (sample <= -1.0f) {
            *dst = -128;
        } else {
            *dst = (Sint8)(sample * 127.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby127 = vdupq_n_f32(127.0f);
        int8_t *mmdst = (int8_t *)dst;
        while (i >= 16) {                                                                                                       /* 16 * float32 */
            const int32x4_t ints1 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src)), one), mulby127));      /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints2 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 4)), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints3 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 8)), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints4 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 12)), one), mulby127)); /* load 4 floats, clamp, convert to sint32 */
            const int8x8_t i8lo = vmovn_s16(vcombine_s16(vmovn_s32(ints1), vmovn_s32(ints2)));                                  /* narrow to sint16, combine, narrow to sint8 */
            const int8x8_t i8hi = vmovn_s16(vcombine_s16(vmovn_s32(ints3), vmovn_s32(ints4)));                                  /* narrow to sint16, combine, narrow to sint8 */
            vst1q_s8(mmdst, vcombine_s8(i8lo, i8hi));                                                                           /* combine to int8x16_t, store out */
            i -= 16;
            src += 16;
            mmdst += 16;
        }
        dst = (Sint8 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 127;
        } else if (sample <= -1.0f) {
            *dst = -128;
        } else {
            *dst = (Sint8)(sample * 127.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_Convert_F32_to_U8_NEON(Uint8 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "U8 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 255;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint8)((sample + 1.0f) * 127.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby127 = vdupq_n_f32(127.0f);
        uint8_t *mmdst = (uint8_t *)dst;
        while (i >= 16) {                                                                                                                         /* 16 * float32 */
            const uint32x4_t uints1 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src)), one), one), mulby127));      /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints2 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 4)), one), one), mulby127));  /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints3 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 8)), one), one), mulby127));  /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints4 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 12)), one), one), mulby127)); /* load 4 floats, clamp, convert to uint32 */
            const uint8x8_t ui8lo = vmovn_u16(vcombine_u16(vmovn_u32(uints1), vmovn_u32(uints2)));                                                /* narrow to uint16, combine, narrow to uint8 */
            const uint8x8_t ui8hi = vmovn_u16(vcombine_u16(vmovn_u32(uints3), vmovn_u32(uints4)));                                                /* narrow to uint16, combine, narrow to uint8 */
            vst1q_u8(mmdst, vcombine_u8(ui8lo, ui8hi));                                                                                           /* combine to uint8x16_t, store out */
            i -= 16;
            src += 16;
            mmdst += 16;
        }

        dst = (Uint8 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 255;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint8)((sample + 1.0f) * 127.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_Convert_F32_to_S16_NEON(Sint16 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S16 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 32767;
        } else if (sample <= -1.0f) {
            *dst = -32768;
        } else {
            *dst = (Sint16)(sample * 32767.0f);
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));

    /* Make sure src is aligned too. */
    if (!(((size_t)src) & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby32767 = vdupq_n_f32(32767.0f);
        int16_t *mmdst = (int16_t *)dst;
        while (i >= 8) {                                                                                                         /* 8 * float32 */
            const int32x4_t ints1 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src)), one), mulby32767));     /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints2 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src + 4)), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            vst1q_s16(mmdst, vcombine_s16(vmovn_s32(ints1), vmovn_s32(ints2)));                                                  /* narrow to sint16, combine, store out. */
            i -= 8;
            src += 8;
            mmdst += 8;
        }
        dst = (Sint16 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 32767;
        } else if (sample <= -1.0f) {
            *dst = -32768;
        } else {
            *dst = (Sint16)(sample * 32767.0f);
        }
        i--;
        src++;
        dst++;
    }
}

static void SDL_Convert_F32_to_S32_NEON(Sint32 *dst, const float *src, int num_samples)
{
    int i;

    LOG_DEBUG_AUDIO_CONVERT("F32", "S32 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for (i = num_samples; i && (((size_t)dst) & 15); --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 2147483647;
        } else if (sample <= -1.0f) {
            *dst = (-2147483647) - 1;
        } else {
            *dst = ((Sint32)(sample * 8388607.0f)) << 8;
        }
    }

    SDL_assert(!i || !(((size_t)dst) & 15));
    SDL_assert(!i || !(((size_t)src) & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby8388607 = vdupq_n_f32(8388607.0f);
        int32_t *mmdst = (int32_t *)dst;
        while (i >= 4) { /* 4 * float32 */
            vst1q_s32(mmdst, vshlq_n_s32(vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(src)), one), mulby8388607)), 8));
            i -= 4;
            src += 4;
            mmdst += 4;
        }
        dst = (Sint32 *)mmdst;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 2147483647;
        } else if (sample <= -1.0f) {
            *dst = (-2147483647) - 1;
        } else {
            *dst = ((Sint32)(sample * 8388607.0f)) << 8;
        }
        i--;
        src++;
        dst++;
    }
}
#endif

/* Function pointers set to a CPU-specific implementation. */
void (*SDL_Convert_S8_to_F32)(float *dst, const Sint8 *src, int num_samples) = NULL;
void (*SDL_Convert_U8_to_F32)(float *dst, const Uint8 *src, int num_samples) = NULL;
void (*SDL_Convert_S16_to_F32)(float *dst, const Sint16 *src, int num_samples) = NULL;
void (*SDL_Convert_S32_to_F32)(float *dst, const Sint32 *src, int num_samples) = NULL;
void (*SDL_Convert_F32_to_S8)(Sint8 *dst, const float *src, int num_samples) = NULL;
void (*SDL_Convert_F32_to_U8)(Uint8 *dst, const float *src, int num_samples) = NULL;
void (*SDL_Convert_F32_to_S16)(Sint16 *dst, const float *src, int num_samples) = NULL;
void (*SDL_Convert_F32_to_S32)(Sint32 *dst, const float *src, int num_samples) = NULL;

void SDL_ChooseAudioConverters(void)
{
    static SDL_bool converters_chosen = SDL_FALSE;
    if (converters_chosen) {
        return;
    }

#define SET_CONVERTER_FUNCS(fntype) \
    SDL_Convert_S8_to_F32 = SDL_Convert_S8_to_F32_##fntype; \
    SDL_Convert_U8_to_F32 = SDL_Convert_U8_to_F32_##fntype; \
    SDL_Convert_S16_to_F32 = SDL_Convert_S16_to_F32_##fntype; \
    SDL_Convert_S32_to_F32 = SDL_Convert_S32_to_F32_##fntype; \
    SDL_Convert_F32_to_S8 = SDL_Convert_F32_to_S8_##fntype; \
    SDL_Convert_F32_to_U8 = SDL_Convert_F32_to_U8_##fntype; \
    SDL_Convert_F32_to_S16 = SDL_Convert_F32_to_S16_##fntype; \
    SDL_Convert_F32_to_S32 = SDL_Convert_F32_to_S32_##fntype; \
    converters_chosen = SDL_TRUE

#ifdef SDL_SSE2_INTRINSICS
    if (SDL_HasSSE2()) {
        SET_CONVERTER_FUNCS(SSE2);
        return;
    }
#endif

#ifdef SDL_NEON_INTRINSICS
    if (SDL_HasNEON()) {
        SET_CONVERTER_FUNCS(NEON);
        return;
    }
#endif

#if NEED_SCALAR_CONVERTER_FALLBACKS
    SET_CONVERTER_FUNCS(Scalar);
#endif

#undef SET_CONVERTER_FUNCS

    SDL_assert(converters_chosen == SDL_TRUE);
}
