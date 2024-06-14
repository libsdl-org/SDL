/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

// Output audio to Android (legacy interface)

#include "../SDL_sysaudio.h"
#include "SDL_androidaudio.h"

#include "../../core/android/SDL_android.h"
#include <android/log.h>


struct SDL_PrivateAudioData
{
    int resume;  // Resume device if it was paused automatically
};

static SDL_AudioDevice *playbackDevice = NULL;
static SDL_AudioDevice *recordingDevice = NULL;

static int ANDROIDAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    }

    const SDL_bool recording = device->recording;

    if (recording) {
        if (recordingDevice) {
            return SDL_SetError("An audio recording device is already opened");
        }
        recordingDevice = device;
    } else {
        if (playbackDevice) {
            return SDL_SetError("An audio playback device is already opened");
        }
        playbackDevice = device;
    }

    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if ((test_format == SDL_AUDIO_U8) ||
            (test_format == SDL_AUDIO_S16) ||
            (test_format == SDL_AUDIO_F32)) {
            device->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        return SDL_SetError("android: Unsupported audio format");
    }

    if (Android_JNI_OpenAudioDevice(device) < 0) {
        return -1;
    }

    SDL_UpdatedAudioDeviceFormat(device);

    return 0;
}

// !!! FIXME: this needs a WaitDevice implementation.

static int ANDROIDAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    Android_JNI_WriteAudioBuffer();
    return 0;
}

static Uint8 *ANDROIDAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return Android_JNI_GetAudioBuffer();
}

static int ANDROIDAUDIO_RecordDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    return Android_JNI_RecordAudioBuffer(buffer, buflen);
}

static void ANDROIDAUDIO_FlushRecording(SDL_AudioDevice *device)
{
    Android_JNI_FlushRecordedAudio();
}

static void ANDROIDAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    /* At this point SDL_CloseAudioDevice via close_audio_device took care of terminating the audio thread
       so it's safe to terminate the Java side buffer and AudioTrack
     */
    if (device->hidden) {
        Android_JNI_CloseAudioDevice(device->recording);
        if (device->recording) {
            SDL_assert(recordingDevice == device);
            recordingDevice = NULL;
        } else {
            SDL_assert(playbackDevice == device);
            playbackDevice = NULL;
        }
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

// Pause (block) all non already paused audio devices by taking their mixer lock
void ANDROIDAUDIO_PauseDevices(void)
{
    // TODO: Handle multiple devices?
    struct SDL_PrivateAudioData *hidden;
    if (playbackDevice && playbackDevice->hidden) {
        hidden = (struct SDL_PrivateAudioData *)playbackDevice->hidden;
        SDL_LockMutex(playbackDevice->lock);
        hidden->resume = SDL_TRUE;
    }

    if (recordingDevice && recordingDevice->hidden) {
        hidden = (struct SDL_PrivateAudioData *)recordingDevice->hidden;
        SDL_LockMutex(recordingDevice->lock);
        hidden->resume = SDL_TRUE;
    }
}

// Resume (unblock) all non already paused audio devices by releasing their mixer lock
void ANDROIDAUDIO_ResumeDevices(void)
{
    // TODO: Handle multiple devices?
    struct SDL_PrivateAudioData *hidden;
    if (playbackDevice && playbackDevice->hidden) {
        hidden = (struct SDL_PrivateAudioData *)playbackDevice->hidden;
        if (hidden->resume) {
            hidden->resume = SDL_FALSE;
            SDL_UnlockMutex(playbackDevice->lock);
        }
    }

    if (recordingDevice && recordingDevice->hidden) {
        hidden = (struct SDL_PrivateAudioData *)recordingDevice->hidden;
        if (hidden->resume) {
            hidden->resume = SDL_FALSE;
            SDL_UnlockMutex(recordingDevice->lock);
        }
    }
}

static SDL_bool ANDROIDAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    // !!! FIXME: if on Android API < 24, DetectDevices and Deinitialize should be NULL and OnlyHasDefaultPlaybackDevice and OnlyHasDefaultRecordingDevice should be SDL_TRUE, since audio device enum and hotplug appears to require Android 7.0+.
    impl->ThreadInit = Android_AudioThreadInit;
    impl->DetectDevices = Android_StartAudioHotplug;
    impl->DeinitializeStart = Android_StopAudioHotplug;
    impl->OpenDevice = ANDROIDAUDIO_OpenDevice;
    impl->PlayDevice = ANDROIDAUDIO_PlayDevice;
    impl->GetDeviceBuf = ANDROIDAUDIO_GetDeviceBuf;
    impl->CloseDevice = ANDROIDAUDIO_CloseDevice;
    impl->RecordDevice = ANDROIDAUDIO_RecordDevice;
    impl->FlushRecording = ANDROIDAUDIO_FlushRecording;

    impl->HasRecordingSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap ANDROIDAUDIO_bootstrap = {
    "android", "SDL Android audio driver", ANDROIDAUDIO_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_ANDROID
