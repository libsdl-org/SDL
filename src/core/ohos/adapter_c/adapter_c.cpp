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

#include "adapter_c.h"
#include <hilog/log.h>
#include "napi/native_api.h"
#include "cJSON.h"
#include "../SDL_ohos_tstype.h"
#include "adapter_c_ts.h"

void ThreadSafeSyn(cJSON * const root) {
    ThreadLockInfo *lockInfo = new ThreadLockInfo();
    long long lockInfoPointer = (long long)lockInfo;
    cJSON_AddNumberToObject(root, OHOS_JSON_ASYN, (double)lockInfoPointer);
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        delete lockInfo;
        return;
    }
    std::unique_lock<std::mutex> lock(lockInfo->mutex);
    lockInfo->condition.wait(lock, [lockInfo] { return lockInfo->ready; });
    delete lockInfo;
    return;
}

static bool ThreadSafeAsyn(cJSON * const root) {
    napi_status status = napi_call_threadsafe_function(g_napiCallback->tsfn, root, napi_tsfn_nonblocking);
    if (status != napi_ok) {
        return false;
    }
    return true;
}

napi_ref GetRootNode(int windowId) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return nullptr;
    }
    cJSON_AddNumberToObject(root, "windowId", windowId);
    napi_ref returnValue = nullptr;
    long long returnValuePointer = (long long)(&returnValue);
    cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);

    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        OHOS_TS_GetRootNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_GET_ROOTNODE);
        ThreadSafeSyn(root);
    }
    return returnValue;
}

char* GetXComponentId(napi_ref nodeRef)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return nullptr;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    char *returnValue = nullptr;
    long long returnValuePointer = (long long)(&returnValue);
    cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);

    std::thread::id cur_thread_id = std::this_thread::get_id();
    ThreadLockInfo *lockInfo;
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        OHOS_TS_GetXComponentId(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_GET_XCOMPONENTID);
        ThreadSafeSyn(root);
    }
    return returnValue;
}

napi_ref AddSdlChildNode(napi_ref nodeRef, NodeParams *nodeParams)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return nullptr;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    cJSON_AddNumberToObject(root, "nodeParams", (long long)nodeParams);
    napi_ref returnValue = nullptr;
    long long returnValuePointer = (long long)(&returnValue);
    cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);

    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        OHOS_TS_AddChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_ADDCHILDNODE);
        ThreadSafeSyn(root);
    }
    return returnValue;
}

bool RemoveSdlChildNode(napi_ref nodeChildRef) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeChildRef);
    
    bool  returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_REMOVENODE);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

bool RaiseNode(napi_ref nodeRef) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RaiseNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_RAISENODE);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

bool LowerNode(napi_ref nodeRef) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_RAISENODE);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

bool ResizeNode(napi_ref nodeRef, std::string width, std::string height) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    cJSON_AddStringToObject(root, OHOS_JSON_WIDTH, width.c_str());
    cJSON_AddStringToObject(root, OHOS_JSON_HEIGHT, height.c_str());

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_RESIZENODE);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

bool ReParentNode(napi_ref nodeParentNewRef, napi_ref nodeChildRef) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeParentNewRef);
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF2, (long long)nodeChildRef);

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_REPARENT);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

bool SetNodeVisibility(napi_ref nodeRef, int visibility) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    cJSON_AddNumberToObject(root, OHOS_JSON_VISIBILITY, visibility);

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_VISIBILITY);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}

NodeRect *GetNodeRect(napi_ref nodeRef) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return nullptr;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    NodeRect *returnValue = new NodeRect;
    long long returnValuePointer = (long long)(returnValue);
    cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);

    std::thread::id cur_thread_id = std::this_thread::get_id();
    ThreadLockInfo *lockInfo;
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        OHOS_TS_GetRootNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_GETNODERECT);
        ThreadSafeSyn(root);
    }
    return returnValue;
}

bool MoveNode(napi_ref nodeRef, std::string x, std::string y) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }
    cJSON_AddNumberToObject(root, OHOS_JSON_NODEREF, (long long)nodeRef);
    cJSON_AddStringToObject(root, OHOS_JSON_X, x.c_str());
    cJSON_AddStringToObject(root, OHOS_JSON_Y, y.c_str());

    bool returnValue = false;
    std::thread::id cur_thread_id = std::this_thread::get_id();
    if (cur_thread_id == g_napiCallback->mainThreadId) {
        long long returnValuePointer = (long long)(&returnValue);
        cJSON_AddNumberToObject(root, OHOS_JSON_RETURN_VALUE, returnValuePointer);
        OHOS_TS_RemoveChildNode(root);
        cJSON_free(root);
    } else {
        cJSON_AddNumberToObject(root, OHOS_TS_CALLBACK_TYPE, NAPI_CALLBACK_MOVENODE);
        returnValue = ThreadSafeAsyn(root);
    }
    return returnValue;
}