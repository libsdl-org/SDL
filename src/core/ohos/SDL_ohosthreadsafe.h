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

#ifndef SDL_OHOSTHREADSAFE_H
#define SDL_OHOSTHREADSAFE_H

#include <memory>
#include "SDL_stdinc.h"
#include "SDL_atomic.h"
#include "SDL_surface.h"
#include "cJSON.h"
#include "SDL_ohos_tstype.h"

typedef int (*SdlMainFunc)(int argc, char *argv[]);
typedef struct {
    char **argvs;
    int argcs;
    char *functionName;
    char *libraryFile;
} OhosSDLEntryInfo;

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

void OHOS_TS_Call(napi_env env, napi_value jsCb, void *context, void *data);

SDL_bool OHOS_RunThread(OhosSDLEntryInfo *info);

void OHOS_ThreadExit(void);

void OHOS_TS_GetWindowId(const cJSON *root);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif // SDL_OHOSTHREADSAFE_H
