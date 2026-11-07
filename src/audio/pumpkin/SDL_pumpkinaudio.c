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

#ifdef SDL_AUDIO_DRIVER_PUMPKINOS

/* PumpkinOS audio driver.

   Playback goes through the PalmOS sound stream API (SndStreamCreate),
   which PumpkinOS implements on top of its audio provider. The provider
   pulls audio from a callback on its own thread; SDL pushes audio from its
   audio thread, so the two meet in a ring buffer.

   The PalmOS calls must be made from the task thread: SDL_OpenAudioDevice()
   and SDL_CloseAudioDevice() therefore have to be called from the thread
   that runs PilotMain() (the main thread of the application).

   Recording is not available in PumpkinOS. */

#include "../SDL_sysaudio.h"
#include "SDL_pumpkinaudio.h"
#include "../../core/pumpkin/SDL_pumpkin.h"

#define PUMPKIN_AUDIO_DRIVER_NAME "pumpkinos"

// ring buffer capacity, in SDL device buffers
#define PUMPKIN_RING_BUFFERS 4

// Called by the PumpkinOS audio thread when the provider needs more audio.
static Err PUMPKIN_StreamCallback(void *userdata, SndStreamRef channel, void *buffer, UInt32 numframes)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)userdata;
    struct SDL_PrivateAudioData *h = device->hidden;
    Uint8 *out = (Uint8 *)buffer;
    const int want = (int)numframes * SDL_AUDIO_FRAMESIZE(device->spec);
    int avail, first;

    (void)channel;

    SDL_LockMutex(h->lock);
    avail = SDL_min(h->ring_fill, want);
    if (avail > 0) {
        first = SDL_min(avail, h->ring_size - h->ring_read);
        SDL_memcpy(out, h->ring + h->ring_read, first);
        if (avail > first) {
            SDL_memcpy(out + first, h->ring, avail - first);
        }
        h->ring_read = (h->ring_read + avail) % h->ring_size;
        h->ring_fill -= avail;
    }
    SDL_SignalCondition(h->cond);
    SDL_UnlockMutex(h->lock);

    if (avail < want) {
        SDL_memset(out + avail, device->silence_value, want - avail);
    }
    return errNone;
}

static bool PUMPKIN_OpenDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h;
    const SDL_AudioFormat *closefmts;
    SDL_AudioFormat test_format;
    SndSampleType type = sndInt16;
    SndStreamRef channel = 0;
    Err err;

    if (device->recording) {
        return SDL_SetError("PumpkinOS: audio recording is not supported");
    }

    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return false;
    }
    device->hidden = h;

    closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == SDL_AUDIO_S16) {
            type = sndInt16;
            break;
        } else if (test_format == SDL_AUDIO_U8) {
            type = sndUInt8;
            break;
        } else if (test_format == SDL_AUDIO_S32) {
            type = sndInt32;
            break;
        }
    }
    if (!test_format) {
        return SDL_SetError("PumpkinOS: no supported audio format found");
    }
    device->spec.format = test_format;
    if (device->spec.channels > 2) {
        device->spec.channels = 2;
    } else if (device->spec.channels < 1) {
        device->spec.channels = 1;
    }
    SDL_UpdatedAudioDeviceFormat(device);

    h->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
    h->ring_size = device->buffer_size * PUMPKIN_RING_BUFFERS;
    h->ring = (Uint8 *)SDL_malloc(h->ring_size);
    h->lock = SDL_CreateMutex();
    h->cond = SDL_CreateCondition();
    if (!h->mixbuf || !h->ring || !h->lock || !h->cond) {
        return false;
    }
    SDL_memset(h->mixbuf, device->silence_value, device->buffer_size);
    h->io_delay = (device->sample_frames * 1000) / device->spec.freq;

    if (!pumpkin_sound_enabled()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "PumpkinOS: sound is disabled, audio will be discarded");
        h->silent = true;
        return true;
    }

    err = SndStreamCreate(&channel, sndOutput, device->spec.freq, type,
                          device->spec.channels == 2 ? sndStereo : sndMono,
                          PUMPKIN_StreamCallback, device, device->sample_frames, true);
    if (err != errNone) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "PumpkinOS: SndStreamCreate failed (%d), audio will be discarded", (int)err);
        h->silent = true;
        return true;
    }
    h->channel = channel;
    h->have_channel = true;

    err = SndStreamStart(channel);
    if (err != errNone) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "PumpkinOS: SndStreamStart failed (%d), audio will be discarded", (int)err);
        h->silent = true;
        return true;
    }
    return true;
}

static void PUMPKIN_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h = device->hidden;

    if (!h) {
        return;
    }
    if (h->have_channel) {
        SndStreamStop(h->channel);
        SndStreamDelete(h->channel);
        h->have_channel = false;
    }
    if (h->cond) {
        SDL_DestroyCondition(h->cond);
    }
    if (h->lock) {
        SDL_DestroyMutex(h->lock);
    }
    SDL_free(h->ring);
    SDL_free(h->mixbuf);
    SDL_free(h);
    device->hidden = NULL;
}

static bool PUMPKIN_WaitDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h = device->hidden;

    if (h->silent) {
        SDL_Delay(h->io_delay);
        return true;
    }

    SDL_LockMutex(h->lock);
    while ((h->ring_size - h->ring_fill) < device->buffer_size && !SDL_GetAtomicInt(&device->shutdown)) {
        SDL_WaitConditionTimeout(h->cond, h->lock, 100);
    }
    SDL_UnlockMutex(h->lock);
    return true;
}

static bool PUMPKIN_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    int room, n, pos, first;

    if (h->silent) {
        return true;
    }

    SDL_LockMutex(h->lock);
    room = h->ring_size - h->ring_fill;
    n = SDL_min(buflen, room); // WaitDevice made room; anything beyond is dropped
    if (n > 0) {
        pos = (h->ring_read + h->ring_fill) % h->ring_size;
        first = SDL_min(n, h->ring_size - pos);
        SDL_memcpy(h->ring + pos, buffer, first);
        if (n > first) {
            SDL_memcpy(h->ring, buffer + first, n - first);
        }
        h->ring_fill += n;
    }
    SDL_UnlockMutex(h->lock);
    return true;
}

static Uint8 *PUMPKIN_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->mixbuf;
}

static bool PUMPKIN_AUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = PUMPKIN_OpenDevice;
    impl->CloseDevice = PUMPKIN_CloseDevice;
    impl->WaitDevice = PUMPKIN_WaitDevice;
    impl->PlayDevice = PUMPKIN_PlayDevice;
    impl->GetDeviceBuf = PUMPKIN_GetDeviceBuf;

    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->HasRecordingSupport = false;

    return true;
}

AudioBootStrap PUMPKINAUDIO_bootstrap = {
    PUMPKIN_AUDIO_DRIVER_NAME, "PumpkinOS audio driver", PUMPKIN_AUDIO_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_PUMPKINOS
