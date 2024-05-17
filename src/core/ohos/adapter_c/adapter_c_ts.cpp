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
#include "SDL_log.h"
#include "adapter_c.h"
#include "../SDL_ohos_tstype.h"
#include "adapter_c_ts.h"

#define OHOS_NAPI_ARG_TWO   2
#define OHOS_NAPI_ARG_THREE   3
using namespace  std;


void OHOS_TS_GetLockInfo(const cJSON *root, ThreadLockInfo **lockInfo)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_ASYN);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        *lockInfo = reinterpret_cast<ThreadLockInfo *>(temp);
    }
}

void OHOS_TS_wakeup(const cJSON *root, ThreadLockInfo *lockInfo)
{
    if (lockInfo == nullptr) {
        return;
    }
    lockInfo->ready = true;
    lockInfo->condition.notify_all();
    return;
}

static napi_value OHOS_TS_GetNode(const cJSON *root)
{
    long long temp;
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_NODEREF);
    temp = static_cast<long long>(data->valuedouble);
    napi_ref viewRef = reinterpret_cast<napi_ref>(temp);

    napi_value node;
    napi_get_reference_value(g_napiCallback->env, viewRef, &node);
    return node;
}

static napi_value OHOS_TS_GetJsMethod(string name)
{
    napi_value callback = nullptr;
    napi_get_reference_value(g_napiCallback->env, g_napiCallback->callbackRef, &callback);
    napi_value jsMethod;
    napi_get_named_property(g_napiCallback->env, callback, name.c_str(), &jsMethod);
    return jsMethod;
}

void OHOS_TS_GetRootNode(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "windowId");
    int x = data->valueint;

    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    long long temp = static_cast<long long>(data->valuedouble);
    napi_ref *returnValue = reinterpret_cast<napi_ref *>(temp);

    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};

    napi_create_int32(g_napiCallback->env, x, &argv[OHOS_THREADSAFE_ARG0]);
    
    ThreadLockInfo *lockInfo = nullptr;
    OHOS_TS_GetLockInfo(root, &lockInfo);

    napi_value jsMethod = OHOS_TS_GetJsMethod("getNodeByWindowId");
    napi_value tempReturn = nullptr;
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, &tempReturn);

    if (tempReturn != nullptr) {
        napi_create_reference(g_napiCallback->env, tempReturn, 1, returnValue);
    }

    OHOS_TS_wakeup(root, lockInfo);
}

void OHOS_TS_GetXComponentId(const cJSON *root)
{
    long long temp;
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    temp = static_cast<long long>(data->valuedouble);
    char **returnValue = reinterpret_cast<char **>(temp);

    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);

    napi_value jsMethod = OHOS_TS_GetJsMethod("getXcomponentId");

    ThreadLockInfo *lockInfo = nullptr;
    OHOS_TS_GetLockInfo(root, &lockInfo);

    napi_value tempReturn = nullptr;
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, &tempReturn);

    if (tempReturn != nullptr) {
        size_t len = 0;
        napi_get_value_string_utf8(g_napiCallback->env, tempReturn, *returnValue, 0, &len);
        *returnValue = (char *)SDL_malloc(len + 1);
        napi_get_value_string_utf8(g_napiCallback->env, tempReturn, *returnValue, len + 1, &len);
    }
    OHOS_TS_wakeup(root, lockInfo);
}

static void configNode(const NodeParams &nodeParams, napi_value *nodeParamsNapi) {
    napi_value width;
    napi_value height;
    napi_value x;
    napi_value y;
    napi_value borderColor;
    napi_value borderWidth;
    napi_value nodeType;

    napi_create_string_utf8(g_napiCallback->env, nodeParams.width.c_str(), NAPI_AUTO_LENGTH, &width);
    napi_create_string_utf8(g_napiCallback->env, nodeParams.height.c_str(), NAPI_AUTO_LENGTH, &height);
    napi_create_string_utf8(g_napiCallback->env, nodeParams.x.c_str(), NAPI_AUTO_LENGTH, &x);
    napi_create_string_utf8(g_napiCallback->env, nodeParams.y.c_str(), NAPI_AUTO_LENGTH, &y);
    napi_create_string_utf8(g_napiCallback->env, nodeParams.border_color.c_str(), NAPI_AUTO_LENGTH, &borderColor);
    napi_create_string_utf8(g_napiCallback->env, nodeParams.border_width.c_str(), NAPI_AUTO_LENGTH, &borderWidth);
    napi_create_int32(g_napiCallback->env, nodeParams.nodeType, &nodeType);

    napi_create_object(g_napiCallback->env, nodeParamsNapi);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "width", width);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "height", height);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "position_x", x);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "position_y", y);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "node_type", nodeType);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "border_color", borderColor);
    napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "border_width", borderWidth);

    if (nodeParams.componentModel) {
        napi_value id;
        napi_value type;
        napi_value libraryName;
        napi_create_string_utf8(g_napiCallback->env, nodeParams.componentModel->id.c_str(), NAPI_AUTO_LENGTH, &id);
        napi_create_int32(g_napiCallback->env, nodeParams.componentModel->type, &type);
        napi_create_string_utf8(g_napiCallback->env, nodeParams.componentModel->libraryName.c_str(), NAPI_AUTO_LENGTH,
                                &libraryName);

        napi_value model;
        napi_create_object(g_napiCallback->env, &model);
        napi_set_named_property(g_napiCallback->env, model, "id", id);
        napi_set_named_property(g_napiCallback->env, model, "type", type);
        napi_set_named_property(g_napiCallback->env, model, "libraryname", libraryName);
        if (nodeParams.componentModel->onLoad) {
            napi_value onload = (napi_value)nodeParams.componentModel->onLoad;
            napi_set_named_property(g_napiCallback->env, model, "onLoad", onload);
        }
        if (nodeParams.componentModel->onDestroy) {
            napi_value onDestroy = (napi_value)nodeParams.componentModel->onDestroy;
            napi_set_named_property(g_napiCallback->env, model, "onDestroy", onDestroy);
        }

        napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "node_xcomponent", model);
    } else {
        napi_set_named_property(g_napiCallback->env, *nodeParamsNapi, "node_xcomponent", nullptr);
    }
}

void OHOS_TS_AddChildNode(const cJSON *root)
{
    long long temp;

    cJSON *data = cJSON_GetObjectItem(root, "nodeParams");
    temp = static_cast<long long>(data->valuedouble);
    NodeParams *nodeParams = reinterpret_cast<NodeParams *>(temp);

    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    temp = static_cast<long long>(data->valuedouble);
    napi_ref *returnValue = reinterpret_cast<napi_ref *>(temp);

    napi_value argv[OHOS_THREADSAFE_ARG2] = {nullptr};
    napi_value nodeParamsNapi;
    configNode(*nodeParams, &nodeParamsNapi);
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    argv[OHOS_THREADSAFE_ARG1] = nodeParamsNapi;

    napi_value jsMethod = OHOS_TS_GetJsMethod("addChildNode");

    ThreadLockInfo *lockInfo = nullptr;
    OHOS_TS_GetLockInfo(root, &lockInfo);

    napi_value tempReturn = nullptr;
    napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG2, argv, &tempReturn);

    if (tempReturn != nullptr) {
        napi_create_reference(g_napiCallback->env, tempReturn, 1, returnValue);
    }

    OHOS_TS_wakeup(root, lockInfo);
}

void OHOS_TS_RemoveChildNode(const cJSON *root)
{
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_value jsMethod = OHOS_TS_GetJsMethod("removeChildNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_RaiseNode(const cJSON *root)
{
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_value jsMethod = OHOS_TS_GetJsMethod("raiseNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_LowerNode(const cJSON *root)
{
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_value jsMethod = OHOS_TS_GetJsMethod("lowerNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_ResizeNode(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_WIDTH);
    const char *width = data->valuestring;
    data = cJSON_GetObjectItem(root, OHOS_JSON_HEIGHT);
    const char *height = data->valuestring;
    
    napi_value argv[OHOS_THREADSAFE_ARG3] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_create_string_utf8(g_napiCallback->env, width, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG1]);
    napi_create_string_utf8(g_napiCallback->env, height, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG2]);

    napi_value jsMethod = OHOS_TS_GetJsMethod("resizeNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG3, argv, nullptr);
    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_ReParentNode(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_NODEREF2);
    long long temp = 0;
    temp = static_cast<long long>(data->valuedouble);
    napi_ref viewRef = reinterpret_cast<napi_ref>(temp);

    napi_value node;
    napi_get_reference_value(g_napiCallback->env, viewRef, &node);

    napi_value argv[OHOS_THREADSAFE_ARG2] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    argv[OHOS_THREADSAFE_ARG0] = node;

    napi_value jsMethod = OHOS_TS_GetJsMethod("reParentNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG2, argv, nullptr);
    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_SetNodeVisibility(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_VISIBILITY);
    int visibility = data->valueint;
    
    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_create_int32(g_napiCallback->env, visibility, &argv[OHOS_THREADSAFE_ARG1]);
    napi_value jsMethod = OHOS_TS_GetJsMethod("setNodeVisibility");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, nullptr);
    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}

void OHOS_TS_GetNodeRect(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    long long temp = static_cast<long long>(data->valuedouble);
    NodeRect *returnValue = reinterpret_cast<NodeRect *>(temp);

    napi_value argv[OHOS_THREADSAFE_ARG1] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);

    napi_value jsMethod = OHOS_TS_GetJsMethod("getNodeRect");

    ThreadLockInfo *lockInfo = nullptr;
    OHOS_TS_GetLockInfo(root, &lockInfo);

    napi_value result = nullptr;
    napi_status status = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG1, argv, &result);
    if (status == napi_ok) {
        bool isArray = false;
        napi_is_array(g_napiCallback->env, result, &isArray);
        bool hasEle = false;
        napi_has_element(g_napiCallback->env, result, 0, &hasEle);
        if (hasEle) {
            hasEle = false;
            napi_value value;
            napi_get_element(g_napiCallback->env, result, 0, &value);
            napi_get_value_int64(g_napiCallback->env, value, &(returnValue->offsetX));
        }
        napi_has_element(g_napiCallback->env, result, 1, &hasEle);
        if (hasEle) {
            hasEle = false;
            napi_value value;
            napi_get_element(g_napiCallback->env, result, 1, &value);
            napi_get_value_int64(g_napiCallback->env, value, &(returnValue->offsetY));
        }
        napi_has_element(g_napiCallback->env, result, OHOS_NAPI_ARG_TWO, &hasEle);
        if (hasEle) {
            hasEle = false;
            napi_value value;
            napi_get_element(g_napiCallback->env, result, OHOS_NAPI_ARG_TWO, &value);
            napi_get_value_int64(g_napiCallback->env, value, &(returnValue->width));
        }
        napi_has_element(g_napiCallback->env, result, OHOS_NAPI_ARG_THREE, &hasEle);
        if (hasEle) {
            hasEle = false;
            napi_value value;
            napi_get_element(g_napiCallback->env, result, OHOS_NAPI_ARG_THREE, &value);
            napi_get_value_int64(g_napiCallback->env, value, &(returnValue->height));
        }
    }
    OHOS_TS_wakeup(root, lockInfo);
}

void OHOS_TS_MoveNode(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, OHOS_JSON_X);
    const char *width = data->valuestring;
    data = cJSON_GetObjectItem(root, OHOS_JSON_Y);
    const char *height = data->valuestring;

    napi_value argv[OHOS_THREADSAFE_ARG3] = {nullptr};
    argv[OHOS_THREADSAFE_ARG0] = OHOS_TS_GetNode(root);
    napi_create_string_utf8(g_napiCallback->env, width, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG1]);
    napi_create_string_utf8(g_napiCallback->env, height, NAPI_AUTO_LENGTH, &argv[OHOS_THREADSAFE_ARG2]);

    napi_value jsMethod = OHOS_TS_GetJsMethod("moveNode");
    napi_status ret = napi_call_function(g_napiCallback->env, nullptr, jsMethod, OHOS_THREADSAFE_ARG3, argv, nullptr);
    data = cJSON_GetObjectItem(root, OHOS_JSON_RETURN_VALUE);
    if (data != nullptr) {
        long long temp = static_cast<long long>(data->valuedouble);
        bool *returnValue = reinterpret_cast<bool *>(temp);
        *returnValue = ((ret == napi_ok) ? true : false);
    }
}
