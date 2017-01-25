/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_loadso.h"
#include "SDL_assert.h"
#include "../SDL_dataqueue.h"
#include "SDL_cpuinfo.h"

#ifdef __SSE3__
#define HAVE_SSE3_INTRINSICS 1
#endif

#if HAVE_SSE3_INTRINSICS
/* Effectively mix right and left channels into a single channel */
static void SDLCALL
SDL_ConvertStereoToMono_SSE3(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i = cvt->len_cvt / 8;

    LOG_DEBUG_CONVERT("stereo", "mono (using SSE3)");
    SDL_assert(format == AUDIO_F32SYS);

    /* We can only do this if dst is aligned to 16 bytes; since src is the
       same pointer and it moves by 2, it can't be forcibly aligned. */
    if ((((size_t) dst) & 15) == 0) {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        const __m128 divby2 = _mm_set1_ps(0.5f);
        while (i >= 4) {   /* 4 * float32 */
            _mm_store_ps(dst, _mm_mul_ps(_mm_hadd_ps(_mm_load_ps(src), _mm_load_ps(src+4)), divby2));
            i -= 4; src += 8; dst += 4;
        }
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

/* Effectively mix right and left channels into a single channel */
static void SDLCALL
SDL_ConvertStereoToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("stereo", "mono");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i, src += 2) {
        *(dst++) = (src[0] + src[1]) * 0.5f;
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from 5.1 to stereo. Average left and right, discard subwoofer. */
static void SDLCALL
SDL_Convert51ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "stereo");
    SDL_assert(format == AUDIO_F32SYS);

    /* this assumes FL+FR+FC+subwoof+BL+BR layout. */
    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 2) {
        const double front_center = (double) src[2];
        dst[0] = (float) ((src[0] + front_center + src[4]) / 3.0);  /* left */
        dst[1] = (float) ((src[1] + front_center + src[5]) / 3.0);  /* right */
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from 5.1 to quad */
static void SDLCALL
SDL_Convert51ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    LOG_DEBUG_CONVERT("5.1", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    /* assumes quad is FL+FR+BL+BR layout and 5.1 is FL+FR+FC+subwoof+BL+BR */
    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 4) {
        /* FIXME: this is a good candidate for SIMD. */
        const double front_center = (double) src[2];
        dst[0] = (float) ((src[0] + front_center) * 0.5);  /* FL */
        dst[1] = (float) ((src[1] + front_center) * 0.5);  /* FR */
        dst[2] = (float) ((src[4] + front_center) * 0.5);  /* BL */
        dst[3] = (float) ((src[5] + front_center) * 0.5);  /* BR */
    }

    cvt->len_cvt /= 6;
    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Duplicate a mono channel to both stereo channels */
static void SDLCALL
SDL_ConvertMonoToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
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
SDL_ConvertStereoTo51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
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
        ce = (lf + rf) * 0.5f;
        dst[0] = lf + (lf - ce);  /* FL */
        dst[1] = rf + (rf - ce);  /* FR */
        dst[2] = ce;  /* FC */
        dst[3] = ce;  /* !!! FIXME: wrong! This is the subwoofer. */
        dst[4] = lf;  /* BL */
        dst[5] = rf;  /* BR */
    }

    cvt->len_cvt *= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Duplicate a stereo channel to a pseudo-4.0 stream */
static void SDLCALL
SDL_ConvertStereoToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 2);
    float lf, rf;
    int i;

    LOG_DEBUG_CONVERT("stereo", "quad");
    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i) {
        dst -= 4;
        src -= 2;
        lf = src[0];
        rf = src[1];
        dst[0] = lf;  /* FL */
        dst[1] = rf;  /* FR */
        dst[2] = lf;  /* BL */
        dst[3] = rf;  /* BR */
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static int
SDL_ResampleAudioSimple(const int chans, const double rate_incr,
                        float *last_sample, const float *inbuf,
                        const int inbuflen, float *outbuf, const int outbuflen)
{
    const int framelen = chans * (int)sizeof (float);
    const int total = (inbuflen / framelen);
    const int finalpos = (total * chans) - chans;
    const int dest_samples = (int)(((double)total) * rate_incr);
    const double src_incr = 1.0 / rate_incr;
    float *dst;
    double idx;
    int i;

    SDL_assert((dest_samples * framelen) <= outbuflen);
    SDL_assert((inbuflen % framelen) == 0);

    if (rate_incr > 1.0) {  /* upsample */
        float *target = (outbuf + chans);
        dst = outbuf + (dest_samples * chans);
        idx = (double) total;

        if (chans == 1) {
            const float final_sample = inbuf[finalpos];
            float earlier_sample = inbuf[finalpos];
            while (dst > target) {
                const int pos = ((int) idx) * chans;
                const float *src = &inbuf[pos];
                const float val = *(--src);
                SDL_assert(pos >= 0.0);
                *(--dst) = (val + earlier_sample) * 0.5f;
                earlier_sample = val;
                idx -= src_incr;
            }
            /* do last sample, interpolated against previous run's state. */
            *(--dst) = (inbuf[0] + last_sample[0]) * 0.5f;
            *last_sample = final_sample;
        } else if (chans == 2) {
            const float final_sample2 = inbuf[finalpos+1];
            const float final_sample1 = inbuf[finalpos];
            float earlier_sample2 = inbuf[finalpos];
            float earlier_sample1 = inbuf[finalpos-1];
            while (dst > target) {
                const int pos = ((int) idx) * chans;
                const float *src = &inbuf[pos];
                const float val2 = *(--src);
                const float val1 = *(--src);
                SDL_assert(pos >= 0.0);
                *(--dst) = (val2 + earlier_sample2) * 0.5f;
                *(--dst) = (val1 + earlier_sample1) * 0.5f;
                earlier_sample2 = val2;
                earlier_sample1 = val1;
                idx -= src_incr;
            }
            /* do last sample, interpolated against previous run's state. */
            *(--dst) = (inbuf[1] + last_sample[1]) * 0.5f;
            *(--dst) = (inbuf[0] + last_sample[0]) * 0.5f;
            last_sample[1] = final_sample2;
            last_sample[0] = final_sample1;
        } else {
            const float *earlier_sample = &inbuf[finalpos];
            float final_sample[8];
            SDL_memcpy(final_sample, &inbuf[finalpos], framelen);
            while (dst > target) {
                const int pos = ((int) idx) * chans;
                const float *src = &inbuf[pos];
                SDL_assert(pos >= 0.0);
                for (i = chans - 1; i >= 0; i--) {
                    const float val = *(--src);
                    *(--dst) = (val + earlier_sample[i]) * 0.5f;
                }
                earlier_sample = src;
                idx -= src_incr;
            }
            /* do last sample, interpolated against previous run's state. */
            for (i = chans - 1; i >= 0; i--) {
                const float val = inbuf[i];
                *(--dst) = (val + last_sample[i]) * 0.5f;
            }
            SDL_memcpy(last_sample, final_sample, framelen);
        }

        dst = (outbuf + (dest_samples * chans));
    } else {  /* downsample */
        float *target = (outbuf + (dest_samples * chans));
        dst = outbuf;
        idx = 0.0;
        if (chans == 1) {
            float last = *last_sample;
            while (dst < target) {
                const int pos = ((int) idx) * chans;
                const float val = inbuf[pos];
                SDL_assert(pos <= finalpos);
                *(dst++) = (val + last) * 0.5f;
                last = val;
                idx += src_incr;
            }
            *last_sample = last;
        } else if (chans == 2) {
            float last1 = last_sample[0];
            float last2 = last_sample[1];
            while (dst < target) {
                const int pos = ((int) idx) * chans;
                const float val1 = inbuf[pos];
                const float val2 = inbuf[pos+1];
                SDL_assert(pos <= finalpos);
                *(dst++) = (val1 + last1) * 0.5f;
                *(dst++) = (val2 + last2) * 0.5f;
                last1 = val1;
                last2 = val2;
                idx += src_incr;
            }
            last_sample[0] = last1;
            last_sample[1] = last2;
        } else {
            while (dst < target) {
                const int pos = ((int) idx) * chans;
                const float *src = &inbuf[pos];
                SDL_assert(pos <= finalpos);
                for (i = 0; i < chans; i++) {
                    const float val = *(src++);
                    *(dst++) = (val + last_sample[i]) * 0.5f;
                    last_sample[i] = val;
                }
                idx += src_incr;
            }
        }
    }

    return (int) ((dst - outbuf) * ((int) sizeof (float)));
}

/* We keep one special-case fast path around for an extremely common audio format. */
static int
SDL_ResampleAudioSimple_si16_c2(const double rate_incr,
                        Sint16 *last_sample, const Sint16 *inbuf,
                        const int inbuflen, Sint16 *outbuf, const int outbuflen)
{
    const int chans = 2;
    const int framelen = 4;  /* stereo 16 bit */
    const int total = (inbuflen / framelen);
    const int finalpos = (total * chans) - chans;
    const int dest_samples = (int)(((double)total) * rate_incr);
    const double src_incr = 1.0 / rate_incr;
    Sint16 *dst;
    double idx;

    SDL_assert((dest_samples * framelen) <= outbuflen);
    SDL_assert((inbuflen % framelen) == 0);

    if (rate_incr > 1.0) {
        Sint16 *target = (outbuf + chans);
        const Sint16 final_right = inbuf[finalpos+1];
        const Sint16 final_left = inbuf[finalpos];
        Sint16 earlier_right = inbuf[finalpos-1];
        Sint16 earlier_left = inbuf[finalpos-2];
        dst = outbuf + (dest_samples * chans);
        idx = (double) total;

        while (dst > target) {
            const int pos = ((int) idx) * chans;
            const Sint16 *src = &inbuf[pos];
            const Sint16 right = *(--src);
            const Sint16 left = *(--src);
            SDL_assert(pos >= 0.0);
            *(--dst) = (((Sint32) right) + ((Sint32) earlier_right)) >> 1;
            *(--dst) = (((Sint32) left) + ((Sint32) earlier_left)) >> 1;
            earlier_right = right;
            earlier_left = left;
            idx -= src_incr;
        }

        /* do last sample, interpolated against previous run's state. */
        *(--dst) = (((Sint32) inbuf[1]) + ((Sint32) last_sample[1])) >> 1;
        *(--dst) = (((Sint32) inbuf[0]) + ((Sint32) last_sample[0])) >> 1;
        last_sample[1] = final_right;
        last_sample[0] = final_left;

        dst = (outbuf + (dest_samples * chans));
    } else {
        Sint16 *target = (outbuf + (dest_samples * chans));
        dst = outbuf;
        idx = 0.0;
        while (dst < target) {
            const int pos = ((int) idx) * chans;
            const Sint16 *src = &inbuf[pos];
            const Sint16 left = *(src++);
            const Sint16 right = *(src++);
            SDL_assert(pos <= finalpos);
            *(dst++) = (((Sint32) left) + ((Sint32) last_sample[0])) >> 1;
            *(dst++) = (((Sint32) right) + ((Sint32) last_sample[1])) >> 1;
            last_sample[0] = left;
            last_sample[1] = right;
            idx += src_incr;
        }
    }

    return (int) ((dst - outbuf) * ((int) sizeof (Sint16)));
}

static void SDLCALL
SDL_ResampleCVT_si16_c2(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Sint16 *src = (const Sint16 *) cvt->buf;
    const int srclen = cvt->len_cvt;
    Sint16 *dst = (Sint16 *) cvt->buf;
    const int dstlen = (cvt->len * cvt->len_mult);
    Sint16 state[2] = { src[0], src[1] };

    SDL_assert(format == AUDIO_S16SYS);

    cvt->len_cvt = SDL_ResampleAudioSimple_si16_c2(cvt->rate_incr, state, src, srclen, dst, dstlen);
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, format);
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

static void
SDL_ResampleCVT(SDL_AudioCVT *cvt, const int chans, const SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    const int srclen = cvt->len_cvt;
    float *dst = (float *) cvt->buf;
    const int dstlen = (cvt->len * cvt->len_mult);
    float state[8];

    SDL_assert(format == AUDIO_F32SYS);

    SDL_memcpy(state, src, chans*sizeof(*src));

    cvt->len_cvt = SDL_ResampleAudioSimple(chans, cvt->rate_incr, state, src, srclen, dst, dstlen);
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

    /* Make sure we zero out the audio conversion before error checking */
    SDL_zerop(cvt);

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
    cvt->src_format = src_fmt;
    cvt->dst_format = dst_fmt;
    cvt->needed = 0;
    cvt->filter_index = 0;
    cvt->filters[0] = NULL;
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = ((double) dst_rate) / ((double) src_rate);

    /* SDL now favors float32 as its preferred internal format, and considers
       everything else to be a degenerate case that we might have to make
       multiple passes over the data to convert to and from float32 as
       necessary. That being said, we keep one special case around for
       efficiency: stereo data in Sint16 format, in the native byte order,
       that only needs resampling. This is likely to be the most popular
       legacy format, that apps, hardware and the OS are likely to be able
       to process directly, so we handle this one case directly without
       unnecessary conversions. This means that apps on embedded devices
       without floating point hardware should consider aiming for this
       format as well. */
    if ((src_channels == 2) && (dst_channels == 2) && (src_fmt == AUDIO_S16SYS) && (dst_fmt == AUDIO_S16SYS) && (src_rate != dst_rate)) {
        cvt->needed = 1;
        cvt->filters[cvt->filter_index++] = SDL_ResampleCVT_si16_c2;
        if (src_rate < dst_rate) {
            const double mult = ((double) dst_rate) / ((double) src_rate);
            cvt->len_mult *= (int) SDL_ceil(mult);
            cvt->len_ratio *= mult;
        } else {
            cvt->len_ratio /= ((double) src_rate) / ((double) dst_rate);
        }
        return 1;
    }

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - resample and change channel count if necessary.
        - convert back to native format.
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
            cvt->filters[cvt->filter_index++] = SDL_Convert_Byteswap;
            cvt->needed = 1;
            return 1;
        }
    }

    /* Convert data types, if necessary. Updates (cvt). */
    if (SDL_BuildAudioTypeCVTToFloat(cvt, src_fmt) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    /* Channel conversion */
    if (src_channels != dst_channels) {
        if ((src_channels == 1) && (dst_channels > 1)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertMonoToStereo;
            cvt->len_mult *= 2;
            src_channels = 2;
            cvt->len_ratio *= 2;
        }
        if ((src_channels == 2) && (dst_channels == 6)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStereoTo51;
            src_channels = 6;
            cvt->len_mult *= 3;
            cvt->len_ratio *= 3;
        }
        if ((src_channels == 2) && (dst_channels == 4)) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertStereoToQuad;
            src_channels = 4;
            cvt->len_mult *= 2;
            cvt->len_ratio *= 2;
        }
        while ((src_channels * 2) <= dst_channels) {
            cvt->filters[cvt->filter_index++] = SDL_ConvertMonoToStereo;
            cvt->len_mult *= 2;
            src_channels *= 2;
            cvt->len_ratio *= 2;
        }
        if ((src_channels == 6) && (dst_channels <= 2)) {
            cvt->filters[cvt->filter_index++] = SDL_Convert51ToStereo;
            src_channels = 2;
            cvt->len_ratio /= 3;
        }
        if ((src_channels == 6) && (dst_channels == 4)) {
            cvt->filters[cvt->filter_index++] = SDL_Convert51ToQuad;
            src_channels = 4;
            cvt->len_ratio /= 2;
        }
        /* This assumes that 4 channel audio is in the format:
           Left {front/back} + Right {front/back}
           so converting to L/R stereo works properly.
         */
        while (((src_channels % 2) == 0) &&
               ((src_channels / 2) >= dst_channels)) {
            SDL_AudioFilter filter = NULL;

            #if HAVE_SSE3_INTRINSICS
            if (SDL_HasSSE3()) {
                filter = SDL_ConvertStereoToMono_SSE3;
            }
            #endif

            if (!filter) {
                filter = SDL_ConvertStereoToMono;
            }

            cvt->filters[cvt->filter_index++] = filter;

            src_channels /= 2;
            cvt->len_ratio /= 2;
        }
        if (src_channels != dst_channels) {
            /* Uh oh.. */ ;
        }
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

struct SDL_AudioStream
{
    SDL_AudioCVT cvt_before_resampling;
    SDL_AudioCVT cvt_after_resampling;
    SDL_DataQueue *queue;
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

    if (inbuf == ((const float *) outbuf)) {  /* libsamplerate can't work in-place. */
        Uint8 *ptr = EnsureStreamBufferSize(stream, inbuflen + outbuflen);
        if (ptr == NULL) {
            SDL_OutOfMemory();
            return 0;
        }
        SDL_memcpy(ptr + outbuflen, ptr, inbuflen);
        inbuf = (const float *) (ptr + outbuflen);
        outbuf = (float *) ptr;
    }

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


typedef struct
{
    SDL_bool resampler_seeded;
    union
    {
        float f[8];
        Sint16 si16[2];
    } resampler_state;
} SDL_AudioStreamResamplerState;

static int
SDL_ResampleAudioStream(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const float *inbuf = (const float *) _inbuf;
    float *outbuf = (float *) _outbuf;
    SDL_AudioStreamResamplerState *state = (SDL_AudioStreamResamplerState*)stream->resampler_state;
    const int chans = (int)stream->pre_resample_channels;

    SDL_assert(chans <= SDL_arraysize(state->resampler_state.f));

    if (!state->resampler_seeded) {
        SDL_memcpy(state->resampler_state.f, inbuf, chans * sizeof (float));
        state->resampler_seeded = SDL_TRUE;
    }

    return SDL_ResampleAudioSimple(chans, stream->rate_incr, state->resampler_state.f, inbuf, inbuflen, outbuf, outbuflen);
}

static int
SDL_ResampleAudioStream_si16_c2(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const Sint16 *inbuf = (const Sint16 *) _inbuf;
    Sint16 *outbuf = (Sint16 *) _outbuf;
    SDL_AudioStreamResamplerState *state = (SDL_AudioStreamResamplerState*)stream->resampler_state;
    const int chans = (int)stream->pre_resample_channels;

    SDL_assert(chans <= SDL_arraysize(state->resampler_state.si16));

    if (!state->resampler_seeded) {
        state->resampler_state.si16[0] = inbuf[0];
        state->resampler_state.si16[1] = inbuf[1];
        state->resampler_seeded = SDL_TRUE;
    }

    return SDL_ResampleAudioSimple_si16_c2(stream->rate_incr, state->resampler_state.si16, inbuf, inbuflen, outbuf, outbuflen);
}

static void
SDL_ResetAudioStreamResampler(SDL_AudioStream *stream)
{
    SDL_AudioStreamResamplerState *state = (SDL_AudioStreamResamplerState*)stream->resampler_state;
    state->resampler_seeded = SDL_FALSE;
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
#ifndef HAVE_LIBSAMPLERATE_H
    const SDL_bool SRC_available = SDL_FALSE;
#endif

    retval = (SDL_AudioStream *) SDL_calloc(1, sizeof (SDL_AudioStream));
    if (!retval) {
        return NULL;
    }

    /* If increasing channels, do it after resampling, since we'd just
       do more work to resample duplicate channels. If we're decreasing, do
       it first so we resample the interpolated data instead of interpolating
       the resampled data (!!! FIXME: decide if that works in practice, though!). */
    pre_resample_channels = SDL_min(src_channels, dst_channels);

    retval->src_sample_frame_size = SDL_AUDIO_BITSIZE(src_format) * src_channels;
    retval->src_format = src_format;
    retval->src_channels = src_channels;
    retval->src_rate = src_rate;
    retval->dst_sample_frame_size = SDL_AUDIO_BITSIZE(dst_format) * dst_channels;
    retval->dst_format = dst_format;
    retval->dst_channels = dst_channels;
    retval->dst_rate = dst_rate;
    retval->pre_resample_channels = pre_resample_channels;
    retval->packetlen = packetlen;
    retval->rate_incr = ((double) dst_rate) / ((double) src_rate);

    /* Not resampling? It's an easy conversion (and maybe not even that!). */
    if (src_rate == dst_rate) {
        retval->cvt_before_resampling.needed = SDL_FALSE;
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, src_format, src_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    /* fast path special case for stereo Sint16 data that just needs resampling. */
    } else if ((!SRC_available) && (src_channels == 2) && (dst_channels == 2) && (src_format == AUDIO_S16SYS) && (dst_format == AUDIO_S16SYS)) {
        SDL_assert(src_rate != dst_rate);
        retval->resampler_state = SDL_calloc(1, sizeof(SDL_AudioStreamResamplerState));
        if (!retval->resampler_state) {
            SDL_FreeAudioStream(retval);
            SDL_OutOfMemory();
            return NULL;
        }
        retval->resampler_func = SDL_ResampleAudioStream_si16_c2;
        retval->reset_resampler_func = SDL_ResetAudioStreamResampler;
        retval->cleanup_resampler_func = SDL_CleanupAudioStreamResampler;
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
            retval->resampler_state = SDL_calloc(1, sizeof(SDL_AudioStreamResamplerState));
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

int
SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, const Uint32 _buflen)
{
    int buflen = (int) _buflen;
    const void *origbuf = buf;

    /* !!! FIXME: several converters can take advantage of SIMD, but only
       !!! FIXME:  if the data is aligned to 16 bytes. EnsureStreamBufferSize()
       !!! FIXME:  guarantees the buffer will align, but the
       !!! FIXME:  converters will iterate over the data backwards if
       !!! FIXME:  the output grows, and this means we won't align if buflen
       !!! FIXME:  isn't a multiple of 16. In these cases, we should chop off
       !!! FIXME:  a few samples at the end and convert them separately. */

    if (!stream) {
        return SDL_InvalidParamError("stream");
    } else if (!buf) {
        return SDL_InvalidParamError("buf");
    } else if (buflen == 0) {
        return 0;  /* nothing to do. */
    } else if ((buflen % stream->src_sample_frame_size) != 0) {
        return SDL_SetError("Can't add partial sample frames");
    }

    if (stream->cvt_before_resampling.needed) {
        const int workbuflen = buflen * stream->cvt_before_resampling.len_mult;  /* will be "* 1" if not needed */
        Uint8 *workbuf = EnsureStreamBufferSize(stream, workbuflen);
        if (workbuf == NULL) {
            return -1;  /* probably out of memory. */
        }
        SDL_assert(buf == origbuf);
        SDL_memcpy(workbuf, buf, buflen);
        stream->cvt_before_resampling.buf = workbuf;
        stream->cvt_before_resampling.len = buflen;
        if (SDL_ConvertAudio(&stream->cvt_before_resampling) == -1) {
            return -1;   /* uhoh! */
        }
        buf = workbuf;
        buflen = stream->cvt_before_resampling.len_cvt;
    }

    if (stream->dst_rate != stream->src_rate) {
        const int workbuflen = buflen * ((int) SDL_ceil(stream->rate_incr));
        Uint8 *workbuf = EnsureStreamBufferSize(stream, workbuflen);
        if (workbuf == NULL) {
            return -1;  /* probably out of memory. */
        }
        /* don't SDL_memcpy(workbuf, buf, buflen) here; our resampler can work inplace or not,
           libsamplerate needs buffers to be separate; either way, avoid a copy here if possible. */
        if (buf != origbuf) {
            buf = workbuf;  /* in case we realloc()'d the pointer. */
        }
        buflen = stream->resampler_func(stream, buf, buflen, workbuf, workbuflen);
        buf = EnsureStreamBufferSize(stream, workbuflen);
        SDL_assert(buf != NULL);  /* shouldn't be growing, just aligning. */
    }

    if (stream->cvt_after_resampling.needed) {
        const int workbuflen = buflen * stream->cvt_after_resampling.len_mult;  /* will be "* 1" if not needed */
        Uint8 *workbuf = EnsureStreamBufferSize(stream, workbuflen);
        if (workbuf == NULL) {
            return -1;  /* probably out of memory. */
        }
        if (buf == origbuf) {  /* copy if we haven't before. */
            SDL_memcpy(workbuf, buf, buflen);
        }
        stream->cvt_after_resampling.buf = workbuf;
        stream->cvt_after_resampling.len = buflen;
        if (SDL_ConvertAudio(&stream->cvt_after_resampling) == -1) {
            return -1;   /* uhoh! */
        }
        buf = workbuf;
        buflen = stream->cvt_after_resampling.len_cvt;
    }

    return SDL_WriteToDataQueue(stream->queue, buf, buflen);
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
    }
}


/* get converted/resampled data from the stream */
int
SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, const Uint32 len)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    } else if (!buf) {
        return SDL_InvalidParamError("buf");
    } else if (len == 0) {
        return 0;  /* nothing to do. */
    } else if ((len % stream->dst_sample_frame_size) != 0) {
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

/* dispose of a stream */
void
SDL_FreeAudioStream(SDL_AudioStream *stream)
{
    if (stream) {
        if (stream->cleanup_resampler_func) {
            stream->cleanup_resampler_func(stream);
        }
        SDL_FreeDataQueue(stream->queue);
        SDL_free(stream->work_buffer_base);
        SDL_free(stream);
    }
}

/* vi: set ts=4 sw=4 expandtab: */

