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

#include "../../core/ohos/SDL_ohos.h"

#include "SDL_timer.h"
#include "../SDL_sysaudio.h"
#include "SDL_ohosaudiomanager.h"
#include "SDL_ohosaudiobuffer.h"

#include <hilog/log.h>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiocapturer.h>
#include <ohaudio/native_audiorenderer.h>

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
static int32_t
OHOSAUDIO_AudioCapturer_OnReadData(OH_AudioCapturer *capturer, void *userData, void *buffer,int32_t length)
{
    OHOS_AUDIOBUFFER_WriteCaptureBuffer(buffer, length);
    return 0;
}

static int32_t
OHOSAUDIO_AudioCapturer_OnStreamEvent(OH_AudioCapturer *capturer, void *userData, OH_AudioStream_Event event)
{
    return 1;
}

static int32_t
OHOSAUDIO_AudioCapturer_OnInterruptEvent(OH_AudioCapturer *capturer, void *userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    return 1;
}

static int32_t
OHOSAUDIO_AudioCapturer_OnError(OH_AudioCapturer *capturer, void *userData, OH_AudioStream_Result error)
{
    return 1;
}

/*
 * Audio Renderer Callbacks
 */
static int32_t
OHOSAUDIO_AudioRenderer_OnWriteData(OH_AudioRenderer *renderer, void *userData, void *buffer, int32_t length)
{
    int iReadAvailableSpace = 0;
    unsigned char *q = NULL;
    int iLoopen = 0;
    while (SDL_AtomicGet(&bAudioFeedDataFlag) == SDL_FALSE) {
        SDL_Delay(2);
    }
    iReadAvailableSpace = renderBufferLength - rendererBufferReadPos;
    if (iReadAvailableSpace <= 0) {
        SDL_LockMutex(audioPlayLock);
        audioPlayCondition = true;
        rendererBufferReadPos = 0;
        iReadAvailableSpace = renderBufferLength;
        SDL_CondBroadcast(audioPlayCond);
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_FALSE);
        SDL_UnlockMutex(audioPlayLock);

        while (SDL_AtomicGet(&bAudioFeedDataFlag) == SDL_FALSE) {
            SDL_Delay(2);
        }
    }
    if (iReadAvailableSpace < length) {
        iLoopen = iReadAvailableSpace;
    } else {
        iLoopen = length;
    }

    q = rendererBuffer + rendererBufferReadPos;
    memcpy(buffer, q, iLoopen);
    memset(q, 0, iLoopen);
    rendererBufferReadPos += iLoopen;
    return 0;
}

static int32_t
OHOSAUDIO_AudioRenderer_OnStreamEvent(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Event event)
{
    return 1;
}

static int32_t
OHOSAUDIO_AudioRenderer_OnInterruptEvent(OH_AudioRenderer *renderer, void *userData, OH_AudioInterrupt_ForceType type, OH_AudioInterrupt_Hint hint)
{
    return 1;
}

static int32_t
OHOSAUDIO_AudioRenderer_OnError(OH_AudioRenderer *renderer, void *userData, OH_AudioStream_Result error)
{
    return 1;
}

/*
 * Audio Functions
 */
int
OHOSAUDIO_NATIVE_OpenAudioDevice(int iscapture, SDL_AudioSpec *spec)
{
    SDL_bool ret;
    OH_AudioStream_Result iRet;
    OH_AudioStream_State iStatus;

    int32_t audioSamplingRate = 0;
    int32_t audioChannelCount = 0;
    OH_AudioStream_SampleFormat audioFormat = 0;
    int audioFormatBitDepth = 0;

    OH_AudioStream_LatencyMode latencyMode = AUDIOSTREAM_LATENCY_MODE_NORMAL;
    OH_AudioStream_EncodingType encodingType = AUDIOSTREAM_ENCODING_TYPE_RAW;
    OH_AudioStream_SourceType audioSourceType = AUDIOSTREAM_SOURCE_TYPE_MIC;
    OH_AudioStream_Usage audioUsage = AUDIOSTREAM_USAGE_MUSIC;

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "open device enter, iscapture=%{public}d",
                 iscapture);

    // Audio stream sampling format
    switch (spec->format) {
    case AUDIO_U8:
        audioFormat = AUDIOSTREAM_SAMPLE_U8; // UNSIGNED-8-BITS
        break;
    case AUDIO_S16:
        audioFormat = AUDIOSTREAM_SAMPLE_S16LE; // SHORT-16-BIT-LITTLE-END
        break;
    case AUDIO_S32:
    case AUDIO_F32SYS:
        audioFormat = AUDIOSTREAM_SAMPLE_S32LE; // SHORT-32-BIT-LITTLE-END
        break;
    default:
        return 0;
    }
    if (iscapture) {
        // Request recording permission
        ret = OHOS_NAPI_RequestPermission("ohos.permission.MICROPHONE");
        if (SDL_TRUE == ret) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                         "request permission succeessed, iscapture=%{public}d", iscapture);
        } else {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                         "request permission failed, iscapture=%{public}d", iscapture);
            return 0;
        }
        iRet = OH_AudioStreamBuilder_Create(&builder2, AUDIOSTREAM_TYPE_CAPTURER);
    } else {
        iRet = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
    }

    if (AUDIOSTREAM_SUCCESS != iRet) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                     "Create Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
        return 0;
    }

    if(iscapture){
        // Set the audio sample rate
        iRet = OH_AudioStreamBuilder_SetSamplingRate(builder2, spec->freq);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetSamplingRate Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio channel
        iRet = OH_AudioStreamBuilder_SetChannelCount(builder2, spec->channels);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetChannelCount Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio sampling format
        iRet = OH_AudioStreamBuilder_SetSampleFormat(builder2, audioFormat);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetSampleFormat Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio scenario: 0 represents the normal scenario and 1 represents the low delay scenario
        iRet = OH_AudioStreamBuilder_SetLatencyMode(builder2, latencyMode);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetLatencyMode Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the encoding type of the audio stream
        iRet = OH_AudioStreamBuilder_SetEncodingType(builder2, encodingType);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetEncodingType Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    }else{
        // Set the audio sample rate
        iRet = OH_AudioStreamBuilder_SetSamplingRate(builder, spec->freq);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetSamplingRate Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio channel
        iRet = OH_AudioStreamBuilder_SetChannelCount(builder, spec->channels);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetChannelCount Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio sampling format
        iRet = OH_AudioStreamBuilder_SetSampleFormat(builder, audioFormat);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetSampleFormat Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the audio scenario: 0 represents the normal scenario and 1 represents the low delay scenario
        iRet = OH_AudioStreamBuilder_SetLatencyMode(builder, latencyMode);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetLatencyMode Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Set the encoding type of the audio stream
        iRet = OH_AudioStreamBuilder_SetEncodingType(builder, encodingType);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                     "SetEncodingType Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    }

    // Set the scene
    if (iscapture) {
        iRet = OH_AudioStreamBuilder_SetCapturerInfo(builder2, audioSourceType);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "SetCapturerInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    } else {
        iRet = OH_AudioStreamBuilder_SetRendererInfo(builder, audioUsage);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "SetRendererInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    }
    // Set the callback for the audio stream
    if (iscapture) {
        OH_AudioCapturer_Callbacks capturerCallbacks;
        capturerCallbacks.OH_AudioCapturer_OnReadData = OHOSAUDIO_AudioCapturer_OnReadData;
        capturerCallbacks.OH_AudioCapturer_OnStreamEvent = OHOSAUDIO_AudioCapturer_OnStreamEvent;
        capturerCallbacks.OH_AudioCapturer_OnInterruptEvent = OHOSAUDIO_AudioCapturer_OnInterruptEvent;
        capturerCallbacks.OH_AudioCapturer_OnError = OHOSAUDIO_AudioCapturer_OnError;
        iRet = OH_AudioStreamBuilder_SetCapturerCallback(builder2, capturerCallbacks, NULL);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "SetCapturerCallback Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
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
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    }
    // Constructing an audio stream
    if (iscapture) {
        iRet = OH_AudioStreamBuilder_GenerateCapturer(builder2, &audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                         "GenerateCapturer Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    } else {
        iRet = OH_AudioStreamBuilder_GenerateRenderer(builder, &audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                         "GenerateObject Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
    }
    if (iscapture) {
        // Query the current input audio stream status
        iRet = OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice",
                         "GetCurrentState Failed, iStatus=%{public}d, Error=%{public}d.", iStatus, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Query the current input audio stream sampling rate
        iRet = OH_AudioCapturer_GetSamplingRate(audioCapturer, &audioSamplingRate);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetSamplingRate Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetSamplingRate succeessed, iscapture=%{public}d, SamplingRate=%{public}d.",
                         iscapture, audioSamplingRate);
            spec->freq = audioSamplingRate;
        }
        // Query the current input audio channel number
        iRet = OH_AudioCapturer_GetChannelCount(audioCapturer, &audioChannelCount);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetChannelCount Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetChannelCount succeessed, iscapture=%{public}d, ChannelCount=%{public}d.",
                         iscapture, audioChannelCount);
            spec->channels = audioChannelCount;
        }
        // Query the current input audio stream format
        iRet = OH_AudioCapturer_GetSampleFormat(audioCapturer, &audioFormat);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetSampleFormat Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        iRet = OH_AudioCapturer_GetCapturerInfo(audioCapturer, &audioSourceType);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetCapturerInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_GetCapturerInfo succeessed, iscapture=%{public}d, SourceType=%{public}d.", iscapture,
                         audioSourceType);
        }
    } else {
        // Query the current output audio stream status
        iRet = OH_AudioRenderer_GetCurrentState(audioRenderer, &iStatus);
        if (AUDIOSTREAM_STATE_PREPARED != iStatus) {
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        // Query the current input audio stream sampling rate
        iRet = OH_AudioRenderer_GetSamplingRate(audioRenderer, &audioSamplingRate);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetSamplingRate Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetSamplingRate succeessed, iscapture=%{public}d, GetSamplingRate=%{public}d.",
                         iscapture, audioSamplingRate);
            spec->freq = audioSamplingRate;
        }
        // query the current input audio channel number
        iRet = OH_AudioRenderer_GetChannelCount(audioRenderer, &audioChannelCount);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetChannelCount Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetChannelCount succeessed, iscapture=%{public}d, GetChannelCount=%{public}d.",
                         iscapture, audioChannelCount);
            spec->channels = audioChannelCount;
        }
        // query the current input audio stream format
        iRet = OH_AudioRenderer_GetSampleFormat(audioRenderer, &audioFormat);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetSampleFormat Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        iRet = OH_AudioRenderer_GetRendererInfo(audioRenderer, &audioUsage);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetRendererInfo Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Renderer_GetRendererInfo succeessed, iscapture=%{public}d, Usage=%{public}d.", iscapture,
                         audioUsage);
        }
    }

    switch (audioFormat) {
    case AUDIOSTREAM_SAMPLE_U8:
        spec->format = AUDIO_U8;
        audioFormatBitDepth = 1;
        break;
    case AUDIOSTREAM_SAMPLE_S16LE:
        spec->format = AUDIO_S16;
        audioFormatBitDepth = 2;
        break;
    case AUDIOSTREAM_SAMPLE_S32LE:
        spec->format = AUDIO_S32;
        audioFormatBitDepth = 4;
        break;
    default:
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "Unsupported audio format: 0x%{public}x", spec->format);
        OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
        return 0;
    }

    if (iscapture) {
	captureBufferLength = (spec->samples * spec->channels * audioFormatBitDepth) * 2;
        OHOS_AUDIOBUFFER_InitCapture(captureBufferLength);
        OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "captureBufferLength=%{public}d.",
                     captureBufferLength);
        
        iRet = OH_AudioCapturer_Start(audioCapturer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_Start Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            OHOS_AUDIOBUFFER_DeInitCapture();
            return 0;
        }
    } else {
        renderBufferLength = spec->samples * spec->channels * audioFormatBitDepth;
        if (NULL == rendererBuffer) {
            rendererBuffer = (unsigned char *)malloc((renderBufferLength) * sizeof(unsigned char));
            if (NULL == rendererBuffer) {
                OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice", "rendererBuffer alloc space faileed");
                return 0;
            }
            memset(rendererBuffer, 0, renderBufferLength);
        } else {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "rendererBuffer alloc space Failed, iStatus=%{public}d, Error=%{public}d.", iStatus, iRet);
        }
        iRet = OH_AudioRenderer_Start(audioRenderer);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, "OpenAudioDevice",
                         "Capturer_Start Failed, iscapture=%{public}d, Error=%{public}d.", iscapture, iRet);
            OHOSAUDIO_NATIVE_CloseAudioDevice(iscapture);
            return 0;
        }
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_FALSE);
        audioPlayLock = SDL_CreateMutex();
        audioPlayCond = SDL_CreateCond();
        audioPlayCondition = SDL_FALSE;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "OpenAudioDevice", "open device leave, iscapture=%{public}d",
                 iscapture);
    return 1;
}

void *
OHOSAUDIO_NATIVE_GetAudioBuf(void)
{
    return rendererBuffer;
}

void
OHOSAUDIO_NATIVE_WriteAudioBuf(void)
{
    SDL_LockMutex(audioPlayLock);
    while (!audioPlayCondition) {
        SDL_AtomicSet(&bAudioFeedDataFlag, SDL_TRUE);
        SDL_CondWait(audioPlayCond, audioPlayLock);
    }
    audioPlayCondition = false;
    SDL_UnlockMutex(audioPlayLock);
}

int
OHOSAUDIO_NATIVE_CaptureAudioBuffer(void *buffer, int buflen)
{
   OH_AudioStream_State iStatus;
   OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_PAUSED == iStatus) {
        OH_AudioCapturer_Start(audioCapturer);
    }
    OHOS_AUDIOBUFFER_ReadCaptureBuffer(buffer, buflen);
    return buflen;
}

void
OHOSAUDIO_NATIVE_FlushCapturedAudio(void)
{
    OH_AudioStream_State iStatus;
    OH_AudioCapturer_GetCurrentState(audioCapturer, &iStatus);
    if (AUDIOSTREAM_STATE_RUNNING == iStatus) {
        OH_AudioCapturer_Pause(audioCapturer);
    }
    OHOS_AUDIOBUFFER_FlushBuffer();
    return;
}

void
OHOSAUDIO_NATIVE_CloseAudioDevice(const int iscapture)
{
    OH_AudioStream_Result iRet;
    // Release the audio stream
    if (iscapture) {
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
            // Wait for the HarmonyOS child thread to exit
            SDL_Delay(10);
            iRet = OH_AudioRenderer_Release(audioRenderer);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                             "SDL audio: OH_AudioRenderer_Release error,error code = %{public}d", iRet);
            }
            audioRenderer = NULL;
        }
        if (NULL != rendererBuffer) {
            free(rendererBuffer);
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
    // Destroy the constructor
    if(iscapture){
        if (NULL != builder2) {
            iRet = OH_AudioStreamBuilder_Destroy(builder2);
            if (AUDIOSTREAM_SUCCESS != iRet) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                         "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
            }
            builder2 = NULL;
        }
    } else {
      if (NULL != builder) {
        iRet = OH_AudioStreamBuilder_Destroy(builder);
        if (AUDIOSTREAM_SUCCESS != iRet) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "CloseAudioDevice",
                         "SDL audio: OH_AudioStreamBuilder_Destroy error,error code = %{public}d", iRet);
        }
        builder = NULL;
      }
    return;
   }
}

void
OHOSAUDIO_PageResume(void)
{
    if (audioRenderer != NULL && gAudioRendorStatus == AUDIOSTREAM_STATE_RUNNING) {
        OH_AudioRenderer_Start(audioRenderer);
    }
}

void
OHOSAUDIO_PagePause(void)
{
   if (audioRenderer != NULL) {
       OH_AudioRenderer_GetCurrentState(audioRenderer, &gAudioRendorStatus);
   }
}

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
