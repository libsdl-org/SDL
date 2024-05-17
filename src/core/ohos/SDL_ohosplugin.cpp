/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <ace/xcomponent/native_interface_xcomponent.h>

#include "SDL_log.h"
#include "SDL_ohosplugin.h"

OhosPluginManager OhosPluginManager::pluginManager;
static SDL_bool windowCondition = SDL_FALSE;
OhosPluginManager::~OhosPluginManager()
{
    for (auto iter = nativeXComponentMap.begin(); iter != nativeXComponentMap.end(); ++iter) {
        if (nullptr != iter->second) {
            delete iter->second;
            iter->second = nullptr;
        }
    }
    nativeXComponentMap.clear();
}

void OhosPluginManager::SetNativeXComponent(std::string &id, OH_NativeXComponent *nativeXComponent)
{
    if (nullptr == nativeXComponent) {
        return;
    }

    nativeXComponentMap[id] = nativeXComponent;
}

void OhosPluginManager::AddXcomPomentIdForThread(std::string &xCompentId, pthread_t threadId)
{
    auto it = threadXcompentList.find(threadId);
    if (it != threadXcompentList.end()) {
        std::vector<std::string> &values = it->second;
        auto itVec = std::find(values.begin(), values.end(), xCompentId);
        if (itVec == values.end()) {
            values.push_back(xCompentId);
        }
    } else {
        std::vector<std::string> values;
        values.push_back(xCompentId);
        threadXcompentList.insert(std::make_pair(threadId, values));
    }
    return;
}

bool OhosPluginManager::FindNativeXcomPoment(std::string &id, OH_NativeXComponent **nativeXComponent)
{
    if (nativeXComponentMap.find(id) != nativeXComponentMap.end()) {
        *nativeXComponent = nativeXComponentMap[id];
        return true;
    } else {
        return false;
    }
}

bool OhosPluginManager::FindNativeWindow(OH_NativeXComponent *nativeXComponent, SDL_WindowData **window)
{
    if (nativeXComponentList.find(nativeXComponent) != nativeXComponentList.end()) {
        *window = nativeXComponentList[nativeXComponent];
    } else {
        return false;
    }
    return true;
}

OhosThreadLock *OhosPluginManager::CreateOhosThreadLock(const pthread_t threadId)
{
    OhosThreadLock *threadLock = nullptr;
    if (ohosThreadLocks.find(threadId) != ohosThreadLocks.end()) {
         threadLock = ohosThreadLocks[threadId];
    } else  {
         threadLock = new OhosThreadLock();
         threadLock->mLock = SDL_CreateMutex();
         threadLock->mCond = SDL_CreateCond();
         ohosThreadLocks.insert(std::make_pair(threadId, threadLock));
    }
    return threadLock;
}

void OhosPluginManager::destroyOhosThreadLock(OhosThreadLock *threadLock)
{
     SDL_DestroyMutex(threadLock->mLock);
     SDL_DestroyCond(threadLock->mCond);
     delete threadLock;
}

void OhosPluginManager::SetNativeXComponentList(OH_NativeXComponent *component, SDL_WindowData *data)
{
    if (nullptr == data || nullptr == component) {
         return;
     }
     if (nativeXComponentList.find(component) == nativeXComponentList.end()) {
         nativeXComponentList[component] = data;
         return;
     }
    
     if (nativeXComponentList[component] != data) {
         SDL_WindowData *tmp = nativeXComponentList[component];
         if (tmp != nullptr) {
             delete tmp;
             tmp = nullptr;
         }
         nativeXComponentList[component] = data;
     }
}

SDL_WindowData* OhosPluginManager::GetWindowDataByXComponent(OH_NativeXComponent *component)
{
     if (nullptr == component) {
         return nullptr;
     }
     if (nativeXComponentList.find(component) == nativeXComponentList.end()) {
         return nullptr;
     }

     return nativeXComponentList[component];
}

pthread_t OhosPluginManager::GetThreadIdFromXComponentId(std::string &id) {
     for (auto &threadIdAndXComponentId : threadXcompentList) {
         for (auto &idStr : threadIdAndXComponentId.second) {
             if (id == idStr) {
                 return threadIdAndXComponentId.first;
             }
         }
     }
     return -1;
}

OhosThreadLock *OhosPluginManager::GetOhosThreadLockFromThreadId(pthread_t threadId)
{
    if (ohosThreadLocks.find(threadId) == ohosThreadLocks.end()) {
        return nullptr;
    }
    return ohosThreadLocks[threadId];
}

int OhosPluginManager::ClearPluginManagerData(std::string &id, OH_NativeXComponent *component, pthread_t threadId)
{
    if (nullptr == component) {
        return -1;
    }
    
    if (nativeXComponentMap.find(id) != nativeXComponentMap.end()) {
        nativeXComponentMap.erase(id);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "This xComponentId: %s not in nativeXComponentMap", id.c_str());
    }
    if (nativeXComponentList.find(component) != nativeXComponentList.end()) {
        delete nativeXComponentList[component];
        nativeXComponentList.erase(component);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "this xComponent not in nativeXComponentList");
    }
    
    if (threadXcompentList.find(threadId) != threadXcompentList.end()) {
     for (auto iter = threadXcompentList[threadId].begin(); iter != threadXcompentList[threadId].end(); iter++) {
         if (*iter == id) {
             threadXcompentList[threadId].erase(iter);
         }
     }
     if (threadXcompentList[threadId].empty()) {
         threadXcompentList.erase(threadId);
     }
    }
    
    if (ohosThreadLocks.find(threadId) != ohosThreadLocks.end()) {
     if (!((threadXcompentList.find(threadId) != threadXcompentList.end()) &&
           (threadXcompentList[threadId].empty()))) {
         auto lock = ohosThreadLocks[threadId];
         if (nullptr != lock) {
             SDL_DestroyMutex(lock->mLock);
             SDL_DestroyCond(lock->mCond);
             delete lock;
             lock = nullptr;
         }
         ohosThreadLocks.erase(threadId);
     }
    }
    return 0;
}
