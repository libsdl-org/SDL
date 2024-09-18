/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_audioresample_h_
#define SDL_audioresample_h_

// Internal functions used by SDL_AudioStream for resampling audio.
// The resampler uses 32:32 fixed-point arithmetic to track its position.

int64_t SDL_GetResampleRate(int src_rate, int dst_rate);

int SDL_GetResamplerHistoryFrames(void);
int SDL_GetResamplerPaddingFrames(int64_t resample_rate);

int64_t SDL_GetResamplerInputFrames(int64_t output_frames, int64_t resample_rate, int64_t resample_offset);
int64_t SDL_GetResamplerOutputFrames(int64_t input_frames, int64_t resample_rate, int64_t *inout_resample_offset);

// Resample some audio.
// REQUIRES: `inframes >= SDL_GetResamplerInputFrames(outframes)`
// REQUIRES: At least `SDL_GetResamplerPaddingFrames(...)` extra frames to the left of src, and right of src+inframes
void SDL_ResampleAudio(int chans, const float *src, int inframes, float *dst, int outframes,
                       int64_t resample_rate, int64_t *inout_resample_offset);

#endif // SDL_audioresample_h_
