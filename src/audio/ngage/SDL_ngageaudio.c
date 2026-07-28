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

    phdata->buffer[0] = SDL_calloc(1, device->buffer_size);
    phdata->buffer[1] = SDL_calloc(1, device->buffer_size);
    if (!phdata->buffer[0] || !phdata->buffer[1])
    {
        SDL_Log("Error: Failed to allocate audio buffers");
        SDL_free(phdata->buffer[0]);
        SDL_free(phdata->buffer[1]);
        SDL_free(phdata);
        return false;
    }

    phdata->fill_index = 0;

    devptr = device;

    device->spec.format = SDL_AUDIO_S16LE;
    device->spec.channels = 1;
    device->spec.freq = 8000;

    SDL_UpdatedAudioDeviceFormat(device);

    return true;
}

/*********************************************

NGAGEAUDIO_GetDeviceBuf -

Return the buffer that is currently being filled by SDL

**********************************************/
static Uint8 *NGAGEAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
    if (!phdata) {
        *buffer_size = 0;
        return 0;
    }
     
    *buffer_size = device->buffer_size;
    
    return phdata->buffer[phdata->fill_index];
}



static void NGAGEAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;

        SDL_free(phdata->buffer[0]);
        SDL_free(phdata->buffer[1]);
        SDL_free(phdata);
        device->hidden = NULL;
    }
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
