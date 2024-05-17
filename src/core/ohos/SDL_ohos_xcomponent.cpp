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
#include "SDL_mutex.h"

#ifdef __OHOS__

#include <js_native_api.h>
#include <js_native_api_types.h>
#include <hilog/log.h>

#include "SDL_stdinc.h"
#include "SDL_assert.h"
#include "SDL_atomic.h"
#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_main.h"
#include "SDL_timer.h"

#include "SDL_system.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "../../events/SDL_events_c.h"
#include "../../video/SDL_egl_c.h"
#ifdef __cplusplus
}
#endif

#include "../../video/ohos/SDL_ohostouch.h"
#include "../../video/ohos/SDL_ohosmouse.h"
#include "../../video/ohos/SDL_ohoskeyboard.h"
#include "../../video/ohos/SDL_ohosvideo.h"

#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include "SDL_ohos_xcomponent.h"
#include "adapter_c/adapter_c.h"
#include "SDL_ohosplugin.h"

#define OHOS_DELAY_TEN 10

static OH_NativeXComponent_Callback callback;
static OH_NativeXComponent_MouseEvent_Callback mouseCallback;

static std::string GetXComponentIdByNative(OH_NativeXComponent *component)
{
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NATIVEXCOMPONENT_RESULT_SUCCESS != OH_NativeXComponent_GetXComponentId(component, idStr, &idSize)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Export: OH_NativeXComponent_GetXComponentId fail");
        return "";
    }
    std::string curXComponentId(idStr);
    return curXComponentId;
}

static SDL_Window *GetWindowFromXComponent(OH_NativeXComponent *component)
{
    std::string curXComponentId = GetXComponentIdByNative(component);
    if (curXComponentId.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "get xComponent error");
        return nullptr;
    }
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this == nullptr) {
        return nullptr;
    }
    SDL_Window *resultWindow = nullptr;
    SDL_Window *curWindow = _this->windows;
    while (curWindow) {
        if (curWindow->xcompentId == nullptr) {
            curWindow = curWindow->next;
            continue;
        }
        std::string xComponentId(curWindow->xcompentId);
        if (xComponentId == curXComponentId) {
            resultWindow = curWindow;
            break;
        }
        curWindow = curWindow->next;
    }
    return resultWindow;
}

static void setWindowDataValue(SDL_WindowData *data, uint64_t width, uint64_t height,
                               double offsetX, double offsetY, void *native_window)
{
    data->height = height;
    data->width = width;
    data->x = offsetX;
    data->y = offsetY;
    data->native_window = (OHNativeWindow *)(native_window);
    if (data->native_window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not fetch native window from UI thread");
    }
}

/* Callbacks*/
static void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window)
{
    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);

    std::string curXComponentId = GetXComponentIdByNative(component);
    if (curXComponentId.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "get xComponent error");
        return;
    }
    SDL_Log("Xcompent is created, component is %s, nativewidow is %p", curXComponentId.c_str(), window);

    SDL_LockMutex(OHOS_PageMutex);
    SDL_WindowData *data = OhosPluginManager::GetInstance()->GetWindowDataByXComponent(component);
    if (data == nullptr) {
        SDL_WindowData *data = (SDL_WindowData *)SDL_malloc(sizeof(SDL_WindowData));
        setWindowDataValue(data, width, height, offsetX, offsetY, window);
        OhosPluginManager::GetInstance()->SetNativeXComponentList(component, data);
    } else {
        setWindowDataValue(data, width, height, offsetX, offsetY, window);
    }
    
    long threadId = OhosPluginManager::GetInstance()->GetThreadIdFromXComponentId(curXComponentId);
    if (threadId == -1) {
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    OhosThreadLock *lock = OhosPluginManager::GetInstance()->GetOhosThreadLockFromThreadId(threadId);
    if (lock == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Get this threadId: %ld lock error", threadId);
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    SDL_CondBroadcast(lock->mCond);
    SDL_UnlockMutex(OHOS_PageMutex);
}

static void OnSurfaceChangedCB(OH_NativeXComponent *component, void *window)
{
    uint64_t width;
    uint64_t height;
    double offsetX;
    double offsetY;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    OH_NativeXComponent_GetXComponentOffset(component, window, &offsetX, &offsetY);
    SDL_Log("Xcompent is changeing, xcomponent is %p", component);

    SDL_LockMutex(OHOS_PageMutex);
    SDL_WindowData *data = OhosPluginManager::GetInstance()->GetWindowDataByXComponent(component);
    if (data != nullptr) {
        setWindowDataValue(data, width, height, offsetX, offsetY, window);
    }

    SDL_Window *curWindow = GetWindowFromXComponent(component);
    if (curWindow != nullptr) {
        OHOS_SendResize(curWindow);
        if (data->egl_xcomponent == EGL_NO_SURFACE) {
            data->egl_xcomponent = SDL_EGL_CreateSurface(SDL_GetVideoDevice(), (NativeWindowType)data->native_window);
        }
        return;
    }
    SDL_UnlockMutex(OHOS_PageMutex);
}

static void OnSurfaceDestroyedCB(OH_NativeXComponent *component, void *window) {
    int nb_attempt = 50;
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {'\0'};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;

    SDL_Log("Xcompent is destroying, component is %p.", component);
    std::string curXComponentId = GetXComponentIdByNative(component);
    if (curXComponentId.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "get xComponent error");
        return;
    }

retry:
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *curWindow = GetWindowFromXComponent(component);
    if (curWindow == nullptr) {
        return;
    }
    
    SDL_LockMutex(OHOS_PageMutex);
    SDL_WindowData *data = (SDL_WindowData *)curWindow->driverdata;
    if (data != nullptr && !data->backup_done) {
        nb_attempt -= 1;
        if (nb_attempt == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Try to release egl_xcomponent\
                with context probably still active");
        } else {
            SDL_Delay(OHOS_DELAY_TEN);
            SDL_UnlockMutex(OHOS_PageMutex);
            goto retry;
        }
    }
    if (data->egl_xcomponent != EGL_NO_SURFACE) {
        SDL_EGL_DestroySurface(_this, data->egl_xcomponent);
        data->egl_xcomponent = EGL_NO_SURFACE;
    }

    if (data->native_window) {
        SDL_free(data->native_window);
        data->native_window = NULL;
    }
    data->height = data->width = 0;
    data->x = data->y = 0;

    long xComponentThreadId = OhosPluginManager::GetInstance()->GetThreadIdFromXComponentId(curXComponentId);
    if (xComponentThreadId == -1) {
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    if (OhosPluginManager::GetInstance()->ClearPluginManagerData(curXComponentId, component, xComponentThreadId) ==
        -1) {
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    SDL_UnlockMutex(OHOS_PageMutex);
}

/* Key */
void onKeyEvent(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_KeyEvent *keyEvent = NULL;
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) >= 0) {
        OH_NativeXComponent_KeyAction action;
        OH_NativeXComponent_KeyCode code;
        OH_NativeXComponent_EventSourceType sourceType;

        OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
        OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);

        if (OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN == action) {
            OHOS_OnKeyDown(code);
        } else if (OH_NATIVEXCOMPONENT_KEY_ACTION_UP == action) {
            OHOS_OnKeyUp(code);
        }
    }
}

/* Touch */
void onNativeTouch(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_TouchEvent touchEvent;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    OH_NativeXComponent_TouchPointToolType toolType = OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN;

    SDL_LockMutex(OHOS_PageMutex);
    OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
    OH_NativeXComponent_GetTouchPointToolType(component, 0, &toolType);
    tiltX = touchEvent.x;
    tiltY = touchEvent.y;

    OhosTouchId ohosTouch;
    ohosTouch.touchDeviceIdIn = touchEvent.deviceId;
    ohosTouch.pointerFingerIdIn = touchEvent.id;
    ohosTouch.action = touchEvent.type;
    ohosTouch.x = tiltX;
    ohosTouch.y = tiltY;
    ohosTouch.p = touchEvent.force;
    
    SDL_Window* curWindow = GetWindowFromXComponent(component);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Get cur window error");
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    OHOS_OnTouch(curWindow, &ohosTouch);

    SDL_UnlockMutex(OHOS_PageMutex);
}

/* Mouse */
void onNativeMouse(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_MouseEvent mouseEvent;
    OHOSWindowSize windowsize;
    int32_t ret = OH_NativeXComponent_GetMouseEvent(component, window, &mouseEvent);
    SDL_LockMutex(OHOS_PageMutex);
    
    windowsize.action = mouseEvent.action;
    windowsize.state = mouseEvent.button;
    windowsize.x = mouseEvent.x;
    windowsize.y = mouseEvent.y;

    SDL_Window *curWindow = GetWindowFromXComponent(component);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Get cur window error");
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    OHOS_OnMouse(curWindow, &windowsize, SDL_TRUE);
    SDL_UnlockMutex(OHOS_PageMutex);
}

static void OnDispatchTouchEventCB(OH_NativeXComponent *component, void *window)
{
    OH_NativeXComponent_TouchEvent touchEvent;
    OhosTouchId ohosTouch;
    int32_t ret = OH_NativeXComponent_GetTouchEvent(component, window, &touchEvent);
    SDL_LockMutex(OHOS_PageMutex);
    ohosTouch.touchDeviceIdIn = touchEvent.deviceId;
    ohosTouch.pointerFingerIdIn = touchEvent.id;
    ohosTouch.action = touchEvent.type;
    ohosTouch.x = touchEvent.x;
    ohosTouch.y = touchEvent.y;
    ohosTouch.p = touchEvent.force;
    SDL_Window *curWindow = GetWindowFromXComponent(component);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Get cur window error");
        SDL_UnlockMutex(OHOS_PageMutex);
        return;
    }
    OHOS_OnTouch(curWindow, &ohosTouch);
    SDL_UnlockMutex(OHOS_PageMutex);
}

void OnHoverEvent(OH_NativeXComponent *component, bool isHover)
{
}

void OnFocusEvent(OH_NativeXComponent *component, void *window)
{
}

void OnBlurEvent(OH_NativeXComponent *component, void *window)
{
}

void OHOS_XcomponentExport(napi_env env, napi_value exports)
{
    napi_value exportInstance = NULL;
    OH_NativeXComponent *nativeXComponent = NULL;
    if ((NULL == env) || (NULL == exports)) {
        return;
    }
    
    if (napi_ok != napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance)) {
        return;
    }
    
    if (napi_ok != napi_unwrap(env, exportInstance, (void **)(&nativeXComponent))) {
        return;
    }
    std::string xComponentId = GetXComponentIdByNative(nativeXComponent);
    if (xComponentId.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "get xComponent error");
        return;
    }

    SDL_Log("Xcompent js callback is coming, xcompent id is %d.", xComponentId.c_str());

    SDL_LockMutex(OHOS_PageMutex);
    OhosPluginManager::GetInstance()->SetNativeXComponent(xComponentId, nativeXComponent);
    long threadId = OhosPluginManager::GetInstance()->GetThreadIdFromXComponentId(xComponentId);
    if (threadId != -1) {
        OhosThreadLock *lock = OhosPluginManager::GetInstance()->GetOhosThreadLockFromThreadId(threadId);
        if (lock != nullptr) {
            SDL_CondBroadcast(lock->mCond);
        }
    }
    SDL_UnlockMutex(OHOS_PageMutex);

    callback.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback.OnSurfaceChanged = OnSurfaceChangedCB;
    callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback.DispatchTouchEvent = onNativeTouch;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);

    mouseCallback.DispatchMouseEvent = OnDispatchTouchEventCB;
    mouseCallback.DispatchMouseEvent = onNativeMouse;
    mouseCallback.DispatchHoverEvent = OnHoverEvent;
    OH_NativeXComponent_RegisterMouseEventCallback(nativeXComponent, &mouseCallback);

    OH_NativeXComponent_RegisterKeyEventCallback(nativeXComponent, onKeyEvent);
    OH_NativeXComponent_RegisterFocusEventCallback(nativeXComponent, OnFocusEvent);
    OH_NativeXComponent_RegisterBlurEventCallback(nativeXComponent, OnBlurEvent);
    return;
}

#endif /* __OHOS__ */

/* vi: set ts=4 sw=4 expandtab: */
