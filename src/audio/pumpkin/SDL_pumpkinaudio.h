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

#ifndef SDL_pumpkinaudio_h_
#define SDL_pumpkinaudio_h_

#include "../SDL_sysaudio.h"

struct SDL_PrivateAudioData
{
    Uint32 channel;        // PalmOS SndStreamRef
    bool have_channel;
    bool silent;           // no PumpkinOS audio: just pace the audio thread
    Uint32 io_delay;       // milliseconds per buffer, for the silent mode

    Uint8 *mixbuf;         // buffer filled by the SDL audio thread

    // ring buffer between the SDL audio thread and the PumpkinOS audio callback
    Uint8 *ring;
    int ring_size;
    int ring_read;
    int ring_fill;
    SDL_Mutex *lock;
    SDL_Condition *cond;
};

#endif // SDL_pumpkinaudio_h_
