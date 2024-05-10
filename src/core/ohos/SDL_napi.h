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

#ifndef SDL_NAPI_H
#define SDL_NAPI_H

#include "SDL_ohosthreadsafe.h"

namespace OHOS {
namespace SDL {

class SDLNapi {
public:
    static napi_value Init(napi_env env, napi_value exports);
private:
    SDLNapi() = default;
    ~SDLNapi() = default;
    static napi_value OHOS_NativeSetScreenResolution(napi_env env, napi_callback_info info);
    static napi_value OHOS_OnNativeResize(napi_env env, napi_callback_info info);
    static napi_value OHOS_KeyDown(napi_env env, napi_callback_info info);
    static napi_value OHOS_KeyUp(napi_env env, napi_callback_info info);
    static napi_value OHOS_OnNativeKeyboardFocusLost(napi_env env, napi_callback_info info);
    static napi_value OHOS_NativeSendQuit(napi_env env, napi_callback_info info);
    static napi_value OHOS_NativeResume(napi_env env, napi_callback_info info);
    static napi_value OHOS_NativePause(napi_env env, napi_callback_info info);
    static napi_value OHOS_NativePermissionResult(napi_env env, napi_callback_info info);
    static napi_value OHOS_OnNativeOrientationChanged(napi_env env, napi_callback_info info);
    static napi_value OHOS_SetResourceManager(napi_env env, napi_callback_info info);
    static napi_value OHOS_OnNativeFocusChanged(napi_env env, napi_callback_info info);
    static napi_value OHOS_TextInput(napi_env env, napi_callback_info info);
    static napi_value OHOS_SetWindowId(napi_env env, napi_callback_info info);
    static napi_value OHOS_SetRootViewControl(napi_env env, napi_callback_info info);
};
} // namespace SDL
} // namespace OHOS

#endif /* SDL_NAPI_H */

/* vi: set ts=4 sw=4 expandtab: */
