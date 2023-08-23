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

// Functions for audio drivers to perform runtime conversion of audio format

#include "SDL_audio_c.h"

#include "../SDL_dataqueue.h"


/* SDL's resampler uses a "bandlimited interpolation" algorithm:
     https://ccrma.stanford.edu/~jos/resample/ */

#include "SDL_audio_resampler_filter.h"

/* For a given srcpos, `srcpos + frame` are sampled, where `-RESAMPLER_ZERO_CROSSINGS < frame <= RESAMPLER_ZERO_CROSSINGS`.
 * Note, when upsampling, it is also possible to start sampling from `srcpos = -1`. */
#define RESAMPLER_MAX_PADDING_FRAMES (RESAMPLER_ZERO_CROSSINGS + 1)

/* The source position is tracked using 32:32 fixed-point arithmetic.
 * This gives high precision and avoids lots of divides in ResampleAudio. */
static Sint64 GetResampleRate(const int src_rate, const int dst_rate)
{
    SDL_assert(src_rate > 0);
    SDL_assert(dst_rate > 0);

    if (src_rate == dst_rate)
        return 0;

    Sint64 sample_rate = ((Sint64)src_rate << 32) / (Sint64)dst_rate;
    SDL_assert(sample_rate > 0);

    return sample_rate;
}

static size_t GetResamplerAvailableOutputFrames(const size_t input_frames, const Sint64 resample_rate, const Sint64 resample_offset)
{
    const Sint64 output_frames = ((((Sint64)input_frames << 32) - resample_offset - 1) / resample_rate) + 1;

    return (size_t) SDL_max(output_frames, 0);
}

static int GetResamplerNeededInputFrames(const int output_frames, const Sint64 resample_rate, const Sint64 resample_offset)
{
    const Sint32 input_frames = (Sint32)((((output_frames - 1) * resample_rate) + resample_offset) >> 32) + 1;

    return (int) SDL_max(input_frames, 0);
}

static int GetResamplerPaddingFrames(const Sint64 resample_rate)
{
    return resample_rate ? RESAMPLER_MAX_PADDING_FRAMES : 0;
}

static int GetHistoryBufferSampleFrames(const int required_resampler_frames)
{
    SDL_assert(required_resampler_frames <= RESAMPLER_MAX_PADDING_FRAMES);

    // Even if we aren't currently resampling, make sure to keep enough history in case we need to later.
    return RESAMPLER_MAX_PADDING_FRAMES;
}

#define RESAMPLER_FILTER_INTERP_BITS (32 - RESAMPLER_BITS_PER_ZERO_CROSSING)
#define RESAMPLER_FILTER_INTERP_RANGE (1 << RESAMPLER_FILTER_INTERP_BITS)

#define RESAMPLER_SAMPLES_PER_FRAME (RESAMPLER_ZERO_CROSSINGS * 2)

// TODO: Add SIMD-accelerated versions
static void ResampleFrame(const float* src, float* dst, const float* filter, const int chans)
{
    int i, chan;

    if (chans == 2) {
        float v0 = 0.0f;
        float v1 = 0.0f;

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            const float scale = filter[i];
            v0 += src[i * 2 + 0] * scale;
            v1 += src[i * 2 + 1] * scale;
        }

        dst[0] = v0;
        dst[1] = v1;
        return;
    }

    if (chans == 1) {
        float v0 = 0.0f;

        for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
            v0 += src[i] * filter[i];
        }

        dst[0] = v0;
        return;
    }

    // Try and give the compiler a hint about how many channels there are
    if (chans < 1 || chans > 8) {
        SDL_assert(!"Invalid channel count");
        return;
    }

    // Calculate the result in-place
    for (chan = 0; chan < chans; ++chan) {
        dst[chan] = 0.0f;
    }

    for (i = 0; i < RESAMPLER_SAMPLES_PER_FRAME; i++) {
        const float* inputs = &src[i * chans];
        const float scale = filter[i];

        for (chan = 0; chan < chans; chan++) {
            dst[chan] += inputs[chan] * scale;
        }
    }
}

static void ResampleAudio(const int chans, const float *inbuf, const int inframes, float *outbuf, const int outframes,
                         const Sint64 resample_rate, Sint64* resample_offset)
{
    SDL_assert(resample_rate > 0);
    float *dst = outbuf;
    int i, j;

    Sint64 srcpos = *resample_offset;

    for (i = 0; i < outframes; i++) {
        int srcindex = (int)(Sint32)(srcpos >> 32);
        Uint32 srcfraction = (Uint32)(srcpos & 0xFFFFFFFF);
        srcpos += resample_rate;

        SDL_assert(srcindex >= -1 && srcindex < inframes);

        const int filterindex = (int)(srcfraction >> RESAMPLER_FILTER_INTERP_BITS) * RESAMPLER_ZERO_CROSSINGS;

        const float interpolation1 = (float)(srcfraction & (RESAMPLER_FILTER_INTERP_RANGE - 1)) * (1.0f / RESAMPLER_FILTER_INTERP_RANGE);
        const float interpolation2 = 1.0f - interpolation1;

        float filter[RESAMPLER_SAMPLES_PER_FRAME];

        for (j = 0; j < RESAMPLER_ZERO_CROSSINGS; j++) {
            const int filt_ind1 = filterindex + (RESAMPLER_ZERO_CROSSINGS - 1) - j;
            const int filt_ind2 = (RESAMPLER_FILTER_SIZE - 1) - filt_ind1;

            const float scale1 = (ResamplerFilter[filt_ind1] * interpolation2) + (ResamplerFilter[filt_ind1 + RESAMPLER_ZERO_CROSSINGS] * interpolation1);
            const float scale2 = (ResamplerFilter[filt_ind2] * interpolation1) + (ResamplerFilter[filt_ind2 + RESAMPLER_ZERO_CROSSINGS] * interpolation2);

            filter[j] = scale1;
            filter[j + RESAMPLER_ZERO_CROSSINGS] = scale2;
        }

        const float* src = &inbuf[(srcindex - (RESAMPLER_ZERO_CROSSINGS - 1)) * chans];
        ResampleFrame(src, dst, filter, chans);
        dst += chans;
    }

    *resample_offset = srcpos - ((Sint64)inframes << 32);
}

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
 * - SR=surround right speaker
 * - SL=surround left speaker
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

#ifdef SDL_SSE3_INTRINSICS
// Convert from stereo to mono. Average left and right.
static void SDL_TARGETING("sse3") SDL_ConvertStereoToMono_SSE3(float *dst, const float *src, int num_frames)
{
    LOG_DEBUG_AUDIO_CONVERT("stereo", "mono (using SSE3)");

    const __m128 divby2 = _mm_set1_ps(0.5f);
    int i = num_frames;

    /* Do SSE blocks as long as we have 16 bytes available.
       Just use unaligned load/stores, if the memory at runtime is
       aligned it'll be just as fast on modern processors */
    while (i >= 4) {  // 4 * float32
        _mm_storeu_ps(dst, _mm_mul_ps(_mm_hadd_ps(_mm_loadu_ps(src), _mm_loadu_ps(src + 4)), divby2));
        i -= 4;
        src += 8;
        dst += 4;
    }

    // Finish off any leftovers with scalar operations.
    while (i) {
        *dst = (src[0] + src[1]) * 0.5f;
        dst++;
        i--;
        src += 2;
    }
}
#endif

#ifdef SDL_SSE_INTRINSICS
// Convert from mono to stereo. Duplicate to stereo left and right.
static void SDL_TARGETING("sse") SDL_ConvertMonoToStereo_SSE(float *dst, const float *src, int num_frames)
{
    LOG_DEBUG_AUDIO_CONVERT("mono", "stereo (using SSE)");

    // convert backwards, since output is growing in-place.
    src += (num_frames-4) * 1;
    dst += (num_frames-4) * 2;

    /* Do SSE blocks as long as we have 16 bytes available.
       Just use unaligned load/stores, if the memory at runtime is
       aligned it'll be just as fast on modern processors */
    // convert backwards, since output is growing in-place.
    int i = num_frames;
    while (i >= 4) {                                           // 4 * float32
        const __m128 input = _mm_loadu_ps(src);                // A B C D
        _mm_storeu_ps(dst, _mm_unpacklo_ps(input, input));     // A A B B
        _mm_storeu_ps(dst + 4, _mm_unpackhi_ps(input, input)); // C C D D
        i -= 4;
        src -= 4;
        dst -= 8;
    }

    // Finish off any leftovers with scalar operations.
    src += 3;
    dst += 6;   // adjust for smaller buffers.
    while (i) {  // convert backwards, since output is growing in-place.
        const float srcFC = src[0];
        dst[1] /* FR */ = srcFC;
        dst[0] /* FL */ = srcFC;
        i--;
        src--;
        dst -= 2;
    }
}
#endif

// Include the autogenerated channel converters...
#include "SDL_audio_channel_converters.h"


static void AudioConvertByteswap(void *dst, const void *src, int num_samples, int bitsize)
{
#if DEBUG_AUDIO_CONVERT
    SDL_Log("SDL_AUDIO_CONVERT: Converting %d-bit byte order", bitsize);
#endif

    switch (bitsize) {
#define CASESWAP(b) \
    case b: { \
        const Uint##b *tsrc = (const Uint##b *)src; \
        Uint##b *tdst = (Uint##b *)dst; \
        for (int i = 0; i < num_samples; i++) { \
            tdst[i] = SDL_Swap##b(tsrc[i]); \
        } \
        break; \
    }

        CASESWAP(16);
        CASESWAP(32);

#undef CASESWAP

    default:
        SDL_assert(!"unhandled byteswap datatype!");
        break;
    }
}

static void AudioConvertToFloat(float *dst, const void *src, int num_samples, SDL_AudioFormat src_fmt)
{
    // Endian conversion is handled separately
    switch (src_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
        case SDL_AUDIO_S8: SDL_Convert_S8_to_F32(dst, (const Sint8 *) src, num_samples); break;
        case SDL_AUDIO_U8: SDL_Convert_U8_to_F32(dst, (const Uint8 *) src, num_samples); break;
        case SDL_AUDIO_S16: SDL_Convert_S16_to_F32(dst, (const Sint16 *) src, num_samples); break;
        case SDL_AUDIO_S32: SDL_Convert_S32_to_F32(dst, (const Sint32 *) src, num_samples); break;
        case SDL_AUDIO_F32: if (dst != src) { SDL_memcpy(dst, src, num_samples * sizeof (float)); } break;  // oh well, just pass it through.
        default: SDL_assert(!"Unexpected audio format!"); break;
    }
}

static void AudioConvertFromFloat(void *dst, const float *src, int num_samples, SDL_AudioFormat dst_fmt)
{
    // Endian conversion is handled separately
    switch (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
        case SDL_AUDIO_S8: SDL_Convert_F32_to_S8((Sint8 *) dst, src, num_samples); break;
        case SDL_AUDIO_U8: SDL_Convert_F32_to_U8((Uint8 *) dst, src, num_samples); break;
        case SDL_AUDIO_S16: SDL_Convert_F32_to_S16((Sint16 *) dst, src, num_samples); break;
        case SDL_AUDIO_S32: SDL_Convert_F32_to_S32((Sint32 *) dst, src, num_samples); break;
        case SDL_AUDIO_F32: if (dst != src) { SDL_memcpy(dst, src, num_samples * sizeof (float)); } break;  // oh well, just pass it through.
        default: SDL_assert(!"Unexpected audio format!"); break;
    }
}

static SDL_bool SDL_IsSupportedAudioFormat(const SDL_AudioFormat fmt)
{
    switch (fmt) {
    case SDL_AUDIO_U8:
    case SDL_AUDIO_S8:
    case SDL_AUDIO_S16LSB:
    case SDL_AUDIO_S16MSB:
    case SDL_AUDIO_S32LSB:
    case SDL_AUDIO_S32MSB:
    case SDL_AUDIO_F32LSB:
    case SDL_AUDIO_F32MSB:
        return SDL_TRUE;  // supported.

    default:
        break;
    }

    return SDL_FALSE;  // unsupported.
}

static SDL_bool SDL_IsSupportedChannelCount(const int channels)
{
    return ((channels >= 1) && (channels <= 8)) ? SDL_TRUE : SDL_FALSE;
}


/* this does type and channel conversions _but not resampling_ (resampling
   happens in SDL_AudioStream). This expects data to be aligned/padded for
   SIMD access. This does not check parameter validity,
   (beyond asserts), it expects you did that already! */
/* all of this has to function as if src==dst (conversion in-place), but as a convenience
   if you're just going to copy the final output elsewhere, you can specify a different
   output pointer. */
static void ConvertAudio(int num_frames, const void *src, SDL_AudioFormat src_format, int src_channels,
                         void *dst, SDL_AudioFormat dst_format, int dst_channels)
{
    SDL_assert(src != NULL);
    SDL_assert(dst != NULL);
    SDL_assert(SDL_IsSupportedAudioFormat(src_format));
    SDL_assert(SDL_IsSupportedAudioFormat(dst_format));
    SDL_assert(SDL_IsSupportedChannelCount(src_channels));
    SDL_assert(SDL_IsSupportedChannelCount(dst_channels));

    if (!num_frames) {
        return;  // no data to convert, quit.
    }

#if DEBUG_AUDIO_CONVERT
    SDL_Log("SDL_AUDIO_CONVERT: Convert format %04x->%04x, channels %u->%u", src_format, dst_format, src_channels, dst_channels);
#endif

    const int dst_bitsize = (int) SDL_AUDIO_BITSIZE(dst_format);
    const int src_bitsize = (int) SDL_AUDIO_BITSIZE(src_format);

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - change channel count if necessary.
        - convert to final data format.
        - byteswap back to foreign format if necessary.

       The expectation is we can process data faster in float32
       (possibly with SIMD), and making several passes over the same
       buffer is likely to be CPU cache-friendly, avoiding the
       biggest performance hit in modern times. Previously we had
       (script-generated) custom converters for every data type and
       it was a bloat on SDL compile times and final library size. */

    // see if we can skip float conversion entirely.
    if (src_channels == dst_channels) {
        if (src_format == dst_format) {
            // nothing to do, we're already in the right format, just copy it over if necessary.
            if (src != dst) {
                SDL_memcpy(dst, src, num_frames * src_channels * (dst_bitsize / 8));
            }
            return;
        }

        // just a byteswap needed?
        if ((src_format & ~SDL_AUDIO_MASK_ENDIAN) == (dst_format & ~SDL_AUDIO_MASK_ENDIAN)) {
            if (src_bitsize == 8) {
                if (src != dst) {
                    SDL_memcpy(dst, src, num_frames * src_channels * (dst_bitsize / 8));
                }
                return;  // nothing to do, it's a 1-byte format.
            }
            AudioConvertByteswap(dst, src, num_frames * src_channels, src_bitsize);
            return;  // all done.
        }
    }

    // make sure we're in native byte order.
    if ((SDL_AUDIO_ISBIGENDIAN(src_format) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && (src_bitsize > 8)) {
        AudioConvertByteswap(dst, src, num_frames * src_channels, src_bitsize);
        src = dst;  // we've written to dst, future work will convert in-place.
    }

    // get us to float format.
    if (!SDL_AUDIO_ISFLOAT(src_format)) {
        AudioConvertToFloat((float *) dst, src, num_frames * src_channels, src_format);
        src = dst;  // we've written to dst, future work will convert in-place.
    }

    // Channel conversion

    if (src_channels != dst_channels) {
        SDL_AudioChannelConverter channel_converter;
        SDL_AudioChannelConverter override = NULL;

        // SDL_IsSupportedChannelCount should have caught these asserts, or we added a new format and forgot to update the table.
        SDL_assert(src_channels <= SDL_arraysize(channel_converters));
        SDL_assert(dst_channels <= SDL_arraysize(channel_converters[0]));

        channel_converter = channel_converters[src_channels - 1][dst_channels - 1];
        SDL_assert(channel_converter != NULL);

        // swap in some SIMD versions for a few of these.
        if (channel_converter == SDL_ConvertStereoToMono) {
            #ifdef SDL_SSE3_INTRINSICS
            if (!override && SDL_HasSSE3()) { override = SDL_ConvertStereoToMono_SSE3; }
            #endif
        } else if (channel_converter == SDL_ConvertMonoToStereo) {
            #ifdef SDL_SSE_INTRINSICS
            if (!override && SDL_HasSSE()) { override = SDL_ConvertMonoToStereo_SSE; }
            #endif
        }

        if (override) {
            channel_converter = override;
        }
        channel_converter((float *) dst, (float *) src, num_frames);
        src = dst;  // we've written to dst, future work will convert in-place.
    }

    // Resampling is not done in here. SDL_AudioStream handles that.

    // Move to final data type.
    if (!SDL_AUDIO_ISFLOAT(dst_format)) {
        AudioConvertFromFloat(dst, (float *) src, num_frames * dst_channels, dst_format);
        src = dst;  // we've written to dst, future work will convert in-place.
    }

    // make sure we're in final byte order.
    if ((SDL_AUDIO_ISBIGENDIAN(dst_format) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && (dst_bitsize > 8)) {
        AudioConvertByteswap(dst, src, num_frames * dst_channels, dst_bitsize);
        src = dst;  // we've written to dst, future work will convert in-place.
    }

    SDL_assert(src == dst);  // if we got here, we _had_ to have done _something_. Otherwise, we should have memcpy'd!
}

// figure out the largest thing we might need for ConvertAudio, which might grow data in-place.
static int CalculateMaxSampleFrameSize(SDL_AudioFormat src_format, int src_channels, SDL_AudioFormat dst_format, int dst_channels)
{
    const int src_format_size = SDL_AUDIO_BITSIZE(src_format) / 8;
    const int dst_format_size = SDL_AUDIO_BITSIZE(dst_format) / 8;
    const int max_app_format_size = SDL_max(src_format_size, dst_format_size);
    const int max_format_size = SDL_max(max_app_format_size, sizeof (float));  // ConvertAudio converts to float internally.
    const int max_channels = SDL_max(src_channels, dst_channels);
    return max_format_size * max_channels;
}

// this assumes you're holding the stream's lock (or are still creating the stream).
static int SetAudioStreamFormat(SDL_AudioStream *stream, const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec)
{
    /* If increasing channels, do it after resampling, since we'd just
       do more work to resample duplicate channels. If we're decreasing, do
       it first so we resample the interpolated data instead of interpolating
       the resampled data (!!! FIXME: decide if that works in practice, though!).
       This is decided in pre_resample_channels. */

    const SDL_AudioFormat src_format = src_spec->format;
    const int src_channels = src_spec->channels;
    const int src_rate = src_spec->freq;
    const SDL_AudioFormat dst_format = dst_spec->format;
    const int dst_channels = dst_spec->channels;
    const int dst_rate = dst_spec->freq;
    const int src_sample_frame_size = (SDL_AUDIO_BITSIZE(src_format) / 8) * src_channels;
    const int dst_sample_frame_size = (SDL_AUDIO_BITSIZE(dst_format) / 8) * dst_channels;
    const int max_sample_frame_size = CalculateMaxSampleFrameSize(src_format, src_channels, dst_format, dst_channels);
    const int prev_history_buffer_frames = stream->history_buffer_frames;
    const int pre_resample_channels = SDL_min(src_channels, dst_channels);
    const Sint64 resample_rate = GetResampleRate(src_rate, dst_rate);
    const int resampler_padding_frames = GetResamplerPaddingFrames(resample_rate);
    const int history_buffer_frames = GetHistoryBufferSampleFrames(resampler_padding_frames);
    const size_t history_buffer_allocation = history_buffer_frames * max_sample_frame_size;
    Uint8 *history_buffer = stream->history_buffer;

    // do all the things that can fail upfront, so we can just return an error without changing the stream if anything goes wrong.

    // set up for (possibly new) conversions

    // grow the history buffer if necessary; often times this won't be, as it already buffers more than immediately necessary in case of a dramatic downsample.
    if (stream->history_buffer_allocation < history_buffer_allocation) {
        history_buffer = (Uint8 *) SDL_aligned_alloc(SDL_SIMDGetAlignment(), history_buffer_allocation);
        if (!history_buffer) {
            return SDL_OutOfMemory();
        }
    }

    // okay, we've done all the things that can fail, now we can change stream state.

    if (stream->history_buffer) {
        if (history_buffer_frames <= prev_history_buffer_frames) {
            ConvertAudio(history_buffer_frames, stream->history_buffer, stream->src_spec.format, stream->src_spec.channels, history_buffer, src_format, src_channels);
        } else {
            ConvertAudio(prev_history_buffer_frames, stream->history_buffer, stream->src_spec.format, stream->src_spec.channels, history_buffer + ((history_buffer_frames - prev_history_buffer_frames) * src_sample_frame_size), src_format, src_channels);
            SDL_memset(history_buffer, SDL_GetSilenceValueForFormat(src_format), (history_buffer_frames - prev_history_buffer_frames) * src_sample_frame_size);  // silence oldest history samples.
        }
    } else if (history_buffer != NULL) {
        SDL_memset(history_buffer, SDL_GetSilenceValueForFormat(src_format), history_buffer_allocation);
    }

    if (history_buffer != stream->history_buffer) {
        SDL_aligned_free(stream->history_buffer);
        stream->history_buffer = history_buffer;
        stream->history_buffer_allocation = history_buffer_allocation;
    }

    stream->resampler_padding_frames = resampler_padding_frames;
    stream->history_buffer_frames = history_buffer_frames;
    stream->max_sample_frame_size = max_sample_frame_size;
    stream->src_sample_frame_size = src_sample_frame_size;
    stream->dst_sample_frame_size = dst_sample_frame_size;
    stream->pre_resample_channels = pre_resample_channels;
    stream->resample_rate = resample_rate;

    if (src_spec != &stream->src_spec) {
        SDL_memcpy(&stream->src_spec, src_spec, sizeof (SDL_AudioSpec));
    }

    if (dst_spec != &stream->dst_spec) {
        SDL_memcpy(&stream->dst_spec, dst_spec, sizeof (SDL_AudioSpec));
    }

    return 0;
}

SDL_AudioStream *SDL_CreateAudioStream(const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec)
{
    // !!! FIXME: fail if audio isn't initialized

    if (!src_spec) {
        SDL_InvalidParamError("src_spec");
        return NULL;
    } else if (!dst_spec) {
        SDL_InvalidParamError("dst_spec");
        return NULL;
    } else if (!SDL_IsSupportedChannelCount(src_spec->channels)) {
        SDL_InvalidParamError("src_spec->channels");
        return NULL;
    } else if (!SDL_IsSupportedChannelCount(dst_spec->channels)) {
        SDL_InvalidParamError("dst_spec->channels");
        return NULL;
    } else if (src_spec->freq <= 0) {
        SDL_InvalidParamError("src_spec->freq");
        return NULL;
    } else if (dst_spec->freq <= 0) {
        SDL_InvalidParamError("dst_spec->freq");
        return NULL;
    } else if (src_spec->freq >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
        SDL_SetError("Source rate is too high");
        return NULL;
    } else if (dst_spec->freq >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
        SDL_SetError("Destination rate is too high");
        return NULL;
    } else if (!SDL_IsSupportedAudioFormat(src_spec->format)) {
        SDL_InvalidParamError("src_spec->format");
        return NULL;
    } else if (!SDL_IsSupportedAudioFormat(dst_spec->format)) {
        SDL_InvalidParamError("dst_spec->format");
        return NULL;
    }

    SDL_AudioStream *retval = (SDL_AudioStream *)SDL_calloc(1, sizeof(SDL_AudioStream));
    if (retval == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    const int packetlen = 4096; // !!! FIXME: good enough for now.
    retval->queue = SDL_CreateDataQueue(packetlen, (size_t)packetlen * 2);
    if (!retval->queue) {
        SDL_DestroyAudioStream(retval);
        return NULL; // SDL_CreateDataQueue should have called SDL_SetError.
    }

    retval->lock = SDL_GetDataQueueMutex(retval->queue);
    SDL_assert(retval->lock != NULL);

    // Make sure we've chosen audio conversion functions (SIMD, scalar, etc.)
    SDL_ChooseAudioConverters();  // !!! FIXME: let's do this during SDL_Init

    retval->packetlen = packetlen;
    SDL_memcpy(&retval->src_spec, src_spec, sizeof (SDL_AudioSpec));

    if (SetAudioStreamFormat(retval, src_spec, dst_spec) == -1) {
        SDL_DestroyAudioStream(retval);
        return NULL;
    }

    return retval;
}

int SDL_SetAudioStreamGetCallback(SDL_AudioStream *stream, SDL_AudioStreamRequestCallback callback, void *userdata)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    SDL_LockMutex(stream->lock);
    stream->get_callback = callback;
    stream->get_callback_userdata = userdata;
    SDL_UnlockMutex(stream->lock);
    return 0;
}

int SDL_SetAudioStreamPutCallback(SDL_AudioStream *stream, SDL_AudioStreamRequestCallback callback, void *userdata)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    SDL_LockMutex(stream->lock);
    stream->put_callback = callback;
    stream->put_callback_userdata = userdata;
    SDL_UnlockMutex(stream->lock);
    return 0;
}

int SDL_LockAudioStream(SDL_AudioStream *stream)
{
    return stream ? SDL_LockMutex(stream->lock) : SDL_InvalidParamError("stream");
}

int SDL_UnlockAudioStream(SDL_AudioStream *stream)
{
    return stream ? SDL_UnlockMutex(stream->lock) : SDL_InvalidParamError("stream");
}

int SDL_GetAudioStreamFormat(SDL_AudioStream *stream, SDL_AudioSpec *src_spec, SDL_AudioSpec *dst_spec)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    SDL_LockMutex(stream->lock);
    if (src_spec) {
        SDL_memcpy(src_spec, &stream->src_spec, sizeof (SDL_AudioSpec));
    }
    if (dst_spec) {
        SDL_memcpy(dst_spec, &stream->dst_spec, sizeof (SDL_AudioSpec));
    }
    SDL_UnlockMutex(stream->lock);
    return 0;
}

int SDL_SetAudioStreamFormat(SDL_AudioStream *stream, const SDL_AudioSpec *src_spec, const SDL_AudioSpec *dst_spec)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }

    if (src_spec) {
        if (!SDL_IsSupportedAudioFormat(src_spec->format)) {
            return SDL_InvalidParamError("src_spec->format");
        } else if (!SDL_IsSupportedChannelCount(src_spec->channels)) {
            return SDL_InvalidParamError("src_spec->channels");
        } else if (src_spec->freq <= 0) {
            return SDL_InvalidParamError("src_spec->freq");
        } else if (src_spec->freq >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
            return SDL_SetError("Source rate is too high");
        }
    }

    if (dst_spec) {
        if (!SDL_IsSupportedAudioFormat(dst_spec->format)) {
            return SDL_InvalidParamError("dst_spec->format");
        } else if (!SDL_IsSupportedChannelCount(dst_spec->channels)) {
            return SDL_InvalidParamError("dst_spec->channels");
        } else if (dst_spec->freq <= 0) {
            return SDL_InvalidParamError("dst_spec->freq");
        } else if (dst_spec->freq >= SDL_MAX_SINT32 / RESAMPLER_SAMPLES_PER_ZERO_CROSSING) {
            return SDL_SetError("Destination rate is too high");
        }
    }

    SDL_LockMutex(stream->lock);
    const int retval = SetAudioStreamFormat(stream, src_spec ? src_spec : &stream->src_spec, dst_spec ? dst_spec : &stream->dst_spec);
    SDL_UnlockMutex(stream->lock);

    return retval;
}

int SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len)
{
#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: wants to put %d preconverted bytes", len);
#endif

    if (stream == NULL) {
        return SDL_InvalidParamError("stream");
    } else if (buf == NULL) {
        return SDL_InvalidParamError("buf");
    } else if (len == 0) {
        return 0; // nothing to do.
    }

    SDL_LockMutex(stream->lock);

    const int prev_available = stream->put_callback ? SDL_GetAudioStreamAvailable(stream) : 0;

    if ((len % stream->src_sample_frame_size) != 0) {
        SDL_UnlockMutex(stream->lock);
        return SDL_SetError("Can't add partial sample frames");
    }

    // just queue the data, we convert/resample when dequeueing.
    const int retval = SDL_WriteToDataQueue(stream->queue, buf, len);
    stream->flushed = SDL_FALSE;

    if (stream->put_callback) {
        const int newavail = SDL_GetAudioStreamAvailable(stream) - prev_available;
        if (newavail > 0) {   // don't call the callback if we can't actually offer new data (still filling future buffer, only added 1 frame but downsampling needs more to produce new sound, etc).
            stream->put_callback(stream, newavail, stream->put_callback_userdata);
        }
    }

    SDL_UnlockMutex(stream->lock);

    return retval;
}


int SDL_FlushAudioStream(SDL_AudioStream *stream)
{
    if (stream == NULL) {
        return SDL_InvalidParamError("stream");
    }

    SDL_LockMutex(stream->lock);
    stream->flushed = SDL_TRUE;
    SDL_UnlockMutex(stream->lock);

    return 0;
}

/* this does not save the previous contents of stream->work_buffer. It's a work buffer!!
   The returned buffer is aligned/padded for use with SIMD instructions. */
static Uint8 *EnsureStreamWorkBufferSize(SDL_AudioStream *stream, size_t newlen)
{
    if (stream->work_buffer_allocation >= newlen) {
        return stream->work_buffer;
    }

    Uint8 *ptr = (Uint8 *) SDL_aligned_alloc(SDL_SIMDGetAlignment(), newlen);
    if (ptr == NULL) {
        SDL_OutOfMemory();
        return NULL;  // previous work buffer is still valid!
    }

    SDL_aligned_free(stream->work_buffer);
    stream->work_buffer = ptr;
    stream->work_buffer_allocation = newlen;
    return ptr;
}

static int CalculateAudioStreamWorkBufSize(const SDL_AudioStream *stream, int input_frames, int output_frames)
{
    int workbuflen = SDL_max(input_frames, output_frames) * stream->max_sample_frame_size;

    if (stream->resample_rate) {
        int resample_frame_size = stream->pre_resample_channels * sizeof(float);

        // Calculate space needed to move to format/channels used for resampling stage.
        int inputlen = (input_frames + (stream->resampler_padding_frames * 2)) * resample_frame_size;

        workbuflen = SDL_max(workbuflen, inputlen);

        // Calculate space needed after resample (which lives in a second copy in the same buffer).
        workbuflen += output_frames * resample_frame_size;
    }

    return workbuflen;
}

// You must hold stream->lock and validate your parameters before calling this!
static int GetAudioStreamDataInternal(SDL_AudioStream *stream, void *buf, int len)
{
    const SDL_AudioFormat src_format = stream->src_spec.format;
    const int src_channels = stream->src_spec.channels;
    const int src_sample_frame_size = stream->src_sample_frame_size;

    const SDL_AudioFormat dst_format = stream->dst_spec.format;
    const int dst_channels = stream->dst_spec.channels;
    const int dst_sample_frame_size = stream->dst_sample_frame_size;

    const int max_sample_frame_size = stream->max_sample_frame_size;
    const int pre_resample_channels = stream->pre_resample_channels;

    const int resampler_padding_frames = stream->resampler_padding_frames;
    const Sint64 resample_rate = stream->resample_rate;

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: asking for an output chunk of %d bytes.", len);
#endif

    // Clamp the output length to the maximum currently available.
    // The rest of this function assumes enough input data is available.
    const int max_available = SDL_GetAudioStreamAvailable(stream);

    if (len > max_available) {
        len = max_available;
    }

    int output_frames = len / dst_sample_frame_size;

    if (output_frames == 0) {
        return 0;  // nothing to do.
    }

    int input_frames = output_frames;

    if (resample_rate) {
        // Calculate the number of input frames necessary for this request.
        // Because resampling happens "between" frames, The same number of output_frames
        // can require a different number of input_frames, depending on the resample_offset.
        input_frames = GetResamplerNeededInputFrames(output_frames, resample_rate, stream->resample_offset);
    }

    // !!! FIXME: this could be less aggressive about allocation, if we decide the necessary size at each stage and select the maximum required.
    int work_buffer_capacity = CalculateAudioStreamWorkBufSize(stream, input_frames, output_frames);
    Uint8* work_buffer = EnsureStreamWorkBufferSize(stream, work_buffer_capacity);

    if (!work_buffer) {
        return -1;
    }

    int input_bytes = input_frames * src_sample_frame_size;
    int padding_bytes = resampler_padding_frames * src_sample_frame_size;

    Uint8* work_buffer_tail = work_buffer;

    // Split the work_buffer into [left_padding][input_buffer][right_padding]
    Uint8* left_padding = work_buffer_tail;
    work_buffer_tail += padding_bytes;

    Uint8* input_buffer = work_buffer_tail;
    work_buffer_tail += input_bytes;

    Uint8* right_padding = work_buffer_tail;
    work_buffer_tail += padding_bytes;

    SDL_assert((work_buffer_tail - work_buffer) <= work_buffer_capacity);

    // Now read unconverted data from the queue into the work buffer to fulfill the request.
    if (input_frames > 0) {
        int bytes_read = (int) SDL_ReadFromDataQueue(stream->queue, input_buffer, input_bytes);
        SDL_assert(bytes_read == input_bytes);
    }

    Uint8 *history_buffer = stream->history_buffer;
    const int history_buffer_frames = stream->history_buffer_frames;
    int history_bytes = history_buffer_frames * src_sample_frame_size;

    // Do we need to fill in the left/right padding?
    if (resampler_padding_frames > 0) {
        // Fill in the left padding using the history buffer
        SDL_assert(padding_bytes <= history_bytes);
        SDL_memcpy(left_padding, history_buffer + history_bytes - padding_bytes, padding_bytes);

        // Fill in the right padding by peeking into the input queue
        int right_padding_bytes = (int) SDL_PeekIntoDataQueue(stream->queue, right_padding, padding_bytes);

        if (right_padding_bytes < padding_bytes) {
            // If we have run out of data, fill the rest with silence.
            // This should only happen if the stream has been flushed.
            SDL_assert(stream->flushed);
            SDL_memset(right_padding + right_padding_bytes, SDL_GetSilenceValueForFormat(src_format), padding_bytes - right_padding_bytes);
        }
    }
    
    // Update the history buffer using the new input data
    if (input_bytes >= history_bytes) {
        SDL_memcpy(history_buffer, input_buffer + (input_bytes - history_bytes), history_bytes);
    } else {
        int preserve_bytes = history_bytes - input_bytes;
        SDL_memmove(history_buffer, history_buffer + input_bytes, preserve_bytes);
        SDL_memcpy(history_buffer + preserve_bytes, input_buffer, input_bytes);
    }

    int output_bytes = output_frames * dst_sample_frame_size;

    // Not resampling? It's an easy conversion (and maybe not even that!)
    if (resample_rate == 0) {
        SDL_assert(input_frames == output_frames);
        SDL_assert(input_buffer == work_buffer);

        // see if we can do the conversion in-place (will fit in `buf` while in-progress), or if we need to do it in the workbuf and copy it over
        if (max_sample_frame_size <= dst_sample_frame_size) {
            ConvertAudio(input_frames, input_buffer, src_format, src_channels, buf, dst_format, dst_channels);
        } else {
            ConvertAudio(input_frames, input_buffer, src_format, src_channels, input_buffer, dst_format, dst_channels);
            SDL_memcpy(buf, input_buffer, output_bytes);
        }

        return output_bytes;
    }

    int work_buffer_frames = (work_buffer_tail - work_buffer) / src_sample_frame_size;
    SDL_assert(work_buffer_frames == input_frames + (resampler_padding_frames * 2));

    // Resampling! get the work buffer to float32 format, etc, in-place.
    ConvertAudio(work_buffer_frames, work_buffer, src_format, src_channels, work_buffer, SDL_AUDIO_F32SYS, pre_resample_channels);

    const int resample_frame_size = pre_resample_channels * sizeof(float);
    const int resample_bytes = output_frames * resample_frame_size;

    // Update the work_buffer pointers based on the new frame size
    input_buffer = work_buffer + ((input_buffer - work_buffer) / src_sample_frame_size * resample_frame_size);
    work_buffer_tail = work_buffer + ((work_buffer_tail - work_buffer) / src_sample_frame_size * resample_frame_size);

    float* resample_buffer;

    // Check if we can resample directly into the output buffer
    if ((dst_format == SDL_AUDIO_F32SYS) && (dst_channels == pre_resample_channels)) {
        resample_buffer = (float *) buf;
    } else {
        resample_buffer = (float *) work_buffer_tail; // do at the end of the buffer so we have room for final convert at front.
        work_buffer_tail += resample_bytes;
    }

    SDL_assert((work_buffer_tail - work_buffer) <= work_buffer_capacity);

    ResampleAudio(pre_resample_channels,
                  (const float *) input_buffer, input_frames,
                  resample_buffer, output_frames,
                  resample_rate, &stream->resample_offset);

    // Get us to the final format!
    // see if we can do the conversion in-place (will fit in `buf` while in-progress), or if we need to do it in the workbuf and copy it over
    if (max_sample_frame_size <= dst_sample_frame_size) {
        ConvertAudio(output_frames, resample_buffer, SDL_AUDIO_F32SYS, pre_resample_channels, buf, dst_format, dst_channels);
    } else {
        ConvertAudio(output_frames, resample_buffer, SDL_AUDIO_F32SYS, pre_resample_channels, work_buffer, dst_format, dst_channels);
        SDL_memcpy(buf, work_buffer, output_bytes);
    }

    return output_bytes;
}

// get converted/resampled data from the stream
int SDL_GetAudioStreamData(SDL_AudioStream *stream, void *voidbuf, int len)
{
    Uint8 *buf = (Uint8 *) voidbuf;

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: want to get %d converted bytes", len);
#endif

    if (stream == NULL) {
        return SDL_InvalidParamError("stream");
    } else if (buf == NULL) {
        return SDL_InvalidParamError("buf");
    } else if (len < 0) {
        return SDL_InvalidParamError("len");
    } else if (len == 0) {
        return 0; // nothing to do.
    }

    SDL_LockMutex(stream->lock);

    len -= len % stream->dst_sample_frame_size;  // chop off any fractional sample frame.

    // give the callback a chance to fill in more stream data if it wants.
    if (stream->get_callback) {
        int approx_request = len / stream->dst_sample_frame_size;  // start with sample frames desired
        if (stream->resample_rate) {
            approx_request = GetResamplerNeededInputFrames(approx_request, stream->resample_rate, stream->resample_offset);

            if (!stream->flushed) {  // do we need to fill the future buffer to accommodate this, too?
                approx_request += stream->resampler_padding_frames;
            }
        }

        approx_request *= stream->src_sample_frame_size;  // convert sample frames to bytes.
        const int already_have = SDL_GetAudioStreamAvailable(stream);
        approx_request -= SDL_min(approx_request, already_have);  // we definitely have this much output already packed in.
        if (approx_request > 0) {  // don't call the callback if we can satisfy this request with existing data.
            stream->get_callback(stream, approx_request, stream->get_callback_userdata);
        }
    }

    int retval = 0;
    while (len > 0) { // didn't ask for a whole sample frame, nothing to do
        const int chunk_size = 32 * 1024;
        const int rc = GetAudioStreamDataInternal(stream, buf, SDL_min(len, chunk_size));

        if (rc == -1) {
            #if DEBUG_AUDIOSTREAM
            SDL_Log("AUDIOSTREAM: output chunk ended up producing an error!");
            #endif
            if (retval == 0) {
                retval = -1;
            }
            break;
        } else {
            #if DEBUG_AUDIOSTREAM
            SDL_Log("AUDIOSTREAM: output chunk ended up being %d bytes.", rc);
            #endif
            buf += rc;
            len -= rc;
            retval += rc;
            if (rc < chunk_size) {
                break;
            }
        }
    }

    SDL_UnlockMutex(stream->lock);

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: Final result was %d", retval);
#endif

    return retval;
}

// number of converted/resampled bytes available
int SDL_GetAudioStreamAvailable(SDL_AudioStream *stream)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }

    SDL_LockMutex(stream->lock);

    // total bytes available in source format in data queue
    size_t count = SDL_GetDataQueueSize(stream->queue);

    // total sample frames available in data queue
    count /= stream->src_sample_frame_size;

    // sample frames after resampling
    if (stream->resample_rate) {
        if (!stream->flushed) {
            // have to save some samples for padding. They aren't available until more data is added or the stream is flushed.
            count = (count < ((size_t) stream->resampler_padding_frames)) ? 0 : (count - stream->resampler_padding_frames);
        }

        count = GetResamplerAvailableOutputFrames(count, stream->resample_rate, stream->resample_offset);
    }

    // convert from sample frames to bytes in destination format.
    count *= stream->dst_sample_frame_size;

    SDL_UnlockMutex(stream->lock);

    // if this overflows an int, just clamp it to a maximum.
    const int max_int = 0x7FFFFFFF;  // !!! FIXME: This will blow up on weird processors. Is there an SDL_INT_MAX?
    return (count >= ((size_t) max_int)) ? max_int : ((int) count);
}

int SDL_ClearAudioStream(SDL_AudioStream *stream)
{
    if (stream == NULL) {
        return SDL_InvalidParamError("stream");
    }

    SDL_LockMutex(stream->lock);
    SDL_ClearDataQueue(stream->queue, (size_t)stream->packetlen * 2);
    if (stream->history_buffer != NULL) {
        SDL_memset(stream->history_buffer, SDL_GetSilenceValueForFormat(stream->src_spec.format), stream->history_buffer_allocation);
    }
    stream->resample_offset = 0;
    stream->flushed = SDL_FALSE;
    SDL_UnlockMutex(stream->lock);
    return 0;
}

void SDL_DestroyAudioStream(SDL_AudioStream *stream)
{
    if (stream) {
        SDL_UnbindAudioStream(stream);
        // do not destroy stream->lock! it's a copy of `stream->queue`'s mutex, so destroying the queue will handle it.
        SDL_DestroyDataQueue(stream->queue);
        SDL_aligned_free(stream->work_buffer);
        SDL_aligned_free(stream->history_buffer);
        SDL_free(stream);
    }
}

int SDL_ConvertAudioSamples(const SDL_AudioSpec *src_spec, const Uint8 *src_data, int src_len,
                            const SDL_AudioSpec *dst_spec, Uint8 **dst_data, int *dst_len)
{
    if (dst_data) {
        *dst_data = NULL;
    }

    if (dst_len) {
        *dst_len = 0;
    }

    if (src_data == NULL) {
        return SDL_InvalidParamError("src_data");
    } else if (src_len < 0) {
        return SDL_InvalidParamError("src_len");
    } else if (dst_data == NULL) {
        return SDL_InvalidParamError("dst_data");
    } else if (dst_len == NULL) {
        return SDL_InvalidParamError("dst_len");
    }

    int retval = -1;
    Uint8 *dst = NULL;
    int dstlen = 0;

    SDL_AudioStream *stream = SDL_CreateAudioStream(src_spec, dst_spec);
    if (stream != NULL) {
        if ((SDL_PutAudioStreamData(stream, src_data, src_len) == 0) && (SDL_FlushAudioStream(stream) == 0)) {
            dstlen = SDL_GetAudioStreamAvailable(stream);
            if (dstlen >= 0) {
                dst = (Uint8 *)SDL_malloc(dstlen);
                if (!dst) {
                    SDL_OutOfMemory();
                } else {
                    retval = (SDL_GetAudioStreamData(stream, dst, dstlen) >= 0) ? 0 : -1;
                }
            }
        }
    }

    if (retval == -1) {
        SDL_free(dst);
    } else {
        *dst_data = dst;
        *dst_len = dstlen;
    }

    SDL_DestroyAudioStream(stream);
    return retval;
}
