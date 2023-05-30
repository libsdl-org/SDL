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

/* Output audio to nowhere... */

#include "../SDL_audio_c.h"
#include "SDL_dummyaudio.h"

/* !!! FIXME: add a dummy WaitDevice to simulate real audio better? */

static int DUMMYAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    device->hidden = (struct SDL_PrivateAudioData *) SDL_malloc(device->buffer_size);
    if (!device->hidden) {
        return SDL_OutOfMemory();
    }
    return 0; /* don't change reported device format. */
}

static void DUMMYAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    SDL_free(device->hidden);
    device->hidden = NULL;
}

static Uint8 *DUMMYAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return (Uint8 *) device->hidden;
}

static int DUMMYAUDIO_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    /* Delay to make this sort of simulate real audio input. */
    SDL_Delay((device->sample_frames * 1000) / device->spec.freq);

    /* always return a full buffer of silence. */
    SDL_memset(buffer, device->silence_value, buflen);
    return buflen;
}

static SDL_bool DUMMYAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->OpenDevice = DUMMYAUDIO_OpenDevice;
    impl->CloseDevice = DUMMYAUDIO_CloseDevice;
    impl->GetDeviceBuf = DUMMYAUDIO_GetDeviceBuf;
    impl->CaptureFromDevice = DUMMYAUDIO_CaptureFromDevice;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;
    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap DUMMYAUDIO_bootstrap = {
    "dummy", "SDL dummy audio driver", DUMMYAUDIO_Init, SDL_TRUE
};
