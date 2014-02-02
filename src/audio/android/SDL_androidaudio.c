/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_ANDROID

/* Output audio to Android */

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_androidaudio.h"

#include "../../core/android/SDL_android.h"

#include <android/log.h>

static void * audioDevice;

static int
AndroidAUD_OpenDevice(_THIS, const char *devname, int iscapture)
{
    SDL_AudioFormat test_format;

    if (iscapture) {
        /* TODO: implement capture */
        return SDL_SetError("Capture not supported on Android");
    }

    if (audioDevice != NULL) {
        return SDL_SetError("Only one audio device at a time please!");
    }

    audioDevice = this;

    test_format = SDL_FirstAudioFormat(this->spec.format);
    while (test_format != 0) { /* no "UNKNOWN" constant */
        if ((test_format == AUDIO_U8) || (test_format == AUDIO_S16LSB)) {
            this->spec.format = test_format;
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (test_format == 0) {
        /* Didn't find a compatible format :( */
        return SDL_SetError("No compatible audio format!");
    }

    if (this->spec.channels > 1) {
        this->spec.channels = 2;
    } else {
        this->spec.channels = 1;
    }

    if (this->spec.freq < 8000) {
        this->spec.freq = 8000;
    }
    if (this->spec.freq > 48000) {
        this->spec.freq = 48000;
    }

    /* TODO: pass in/return a (Java) device ID, also whether we're opening for input or output */
    this->spec.samples = Android_JNI_OpenAudioDevice(this->spec.freq, this->spec.format == AUDIO_U8 ? 0 : 1, this->spec.channels, this->spec.samples);
    SDL_CalculateAudioSpec(&this->spec);

    if (this->spec.samples == 0) {
        /* Init failed? */
        return SDL_SetError("Java-side initialization failed!");
    }

    return 0;
}

static void
AndroidAUD_PlayDevice(_THIS)
{
    Android_JNI_WriteAudioBuffer();
}

static Uint8 *
AndroidAUD_GetDeviceBuf(_THIS)
{
    return Android_JNI_GetAudioBuffer();
}

static void
AndroidAUD_CloseDevice(_THIS)
{
    /* At this point SDL_CloseAudioDevice via close_audio_device took care of terminating the audio thread
       so it's safe to terminate the Java side buffer and AudioTrack
     */
    Android_JNI_CloseAudioDevice();

    if (audioDevice == this) {
        audioDevice = NULL;
    }
}

static int
AndroidAUD_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = AndroidAUD_OpenDevice;
    impl->PlayDevice = AndroidAUD_PlayDevice;
    impl->GetDeviceBuf = AndroidAUD_GetDeviceBuf;
    impl->CloseDevice = AndroidAUD_CloseDevice;

    /* and the capabilities */
    impl->HasCaptureSupport = 0; /* TODO */
    impl->OnlyHasDefaultOutputDevice = 1;
    impl->OnlyHasDefaultInputDevice = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap ANDROIDAUD_bootstrap = {
    "android", "SDL Android audio driver", AndroidAUD_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */

