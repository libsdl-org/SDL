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
#include "napi/native_api.h"
#include "SDL_stdinc.h"
#include "SDL_atomic.h"
#include "SDL_surface.h"

#define OHOS_TS_CALLBACK_TYPE "ohoscalltype"

enum NapiCallBackType {
    NAPI_CALLBACK_CREATE_CUSTOMCURSOR,
    NAPI_CALLBACK_SET_CUSTOMCURSOR,
    NAPI_CALLBACK_SET_SYSTEMCURSOR,
    NAPI_CALLBACK_SET_RELATIVEMOUSEENABLED,
    NAPI_CALLBACK_SET_DISPLAYORIENTATION,
    NAPI_CALLBACK_SHOW_TEXTINPUT,
    NAPI_CALLBACK_REQUEST_PERMISSION,
    NAPI_CALLBACK_HIDE_TEXTINPUT,
    NAPI_CALLBACK_SHOULD_MINIMIZEON_FOCUSLOSS,
    NAPI_CALLBACK_SET_TITLE,
    NAPI_CALLBACK_SET_WINDOWSTYLE,
    NAPI_CALLBACK_SET_ORIENTATION,
    NAPI_CALLBACK_SHOW_TEXTINPUTKEYBOARD,
    NAPI_CALLBACK_SET_WINDOWRESIZE
};

typedef struct {
    napi_env env;
    napi_ref callbackRef;
    napi_threadsafe_function tsfn;
} NapiCallbackContext;

typedef int (*SDL_main_func)(int argc, char *argv[]);
typedef struct {
    char **argvs;
    int argcs;
    char *functionName;
    char *libraryFile;
} OhosSDLEntryInfo;

extern std::unique_ptr<NapiCallbackContext> napiCallback;

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

void OHOS_TS_Call(napi_env env, napi_value jsCb, void *context, void *data);

SDL_bool OHOS_RunThread(OhosSDLEntryInfo *info);

void OHOS_ThreadExit(void);

SDL_bool OHOS_IsThreadRun(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif // SDL_OHOSTHREADSAFE_H
