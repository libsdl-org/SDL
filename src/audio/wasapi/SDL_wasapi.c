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

#ifdef SDL_AUDIO_DRIVER_WASAPI

#include "../../core/windows/SDL_windows.h"
#include "../../core/windows/SDL_immdevice.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"

#define COBJMACROS
#include <audioclient.h>

#include "SDL_wasapi.h"

/* These constants aren't available in older SDKs */
#ifndef AUDCLNT_STREAMFLAGS_RATEADJUST
#define AUDCLNT_STREAMFLAGS_RATEADJUST 0x00100000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

/* Some GUIDs we need to know without linking to libraries that aren't available before Vista. */
static const IID SDL_IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const IID SDL_IID_IAudioCaptureClient = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };


static void WASAPI_DetectDevices(void)
{
    WASAPI_EnumerateEndpoints();
}

static SDL_INLINE SDL_bool WasapiFailed(SDL_AudioDevice *_this, const HRESULT err)
{
    if (err == S_OK) {
        return SDL_FALSE;
    }

    if (err == AUDCLNT_E_DEVICE_INVALIDATED) {
        _this->hidden->device_lost = SDL_TRUE;
    } else if (SDL_AtomicGet(&_this->enabled)) {
        IAudioClient_Stop(_this->hidden->client);
        SDL_OpenedAudioDeviceDisconnected(_this);
        SDL_assert(!SDL_AtomicGet(&_this->enabled));
    }

    return SDL_TRUE;
}

static int UpdateAudioStream(SDL_AudioDevice *_this, const SDL_AudioSpec *oldspec)
{
    /* Since WASAPI requires us to handle all audio conversion, and our
       device format might have changed, we might have to add/remove/change
       the audio stream that the higher level uses to convert data, so
       SDL keeps firing the callback as if nothing happened here. */

    if ((_this->callbackspec.channels == _this->spec.channels) &&
        (_this->callbackspec.format == _this->spec.format) &&
        (_this->callbackspec.freq == _this->spec.freq) &&
        (_this->callbackspec.samples == _this->spec.samples)) {
        /* no need to buffer/convert in an AudioStream! */
        SDL_DestroyAudioStream(_this->stream);
        _this->stream = NULL;
    } else if ((oldspec->channels == _this->spec.channels) &&
               (oldspec->format == _this->spec.format) &&
               (oldspec->freq == _this->spec.freq)) {
        /* The existing audio stream is okay to keep using. */
    } else {
        /* replace the audiostream for new format */
        SDL_DestroyAudioStream(_this->stream);
        if (_this->iscapture) {
            _this->stream = SDL_CreateAudioStream(_this->spec.format,
                                              _this->spec.channels, _this->spec.freq,
                                              _this->callbackspec.format,
                                              _this->callbackspec.channels,
                                              _this->callbackspec.freq);
        } else {
            _this->stream = SDL_CreateAudioStream(_this->callbackspec.format,
                                              _this->callbackspec.channels,
                                              _this->callbackspec.freq, _this->spec.format,
                                              _this->spec.channels, _this->spec.freq);
        }

        if (!_this->stream) {
            return -1; /* SDL_CreateAudioStream should have called SDL_SetError. */
        }
    }

    /* make sure our scratch buffer can cover the new device spec. */
    if (_this->spec.size > _this->work_buffer_len) {
        Uint8 *ptr = (Uint8 *)SDL_realloc(_this->work_buffer, _this->spec.size);
        if (ptr == NULL) {
            return SDL_OutOfMemory();
        }
        _this->work_buffer = ptr;
        _this->work_buffer_len = _this->spec.size;
    }

    return 0;
}

static void ReleaseWasapiDevice(SDL_AudioDevice *_this);

static SDL_bool RecoverWasapiDevice(SDL_AudioDevice *_this)
{
    ReleaseWasapiDevice(_this); /* dump the lost device's handles. */

    if (_this->hidden->default_device_generation) {
        _this->hidden->default_device_generation = SDL_AtomicGet(_this->iscapture ? &SDL_IMMDevice_DefaultCaptureGeneration : &SDL_IMMDevice_DefaultPlaybackGeneration);
    }

    /* this can fail for lots of reasons, but the most likely is we had a
       non-default device that was disconnected, so we can't recover. Default
       devices try to reinitialize whatever the new default is, so it's more
       likely to carry on here, but this handles a non-default device that
       simply had its format changed in the Windows Control Panel. */
    if (WASAPI_ActivateDevice(_this, SDL_TRUE) == -1) {
        SDL_OpenedAudioDeviceDisconnected(_this);
        return SDL_FALSE;
    }

    _this->hidden->device_lost = SDL_FALSE;

    return SDL_TRUE; /* okay, carry on with new device details! */
}

static SDL_bool RecoverWasapiIfLost(SDL_AudioDevice *_this)
{
    const int generation = _this->hidden->default_device_generation;
    SDL_bool lost = _this->hidden->device_lost;

    if (!SDL_AtomicGet(&_this->enabled)) {
        return SDL_FALSE; /* already failed. */
    }

    if (!_this->hidden->client) {
        return SDL_TRUE; /* still waiting for activation. */
    }

    if (!lost && (generation > 0)) { /* is a default device? */
        const int newgen = SDL_AtomicGet(_this->iscapture ? &SDL_IMMDevice_DefaultCaptureGeneration : &SDL_IMMDevice_DefaultPlaybackGeneration);
        if (generation != newgen) { /* the desired default device was changed, jump over to it. */
            lost = SDL_TRUE;
        }
    }

    return lost ? RecoverWasapiDevice(_this) : SDL_TRUE;
}

static Uint8 *WASAPI_GetDeviceBuf(SDL_AudioDevice *_this)
{
    /* get an endpoint buffer from WASAPI. */
    BYTE *buffer = NULL;

    while (RecoverWasapiIfLost(_this) && _this->hidden->render) {
        if (!WasapiFailed(_this, IAudioRenderClient_GetBuffer(_this->hidden->render, _this->spec.samples, &buffer))) {
            return (Uint8 *)buffer;
        }
        SDL_assert(buffer == NULL);
    }

    return (Uint8 *)buffer;
}

static void WASAPI_PlayDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->render != NULL) { /* definitely activated? */
        /* WasapiFailed() will mark the device for reacquisition or removal elsewhere. */
        WasapiFailed(_this, IAudioRenderClient_ReleaseBuffer(_this->hidden->render, _this->spec.samples, 0));
    }
}

static void WASAPI_WaitDevice(SDL_AudioDevice *_this)
{
    while (RecoverWasapiIfLost(_this) && _this->hidden->client && _this->hidden->event) {
        DWORD waitResult = WaitForSingleObjectEx(_this->hidden->event, 200, FALSE);
        if (waitResult == WAIT_OBJECT_0) {
            const UINT32 maxpadding = _this->spec.samples;
            UINT32 padding = 0;
            if (!WasapiFailed(_this, IAudioClient_GetCurrentPadding(_this->hidden->client, &padding))) {
                /*SDL_Log("WASAPI EVENT! padding=%u maxpadding=%u", (unsigned int)padding, (unsigned int)maxpadding);*/
                if (_this->iscapture) {
                    if (padding > 0) {
                        break;
                    }
                } else {
                    if (padding <= maxpadding) {
                        break;
                    }
                }
            }
        } else if (waitResult != WAIT_TIMEOUT) {
            /*SDL_Log("WASAPI FAILED EVENT!");*/
            IAudioClient_Stop(_this->hidden->client);
            SDL_OpenedAudioDeviceDisconnected(_this);
        }
    }
}

static int WASAPI_CaptureFromDevice(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    SDL_AudioStream *stream = _this->hidden->capturestream;
    const int avail = SDL_GetAudioStreamAvailable(stream);
    if (avail > 0) {
        const int cpy = SDL_min(buflen, avail);
        SDL_GetAudioStreamData(stream, buffer, cpy);
        return cpy;
    }

    while (RecoverWasapiIfLost(_this)) {
        HRESULT ret;
        BYTE *ptr = NULL;
        UINT32 frames = 0;
        DWORD flags = 0;

        /* uhoh, client isn't activated yet, just return silence. */
        if (!_this->hidden->capture) {
            /* Delay so we run at about the speed that audio would be arriving. */
            SDL_Delay(((_this->spec.samples * 1000) / _this->spec.freq));
            SDL_memset(buffer, _this->spec.silence, buflen);
            return buflen;
        }

        ret = IAudioCaptureClient_GetBuffer(_this->hidden->capture, &ptr, &frames, &flags, NULL, NULL);
        if (ret != AUDCLNT_S_BUFFER_EMPTY) {
            WasapiFailed(_this, ret); /* mark device lost/failed if necessary. */
        }

        if ((ret == AUDCLNT_S_BUFFER_EMPTY) || !frames) {
            WASAPI_WaitDevice(_this);
        } else if (ret == S_OK) {
            const int total = ((int)frames) * _this->hidden->framesize;
            const int cpy = SDL_min(buflen, total);
            const int leftover = total - cpy;
            const SDL_bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? SDL_TRUE : SDL_FALSE;

            if (silent) {
                SDL_memset(buffer, _this->spec.silence, cpy);
            } else {
                SDL_memcpy(buffer, ptr, cpy);
            }

            if (leftover > 0) {
                ptr += cpy;
                if (silent) {
                    SDL_memset(ptr, _this->spec.silence, leftover); /* I guess this is safe? */
                }

                if (SDL_PutAudioStreamData(stream, ptr, leftover) == -1) {
                    return -1; /* uhoh, out of memory, etc. Kill device.  :( */
                }
            }

            ret = IAudioCaptureClient_ReleaseBuffer(_this->hidden->capture, frames);
            WasapiFailed(_this, ret); /* mark device lost/failed if necessary. */

            return cpy;
        }
    }

    return -1; /* unrecoverable error. */
}

static void WASAPI_FlushCapture(SDL_AudioDevice *_this)
{
    BYTE *ptr = NULL;
    UINT32 frames = 0;
    DWORD flags = 0;

    if (!_this->hidden->capture) {
        return; /* not activated yet? */
    }

    /* just read until we stop getting packets, throwing them away. */
    while (SDL_TRUE) {
        const HRESULT ret = IAudioCaptureClient_GetBuffer(_this->hidden->capture, &ptr, &frames, &flags, NULL, NULL);
        if (ret == AUDCLNT_S_BUFFER_EMPTY) {
            break; /* no more buffered data; we're done. */
        } else if (WasapiFailed(_this, ret)) {
            break; /* failed for some other reason, abort. */
        } else if (WasapiFailed(_this, IAudioCaptureClient_ReleaseBuffer(_this->hidden->capture, frames))) {
            break; /* something broke. */
        }
    }
    SDL_ClearAudioStream(_this->hidden->capturestream);
}

static void ReleaseWasapiDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->client) {
        IAudioClient_Stop(_this->hidden->client);
        IAudioClient_Release(_this->hidden->client);
        _this->hidden->client = NULL;
    }

    if (_this->hidden->render) {
        IAudioRenderClient_Release(_this->hidden->render);
        _this->hidden->render = NULL;
    }

    if (_this->hidden->capture) {
        IAudioCaptureClient_Release(_this->hidden->capture);
        _this->hidden->capture = NULL;
    }

    if (_this->hidden->waveformat) {
        CoTaskMemFree(_this->hidden->waveformat);
        _this->hidden->waveformat = NULL;
    }

    if (_this->hidden->capturestream) {
        SDL_DestroyAudioStream(_this->hidden->capturestream);
        _this->hidden->capturestream = NULL;
    }

    if (_this->hidden->activation_handler) {
        WASAPI_PlatformDeleteActivationHandler(_this->hidden->activation_handler);
        _this->hidden->activation_handler = NULL;
    }

    if (_this->hidden->event) {
        CloseHandle(_this->hidden->event);
        _this->hidden->event = NULL;
    }
}

static void WASAPI_CloseDevice(SDL_AudioDevice *_this)
{
    WASAPI_UnrefDevice(_this);
}

void WASAPI_RefDevice(SDL_AudioDevice *_this)
{
    SDL_AtomicIncRef(&_this->hidden->refcount);
}

void WASAPI_UnrefDevice(SDL_AudioDevice *_this)
{
    if (!SDL_AtomicDecRef(&_this->hidden->refcount)) {
        return;
    }

    /* actual closing happens here. */

    /* don't touch _this->hidden->task in here; it has to be reverted from
       our callback thread. We do that in WASAPI_ThreadDeinit().
       (likewise for _this->hidden->coinitialized). */
    ReleaseWasapiDevice(_this);
    SDL_free(_this->hidden->devid);
    SDL_free(_this->hidden);
}

/* This is called once a device is activated, possibly asynchronously. */
int WASAPI_PrepDevice(SDL_AudioDevice *_this, const SDL_bool updatestream)
{
    /* !!! FIXME: we could request an exclusive mode stream, which is lower latency;
       !!!  it will write into the kernel's audio buffer directly instead of
       !!!  shared memory that a user-mode mixer then writes to the kernel with
       !!!  everything else. Doing this means any other sound using this device will
       !!!  stop playing, including the user's MP3 player and system notification
       !!!  sounds. You'd probably need to release the device when the app isn't in
       !!!  the foreground, to be a good citizen of the system. It's doable, but it's
       !!!  more work and causes some annoyances, and I don't know what the latency
       !!!  wins actually look like. Maybe add a hint to force exclusive mode at
       !!!  some point. To be sure, defaulting to shared mode is the right thing to
       !!!  do in any case. */
    const SDL_AudioSpec oldspec = _this->spec;
    const AUDCLNT_SHAREMODE sharemode = AUDCLNT_SHAREMODE_SHARED;
    UINT32 bufsize = 0; /* this is in sample frames, not samples, not bytes. */
    REFERENCE_TIME default_period = 0;
    IAudioClient *client = _this->hidden->client;
    IAudioRenderClient *render = NULL;
    IAudioCaptureClient *capture = NULL;
    WAVEFORMATEX *waveformat = NULL;
    SDL_AudioFormat test_format;
    SDL_AudioFormat wasapi_format = 0;
    const SDL_AudioFormat *closefmts;
    HRESULT ret = S_OK;
    DWORD streamflags = 0;

    SDL_assert(client != NULL);

#if defined(__WINRT__) || defined(__GDK__) /* CreateEventEx() arrived in Vista, so we need an #ifdef for XP. */
    _this->hidden->event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
#else
    _this->hidden->event = CreateEventW(NULL, 0, 0, NULL);
#endif

    if (_this->hidden->event == NULL) {
        return WIN_SetError("WASAPI can't create an event handle");
    }

    ret = IAudioClient_GetMixFormat(client, &waveformat);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine mix format", ret);
    }

    SDL_assert(waveformat != NULL);
    _this->hidden->waveformat = waveformat;

    _this->spec.channels = (Uint8)waveformat->nChannels;

    /* Make sure we have a valid format that we can convert to whatever WASAPI wants. */
    wasapi_format = WaveFormatToSDLFormat(waveformat);

    closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == wasapi_format) {
            _this->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        return SDL_SetError("%s: Unsupported audio format", "wasapi");
    }

    ret = IAudioClient_GetDevicePeriod(client, &default_period, NULL);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine minimum device period", ret);
    }

    /* we've gotten reports that WASAPI's resampler introduces distortions, but in the short term
       it fixes some other WASAPI-specific quirks we haven't quite tracked down.
       Refer to bug #6326 for the immediate concern. */
#if 0
    _this->spec.freq = waveformat->nSamplesPerSec;  /* force sampling rate so our resampler kicks in, if necessary. */
#else
    /* favor WASAPI's resampler over our own */
    if ((DWORD)_this->spec.freq != waveformat->nSamplesPerSec) {
        streamflags |= (AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);
        waveformat->nSamplesPerSec = _this->spec.freq;
        waveformat->nAvgBytesPerSec = waveformat->nSamplesPerSec * waveformat->nChannels * (waveformat->wBitsPerSample / 8);
    }
#endif

    streamflags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    ret = IAudioClient_Initialize(client, sharemode, streamflags, 0, 0, waveformat, NULL);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't initialize audio client", ret);
    }

    ret = IAudioClient_SetEventHandle(client, _this->hidden->event);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't set event handle", ret);
    }

    ret = IAudioClient_GetBufferSize(client, &bufsize);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine buffer size", ret);
    }

    /* Match the callback size to the period size to cut down on the number of
       interrupts waited for in each call to WaitDevice */
    {
        const float period_millis = default_period / 10000.0f;
        const float period_frames = period_millis * _this->spec.freq / 1000.0f;
        _this->spec.samples = (Uint16)SDL_ceilf(period_frames);
    }

    /* Update the fragment size as size in bytes */
    SDL_CalculateAudioSpec(&_this->spec);

    _this->hidden->framesize = (SDL_AUDIO_BITSIZE(_this->spec.format) / 8) * _this->spec.channels;

    if (_this->iscapture) {
        _this->hidden->capturestream = SDL_CreateAudioStream(_this->spec.format, _this->spec.channels, _this->spec.freq, _this->spec.format, _this->spec.channels, _this->spec.freq);
        if (!_this->hidden->capturestream) {
            return -1; /* already set SDL_Error */
        }

        ret = IAudioClient_GetService(client, &SDL_IID_IAudioCaptureClient, (void **)&capture);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get capture client service", ret);
        }

        SDL_assert(capture != NULL);
        _this->hidden->capture = capture;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start capture", ret);
        }

        WASAPI_FlushCapture(_this); /* MSDN says you should flush capture endpoint right after startup. */
    } else {
        ret = IAudioClient_GetService(client, &SDL_IID_IAudioRenderClient, (void **)&render);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get render client service", ret);
        }

        SDL_assert(render != NULL);
        _this->hidden->render = render;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start playback", ret);
        }
    }

    if (updatestream) {
        return UpdateAudioStream(_this, &oldspec);
    }

    return 0; /* good to go. */
}

static int WASAPI_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    LPCWSTR devid = (LPCWSTR)_this->handle;

    /* Initialize all variables that we clean on shutdown */
    _this->hidden = (struct SDL_PrivateAudioData *) SDL_malloc(sizeof(*_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(_this->hidden);

    WASAPI_RefDevice(_this); /* so CloseDevice() will unref to zero. */

    if (!devid) { /* is default device? */
        _this->hidden->default_device_generation = SDL_AtomicGet(_this->iscapture ? &SDL_IMMDevice_DefaultCaptureGeneration : &SDL_IMMDevice_DefaultPlaybackGeneration);
    } else {
        _this->hidden->devid = SDL_wcsdup(devid);
        if (!_this->hidden->devid) {
            return SDL_OutOfMemory();
        }
    }

    if (WASAPI_ActivateDevice(_this, SDL_FALSE) == -1) {
        return -1; /* already set error. */
    }

    /* Ready, but waiting for async device activation.
       Until activation is successful, we will report silence from capture
       devices and ignore data on playback devices.
       Also, since we don't know the _actual_ device format until after
       activation, we let the app have whatever it asks for. We set up
       an SDL_AudioStream to convert, if necessary, once the activation
       completes. */

    return 0;
}

static void WASAPI_ThreadInit(SDL_AudioDevice *_this)
{
    WASAPI_PlatformThreadInit(_this);
}

static void WASAPI_ThreadDeinit(SDL_AudioDevice *_this)
{
    WASAPI_PlatformThreadDeinit(_this);
}

static void WASAPI_Deinitialize(void)
{
    WASAPI_PlatformDeinit();
}

static SDL_bool WASAPI_Init(SDL_AudioDriverImpl *impl)
{
    if (WASAPI_PlatformInit() == -1) {
        return SDL_FALSE;
    }

    /* Set the function pointers */
    impl->DetectDevices = WASAPI_DetectDevices;
    impl->ThreadInit = WASAPI_ThreadInit;
    impl->ThreadDeinit = WASAPI_ThreadDeinit;
    impl->OpenDevice = WASAPI_OpenDevice;
    impl->PlayDevice = WASAPI_PlayDevice;
    impl->WaitDevice = WASAPI_WaitDevice;
    impl->GetDeviceBuf = WASAPI_GetDeviceBuf;
    impl->CaptureFromDevice = WASAPI_CaptureFromDevice;
    impl->FlushCapture = WASAPI_FlushCapture;
    impl->CloseDevice = WASAPI_CloseDevice;
    impl->Deinitialize = WASAPI_Deinitialize;
    impl->GetDefaultAudioInfo = WASAPI_GetDefaultAudioInfo;
    impl->HasCaptureSupport = SDL_TRUE;
    impl->SupportsNonPow2Samples = SDL_TRUE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap WASAPI_bootstrap = {
    "wasapi", "WASAPI", WASAPI_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_WASAPI */
