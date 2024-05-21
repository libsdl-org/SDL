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

#ifndef ADAPTER_C_H
#define ADAPTER_C_H

#include <string>
#include <thread>
#include <future>
#include "napi/native_api.h"
#include "cJSON.h"

/**
 * NodeType: Node Component Type
 */
enum NodeType {
    XCOMPONENT,  // xcomponent
    UIEXTENSION, // uiextension not support now
    CONTAINER   // notused
};

/**
 * NodeRect: Rect of Node
 */
struct NodeRect {
    int64_t offsetX;
    int64_t offsetY;
    int64_t width;
    int64_t height;
};


/**
 * XComponentType: The type of XComponent
 */
enum XComponentType {
    XCOMPONENTTYPE,
    COMPONENT,
    TEXTURE,
};

/**
 * XComponent component attribute configuration
 */
class XComponentModel {
public:
    XComponentModel(std::string id, XComponentType type, std::string libraryName)
    {
        this->id = id;
        this->type = type;
        this->libraryName = libraryName;
    }

    std::string id = "";
    XComponentType type = XComponentType::XCOMPONENTTYPE;
    std::string libraryName = "";
    void *onLoad = nullptr;    // Not supported
    void *onDestroy = nullptr; // Not supported
    bool focusable = true;
};

/**
 * Node position attribute configuration
 */
class NodePosition {
public:
    NodePosition(std::string width, std::string height, std::string x, std::string y)
    {
        this->width = width;
        this->height = height;
        this->x = x;
        this->y = y;
    }

    std::string width = "50%";
    std::string height = "50%";
    std::string x = "0";
    std::string y = "0";
};

/**
 * Node attribute configuration
 */
class NodeParams {
public:
    NodeParams(NodeType nodeType, XComponentModel *componentModel, NodePosition *nodePosition)
    {
        this->nodeType = nodeType;
        this->componentModel = componentModel;
        this->nodePosition = nodePosition;
    }

    ~NodeParams()
    {
        if (componentModel != nullptr) {
            componentModel = nullptr;
        }
    }
    
    std::string border_color = "#000000";
    std::string border_width = "0";
    NodePosition *nodePosition = nullptr;
    XComponentModel *componentModel = nullptr;
    NodeType nodeType = NodeType::XCOMPONENT;
};

// Defines locks and semaphores.
using ThreadLockInfo = struct {
    std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
};

/**
 * Obtains the root node of a window.
 * @param windowId: Specifies the ID of the window.
 * @return
 */
napi_ref GetRootNode(int windowId);

/**
 * Add a child node to the parent node and returns the child node.
 * @param node: Parent node.
 * @return Child node.
 */
napi_ref AddSdlChildNode(napi_ref nodeRef, NodeParams *nodeParams);

/**
 * Remove a child node from a parent node.
 * @param nodeChild: Child node to be removed.
 * @return An array containing the elements that were deleted.
 */
bool RemoveSdlChildNode(napi_ref nodeChildRef);

/**
 * Place the node at the top layer.
 * @param node: Indicates the node to be operated
 * @return null
 */
bool SdlRaiseNode(napi_ref nodeRef);

/**
 * Place the node at the bottom layer.
 * @param node: Indicates the node to be operated
 * @return null
 */
bool LowerNode(napi_ref nodeRef);

/**
 * Adjusting the Width and Height of a Node.
 * @param node: Indicates the node to be operated
 * @param width
 * @param height
 * @return null
 */
bool ResizeNode(napi_ref nodeRef, std::string width, std::string height);

/**
 * Move a child node to another parent node. Cross-window movement is not supported.
 * @param nodeParentNew: new Parent
 * @param nodeChild: Node needs to be Moved
 * @return null
 */
bool ReParentNode(napi_ref nodeParentNewRef, napi_ref nodeChildRef);

/**
 * Modifying the Position of a Node in the Parent Window.
 * @param node
 * @param x
 * @param y
 * @return null
 */
bool MoveNode(napi_ref nodeRef, std::string x, std::string y);

/**
 * Set the node to invisible.
 * @param node
 * @return null
 */
bool SetNodeVisibility(napi_ref nodeRef, int visibility);

/**
 * Obtaining the Rect of a Node
 * @param node
 * @return {x, y, w, h}
 */
NodeRect *GetNodeRect(napi_ref nodeRef);

/**
 * Returns the XComponent object in the specified node.
 * This object needs to support OH_NativeXComponent_RegisterCallback invoking on the Native side.
 * @param node
 * @return OH_NativeXComponent
 * @todo
 */
char* GetXComponentId(napi_ref nodeRef);

void ThreadSafeSyn(cJSON * const root);

#endif // ADAPTER_C_H