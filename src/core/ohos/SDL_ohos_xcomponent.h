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

#ifndef SDL_OHOSXCOMPONENT_H
#define SDL_OHOSXCOMPONENT_H

#include <native_window/external_window.h>
#include "napi/native_api.h"


extern void OHOS_XcomponentExport(napi_env env, napi_value exports);
extern OHNativeWindow *gNative_window;

#endif /* SDL_OHOSXCOMPONENT_H */

/* vi: set ts=4 sw=4 expandtab: */