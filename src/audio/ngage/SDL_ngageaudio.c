/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_AUDIO_DRIVER_NGAGE

#include "../SDL_sysaudio.h"
#include "SDL_ngageaudio.h"

static SDL_AudioDevice *devptr = NULL;

SDL_AudioDevice *NGAGE_GetAudioDeviceAddr()
{
    return devptr;
}

static bool NGAGEAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    SDL_PrivateAudioData *phdata = SDL_calloc(1, sizeof(SDL_PrivateAudioData));
    if (!phdata) {
        SDL_OutOfMemory();
        return false;
    }
    device->hidden = phdata;

    phdata->buffer = SDL_calloc(1, device->buffer_size);
    if (!phdata->buffer) {
        SDL_OutOfMemory();
        SDL_free(phdata);
        return false;
    }
    devptr = device;

    // Since the phone can change the sample rate during a phone call,
    // we set the sample rate to 8KHz to be safe.  Even though it
    // might be possible to adjust the sample rate dynamically, it's
    // not supported by the current implementation.

    device->spec.format = SDL_AUDIO_S16LE;
    device->spec.channels = 1;
    device->spec.freq = 8000;

    SDL_UpdatedAudioDeviceFormat(device);

    return true;
}

static Uint8 *NGAGEAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
    if (!phdata) {
        *buffer_size = 0;
        return 0;
    }

    *buffer_size = device->buffer_size;
    return phdata->buffer;
}

static void NGAGEAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        SDL_free(device->hidden->buffer);
        SDL_free(device->hidden);
    }

    return;
}

static bool NGAGEAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = NGAGEAUDIO_OpenDevice;
    impl->GetDeviceBuf = NGAGEAUDIO_GetDeviceBuf;
    impl->CloseDevice = NGAGEAUDIO_CloseDevice;

    impl->ProvidesOwnCallbackThread = true;
    impl->OnlyHasDefaultPlaybackDevice = true;

    return true;
}

AudioBootStrap NGAGEAUDIO_bootstrap = { "N-Gage", "N-Gage audio driver", NGAGEAUDIO_Init, false };

#endif // SDL_AUDIO_DRIVER_NGAGE
