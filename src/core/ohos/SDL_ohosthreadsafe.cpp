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

#include "SDL_ohosthreadsafe.h"

#include <dlfcn.h>
#include <memory>
#include <unordered_map>
#include "cJSON.h"
extern "C" {
#include "../../thread/SDL_systhread.h"
}
#include <multimedia/image_framework/image_mdk_common.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include <cstdlib>
#include "../../SDL_internal.h"
#include "SDL_timer.h"
#include "SDL_log.h"

#define OHOS_THREADSAFE_ARG0 0
#define OHOS_THREADSAFE_ARG1 1
#define OHOS_THREADSAFE_ARG2 2
#define OHOS_THREADSAFE_ARG3 3
#define OHOS_THREADSAFE_ARG4 4
#define OHOS_THREADSAFE_ARG5 5

std::unique_ptr<NapiCallbackContext> g_napiCallback = nullptr;

static SDL_Thread *g_sdlMainThread;

typedef void (*OHOS_TS_Fuction)(const cJSON *root);

static void OHOS_TS_ShowTextInput(const cJSON *root);
static void OHOS_TS_RequestPermission(const cJSON *root);
static void OHOS_TS_HideTextInput(const cJSON *root);
static void OHOS_TS_ShouldMinimizeOnFocusLoss(const cJSON *root);
static void OHOS_TS_SetTitle(const cJSON *root);
static void OHOS_TS_SetWindowStyle(const cJSON *root);
static void OHOS_TS_ShowTextInputKeyboard(const cJSON *root);
static void OHOS_TS_SetOrientation(const cJSON *root);
static void OHOS_TS_CreateCustomCursor(const cJSON *root);
static void OHOS_SetSystemCursor(const cJSON *root);
static void OHOS_TS_SetWindowResize(const cJSON *root);

static std::unordered_map<NapiCallBackType, OHOS_TS_Fuction> tsFuctions = {
    {NAPI_CALLBACK_SET_SYSTEMCURSOR, OHOS_SetSystemCursor},
    {NAPI_CALLBACK_SHOW_TEXTINPUT, OHOS_TS_ShowTextInput},
    {NAPI_CALLBACK_HIDE_TEXTINPUT, OHOS_TS_HideTextInput},
    {NAPI_CALLBACK_SHOULD_MINIMIZEON_FOCUSLOSS, OHOS_TS_ShouldMinimizeOnFocusLoss},
    {NAPI_CALLBACK_SET_TITLE, OHOS_TS_SetTitle},
    {NAPI_CALLBACK_SET_WINDOWSTYLE, OHOS_TS_SetWindowStyle},
    {NAPI_CALLBACK_SET_ORIENTATION, OHOS_TS_SetOrientation},
    {NAPI_CALLBACK_SET_WINDOWSTYLE, OHOS_TS_SetWindowStyle},
    {NAPI_CALLBACK_SHOW_TEXTINPUTKEYBOARD, OHOS_TS_ShowTextInputKeyboard},
    {NAPI_CALLBACK_SET_WINDOWRESIZE, OHOS_TS_SetWindowResize},
    {NAPI_CALLBACK_SET_WINDOWRESIZE, OHOS_TS_SetWindowResize},
    {NAPI_CALLBACK_CREATE_CUSTOMCURSOR, OHOS_TS_CreateCustomCursor},
    {NAPI_CALLBACK_REQUEST_PERMISSION, OHOS_TS_RequestPermission}
};

static void OHOS_TS_SetWindowResize(const cJSON *root)
{
    int x;
    int y;
    int w;
    int h;
    cJSON *data = cJSON_GetObjectItem(root, "x");
    x = data->valueint;

    data = cJSON_GetObjectItem(root, "y");
    y = data->valueint;

    data = cJSON_GetObjectItem(root, "w");
    w = data->valueint;

    data = cJSON_GetObjectItem(root, "h");
    h = data->valueint;
    size_t argc = OHOS_THREADSAFE_ARG4;
    napi_value args[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG4] = {nullptr};

    napi_create_int32(g_napiCallback->env, x, &argv[OHOS_THREADSAFE_ARG0]);
    napi_create_int32(g_napiCallback->env, y, &argv[OHOS_THREADSAFE_ARG1]);
    napi_create_int32(g_napiCallback->env, w, &argv[OHOS_THREADSAFE_ARG2]);
    napi_create_int32(g_napiCallback->env, h, &argv[OHOS_THREADSAFE_ARG3]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "nAPISetWindowResize", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG4, argv, nullptr);
}

static void OHOS_TS_ShowTextInput(const cJSON *root)
{
    int x;
    int y;
    int w;
    int h;
    cJSON *data = cJSON_GetObjectItem(root, "x");
    x = data->valueint;

    data = cJSON_GetObjectItem(root, "y");
    y = data->valueint;

    data = cJSON_GetObjectItem(root, "w");
    w = data->valueint;

    data = cJSON_GetObjectItem(root, "h");
    h = data->valueint;

    size_t argc = OHOS_THREADSAFE_ARG4;
    napi_value args[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG4] = {nullptr};

    napi_create_int32(g_napiCallback->env, x, &argv[OHOS_THREADSAFE_ARG0]);
    napi_create_int32(g_napiCallback->env, y, &argv[OHOS_THREADSAFE_ARG1]);
    napi_create_int32(g_napiCallback->env, w, &argv[OHOS_THREADSAFE_ARG2]);
    napi_create_int32(g_napiCallback->env, h, &argv[OHOS_THREADSAFE_ARG3]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "showTextInput", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG4, argv, nullptr);
    return;
}

static void OHOS_TS_RequestPermission(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "permission");
    const char *permission = data->valuestring;
    
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_create_string_utf8(g_napiCallback->env, permission, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "requestPermission", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    
    return;
}

static void OHOS_TS_HideTextInput(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "flag");
    int flag = data->valueint;
    size_t argc = OHOS_THREADSAFE_ARG1;
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_create_int32(g_napiCallback->env, flag, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "hideTextInput", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
}

static void OHOS_TS_ShouldMinimizeOnFocusLoss(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "flag");
    int flag = data->valueint;
    size_t argc = OHOS_THREADSAFE_ARG1;
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_create_int32(g_napiCallback->env, flag, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "shouldMinimizeOnFocusLoss", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
}

static void OHOS_TS_SetTitle(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "title");
    const char *title = data->valuestring;
    size_t argc = OHOS_THREADSAFE_ARG1;
    napi_value args[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_create_string_utf8(g_napiCallback->env, title, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "setTitle", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
}

static void OHOS_TS_SetWindowStyle(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "fullscreen");
    SDL_bool fullscreen = (SDL_bool)data->type;
    size_t argc = OHOS_THREADSAFE_ARG1;
    napi_value args[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};

    napi_get_boolean(g_napiCallback->env, fullscreen, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "setWindowStyle", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
}

static void OHOS_TS_ShowTextInputKeyboard(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "isshow");
    SDL_bool isshow = (SDL_bool)data->type;
    
    size_t argc = OHOS_THREADSAFE_ARG1;
    napi_value args[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};

    napi_get_boolean(g_napiCallback->env, isshow, &argv[OHOS_THREADSAFE_ARG0]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "showTextInput2", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
}

static void OHOS_TS_SetOrientation(const cJSON *root)
{
    int w;
    int h;
    int resizable;
    cJSON *data = cJSON_GetObjectItem(root, "w");
    w = data->valueint;

    data = cJSON_GetObjectItem(root, "h");
    h = data->valueint;

    data = cJSON_GetObjectItem(root, "resizable");
    resizable = data->valueint;

    data = cJSON_GetObjectItem(root, "hint");
    const char *hint = data->valuestring;
    
    size_t argc = OHOS_THREADSAFE_ARG4;
    napi_value args[OHOS_THREADSAFE_ARG4] = {nullptr};
    napi_value argv[OHOS_THREADSAFE_ARG4] = {nullptr};
    napi_create_int32(g_napiCallback->env, w, &argv[OHOS_THREADSAFE_ARG0]);
    napi_create_int32(g_napiCallback->env, h, &argv[OHOS_THREADSAFE_ARG1]);
    napi_create_int32(g_napiCallback->env, resizable, &argv[OHOS_THREADSAFE_ARG2]);
    napi_create_string_utf8(g_napiCallback->env, hint, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG3]);

    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "setOrientation", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG4, argv, nullptr);
}

static void OHOS_TS_CreateCustomCursor(const cJSON *root)
{
    int hotX;
    int hotY;
    int bytesPerPixel;
    int w;
    int h;
    long long xcomponentPixels;
    cJSON *data = cJSON_GetObjectItem(root, "xcomponentpixel");
    xcomponentPixels = static_cast<long long>(data->valuedouble);
    void *xcomponentpixelbuffer = reinterpret_cast<void *>(xcomponentPixels);

    data = cJSON_GetObjectItem(root, "hot_x");
    hotX = data->valueint;

    data = cJSON_GetObjectItem(root, "hot_y");
    hotY = data->valueint;

    data = cJSON_GetObjectItem(root, "BytesPerPixel");
    bytesPerPixel = data->valueint;

    data = cJSON_GetObjectItem(root, "w");
    w = data->valueint;

    data = cJSON_GetObjectItem(root, "h");
    h = data->valueint;
    
    napi_value argv[OHOS_THREADSAFE_ARG3] = {nullptr};
    OhosPixelMapCreateOps createOps;
    createOps.width = w;
    createOps.height = h;
    createOps.pixelFormat = bytesPerPixel;
    createOps.alphaType = 0;
    size_t bufferSize = createOps.width * createOps.height * bytesPerPixel;
    int32_t res = OH_PixelMap_CreatePixelMap(g_napiCallback->env, createOps, (uint8_t *)xcomponentpixelbuffer,
                                             bufferSize, &argv[OHOS_THREADSAFE_ARG0]);
    if (res != IMAGE_RESULT_SUCCESS || argv[OHOS_THREADSAFE_ARG0] == nullptr) {
        SDL_Log("OH_PixelMap_CreatePixelMap is failed");
    }
    napi_create_int32(g_napiCallback->env, hotX, &argv[OHOS_THREADSAFE_ARG1]); // coordinate x
    napi_create_int32(g_napiCallback->env, hotY, &argv[OHOS_THREADSAFE_ARG2]); // coordinate y
    
    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "setCustomCursorandCreate", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG3, argv, nullptr);
    SDL_free(xcomponentpixelbuffer);
    return;
}

static void OHOS_TS_SetCustomCursor(const cJSON *root)
{
    // if use root, delete this line
    (void)root;
    return;
}

static void OHOS_SetSystemCursor(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "cursorID");
    int cursorID = data->valueint;
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    napi_create_int32(g_napiCallback->env, cursorID, &argv[OHOS_THREADSAFE_ARG0]);
    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, "setPointer", &jsMethod);
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    return;
}

/* Relative mouse support */
static SDL_bool OHOS_SupportsRelativeMouse(void)
{
    return SDL_TRUE;
}

static SDL_bool OHOS_SetRelativeMouseEnabled(SDL_bool enabled)
{
    return SDL_TRUE;
}

void OHOS_TS_Call(napi_env env, napi_value jsCb, void *context, void *data)
{
    if (data == nullptr) {
        return;
    }
    cJSON *root = (cJSON*)data;
    cJSON *json = cJSON_GetObjectItem(root, OHOS_TS_CALLBACK_TYPE);
    if (json == nullptr) {
        return;
    }
    NapiCallBackType type = static_cast<NapiCallBackType>(json->valueint);
    OHOS_TS_Fuction fuc = tsFuctions[type];
    fuc(root);
    free(data);
    root = nullptr;
    json = nullptr;
}

/* The general mixing thread function */
static int OHOS_RunMain(void *mainFucInfo)
{
    OhosSDLEntryInfo *info = (OhosSDLEntryInfo *)mainFucInfo;
    void *libraryHandle;
    do {
        libraryHandle = dlopen(info->libraryFile, RTLD_GLOBAL);
        if (!libraryHandle) {
            break;
        }
        SdlMainFunc SDL_main = reinterpret_cast<SdlMainFunc>(dlsym(libraryHandle, info->functionName));
        if (!SDL_main) {
            break;
        }
        SDL_main(info->argcs, info->argvs);
    } while (0);
    for (int i = 0; i < info->argcs; ++i) {
        SDL_free(info->argvs[i]);
    }
    SDL_small_free(info->argvs, SDL_TRUE);
    SDL_free(info->functionName);
    if (libraryHandle != nullptr) {
        dlclose(libraryHandle);
    }
    SDL_free(info->libraryFile);
    SDL_free(info);
    return 0;
}

SDL_bool OHOS_RunThread(OhosSDLEntryInfo *info)
{
    const size_t stacksize = 64 * 1024;
    char threadname[64] = "SDLMain";
    g_sdlMainThread = SDL_CreateThreadInternal(OHOS_RunMain, threadname, stacksize, info);
    return g_sdlMainThread == NULL ? SDL_FALSE : SDL_TRUE;
}

SDL_bool OHOS_IsThreadRun(void)
{
    return g_sdlMainThread == NULL ? SDL_FALSE : SDL_TRUE;
}

void OHOS_ThreadExit(void)
{
    SDL_free(g_sdlMainThread);
    napi_release_threadsafe_function(g_napiCallback->tsfn, napi_tsfn_release);
    g_napiCallback->tsfn = nullptr;
    g_napiCallback = NULL;
}