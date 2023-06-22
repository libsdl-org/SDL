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

#ifndef SDL_audio_c_h_
#define SDL_audio_c_h_

#include "SDL_internal.h"

#define DEBUG_AUDIOSTREAM 0
#define DEBUG_AUDIO_CONVERT 0

#if DEBUG_AUDIO_CONVERT
#define LOG_DEBUG_AUDIO_CONVERT(from, to) SDL_Log("SDL_AUDIO_CONVERT: Converting %s to %s.\n", from, to);
#else
#define LOG_DEBUG_AUDIO_CONVERT(from, to)
#endif

/* Functions and variables exported from SDL_audio.c for SDL_sysaudio.c */

/* Function to get a list of audio formats, ordered most similar to `format` to least, 0-terminated. Don't free results. */
const SDL_AudioFormat *SDL_ClosestAudioFormats(SDL_AudioFormat format);

/* Function to calculate the size and silence for a SDL_AudioSpec */
extern Uint8 SDL_GetSilenceValueForFormat(const SDL_AudioFormat format);
extern void SDL_CalculateAudioSpec(SDL_AudioSpec *spec);

/* Must be called at least once before using converters (SDL_CreateAudioStream will call it). */
extern void SDL_ChooseAudioConverters(void);

/* These pointers get set during SDL_ChooseAudioConverters() to various SIMD implementations. */
extern void (*SDL_Convert_S8_to_F32)(float *dst, const Sint8 *src, int num_samples);
extern void (*SDL_Convert_U8_to_F32)(float *dst, const Uint8 *src, int num_samples);
extern void (*SDL_Convert_S16_to_F32)(float *dst, const Sint16 *src, int num_samples);
extern void (*SDL_Convert_S32_to_F32)(float *dst, const Sint32 *src, int num_samples);
extern void (*SDL_Convert_F32_to_S8)(Sint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_U8)(Uint8 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S16)(Sint16 *dst, const float *src, int num_samples);
extern void (*SDL_Convert_F32_to_S32)(Sint32 *dst, const float *src, int num_samples);

/**
 * Use this function to initialize a particular audio driver.
 *
 * This function is used internally, and should not be used unless you have a
 * specific need to designate the audio driver you want to use. You should
 * normally use SDL_Init() or SDL_InitSubSystem().
 *
 * \param driver_name the name of the desired audio driver
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 */
extern int SDL_InitAudio(const char *driver_name);

/**
 * Use this function to shut down audio if you initialized it with SDL_InitAudio().
 *
 * This function is used internally, and should not be used unless you have a
 * specific need to specify the audio driver you want to use. You should
 * normally use SDL_Quit() or SDL_QuitSubSystem().
 */
extern void SDL_QuitAudio(void);

#endif /* SDL_audio_c_h_ */
