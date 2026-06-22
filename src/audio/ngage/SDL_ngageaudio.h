/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_ngageaudio_h
#define SDL_ngageaudio_h

#ifndef SDL_HINT_AUDIO_NGAGE_LATENCY
#define SDL_HINT_AUDIO_NGAGE_LATENCY "SDL_AUDIO_NGAGE_LATENCY"
#endif

#ifndef SDL_HINT_AUDIO_NGAGE_SCHEDULER_TICK
#define SDL_HINT_AUDIO_NGAGE_SCHEDULER_TICK   "SDL_AUDIO_NGAGE_SCHEDULER_TICK"
#endif

#ifndef SDL_HINT_AUDIO_NGAGE_PROCESS_TICK
#define SDL_HINT_AUDIO_NGAGE_PROCESS_TICK     "SDL_AUDIO_NGAGE_PROCESS_TICK"
#endif

#ifndef SDL_HINT_AUDIO_NGAGE_PROCESS_PRIORITY
#define SDL_HINT_AUDIO_NGAGE_PROCESS_PRIORITY "SDL_AUDIO_NGAGE_PROCESS_PRIORITY"
#endif
typedef struct SDL_PrivateAudioData
{
    Uint8 *buffer[2];
    int fill_index; /* Which buffer SDL is currently filling */
    int play_index; /* Which buffer the hardware is currently using*/
    int buffer_size;

} SDL_PrivateAudioData;

#ifdef __cplusplus
extern "C" {
#endif

#include "../SDL_sysaudio.h"

SDL_AudioDevice *NGAGE_GetAudioDeviceAddr();

#ifdef __cplusplus
}
#endif

#endif // SDL_ngageaudio_h
