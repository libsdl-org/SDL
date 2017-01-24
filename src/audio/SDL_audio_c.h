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

#ifndef SDL_audio_c_h_
#define SDL_audio_c_h_

#include "../SDL_internal.h"

#ifndef DEBUG_CONVERT
#define DEBUG_CONVERT 0
#endif

#if DEBUG_CONVERT
#define LOG_DEBUG_CONVERT(from, to) fprintf(stderr, "Converting %s to %s.\n", from, to);
#else
#define LOG_DEBUG_CONVERT(from, to)
#endif

/* Functions and variables exported from SDL_audio.c for SDL_sysaudio.c */

#ifdef HAVE_LIBSAMPLERATE_H
#include "samplerate.h"
extern SDL_bool SRC_available;
extern int SRC_converter;
extern SRC_STATE* (*SRC_src_new)(int converter_type, int channels, int *error);
extern int (*SRC_src_process)(SRC_STATE *state, SRC_DATA *data);
extern int (*SRC_src_reset)(SRC_STATE *state);
extern SRC_STATE* (*SRC_src_delete)(SRC_STATE *state);
extern const char* (*SRC_src_strerror)(int error);
#endif

/* Functions to get a list of "close" audio formats */
extern SDL_AudioFormat SDL_FirstAudioFormat(SDL_AudioFormat format);
extern SDL_AudioFormat SDL_NextAudioFormat(void);

/* Function to calculate the size and silence for a SDL_AudioSpec */
extern void SDL_CalculateAudioSpec(SDL_AudioSpec * spec);

/* These pointers get set during init to various SIMD implementations. */
extern SDL_AudioFilter SDL_Convert_S8_to_F32;
extern SDL_AudioFilter SDL_Convert_U8_to_F32;
extern SDL_AudioFilter SDL_Convert_S16_to_F32;
extern SDL_AudioFilter SDL_Convert_U16_to_F32;
extern SDL_AudioFilter SDL_Convert_S32_to_F32;
extern SDL_AudioFilter SDL_Convert_F32_to_S8;
extern SDL_AudioFilter SDL_Convert_F32_to_U8;
extern SDL_AudioFilter SDL_Convert_F32_to_S16;
extern SDL_AudioFilter SDL_Convert_F32_to_U16;
extern SDL_AudioFilter SDL_Convert_F32_to_S32;


/* SDL_AudioStream is a new audio conversion interface. It
    might eventually become a public API.
   The benefits vs SDL_AudioCVT:
    - it can handle resampling data in chunks without generating
      artifacts, when it doesn't have the complete buffer available.
    - it can handle incoming data in any variable size.
    - You push data as you have it, and pull it when you need it

    (Note that currently this converts as data is put into the stream, so
    you need to push more than a handful of bytes if you want decent
    resampling. This can be changed later.)
 */

/* this is opaque to the outside world. */
typedef struct SDL_AudioStream SDL_AudioStream;

/* create a new stream */
SDL_AudioStream *SDL_NewAudioStream(const SDL_AudioFormat src_format,
                                    const Uint8 src_channels,
                                    const int src_rate,
                                    const SDL_AudioFormat dst_format,
                                    const Uint8 dst_channels,
                                    const int dst_rate);

/* add data to be converted/resampled to the stream */
int SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, const Uint32 len);

/* get converted/resampled data from the stream */
int SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, const Uint32 len);

/* clear any pending data in the stream without converting it. */
void SDL_AudioStreamClear(SDL_AudioStream *stream);

/* number of converted/resampled bytes available */
int SDL_AudioStreamAvailable(SDL_AudioStream *stream);

/* dispose of a stream */
void SDL_FreeAudioStream(SDL_AudioStream *stream);

#endif

/* vi: set ts=4 sw=4 expandtab: */
