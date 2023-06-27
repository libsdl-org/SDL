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

#ifdef SDL_AUDIO_DRIVER_VITA

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_vitaaudio.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/audioout.h>
#include <psp2/audioin.h>

#define SCE_AUDIO_SAMPLE_ALIGN(s) (((s) + 63) & ~63)
#define SCE_AUDIO_MAX_VOLUME      0x8000

static int VITAAUD_OpenCaptureDevice(SDL_AudioDevice *_this)
{
    _this->spec.freq = 16000;
    _this->spec.samples = 512;
    _this->spec.channels = 1;

    SDL_CalculateAudioSpec(&_this->spec);

    _this->hidden->port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE, 512, 16000, SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);

    if (_this->hidden->port < 0) {
        return SDL_SetError("Couldn't open audio in port: %x", _this->hidden->port);
    }

    return 0;
}

static int VITAAUD_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    int format, mixlen, i, port = SCE_AUDIO_OUT_PORT_TYPE_MAIN;
    int vols[2] = { SCE_AUDIO_MAX_VOLUME, SCE_AUDIO_MAX_VOLUME };
    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts;

    _this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc(sizeof(*_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(_this->hidden, 0, sizeof(*_this->hidden));

    closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == SDL_AUDIO_S16LSB) {
            _this->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        return SDL_SetError("Unsupported audio format");
    }

    if (_this->iscapture) {
        return VITAAUD_OpenCaptureDevice(_this);
    }

    /* The sample count must be a multiple of 64. */
    _this->spec.samples = SCE_AUDIO_SAMPLE_ALIGN(_this->spec.samples);

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

    /* Setup the hardware channel. */
    if (_this->spec.channels == 1) {
        format = SCE_AUDIO_OUT_MODE_MONO;
    } else {
        format = SCE_AUDIO_OUT_MODE_STEREO;
    }

    if (_this->spec.freq < 48000) {
        port = SCE_AUDIO_OUT_PORT_TYPE_BGM;
    }

    _this->hidden->port = sceAudioOutOpenPort(port, _this->spec.samples, _this->spec.freq, format);
    if (_this->hidden->port < 0) {
        SDL_aligned_free(_this->hidden->rawbuf);
        _this->hidden->rawbuf = NULL;
        return SDL_SetError("Couldn't open audio out port: %x", _this->hidden->port);
    }

    sceAudioOutSetVolume(_this->hidden->port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vols);

    SDL_memset(_this->hidden->rawbuf, 0, mixlen);
    for (i = 0; i < NUM_BUFFERS; i++) {
        _this->hidden->mixbufs[i] = &_this->hidden->rawbuf[i * _this->spec.size];
    }

    _this->hidden->next_buffer = 0;
    return 0;
}

static void VITAAUD_PlayDevice(SDL_AudioDevice *_this)
{
    Uint8 *mixbuf = _this->hidden->mixbufs[_this->hidden->next_buffer];

    sceAudioOutOutput(_this->hidden->port, mixbuf);

    _this->hidden->next_buffer = (_this->hidden->next_buffer + 1) % NUM_BUFFERS;
}

/* This function waits until it is possible to write a full sound buffer */
static void VITAAUD_WaitDevice(SDL_AudioDevice *_this)
{
    /* Because we block when sending audio, there's no need for this function to do anything. */
}

static Uint8 *VITAAUD_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return _this->hidden->mixbufs[_this->hidden->next_buffer];
}

static void VITAAUD_CloseDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->port >= 0) {
        if (_this->iscapture) {
            sceAudioInReleasePort(_this->hidden->port);
        } else {
            sceAudioOutReleasePort(_this->hidden->port);
        }
        _this->hidden->port = -1;
    }

    if (!_this->iscapture && _this->hidden->rawbuf != NULL) {
        SDL_aligned_free(_this->hidden->rawbuf);
        _this->hidden->rawbuf = NULL;
    }
}

static int VITAAUD_CaptureFromDevice(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    int ret;
    SDL_assert(buflen == _this->spec.size);
    ret = sceAudioInInput(_this->hidden->port, buffer);
    if (ret < 0) {
        return SDL_SetError("Failed to capture from device: %x", ret);
    }
    return _this->spec.size;
}

static void VITAAUD_ThreadInit(SDL_AudioDevice *_this)
{
    /* Increase the priority of this audio thread by 1 to put it
       ahead of other SDL threads. */
    SceUID thid;
    SceKernelThreadInfo info;
    thid = sceKernelGetThreadId();
    info.size = sizeof(SceKernelThreadInfo);
    if (sceKernelGetThreadInfo(thid, &info) == 0) {
        sceKernelChangeThreadPriority(thid, info.currentPriority - 1);
    }
}

static SDL_bool VITAAUD_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->OpenDevice = VITAAUD_OpenDevice;
    impl->PlayDevice = VITAAUD_PlayDevice;
    impl->WaitDevice = VITAAUD_WaitDevice;
    impl->GetDeviceBuf = VITAAUD_GetDeviceBuf;
    impl->CloseDevice = VITAAUD_CloseDevice;
    impl->ThreadInit = VITAAUD_ThreadInit;

    impl->CaptureFromDevice = VITAAUD_CaptureFromDevice;

    /* and the capabilities */
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap VITAAUD_bootstrap = {
    "vita", "VITA audio driver", VITAAUD_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_VITA */
