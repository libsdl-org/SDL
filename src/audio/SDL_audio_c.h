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

#ifndef DEBUG_CONVERT
#define DEBUG_CONVERT 0
#endif

#if DEBUG_CONVERT
#define LOG_DEBUG_CONVERT(from, to) fprintf(stderr, "Converting %s to %s.\n", from, to);
#else
#define LOG_DEBUG_CONVERT(from, to)
#endif

/* Functions and variables exported from SDL_audio.c for SDL_sysaudio.c */

/* Functions to get a list of "close" audio formats */
extern SDL_AudioFormat SDL_FirstAudioFormat(SDL_AudioFormat format);
extern SDL_AudioFormat SDL_NextAudioFormat(void);

/* Function to calculate the size and silence for a SDL_AudioSpec */
extern void SDL_CalculateAudioSpec(SDL_AudioSpec * spec);

void SDLCALL SDL_Convert_S8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_U8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_S16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_U16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_S32_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S8(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_U8(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S16(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_U16(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDL_Upsample_Arbitrary(SDL_AudioCVT *cvt, const int channels);
void SDL_Downsample_Arbitrary(SDL_AudioCVT *cvt, const int channels);
void SDL_Upsample_x2(SDL_AudioCVT *cvt, const int channels);
void SDL_Upsample_x4(SDL_AudioCVT *cvt, const int channels);
void SDL_Downsample_Multiple(SDL_AudioCVT *cvt, const int multiple, const int channels);

/* vi: set ts=4 sw=4 expandtab: */
