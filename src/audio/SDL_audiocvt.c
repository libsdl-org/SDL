/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_audio.h"
#include "SDL_audio_c.h"

#include "SDL_assert.h"


/* Effectively mix right and left channels into a single channel */
static void SDLCALL
SDL_ConvertMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("stereo", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i, src += 2) {
        *(dst++) = (float) ((((double) src[0]) + ((double) src[1])) * 0.5);
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Discard top 4 channels */
static void SDLCALL
SDL_ConvertStrip(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6 channels", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 2) {
        dst[0] = src[0];
        dst[1] = src[1];
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Discard top 2 channels of 6 */
static void SDLCALL
SDL_ConvertStrip_2(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("6 channels", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 4) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
    }

    cvt->len_cvt /= 6;
    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

/* Duplicate a mono channel to both stereo channels */
static void SDLCALL
SDL_ConvertStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 2);
    int i;

    LOG_DEBUG_CONVERT("mono", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / sizeof (float); i; --i) {
        src--;
        dst -= 2;
        dst[0] = dst[1] = *src;
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Duplicate a stereo channel to a pseudo-5.1 stream */
static void SDLCALL
SDL_ConvertSurround(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    int i;
    float lf, rf, ce;
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 3);

    LOG_DEBUG_CONVERT("stereo", "5.1");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i) {
        dst -= 6;
        src -= 2;
        lf = src[0];
        rf = src[1];
        ce = (lf * 0.5f) + (rf * 0.5f);
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = lf - ce;
        dst[3] = rf - ce;
        dst[4] = dst[5] = ce;
    }

    cvt->len_cvt *= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Duplicate a stereo channel to a pseudo-4.0 stream */
static void SDLCALL
SDL_ConvertSurround_4(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 2);
    float lf, rf, ce;
    int i;

    LOG_DEBUG_CONVERT("stereo", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i) {
        dst -= 4;
        src -= 2;
        lf = src[0];
        rf = src[1];
        ce = (lf / 2) + (rf / 2);
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = lf - ce;
        dst[3] = rf - ce;
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
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
SDL_BuildAudioTypeCVTToFloat(SDL_AudioCVT *cvt, const SDL_AudioFormat src_fmt)
{
    int retval = 0;  /* 0 == no conversion necessary. */

    if ((SDL_AUDIO_ISBIGENDIAN(src_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN)) {
        cvt->filters[cvt->filter_index++] = SDL_Convert_Byteswap;
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
            return SDL_SetError("No conversion available for these formats");
        }

        cvt->filters[cvt->filter_index++] = filter;
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
            return SDL_SetError("No conversion available for these formats");
        }

        cvt->filters[cvt->filter_index++] = filter;
        if (src_bitsize < dst_bitsize) {
            const int mult = (dst_bitsize / src_bitsize);
            cvt->len_mult *= mult;
            cvt->len_ratio *= mult;
        } else if (src_bitsize > dst_bitsize) {
            cvt->len_ratio /= (src_bitsize / dst_bitsize);
        }
        retval = 1;  /* added a converter. */
    }

    if ((SDL_AUDIO_ISBIGENDIAN(dst_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN)) {
        cvt->filters[cvt->filter_index++] = SDL_Convert_Byteswap;
        retval = 1;  /* added a converter. */
    }

    return retval;
}

static int
SDL_FindFrequencyMultiple(const int src_rate, const int dst_rate)
{
    int retval = 0;

    /* If we only built with the arbitrary resamplers, ignore multiples. */
    int lo, hi;
    int div;

    SDL_assert(src_rate != 0);
    SDL_assert(dst_rate != 0);
    SDL_assert(src_rate != dst_rate);

    if (src_rate < dst_rate) {
        lo = src_rate;
        hi = dst_rate;
    } else {
        lo = dst_rate;
        hi = src_rate;
    }

    /* zero means "not a supported multiple" ... we only do 2x and 4x. */
    if ((hi % lo) != 0)
        return 0;               /* not a multiple. */

    div = hi / lo;
    retval = ((div == 2) || (div == 4)) ? div : 0;

    return retval;
}

#define RESAMPLER_FUNCS(chans) \
    static void SDLCALL \
    SDL_Upsample_Arbitrary_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Upsample_Arbitrary(cvt, chans); \
    }\
    static void SDLCALL \
    SDL_Downsample_Arbitrary_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Downsample_Arbitrary(cvt, chans); \
    } \
    static void SDLCALL \
    SDL_Upsample_x2_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Upsample_x2(cvt, chans); \
    } \
    static void SDLCALL \
    SDL_Downsample_x2_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Downsample_Multiple(cvt, 2, chans); \
    } \
    static void SDLCALL \
    SDL_Upsample_x4_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Upsample_x4(cvt, chans); \
    } \
    static void SDLCALL \
    SDL_Downsample_x4_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_assert(format == AUDIO_F32SYS); \
        SDL_Downsample_Multiple(cvt, 4, chans); \
    }
RESAMPLER_FUNCS(1)
RESAMPLER_FUNCS(2)
RESAMPLER_FUNCS(4)
RESAMPLER_FUNCS(6)
RESAMPLER_FUNCS(8)
#undef RESAMPLER_FUNCS

static int
SDL_BuildAudioResampleCVT(SDL_AudioCVT * cvt, int dst_channels,
                          int src_rate, int dst_rate)
{
    if (src_rate != dst_rate) {
        const int upsample = (src_rate < dst_rate) ? 1 : 0;
        const int multiple = SDL_FindFrequencyMultiple(src_rate, dst_rate);
        SDL_AudioFilter filter = NULL;

        #define PICK_CHANNEL_FILTER(upordown, resampler) switch (dst_channels) { \
            case 1: filter = SDL_##upordown##_##resampler##_c1; break; \
            case 2: filter = SDL_##upordown##_##resampler##_c2; break; \
            case 4: filter = SDL_##upordown##_##resampler##_c4; break; \
            case 6: filter = SDL_##upordown##_##resampler##_c6; break; \
            case 8: filter = SDL_##upordown##_##resampler##_c8; break; \
            default: break; \
        }

        if (upsample) {
            if (multiple == 0) {
                PICK_CHANNEL_FILTER(Upsample, Arbitrary);
            } else if (multiple == 2) {
                PICK_CHANNEL_FILTER(Upsample, x2);
            } else if (multiple == 4) {
                PICK_CHANNEL_FILTER(Upsample, x4);
            }
        } else {
            if (multiple == 0) {
                PICK_CHANNEL_FILTER(Downsample, Arbitrary);
            } else if (multiple == 2) {
                PICK_CHANNEL_FILTER(Downsample, x2);
            } else if (multiple == 4) {
                PICK_CHANNEL_FILTER(Downsample, x4);
            }
        }

        #undef PICK_CHANNEL_FILTER

        if (filter == NULL) {
            return SDL_SetError("No conversion available for these rates");
        }

        /* Update (cvt) with filter details... */
        cvt->filters[cvt->filter_index++] = filter;
        if (src_rate < dst_rate) {
            const double mult = ((double) dst_rate) / ((double) src_rate);
            cvt->len_mult *= (int) SDL_ceil(mult);
            cvt->len_ratio *= mult;
        } else {
            cvt->len_ratio /= ((double) src_rate) / ((double) dst_rate);
        }

        return 1;               /* added a converter. */
    }

    return 0;                   /* no conversion necessary. */
}


/* Creates a set of audio filters to convert from one format to another.
   Returns -1 if the format conversion is not supported, 0 if there's
   no conversion needed, or 1 if the audio filter is set up.
*/

int
SDL_BuildAudioCVT(SDL_AudioCVT * cvt,
                  SDL_AudioFormat src_fmt, Uint8 src_channels, int src_rate,
                  SDL_AudioFormat dst_fmt, Uint8 dst_channels, int dst_rate)
{
    /* Sanity check target pointer */
    if (cvt == NULL) {
        return SDL_InvalidParamError("cvt");
    }

    /* there are no unsigned types over 16 bits, so catch this up front. */
    if ((SDL_AUDIO_BITSIZE(src_fmt) > 16) && (!SDL_AUDIO_ISSIGNED(src_fmt))) {
        return SDL_SetError("Invalid source format");
    }
    if ((SDL_AUDIO_BITSIZE(dst_fmt) > 16) && (!SDL_AUDIO_ISSIGNED(dst_fmt))) {
        return SDL_SetError("Invalid destination format");
    }

    /* prevent possible divisions by zero, etc. */
    if ((src_channels == 0) || (dst_channels == 0)) {
        return SDL_SetError("Source or destination channels is zero");
    }
    if ((src_rate == 0) || (dst_rate == 0)) {
        return SDL_SetError("Source or destination rate is zero");
    }
#if DEBUG_CONVERT
    printf("Build format %04x->%04x, channels %u->%u, rate %d->%d\n",
           src_fmt, dst_fmt, src_channels, dst_channels, src_rate, dst_rate);
#endif

    /* Start off with no conversion necessary */
    SDL_zerop(cvt);
    cvt->src_format = src_fmt;
    cvt->dst_format = dst_fmt;
    cvt->needed = 0;
    cvt->filter_index = 0;
    cvt->filters[0] = NULL;
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = ((double) dst_rate) / ((double) src_rate);

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - resample and change channel count if necessary.
        - convert back to native format.
        - byteswap back to foreign format if necessary.

       The expectation is we can process data faster in float32
       (possibly with SIMD), and making several passes over the same
       buffer in is likely to be CPU cache-friendly, avoiding the
       biggest performance hit in modern times. Previously we had
       (script-generated) custom converters for every data type and
       it was a bloat on SDL compile times and final library size. */

    /* see if we can skip float conversion entirely (just a byteswap needed). */
    if ((src_rate == dst_rate) && (src_channels == dst_channels) &&
        ((src_fmt != dst_fmt) &&
         ((src_fmt & ~SDL_AUDIO_MASK_ENDIAN) == (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN)))) {
        cvt->filters[cvt->filter_index++] = SDL_Convert_Byteswap;
        cvt->needed = 1;
        return 1;
    }

    /* Convert data types, if necessary. Updates (cvt). */
    if (SDL_BuildAudioTypeCVTToFloat(cvt, src_fmt) == -1) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    /* Channel conversion */
    if (src_channels != dst_channels) {
        if ((src_channels == 1) && (dst_channels > 1)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStereo;
            cvt->len_mult *= 2;
            src_channels = 2;
            cvt->len_ratio *= 2;
        }
        if ((src_channels == 2) && (dst_channels == 6)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertSurround;
            src_channels = 6;
            cvt->len_mult *= 3;
            cvt->len_ratio *= 3;
        }
        if ((src_channels == 2) && (dst_channels == 4)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertSurround_4;
            src_channels = 4;
            cvt->len_mult *= 2;
            cvt->len_ratio *= 2;
        }
        while ((src_channels * 2) <= dst_channels) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStereo;
            cvt->len_mult *= 2;
            src_channels *= 2;
            cvt->len_ratio *= 2;
        }
        if ((src_channels == 6) && (dst_channels <= 2)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStrip;
            src_channels = 2;
            cvt->len_ratio /= 3;
        }
        if ((src_channels == 6) && (dst_channels == 4)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStrip_2;
            src_channels = 4;
            cvt->len_ratio /= 2;
        }
        /* This assumes that 4 channel audio is in the format:
           Left {front/back} + Right {front/back}
           so converting to L/R stereo works properly.
         */
        while (((src_channels % 2) == 0) &&
               ((src_channels / 2) >= dst_channels)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertMono;
            src_channels /= 2;
            cvt->len_ratio /= 2;
        }
        if (src_channels != dst_channels) {
            /* Uh oh.. */ ;
        }
    }

    /* Do rate conversion, if necessary. Updates (cvt). */
    if (SDL_BuildAudioResampleCVT(cvt, dst_channels, src_rate, dst_rate) ==
        -1) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    if (SDL_BuildAudioTypeCVTFromFloat(cvt, dst_fmt) == -1) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    cvt->needed = (cvt->filter_index != 0);
    return (cvt->needed);
}

/* vi: set ts=4 sw=4 expandtab: */

