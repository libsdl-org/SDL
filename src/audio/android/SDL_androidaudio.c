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

#ifdef SDL_AUDIO_DRIVER_ANDROID

/* Output audio to Android */

#include "../SDL_sysaudio.h"
#include "../SDL_audio_c.h"
#include "SDL_androidaudio.h"

#include "../../core/android/SDL_android.h"
#include <android/log.h>


struct SDL_PrivateAudioData
{
    /* Resume device if it was paused automatically */
    int resume;
};

static SDL_AudioDevice *audioDevice = NULL;
static SDL_AudioDevice *captureDevice = NULL;


static int ANDROIDAUDIO_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts;
    SDL_bool iscapture = _this->iscapture;

    if (iscapture) {
        if (captureDevice) {
            return SDL_SetError("An audio capture device is already opened");
        }
    }

    if (!iscapture) {
        if (audioDevice) {
            return SDL_SetError("An audio playback device is already opened");
        }
    }

    if (iscapture) {
        captureDevice = _this;
    } else {
        audioDevice = _this;
    }

    _this->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if ((test_format == SDL_AUDIO_U8) ||
            (test_format == SDL_AUDIO_S16) ||
            (test_format == SDL_AUDIO_F32)) {
            _this->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        /* Didn't find a compatible format :( */
        return SDL_SetError("%s: Unsupported audio format", "android");
    }

    {
        int audio_device_id = 0;
        if (devname != NULL) {
            audio_device_id = SDL_atoi(devname);
        }
        if (Android_JNI_OpenAudioDevice(iscapture, audio_device_id, &_this->spec) < 0) {
            return -1;
        }
    }

    SDL_CalculateAudioSpec(&_this->spec);

    return 0;
}

static void ANDROIDAUDIO_PlayDevice(SDL_AudioDevice *_this)
{
    Android_JNI_WriteAudioBuffer();
}

static Uint8 *ANDROIDAUDIO_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return Android_JNI_GetAudioBuffer();
}

static int ANDROIDAUDIO_CaptureFromDevice(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    return Android_JNI_CaptureAudioBuffer(buffer, buflen);
}

static void ANDROIDAUDIO_FlushCapture(SDL_AudioDevice *_this)
{
    Android_JNI_FlushCapturedAudio();
}

static void ANDROIDAUDIO_CloseDevice(SDL_AudioDevice *_this)
{
    /* At this point SDL_CloseAudioDevice via close_audio_device took care of terminating the audio thread
       so it's safe to terminate the Java side buffer and AudioTrack
     */
    Android_JNI_CloseAudioDevice(_this->iscapture);
    if (_this->iscapture) {
        SDL_assert(captureDevice == _this);
        captureDevice = NULL;
    } else {
        SDL_assert(audioDevice == _this);
        audioDevice = NULL;
    }
    SDL_free(_this->hidden);
}

static SDL_bool ANDROIDAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->DetectDevices = Android_DetectDevices;
    impl->OpenDevice = ANDROIDAUDIO_OpenDevice;
    impl->PlayDevice = ANDROIDAUDIO_PlayDevice;
    impl->GetDeviceBuf = ANDROIDAUDIO_GetDeviceBuf;
    impl->CloseDevice = ANDROIDAUDIO_CloseDevice;
    impl->CaptureFromDevice = ANDROIDAUDIO_CaptureFromDevice;
    impl->FlushCapture = ANDROIDAUDIO_FlushCapture;
    impl->AllowsArbitraryDeviceNames = SDL_TRUE;

    /* and the capabilities */
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultOutputDevice = SDL_FALSE;
    impl->OnlyHasDefaultCaptureDevice = SDL_FALSE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap ANDROIDAUDIO_bootstrap = {
    "android", "SDL Android audio driver", ANDROIDAUDIO_Init, SDL_FALSE
};

/* Pause (block) all non already paused audio devices by taking their mixer lock */
void ANDROIDAUDIO_PauseDevices(void)
{
    /* TODO: Handle multiple devices? */
    struct SDL_PrivateAudioData *private;
    if (audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *)audioDevice->hidden;
        if (SDL_AtomicGet(&audioDevice->paused)) {
            /* The device is already paused, leave it alone */
            private->resume = SDL_FALSE;
        } else {
            SDL_LockMutex(audioDevice->mixer_lock);
            SDL_AtomicSet(&audioDevice->paused, 1);
            private->resume = SDL_TRUE;
        }
    }

    if (captureDevice != NULL && captureDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *)captureDevice->hidden;
        if (SDL_AtomicGet(&captureDevice->paused)) {
            /* The device is already paused, leave it alone */
            private->resume = SDL_FALSE;
        } else {
            SDL_LockMutex(captureDevice->mixer_lock);
            SDL_AtomicSet(&captureDevice->paused, 1);
            private->resume = SDL_TRUE;
        }
    }
}

/* Resume (unblock) all non already paused audio devices by releasing their mixer lock */
void ANDROIDAUDIO_ResumeDevices(void)
{
    /* TODO: Handle multiple devices? */
    struct SDL_PrivateAudioData *private;
    if (audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *)audioDevice->hidden;
        if (private->resume) {
            SDL_AtomicSet(&audioDevice->paused, 0);
            private->resume = SDL_FALSE;
            SDL_UnlockMutex(audioDevice->mixer_lock);
        }
    }

    if (captureDevice != NULL && captureDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *)captureDevice->hidden;
        if (private->resume) {
            SDL_AtomicSet(&captureDevice->paused, 0);
            private->resume = SDL_FALSE;
            SDL_UnlockMutex(captureDevice->mixer_lock);
        }
    }
}

#endif /* SDL_AUDIO_DRIVER_ANDROID */
