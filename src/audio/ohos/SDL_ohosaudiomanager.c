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
#include "SDL_ohosaudiobuffer.h"
#include "SDL_ohosaudiomanager.h"

#define DEFAULT_MS 2
#define THREAD_MS 10

/*
 * Audio support
 */
static int captureBufferLength = 0;
static int renderBufferLength = 0;

static unsigned char *rendererBuffer = NULL;
static int rendererBufferLen = 0;
static int rendererBufferReadPos = 0;

static OH_AudioStreamBuilder *builder = NULL;
static OH_AudioStreamBuilder *builder2 = NULL;
static OH_AudioCapturer *audioCapturer = NULL;
static OH_AudioRenderer *audioRenderer = NULL;

static SDL_atomic_t bAudioFeedDataFlag;
static SDL_bool audioPlayCondition = SDL_FALSE;
static SDL_mutex *audioPlayLock;
static SDL_cond *audioPlayCond;

static OH_AudioStream_State gAudioRendorStatus;

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
static int32_t OHOSAUDIO_AudioRenderer_OnWriteData(OH_AudioRenderer *renderer, void *userData,
                                                   void *buffer, int32_t length)
{
    int iReadAvailableSpace = 0;
    unsigned char *q = NULL;
    int iLoopen = 0;
    while (SDL_AtomicGet(&bAudioFeedDataFlag) == SDL_FALSE) {
        SDL_Delay(DEFAULT_MS);
    }
    iReadAvailableSpace = renderBufferLength - rendererBufferReadPos;
    if (iReadAvailableSpace <= 0) {
        SDL_LockMutex(audioPlayLock);
        audioPlayCondition = SDL_TRUE;
        rendererBufferReadPos = 0;
        iReadAvailableSpace = renderBufferLength;
        SDL_CondBroadcast(audioPlayCond);
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_FALSE);
        SDL_UnlockMutex(audioPlayLock);

        while (SDL_AtomicGet(&bAudioFeedDataFlag) == SDL_FALSE) {
            SDL_Delay(DEFAULT_MS);
        }
    }
    if (iReadAvailableSpace < length) {
        iLoopen = iReadAvailableSpace;
    } else {
        iLoopen = length;
    }

    q = rendererBuffer + rendererBufferReadPos;
    SDL_memcpy(buffer, q, iLoopen);
    SDL_memset(q, 0, iLoopen);
    rendererBufferReadPos += iLoopen;
    return 0;
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

static int OHOSAUDIO_CreateBuilder(int iscapture)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        // Request recording permission
        if (OHOS_NAPI_RequestPermission("ohos.permission.MICROPHONE") != SDL_TRUE) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "request permission failed, iscapture=%{public}d", iscapture);
            return -1;
        }
        iRet = OH_AudioStreamBuilder_Create(&builder2, AUDIOSTREAM_TYPE_CAPTURER);
    } else {
        iRet = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
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

static int OHOSAUDIO_SetBuilder(int iscapture, SDL_AudioSpec *spec)
{
    if (iscapture != 0) {
        return OHOSAUDIO_InSetBuilder(builder2, spec);
    } else {
        return OHOSAUDIO_InSetBuilder(builder, spec);
    }
}

static int OHOSAUDIO_SetCapturerInfo(int iscapture)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        iRet = OH_AudioStreamBuilder_SetCapturerInfo(builder2, AUDIOSTREAM_SOURCE_TYPE_MIC);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetCapturerInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    } else {
        iRet = OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MUSIC);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetRendererInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_SetCapturerCallback(int iscapture)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        OH_AudioCapturer_Callbacks capturerCallbacks;
        capturerCallbacks.OH_AudioCapturer_OnReadData = OHOSAUDIO_AudioCapturer_OnReadData;
        capturerCallbacks.OH_AudioCapturer_OnStreamEvent = OHOSAUDIO_AudioCapturer_OnStreamEvent;
        capturerCallbacks.OH_AudioCapturer_OnInterruptEvent = OHOSAUDIO_AudioCapturer_OnInterruptEvent;
        capturerCallbacks.OH_AudioCapturer_OnError = OHOSAUDIO_AudioCapturer_OnError;
        iRet = OH_AudioStreamBuilder_SetCapturerCallback(builder2, capturerCallbacks, NULL);
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
        iRet = OH_AudioStreamBuilder_SetRendererCallback(builder, rendererCallbacks, NULL);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "SetRendererCallback Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_GenerateCapturer(int iscapture)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        iRet = OH_AudioStreamBuilder_GenerateCapturer(builder2, &audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "GenerateCapturer Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    } else {
        iRet = OH_AudioStreamBuilder_GenerateRenderer(builder, &audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                "GenerateObject Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            return -1;
        }
    }
    return 0;
}

static int OHOSAUDIO_GetInfo(int iscapture, SDL_AudioSpec *spec)
{
    OH_AudioStream_State iStatus;
    OH_AudioStream_SourceType audioSourceType;
    OH_AudioStream_Usage audioUsage;
    int audioFormat;
    int32_t audioSamplingRate = 0;
    int32_t audioChannelCount = 0;
    if (iscapture != 0) {
        // Query the current input audio stream status
        OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            return -1;
        }
        
        // Query the current input audio stream sampling rate
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetSamplingRate(audioCapturer, &audioSamplingRate)) {
            return -1;
        }
        
        // Query the current input audio channel number
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetChannelCount(audioCapturer, &audioChannelCount)) {
            return -1;
        }
        
        // Query the current input audio stream format
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetSampleFormat(audioCapturer, &audioFormat)) {
            return -1;
        }
        
        if (AUDIOSTREAM_SUCCESS != OH_AudioCapturer_GetCapturerInfo(audioCapturer, &audioSourceType)) {
            return -1;
        }
        
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "iStatus=%{public}d, SourceType=%{public}d,"\
            " SamplingRate=%{public}d, ChannelCount=%{public}d.", iStatus, audioSourceType, spec->freq, spec->channels);
    } else {
        // Query the current output audio stream status
        OH_AudioRenderer_GetCurrentState(audioRenderer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            return -1;
        }
        // Query the current input audio stream sampling rate
        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetSamplingRate(audioRenderer, &audioSamplingRate)) {
            return -1;
        }
        
        // query the current input audio channel number
        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetChannelCount(audioRenderer, &audioChannelCount)) {
            return -1;
        }
        
        // query the current input audio stream format
        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetSampleFormat(audioRenderer, &audioFormat)) {
            return -1;
        }

        if (AUDIOSTREAM_SUCCESS != OH_AudioRenderer_GetRendererInfo(audioRenderer, &audioUsage)) {
            return -1;
        }
        
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "iStatus=%{public}d, ChannelCount=%{public}d,"\
            " SamplingRate=%{public}d, Usage=%{public}d.", iStatus, spec->channels, spec->freq, audioUsage);
    }
    
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
            audioFormatBitDepth = AUDIO_U8;
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

static int OHOSAUDIO_Start(int iscapture, SDL_AudioSpec *spec, int audioFormatBitDepth)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        #define ADDITIONAL_BUFFER_FACTOR 2
        captureBufferLength = (spec->samples * spec->channels * audioFormatBitDepth) * ADDITIONAL_BUFFER_FACTOR;
        OHOS_AUDIOBUFFER_InitCapture((unsigned int)captureBufferLength);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "captureBufferLength=%{public}d.",
            captureBufferLength);

        iRet = OH_AudioCapturer_Start(audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "Capturer_Start Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            OHOS_AUDIOBUFFER_DeInitCapture();
            return -1;
        }
    } else {
        renderBufferLength = spec->samples * spec->channels * audioFormatBitDepth;
        if (NULL == rendererBuffer) {
            rendererBuffer = (unsigned char *)SDL_malloc((unsigned long long)renderBufferLength * 
                sizeof(unsigned char));
            if (NULL == rendererBuffer) {
                OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "rendererBuffer alloc space faileed");
                return -1;
            }
            SDL_memset(rendererBuffer, 0, renderBufferLength);
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "rendererBuffer alloc space Failed, Error=%{public}d.", iRet);
        }
        iRet = OH_AudioRenderer_Start(audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                "Capturer_Start Failed, Error=%{public}d.", iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return -1;
        }
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_FALSE);
        audioPlayLock = SDL_CreateMutex();
        audioPlayCond = SDL_CreateCond();
        audioPlayCondition = SDL_FALSE;
    }
    return 0;
}

/*
 * Audio Functions
 */
int OHOSAUDIO_NATIVE_OpenAudioDevice(int iscapture, SDL_AudioSpec *spec)
{
    int audioFormat;
    int audioFormatBitDepth = 0;
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "open device, iscapture=%{public}d", iscapture);
    
    if (OHOSAUDIO_CreateBuilder(iscapture) < 0) {
        return -1;
    }

    if (OHOSAUDIO_SetBuilder(iscapture, spec) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }
    
    // Set the scene
    if (OHOSAUDIO_SetCapturerInfo(iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }
    
    // Set the callback for the audio stream
    if (OHOSAUDIO_SetCapturerCallback(iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }

    // Constructing an audio stream
    if (OHOSAUDIO_GenerateCapturer(iscapture) < 0) {
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }

    audioFormat = OHOSAUDIO_GetInfo(iscapture, spec);
    if (audioFormat < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OHOSAUDIO_GetInfo run Failed");
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }
    
    audioFormatBitDepth = OHOSAUDIO_Format2Depth(spec, audioFormat);
    if (audioFormatBitDepth < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "Failed depth=%{public}d", audioFormatBitDepth);
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return -1;
    }

    if (OHOSAUDIO_Start(iscapture, spec, audioFormatBitDepth) < 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "OHOSAUDIO_Start run Failed");
        return -1;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "open device leave.");
    return 0;
}

void *OHOSAUDIO_NATIVE_GetAudioBuf(void)
{
    return rendererBuffer;
}

void OHOSAUDIO_NATIVE_WriteAudioBuf(void)
{
    SDL_LockMutex(audioPlayLock);
    while (!audioPlayCondition) {
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_TRUE);
        SDL_CondWait(audioPlayCond, audioPlayLock);
    }
    audioPlayCondition = SDL_FALSE;
    SDL_UnlockMutex(audioPlayLock);
}

int OHOSAUDIO_NATIVE_CaptureAudioBuffer(void *buffer, int buflen)
{
    OH_AudioStream_State iStatus;
    OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_PAUSED == iStatus) {
        OH_AudioCapturer_Start(audioCapturer);
    }
    OHOS_AUDIOBUFFER_ReadCaptureBuffer(buffer, (unsigned int)buflen);
    return buflen;
}

void OHOSAUDIO_NATIVE_FlushCapturedAudio(void)
{
    OH_AudioStream_State iStatus;
    OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_RUNNING == iStatus) {
        OH_AudioCapturer_Pause(audioCapturer);
    }
    OHOS_AUDIOBUFFER_FlushBuffer();
}

static void OHOSAUDIO_DestroyBuilder(int iscapture)
{
    OH_AudioStream_Result iRet;
    if (iscapture != 0) {
        if (NULL == builder2) {
            return;
        }
        iRet = OH_AudioStreamBuilder_Destroy(builder2);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
        }
        builder2 = NULL;
    } else {
        if (NULL == builder) {
            return;
        }
        iRet = OH_AudioStreamBuilder_Destroy(builder);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
        }
        builder = NULL;
    }
}

void OHOSAUDIO_NATIVE_CloseAudioDevice(const int iscapture)
{
    OH_AudioStream_Result iRet;
    // Release the audio stream
    if (iscapture != 0) {
        if (NULL != audioCapturer) {
            // Stop recording
            iRet = OH_AudioCapturer_Stop(audioCapturer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioCapturer_Stop error,error code = %{public}d", iRet);
            }
            // Releasing the recording instance
            iRet = OH_AudioCapturer_Release(audioCapturer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioCapturer_Release error,error code = %{public}d", iRet);
            }
            audioCapturer = NULL;
        }
        OHOS_AUDIOBUFFER_DeInitCapture();
    } else {
        if (NULL != audioRenderer) {
            iRet = OH_AudioRenderer_Stop(audioRenderer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioRenderer_Stop error,error code = %{public}d", iRet);
            }
            SDL_AtomicSet(&bAudioFeedDataFlag, SDL_TRUE);
            // Wait for the thread to exit
            SDL_Delay(THREAD_MS);
            iRet = OH_AudioRenderer_Release(audioRenderer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                    "SDL audio: OH_AudioRenderer_Release error,error code = %{public}d", iRet);
            }
            audioRenderer = NULL;
        }
        if (NULL != rendererBuffer) {
            SDL_free(rendererBuffer);
            rendererBuffer = NULL;
        }

        if (audioPlayLock != NULL) {
            SDL_DestroyMutex(audioPlayLock);
            audioPlayLock = NULL;
        }
        if (audioPlayCond != NULL) {
            SDL_DestroyCond(audioPlayCond);
            audioPlayCond = NULL;
        }
    }
    OHOSAUDIO_DestroyBuilder(iscapture);
}

void OHOSAUDIO_PageResume(void)
{
    if (audioRenderer != NULL && gAudioRendorStatus == AUDIOSTREAM_STATE_RUNNING) {
        OH_AudioRenderer_Start(audioRenderer);
    }
}

void OHOSAUDIO_PagePause(void)
{
    if (audioRenderer != NULL) {
        OH_AudioRenderer_GetCurrentState(audioRenderer, &gAudioRendorStatus);
    }
}

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
