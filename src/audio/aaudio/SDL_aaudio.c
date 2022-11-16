/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_AUDIO_DRIVER_AAUDIO

#include "SDL_audio.h"
#include "SDL_loadso.h"
#include "../SDL_audio_c.h"
#include "../../core/android/SDL_android.h"
#include "SDL_aaudio.h"

/* Debug */
#if 0
#  define LOGI(...) SDL_Log(__VA_ARGS__);
#else
#  define LOGI(...)
#endif

typedef struct AAUDIO_Data
{
    AAudioStreamBuilder *builder;
    void *handle;
#define SDL_PROC(ret,func,params) ret (*func) params;
#  include "SDL_aaudiofuncs.h"
#undef SDL_PROC
} AAUDIO_Data;
static AAUDIO_Data ctx;

static SDL_AudioDevice *audioDevice = NULL;
static SDL_AudioDevice *captureDevice = NULL;

static int aaudio_LoadFunctions(AAUDIO_Data *data)
{
#define SDL_PROC(ret,func,params)                                                                   \
    do {                                                                                            \
        data->func = SDL_LoadFunction(data->handle, #func);                                         \
        if (! data->func) {                                                                         \
            return SDL_SetError("Couldn't load AAUDIO function %s: %s", #func, SDL_GetError());     \
        }                                                                                           \
    } while (0);
#include "SDL_aaudiofuncs.h"
#undef SDL_PROC
    return 0;
}

void aaudio_errorCallback( AAudioStream *stream, void *userData, aaudio_result_t error );
void aaudio_errorCallback( AAudioStream *stream, void *userData, aaudio_result_t error )
{
    LOGI( "SDL aaudio_errorCallback: %d - %s", error, ctx.AAudio_convertResultToText( error ) );
}

#define LIB_AAUDIO_SO "libaaudio.so"

static int
aaudio_OpenDevice(_THIS, const char *devname)
{
    struct SDL_PrivateAudioData *private;
    SDL_bool iscapture = this->iscapture;
    aaudio_result_t res;
    LOGI(__func__);

    SDL_assert((captureDevice == NULL) || !iscapture);
    SDL_assert((audioDevice == NULL) || iscapture);

    if (iscapture) {
        if (!Android_JNI_RequestPermission("android.permission.RECORD_AUDIO")) {
            LOGI("This app doesn't have RECORD_AUDIO permission");
            return SDL_SetError("This app doesn't have RECORD_AUDIO permission");
        }
    }

    if (iscapture) {
        captureDevice = this;
    } else {
        audioDevice = this;
    }

    this->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, (sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    private = this->hidden;

    ctx.AAudioStreamBuilder_setSampleRate(ctx.builder, this->spec.freq);
    ctx.AAudioStreamBuilder_setChannelCount(ctx.builder, this->spec.channels);
    {
        aaudio_direction_t direction = (iscapture ? AAUDIO_DIRECTION_INPUT : AAUDIO_DIRECTION_OUTPUT);
        ctx.AAudioStreamBuilder_setDirection(ctx.builder, direction);
    }
    {
        aaudio_format_t format = AAUDIO_FORMAT_PCM_FLOAT;
        if (this->spec.format == AUDIO_S16SYS) {
            format = AAUDIO_FORMAT_PCM_I16;
        } else if (this->spec.format == AUDIO_S16SYS) {
            format = AAUDIO_FORMAT_PCM_FLOAT;
        }
        ctx.AAudioStreamBuilder_setFormat(ctx.builder, format);
    }

    ctx.AAudioStreamBuilder_setErrorCallback( ctx.builder, aaudio_errorCallback, private );

    LOGI("AAudio Try to open %u hz %u bit chan %u %s samples %u",
          this->spec.freq, SDL_AUDIO_BITSIZE(this->spec.format),
          this->spec.channels, (this->spec.format & 0x1000) ? "BE" : "LE", this->spec.samples);

    res = ctx.AAudioStreamBuilder_openStream(ctx.builder, &private->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStreamBuilder_openStream %d", res);
        return SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
    }

    this->spec.freq = ctx.AAudioStream_getSampleRate(private->stream);
    this->spec.channels = ctx.AAudioStream_getChannelCount(private->stream);
    {
        aaudio_format_t fmt = ctx.AAudioStream_getFormat(private->stream);
        if (fmt == AAUDIO_FORMAT_PCM_I16) {
            this->spec.format = AUDIO_S16SYS;
        } else if (fmt == AAUDIO_FORMAT_PCM_FLOAT) {
            this->spec.format = AUDIO_F32SYS;
        }
    }

    LOGI("AAudio Try to open %u hz %u bit chan %u %s samples %u",
          this->spec.freq, SDL_AUDIO_BITSIZE(this->spec.format),
          this->spec.channels, (this->spec.format & 0x1000) ? "BE" : "LE", this->spec.samples);

    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate mixing buffer */
    if (!iscapture) {
        private->mixlen = this->spec.size;
        private->mixbuf = (Uint8 *) SDL_malloc(private->mixlen);
        if (private->mixbuf == NULL) {
            return SDL_OutOfMemory();
        }
        SDL_memset(private->mixbuf, this->spec.silence, this->spec.size);
    }

    private->frame_size = this->spec.channels * (SDL_AUDIO_BITSIZE(this->spec.format) / 8);

    res = ctx.AAudioStream_requestStart(private->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStream_requestStart %d iscapture:%d", res, iscapture);
        return SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
    }

    LOGI("SDL AAudioStream_requestStart OK");
    return 0;
}

static void
aaudio_CloseDevice(_THIS)
{
    struct SDL_PrivateAudioData *private = this->hidden;
    aaudio_result_t res;
    LOGI(__func__);

    if (private->stream) {
        res = ctx.AAudioStream_requestStop(private->stream);
        if (res != AAUDIO_OK) {
            LOGI("SDL Failed AAudioStream_requestStop %d", res);
            SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            return;
        }

        res = ctx.AAudioStream_close(private->stream);
        if (res != AAUDIO_OK) {
            LOGI("SDL Failed AAudioStreamBuilder_delete %d", res);
            SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            return;
        }
    }

    if (this->iscapture) {
        SDL_assert(captureDevice == this);
        captureDevice = NULL;
    } else {
        SDL_assert(audioDevice == this);
        audioDevice = NULL;
    }

    SDL_free(this->hidden->mixbuf);
    SDL_free(this->hidden);
}

static Uint8 *
aaudio_GetDeviceBuf(_THIS)
{
    struct SDL_PrivateAudioData *private = this->hidden;
    return private->mixbuf;
}

static void
aaudio_PlayDevice(_THIS)
{
    struct SDL_PrivateAudioData *private = this->hidden;
    aaudio_result_t res;
    int64_t timeoutNanoseconds = 1 * 1000 * 1000; /* 8 ms */
    res = ctx.AAudioStream_write(private->stream,  private->mixbuf, private->mixlen / private->frame_size, timeoutNanoseconds);
    if (res < 0) {
        LOGI("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
    } else {
        LOGI("SDL AAudio play: %d frames, wanted:%d frames", (int)res, private->mixlen / private->frame_size);
    }

#if 0
    /* Log under-run count */
    {
        static int prev = 0;
        int32_t cnt = ctx.AAudioStream_getXRunCount(private->stream);
        if (cnt != prev) {
            SDL_Log("AAudio underrun: %d - total: %d", cnt - prev, cnt);
            prev = cnt;
        }
    }
#endif
}

static int
aaudio_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *private = this->hidden;
    aaudio_result_t res;
    int64_t timeoutNanoseconds = 8 * 1000 * 1000; /* 8 ms */
    res = ctx.AAudioStream_read(private->stream,  buffer, buflen / private->frame_size, timeoutNanoseconds);
    if (res < 0) {
        LOGI("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
        return -1;
    }
    LOGI("SDL AAudio capture:%d frames, wanted:%d frames", (int)res, buflen / private->frame_size);
    return res * private->frame_size;
}

static void
aaudio_Deinitialize(void)
{
    LOGI(__func__);
    if (ctx.handle) {
        if (ctx.builder) {
            aaudio_result_t res;
            res = ctx.AAudioStreamBuilder_delete(ctx.builder);
            if (res != AAUDIO_OK) {
                SDL_SetError("Failed AAudioStreamBuilder_delete %s", ctx.AAudio_convertResultToText(res));
            }
        }
        SDL_UnloadObject(ctx.handle);
    }
    ctx.handle = NULL;
    ctx.builder = NULL;
    LOGI("End AAUDIO %s", SDL_GetError());
}

static SDL_bool
aaudio_Init(SDL_AudioDriverImpl *impl)
{
    aaudio_result_t res;
    LOGI(__func__);

    /* AAudio was introduced in Android 8.0, but has reference counting crash issues in that release,
     * so don't use it until 8.1.
     *
     * See https://github.com/google/oboe/issues/40 for more information.
     */
    if (SDL_GetAndroidSDKVersion() < 27) {
        return SDL_FALSE;
    }

    SDL_zero(ctx);

    ctx.handle = SDL_LoadObject(LIB_AAUDIO_SO);
    if (ctx.handle == NULL) {
        LOGI("SDL couldn't find " LIB_AAUDIO_SO);
        goto failure;
    }

    if (aaudio_LoadFunctions(&ctx) < 0) {
        goto failure;
    }

    res = ctx.AAudio_createStreamBuilder(&ctx.builder);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudio_createStreamBuilder %d", res);
        goto failure;
    }

    if (ctx.builder == NULL) {
        LOGI("SDL Failed AAudio_createStreamBuilder - builder NULL");
        goto failure;
    }

    impl->Deinitialize = aaudio_Deinitialize;
    impl->OpenDevice = aaudio_OpenDevice;
    impl->CloseDevice = aaudio_CloseDevice;
    impl->PlayDevice = aaudio_PlayDevice;
    impl->GetDeviceBuf = aaudio_GetDeviceBuf;
    impl->CaptureFromDevice = aaudio_CaptureFromDevice;

    /* and the capabilities */
    impl->HasCaptureSupport = SDL_TRUE;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;

    /* this audio target is available. */
    LOGI("SDL aaudio_Init OK");
    return SDL_TRUE;

failure:
    if (ctx.handle) {
        if (ctx.builder) {
            ctx.AAudioStreamBuilder_delete(ctx.builder);
        }
        SDL_UnloadObject(ctx.handle);
    }
    ctx.handle = NULL;
    ctx.builder = NULL;
    return SDL_FALSE;
}

AudioBootStrap aaudio_bootstrap = {
    "AAudio", "AAudio audio driver", aaudio_Init, SDL_FALSE
};

/* Pause (block) all non already paused audio devices by taking their mixer lock */
void aaudio_PauseDevices(void)
{
    /* TODO: Handle multiple devices? */
    struct SDL_PrivateAudioData *private;
    if (audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *) audioDevice->hidden;

        if (private->stream) {
            aaudio_result_t res = ctx.AAudioStream_requestPause(private->stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestPause %d", res);
                SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            }
        }

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
        private = (struct SDL_PrivateAudioData *) captureDevice->hidden;

        if (private->stream) {
            /* Pause() isn't implemented for 'capture', use Stop() */
            aaudio_result_t res = ctx.AAudioStream_requestStop(private->stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestStop %d", res);
                SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            }
        }

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
void aaudio_ResumeDevices(void)
{
    /* TODO: Handle multiple devices? */
    struct SDL_PrivateAudioData *private;
    if (audioDevice != NULL && audioDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *) audioDevice->hidden;

        if (private->resume) {
            SDL_AtomicSet(&audioDevice->paused, 0);
            private->resume = SDL_FALSE;
            SDL_UnlockMutex(audioDevice->mixer_lock);
        }

        if (private->stream) {
            aaudio_result_t res = ctx.AAudioStream_requestStart(private->stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestStart %d", res);
                SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            }
        }
    }

    if (captureDevice != NULL && captureDevice->hidden != NULL) {
        private = (struct SDL_PrivateAudioData *) captureDevice->hidden;

        if (private->resume) {
            SDL_AtomicSet(&captureDevice->paused, 0);
            private->resume = SDL_FALSE;
            SDL_UnlockMutex(captureDevice->mixer_lock);
        }

        if (private->stream) {
            aaudio_result_t res = ctx.AAudioStream_requestStart(private->stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestStart %d", res);
                SDL_SetError("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
            }
        }
    }
}

/*
 We can sometimes get into a state where AAudioStream_write() will just block forever until we pause and unpause.
 None of the standard state queries indicate any problem in my testing. And the error callback doesn't actually get called.
 But, AAudioStream_getTimestamp() does return AAUDIO_ERROR_INVALID_STATE
*/
SDL_bool aaudio_DetectBrokenPlayState( void )
{
    struct SDL_PrivateAudioData *private;
    int64_t framePosition, timeNanoseconds;
    aaudio_result_t res;

    if ( !audioDevice || !audioDevice->hidden ) {
        return SDL_FALSE;
    }

    private = audioDevice->hidden;

    res = ctx.AAudioStream_getTimestamp( private->stream, CLOCK_MONOTONIC, &framePosition, &timeNanoseconds );
    if ( res == AAUDIO_ERROR_INVALID_STATE ) {
        aaudio_stream_state_t currentState = ctx.AAudioStream_getState( private->stream );
        /* AAudioStream_getTimestamp() will also return AAUDIO_ERROR_INVALID_STATE while the stream is still initially starting. But we only care if it silently went invalid while playing. */
        if ( currentState == AAUDIO_STREAM_STATE_STARTED ) {
            LOGI( "SDL aaudio_DetectBrokenPlayState: detected invalid audio device state: AAudioStream_getTimestamp result=%d, framePosition=%lld, timeNanoseconds=%lld, getState=%d", (int)res, (long long)framePosition, (long long)timeNanoseconds, (int)currentState );
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

#endif /* SDL_AUDIO_DRIVER_AAUDIO */

/* vi: set ts=4 sw=4 expandtab: */
