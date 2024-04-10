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
#include "SDL_ohosfile.h"

#include <rawfile/raw_file_manager.h>

SDL_RWops *gCtx = nullptr;
char *g_path = nullptr;

const char *SDL_OHOSGetInternalStoragePath(void) { return g_path; }

int OHOS_FileOpen(SDL_RWops *ctx, const char *fileName, const char *mode)
{
    gCtx->hidden.ohosio.fileName = (char *)fileName;
    gCtx->hidden.ohosio.mode = (char *)mode;
    gCtx->hidden.ohosio.position = 0;

    NativeResourceManager *nativeResourceManager =
        static_cast<NativeResourceManager *>(gCtx->hidden.ohosio.nativeResourceManager);
    RawFile *rawFile = OH_ResourceManager_OpenRawFile(nativeResourceManager, fileName);

    if (!rawFile) {
        return -1;
    }

    gCtx->hidden.ohosio.fileNameRef = rawFile;

    long rawFileSize = OH_ResourceManager_GetRawFileSize(rawFile);
    gCtx->hidden.ohosio.size = rawFileSize;

    RawFileDescriptor descriptor;
    bool result = OH_ResourceManager_GetRawFileDescriptor(rawFile, descriptor);
    gCtx->hidden.ohosio.fd = descriptor.fd;
    gCtx->hidden.ohosio.fileDescriptorRef = static_cast<void *>(&descriptor);

    long rawFileOffset = OH_ResourceManager_GetRawFileOffset(rawFile);
    gCtx->hidden.ohosio.offset = rawFileOffset;
    ctx = gCtx;

    /* Seek to the correct offset in the file. */
    int position = OH_ResourceManager_SeekRawFile(rawFile, gCtx->hidden.ohosio.offset, SEEK_SET);

    return 0;
}

Sint64 OHOS_FileSize(SDL_RWops *ctx) { return gCtx->hidden.ohosio.size; }

size_t OHOS_FileSeekInlineSwitch(Sint64 *offset, int whence)
{
    switch (whence) {
        case RW_SEEK_SET:
            if (gCtx->hidden.ohosio.size != -1 && *offset > gCtx->hidden.ohosio.size) {
                *offset = gCtx->hidden.ohosio.size;
            }
            *offset += gCtx->hidden.ohosio.offset;
            break;
        case RW_SEEK_CUR:
            *offset += gCtx->hidden.ohosio.position;
            if (gCtx->hidden.ohosio.size != -1 && *offset > gCtx->hidden.ohosio.size) {
                *offset = gCtx->hidden.ohosio.size;
            }
            *offset += gCtx->hidden.ohosio.offset;
            break;
        case RW_SEEK_END:
            *offset = gCtx->hidden.ohosio.offset + gCtx->hidden.ohosio.size + *offset;
            break;
        default:
            return -1;
        }
    return 0;
}

size_t OHOS_FileSeekInlineSwitchPos(Sint64 *offset, Sint64 *newPosition, int whence)
{
    switch (whence) {
        case RW_SEEK_SET:
            *newPosition = *offset;
            break;
        case RW_SEEK_CUR:
            *newPosition = gCtx->hidden.ohosio.position + *offset;
            break;
        case RW_SEEK_END:
            *newPosition = gCtx->hidden.ohosio.size + *offset;
            break;
        default:
            return -1;
        }
    return 0;
}

Sint64 OHOS_FileSeek(SDL_RWops *ctx, Sint64 offset, int whence)
{
    if (gCtx->hidden.ohosio.nativeResourceManager) {
        size_t result = OHOS_FileSeekInlineSwitch(&offset, whence);
        if (result == -1) {
            return SDL_SetError("Unknown value for 'whence'");
        }
        RawFile *rawFile = static_cast<RawFile *>(gCtx->hidden.ohosio.fileNameRef);
        int ret = OH_ResourceManager_SeekRawFile(rawFile, offset, SEEK_SET);
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            ret = offset;
        }
        gCtx->hidden.ohosio.position = ret - gCtx->hidden.ohosio.offset;
    } else {
        Sint64 newPosition;
        Sint64 movement;
        size_t result = OHOS_FileSeekInlineSwitchPos(&offset, &newPosition, whence);
        if (result == -1) {
            return SDL_SetError("Unknown value for 'whence'");
        }
        /* Validate the new position */
        if (newPosition < 0) {
            return SDL_Error(SDL_EFSEEK);
        }
        if (newPosition > gCtx->hidden.ohosio.size) {
            newPosition = gCtx->hidden.ohosio.size;
        }
        movement = newPosition - gCtx->hidden.ohosio.position;
        if (movement > 0) {
            size_t result = OHOS_FileSeekInline(&movement);
            if (result == -1) {
                return -1;
            }
        } else if (movement < 0) {
            /* We can't seek backwards so we have to reopen the file and seek */
            /* forwards which obviously isn't very efficient */
            OHOS_FileClose(ctx, SDL_FALSE);
            OHOS_FileOpen(ctx, gCtx->hidden.ohosio.fileName, gCtx->hidden.ohosio.mode);
            OHOS_FileSeek(ctx, newPosition, RW_SEEK_SET);
        }
    }
    return gCtx->hidden.ohosio.position;
}

size_t OHOS_FileSeekInline(Sint64 *movement)
{
    unsigned char buffer[4096];

    /* The easy case where we're seeking forwards */
    while (*movement > 0) {
        Sint64 amount = sizeof(buffer);
        size_t result;
        if (amount > *movement) {
            amount = *movement;
        }
        result = OHOS_FileRead(gCtx, buffer, 1, (size_t)amount);
        if (result <= 0) {
            /* Failed to read/skip the required amount, so fail */
            return -1;
        }
        *movement -= result;
    }
    return 0;
}

size_t OHOS_FileRead(SDL_RWops *ctx, void *buffer, size_t size, size_t maxnum)
{
    if (gCtx->hidden.ohosio.nativeResourceManager) {
        size_t bytesMax = size * maxnum;
        size_t result;
        if (gCtx->hidden.ohosio.size != -1 &&
            gCtx->hidden.ohosio.position + bytesMax > gCtx->hidden.ohosio.size) {
            bytesMax = gCtx->hidden.ohosio.size - gCtx->hidden.ohosio.position;
        }
        RawFile *rawFile = static_cast<RawFile *>(gCtx->hidden.ohosio.fileNameRef);
        result = OH_ResourceManager_ReadRawFile(rawFile, buffer, bytesMax);
        if (result > 0  && size != 0) {
            gCtx->hidden.ohosio.position += result;
            return result / size;
        } else {
            return -1;
        }
        return 0;
    } else {
        long bytesRemaining = size * maxnum;
        long bytesMax = gCtx->hidden.ohosio.size - gCtx->hidden.ohosio.position;
        int bytesRead = 0;

        /* Don't read more bytes than those that remain in the file, otherwise we get an exception */
        if (bytesRemaining > bytesMax) {
            bytesRemaining = bytesMax;
        }
        unsigned char byteBuffer[bytesRemaining];
        while (bytesRemaining > 0) {
            RawFile *rawFile = static_cast<RawFile *>(gCtx->hidden.ohosio.fileNameRef);
            int result = OH_ResourceManager_ReadRawFile(rawFile, byteBuffer, bytesRemaining);
            if (result < 0) {
                break;
            }

            bytesRemaining -= result;
            bytesRead += result;
            gCtx->hidden.ohosio.position += result;
        }
        if (size != 0) {
            return bytesRead / size;
        } else {
            return -1;
        }
    }
}

size_t OHOS_FileWrite(SDL_RWops *ctx, const void *buffer, size_t size, size_t num)
{
    SDL_SetError("Cannot write to OHOS package filesystem");
    return 0;
}

int OHOS_FileClose(SDL_RWops *ctx, SDL_bool release)
{
    int result = 0;

    if (ctx) {
        OHOS_CloseResourceManager();

        if (release) {
            SDL_FreeRW(ctx);
        }
    }

    return result;
}

void OHOS_CloseResourceManager(void)
{
    RawFile *rawFile = static_cast<RawFile *>(gCtx->hidden.ohosio.fileNameRef);
    if (rawFile) {
        OH_ResourceManager_CloseRawFile(rawFile);
    }

    RawFileDescriptor *descriptor = static_cast<RawFileDescriptor *>(gCtx->hidden.ohosio.fileDescriptorRef);
    if (descriptor) {
        OH_ResourceManager_ReleaseRawFileDescriptor(*descriptor);
    }
}
