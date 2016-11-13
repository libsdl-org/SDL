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
#include "SDL_audio.h"
#include "SDL_audio_c.h"
#include "SDL_assert.h"

#define DIVBY127 0.0078740157480315f
#define DIVBY32767 3.05185094759972e-05f
#define DIVBY2147483647 4.6566128752458e-10f

void SDLCALL
SDL_Convert_S8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint8 *src = ((const Uint8 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 4)) - 1;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_S8", "AUDIO_F32");

    for (i = cvt->len_cvt / sizeof (Uint8); i; --i, --src, --dst) {
        *dst = (((float) ((Sint8) *src)) * DIVBY127);
    }

    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void SDLCALL
SDL_Convert_U8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint8 *src = ((const Uint8 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 4)) - 1;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_U8", "AUDIO_F32");

    for (i = cvt->len_cvt / sizeof (Uint8); i; --i, --src, --dst) {
        *dst = ((((float) *src) * DIVBY127) - 1.0f);
    }

    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void SDLCALL
SDL_Convert_S16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Sint16 *src = ((const Sint16 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 2)) - 1;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_S16", "AUDIO_F32");

    for (i = cvt->len_cvt / sizeof (Sint16); i; --i, --src, --dst) {
        *dst = (((float) *src) * DIVBY32767);
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void SDLCALL
SDL_Convert_U16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint16 *src = ((const Uint16 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 2)) - 1;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_U16", "AUDIO_F32");

    for (i = cvt->len_cvt / sizeof (Uint16); i; --i, --src, --dst) {
        *dst = ((((float) *src) * DIVBY32767) - 1.0f);
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void SDLCALL
SDL_Convert_S32_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint32 *src = (const Uint32 *) cvt->buf;
    float *dst = (float *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_S32", "AUDIO_F32");

    for (i = cvt->len_cvt / sizeof (Sint32); i; --i, ++src, ++dst) {
        *dst = (((float) *src) * DIVBY2147483647);
    }

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void SDLCALL
SDL_Convert_F32_to_S8(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint8 *dst = (Sint8 *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S8");

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        *dst = (Sint8) (*src * 127.0f);
    }

    cvt->len_cvt /= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S8);
    }
}

void SDLCALL
SDL_Convert_F32_to_U8(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Uint8 *dst = (Uint8 *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U8");

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        *dst = (Uint8) ((*src + 1.0f) * 127.0f);
    }

    cvt->len_cvt /= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_U8);
    }
}

void SDLCALL
SDL_Convert_F32_to_S16(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint16 *dst = (Sint16 *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S16");

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        *dst = (Sint16) (*src * 32767.0f);
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S16SYS);
    }
}

void SDLCALL
SDL_Convert_F32_to_U16(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Uint16 *dst = (Uint16 *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U16");

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        *dst = (Uint16) ((*src + 1.0f) * 32767.0f);
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_U16SYS);
    }
}

void SDLCALL
SDL_Convert_F32_to_S32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint32 *dst = (Sint32 *) cvt->buf;
    int i;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S32");

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        *dst = (Sint32) (*src * 2147483647.0);
    }

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S32SYS);
    }
}

void
SDL_Upsample_Arbitrary(SDL_AudioCVT *cvt, const int channels)
{
    const int srcsize = cvt->len_cvt - (64 * channels);
    const int dstsize = (int) (((double)(cvt->len_cvt/(channels*4))) * cvt->rate_incr) * (channels*4);
    register int eps = 0;
    float *dst = ((float *) (cvt->buf + dstsize)) - 8;
    const float *src = ((float *) (cvt->buf + cvt->len_cvt)) - 8;
    const float *target = ((const float *) cvt->buf);
    const size_t cpy = sizeof (float) * channels;
    float last_sample[8];
    float sample[8];
    int i;

#if DEBUG_CONVERT
    fprintf(stderr, "Upsample arbitrary (x%f), %d channels.\n", cvt->rate_incr, channels);
#endif

    SDL_assert(channels <= 8);

    SDL_memcpy(sample, src, cpy);
    SDL_memcpy(last_sample, src, cpy);

    while (dst > target) {
        SDL_memcpy(dst, sample, cpy);
        dst -= 8;
        eps += srcsize;
        if ((eps << 1) >= dstsize) {
            src -= 8;
            for (i = 0; i < channels; i++) {
                sample[i] = (float) ((((double) src[i]) + ((double) last_sample[i])) * 0.5);
            }
            SDL_memcpy(last_sample, sample, cpy);
            eps -= dstsize;
        }
    }

    cvt->len_cvt = dstsize;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Downsample_Arbitrary(SDL_AudioCVT *cvt, const int channels)
{
    const int srcsize = cvt->len_cvt - (64 * channels);
    const int dstsize = (int) (((double)(cvt->len_cvt/(channels*4))) * cvt->rate_incr) * (channels*4);
    register int eps = 0;
    float *dst = (float *) cvt->buf;
    const float *src = (float *) cvt->buf;
    const float *target = (const float *) (cvt->buf + dstsize);
    const size_t cpy = sizeof (float) * channels;
    float last_sample[8];
    float sample[8];
    int i;

#if DEBUG_CONVERT
    fprintf(stderr, "Downsample arbitrary (x%f), %d channels.\n", cvt->rate_incr, channels);
#endif

    SDL_assert(channels <= 8);

    SDL_memcpy(sample, src, cpy);
    SDL_memcpy(last_sample, src, cpy);

    while (dst < target) {
        src += 8;
        eps += dstsize;
        if ((eps << 1) >= srcsize) {
            SDL_memcpy(dst, sample, cpy);
            dst += 8;
            for (i = 0; i < channels; i++) {
                sample[i] = (float) ((((double) src[i]) + ((double) last_sample[i])) * 0.5);
            }
            SDL_memcpy(last_sample, sample, cpy);
            eps -= srcsize;
        }
    }

    cvt->len_cvt = dstsize;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Upsample_x2(SDL_AudioCVT *cvt, const int channels)
{
    const int dstsize = cvt->len_cvt * 2;
    float *dst = ((float *) (cvt->buf + dstsize)) - (channels * 2);
    const float *src = ((float *) (cvt->buf + cvt->len_cvt)) - channels;
    const float *target = ((const float *) cvt->buf);
    const size_t cpy = sizeof (float) * channels;
    float last_sample[8];
    int i;

#if DEBUG_CONVERT
    fprintf(stderr, "Upsample (x2), %d channels.\n", channels);
#endif

    SDL_assert(channels <= 8);
    SDL_memcpy(last_sample, src, cpy);

    while (dst > target) {
        for (i = 0; i < channels; i++) {
            dst[i] = (float) ((((double)src[i]) + ((double)last_sample[i])) * 0.5);
        }
        dst -= channels;
        SDL_memcpy(dst, src, cpy);
        SDL_memcpy(last_sample, src, cpy);
        src -= channels;
        dst -= channels;
    }

    cvt->len_cvt = dstsize;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Upsample_x4(SDL_AudioCVT *cvt, const int channels)
{
    const int dstsize = cvt->len_cvt * 4;
    float *dst = ((float *) (cvt->buf + dstsize)) - (channels * 4);
    const float *src = ((float *) (cvt->buf + cvt->len_cvt)) - channels;
    const float *target = ((const float *) cvt->buf);
    const size_t cpy = sizeof (float) * channels;
    float last_sample[8];
    int i;

#if DEBUG_CONVERT
    fprintf(stderr, "Upsample (x4), %d channels.\n", channels);
#endif

    SDL_assert(channels <= 8);
    SDL_memcpy(last_sample, src, cpy);

    while (dst > target) {
        for (i = 0; i < channels; i++) {
            dst[i] = (float) ((((double) src[i]) + (3.0 * ((double) last_sample[i]))) * 0.25);
        }
        dst -= channels;
        for (i = 0; i < channels; i++) {
            dst[i] = (float) ((((double) src[i]) + ((double) last_sample[i])) * 0.25);
        }
        dst -= channels;
        for (i = 0; i < channels; i++) {
            dst[i] = (float) (((3.0 * ((double) src[i])) + ((double) last_sample[i])) * 0.25);
        }
        dst -= channels;
        SDL_memcpy(dst, src, cpy);
        dst -= channels;
        SDL_memcpy(last_sample, src, cpy);
        src -= channels;
    }

    cvt->len_cvt = dstsize;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Downsample_Multiple(SDL_AudioCVT *cvt, const int multiple, const int channels)
{
    const int dstsize = cvt->len_cvt / multiple;
    float *dst = (float *) cvt->buf;
    const float *src = (float *) cvt->buf;
    const float *target = (const float *) (cvt->buf + dstsize);
    const size_t cpy = sizeof (float) * channels;
    float last_sample[8];
    int i;

#if DEBUG_CONVERT
    fprintf(stderr, "Downsample (x%d), %d channels.\n", multiple, channels);
#endif

    SDL_assert(channels <= 8);
    SDL_memcpy(last_sample, src, cpy);

    while (dst < target) {
        for (i = 0; i < channels; i++) {
            dst[i] = (float) ((((double)src[i]) + ((double)last_sample[i])) * 0.5);
        }
        dst += channels;

        SDL_memcpy(last_sample, src, cpy);
        src += (channels * multiple);
    }

    cvt->len_cvt = dstsize;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
