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

#include "SDL_sysaudio.h"
#include "SDL_audioresample.h"

// SDL's resampler uses a "bandlimited interpolation" algorithm:
//     https://ccrma.stanford.edu/~jos/resample/

#include "SDL_audio_resampler_filter.h"

// For a given srcpos, `srcpos + frame` are sampled, where `-RESAMPLER_ZERO_CROSSINGS < frame <= RESAMPLER_ZERO_CROSSINGS`.
// Note, when upsampling, it is also possible to start sampling from `srcpos = -1`.
#define RESAMPLER_MAX_PADDING_FRAMES (RESAMPLER_ZERO_CROSSINGS + 1)

#define RESAMPLER_FILTER_INTERP_BITS  (32 - RESAMPLER_BITS_PER_ZERO_CROSSING)
#define RESAMPLER_FILTER_INTERP_RANGE (1 << RESAMPLER_FILTER_INTERP_BITS)

#define RESAMPLER_SAMPLES_PER_FRAME (RESAMPLER_ZERO_CROSSINGS * 2)

#define RESAMPLER_FULL_FILTER_SIZE (RESAMPLER_SAMPLES_PER_FRAME * (RESAMPLER_SAMPLES_PER_ZERO_CROSSING + 1))

static void ResampleFrame_Scalar(const float *src, float *dst, const float *raw_filter, float interp, int chans)
{
    int i, chan;

    float filter[RESAMPLER_SAMPLES_PER_FRAME];

    // Interpolate between the nearest two filters
    for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
        filter[i] = (raw_filter[i] * (1.0f - interp)) + (raw_filter[i + RESAMPLER_SAMPLES_PER_FRAME] * interp);
    }

    if (chans == 2) {
        float out[2];
        out[0] = 0.0f;
        out[1] = 0.0f;

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            const float scale = filter[i];
            out[0] += src[i * 2 + 0] * scale;
            out[1] += src[i * 2 + 1] * scale;
        }

        dst[0] = out[0];
        dst[1] = out[1];
        return;
    }

    if (chans == 1) {
        float out = 0.0f;

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            out += src[i] * filter[i];
        }

        dst[0] = out;
        return;
    }

    for (chan = 0; chan < chans; chan++) {
        float f = 0.0f;

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            f += src[i * chans + chan] * filter[i];
        }

        dst[chan] = f;
    }
}

#ifdef SDL_SSE_INTRINSICS
static void SDL_TARGETING("sse") ResampleFrame_SSE(const float *src, float *dst, const float *raw_filter, float interp, int chans)
{
#if RESAMPLER_SAMPLES_PER_FRAME != 10
#error Invalid samples per frame
#endif

    // Load the filter
    __m128 f0 = _mm_loadu_ps(raw_filter + 0);
    __m128 f1 = _mm_loadu_ps(raw_filter + 4);
    __m128 f2 = _mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(raw_filter + 8));

    __m128 g0 = _mm_loadu_ps(raw_filter + 10);
    __m128 g1 = _mm_loadu_ps(raw_filter + 14);
    __m128 g2 = _mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(raw_filter + 18));

    __m128 interp1 = _mm_set1_ps(interp);
    __m128 interp2 = _mm_sub_ps(_mm_set1_ps(1.0f), _mm_set1_ps(interp));

    // Linear interpolate the filter
    f0 = _mm_add_ps(_mm_mul_ps(f0, interp2), _mm_mul_ps(g0, interp1));
    f1 = _mm_add_ps(_mm_mul_ps(f1, interp2), _mm_mul_ps(g1, interp1));
    f2 = _mm_add_ps(_mm_mul_ps(f2, interp2), _mm_mul_ps(g2, interp1));

    if (chans == 2) {
        // Duplicate each of the filter elements
        g0 = _mm_unpackhi_ps(f0, f0);
        f0 = _mm_unpacklo_ps(f0, f0);
        g1 = _mm_unpackhi_ps(f1, f1);
        f1 = _mm_unpacklo_ps(f1, f1);
        f2 = _mm_unpacklo_ps(f2, f2);

        // Multiply the filter by the input
        f0 = _mm_mul_ps(f0, _mm_loadu_ps(src + 0));
        g0 = _mm_mul_ps(g0, _mm_loadu_ps(src + 4));
        f1 = _mm_mul_ps(f1, _mm_loadu_ps(src + 8));
        g1 = _mm_mul_ps(g1, _mm_loadu_ps(src + 12));
        f2 = _mm_mul_ps(f2, _mm_loadu_ps(src + 16));

        // Calculate the sum
        f0 = _mm_add_ps(_mm_add_ps(_mm_add_ps(f0, g0), _mm_add_ps(f1, g1)), f2);
        f0 = _mm_add_ps(f0, _mm_movehl_ps(f0, f0));

        // Store the result
        _mm_storel_pi((__m64 *)dst, f0);
        return;
    }

    if (chans == 1) {
        // Multiply the filter by the input
        f0 = _mm_mul_ps(f0, _mm_loadu_ps(src + 0));
        f1 = _mm_mul_ps(f1, _mm_loadu_ps(src + 4));
        f2 = _mm_mul_ps(f2, _mm_loadl_pi(_mm_setzero_ps(), (const __m64 *)(src + 8)));

        // Calculate the sum
        f0 = _mm_add_ps(f0, f1);
        f0 = _mm_add_ps(_mm_add_ps(f0, f2), _mm_movehl_ps(f0, f0));
        f0 = _mm_add_ss(f0, _mm_shuffle_ps(f0, f0, _MM_SHUFFLE(1, 1, 1, 1)));

        // Store the result
        _mm_store_ss(dst, f0);
        return;
    }

    float filter[RESAMPLER_SAMPLES_PER_FRAME];
    _mm_storeu_ps(filter + 0, f0);
    _mm_storeu_ps(filter + 4, f1);
    _mm_storel_pi((__m64 *)(filter + 8), f2);

    int i, chan = 0;

    for (; chan + 4 <= chans; chan += 4) {
        f0 = _mm_setzero_ps();

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            f0 = _mm_add_ps(f0, _mm_mul_ps(_mm_loadu_ps(&src[i * chans + chan]), _mm_load1_ps(&filter[i])));
        }

        _mm_storeu_ps(&dst[chan], f0);
    }

    for (; chan < chans; chan++) {
        f0 = _mm_setzero_ps();

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            f0 = _mm_add_ss(f0, _mm_mul_ss(_mm_load_ss(&src[i * chans + chan]), _mm_load_ss(&filter[i])));
        }

        _mm_store_ss(&dst[chan], f0);
    }
}
#endif

static void (*ResampleFrame)(const float *src, float *dst, const float *raw_filter, float interp, int chans);

static float FullResamplerFilter[RESAMPLER_FULL_FILTER_SIZE];

void SDL_SetupAudioResampler(void)
{
    static SDL_bool setup = SDL_FALSE;
    if (setup) {
        return;
    }

    // Build a table combining the left and right wings, for faster access
    int i, j;

    for (i = 0; i < RESAMPLER_SAMPLES_PER_ZERO_CROSSING; ++i) {
        for (j = 0; j < RESAMPLER_ZERO_CROSSINGS; j++) {
            int lwing = (i * RESAMPLER_SAMPLES_PER_FRAME) + (RESAMPLER_ZERO_CROSSINGS - 1) - j;
            int rwing = (RESAMPLER_FULL_FILTER_SIZE - 1) - lwing;

            float value = ResamplerFilter[(i * RESAMPLER_ZERO_CROSSINGS) + j];
            FullResamplerFilter[lwing] = value;
            FullResamplerFilter[rwing] = value;
        }
    }

    for (i = 0; i < RESAMPLER_ZERO_CROSSINGS; ++i) {
        int rwing = i + RESAMPLER_ZERO_CROSSINGS;
        int lwing = (RESAMPLER_FULL_FILTER_SIZE - 1) - rwing;

        FullResamplerFilter[lwing] = 0.0f;
        FullResamplerFilter[rwing] = 0.0f;
    }

    ResampleFrame = ResampleFrame_Scalar;

#ifdef SDL_SSE_INTRINSICS
    if (SDL_HasSSE()) {
        ResampleFrame = ResampleFrame_SSE;
    }
#endif

    setup = SDL_TRUE;
}

Sint64 SDL_GetResampleRate(int src_rate, int dst_rate)
{
    SDL_assert(src_rate > 0);
    SDL_assert(dst_rate > 0);

    Sint64 sample_rate = ((Sint64)src_rate << 32) / (Sint64)dst_rate;
    SDL_assert(sample_rate > 0);

    return sample_rate;
}

int SDL_GetResamplerHistoryFrames(void)
{
    // Even if we aren't currently resampling, make sure to keep enough history in case we need to later.

    return RESAMPLER_MAX_PADDING_FRAMES;
}

int SDL_GetResamplerPaddingFrames(Sint64 resample_rate)
{
    // This must always be <= SDL_GetResamplerHistoryFrames()

    return resample_rate ? RESAMPLER_MAX_PADDING_FRAMES : 0;
}

// These are not general purpose. They do not check for all possible underflow/overflow
SDL_FORCE_INLINE Sint64 ResamplerAdd(Sint64 a, Sint64 b, Sint64 *ret)
{
    if ((b > 0) && (a > SDL_MAX_SINT64 - b)) {
        return -1;
    }

    *ret = a + b;
    return 0;
}

SDL_FORCE_INLINE Sint64 ResamplerMul(Sint64 a, Sint64 b, Sint64 *ret)
{
    if ((b > 0) && (a > SDL_MAX_SINT64 / b)) {
        return -1;
    }

    *ret = a * b;
    return 0;
}

Sint64 SDL_GetResamplerInputFrames(Sint64 output_frames, Sint64 resample_rate, Sint64 resample_offset)
{
    // Calculate the index of the last input frame, then add 1.
    // ((((output_frames - 1) * resample_rate) + resample_offset) >> 32) + 1

    Sint64 output_offset;
    if (ResamplerMul(output_frames, resample_rate, &output_offset) ||
        ResamplerAdd(output_offset, -resample_rate + resample_offset + 0x100000000, &output_offset)) {
        output_offset = SDL_MAX_SINT64;
    }

    Sint64 input_frames = (Sint64)(Sint32)(output_offset >> 32);
    input_frames = SDL_max(input_frames, 0);

    return input_frames;
}

Sint64 SDL_GetResamplerOutputFrames(Sint64 input_frames, Sint64 resample_rate, Sint64 *inout_resample_offset)
{
    Sint64 resample_offset = *inout_resample_offset;

    // input_offset = (input_frames << 32) - resample_offset;
    Sint64 input_offset;
    if (ResamplerMul(input_frames, 0x100000000, &input_offset) ||
        ResamplerAdd(input_offset, -resample_offset, &input_offset)) {
        input_offset = SDL_MAX_SINT64;
    }

    // output_frames = div_ceil(input_offset, resample_rate)
    Sint64 output_frames = (input_offset > 0) ? (((input_offset - 1) / resample_rate) + 1) : 0;

    *inout_resample_offset = (output_frames * resample_rate) - input_offset;

    return output_frames;
}

void SDL_ResampleAudio(int chans, const float *src, int inframes, float *dst, int outframes,
                       Sint64 resample_rate, Sint64 *inout_resample_offset)
{
    int i;
    Sint64 srcpos = *inout_resample_offset;

    SDL_assert(resample_rate > 0);

    for (i = 0; i < outframes; i++) {
        int srcindex = (int)(Sint32)(srcpos >> 32);
        Uint32 srcfraction = (Uint32)(srcpos & 0xFFFFFFFF);
        srcpos += resample_rate;

        SDL_assert(srcindex >= -1 && srcindex < inframes);

        const float *filter = &FullResamplerFilter[(srcfraction >> RESAMPLER_FILTER_INTERP_BITS) * RESAMPLER_SAMPLES_PER_FRAME];
        const float interp = (float)(srcfraction & (RESAMPLER_FILTER_INTERP_RANGE - 1)) * (1.0f / RESAMPLER_FILTER_INTERP_RANGE);

        const float *frame = &src[(srcindex - (RESAMPLER_ZERO_CROSSINGS - 1)) * chans];
        ResampleFrame(frame, dst, filter, interp, chans);

        dst += chans;
    }

    *inout_resample_offset = srcpos - ((Sint64)inframes << 32);
}
