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

#ifndef SDL_OHOSFILE_H
#define SDL_OHOSFILE_H
#include "SDL_rwops.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

const char *SDL_OHOSGetInternalStoragePath(void);
int OHOS_FileOpen(SDL_RWops *ctx, const char *fileName, const char *mode);
Sint64 OHOS_FileSize(SDL_RWops *ctx);
Sint64 OHOS_FileSeek(SDL_RWops *ctx, Sint64 offset, int whence);
size_t OHOS_FileRead(SDL_RWops *ctx, void *buffer, size_t size, size_t maxnum);
size_t OHOS_FileWrite(SDL_RWops *ctx, const void *buffer, size_t size, size_t num);
int OHOS_FileClose(SDL_RWops *ctx, SDL_bool release);
size_t OHOS_FileSeekInline(Sint64 *movement);
size_t OHOS_FileSeekInlineSwitch(Sint64 *offset, int whence);

extern SDL_RWops *gCtx;
extern char *g_path;
extern char *g_path;
void OHOS_CloseResourceManager(void);
void OHOS_NAPI_GetResourceManager(SDL_RWops *ctx, const char *fileName);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif // SDL_OHOSFILE_H
