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

#ifndef SDL_OHOSPLUGIN_H
#define SDL_OHOSPLUGIN_H
#include <string>
#include <unordered_map>

#include <js_native_api.h>
#include <js_native_api_types.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
extern "C" {
#include "../../thread/SDL_systhread.h"
}
#include "../../video/ohos/SDL_ohosvideo.h"
#include "SDL_ohoshead.h"

class OhosPluginManager {
public:
    ~OhosPluginManager();

    static OhosPluginManager *GetInstance()
    {
        return &OhosPluginManager::pluginManager;
    }

    void AddXcomPomentIdForThread(std::string &xCompentId, pthread_t threadId);
    void SetNativeXComponent(std::string &id, OH_NativeXComponent *nativeXComponent);
    bool FindNativeXcomPoment(std::string &id, OH_NativeXComponent **nativeXComponent);
    bool FindNativeWindow(OH_NativeXComponent *nativeXComponent, SDL_WindowData **window);
    OhosThreadLock* CreateOhosThreadLock(const pthread_t threadId);
    void DestroyOhosThreadLock(OhosThreadLock *threadLock);

    void SetNativeXComponentList(OH_NativeXComponent*, SDL_WindowData *);
    pthread_t  GetThreadIdFromXComponentId(std::string &id);
    OhosThreadLock *GetOhosThreadLockFromThreadId(pthread_t threadId);
    int ClearPluginManagerData(std::string &id, OH_NativeXComponent *component, pthread_t threadId);
    
    SDL_WindowData* GetWindowDataByXComponent(OH_NativeXComponent *component);

private:
    static OhosPluginManager pluginManager;

    std::unordered_map<std::string, OH_NativeXComponent *> nativeXComponentMap;
    std::unordered_map<pthread_t, std::vector<std::string>> threadXcompentList;
    std::unordered_map<pthread_t, OhosThreadLock *> ohosThreadLocks;
    std::unordered_map<OH_NativeXComponent *, SDL_WindowData *> nativeXComponentList;
};
#endif // SDL_OHOSPLUGIN_H
