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
#include "SDL_ohosplugin_c.h"
#include "SDL_ohosplugin.h"

bool OHOS_FindNativeXcomPoment(char *id, OH_NativeXComponent **nativeXComponent)
{
    if (id == NULL) {
        return false;
    }
    std::string strId(id);
    SDL_LockMutex(g_ohosPageMutex);
    OH_NativeXComponent *temp = nullptr;
    bool isFind =  OhosPluginManager::GetInstance()->FindNativeXcomPoment(strId, &temp);
    *nativeXComponent = temp;
    SDL_UnlockMutex(g_ohosPageMutex);
    return isFind;
}

void OHOS_AddXcomPomentIdForThread(char *xCompentId, pthread_t threadId)
{
    std::string strId(xCompentId);
    SDL_LockMutex(g_ohosPageMutex);
    OhosPluginManager::GetInstance()->AddXcomPomentIdForThread(strId, threadId);
    SDL_UnlockMutex(g_ohosPageMutex);
    return;
}

bool OHOS_FindNativeWindow(OH_NativeXComponent *nativeXComponent, SDL_WindowData **window)
{
    SDL_LockMutex(g_ohosPageMutex);
    SDL_WindowData *temp = nullptr;
    bool isFind = OhosPluginManager::GetInstance()->FindNativeWindow(nativeXComponent, &temp);
    *window = temp;
    SDL_UnlockMutex(g_ohosPageMutex);
    return isFind;
}

void OHOS_ClearPluginData(char *id)
{
    std::string strId(id);
    OH_NativeXComponent *c = nullptr;
	
    SDL_LockMutex(g_ohosPageMutex);
    pthread_t t = OhosPluginManager::GetInstance()->GetThreadIdFromXComponentId(strId);
    OhosPluginManager::GetInstance()->FindNativeXcomPoment(strId, &c);
    OhosPluginManager::GetInstance()->ClearPluginManagerData(strId, c, t);
    SDL_UnlockMutex(g_ohosPageMutex);
}
