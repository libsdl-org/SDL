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

#ifndef SDL_OHOS_TSTYPE_H
#define SDL_OHOS_TSTYPE_H

#include <thread>
#include "napi/native_api.h"

#define OHOS_TS_CALLBACK_TYPE  "ohoscalltype"
#define OHOS_JSON_RETURN_VALUE "returnvalue"
#define OHOS_JSON_ASYN         "asyn"
#define OHOS_JSON_NODEREF      "noderef"
#define OHOS_JSON_NODEREF2     "noderef2"
#define OHOS_JSON_WIDTH        "width"
#define OHOS_JSON_HEIGHT       "height"
#define OHOS_JSON_X            "x"
#define OHOS_JSON_Y            "y"
#define OHOS_JSON_VISIBILITY   "visibility"

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
    NAPI_CALLBACK_SET_WINDOWRESIZE,
    NAPI_CALLBACK_GET_ROOTNODE,
    NAPI_CALLBACK_GET_XCOMPONENTID,
    NAPI_CALLBACK_ADDCHILDNODE,
    NAPI_CALLBACK_REMOVENODE,
    NAPI_CALLBACK_RAISENODE,
    NAPI_CALLBACK_LOWERNODE,
    NAPI_CALLBACK_RESIZENODE,
    NAPI_CALLBACK_REPARENT,
    NAPI_CALLBACK_VISIBILITY,
    NAPI_CALLBACK_GETNODERECT,
    NAPI_CALLBACK_MOVENODE,
    NAPI_CALLBACK_GETWINDOWID
};

typedef struct {
    napi_env env;
    napi_ref callbackRef;
    napi_threadsafe_function tsfn;
    std::thread::id mainThreadId;
} NapiCallbackContext;

#define OHOS_THREADSAFE_ARG0 0
#define OHOS_THREADSAFE_ARG1 1
#define OHOS_THREADSAFE_ARG2 2
#define OHOS_THREADSAFE_ARG3 3
#define OHOS_THREADSAFE_ARG4 4
#define OHOS_THREADSAFE_ARG5 5

extern std::unique_ptr<NapiCallbackContext> g_napiCallback;
#endif // SDL_OHOS_TSTYPE_H
