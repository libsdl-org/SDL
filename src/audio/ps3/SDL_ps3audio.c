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

#ifdef SDL_AUDIO_DRIVER_PS3

#include "../SDL_audiodev_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_ps3audio.h"
#include <lv2/thread.h>
#include <sys/thread.h>

static bool PS3AUDIO_OpenDevice(SDL_AudioDevice *device)
{
    const SDL_AudioFormat *closefmts;
    SDL_AudioFormat test_format;
    
    device->hidden = (struct SDL_PrivateAudioData *)
        SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        SDL_OutOfMemory();
        return false;
    }

    // PS3 Libaudio only handles floats
    closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (test_format == SDL_AUDIO_F32BE) {
            device->spec.format = test_format;
            break;
        }
    }

    if (!test_format) {
        return SDL_SetError("Unsupported audio format");
    }

    int ret = audioInit();

    // either 2 or 8 channel
    device->hidden->params.numChannels = AUDIO_PORT_2CH;
    // 8 16 or 32 block buffer
    device->hidden->params.numBlocks = AUDIO_BLOCK_32;
    // extended attributes
    device->hidden->params.attrib = 0;
    // sound level (1 is default)
    device->hidden->params.level = 1;

    ret = audioPortOpen(&device->hidden->params, &device->hidden->portNum);

    ret = audioGetPortConfig(device->hidden->portNum, &device->hidden->config);

    // create the event queue here — just creation, no thread binding yet
    if (audioCreateNotifyEventQueue(&device->hidden->snd_queue, &device->hidden->snd_queue_key) != 0) {
        return SDL_SetError("PS3AUDIO: failed to create notify event queue");
    }
    
    ret = audioPortStart(device->hidden->portNum);

    device->hidden->last_filled_buf = 0;
    device->hidden->next_buffer = 0;

    device->spec.format = test_format;
    device->spec.freq = 48000;
    device->spec.channels = device->hidden->config.channelCount;

    // PS3 audio block is always 256 samples
    device->sample_frames = 256;

    /* Allocate the mixing buffer.  Its size and starting address must
    be a multiple of 64 bytes.  Our sample count is already a multiple of
    64, so spec->size should be a multiple of 64 as well. */
    const int mixlen = device->buffer_size * NUM_BUFFERS;
    device->hidden->rawbuf = (Uint8 *)SDL_aligned_alloc(64, mixlen);
    if (!device->hidden->rawbuf) {
        return SDL_SetError("Couldn't allocate mixing buffer");
    }

    SDL_memset(device->hidden->rawbuf, device->silence_value, mixlen);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        device->hidden->mixbufs[i] = &device->hidden->rawbuf[i * device->buffer_size];
    }

    return ret == 0;
}

static bool PS3AUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hwdata = (struct SDL_PrivateAudioData *)device->hidden;

    u32 block_index = hwdata->last_filled_buf % hwdata->params.numBlocks;

    float *dst = (float *)hwdata->config.audioDataStart
                 + block_index * device->sample_frames * hwdata->config.channelCount;

    SDL_memcpy(dst, buffer, buflen);

    hwdata->last_filled_buf = (hwdata->last_filled_buf + 1) % hwdata->params.numBlocks;

    return true;
}

static void PS3AUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        audioPortStop(device->hidden->portNum);
        audioRemoveNotifyEventQueue(device->hidden->snd_queue_key);
        audioPortClose(device->hidden->portNum);
        sysEventQueueDestroy(device->hidden->snd_queue, 0);
        audioQuit();

        if (device->hidden->rawbuf) {
            SDL_aligned_free(device->hidden->rawbuf);
            device->hidden->rawbuf = NULL;
        }
        SDL_free(device->hidden);
    }
}

static Uint8 *PS3AUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    struct SDL_PrivateAudioData *hwdata = (struct SDL_PrivateAudioData *)device->hidden;

    *buffer_size = device->buffer_size;

    Uint8 *buf = hwdata->mixbufs[hwdata->next_buffer];
    hwdata->next_buffer = (hwdata->next_buffer + 1) % NUM_BUFFERS;

    return buf;
}

static bool PS3AUDIO_WaitDevice(SDL_AudioDevice *device)
{
    sys_event_t event;
    sysEventQueueReceive( device->hidden->snd_queue, &event, UINT64_MAX);

    return true;
}

static void PS3AUDIO_ThreadInit(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hwdata = (struct SDL_PrivateAudioData *)device->hidden;

    // raise audio thread priority
    // sys_ppu_thread_t tid;
    // sysThreadGetId(&tid);
    // lower number = higher priority on PS3
    // sysThreadSetPriority(tid, 100);

    // bind the event queue to the audio port from within the audio thread
    audioSetNotifyEventQueue(hwdata->snd_queue_key);
}

bool PS3AUDIO_Init(SDL_AudioDriverImpl * impl)
{
    impl->OpenDevice = PS3AUDIO_OpenDevice;
    impl->PlayDevice = PS3AUDIO_PlayDevice;
    impl->WaitDevice = PS3AUDIO_WaitDevice;
    impl->CloseDevice = PS3AUDIO_CloseDevice;
    impl->GetDeviceBuf = PS3AUDIO_GetDeviceBuf;
    impl->ThreadInit = PS3AUDIO_ThreadInit;
    impl->OnlyHasDefaultPlaybackDevice = 1;

    return true;
}

AudioBootStrap PS3AUDIO_bootstrap = {
    "ps3", "PS3 audio driver", PS3AUDIO_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_PS3

/* vi: set ts=4 sw=4 expandtab: */
