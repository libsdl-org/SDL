/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License,Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_OHOS

#include <hilog/log.h>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiocapturer.h>
#include <ohaudio/native_audiorenderer.h>
#include "../../core/ohos/SDL_ohos.h"

#include "SDL_timer.h"
#include "../SDL_sysaudio.h"
#include "SDL_ohosaudio.h"
#include "SDL_ohosaudiobuffer.h"
#include "SDL_ohosaudiomanager.h"

#define DEFAULT_MS 2
#define THREAD_MS 10

/*
 * Audio support
 */
static int captureBufferLength = 0;

static OH_AudioStream_State gAudioRendorStatus;

#define OHOS_RENDER_BUFFER_SHUTDOEN_LEN 1024

/*
 * Audio Capturer Callbacks
 */
static int32_t OHOSAUDIO_AudioCapturer_OnReadData(OH_AudioCapturer *capturer, void *userData,
                                                  void *buffer, int32_t length)
{
    OHOS_AUDIOBUFFER_WriteCaptureBuffer(buffer, length);
    return 0;
}

static int32_t OHOSAUDIO_AudioCapturer_OnStreamEvent(OH_AudioCapturer *capturer, void *userData,
                                                     OH_AudioStream_Event event)
{
    return 1;
}

static int32_t OHOSAUDIO_AudioCapturer_OnInterruptEvent(OH_AudioCapturer *capturer, void *userData,
                                                        OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    return 1;
}

static int32_t OHOSAUDIO_AudioCapturer_OnError(OH_AudioCapturer *capturer, void *userData, OH_AudioStream_Result error)
{
    return 1;
}

/*
 * Audio Renderer Callbacks
 */
static int32_t OHOSAUDIO_AudioRenderer_OnWriteData(OH_AudioRenderer *renderer, void *userData, void *buffer,
                                                   int32_t length)
{
    SDL_AudioDevice *device = (SDL_AudioDevice *)userData;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    SDL_LockMutex(private->audioPlayLock);
    if (private->ohosFrameSize == -1 && length > 0) {
        private->ohosFrameSize = length;
        SDL_CondBroadcast(private->bufferCond);
    }
    while (SDL_AtomicGet(&private->stateFlag) == SDL_FALSE &&
           SDL_AtomicGet(&private->isShutDown) == SDL_FALSE) {
        SDL_CondWait(private->full, private->audioPlayLock);
    }
    if (SDL_AtomicGet(&private->isShutDown) == SDL_FALSE && private->rendererBuffer != NULL) {
        SDL_memcpy(buffer, private->rendererBuffer, private->ohosFrameSize);
        SDL_AtomicSet(&private->stateFlag, SDL_FALSE);
        SDL_CondBroadcast(private->empty);
    } else {
        int value = 0;
        if (device != NULL) {
            value = device->spec.silence;
        }
        SDL_memset(buffer, value, private->ohosFrameSize);
    }
    SDL_UnlockMutex(private->audioPlayLock);
    return 0;
}

void *OHOSAUDIO_NATIVE_GetAudioBuf(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    SDL_LockMutex(private->audioPlayLock);
    while ((private->rendererBuffer == NULL || SDL_AtomicGet(&private->stateFlag) == SDL_TRUE) &&
           SDL_AtomicGet(&private->isShutDown) == SDL_FALSE) {
        SDL_CondWait(private->empty, private->audioPlayLock);
    }
    // go here, may is shut down state and ohos render start failed, just init buffer
    // make sure shutdown normal
    if (private->rendererBuffer == NULL) {
        private->rendererBuffer = SDL_malloc(OHOS_RENDER_BUFFER_SHUTDOEN_LEN);
        device->callbackspec.size = OHOS_RENDER_BUFFER_SHUTDOEN_LEN;
        device->spec.size = OHOS_RENDER_BUFFER_SHUTDOEN_LEN;
    } else {
        device->callbackspec.size = private->ohosFrameSize;
        device->spec.size = private->ohosFrameSize;
    }
    SDL_UnlockMutex(private->audioPlayLock);
    return private->rendererBuffer;
}

void OHOSAUDIO_NATIVE_WriteAudioBuf(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    SDL_LockMutex(private->audioPlayLock);
    SDL_AtomicSet(&private->stateFlag, SDL_TRUE);
    SDL_CondBroadcast(private->full);
    SDL_UnlockMutex(private->audioPlayLock);
}

static int32_t OHOSAUDIO_AudioRenderer_OnStreamEvent(OH_AudioRenderer *renderer,
                                                     void *userData, OH_AudioStream_Event event)
{
    return 1;
}

static int32_t OHOSAUDIO_AudioRenderer_OnInterruptEvent(OH_AudioRenderer *renderer, void *userData,
                                                        OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    return 1;
}

static int32_t OHOSAUDIO_AudioRenderer_OnError(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Result error)
{
    return 1;
}

static int OHOSAUDIO_BitSampleFormat(SDL_AudioFormat bitSample)
{
    // Audio stream sampling format
    switch (bitSample) {
        case AUDIO_U8:
            return AUDIOSTREAM_SAMPLE_U8; // UNSIGNED-8-BITS
        case AUDIO_S16:
            return AUDIOSTREAM_SAMPLE_S16LE; // SHORT-16-BIT-LITTLE-END
        case AUDIO_S32:
        case AUDIO_F32SYS:
            return AUDIOSTREAM_SAMPLE_S32LE; // SHORT-32-BIT-LITTLE-END
        default:
            return -1;
    }
}

static int OHOSAUDIO_CreateBuilder(SDL_AudioDevice *device, int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        // Request recording permission
        if (OHOS_NAPI_RequestPermission("ohos.permission.MICROPHONE") != SDL_TRUE) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "request permission failed, iscapture=%{public}d", iscapture);
            return -1;
        }
        iRet = OH_AudioStreamBuilder_Create(&private->builder, AUDIOSTREAM_TYPE_CAPTURER);
    } else {
        iRet = OH_AudioStreamBuilder_Create(&private->builder, AUDIOSTREAM_TYPE_RENDERER);
    }

    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
            "Create Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
        return -1;
    }
    return 0;
}

static int OHOSAUDIO_InSetBuilder(OH_AudioStreamBuilder *pBuilder, SDL_AudioSpec *spec)
{
    OH_AudioStream_Result iRet;
    int audioFormat = OHOSAUDIO_BitSampleFormat(spec->format);
    if (audioFormat < 0) {
        return -1;
    }
    // Set the audio sample rate
    iRet = OH_AudioStreamBuilder_SetSamplingRate(pBuilder, spec->freq);
    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
            "SetSamplingRate Failed, Error=%{public}d.", iRet);
        return -1;
    }
    // Set the audio channel
    iRet = OH_AudioStreamBuilder_SetChannelCount(pBuilder, spec->channels);
    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
            "SetChannelCount Failed, Error=%{public}d.", iRet);
        return -1;
    }
    // Set the audio sampling format
    iRet = OH_AudioStreamBuilder_SetSampleFormat(pBuilder, audioFormat);
    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
            "SetSampleFormat Failed, Error=%{public}d.", iRet);
        return -1;
    }
    // Set the audio scenario: 0 represents the normal scenario and 1 represents the low delay scenario
    iRet = OH_AudioStreamBuilder_SetLatencyMode(pBuilder, AUDIOSTREAM_LATENCY_MODE_NORMAL);
    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
            "SetLatencyMode Failed, Error=%{public}d.", iRet);
        return -1;
    }
    // Set the encoding type of the audio stream
    iRet = OH_AudioStreamBuilder_SetEncodingType(pBuilder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
            "SetEncodingType Failed, Error=%{public}d.", iRet);
        return -1;
    }
    return 0;
}

static int OHOSAUDIO_SetBuilder(SDL_AudioDevice *device, int iscapture, SDL_AudioSpec *spec)
{
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        return OHOSAUDIO_InSetBuilder(private->builder, spec);
    } else {
        return OHOSAUDIO_InSetBuilder(private->builder, spec);
    }
}

static int OHOSAUDIO_SetCapturerInfo(SDL_AudioDevice *device, int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        iRet = OH_AudioStreamBuilder_SetCapturerInfo(private->builder, AUDIOSTREAM_SOURCE_TYPE_MIC);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetCapturerInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    } else {
        iRet = OH_AudioStreamBuilder_SetRendererInfo(private->builder, AUDIOSTREAM_USAGE_MUSIC);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetRendererInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_SetCapturerCallback(SDL_AudioDevice *device, int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        OH_AudioCapturer_Callbacks capturerCallbacks;
        capturerCallbacks.OH_AudioCapturer_OnReadData = OHOSAUDIO_AudioCapturer_OnReadData;
        capturerCallbacks.OH_AudioCapturer_OnStreamEvent = OHOSAUDIO_AudioCapturer_OnStreamEvent;
        capturerCallbacks.OH_AudioCapturer_OnInterruptEvent = OHOSAUDIO_AudioCapturer_OnInterruptEvent;
        capturerCallbacks.OH_AudioCapturer_OnError = OHOSAUDIO_AudioCapturer_OnError;
        iRet = OH_AudioStreamBuilder_SetCapturerCallback(private->builder, capturerCallbacks, NULL);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetCapturerCallback Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    } else {
        OH_AudioRenderer_Callbacks rendererCallbacks;
        rendererCallbacks.OH_AudioRenderer_OnWriteData = OHOSAUDIO_AudioRenderer_OnWriteData;
        rendererCallbacks.OH_AudioRenderer_OnStreamEvent = OHOSAUDIO_AudioRenderer_OnStreamEvent;
        rendererCallbacks.OH_AudioRenderer_OnInterruptEvent = OHOSAUDIO_AudioRenderer_OnInterruptEvent;
        rendererCallbacks.OH_AudioRenderer_OnError = OHOSAUDIO_AudioRenderer_OnError;
        iRet = OH_AudioStreamBuilder_SetRendererCallback(private->builder, rendererCallbacks, device);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetRendererCallback Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_GenerateCapturer(SDL_AudioDevice *device, int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        iRet = OH_AudioStreamBuilder_GenerateCapturer(private->builder, &private->audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "GenerateCapturer Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    } else {
        iRet = OH_AudioStreamBuilder_GenerateRenderer(private->builder, &private->audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "GenerateObject Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_GetInfo(SDL_AudioDevice *device, int iscapture, SDL_AudioSpec *spec)
{
    OH_AudioStream_State iStatus;
    OH_AudioStream_SourceType audioSourceType;
    OH_AudioStream_Usage audioUsage;
    int audioFormat;
    int32_t audioSamplingRate = 0;
    int32_t audioChannelCount = 0;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        OH_AudioCapturer_GetCurrentState(private->audioCapturer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetSamplingRate(private->audioCapturer, &audioSamplingRate)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetChannelCount(private->audioCapturer, &audioChannelCount)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetSampleFormat(private->audioCapturer, &audioFormat)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetCapturerInfo(private->audioCapturer, &audioSourceType)) {
            return -1;
        }
    } else {
        OH_AudioRenderer_GetCurrentState(private->audioRenderer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            return -1;
        }

        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetSamplingRate(private->audioRenderer, &audioSamplingRate)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetChannelCount(private->audioRenderer, &audioChannelCount)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetSampleFormat(private->audioRenderer, &audioFormat)) {
            return -1;
        }

        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetRendererInfo(private->audioRenderer, &audioUsage)) {
            return -1;
        }
    }

    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "Status=%{public}d, SamplingRate=%{public}d,"\
        " ChannelCount=%{public}d, AudioFormat=%{public}d.", iStatus, spec->freq, spec->channels, audioFormat);

    spec->freq = audioSamplingRate;
    spec->channels = audioChannelCount;
    return audioFormat;
}

static int OHOSAUDIO_Format2Depth(SDL_AudioSpec *spec, int audioFormat)
{
    enum {
        BIT_DEPTH_U8 = 1,
        BIT_DEPTH_S16 = 2,
        BIT_DEPTH_S32 = 4
    };
    int audioFormatBitDepth = -1;
    switch (audioFormat) {
        case AUDIOSTREAM_SAMPLE_U8:
            spec->format = AUDIO_U8;
            audioFormatBitDepth = BIT_DEPTH_U8;
            break;
        case AUDIOSTREAM_SAMPLE_S16LE:
            spec->format = AUDIO_S16;
            audioFormatBitDepth = BIT_DEPTH_S16;
            break;
        case AUDIOSTREAM_SAMPLE_S32LE:
            spec->format = AUDIO_S32;
            audioFormatBitDepth = BIT_DEPTH_S32;
            break;
        default:
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "Unsupported: 0x%{public}x", spec->format);
    }
    return audioFormatBitDepth;
}

static int OHOSAUDIO_WaitInitRenderBuffer(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    SDL_LockMutex(private->audioPlayLock);
    while (private->ohosFrameSize == -1) {
        SDL_CondWait(private->bufferCond, private->audioPlayLock);
    }
    SDL_free(private->rendererBuffer);
    if (private->ohosFrameSize < 0) {
        SDL_UnlockMutex(private->audioPlayLock);
        return -1;
    }
    private->rendererBuffer = SDL_malloc(private->ohosFrameSize + 1);
    if (private->rendererBuffer == NULL) {
        SDL_UnlockMutex(private->audioPlayLock);
        return -1;
    }
    SDL_memset(private->rendererBuffer, 0, private->ohosFrameSize);
    SDL_CondBroadcast(private->empty);
    SDL_UnlockMutex(private->audioPlayLock);
    return 0;
}

static int OHOSAUDIO_Start(SDL_AudioDevice *device, int iscapture, SDL_AudioSpec *spec, int audioFormatBitDepth)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        #define ADDITIONAL_BUFFER_FACTOR 2
        captureBufferLength = (spec->samples * spec->channels * audioFormatBitDepth) * ADDITIONAL_BUFFER_FACTOR;
        OHOS_AUDIOBUFFER_InitCapture((unsigned int)captureBufferLength);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "captureBufferLength=%{public}d.",
            captureBufferLength);

        iRet = OH_AudioCapturer_Start(private->audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "Capturer_Start Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
            OHOS_AUDIOBUFFER_DeInitCapture();
            return -1;
        }
    } else {
        SDL_AtomicSet(&private->stateFlag, SDL_FALSE);
        private->audioPlayLock = SDL_CreateMutex();
        private->empty = SDL_CreateCond();
        private->full = SDL_CreateCond();
        private->bufferCond = SDL_CreateCond();
        private->ohosFrameSize = -1;
        SDL_AtomicSet(&private->isShutDown, SDL_FALSE);
        SDL_AtomicSet(&private->stateFlag, SDL_FALSE);
        iRet = OH_AudioRenderer_Start(private->audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "Renderer_Start Failed, Error=%{public}d.", iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
            return -1;
        }
        if (OHOSAUDIO_WaitInitRenderBuffer(device) < 0) {
            OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
            return -1;
        }
    }
    return 0;
}

/*
 * Audio Functions
 */
int OHOSAUDIO_NATIVE_OpenAudioDevice(SDL_AudioDevice *device, int iscapture, SDL_AudioSpec *spec)
{
    int audioFormat;
    int audioFormatBitDepth = 0;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OpenDevice iscapture=%{public}d", iscapture);
    
    if (OHOSAUDIO_CreateBuilder(device, iscapture) < 0) {
        return -1;
    }

    if (OHOSAUDIO_SetBuilder(device, iscapture, spec) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }
    
    // Set the scene
    if (OHOSAUDIO_SetCapturerInfo(device, iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }
    
    // Set the callback for the audio stream
    if (OHOSAUDIO_SetCapturerCallback(device, iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }

    // Constructing an audio stream
    if (OHOSAUDIO_GenerateCapturer(device, iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }

    audioFormat = OHOSAUDIO_GetInfo(device, iscapture, spec);
    if (audioFormat < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OHOSAUDIO_GetInfo run Failed");
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }
    
    audioFormatBitDepth = OHOSAUDIO_Format2Depth(spec, audioFormat);
    if (audioFormatBitDepth < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "Failed depth=%{public}d", audioFormatBitDepth);
        OHOSAUDIO_NATIVE_CloseAudioDevice(device, iscapture);
        return -1;
    }

    if (OHOSAUDIO_Start(device, iscapture, spec, audioFormatBitDepth) < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OHOSAUDIO_Start run Failed");
        return -1;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OpenDevice end iscapture=%{public}d", iscapture);
    return 0;
}

int OHOSAUDIO_NATIVE_CaptureAudioBuffer(SDL_AudioDevice *device, void *buffer, int buflen)
{
    OH_AudioStream_State iStatus;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    OH_AudioCapturer_GetCurrentState(private->audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_PAUSED == iStatus) {
        OH_AudioCapturer_Start(private->audioCapturer);
    }
    OHOS_AUDIOBUFFER_ReadCaptureBuffer(buffer, (unsigned int)buflen);
    return buflen;
}

void OHOSAUDIO_NATIVE_FlushCapturedAudio(SDL_AudioDevice *device)
{
    OH_AudioStream_State iStatus;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    OH_AudioCapturer_GetCurrentState(private->audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_RUNNING == iStatus) {
        OH_AudioCapturer_Pause(private->audioCapturer);
    }
    OHOS_AUDIOBUFFER_FlushBuffer();
}

static void OHOSAUDIO_DestroyBuilder(SDL_AudioDevice *device, int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (iscapture != 0) {
        if (NULL == private->builder) {
            return;
        }
        iRet = OH_AudioStreamBuilder_Destroy(private->builder);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
        }
        private->builder = NULL;
    } else {
        if (NULL == private->builder) {
            return;
        }
        iRet = OH_AudioStreamBuilder_Destroy(private->builder);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
        }
        private->builder = NULL;
    }
}

static OHOSAUDIO_NATIVE_CloseRender(SDL_AudioDevice *device)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    if (NULL != private->audioRenderer) {
        iRet = OH_AudioRenderer_Stop(private->audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                         "SDL audio: OH_AudioRenderer_Stop error,error code = %{public}d", iRet);
        }
        SDL_CondBroadcast(private->full);
        iRet = OH_AudioRenderer_Release(private->audioRenderer);
        SDL_AtomicSet(&private->stateFlag, SDL_FALSE);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                         "SDL audio: OH_AudioRenderer_Release error,error code = %{public}d", iRet);
        }
        private->audioRenderer = NULL;
    }
    private->ohosFrameSize = -1;
    if (private->rendererBuffer != NULL) {
        SDL_free(private->rendererBuffer);
        private->rendererBuffer = NULL;
    }

    if (private->bufferCond != NULL) {
        SDL_DestroyCond(private->bufferCond);
        private->bufferCond = NULL;
    }

    if (private->audioPlayLock != NULL) {
        SDL_DestroyMutex(private->audioPlayLock);
        private->audioPlayLock = NULL;
    }
    if (private->empty != NULL) {
        SDL_DestroyCond(private->empty);
        private->empty = NULL;
    }
    if (private->full != NULL) {
        SDL_DestroyCond(private->full);
        private->full = NULL;
    }
}

void OHOSAUDIO_NATIVE_PrepareClose(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    SDL_LockMutex(private->audioPlayLock);
    SDL_AtomicSet(&private->isShutDown, SDL_TRUE);
    SDL_CondBroadcast(private->empty);
    SDL_UnlockMutex(private->audioPlayLock);
}

void OHOSAUDIO_NATIVE_CloseAudioDevice(SDL_AudioDevice *device, const int iscapture)
{
    OH_AudioStream_Result iRet;
    struct SDL_PrivateAudioData *private = (struct SDL_PrivateAudioData *)device->hidden;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice", "CloseDevice iscapture=%{public}d", iscapture);
    // Release the audio stream
    if (iscapture != 0) {
        if (NULL != private->audioCapturer) {
            // Stop recording
            iRet = OH_AudioCapturer_Stop(private->audioCapturer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioCapturer_Stop error,error code = %{public}d", iRet);
            }
            // Releasing the recording instance
            iRet = OH_AudioCapturer_Release(private->audioCapturer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioCapturer_Release error,error code = %{public}d", iRet);
            }
            private->audioCapturer = NULL;
        }
        OHOS_AUDIOBUFFER_DeInitCapture();
    } else {
        OHOSAUDIO_NATIVE_CloseRender(device);
    }
    OHOSAUDIO_DestroyBuilder(device, iscapture);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice", "CloseDevice end iscapture=%{public}d", iscapture);
}

void OHOSAUDIO_PageResume(void)
{
}

void OHOSAUDIO_PagePause(void)
{
}

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
