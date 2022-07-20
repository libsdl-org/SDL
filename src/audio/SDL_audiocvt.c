/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* Functions for audio drivers to perform runtime conversion of audio format */

/* FIXME: Channel weights when converting from more channels to fewer may need to be adjusted, see https://msdn.microsoft.com/en-us/library/windows/desktop/ff819070(v=vs.85).aspx
*/

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_audio_c.h"

#include "SDL_loadso.h"
#include "../SDL_dataqueue.h"
#include "SDL_cpuinfo.h"

#define DEBUG_AUDIOSTREAM 0

#ifdef __ARM_NEON
#define HAVE_NEON_INTRINSICS 1
#endif

#ifdef __SSE__
#define HAVE_SSE_INTRINSICS 1
#endif

#ifdef __SSE3__
#define HAVE_SSE3_INTRINSICS 1
#endif

#if defined(HAVE_IMMINTRIN_H) && !defined(SDL_DISABLE_IMMINTRIN_H)
#define HAVE_AVX_INTRINSICS 1
#endif
#if defined __clang__
# if (!__has_attribute(target))
#   undef HAVE_AVX_INTRINSICS
# endif
# if (defined(_MSC_VER) || defined(__SCE__)) && !defined(__AVX__)
#   undef HAVE_AVX_INTRINSICS
# endif
#elif defined __GNUC__
# if (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
#   undef HAVE_AVX_INTRINSICS
# endif
#endif

/*
 * CHANNEL LAYOUTS AS SDL EXPECTS THEM:
 *
 * (Even if the platform expects something else later, that
 * SDL will swizzle between the app and the platform).
 *
 * Abbreviations:
 * - FRONT=single mono speaker
 * - FL=front left speaker
 * - FR=front right speaker
 * - FC=front center speaker
 * - BL=back left speaker
 * - BR=back right speaker
 * - SR=side right speaker
 * - SL=side left speaker
 * - BC=back center speaker
 * - LFE=low-frequency speaker
 *
 * These are listed in the order they are laid out in
 * memory, so "FL+FR" means "the front left speaker is
 * layed out in memory first, then the front right, then
 * it repeats for the next audio frame".
 *
 * 1 channel (mono) layout: FRONT
 * 2 channels (stereo) layout: FL+FR
 * 3 channels (2.1) layout: FL+FR+LFE
 * 4 channels (quad) layout: FL+FR+BL+BR
 * 5 channels (4.1) layout: FL+FR+LFE+BL+BR
 * 6 channels (5.1) layout: FL+FR+FC+LFE+BL+BR
 * 7 channels (6.1) layout: FL+FR+FC+LFE+BC+SL+SR
 * 8 channels (7.1) layout: FL+FR+FC+LFE+BL+BR+SL+SR
 */


#if 0  /* !!! FIXME: these need to be updated to match the new scalar code. */
#if HAVE_AVX_INTRINSICS
/* MSVC will always accept AVX intrinsics when compiling for x64 */
#if defined(__clang__) || defined(__GNUC__)
__attribute__((target("avx")))
#endif
/* Convert from 5.1 to stereo. Average left and right, distribute center, discard LFE. */
static void SDLCALL
SDL_Convert51ToStereo_AVX(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i = cvt->len_cvt / (sizeof (float) * 6);
    const float two_fifths_f = 1.0f / 2.5f;
    const __m256 two_fifths_v = _mm256_set1_ps(two_fifths_f);
    const __m256 half = _mm256_set1_ps(0.5f);

    LOG_DEBUG_CONVERT("5.1", "stereo (using AVX)");
    SDL_assert(format == AUDIO_F32SYS);

    /* SDL's 5.1 layout: FL+FR+FC+LFE+BL+BR */
    while (i >= 4) {
        __m256 in0 = _mm256_loadu_ps(src + 0);  /* 0FL 0FR 0FC 0LF 0BL 0BR 1FL 1FR */
        __m256 in1 = _mm256_loadu_ps(src + 8);  /* 1FC 1LF 1BL 1BR 2FL 2FR 2FC 2LF */
        __m256 in2 = _mm256_loadu_ps(src + 16); /* 2BL 2BR 3FL 3FR 3FC 3LF 3BL 3BR */

        /* 0FL 0FR 0FC 0LF 2FL 2FR 2FC 2LF */
        __m256 temp0 = _mm256_blend_ps(in0, in1, 0xF0);
        /* 1FC 1LF 1BL 1BR 3FC 3LF 3BL 3BR */
        __m256 temp1 = _mm256_blend_ps(in1, in2, 0xF0);

        /* 0FC 0FC 1FC 1FC 2FC 2FC 3FC 3FC */
        __m256 fc_distributed = _mm256_mul_ps(half, _mm256_shuffle_ps(temp0, temp1, _MM_SHUFFLE(0, 0, 2, 2)));

        /* 0FL 0FR 1BL 1BR 2FL 2FR 3BL 3BR */
        __m256 permuted0 = _mm256_blend_ps(temp0, temp1, 0xCC);
        /* 0BL 0BR 1FL 1FR 2BL 2BR 3FL 3FR */
        __m256 permuted1 = _mm256_permute2f128_ps(in0, in2, 0x21);

        /*   0FL 0FR 1BL 1BR 2FL 2FR 3BL 3BR */
        /* + 0BL 0BR 1FL 1FR 2BL 2BR 3FL 3FR */
        /* =  0L  0R  1L  1R  2L  2R  3L  3R */
        __m256 out = _mm256_add_ps(permuted0, permuted1);
        out = _mm256_add_ps(out, fc_distributed);
        out = _mm256_mul_ps(out, two_fifths_v);

        _mm256_storeu_ps(dst, out);

        i -= 4; src += 24; dst += 8;
    }


    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float front_center_distributed = src[2] * 0.5f;
        dst[0] = (src[0] + front_center_distributed + src[4]) * two_fifths_f;  /* left */
        dst[1] = (src[1] + front_center_distributed + src[5]) * two_fifths_f;  /* right */
        i--; src += 6; dst+=2;
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}
#endif

#if HAVE_SSE_INTRINSICS
/* Convert from 5.1 to stereo. Average left and right, distribute center, discard LFE. */
static void SDLCALL
SDL_Convert51ToStereo_SSE(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i = cvt->len_cvt / (sizeof (float) * 6);
    const float two_fifths_f = 1.0f / 2.5f;
    const __m128 two_fifths_v = _mm_set1_ps(two_fifths_f);
    const __m128 half = _mm_set1_ps(0.5f);

    LOG_DEBUG_CONVERT("5.1", "stereo (using SSE)");
    SDL_assert(format == AUDIO_F32SYS);

    /* SDL's 5.1 layout: FL+FR+FC+LFE+BL+BR */
    /* Just use unaligned load/stores, if the memory at runtime is */
    /* aligned it'll be just as fast on modern processors */
    while (i >= 2) {
        /* Two 5.1 samples (12 floats) fit nicely in three 128bit */
        /* registers. Using shuffles they can be rearranged so that */
        /* the conversion math can be vectorized. */
        __m128 in0 = _mm_loadu_ps(src);     /* 0FL 0FR 0FC 0LF */
        __m128 in1 = _mm_loadu_ps(src + 4); /* 0BL 0BR 1FL 1FR */
        __m128 in2 = _mm_loadu_ps(src + 8); /* 1FC 1LF 1BL 1BR */

        /* 0FC 0FC 1FC 1FC */
        __m128 fc_distributed = _mm_mul_ps(half, _mm_shuffle_ps(in0, in2, _MM_SHUFFLE(0, 0, 2, 2)));

        /* 0FL 0FR 1BL 1BR */
        __m128 blended = _mm_shuffle_ps(in0, in2, _MM_SHUFFLE(3, 2, 1, 0));

        /*   0FL 0FR 1BL 1BR */
        /* + 0BL 0BR 1FL 1FR */
        /* =  0L  0R  1L  1R */
        __m128 out = _mm_add_ps(blended, in1);
        out = _mm_add_ps(out, fc_distributed);
        out = _mm_mul_ps(out, two_fifths_v);

        _mm_storeu_ps(dst, out);

        i -= 2; src += 12; dst += 4;
    }


    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float front_center_distributed = src[2] * 0.5f;
        dst[0] = (src[0] + front_center_distributed + src[4]) * two_fifths_f;  /* left */
        dst[1] = (src[1] + front_center_distributed + src[5]) * two_fifths_f;  /* right */
        i--; src += 6; dst+=2;
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}
#endif

#if HAVE_NEON_INTRINSICS
/* Convert from 5.1 to stereo. Average left and right, distribute center, discard LFE. */
static void SDLCALL
SDL_Convert51ToStereo_NEON(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i = cvt->len_cvt / (sizeof (float) * 6);
    const float two_fifths_f = 1.0f / 2.5f;
    const float32x4_t two_fifths_v = vdupq_n_f32(two_fifths_f);
    const float32x4_t half = vdupq_n_f32(0.5f);

    LOG_DEBUG_CONVERT("5.1", "stereo (using NEON)");
    SDL_assert(format == AUDIO_F32SYS);

    /* SDL's 5.1 layout: FL+FR+FC+LFE+BL+BR */

    /* Just use unaligned load/stores, it's the same NEON instructions and
       hopefully even unaligned NEON is faster than the scalar fallback. */
    while (i >= 2) {
        /* Two 5.1 samples (12 floats) fit nicely in three 128bit */
        /* registers. Using shuffles they can be rearranged so that */
        /* the conversion math can be vectorized. */
        const float32x4_t in0 = vld1q_f32(src);     /* 0FL 0FR 0FC 0LF */
        const float32x4_t in1 = vld1q_f32(src + 4); /* 0BL 0BR 1FL 1FR */
        const float32x4_t in2 = vld1q_f32(src + 8); /* 1FC 1LF 1BL 1BR */

        /* 0FC 0FC 1FC 1FC */
        const float32x4_t fc_distributed = vmulq_f32(half, vcombine_f32(vdup_lane_f32(vget_high_f32(in0), 0), vdup_lane_f32(vget_low_f32(in2), 0)));

        /* 0FL 0FR 1BL 1BR */
        const float32x4_t blended = vcombine_f32(vget_low_f32(in0), vget_high_f32(in2));

        /*   0FL 0FR 1BL 1BR */
        /* + 0BL 0BR 1FL 1FR */
        /* =  0L  0R  1L  1R */
        float32x4_t out = vaddq_f32(blended, in1);
        out = vaddq_f32(out, fc_distributed);
        out = vmulq_f32(out, two_fifths_v);

        vst1q_f32(dst, out);

        i -= 2; src += 12; dst += 4;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        const float front_center_distributed = src[2] * 0.5f;
        dst[0] = (src[0] + front_center_distributed + src[4]) * two_fifths_f;  /* left */
        dst[1] = (src[1] + front_center_distributed + src[5]) * two_fifths_f;  /* right */
        i--; src += 6; dst+=2;
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}
#endif
#endif


/* Channel conversion is now mostly following this scheme, borrowed from FNA
   (which is following the scheme of XNA)...
   https://github.com/FNA-XNA/FAudio/blob/master/src/matrix_defaults.inl */

/* CONVERT FROM MONO... */
/* Mono duplicates to stereo and all other channels are silenced. */

#define CVT_MONO_TO(toname, tonamestr, num_channels, zeroingcode) \
    static void SDLCALL SDL_ConvertMonoTo##toname(SDL_AudioCVT * cvt, SDL_AudioFormat format) { \
        const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 1; \
        float *dst = ((float *) (cvt->buf + cvt->len_cvt * num_channels)) - num_channels; \
        int i; \
        LOG_DEBUG_CONVERT("mono", tonamestr); \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_assert(num_channels >= 2); \
        for (i = cvt->len_cvt / sizeof (float); i; i--, src--, dst -= num_channels) { \
            dst[0] = dst[1] = *src; \
            zeroingcode; \
        } \
        cvt->len_cvt *= num_channels; \
        if (cvt->filters[++cvt->filter_index]) { \
            cvt->filters[cvt->filter_index] (cvt, format); \
        } \
    }
CVT_MONO_TO(Stereo, "stereo", 2, {});
CVT_MONO_TO(21, "2.1", 3, { dst[2] = 0.0f; });
CVT_MONO_TO(Quad, "quad", 4, { dst[2] = dst[3] = 0.0f; });
CVT_MONO_TO(41, "4.1", 5, { dst[2] = dst[3] = dst[4] = 0.0f; });
CVT_MONO_TO(51, "5.1", 6, { dst[2] = dst[3] = dst[4] = dst[5] = 0.0f; });
CVT_MONO_TO(61, "6.1", 7, { dst[2] = dst[3] = dst[4] = dst[5] = dst[6] = 0.0f; });
CVT_MONO_TO(71, "7.1", 8, { dst[2] = dst[3] = dst[4] = dst[5] = dst[6] = dst[7] = 0.0f; });
#undef CVT_MONO_TO



/* CONVERT FROM STEREO... */
/* Stereo duplicates to two front speakers and all other channels are silenced. */

#if HAVE_SSE3_INTRINSICS
/* Convert from stereo to mono. Average left and right. */
static void SDLCALL
SDL_ConvertStereoToMono_SSE3(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const __m128 divby2 = _mm_set1_ps(0.5f);
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i = cvt->len_cvt / 8;

    LOG_DEBUG_CONVERT("stereo", "mono (using SSE3)");
    SDL_assert(format == AUDIO_F32SYS);

    /* Do SSE blocks as long as we have 16 bytes available.
       Just use unaligned load/stores, if the memory at runtime is
       aligned it'll be just as fast on modern processors */
    while (i >= 4) {   /* 4 * float32 */
        _mm_storeu_ps(dst, _mm_mul_ps(_mm_hadd_ps(_mm_loadu_ps(src), _mm_loadu_ps(src+4)), divby2));
        i -= 4; src += 8; dst += 4;
    }

    /* Finish off any leftovers with scalar operations. */
    while (i) {
        *dst = (src[0] + src[1]) * 0.5f;
        dst++; i--; src += 2;
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}
#endif

/* Convert from stereo to mono. Average left and right. */
static void SDLCALL
SDL_ConvertStereoToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("stereo", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 2); i; --i, src += 2) {
        *(dst++) = (src[0] + src[1]) * 0.5f;
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

#define CVT_STEREO_TO(toname, tonamestr, num_channels, zeroingcode) \
    static void SDLCALL SDL_ConvertStereoTo##toname(SDL_AudioCVT * cvt, SDL_AudioFormat format) { \
        int i; \
        const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 2; \
        float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 2) * num_channels))) - num_channels; \
        LOG_DEBUG_CONVERT("stereo", tonamestr); \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_assert(num_channels >= 3); \
        for (i = cvt->len_cvt / (sizeof (float) * 2); i; --i, dst -= num_channels, src -= 2) { \
            dst[0] = src[0]; \
            dst[1] = src[1]; \
            zeroingcode; \
        } \
        cvt->len_cvt = (cvt->len_cvt / 2) * num_channels; \
        if (cvt->filters[++cvt->filter_index]) { \
            cvt->filters[cvt->filter_index] (cvt, format); \
        } \
    }

CVT_STEREO_TO(21, "2.1", 3, { dst[2] = 0.0f; });
CVT_STEREO_TO(Quad, "quad", 4, { dst[2] = dst[3] = 0.0f; });
CVT_STEREO_TO(41, "4.1", 5, { dst[2] = dst[3] = dst[4] = 0.0f; });
CVT_STEREO_TO(51, "5.1", 6, { dst[2] = dst[3] = dst[4] = dst[5] = 0.0f; });
CVT_STEREO_TO(61, "6.1", 7, { dst[2] = dst[3] = dst[4] = dst[5] = dst[6] = 0.0f; });
CVT_STEREO_TO(71, "7.1", 8, { dst[2] = dst[3] = dst[4] = dst[5] = dst[6] = dst[7] = 0.0f; });
#undef CVT_STEREO_TO



/* CONVERT FROM 2.1... */
/* 2.1 duplicates to two front speakers (and LFE when available) and all other channels are silenced. */

/* Convert from 2.1 to mono. Average left and right, drop LFE. */
static void SDLCALL
SDL_Convert21ToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("2.1", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 2); i; --i, src += 3) {
        *(dst++) = (src[0] + src[1]) * 0.5f;
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

#define CVT_21_TO(toname, tonamestr, num_channels, customcode) \
    static void SDLCALL SDL_Convert21To##toname(SDL_AudioCVT * cvt, SDL_AudioFormat format) { \
        int i; \
        float lf, rf, lfe; \
        const float *src = (const float *) (cvt->buf + cvt->len_cvt); \
        float *dst = (float *) (cvt->buf + ((cvt->len_cvt / 3) * num_channels)); \
        LOG_DEBUG_CONVERT("2.1", tonamestr); \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_assert(num_channels >= 2); \
        for (i = cvt->len_cvt / (sizeof (float) * 3); i; --i) { \
            dst -= num_channels; \
            src -= 2; \
            lf = src[0]; \
            rf = src[1]; \
            lfe = src[2]; \
            dst[0] = lf; \
            dst[1] = rf; \
            customcode; \
        } \
        cvt->len_cvt = (cvt->len_cvt / 3) * num_channels; \
        if (cvt->filters[++cvt->filter_index]) { \
            cvt->filters[cvt->filter_index] (cvt, format); \
        } \
    }

CVT_21_TO(Stereo, "stereo", 2, { (void) lfe; });
CVT_21_TO(Quad, "quad", 4, { (void) lfe; dst[2] = dst[3] = 0.0f; });
CVT_21_TO(41, "4.1", 5, { dst[2] = lfe; dst[3] = dst[4] = 0.0f; });
CVT_21_TO(51, "5.1", 6, { dst[2] = 0.0f; dst[3] = lfe; dst[4] = dst[5] = 0.0f; });
CVT_21_TO(61, "6.1", 7, { dst[2] = 0.0f; dst[3] = lfe; dst[4] = dst[5] = dst[6] = 0.0f; });
CVT_21_TO(71, "7.1", 8, { dst[2] = 0.0f; dst[3] = lfe; dst[4] = dst[5] = dst[6] = dst[7] = 0.0f; });
#undef CVT_21_TO


/* CONVERT FROM QUAD... */

static void SDLCALL
SDL_ConvertQuadToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("quad", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: could benefit from SIMD */
    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src += 4) {
        *(dst++) = (src[0] + src[1] + src[3] + src[4]) * 0.25f;
    }

    cvt->len_cvt /= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("quad", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: could benefit from SIMD */
    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src += 4) {
        const float fl = src[0];
        const float fr = src[1];
        const float bl = src[2];
        const float br = src[3];
        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right)...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.421000004f) + (bl * 0.358999997f) + (br * 0.219999999f);
        *(dst++) = (fr * 0.421000004f) + (br * 0.358999997f) + (bl * 0.219999999f);
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadTo21(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("quad", "2.1");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: could benefit from SIMD */
    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src += 4) {
        const float fl = src[0];
        const float fr = src[1];
        const float bl = src[2];
        const float br = src[3];
        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right)...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.421000004f) + (bl * 0.358999997f) + (br * 0.219999999f);
        *(dst++) = (fr * 0.421000004f) + (br * 0.358999997f) + (bl * 0.219999999f);
        *(dst++) = 0.0f;  /* lfe */
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadTo41(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 4;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 4) * 5))) - 5;
    int i;

    LOG_DEBUG_CONVERT("quad", "4.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src -= 4, dst -= 5) {
        dst[4] = src[3];
        dst[3] = src[2];
        dst[2] = 0.0f;  /* LFE */
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 5;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadTo51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 4;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 4) * 6))) - 6;
    int i;

    LOG_DEBUG_CONVERT("quad", "5.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src -= 4, dst -= 6) {
        dst[5] = src[3];
        dst[4] = src[2];
        dst[3] = 0.0f;  /* LFE */
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadTo61(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 4;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 4) * 7))) - 7;
    int i;

    LOG_DEBUG_CONVERT("quad", "6.1");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: I'm skeptical XNA/FNA's conversion is right, here. */

    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src -= 4, dst -= 7) {
        const float bl = src[2];
        const float br = src[3];
        dst[6] = br * 0.796000004f;
        dst[5] = bl * 0.796000004f;
        dst[4] = (bl + br) * 0.5f;  /* average BL+BR to BC */
        dst[3] = 0.0f;  /* LFE */
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1] * 0.939999998f;
        dst[0] = src[0] * 0.939999998f;
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 7;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_ConvertQuadTo71(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 4;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 4) * 8))) - 8;
    int i;

    LOG_DEBUG_CONVERT("quad", "7.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src -= 4, dst -= 8) {
        dst[7] = 0.0f;  /* SR */
        dst[6] = 0.0f;  /* SL */
        dst[5] = src[3];
        dst[4] = src[2];
        dst[3] = 0.0f;  /* LFE */
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 4) * 8;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* CONVERT FROM 4.1... */

static void SDLCALL
SDL_Convert41ToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("4.1", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src += 5) {
        *(dst++) = (src[0] + src[1] + src[3] + src[4]) * 0.25f;
    }

    cvt->len_cvt /= 5;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("4.1", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src += 5) {
        const float fl = src[0];
        const float fr = src[1];
        const float bl = src[3];
        const float br = src[4];
        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.374222219f) + (bl * 0.319111109f) + (br * 0.195555553f);
        *(dst++) = (fr * 0.374222219f) + (br * 0.319111109f) + (bl * 0.195555553f);
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41To21(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("4.1", "2.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src += 5) {
        const float fl = src[0];
        const float fr = src[1];
        const float lfe = src[2];
        const float bl = src[3];
        const float br = src[4];
        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.374222219f) + (bl * 0.319111109f) + (br * 0.195555553f);
        *(dst++) = (fr * 0.374222219f) + (br * 0.319111109f) + (bl * 0.195555553f);
        *(dst++) = lfe;
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("4.1", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src += 5) {
        /* !!! FIXME: FNA/XNA mixes a little of the LFE into every channel...but this can't possibly be right, right...? I just drop the LFE and copy the channels. */
        *(dst++) = src[0];
        *(dst++) = src[1];
        *(dst++) = src[3];
        *(dst++) = src[4];
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41To51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 5;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 5) * 6))) - 6;
    int i;

    LOG_DEBUG_CONVERT("4.1", "5.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src -= 5, dst -= 6) {
        dst[5] = src[4];
        dst[4] = src[3];
        dst[3] = src[2];
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41To61(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 5;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 4) * 7))) - 7;
    int i;

    LOG_DEBUG_CONVERT("4.1", "6.1");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: I'm skeptical XNA/FNA's conversion is right, here. */

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src -= 5, dst -= 7) {
        const float bl = src[3];
        const float br = src[4];
        dst[6] = br * 0.796000004f;
        dst[5] = bl * 0.796000004f;
        dst[4] = (bl + br) * 0.5f;  /* average BL+BR to BC */
        dst[3] = src[2];
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1] * 0.939999998f;
        dst[0] = src[0] * 0.939999998f;
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 7;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert41To71(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 5;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 5) * 8))) - 8;
    int i;

    LOG_DEBUG_CONVERT("4.1", "7.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 5); i; --i, src -= 5, dst -= 8) {
        dst[7] = 0.0f;  /* SR */
        dst[6] = 0.0f;  /* SL */
        dst[5] = src[4];
        dst[4] = src[3];
        dst[3] = src[2];
        dst[2] = 0.0f;  /* FC */
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 5) * 8;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}



/* CONVERT FROM 5.1... */

static void SDLCALL
SDL_Convert51ToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6) {
        *(dst++) = (src[0] + src[1] + src[2] + src[4] + src[5]) * 0.200000003f;
    }

    cvt->len_cvt /= 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bl = src[4];
        const float br = src[5];
        const float extra = 0.090909094f / 4.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * (0.294545442f+extra)) + (fc * (0.208181813f+extra)) + (bl * (0.251818180f+extra)) + (br * (0.154545456f+extra));
        *(dst++) = (fr * (0.294545442f+extra)) + (fc * (0.208181813f+extra)) + (br * (0.251818180f+extra)) + (bl * (0.154545456f+extra));
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51To21(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "2.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.324000001f) + (fc * 0.229000002f) + (bl * 0.277000010f) + (br * 0.170000002f);
        *(dst++) = (fr * 0.324000001f) + (fc * 0.229000002f) + (br * 0.277000010f) + (bl * 0.170000002f);
        *(dst++) = lfe;
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bl = src[4];
        const float br = src[5];
        const float extra = 0.047619049f / 2.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */
        *(dst++) = (fl * (0.558095276f+extra)) + (fc * (0.394285709f+extra));
        *(dst++) = (fr * (0.558095276f+extra)) + (fc * (0.394285709f+extra));
        *(dst++) = bl;  /* !!! FIXME: XNA/FNA quiets the back speakers here to ~58%, but I'm copying them through. Not sure why they do it like that. */
        *(dst++) = br;  /* !!! FIXME: XNA/FNA quiets the back speakers here to ~58%, but I'm copying them through. Not sure why they do it like that. */
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51To41(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "4.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];
        *(dst++) = (fl * 0.586000025f) + (fc * 0.414000005f);
        *(dst++) = (fr * 0.586000025f) + (fc * 0.414000005f);
        *(dst++) = lfe;
        *(dst++) = bl;  /* !!! FIXME: XNA/FNA quiets the back speakers here to ~58%, but I'm copying them through. Not sure why they do it like that. */
        *(dst++) = br;  /* !!! FIXME: XNA/FNA quiets the back speakers here to ~58%, but I'm copying them through. Not sure why they do it like that. */
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 5;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51To61(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 6;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 6) * 7))) - 7;
    int i;

    LOG_DEBUG_CONVERT("5.1", "6.1");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: I'm skeptical XNA/FNA's conversion is right, here. Why is it quieting the back speakers instead of copying them through as-is? */
    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src -= 6, dst -= 7) {
        const float bl = src[4];
        const float br = src[5];
        dst[6] = br * 0.796000004f;
        dst[5] = bl * 0.796000004f;
        dst[4] = (bl + br) * 0.5f;  /* average BL+BR to BC */
        dst[3] = src[3];
        dst[2] = src[2] * 0.939999998f;
        dst[1] = src[1] * 0.939999998f;
        dst[0] = src[0] * 0.939999998f;
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 7;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert51To71(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 6;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 6) * 8))) - 8;
    int i;

    LOG_DEBUG_CONVERT("5.1", "7.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src -= 6, dst -= 8) {
        dst[7] = 0.0f;  /* SR */
        dst[6] = 0.0f;  /* SL */
        dst[5] = src[5];
        dst[4] = src[4];
        dst[3] = src[3];
        dst[2] = src[2];
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 6) * 8;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}



/* CONVERT FROM 6.1... */

static void SDLCALL
SDL_Convert61ToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        *(dst++) = (src[0] + src[1] + src[2] + src[4] + src[5] + src[6]) * 0.166666672f;
    }

    cvt->len_cvt /= 7;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bc = src[4];
        const float sl = src[5];
        const float sr = src[6];
        const float extra = 0.076923080f / 5.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * (0.247384623f+extra)) + (fc * (0.174461529f+extra)) + (bc * (0.174461529f+extra)) + (sl * (0.226153851f+extra)) + (sr * (0.100615382f+extra));
        *(dst++) = (fr * (0.247384623f+extra)) + (fc * (0.174461529f+extra)) + (bc * (0.174461529f+extra)) + (sr * (0.226153851f+extra)) + (sl * (0.100615382f+extra));
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61To21(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "2.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bc = src[4];
        const float sl = src[5];
        const float sr = src[6];

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.247384623f) + (fc * 0.174461529f) + (bc * 0.174461529f) + (sl * 0.226153851f) + (sr * 0.100615382f);
        *(dst++) = (fr * 0.247384623f) + (fc * 0.174461529f) + (bc * 0.174461529f) + (sr * 0.226153851f) + (sl * 0.100615382f);
        *(dst++) = lfe;
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bc = src[4];
        const float sl = src[5];
        const float sr = src[6];
        const float extra = 0.040000003f / 3.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */
        const float extra2 = 0.040000003f / 2.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */
        *(dst++) = (fl * (0.463679999f+extra)) + (fc * (0.327360004f+extra)) + (sl * (0.168960005f+extra));
        *(dst++) = (fr * (0.463679999f+extra)) + (fc * (0.327360004f+extra)) + (sr * (0.168960005f+extra));
        *(dst++) = (bc * (0.327360004f+extra2)) + (sl * (0.431039989f+extra2));
        *(dst++) = (bc * (0.327360004f+extra2)) + (sr * (0.431039989f+extra2));
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61To41(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "4.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bc = src[4];
        const float sl = src[5];
        const float sr = src[6];
        *(dst++) = (fl * 0.483000010f) + (fc * 0.340999991f) + (sl * 0.175999999f);
        *(dst++) = (fr * 0.483000010f) + (fc * 0.340999991f) + (sr * 0.175999999f);
        *(dst++) = lfe;
        *(dst++) = (bc * 0.340999991f) + (sl * 0.449000001f);
        *(dst++) = (bc * 0.340999991f) + (sr * 0.449000001f);
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 5;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61To51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6.1", "5.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src += 7) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bc = src[4];
        const float sl = src[5];
        const float sr = src[6];
        /* !!! FIXME: XNA/FNA quiets the front speakers a bunch, but leaves the back speakers at about the same volume. I'm not sure that's right. */
        *(dst++) = (fl * 0.611000001f) + (sl * 0.223000005f);
        *(dst++) = (fr * 0.611000001f) + (sr * 0.223000005f);
        *(dst++) = (fc * 0.611000001f);  /* !!! FIXME: XNA/FNA silence the FC speaker to ~61%, but I'm not sure this is right. */
        *(dst++) = lfe;
        *(dst++) = (bc * 0.432000011f) + (sl * 0.568000019f);
        *(dst++) = (bc * 0.432000011f) + (sr * 0.568000019f);
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert61To71(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = ((const float *) (cvt->buf + cvt->len_cvt)) - 7;
    float *dst = ((float *) (cvt->buf + ((cvt->len_cvt / 7) * 8))) - 8;
    int i;

    LOG_DEBUG_CONVERT("6.1", "7.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 7); i; --i, src -= 7, dst -= 8) {
        const float bc = src[4];
        dst[7] = src[6];
        dst[6] = src[5];
        dst[5] = (bc * 0.707000017f);
        dst[4] = (bc * 0.707000017f);
        dst[3] = src[3];
        dst[2] = src[2];
        dst[1] = src[1];
        dst[0] = src[0];
    }

    cvt->len_cvt = (cvt->len_cvt / 7) * 8;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* CONVERT FROM 7.1... */

static void SDLCALL
SDL_Convert71ToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: we can probably SIMD this. */
    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        *(dst++) = (src[0] + src[1] + src[2] + src[4] + src[5] + src[6] + src[7]) * 0.143142849f;
    }

    cvt->len_cvt /= 8;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: we can probably SIMD this. */
    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];
        const float extra = 0.066666670f / 6.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * (0.211866662f+extra)) + (fc * (0.150266662f+extra)) + (bl * (0.181066677f+extra)) + (br * (0.111066669f+extra)) + (sl * (0.194133341f+extra)) + (sr * (0.085866667f+extra));
        *(dst++) = (fr * (0.211866662f+extra)) + (fc * (0.150266662f+extra)) + (br * (0.181066677f+extra)) + (bl * (0.111066669f+extra)) + (sr * (0.194133341f+extra)) + (sl * (0.085866667f+extra));
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71To21(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "2.1");
    SDL_assert(format == AUDIO_F32SYS);

    /* !!! FIXME: we can probably SIMD this. */
    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];

        /* !!! FIXME: FNA/XNA mixes a little of the back right into the left (and back left into the right) and a little of the LFE...but this can't possibly be right, right...? */
        *(dst++) = (fl * 0.211866662f) + (fc * 0.150266662f) + (bl * 0.181066677f) + (br * 0.111066669f) + (sl * 0.194133341f) + (sr * 0.085866667f);
        *(dst++) = (fr * 0.211866662f) + (fc * 0.150266662f) + (br * 0.181066677f) + (bl * 0.111066669f) + (sr * 0.194133341f) + (sl * 0.085866667f);
        *(dst++) = lfe;
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];
        const float extra = 0.034482758f / 3.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */
        const float extra2 = 0.034482758f / 2.0f;  /* this was the LFE distribution, we'll just split it between the other channels for now. */
        *(dst++) = (fl * (0.466344833f+extra)) + (fc * (0.329241365f+extra)) + (sl * (0.169931039f+extra));
        *(dst++) = (fr * (0.466344833f+extra)) + (fc * (0.329241365f+extra)) + (sr * (0.169931039f+extra));
        *(dst++) = (bl * (0.466344833f+extra2)) + (sl * (0.433517247f+extra2));
        *(dst++) = (br * (0.466344833f+extra2)) + (sr * (0.433517247f+extra2));
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71To41(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "4.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];
        *(dst++) = (fl * 0.483000010f) + (fc * 0.340999991f) + (sl * 0.175999999f);
        *(dst++) = (fr * 0.483000010f) + (fc * 0.340999991f) + (sr * 0.175999999f);
        *(dst++) = lfe;
        *(dst++) = (bl * 0.483000010f) + (sl * 0.449000001f);
        *(dst++) = (br * 0.483000010f) + (sr * 0.449000001f);
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 5;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71To51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "5.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];
        /* !!! FIXME: XNA/FNA quiets the front speakers a bunch, but leaves the back speakers at about the same volume. I'm not sure that's right. */
        *(dst++) = (fl * 0.518000007f) + (sl * 0.188999996f);
        *(dst++) = (fr * 0.518000007f) + (sr * 0.188999996f);
        *(dst++) = (fc * 0.518000007f);  /* !!! FIXME: XNA/FNA silence the FC speaker to ~51%, but I'm not sure this is right. */
        *(dst++) = lfe;
        *(dst++) = (bl * 0.518000007f) + (sl * 0.188999996f);
        *(dst++) = (br * 0.518000007f) + (sr * 0.188999996f);
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static void SDLCALL
SDL_Convert71To61(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("7.1", "6.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8) {
        const float fl = src[0];
        const float fr = src[1];
        const float fc = src[2];
        const float lfe = src[3];
        const float bl = src[4];
        const float br = src[5];
        const float sl = src[6];
        const float sr = src[7];
        /* !!! FIXME: XNA/FNA quiets the front speakers a bunch, but leaves the back speakers at about the same volume. I'm not sure that's right. */
        *(dst++) = (fl * 0.541000009f);
        *(dst++) = (fr * 0.541000009f);
        *(dst++) = (fc * 0.541000009f);  /* !!! FIXME: XNA/FNA silence the FC speaker to ~61%, but I'm not sure this is right. */
        *(dst++) = lfe;
        *(dst++) = (bl * 0.287999988f) + (br * 0.287999988f);
        *(dst++) = (bl * 0.458999991f) + (sl * 0.541000009f);
        *(dst++) = (br * 0.458999991f) + (sr * 0.541000009f);
    }

    cvt->len_cvt = (cvt->len_cvt / 8) * 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static const SDL_AudioFilter channel_converters[8][8] = {   /* from][to] */
    { NULL, SDL_ConvertMonoToStereo, SDL_ConvertMonoTo21, SDL_ConvertMonoToQuad, SDL_ConvertMonoTo41, SDL_ConvertMonoTo51, SDL_ConvertMonoTo61, SDL_ConvertMonoTo71 },
    { SDL_ConvertStereoToMono, NULL, SDL_ConvertStereoTo21, SDL_ConvertStereoToQuad, SDL_ConvertStereoTo41, SDL_ConvertStereoTo51, SDL_ConvertStereoTo61, SDL_ConvertStereoTo71 },
    { SDL_Convert21ToMono, SDL_Convert21ToStereo, NULL, SDL_Convert21ToQuad, SDL_Convert21To41, SDL_Convert21To51, SDL_Convert21To61, SDL_Convert21To71 },
    { SDL_ConvertQuadToMono, SDL_ConvertQuadToStereo, SDL_ConvertQuadTo21, NULL, SDL_ConvertQuadTo41, SDL_ConvertQuadTo51, SDL_ConvertQuadTo61, SDL_ConvertQuadTo71 },
    { SDL_Convert41ToMono, SDL_Convert41ToStereo, SDL_Convert41To21, SDL_Convert41ToQuad, NULL, SDL_Convert41To51, SDL_Convert41To61, SDL_Convert41To71 },
    { SDL_Convert51ToMono, SDL_Convert51ToStereo, SDL_Convert51To21, SDL_Convert51ToQuad, SDL_Convert51To41, NULL, SDL_Convert51To61, SDL_Convert51To71 },
    { SDL_Convert61ToMono, SDL_Convert61ToStereo, SDL_Convert61To21, SDL_Convert61ToQuad, SDL_Convert61To41, SDL_Convert61To51, NULL, SDL_Convert61To71 },
    { SDL_Convert71ToMono, SDL_Convert71ToStereo, SDL_Convert71To21, SDL_Convert71ToQuad, SDL_Convert71To41, SDL_Convert71To51, SDL_Convert71To61, NULL }
};



/* SDL's resampler uses a "bandlimited interpolation" algorithm:
     https://ccrma.stanford.edu/~jos/resample/ */

#include "SDL_audio_resampler_filter.h"

static int
ResamplerPadding(const int inrate, const int outrate)
{
    if (inrate == outrate) {
        return 0;
    }
    if (inrate > outrate) {
        return (int) SDL_ceilf(((float) (RESAMPLER_SAMPLES_PER_ZERO_CROSSING * inrate) / ((float) outrate)));
    }
    return RESAMPLER_SAMPLES_PER_ZERO_CROSSING;
}

/* lpadding and rpadding are expected to be buffers of (ResamplePadding(inrate, outrate) * chans * sizeof (float)) bytes. */
static int
SDL_ResampleAudio(const int chans, const int inrate, const int outrate,
                        const float *lpadding, const float *rpadding,
                        const float *inbuf, const int inbuflen,
                        float *outbuf, const int outbuflen)
{
    /* Note that this used to be double, but it looks like we can get by with float in most cases at
       almost twice the speed on Intel processors, and orders of magnitude more
       on CPUs that need a software fallback for double calculations. */
    typedef float ResampleFloatType;

    const ResampleFloatType finrate = (ResampleFloatType) inrate;
    const ResampleFloatType outtimeincr = ((ResampleFloatType) 1.0f) / ((ResampleFloatType) outrate);
    const ResampleFloatType ratio = ((float) outrate) / ((float) inrate);
    const int paddinglen = ResamplerPadding(inrate, outrate);
    const int framelen = chans * (int)sizeof (float);
    const int inframes = inbuflen / framelen;
    const int wantedoutframes = (int) ((inbuflen / framelen) * ratio);  /* outbuflen isn't total to write, it's total available. */
    const int maxoutframes = outbuflen / framelen;
    const int outframes = SDL_min(wantedoutframes, maxoutframes);
    ResampleFloatType outtime = 0.0f;
    float *dst = outbuf;
    int i, j, chan;

    for (i = 0; i < outframes; i++) {
        const int srcindex = (int) (outtime * inrate);
        const ResampleFloatType intime = ((ResampleFloatType) srcindex) / finrate;
        const ResampleFloatType innexttime = ((ResampleFloatType) (srcindex + 1)) / finrate;
        const ResampleFloatType indeltatime = innexttime - intime;
        const ResampleFloatType interpolation1 = (indeltatime == 0.0f) ? 1.0f : (1.0f - ((innexttime - outtime) / indeltatime));
        const int filterindex1 = (int) (interpolation1 * RESAMPLER_SAMPLES_PER_ZERO_CROSSING);
        const ResampleFloatType interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (int) (interpolation2 * RESAMPLER_SAMPLES_PER_ZERO_CROSSING);

        for (chan = 0; chan < chans; chan++) {
            float outsample = 0.0f;

            /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
            for (j = 0; (filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)) < RESAMPLER_FILTER_SIZE; j++) {
                const int srcframe = srcindex - j;
                /* !!! FIXME: we can bubble this conditional out of here by doing a pre loop. */
                const float insample = (srcframe < 0) ? lpadding[((paddinglen + srcframe) * chans) + chan] : inbuf[(srcframe * chans) + chan];
                outsample += (float)(insample * (ResamplerFilter[filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)] + (interpolation1 * ResamplerFilterDifference[filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)])));
            }

            /* Do the right wing! */
            for (j = 0; (filterindex2 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)) < RESAMPLER_FILTER_SIZE; j++) {
                const int jsamples = j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING;
                const int srcframe = srcindex + 1 + j;
                /* !!! FIXME: we can bubble this conditional out of here by doing a post loop. */
                const float insample = (srcframe >= inframes) ? rpadding[((srcframe - inframes) * chans) + chan] : inbuf[(srcframe * chans) + chan];
                outsample += (float)(insample * (ResamplerFilter[filterindex2 + jsamples] + (interpolation2 * ResamplerFilterDifference[filterindex2 + jsamples])));
            }

            *(dst++) = outsample;
        }

        outtime += outtimeincr;
    }

    return outframes * chans * sizeof (float);
}

int
SDL_ConvertAudio(SDL_AudioCVT * cvt)
{
    /* !!! FIXME: (cvt) should be const; stack-copy it here. */
    /* !!! FIXME: (actually, we can't...len_cvt needs to be updated. Grr.) */

    /* Make sure there's data to convert */
    if (cvt->buf == NULL) {
        return SDL_SetError("No buffer allocated for conversion");
    }

    /* Return okay if no conversion is necessary */
    cvt->len_cvt = cvt->len;
    if (cvt->filters[0] == NULL) {
        return 0;
    }

    /* Set up the conversion and go! */
    cvt->filter_index = 0;
    cvt->filters[0] (cvt, cvt->src_format);
    return 0;
}

static void SDLCALL
SDL_Convert_Byteswap(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
#if DEBUG_CONVERT
    printf("Converting byte order\n");
#endif

    switch (SDL_AUDIO_BITSIZE(format)) {
        #define CASESWAP(b) \
            case b: { \
                Uint##b *ptr = (Uint##b *) cvt->buf; \
                int i; \
                for (i = cvt->len_cvt / sizeof (*ptr); i; --i, ++ptr) { \
                    *ptr = SDL_Swap##b(*ptr); \
                } \
                break; \
            }

        CASESWAP(16);
        CASESWAP(32);
        CASESWAP(64);

        #undef CASESWAP

        default: SDL_assert(!"unhandled byteswap datatype!"); break;
    }

    if (cvt->filters[++cvt->filter_index]) {
        /* flip endian flag for data. */
        if (format & SDL_AUDIO_MASK_ENDIAN) {
            format &= ~SDL_AUDIO_MASK_ENDIAN;
        } else {
            format |= SDL_AUDIO_MASK_ENDIAN;
        }
        cvt->filters[cvt->filter_index](cvt, format);
    }
}

static int
SDL_AddAudioCVTFilter(SDL_AudioCVT *cvt, const SDL_AudioFilter filter)
{
    if (cvt->filter_index >= SDL_AUDIOCVT_MAX_FILTERS) {
        return SDL_SetError("Too many filters needed for conversion, exceeded maximum of %d", SDL_AUDIOCVT_MAX_FILTERS);
    }
    SDL_assert(filter != NULL);
    cvt->filters[cvt->filter_index++] = filter;
    cvt->filters[cvt->filter_index] = NULL; /* Moving terminator */
    return 0;
}

static int
SDL_BuildAudioTypeCVTToFloat(SDL_AudioCVT *cvt, const SDL_AudioFormat src_fmt)
{
    int retval = 0;  /* 0 == no conversion necessary. */

    if ((SDL_AUDIO_ISBIGENDIAN(src_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && SDL_AUDIO_BITSIZE(src_fmt) > 8) {
        if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
            return -1;
        }
        retval = 1;  /* added a converter. */
    }

    if (!SDL_AUDIO_ISFLOAT(src_fmt)) {
        const Uint16 src_bitsize = SDL_AUDIO_BITSIZE(src_fmt);
        const Uint16 dst_bitsize = 32;
        SDL_AudioFilter filter = NULL;

        switch (src_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
            case AUDIO_S8: filter = SDL_Convert_S8_to_F32; break;
            case AUDIO_U8: filter = SDL_Convert_U8_to_F32; break;
            case AUDIO_S16: filter = SDL_Convert_S16_to_F32; break;
            case AUDIO_U16: filter = SDL_Convert_U16_to_F32; break;
            case AUDIO_S32: filter = SDL_Convert_S32_to_F32; break;
            default: SDL_assert(!"Unexpected audio format!"); break;
        }

        if (!filter) {
            return SDL_SetError("No conversion from source format to float available");
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1;
        }
        if (src_bitsize < dst_bitsize) {
            const int mult = (dst_bitsize / src_bitsize);
            cvt->len_mult *= mult;
            cvt->len_ratio *= mult;
        } else if (src_bitsize > dst_bitsize) {
            cvt->len_ratio /= (src_bitsize / dst_bitsize);
        }

        retval = 1;  /* added a converter. */
    }

    return retval;
}

static int
SDL_BuildAudioTypeCVTFromFloat(SDL_AudioCVT *cvt, const SDL_AudioFormat dst_fmt)
{
    int retval = 0;  /* 0 == no conversion necessary. */

    if (!SDL_AUDIO_ISFLOAT(dst_fmt)) {
        const Uint16 dst_bitsize = SDL_AUDIO_BITSIZE(dst_fmt);
        const Uint16 src_bitsize = 32;
        SDL_AudioFilter filter = NULL;
        switch (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
            case AUDIO_S8: filter = SDL_Convert_F32_to_S8; break;
            case AUDIO_U8: filter = SDL_Convert_F32_to_U8; break;
            case AUDIO_S16: filter = SDL_Convert_F32_to_S16; break;
            case AUDIO_U16: filter = SDL_Convert_F32_to_U16; break;
            case AUDIO_S32: filter = SDL_Convert_F32_to_S32; break;
            default: SDL_assert(!"Unexpected audio format!"); break;
        }

        if (!filter) {
            return SDL_SetError("No conversion from float to format 0x%.4x available", dst_fmt);
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1;
        }
        if (src_bitsize < dst_bitsize) {
            const int mult = (dst_bitsize / src_bitsize);
            cvt->len_mult *= mult;
            cvt->len_ratio *= mult;
        } else if (src_bitsize > dst_bitsize) {
            cvt->len_ratio /= (src_bitsize / dst_bitsize);
        }
        retval = 1;  /* added a converter. */
    }

    if ((SDL_AUDIO_ISBIGENDIAN(dst_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && SDL_AUDIO_BITSIZE(dst_fmt) > 8) {
        if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
            return -1;
        }
        retval = 1;  /* added a converter. */
    }

    return retval;
}

static void
SDL_ResampleCVT(SDL_AudioCVT *cvt, const int chans, const SDL_AudioFormat format)
{
    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    const int inrate = (int) (size_t) cvt->filters[SDL_AUDIOCVT_MAX_FILTERS-1];
    const int outrate = (int) (size_t) cvt->filters[SDL_AUDIOCVT_MAX_FILTERS];
    const float *src = (const float *) cvt->buf;
    const int srclen = cvt->len_cvt;
    /*float *dst = (float *) cvt->buf;
    const int dstlen = (cvt->len * cvt->len_mult);*/
    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    float *dst = (float *) (cvt->buf + srclen);
    const int dstlen = (cvt->len * cvt->len_mult) - srclen;
    const int requestedpadding = ResamplerPadding(inrate, outrate);
    int paddingsamples;
    float *padding;

    if (requestedpadding < SDL_MAX_SINT32 / chans) {
        paddingsamples = requestedpadding * chans;
    } else {
        paddingsamples = 0;
    }
    SDL_assert(format == AUDIO_F32SYS);

    /* we keep no streaming state here, so pad with silence on both ends. */
    padding = (float *) SDL_calloc(paddingsamples ? paddingsamples : 1, sizeof (float));
    if (!padding) {
        SDL_OutOfMemory();
        return;
    }

    cvt->len_cvt = SDL_ResampleAudio(chans, inrate, outrate, padding, padding, src, srclen, dst, dstlen);

    SDL_free(padding);

    SDL_memmove(cvt->buf, dst, cvt->len_cvt);  /* !!! FIXME: remove this if we can get the resampler to work in-place again. */

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, format);
    }
}

/* !!! FIXME: We only have this macro salsa because SDL_AudioCVT doesn't
   !!! FIXME:  store channel info, so we have to have function entry
   !!! FIXME:  points for each supported channel count and multiple
   !!! FIXME:  vs arbitrary. When we rev the ABI, clean this up. */
#define RESAMPLER_FUNCS(chans) \
    static void SDLCALL \
    SDL_ResampleCVT_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_ResampleCVT(cvt, chans, format); \
    }
RESAMPLER_FUNCS(1)
RESAMPLER_FUNCS(2)
RESAMPLER_FUNCS(4)
RESAMPLER_FUNCS(6)
RESAMPLER_FUNCS(8)
#undef RESAMPLER_FUNCS

static SDL_AudioFilter
ChooseCVTResampler(const int dst_channels)
{
    switch (dst_channels) {
        case 1: return SDL_ResampleCVT_c1;
        case 2: return SDL_ResampleCVT_c2;
        case 4: return SDL_ResampleCVT_c4;
        case 6: return SDL_ResampleCVT_c6;
        case 8: return SDL_ResampleCVT_c8;
        default: break;
    }

    return NULL;
}

static int
SDL_BuildAudioResampleCVT(SDL_AudioCVT * cvt, const int dst_channels,
                          const int src_rate, const int dst_rate)
{
    SDL_AudioFilter filter;

    if (src_rate == dst_rate) {
        return 0;  /* no conversion necessary. */
    }

    filter = ChooseCVTResampler(dst_channels);
    if (filter == NULL) {
        return SDL_SetError("No conversion available for these rates");
    }

    /* Update (cvt) with filter details... */
    if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
        return -1;
    }

    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    if (cvt->filter_index >= (SDL_AUDIOCVT_MAX_FILTERS-2)) {
        return SDL_SetError("Too many filters needed for conversion, exceeded maximum of %d", SDL_AUDIOCVT_MAX_FILTERS-2);
    }
    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS-1] = (SDL_AudioFilter) (uintptr_t) src_rate;
    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS] = (SDL_AudioFilter) (uintptr_t) dst_rate;

    if (src_rate < dst_rate) {
        const double mult = ((double) dst_rate) / ((double) src_rate);
        cvt->len_mult *= (int) SDL_ceil(mult);
        cvt->len_ratio *= mult;
    } else {
        cvt->len_ratio /= ((double) src_rate) / ((double) dst_rate);
    }

    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    /* the buffer is big enough to hold the destination now, but
       we need it large enough to hold a separate scratch buffer. */
    cvt->len_mult *= 2;

    return 1;               /* added a converter. */
}

static SDL_bool
SDL_SupportedAudioFormat(const SDL_AudioFormat fmt)
{
    switch (fmt) {
        case AUDIO_U8:
        case AUDIO_S8:
        case AUDIO_U16LSB:
        case AUDIO_S16LSB:
        case AUDIO_U16MSB:
        case AUDIO_S16MSB:
        case AUDIO_S32LSB:
        case AUDIO_S32MSB:
        case AUDIO_F32LSB:
        case AUDIO_F32MSB:
            return SDL_TRUE;  /* supported. */

        default:
            break;
    }

    return SDL_FALSE;  /* unsupported. */
}

static SDL_bool
SDL_SupportedChannelCount(const int channels)
{
    return ((channels >= 1) && (channels <= 8)) ? SDL_TRUE : SDL_FALSE;
}


/* Creates a set of audio filters to convert from one format to another.
   Returns 0 if no conversion is needed, 1 if the audio filter is set up,
   or -1 if an error like invalid parameter, unsupported format, etc. occurred.
*/

int
SDL_BuildAudioCVT(SDL_AudioCVT * cvt,
                  SDL_AudioFormat src_fmt, Uint8 src_channels, int src_rate,
                  SDL_AudioFormat dst_fmt, Uint8 dst_channels, int dst_rate)
{
    SDL_AudioFilter channel_converter = NULL;

    /* Sanity check target pointer */
    if (cvt == NULL) {
        return SDL_InvalidParamError("cvt");
    }

    /* Make sure we zero out the audio conversion before error checking */
    SDL_zerop(cvt);

    if (!SDL_SupportedAudioFormat(src_fmt)) {
        return SDL_SetError("Invalid source format");
    }
    if (!SDL_SupportedAudioFormat(dst_fmt)) {
        return SDL_SetError("Invalid destination format");
    }
    if (!SDL_SupportedChannelCount(src_channels)) {
        return SDL_SetError("Invalid source channels");
    }
    if (!SDL_SupportedChannelCount(dst_channels)) {
        return SDL_SetError("Invalid destination channels");
    }
    if (src_rate <= 0) {
        return SDL_SetError("Source rate is equal to or less than zero");
    }
    if (dst_rate <= 0) {
        return SDL_SetError("Destination rate is equal to or less than zero");
    }
    if (src_rate >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
        return SDL_SetError("Source rate is too high");
    }
    if (dst_rate >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
        return SDL_SetError("Destination rate is too high");
    }

#if DEBUG_CONVERT
    printf("Build format %04x->%04x, channels %u->%u, rate %d->%d\n",
           src_fmt, dst_fmt, src_channels, dst_channels, src_rate, dst_rate);
#endif

    /* Start off with no conversion necessary */
    cvt->src_format = src_fmt;
    cvt->dst_format = dst_fmt;
    cvt->needed = 0;
    cvt->filter_index = 0;
    SDL_zeroa(cvt->filters);
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = ((double) dst_rate) / ((double) src_rate);

    /* Make sure we've chosen audio conversion functions (SIMD, scalar, etc.) */
    SDL_ChooseAudioConverters();

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - resample and change channel count if necessary.
        - convert to final data format.
        - byteswap back to foreign format if necessary.

       The expectation is we can process data faster in float32
       (possibly with SIMD), and making several passes over the same
       buffer is likely to be CPU cache-friendly, avoiding the
       biggest performance hit in modern times. Previously we had
       (script-generated) custom converters for every data type and
       it was a bloat on SDL compile times and final library size. */

    /* see if we can skip float conversion entirely. */
    if (src_rate == dst_rate && src_channels == dst_channels) {
        if (src_fmt == dst_fmt) {
            return 0;
        }

        /* just a byteswap needed? */
        if ((src_fmt & ~SDL_AUDIO_MASK_ENDIAN) == (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN)) {
            if (SDL_AUDIO_BITSIZE(dst_fmt) == 8) {
                return 0;
            }
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
                return -1;
            }
            cvt->needed = 1;
            return 1;
        }
    }

    /* Convert data types, if necessary. Updates (cvt). */
    if (SDL_BuildAudioTypeCVTToFloat(cvt, src_fmt) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }


    /* Channel conversion */

    /* SDL_SupportedChannelCount should have caught these asserts, or we added a new format and forgot to update the table. */
    SDL_assert(src_channels <= SDL_arraysize(channel_converters));
    SDL_assert(dst_channels <= SDL_arraysize(channel_converters[0]));

    channel_converter = channel_converters[src_channels-1][dst_channels-1];
    if ((channel_converter == NULL) != (src_channels == dst_channels)) {
        /* All combinations of supported channel counts should have been handled by now, but let's be defensive */
        return SDL_SetError("Invalid channel combination");
    } else if (channel_converter != NULL) {
        /* swap in some SIMD versions for a few of these. */
        if (channel_converter == SDL_Convert51ToStereo) {
            SDL_AudioFilter filter = NULL;
#if 0 /* !!! FIXME: these have not been updated for the new formulas */
            #if HAVE_AVX_INTRINSICS
            if (!filter && SDL_HasAVX()) { filter = SDL_Convert51ToStereo_AVX; }
            #endif
            #if HAVE_SSE_INTRINSICS
            if (!filter && SDL_HasSSE()) { filter = SDL_Convert51ToStereo_SSE; }
            #endif
            #if HAVE_NEON_INTRINSICS
            if (!filter && SDL_HasNEON()) { filter = SDL_Convert51ToStereo_NEON; }
            #endif
#endif
            if (filter) { channel_converter = filter; }
        } else if (channel_converter == SDL_ConvertStereoToMono) {
            SDL_AudioFilter filter = NULL;
            #if HAVE_SSE3_INTRINSICS
            if (!filter && SDL_HasSSE3()) { filter = SDL_ConvertStereoToMono_SSE3; }
            #endif
            if (filter) { channel_converter = filter; }
        }

        if (SDL_AddAudioCVTFilter(cvt, channel_converter) < 0) {
            return -1;
        }

        if (src_channels < dst_channels) {
            cvt->len_mult = ((cvt->len_mult * dst_channels) + (src_channels-1)) / src_channels;
        }

        cvt->len_ratio = (cvt->len_ratio * dst_channels) / src_channels;
        src_channels = dst_channels;
    }

    /* Do rate conversion, if necessary. Updates (cvt). */
    if (SDL_BuildAudioResampleCVT(cvt, dst_channels, src_rate, dst_rate) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    /* Move to final data type. */
    if (SDL_BuildAudioTypeCVTFromFloat(cvt, dst_fmt) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    cvt->needed = (cvt->filter_index != 0);
    return (cvt->needed);
}

typedef int (*SDL_ResampleAudioStreamFunc)(SDL_AudioStream *stream, const void *inbuf, const int inbuflen, void *outbuf, const int outbuflen);
typedef void (*SDL_ResetAudioStreamResamplerFunc)(SDL_AudioStream *stream);
typedef void (*SDL_CleanupAudioStreamResamplerFunc)(SDL_AudioStream *stream);

struct _SDL_AudioStream
{
    SDL_AudioCVT cvt_before_resampling;
    SDL_AudioCVT cvt_after_resampling;
    SDL_DataQueue *queue;
    SDL_bool first_run;
    Uint8 *staging_buffer;
    int staging_buffer_size;
    int staging_buffer_filled;
    Uint8 *work_buffer_base;  /* maybe unaligned pointer from SDL_realloc(). */
    int work_buffer_len;
    int src_sample_frame_size;
    SDL_AudioFormat src_format;
    Uint8 src_channels;
    int src_rate;
    int dst_sample_frame_size;
    SDL_AudioFormat dst_format;
    Uint8 dst_channels;
    int dst_rate;
    double rate_incr;
    Uint8 pre_resample_channels;
    int packetlen;
    int resampler_padding_samples;
    float *resampler_padding;
    void *resampler_state;
    SDL_ResampleAudioStreamFunc resampler_func;
    SDL_ResetAudioStreamResamplerFunc reset_resampler_func;
    SDL_CleanupAudioStreamResamplerFunc cleanup_resampler_func;
};

static Uint8 *
EnsureStreamBufferSize(SDL_AudioStream *stream, const int newlen)
{
    Uint8 *ptr;
    size_t offset;

    if (stream->work_buffer_len >= newlen) {
        ptr = stream->work_buffer_base;
    } else {
        ptr = (Uint8 *) SDL_realloc(stream->work_buffer_base, newlen + 32);
        if (!ptr) {
            SDL_OutOfMemory();
            return NULL;
        }
        /* Make sure we're aligned to 16 bytes for SIMD code. */
        stream->work_buffer_base = ptr;
        stream->work_buffer_len = newlen;
    }

    offset = ((size_t) ptr) & 15;
    return offset ? ptr + (16 - offset) : ptr;
}

#ifdef HAVE_LIBSAMPLERATE_H
static int
SDL_ResampleAudioStream_SRC(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const float *inbuf = (const float *) _inbuf;
    float *outbuf = (float *) _outbuf;
    const int framelen = sizeof(float) * stream->pre_resample_channels;
    SRC_STATE *state = (SRC_STATE *)stream->resampler_state;
    SRC_DATA data;
    int result;

    SDL_assert(inbuf != ((const float *) outbuf));  /* SDL_AudioStreamPut() shouldn't allow in-place resamples. */

    data.data_in = (float *)inbuf; /* Older versions of libsamplerate had a non-const pointer, but didn't write to it */
    data.input_frames = inbuflen / framelen;
    data.input_frames_used = 0;

    data.data_out = outbuf;
    data.output_frames = outbuflen / framelen;

    data.end_of_input = 0;
    data.src_ratio = stream->rate_incr;

    result = SRC_src_process(state, &data);
    if (result != 0) {
        SDL_SetError("src_process() failed: %s", SRC_src_strerror(result));
        return 0;
    }

    /* If this fails, we need to store them off somewhere */
    SDL_assert(data.input_frames_used == data.input_frames);

    return data.output_frames_gen * (sizeof(float) * stream->pre_resample_channels);
}

static void
SDL_ResetAudioStreamResampler_SRC(SDL_AudioStream *stream)
{
    SRC_src_reset((SRC_STATE *)stream->resampler_state);
}

static void
SDL_CleanupAudioStreamResampler_SRC(SDL_AudioStream *stream)
{
    SRC_STATE *state = (SRC_STATE *)stream->resampler_state;
    if (state) {
        SRC_src_delete(state);
    }

    stream->resampler_state = NULL;
    stream->resampler_func = NULL;
    stream->reset_resampler_func = NULL;
    stream->cleanup_resampler_func = NULL;
}

static SDL_bool
SetupLibSampleRateResampling(SDL_AudioStream *stream)
{
    int result = 0;
    SRC_STATE *state = NULL;

    if (SRC_available) {
        state = SRC_src_new(SRC_converter, stream->pre_resample_channels, &result);
        if (!state) {
            SDL_SetError("src_new() failed: %s", SRC_src_strerror(result));
        }
    }

    if (!state) {
        SDL_CleanupAudioStreamResampler_SRC(stream);
        return SDL_FALSE;
    }

    stream->resampler_state = state;
    stream->resampler_func = SDL_ResampleAudioStream_SRC;
    stream->reset_resampler_func = SDL_ResetAudioStreamResampler_SRC;
    stream->cleanup_resampler_func = SDL_CleanupAudioStreamResampler_SRC;

    return SDL_TRUE;
}
#endif /* HAVE_LIBSAMPLERATE_H */


static int
SDL_ResampleAudioStream(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const Uint8 *inbufend = ((const Uint8 *) _inbuf) + inbuflen;
    const float *inbuf = (const float *) _inbuf;
    float *outbuf = (float *) _outbuf;
    const int chans = (int) stream->pre_resample_channels;
    const int inrate = stream->src_rate;
    const int outrate = stream->dst_rate;
    const int paddingsamples = stream->resampler_padding_samples;
    const int paddingbytes = paddingsamples * sizeof (float);
    float *lpadding = (float *) stream->resampler_state;
    const float *rpadding = (const float *) inbufend; /* we set this up so there are valid padding samples at the end of the input buffer. */
    const int cpy = SDL_min(inbuflen, paddingbytes);
    int retval;

    SDL_assert(inbuf != ((const float *) outbuf));  /* SDL_AudioStreamPut() shouldn't allow in-place resamples. */

    retval = SDL_ResampleAudio(chans, inrate, outrate, lpadding, rpadding, inbuf, inbuflen, outbuf, outbuflen);

    /* update our left padding with end of current input, for next run. */
    SDL_memcpy((lpadding + paddingsamples) - (cpy / sizeof (float)), inbufend - cpy, cpy);
    return retval;
}

static void
SDL_ResetAudioStreamResampler(SDL_AudioStream *stream)
{
    /* set all the padding to silence. */
    const int len = stream->resampler_padding_samples;
    SDL_memset(stream->resampler_state, '\0', len * sizeof (float));
}

static void
SDL_CleanupAudioStreamResampler(SDL_AudioStream *stream)
{
    SDL_free(stream->resampler_state);
}

SDL_AudioStream *
SDL_NewAudioStream(const SDL_AudioFormat src_format,
                   const Uint8 src_channels,
                   const int src_rate,
                   const SDL_AudioFormat dst_format,
                   const Uint8 dst_channels,
                   const int dst_rate)
{
    const int packetlen = 4096;  /* !!! FIXME: good enough for now. */
    Uint8 pre_resample_channels;
    SDL_AudioStream *retval;

    retval = (SDL_AudioStream *) SDL_calloc(1, sizeof (SDL_AudioStream));
    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* If increasing channels, do it after resampling, since we'd just
       do more work to resample duplicate channels. If we're decreasing, do
       it first so we resample the interpolated data instead of interpolating
       the resampled data (!!! FIXME: decide if that works in practice, though!). */
    pre_resample_channels = SDL_min(src_channels, dst_channels);

    retval->first_run = SDL_TRUE;
    retval->src_sample_frame_size = (SDL_AUDIO_BITSIZE(src_format) / 8) * src_channels;
    retval->src_format = src_format;
    retval->src_channels = src_channels;
    retval->src_rate = src_rate;
    retval->dst_sample_frame_size = (SDL_AUDIO_BITSIZE(dst_format) / 8) * dst_channels;
    retval->dst_format = dst_format;
    retval->dst_channels = dst_channels;
    retval->dst_rate = dst_rate;
    retval->pre_resample_channels = pre_resample_channels;
    retval->packetlen = packetlen;
    retval->rate_incr = ((double) dst_rate) / ((double) src_rate);
    retval->resampler_padding_samples = ResamplerPadding(retval->src_rate, retval->dst_rate) * pre_resample_channels;
    retval->resampler_padding = (float *) SDL_calloc(retval->resampler_padding_samples ? retval->resampler_padding_samples : 1, sizeof (float));

    if (retval->resampler_padding == NULL) {
        SDL_FreeAudioStream(retval);
        SDL_OutOfMemory();
        return NULL;
    }

    retval->staging_buffer_size = ((retval->resampler_padding_samples / retval->pre_resample_channels) * retval->src_sample_frame_size);
    if (retval->staging_buffer_size > 0) {
        retval->staging_buffer = (Uint8 *) SDL_malloc(retval->staging_buffer_size);
        if (retval->staging_buffer == NULL) {
            SDL_FreeAudioStream(retval);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    /* Not resampling? It's an easy conversion (and maybe not even that!) */
    if (src_rate == dst_rate) {
        retval->cvt_before_resampling.needed = SDL_FALSE;
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, src_format, src_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    } else {
        /* Don't resample at first. Just get us to Float32 format. */
        /* !!! FIXME: convert to int32 on devices without hardware float. */
        if (SDL_BuildAudioCVT(&retval->cvt_before_resampling, src_format, src_channels, src_rate, AUDIO_F32SYS, pre_resample_channels, src_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }

#ifdef HAVE_LIBSAMPLERATE_H
        SetupLibSampleRateResampling(retval);
#endif

        if (!retval->resampler_func) {
            retval->resampler_state = SDL_calloc(retval->resampler_padding_samples, sizeof (float));
            if (!retval->resampler_state) {
                SDL_FreeAudioStream(retval);
                SDL_OutOfMemory();
                return NULL;
            }

            retval->resampler_func = SDL_ResampleAudioStream;
            retval->reset_resampler_func = SDL_ResetAudioStreamResampler;
            retval->cleanup_resampler_func = SDL_CleanupAudioStreamResampler;
        }

        /* Convert us to the final format after resampling. */
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, AUDIO_F32SYS, pre_resample_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    }

    retval->queue = SDL_NewDataQueue(packetlen, packetlen * 2);
    if (!retval->queue) {
        SDL_FreeAudioStream(retval);
        return NULL;  /* SDL_NewDataQueue should have called SDL_SetError. */
    }

    return retval;
}

static int
SDL_AudioStreamPutInternal(SDL_AudioStream *stream, const void *buf, int len, int *maxputbytes)
{
    int buflen = len;
    int workbuflen;
    Uint8 *workbuf;
    Uint8 *resamplebuf = NULL;
    int resamplebuflen = 0;
    int neededpaddingbytes;
    int paddingbytes;

    /* !!! FIXME: several converters can take advantage of SIMD, but only
       !!! FIXME:  if the data is aligned to 16 bytes. EnsureStreamBufferSize()
       !!! FIXME:  guarantees the buffer will align, but the
       !!! FIXME:  converters will iterate over the data backwards if
       !!! FIXME:  the output grows, and this means we won't align if buflen
       !!! FIXME:  isn't a multiple of 16. In these cases, we should chop off
       !!! FIXME:  a few samples at the end and convert them separately. */

    /* no padding prepended on first run. */
    neededpaddingbytes = stream->resampler_padding_samples * sizeof (float);
    paddingbytes = stream->first_run ? 0 : neededpaddingbytes;
    stream->first_run = SDL_FALSE;

    /* Make sure the work buffer can hold all the data we need at once... */
    workbuflen = buflen;
    if (stream->cvt_before_resampling.needed) {
        workbuflen *= stream->cvt_before_resampling.len_mult;
    }

    if (stream->dst_rate != stream->src_rate) {
        /* resamples can't happen in place, so make space for second buf. */
        const int framesize = stream->pre_resample_channels * sizeof (float);
        const int frames = workbuflen / framesize;
        resamplebuflen = ((int) SDL_ceil(frames * stream->rate_incr)) * framesize;
        #if DEBUG_AUDIOSTREAM
        printf("AUDIOSTREAM: will resample %d bytes to %d (ratio=%.6f)\n", workbuflen, resamplebuflen, stream->rate_incr);
        #endif
        workbuflen += resamplebuflen;
    }

    if (stream->cvt_after_resampling.needed) {
        /* !!! FIXME: buffer might be big enough already? */
        workbuflen *= stream->cvt_after_resampling.len_mult;
    }

    workbuflen += neededpaddingbytes;

    #if DEBUG_AUDIOSTREAM
    printf("AUDIOSTREAM: Putting %d bytes of preconverted audio, need %d byte work buffer\n", buflen, workbuflen);
    #endif

    workbuf = EnsureStreamBufferSize(stream, workbuflen);
    if (!workbuf) {
        return -1;  /* probably out of memory. */
    }

    resamplebuf = workbuf;  /* default if not resampling. */

    SDL_memcpy(workbuf + paddingbytes, buf, buflen);

    if (stream->cvt_before_resampling.needed) {
        stream->cvt_before_resampling.buf = workbuf + paddingbytes;
        stream->cvt_before_resampling.len = buflen;
        if (SDL_ConvertAudio(&stream->cvt_before_resampling) == -1) {
            return -1;   /* uhoh! */
        }
        buflen = stream->cvt_before_resampling.len_cvt;

        #if DEBUG_AUDIOSTREAM
        printf("AUDIOSTREAM: After initial conversion we have %d bytes\n", buflen);
        #endif
    }

    if (stream->dst_rate != stream->src_rate) {
        /* save off some samples at the end; they are used for padding now so
           the resampler is coherent and then used at the start of the next
           put operation. Prepend last put operation's padding, too. */

        /* prepend prior put's padding. :P */
        if (paddingbytes) {
            SDL_memcpy(workbuf, stream->resampler_padding, paddingbytes);
            buflen += paddingbytes;
        }

        /* save off the data at the end for the next run. */
        SDL_memcpy(stream->resampler_padding, workbuf + (buflen - neededpaddingbytes), neededpaddingbytes);

        resamplebuf = workbuf + buflen;  /* skip to second piece of workbuf. */
        SDL_assert(buflen >= neededpaddingbytes);
        if (buflen > neededpaddingbytes) {
            buflen = stream->resampler_func(stream, workbuf, buflen - neededpaddingbytes, resamplebuf, resamplebuflen);
        } else {
            buflen = 0;
        }

        #if DEBUG_AUDIOSTREAM
        printf("AUDIOSTREAM: After resampling we have %d bytes\n", buflen);
        #endif
    }

    if (stream->cvt_after_resampling.needed && (buflen > 0)) {
        stream->cvt_after_resampling.buf = resamplebuf;
        stream->cvt_after_resampling.len = buflen;
        if (SDL_ConvertAudio(&stream->cvt_after_resampling) == -1) {
            return -1;   /* uhoh! */
        }
        buflen = stream->cvt_after_resampling.len_cvt;

        #if DEBUG_AUDIOSTREAM
        printf("AUDIOSTREAM: After final conversion we have %d bytes\n", buflen);
        #endif
    }

    #if DEBUG_AUDIOSTREAM
    printf("AUDIOSTREAM: Final output is %d bytes\n", buflen);
    #endif

    if (maxputbytes) {
        const int maxbytes = *maxputbytes;
        if (buflen > maxbytes)
            buflen = maxbytes;
        *maxputbytes -= buflen;
    }

    /* resamplebuf holds the final output, even if we didn't resample. */
    return buflen ? SDL_WriteToDataQueue(stream->queue, resamplebuf, buflen) : 0;
}

int
SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, int len)
{
    /* !!! FIXME: several converters can take advantage of SIMD, but only
       !!! FIXME:  if the data is aligned to 16 bytes. EnsureStreamBufferSize()
       !!! FIXME:  guarantees the buffer will align, but the
       !!! FIXME:  converters will iterate over the data backwards if
       !!! FIXME:  the output grows, and this means we won't align if buflen
       !!! FIXME:  isn't a multiple of 16. In these cases, we should chop off
       !!! FIXME:  a few samples at the end and convert them separately. */

    #if DEBUG_AUDIOSTREAM
    printf("AUDIOSTREAM: wants to put %d preconverted bytes\n", buflen);
    #endif

    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    if (!buf) {
        return SDL_InvalidParamError("buf");
    }
    if (len == 0) {
        return 0;  /* nothing to do. */
    }
    if ((len % stream->src_sample_frame_size) != 0) {
        return SDL_SetError("Can't add partial sample frames");
    }

    if (!stream->cvt_before_resampling.needed &&
        (stream->dst_rate == stream->src_rate) &&
        !stream->cvt_after_resampling.needed) {
        #if DEBUG_AUDIOSTREAM
        printf("AUDIOSTREAM: no conversion needed at all, queueing %d bytes.\n", len);
        #endif
        return SDL_WriteToDataQueue(stream->queue, buf, len);
    }

    while (len > 0) {
        int amount;

        /* If we don't have a staging buffer or we're given enough data that
           we don't need to store it for later, skip the staging process.
         */
        if (!stream->staging_buffer_filled && len >= stream->staging_buffer_size) {
            return SDL_AudioStreamPutInternal(stream, buf, len, NULL);
        }

        /* If there's not enough data to fill the staging buffer, just save it */
        if ((stream->staging_buffer_filled + len) < stream->staging_buffer_size) {
            SDL_memcpy(stream->staging_buffer + stream->staging_buffer_filled, buf, len);
            stream->staging_buffer_filled += len;
            return 0;
        }

        /* Fill the staging buffer, process it, and continue */
        amount = (stream->staging_buffer_size - stream->staging_buffer_filled);
        SDL_assert(amount > 0);
        SDL_memcpy(stream->staging_buffer + stream->staging_buffer_filled, buf, amount);
        stream->staging_buffer_filled = 0;
        if (SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_size, NULL) < 0) {
            return -1;
        }
        buf = (void *)((Uint8 *)buf + amount);
        len -= amount;
    }
    return 0;
}

int SDL_AudioStreamFlush(SDL_AudioStream *stream)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }

    #if DEBUG_AUDIOSTREAM
    printf("AUDIOSTREAM: flushing! staging_buffer_filled=%d bytes\n", stream->staging_buffer_filled);
    #endif

    /* shouldn't use a staging buffer if we're not resampling. */
    SDL_assert((stream->dst_rate != stream->src_rate) || (stream->staging_buffer_filled == 0));

    if (stream->staging_buffer_filled > 0) {
        /* push the staging buffer + silence. We need to flush out not just
           the staging buffer, but the piece that the stream was saving off
           for right-side resampler padding. */
        const SDL_bool first_run = stream->first_run;
        const int filled = stream->staging_buffer_filled;
        int actual_input_frames = filled / stream->src_sample_frame_size;
        if (!first_run)
            actual_input_frames += stream->resampler_padding_samples / stream->pre_resample_channels;

        if (actual_input_frames > 0) {  /* don't bother if nothing to flush. */
            /* This is how many bytes we're expecting without silence appended. */
            int flush_remaining = ((int) SDL_ceil(actual_input_frames * stream->rate_incr)) * stream->dst_sample_frame_size;

            #if DEBUG_AUDIOSTREAM
            printf("AUDIOSTREAM: flushing with padding to get max %d bytes!\n", flush_remaining);
            #endif

            SDL_memset(stream->staging_buffer + filled, '\0', stream->staging_buffer_size - filled);
            if (SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_size, &flush_remaining) < 0) {
                return -1;
            }

            /* we have flushed out (or initially filled) the pending right-side
               resampler padding, but we need to push more silence to guarantee
               the staging buffer is fully flushed out, too. */
            SDL_memset(stream->staging_buffer, '\0', filled);
            if (SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_size, &flush_remaining) < 0) {
                return -1;
            }
        }
    }

    stream->staging_buffer_filled = 0;
    stream->first_run = SDL_TRUE;

    return 0;
}

/* get converted/resampled data from the stream */
int
SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, int len)
{
    #if DEBUG_AUDIOSTREAM
    printf("AUDIOSTREAM: want to get %d converted bytes\n", len);
    #endif

    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    if (!buf) {
        return SDL_InvalidParamError("buf");
    }
    if (len <= 0) {
        return 0;  /* nothing to do. */
    }
    if ((len % stream->dst_sample_frame_size) != 0) {
        return SDL_SetError("Can't request partial sample frames");
    }

    return (int) SDL_ReadFromDataQueue(stream->queue, buf, len);
}

/* number of converted/resampled bytes available */
int
SDL_AudioStreamAvailable(SDL_AudioStream *stream)
{
    return stream ? (int) SDL_CountDataQueue(stream->queue) : 0;
}

void
SDL_AudioStreamClear(SDL_AudioStream *stream)
{
    if (!stream) {
        SDL_InvalidParamError("stream");
    } else {
        SDL_ClearDataQueue(stream->queue, stream->packetlen * 2);
        if (stream->reset_resampler_func) {
            stream->reset_resampler_func(stream);
        }
        stream->first_run = SDL_TRUE;
        stream->staging_buffer_filled = 0;
    }
}

/* dispose of a stream */
void
SDL_FreeAudioStream(SDL_AudioStream *stream)
{
    if (stream) {
        if (stream->cleanup_resampler_func) {
            stream->cleanup_resampler_func(stream);
        }
        SDL_FreeDataQueue(stream->queue);
        SDL_free(stream->staging_buffer);
        SDL_free(stream->work_buffer_base);
        SDL_free(stream->resampler_padding);
        SDL_free(stream);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
