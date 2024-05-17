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

#ifndef SDL_OHOSPLUGIN_C_H
#define SDL_OHOSPLUGIN_C_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include "SDL_ohoshead.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
bool OHOS_FindNativeXcomPoment(char *id, OH_NativeXComponent **nativeXComponent);

bool OHOS_FindNativeWindow(OH_NativeXComponent *nativeXComponent, SDL_WindowData **window);

OhosThreadLock* OHOS_CreateThreadLock(long *id); 

void OHOS_AddXcomPomentIdForThread(char *xCompentId, pthread_t threadId);

void OHOS_FindOrCreateThreadLock(pthread_t id, OhosThreadLock **lock);
/* *INDENT-ON* */
#endif

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif // SDL_OHOSPLUGIN_C_H
