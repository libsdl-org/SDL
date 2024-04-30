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

#ifndef ADAPTER_C_TS_H
#define ADAPTER_C_TS_H
#include "adapter_c.h"
#include "cJSON.h"

void OHOS_TS_GetRootNode(const cJSON *root);

void OHOS_TS_GetXComponentId(const cJSON *root);

void OHOS_TS_AddChildNode(const cJSON *root);

void OHOS_TS_RemoveChildNode(const cJSON *root);

void OHOS_TS_RaiseNode(const cJSON *root);

void OHOS_TS_LowerNode(const cJSON *root);

void OHOS_TS_ResizeNode(const cJSON *root);

void OHOS_TS_ReParentNode(const cJSON *root);

void OHOS_TS_SetNodeVisibility(const cJSON *root);

void OHOS_TS_GetNodeRect(const cJSON *root);

void OHOS_TS_MoveNode(const cJSON *root);

void OHOS_TS_wakeup(const cJSON *root, ThreadLockInfo *lockInfo);

void OHOS_TS_GetLockInfo(const cJSON *root, ThreadLockInfo **lockInfo);

#endif // ADAPTER_C_TS_H
