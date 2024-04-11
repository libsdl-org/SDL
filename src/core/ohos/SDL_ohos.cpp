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

#ifdef __OHOS__

#include "node_api.h"
#include "SDL_napi.h"
#include "SDL_log.h"
#include <rawfile/raw_file_manager.h>
#include <unistd.h>
#include <hilog/log.h>
#include <dlfcn.h>
#include "cJSON.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "SDL.h"
#include "../../video/SDL_sysvideo.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_quit.h"
#ifdef __cplusplus
}
#endif

#include "SDL_ohos.h"
#include "SDL_ohosfile.h"
#include "../../video/ohos/SDL_ohosvideo.h"
#include "../../video/ohos/SDL_ohoskeyboard.h"
#include "../../audio/ohos/SDL_ohosaudio.h"
#define OHOS_DELAY_FIFTY 50
#define OHOS_DELAY_TEN 10
#define OHOS_START_ARGS_INDEX 2
#define OHOS_INDEX_ARG0 0
#define OHOS_INDEX_ARG1 1
#define OHOS_INDEX_ARG2 2
#define OHOS_INDEX_ARG3 3
#define OHOS_INDEX_ARG4 4
#define OHOS_INDEX_ARG5 5

using namespace std;
using namespace OHOS::SDL;

SDL_DisplayOrientation displayOrientation;

SDL_atomic_t bPermissionRequestPending;
SDL_bool bPermissionRequestResult;
static SDL_atomic_t bQuit;

/* Lock / Unlock Mutex */
void OHOS_PAGEMUTEX_Lock()
{
    SDL_LockMutex(OHOS_PageMutex);
}

void OHOS_PAGEMUTEX_Unlock()
{
    SDL_UnlockMutex(OHOS_PageMutex);
}

void OHOS_PAGEMUTEX_LockRunning()
{
    int pauseSignaled = 0;
    int resumeSignaled = 0;
retry:
    SDL_LockMutex(OHOS_PageMutex);
    pauseSignaled = SDL_SemValue(OHOS_PauseSem);
    resumeSignaled = SDL_SemValue(OHOS_ResumeSem);
    if (pauseSignaled > resumeSignaled) {
        SDL_UnlockMutex(OHOS_PageMutex);
        SDL_Delay(OHOS_DELAY_FIFTY);
        goto retry;
    }
}

void OHOS_SetDisplayOrientation(int orientation)
{
    displayOrientation = (SDL_DisplayOrientation)orientation;
}

SDL_DisplayOrientation OHOS_GetDisplayOrientation()
{
    return displayOrientation;
}

void OHOS_NAPI_SetWindowResize(int x, int y, int w, int h)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_WINDOWRESIZE);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShowTextInput(int x, int y, int w, int h)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOW_TEXTINPUT);
    cJSON_AddNumberToObject(root, "x", x);
    cJSON_AddNumberToObject(root, "y", y);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

SDL_bool OHOS_NAPI_RequestPermission(const char *permission)
{
    /* Wait for any pending request on another thread */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(OHOS_DELAY_TEN);
    }
    SDL_AtomicSet(&bPermissionRequestPending, SDL_TRUE);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_REQUEST_PERMISSION);
    cJSON_AddStringToObject(root, "permission", permission);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    /* Wait for the request to complete */
    while (SDL_AtomicGet(&bPermissionRequestPending) == SDL_TRUE) {
        SDL_Delay(OHOS_DELAY_TEN);
    }
    return bPermissionRequestResult;
}

void OHOS_NAPI_HideTextInput(int flag)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_HIDE_TEXTINPUT);
    cJSON_AddNumberToObject(root, "flag", flag);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShouldMinimizeOnFocusLoss(int flag)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOULD_MINIMIZEON_FOCUSLOSS);
    cJSON_AddNumberToObject(root, "flag", flag);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetTitle(const char *title)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_TITLE);
    cJSON_AddStringToObject(root, "title", title);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetWindowStyle(SDL_bool fullscreen)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_WINDOWSTYLE);
    cJSON_AddBoolToObject(root, "fullscreen", fullscreen);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_ShowTextInputKeyboard(SDL_bool isshow)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SHOW_TEXTINPUTKEYBOARD);
    cJSON_AddBoolToObject(root, "isshow", isshow);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

void OHOS_NAPI_SetOrientation(int w, int h, int resizable, const char *hint)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_ORIENTATION);
    cJSON_AddNumberToObject(root, "w", w);
    cJSON_AddNumberToObject(root, "h", h);
    cJSON_AddNumberToObject(root, "resizable", resizable);
    cJSON_AddStringToObject(root, "hint", hint);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
}

int OHOS_CreateCustomCursor(SDL_Surface *xcomponent, int hotX, int hotY)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return -1;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_CREATE_CUSTOMCURSOR);
    cJSON_AddNumberToObject(root, "hot_x", hotX);
    cJSON_AddNumberToObject(root, "hot_y", hotY);
    cJSON_AddNumberToObject(root, "BytesPerPixel", xcomponent->format->BytesPerPixel);
    cJSON_AddNumberToObject(root, "w", xcomponent->w);
    cJSON_AddNumberToObject(root, "h", xcomponent->h);
    size_t bufferSize = xcomponent->w * xcomponent->h * xcomponent->format->BytesPerPixel;
    void *buff = SDL_malloc(bufferSize);
    SDL_memcpy(buff, xcomponent->pixels, bufferSize);
    long long xcomponentpixel = (long long)buff;
    cJSON_AddNumberToObject(root, "xcomponentpixel", (double)xcomponentpixel);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return 1;
}

SDL_bool OHOS_SetCustomCursor(int cursorID)
{
    if (SDL_AtomicGet(&bQuit) == SDL_TRUE) {
        return SDL_TRUE;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, "cursorID", cursorID);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_FALSE;
}

SDL_bool OHOS_SetSystemCursor(int cursorID)
{
    if (SDL_AtomicGet(&bQuit) == SDL_TRUE) {
        return SDL_TRUE;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Error creating JSON object.");
        return SDL_FALSE;
    }
    cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_SET_SYSTEMCURSOR);
    cJSON_AddNumberToObject(root, "cursorID", cursorID);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        cJSON_free(root);
    }
    return SDL_TRUE;
}

/* Relative mouse support */
SDL_bool OHOS_SupportsRelativeMouse(void)
{
    return SDL_TRUE;
}

SDL_bool OHOS_SetRelativeMouseEnabled(SDL_bool enabled)
{
    return SDL_TRUE;
}

napi_value SDLNapi::OHOS_SetResourceManager(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[OHOS_INDEX_ARG2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], g_path, 0, &len);
    
    if (g_path != nullptr) {
        delete g_path;
        g_path = nullptr;
    }
    
    g_path = new char[len + 1];
    napi_get_value_string_utf8(env, args[0], g_path, len + 1, &len);
    
    gCtx = SDL_AllocRW();
    NativeResourceManager *nativeResourceManager = OH_ResourceManager_InitNativeResourceManager(env, args[1]);
    gCtx->hidden.ohosio.nativeResourceManager = nativeResourceManager;
    return nullptr;
}

napi_value SDLNapi::OHOS_NativeSetScreenResolution(napi_env env, napi_callback_info info)
{
    size_t argc = 6;
    napi_value args[6];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    int xcomponentWidth;
    int xcomponentHeight;
    int deviceWidth;
    int deviceHeight;
    int format;
    double rate;
    napi_get_value_int32(env, args[OHOS_INDEX_ARG0], &xcomponentWidth);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG1], &xcomponentHeight);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG2], &deviceWidth);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG3], &deviceHeight);
    napi_get_value_int32(env, args[OHOS_INDEX_ARG4], &format);
    napi_get_value_double(env, args[OHOS_INDEX_ARG5], &rate);
    SDL_LockMutex(OHOS_PageMutex);
    OHOS_SetScreenResolution(deviceWidth, deviceHeight, format, rate);
    SDL_UnlockMutex(OHOS_PageMutex);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeResize(napi_env env, napi_callback_info info)
{
    SDL_LockMutex(OHOS_PageMutex);
    if (g_ohosWindow) {
        OHOS_SendResize(g_ohosWindow);
    }
    SDL_UnlockMutex(OHOS_PageMutex);
    return nullptr;
}

napi_value SDLNapi::OHOS_TextInput(napi_env env, napi_callback_info info)
{
    size_t requireArgc = 2;
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    char* inputBuffer = nullptr;
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    napi_valuetype valuetype0;
    napi_typeof(env, args[0], &valuetype0);
    
    napi_valuetype valuetype1;
    napi_typeof(env, args[1], &valuetype1);
    
	//atkts传过来的文本框的内容的长度
    int bufSize;
    napi_get_value_int32(env, args[0], &bufSize);
    size_t stringLength = bufSize + 1;
    size_t length;
    
    inputBuffer = new char[stringLength];
    
    napi_get_value_string_utf8(env, args[1], inputBuffer, stringLength, &length);
    
    SDL_Event keyEvent;
    keyEvent.type = SDL_KEYDOWN;
    keyEvent.key.keysym.sym = SDLK_RETURN;
    SDL_PushEvent(&keyEvent);

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(SDL_Event)); // 清空event结构体
    event.type = SDL_TEXTINPUT;
    
    SDL_strlcpy(event.text.text, inputBuffer, sizeof(inputBuffer));
    
    SDL_PushEvent(&event); // 推送事件到事件队列
    
    delete inputBuffer;
    return nullptr;
}

napi_value SDLNapi::OHOS_KeyDown(napi_env env, napi_callback_info info)
{
    int keycode;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &keycode);
    OHOS_OnKeyDown(keycode);
    return nullptr;
}

napi_value SDLNapi::OHOS_KeyUp(napi_env env, napi_callback_info info)
{
    int keycode;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &keycode);
    OHOS_OnKeyUp(keycode);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeKeyboardFocusLost(napi_env env, napi_callback_info info)
{
    SDL_StopTextInput();
    return nullptr;
}

static void OHOS_NativeQuit(void)
{
    const char *str;
    if (OHOS_PageMutex) {
        SDL_DestroyMutex(OHOS_PageMutex);
        OHOS_PageMutex = nullptr;
    }

    if (OHOS_PauseSem) {
        SDL_DestroySemaphore(OHOS_PauseSem);
        OHOS_PauseSem = nullptr;
    }

    if (OHOS_ResumeSem) {
        SDL_DestroySemaphore(OHOS_ResumeSem);
        OHOS_ResumeSem = nullptr;
    }

    str = SDL_GetError();
    if (str && str[0]) {
    } else {
    }
    return;
}

napi_value SDLNapi::OHOS_NativeSendQuit(napi_env env, napi_callback_info info)
{
    SDL_AtomicSet(&bQuit, SDL_TRUE);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    napi_value sum = 0;
    SDL_SendQuit();
    SDL_SendAppEvent(SDL_APP_TERMINATING);
    OHOS_ThreadExit();
    OHOS_NativeQuit();
    while (SDL_SemTryWait(OHOS_PauseSem) == 0) {
    }
    SDL_SemPost(OHOS_ResumeSem);
    return nullptr;
}

napi_value SDLNapi::OHOS_NativeResume(napi_env env, napi_callback_info info)
{
    SDL_SemPost(OHOS_ResumeSem);
    OHOSAUDIO_PageResume();
    return nullptr;
}

napi_value SDLNapi::OHOS_NativePause(napi_env env, napi_callback_info info)
{
    SDL_SemPost(OHOS_PauseSem);
    OHOSAUDIO_PagePause();
    return nullptr;
}

napi_value SDLNapi::OHOS_NativePermissionResult(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    bool result;
    napi_get_value_bool(env, args[0], &result);
    bPermissionRequestResult = result ? SDL_TRUE : SDL_FALSE;
    SDL_AtomicSet(&bPermissionRequestPending, SDL_FALSE);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeOrientationChanged(napi_env env, napi_callback_info info)
{
    int orientation;
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_get_value_int32(env, args[0], &orientation);
    SDL_LockMutex(OHOS_PageMutex);
    OHOS_SetDisplayOrientation(orientation);
    if (g_ohosWindow) {
        SDL_VideoDisplay *display = SDL_GetDisplay(0);
        SDL_SendDisplayEvent(display, SDL_DISPLAYEVENT_ORIENTATION, orientation);
    }
    SDL_UnlockMutex(OHOS_PageMutex);
    return nullptr;
}

napi_value SDLNapi::OHOS_OnNativeFocusChanged(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    bool focus;
    napi_get_value_bool(env, args[0], &focus);
    if (g_ohosWindow) {
        SDL_SendWindowEvent(g_ohosWindow,
            (focus = SDL_TRUE ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST), 0, 0);
    }
    return nullptr;
}

static void OHOS_NAPI_NativeSetup(void)
{
    SDL_setenv("SDL_VIDEO_GL_DRIVER", "libGLESv3.so", 1);
    SDL_setenv("SDL_VIDEO_EGL_DRIVER", "libEGL.so", 1);
    SDL_setenv("SDL_ASSERT", "ignore", 1);
    SDL_AtomicSet(&bPermissionRequestPending, SDL_FALSE);
    SDL_AtomicSet(&bQuit, SDL_FALSE);
    return;
}

static napi_value OHOS_NAPI_Init(napi_env env, napi_callback_info info)
{
    if (g_napiCallback == nullptr) {
        g_napiCallback = std::make_unique<NapiCallbackContext>();
    }
    OHOS_NAPI_NativeSetup();
    g_napiCallback->env = env;
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    napi_create_reference(env, args[0], 1, &g_napiCallback->callbackRef);

    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "SDLThreadSafe", NAPI_AUTO_LENGTH, &resourceName);

    napi_create_threadsafe_function(env, args[0], nullptr, resourceName, 0, 1, nullptr, nullptr, nullptr, OHOS_TS_Call,
                                    &g_napiCallback->tsfn);
    return nullptr;
}

static int OHOS_NAPI_GetInfo(napi_env &env, napi_callback_info &info, napi_value *argv, char **library_file, char **function_name)
{
    napi_status status;
    napi_valuetype valuetype;
    size_t buffer_size;
    size_t argc = 10;
    
    status = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (status != napi_ok) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():failed to obtained argument!");
        return -1;
    }
    
    status = napi_typeof(env, argv[0], &valuetype);
    if (status != napi_ok || valuetype != napi_string) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
        return -1;
    }
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &buffer_size);
    *library_file = new char[buffer_size];
    napi_get_value_string_utf8(env, argv[0], *library_file, buffer_size + 1, &buffer_size);

    status = napi_typeof(env, argv[1], &valuetype);
    if (status != napi_ok || valuetype != napi_string) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
        SDL_free(library_file);
        return -1;
    }
    napi_get_value_string_utf8(env, argv[1], nullptr, 0, &buffer_size);
    *function_name = new char[buffer_size];
    napi_get_value_string_utf8(env, argv[1], *function_name, buffer_size + 1, &buffer_size);
    return argc;
}

static int OHOS_NAPI_SetArgs(napi_env& env, char **argvs, int &argcs, size_t &argc, napi_value* argv)
{
    napi_status status;
    napi_valuetype valuetype;
    size_t buffer_size;
    int i = 2;
    
    argvs[argcs++] = SDL_strdup("SDL_main");
    for (i = OHOS_START_ARGS_INDEX; i < argc; ++i) {
        char *arg = NULL;
        status = napi_typeof(env, argv[i], &valuetype);
        if (status != napi_ok || valuetype != napi_string) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLAppEntry():invalid type of argument!");
            break;
        }
        napi_get_value_string_utf8(env, argv[i], nullptr, 0, &buffer_size);
        arg = new char[buffer_size + 1];
        SDL_memset(arg, 0, buffer_size + 1);
        napi_get_value_string_utf8(env, argv[i], arg, buffer_size + 1, &buffer_size);
        if (!arg) {
            delete[] arg;
            arg = SDL_strdup("");
        }
        argvs[argcs++] = arg;
    }
    return i;
}

static napi_value OHOS_NAPI_SDLAppEntry(napi_env env, napi_callback_info info)
{
    char *library_file;
    char *function_name;
    napi_value argv[10];
    size_t argc = 10;
    char **argvs;
    int argcs = 0;
    bool isstack;
    int i;

    if (OHOS_IsThreadRun() == SDL_TRUE) {
        return nullptr;
    }

    argc = OHOS_NAPI_GetInfo(env, info, argv, &library_file, &function_name);
    if (argc == -1)
        return nullptr;
    argvs = SDL_small_alloc(char *, argc, &isstack);
    i = OHOS_NAPI_SetArgs(env, argvs, argcs, argc, argv);

    bool isRunThread = true;
    OhosSDLEntryInfo *entry = NULL;
    if (i == argc) {
        argvs[argcs] = NULL;
        entry = (OhosSDLEntryInfo *)SDL_malloc(sizeof(OhosSDLEntryInfo));
        if (entry != NULL) {
            entry->argcs = argcs;
            entry->argvs = argvs;
            entry->functionName = function_name;
            entry->libraryFile = library_file;
            isRunThread = OHOS_RunThread(entry);
        } else {
            isRunThread = false;
        }
    }

    // Not run SDL thread succuss then free memory, if run SDL thread succuss, SDL thread deal the memory.
    if (!isRunThread) {
        for (i = 0; i < argcs; ++i) {
            SDL_free(argvs[i]);
        }
        SDL_small_free(argvs, isstack);
        SDL_free(info);
        SDL_free(function_name);
        if (library_file != nullptr) {
            dlclose(library_file);
            SDL_free(library_file);
            library_file = nullptr;
        }
        SDL_free(entry);
    }

    return nullptr;
}


napi_value SDLNapi::Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"sdlAppEntry", nullptr, OHOS_NAPI_SDLAppEntry, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativeSetScreenResolution", nullptr, OHOS_NativeSetScreenResolution, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"onNativeResize", nullptr, OHOS_OnNativeResize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"textInput", nullptr, OHOS_TextInput, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"keyDown", nullptr, OHOS_KeyDown, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"keyUp", nullptr, OHOS_KeyUp, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onNativeKeyboardFocusLost", nullptr, OHOS_OnNativeKeyboardFocusLost, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"nativeSendQuit", nullptr, OHOS_NativeSendQuit, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativeResume", nullptr, OHOS_NativeResume, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativePause", nullptr, OHOS_NativePause, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"nativePermissionResult", nullptr, OHOS_NativePermissionResult, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"onNativeOrientationChanged", nullptr, OHOS_OnNativeOrientationChanged, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"setResourceManager", nullptr, OHOS_SetResourceManager, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onNativeFocusChanged", nullptr, OHOS_OnNativeFocusChanged, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"init", nullptr, OHOS_NAPI_Init, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    OHOS_XcomponentExport(env, exports);
    return exports;
}

EXTERN_C_START
static napi_value SDLNapiInit(napi_env env, napi_value exports)
{
    return SDLNapi::Init(env, exports);
}
EXTERN_C_END

napi_module OHOSNapiModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = SDLNapiInit,
    .nm_modname = "SDL2d",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&OHOSNapiModule);
}

#endif /* __OHOS__ */

/* vi: set ts=4 sw=4 expandtab: */
