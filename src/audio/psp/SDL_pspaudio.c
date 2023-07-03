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

#ifdef SDL_AUDIO_DRIVER_PSP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_pspaudio.h"

#include <pspaudio.h>
#include <pspthreadman.h>

/* The tag name used by PSP audio */
#define PSPAUDIO_DRIVER_NAME "psp"

static inline SDL_bool isBasicAudioConfig(const SDL_AudioSpec *spec)
{
    return spec->freq == 44100;
}

static int PSPAUDIO_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    int format, mixlen, i;

    _this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc(sizeof(*_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(_this->hidden);

    /* device only natively supports S16LSB */
    _this->spec.format = SDL_AUDIO_S16LSB;

    /*  PSP has some limitations with the Audio. It fully supports 44.1KHz (Mono & Stereo),
        however with frequencies different than 44.1KHz, it just supports Stereo,
        so a resampler must be done for these scenarios */
    if (isBasicAudioConfig(&_this->spec)) {
        /* The sample count must be a multiple of 64. */
        _this->spec.samples = PSP_AUDIO_SAMPLE_ALIGN(_this->spec.samples);
        /* The number of channels (1 or 2). */
        _this->spec.channels = _this->spec.channels == 1 ? 1 : 2;
        format = _this->spec.channels == 1 ? PSP_AUDIO_FORMAT_MONO : PSP_AUDIO_FORMAT_STEREO;
        _this->hidden->channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, _this->spec.samples, format);
    } else {
        /* 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11050, 8000 */
        switch (_this->spec.freq) {
        case 8000:
        case 11025:
        case 12000:
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
            _this->spec.freq = _this->spec.freq;
            break;
        default:
            _this->spec.freq = 48000;
            break;
        }
        /* The number of samples to output in one output call (min 17, max 4111). */
        _this->spec.samples = _this->spec.samples < 17 ? 17 : (_this->spec.samples > 4111 ? 4111 : _this->spec.samples);
        _this->spec.channels = 2; /* we're forcing the hardware to stereo. */
        _this->hidden->channel = sceAudioSRCChReserve(_this->spec.samples, _this->spec.freq, 2);
    }

    if (_this->hidden->channel < 0) {
        SDL_aligned_free(_this->hidden->rawbuf);
        _this->hidden->rawbuf = NULL;
        return SDL_SetError("Couldn't reserve hardware channel");
    }

    /* Update the fragment size as size in bytes. */
    SDL_CalculateAudioSpec(&_this->spec);

    /* Allocate the mixing buffer.  Its size and starting address must
       be a multiple of 64 bytes.  Our sample count is already a multiple of
       64, so spec->size should be a multiple of 64 as well. */
    mixlen = _this->spec.size * NUM_BUFFERS;
    _this->hidden->rawbuf = (Uint8 *)SDL_aligned_alloc(64, mixlen);
    if (_this->hidden->rawbuf == NULL) {
        return SDL_SetError("Couldn't allocate mixing buffer");
    }

    SDL_memset(_this->hidden->rawbuf, 0, mixlen);
    for (i = 0; i < NUM_BUFFERS; i++) {
        _this->hidden->mixbufs[i] = &_this->hidden->rawbuf[i * _this->spec.size];
    }

    _this->hidden->next_buffer = 0;
    return 0;
}

static void PSPAUDIO_PlayDevice(SDL_AudioDevice *_this)
{
    Uint8 *mixbuf = _this->hidden->mixbufs[_this->hidden->next_buffer];
    if (!isBasicAudioConfig(&_this->spec)) {
        SDL_assert(_this->spec.channels == 2);
        sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, mixbuf);
    } else {
        sceAudioOutputPannedBlocking(_this->hidden->channel, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, mixbuf);
    }

    _this->hidden->next_buffer = (_this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void PSPAUDIO_WaitDevice(SDL_AudioDevice *_this)
{
    /* Because we block when sending audio, there's no need for this function to do anything. */
}

static Uint8 *PSPAUDIO_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return _this->hidden->mixbufs[_this->hidden->next_buffer];
}

static void PSPAUDIO_CloseDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->channel >= 0) {
        if (!isBasicAudioConfig(&_this->spec)) {
            sceAudioSRCChRelease();
        } else {
            sceAudioChRelease(_this->hidden->channel);
        }
        _this->hidden->channel = -1;
    }

    if (_this->hidden->rawbuf != NULL) {
        SDL_aligned_free(_this->hidden->rawbuf);
        _this->hidden->rawbuf = NULL;
    }
}

static void PSPAUDIO_ThreadInit(SDL_AudioDevice *_this)
{
    /* Increase the priority of this audio thread by 1 to put it
       ahead of other SDL threads. */
    SceUID thid;
    SceKernelThreadInfo status;
    thid = sceKernelGetThreadId();
    status.size = sizeof(SceKernelThreadInfo);
    if (sceKernelReferThreadStatus(thid, &status) == 0) {
        sceKernelChangeThreadPriority(thid, status.currentPriority - 1);
    }
}

static SDL_bool PSPAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->OpenDevice = PSPAUDIO_OpenDevice;
    impl->PlayDevice = PSPAUDIO_PlayDevice;
    impl->WaitDevice = PSPAUDIO_WaitDevice;
    impl->GetDeviceBuf = PSPAUDIO_GetDeviceBuf;
    impl->CloseDevice = PSPAUDIO_CloseDevice;
    impl->ThreadInit = PSPAUDIO_ThreadInit;

    /* PSP audio device */
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    /*
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;
    */
    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap PSPAUDIO_bootstrap = {
    "psp", "PSP audio driver", PSPAUDIO_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_PSP */
