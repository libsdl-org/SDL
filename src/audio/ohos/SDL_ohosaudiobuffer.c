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

#include "SDL_stdinc.h"
#include "SDL_atomic.h"
#include "SDL_ohosaudiobuffer.h"

#define DEFAULT_MS 2

typedef struct {
    unsigned char *buffer;
    unsigned int size;
    SDL_atomic_t in;
    SDL_atomic_t out;
} AudioCaptureBuffer;

AudioCaptureBuffer *gCaptureBuffer;

int OHOS_AUDIOBUFFER_InitCapture(unsigned int bufferSize)
{
    gCaptureBuffer = (AudioCaptureBuffer *)SDL_malloc(sizeof(AudioCaptureBuffer));
    if (gCaptureBuffer == NULL) {
        SDL_Log("Malloc capture struct failed.");
        return -1;
    }
    gCaptureBuffer->buffer = (unsigned char *)SDL_malloc(bufferSize);
    if (gCaptureBuffer->buffer == NULL) {
        SDL_free(gCaptureBuffer);
        SDL_Log("Malloc capture buffer failed.");
        return -1;
    }
    SDL_memset(gCaptureBuffer->buffer, 0, bufferSize);

    gCaptureBuffer->size = bufferSize;
    SDL_AtomicSet(&gCaptureBuffer->in, 0);
    SDL_AtomicSet(&gCaptureBuffer->out, 0);
    return 0;
}

int OHOS_AUDIOBUFFER_DeInitCapture(void)
{
    if (gCaptureBuffer != NULL) {
        if (gCaptureBuffer->buffer != NULL) {
            SDL_free(gCaptureBuffer->buffer);
        }
        SDL_free(gCaptureBuffer);
        gCaptureBuffer = NULL;
    }
    return 0;
}

static SDL_bool OHOS_AUDIOBUFFER_IsEmpty(unsigned int size)
{
    SDL_bool isEmpty;
    unsigned int in = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->in);
    unsigned int out = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->out);
    if (in == out) {
        return SDL_TRUE;
    }
    if (out > in) {
        isEmpty = (out - in < size) ? SDL_TRUE : SDL_FALSE;
    } else {
        isEmpty = (gCaptureBuffer->size - in + out < size) ? SDL_TRUE : SDL_FALSE;
    }
    return isEmpty;
}

int OHOS_AUDIOBUFFER_ReadCaptureBuffer(unsigned char *buffer, unsigned int size)
{
    unsigned char *pDividerBuffer;
    unsigned char *pIn;
    unsigned int in;
    unsigned int out;
    while (OHOS_AUDIOBUFFER_IsEmpty(size) == SDL_TRUE) {
        SDL_Delay(DEFAULT_MS);
    }
    in = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->in);
    out = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->out);
    pIn = gCaptureBuffer->buffer + in;
    if (out > in) {
        SDL_memcpy(buffer, pIn, size);
        SDL_AtomicSet(&gCaptureBuffer->in, in + size);
    } else {
        unsigned int len = gCaptureBuffer->size - in;
        if (len >= size) {
            SDL_memcpy(buffer, pIn, size);
            SDL_AtomicSet(&gCaptureBuffer->in, in + size);
        } else {
            SDL_memcpy(buffer, pIn, len);
            pDividerBuffer = buffer + len;
            SDL_memcpy(pDividerBuffer, gCaptureBuffer->buffer, size - len);
            SDL_AtomicSet(&gCaptureBuffer->in, size - len);
        }
    }
    return 0;
}

static SDL_bool OHOS_AUDIOBUFFER_IsFull(unsigned int size)
{
    SDL_bool isFull = SDL_FALSE;
    unsigned int in = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->in);
    unsigned int out = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->out);
    if (in == out) {
        return SDL_FALSE;
    }
    if (out > in) {
        isFull = (gCaptureBuffer->size - out + in < size) ? SDL_TRUE : SDL_FALSE;
    } else {
        isFull = (in - out < size) ? SDL_TRUE : SDL_FALSE;
    }
    return isFull;
}

int OHOS_AUDIOBUFFER_WriteCaptureBuffer(unsigned char *buffer, unsigned int size)
{
    unsigned char *pOut;
    unsigned char *pDividerBuffer;
    unsigned int in;
    unsigned int out;
    while (OHOS_AUDIOBUFFER_IsFull(size) == SDL_TRUE) {
        SDL_Delay(DEFAULT_MS);
    }
    in = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->in);
    out = (unsigned int)SDL_AtomicGet(&gCaptureBuffer->out);
    pOut = gCaptureBuffer->buffer + out;
    if (out > in) {
        unsigned int len = gCaptureBuffer->size - out;
        if (len >= size) {
            SDL_memcpy(pOut, buffer, size);
            SDL_AtomicSet(&gCaptureBuffer->out, out + size);
        } else {
            SDL_memcpy(pOut, buffer, len);
            pDividerBuffer = buffer + len;
            SDL_memcpy(gCaptureBuffer->buffer, pDividerBuffer, size - len);
            SDL_AtomicSet(&gCaptureBuffer->out, size - len);
        }
    } else {
        SDL_memcpy(pOut, buffer, size);
        SDL_AtomicSet(&gCaptureBuffer->out, out + size);
    }
    return 0;
}

void OHOS_AUDIOBUFFER_FlushBuffer(void)
{
    SDL_AtomicSet(&gCaptureBuffer->in, 0);
    SDL_AtomicSet(&gCaptureBuffer->out, 0);
    SDL_memset(gCaptureBuffer->buffer, 0, gCaptureBuffer->size);
}

#endif /* SDL_AUDIO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
