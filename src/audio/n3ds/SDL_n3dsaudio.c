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

#ifdef SDL_AUDIO_DRIVER_N3DS

/* N3DS Audio driver */

#include "../SDL_sysaudio.h"
#include "SDL_n3dsaudio.h"

#define N3DSAUDIO_DRIVER_NAME "n3ds"

static dspHookCookie dsp_hook;
static SDL_AudioDevice *audio_device;

static void FreePrivateData(SDL_AudioDevice *_this);
static int FindAudioFormat(SDL_AudioDevice *_this);

static SDL_INLINE void contextLock(SDL_AudioDevice *_this)
{
    LightLock_Lock(&_this->hidden->lock);
}

static SDL_INLINE void contextUnlock(SDL_AudioDevice *_this)
{
    LightLock_Unlock(&_this->hidden->lock);
}

static void N3DSAUD_LockAudio(SDL_AudioDevice *_this)
{
    contextLock(_this);
}

static void N3DSAUD_UnlockAudio(SDL_AudioDevice *_this)
{
    contextUnlock(_this);
}

static void N3DSAUD_DspHook(DSP_HookType hook)
{
    if (hook == DSPHOOK_ONCANCEL) {
        contextLock(audio_device);
        audio_device->hidden->isCancelled = SDL_TRUE;
        SDL_AtomicSet(&audio_device->enabled, SDL_FALSE);
        CondVar_Broadcast(&audio_device->hidden->cv);
        contextUnlock(audio_device);
    }
}

static void AudioFrameFinished(void *device)
{
    bool shouldBroadcast = false;
    unsigned i;
    SDL_AudioDevice *_this = (SDL_AudioDevice *)device;

    contextLock(_this);

    for (i = 0; i < NUM_BUFFERS; i++) {
        if (_this->hidden->waveBuf[i].status == NDSP_WBUF_DONE) {
            _this->hidden->waveBuf[i].status = NDSP_WBUF_FREE;
            shouldBroadcast = SDL_TRUE;
        }
    }

    if (shouldBroadcast) {
        CondVar_Broadcast(&_this->hidden->cv);
    }

    contextUnlock(_this);
}

static int N3DSAUDIO_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    Result ndsp_init_res;
    Uint8 *data_vaddr;
    float mix[12];
    _this->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*_this->hidden));

    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    /* Initialise the DSP service */
    ndsp_init_res = ndspInit();
    if (R_FAILED(ndsp_init_res)) {
        if ((R_SUMMARY(ndsp_init_res) == RS_NOTFOUND) && (R_MODULE(ndsp_init_res) == RM_DSP)) {
            SDL_SetError("DSP init failed: dspfirm.cdc missing!");
        } else {
            SDL_SetError("DSP init failed. Error code: 0x%lX", ndsp_init_res);
        }
        return -1;
    }

    /* Initialise internal state */
    LightLock_Init(&_this->hidden->lock);
    CondVar_Init(&_this->hidden->cv);

    if (_this->spec.channels > 2) {
        _this->spec.channels = 2;
    }

    /* Should not happen but better be safe. */
    if (FindAudioFormat(_this) < 0) {
        return SDL_SetError("No supported audio format found.");
    }

    /* Update the fragment size as size in bytes */
    SDL_CalculateAudioSpec(&_this->spec);

    /* Allocate mixing buffer */
    if (_this->spec.size >= SDL_MAX_UINT32 / 2) {
        return SDL_SetError("Mixing buffer is too large.");
    }

    _this->hidden->mixlen = _this->spec.size;
    _this->hidden->mixbuf = (Uint8 *)SDL_malloc(_this->spec.size);
    if (_this->hidden->mixbuf == NULL) {
        return SDL_OutOfMemory();
    }

    SDL_memset(_this->hidden->mixbuf, _this->spec.silence, _this->spec.size);

    data_vaddr = (Uint8 *)linearAlloc(_this->hidden->mixlen * NUM_BUFFERS);
    if (data_vaddr == NULL) {
        return SDL_OutOfMemory();
    }

    SDL_memset(data_vaddr, 0, _this->hidden->mixlen * NUM_BUFFERS);
    DSP_FlushDataCache(data_vaddr, _this->hidden->mixlen * NUM_BUFFERS);

    _this->hidden->nextbuf = 0;
    _this->hidden->channels = _this->spec.channels;
    _this->hidden->samplerate = _this->spec.freq;

    ndspChnReset(0);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, _this->spec.freq);
    ndspChnSetFormat(0, _this->hidden->format);

    SDL_memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

    SDL_memset(_this->hidden->waveBuf, 0, sizeof(ndspWaveBuf) * NUM_BUFFERS);

    for (unsigned i = 0; i < NUM_BUFFERS; i++) {
        _this->hidden->waveBuf[i].data_vaddr = data_vaddr;
        _this->hidden->waveBuf[i].nsamples = _this->hidden->mixlen / _this->hidden->bytePerSample;
        data_vaddr += _this->hidden->mixlen;
    }

    /* Setup callback */
    audio_device = _this;
    ndspSetCallback(AudioFrameFinished, _this);
    dspHook(&dsp_hook, N3DSAUD_DspHook);

    return 0;
}

static int N3DSAUDIO_CaptureFromDevice(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    /* Delay to make this sort of simulate real audio input. */
    SDL_Delay((_this->spec.samples * 1000) / _this->spec.freq);

    /* always return a full buffer of silence. */
    SDL_memset(buffer, _this->spec.silence, buflen);
    return buflen;
}

static void N3DSAUDIO_PlayDevice(SDL_AudioDevice *_this)
{
    size_t nextbuf;
    size_t sampleLen;
    contextLock(_this);

    nextbuf = _this->hidden->nextbuf;
    sampleLen = _this->hidden->mixlen;

    if (_this->hidden->isCancelled ||
        _this->hidden->waveBuf[nextbuf].status != NDSP_WBUF_FREE) {
        contextUnlock(_this);
        return;
    }

    _this->hidden->nextbuf = (nextbuf + 1) % NUM_BUFFERS;

    contextUnlock(_this);

    SDL_memcpy((void *)_this->hidden->waveBuf[nextbuf].data_vaddr,
           _this->hidden->mixbuf, sampleLen);
    DSP_FlushDataCache(_this->hidden->waveBuf[nextbuf].data_vaddr, sampleLen);

    ndspChnWaveBufAdd(0, &_this->hidden->waveBuf[nextbuf]);
}

static void N3DSAUDIO_WaitDevice(SDL_AudioDevice *_this)
{
    contextLock(_this);
    while (!_this->hidden->isCancelled &&
           _this->hidden->waveBuf[_this->hidden->nextbuf].status != NDSP_WBUF_FREE) {
        CondVar_Wait(&_this->hidden->cv, &_this->hidden->lock);
    }
    contextUnlock(_this);
}

static Uint8 *N3DSAUDIO_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return _this->hidden->mixbuf;
}

static void N3DSAUDIO_CloseDevice(SDL_AudioDevice *_this)
{
    contextLock(_this);

    dspUnhook(&dsp_hook);
    ndspSetCallback(NULL, NULL);

    if (!_this->hidden->isCancelled) {
        ndspChnReset(0);
        SDL_memset(_this->hidden->waveBuf, 0, sizeof(ndspWaveBuf) * NUM_BUFFERS);
        CondVar_Broadcast(&_this->hidden->cv);
    }

    contextUnlock(_this);

    ndspExit();

    FreePrivateData(_this);
}

static void N3DSAUDIO_ThreadInit(SDL_AudioDevice *_this)
{
    s32 current_priority;
    svcGetThreadPriority(&current_priority, CUR_THREAD_HANDLE);
    current_priority--;
    /* 0x18 is reserved for video, 0x30 is the default for main thread */
    current_priority = SDL_clamp(current_priority, 0x19, 0x2F);
    svcSetThreadPriority(CUR_THREAD_HANDLE, current_priority);
}

static SDL_bool N3DSAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    impl->OpenDevice = N3DSAUDIO_OpenDevice;
    impl->PlayDevice = N3DSAUDIO_PlayDevice;
    impl->WaitDevice = N3DSAUDIO_WaitDevice;
    impl->GetDeviceBuf = N3DSAUDIO_GetDeviceBuf;
    impl->CloseDevice = N3DSAUDIO_CloseDevice;
    impl->ThreadInit = N3DSAUDIO_ThreadInit;
    impl->LockDevice = N3DSAUD_LockAudio;
    impl->UnlockDevice = N3DSAUD_UnlockAudio;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;

    /* Should be possible, but micInit would fail */
    impl->HasCaptureSupport = SDL_FALSE;
    impl->CaptureFromDevice = N3DSAUDIO_CaptureFromDevice;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap N3DSAUDIO_bootstrap = {
    N3DSAUDIO_DRIVER_NAME,
    "SDL N3DS audio driver",
    N3DSAUDIO_Init,
    0
};

/**
 * Cleans up all allocated memory, safe to call with null pointers
 */
static void FreePrivateData(SDL_AudioDevice *_this)
{
    if (!_this->hidden) {
        return;
    }

    if (_this->hidden->waveBuf[0].data_vaddr) {
        linearFree((void *)_this->hidden->waveBuf[0].data_vaddr);
    }

    if (_this->hidden->mixbuf) {
        SDL_free(_this->hidden->mixbuf);
        _this->hidden->mixbuf = NULL;
    }

    SDL_free(_this->hidden);
    _this->hidden = NULL;
}

static int FindAudioFormat(SDL_AudioDevice *_this)
{
    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        _this->spec.format = test_format;
        switch (test_format) {
        case SDL_AUDIO_S8:
            /* Signed 8-bit audio supported */
            _this->hidden->format = (_this->spec.channels == 2) ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_MONO_PCM8;
            _this->hidden->isSigned = 1;
            _this->hidden->bytePerSample = _this->spec.channels;
            return 0;
        case SDL_AUDIO_S16:
            /* Signed 16-bit audio supported */
            _this->hidden->format = (_this->spec.channels == 2) ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16;
            _this->hidden->isSigned = 1;
            _this->hidden->bytePerSample = _this->spec.channels * 2;
            return 0;
        }
    }

    return -1;
}

#endif /* SDL_AUDIO_DRIVER_N3DS */
