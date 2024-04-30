// Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
// In the following comments, node indicates the ViewNodeController.

import { ViewNodeController, NodeParams } from '../common/Node';
import { UIContext } from '@ohos.arkui.UIContext';
import { XComponentModel } from '../common/ComponentModel';

const nodeMap = new Map<number, ViewNodeController>();

/**
 * Get the unique root node in the window
 * @param windowId OHOS Window ID
 * @returns ViewNodeController
 */
export function getNodeByWindowId(windowId: number): ViewNodeController | undefined {
  return nodeMap.get(windowId);
}

/**
 * Instantiate rootNodeController and add it to the rootNodeControllerList
 * @param rootNodeController root node
 * @param uiContext UIContext used for initializing the root node
 * @param nodeParams NodeParams used for initializing the root node
 * @param rootNodeControllerList
 */
export function addRootNode(windowId: number, uiContext: UIContext, nodeParams: NodeParams, rootNodeControllerList?: Array<ViewNodeController>) {
  let rootNodeController = new ViewNodeController(uiContext, nodeParams);
  nodeMap.set(windowId, rootNodeController);
  rootNodeControllerList?.push(rootNodeController);
  return rootNodeController;
}

/**
 * Create a child node and mount it to the parent node.
 * @param nodeControllerParent
 * @param nodeParams NodeParams used for initializing the child node
 * @returns child node
 */
export function addChildNode(nodeControllerParent: ViewNodeController, nodeParams?: NodeParams): ViewNodeController {
  let nodeControllerChild: ViewNodeController;
  if (nodeParams == undefined) {
    nodeControllerChild = new ViewNodeController(nodeControllerParent.getUIContext());
  } else {
    if (nodeParams.node_xcomponent != undefined) {
      let nodeXComponent: XComponentModel = new XComponentModel(nodeParams.node_xcomponent.id, nodeParams.node_xcomponent.type, nodeParams.node_xcomponent.libraryname);
      if (nodeParams.node_xcomponent.onLoad != null && nodeParams.node_xcomponent.onLoad != undefined) {
        nodeXComponent.onLoad = nodeParams.node_xcomponent.onLoad;
      }
      if (nodeParams.node_xcomponent.onDestroy != null && nodeParams.node_xcomponent.onDestroy != undefined) {
        nodeXComponent.onDestroy = nodeParams.node_xcomponent.onDestroy;
      }
      let newNodeParams = new NodeParams(nodeParams.position_x, nodeParams.position_y, nodeParams.width, nodeParams.height, nodeParams.node_type, nodeXComponent);
      newNodeParams.border_color = nodeParams.border_color;
      newNodeParams.border_width = nodeParams.border_width;
      nodeControllerChild = new ViewNodeController(nodeControllerParent.getUIContext(), newNodeParams);
    } else {
      let newNodeParams = new NodeParams(nodeParams.position_x, nodeParams.position_y, nodeParams.width, nodeParams.height, nodeParams.node_type);
      newNodeParams.border_color = nodeParams.border_color;
      newNodeParams.border_width = nodeParams.border_width;
      nodeControllerChild = new ViewNodeController(nodeControllerParent.getUIContext(), newNodeParams);
    }
  }

  nodeControllerParent.getParams().node_list?.push(nodeControllerChild);
  nodeControllerChild.setParent(nodeControllerParent);
  nodeControllerParent.update();
  return nodeControllerChild;
}

/**
 * Modify NodeController Params
 * @param node
 * @param nodeParams
 * @returns
 */
export function setNodeParams(node: ViewNodeController, nodeParams: NodeParams): ViewNodeController {
  if (node) {
    node.setParams(nodeParams);
  }
  return node;
}

/**
 * Set node border width
 * @param node
 * @param borderWidth
 * @returns
 */
export function setNodeBorderWidth(node: ViewNodeController, borderWidth: Length): ViewNodeController {
  if (node) {
    node.getParams().border_width = borderWidth;
    let nodeControllerParent: ViewNodeController | null = node.getParent();
    if (nodeControllerParent) {
      nodeControllerParent.update()
    }
  }
  return node;
}

/**
 * Set node border color
 * @param node
 * @param borderColor
 * @returns
 */
export function setNodeBorderColor(node: ViewNodeController, borderColor: string): ViewNodeController {
  if (node) {
    node.getParams().border_color = borderColor;
    let nodeControllerParent: ViewNodeController | null = node.getParent();
    if (nodeControllerParent) {
      nodeControllerParent.update()
    }
  }
  return node;
}

/**
 * Remove the child node from the parent node.
 * @param nodeControllerChild child node to be removed
 */
export function removeChildNode(nodeControllerChild: ViewNodeController) {
  let removeList: ViewNodeController[] = []
  let nodeControllerParent: ViewNodeController | null = nodeControllerChild.getParent();
  if (nodeControllerParent) {
    let nodeList: ViewNodeController[] = nodeControllerParent.getParams().node_list;
    for (let i = 0; i < nodeList.length; i++) {
      if (nodeList[i].getCurNode() == nodeControllerChild.getCurNode()) {
        removeList = nodeControllerParent.getParams().node_list.splice(i, 1);
      }
    }
    nodeControllerParent.update();
  }

  // delete node
  for (let i = 0; i < removeList.length; i++) {
    removeList[i].delete();
  }
}

/**
 * Change the Size of a Node
 * @param nodeController
 * @param width
 * @param height
 */
export function resizeNode(nodeController: ViewNodeController, width: string, height: string): void {
  nodeController.getParams().width = width;
  nodeController.getParams().height = height;
  let nodeControllerParent: ViewNodeController | null = nodeController.getParent();
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

/**
 * Change the z-index of a Node
 * @param nodeController
 * @param zIndex
 */
export function adjustNodeZOrder(nodeController: ViewNodeController, zIndex: number) {
  nodeController.getParams().z_index = zIndex;
  let nodeControllerParent: ViewNodeController | null = nodeController.getParent();
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

/**
 * Move node to the top
 * @param nodeController
 */
export function raiseNode(nodeController: ViewNodeController) {
  let nodeControllerParent: ViewNodeController = nodeController.getParent() as ViewNodeController;
  let max_z_index = 0;
  let node_list = nodeControllerParent.getParams().node_list;
  for (let i = 0; i < node_list.length; i++) {
    max_z_index = node_list[i].getParams().z_index > max_z_index ? node_list[i].getParams().z_index : max_z_index;
  }

  nodeController.getParams().z_index = max_z_index + 1;
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

/**
 * Move node to the bottom
 * @param nodeController
 */
export function lowerNode(nodeController: ViewNodeController) {
  let nodeControllerParent: ViewNodeController | null = nodeController.getParent() as ViewNodeController;
  let min_z_index = Infinity;
  let node_list = nodeControllerParent.getParams().node_list;
  for (let i = 0; i < node_list.length; i++) {
    min_z_index = node_list[i].getParams().z_index < min_z_index ? node_list[i].getParams().z_index : min_z_index;
  }

  if (min_z_index > 0)
    nodeController.getParams().z_index = min_z_index - 1;
  else
    nodeController.getParams().z_index = 0;
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

/**
 * Remount the node to the new parent node.
 */
export function reParentNode(nodeControllerParentNew: ViewNodeController, nodeControllerChild: ViewNodeController) {
  let nodeControllerParent: ViewNodeController | null = nodeControllerChild.getParent();
  if (nodeControllerParent) {
    let nodeList: ViewNodeController[] = nodeControllerParent.getParams().node_list;
    for (let i = 0; i < nodeList.length; i++) {
      if (nodeList[i].getCurNode() == nodeControllerChild.getCurNode()) {
        nodeControllerParent.getParams().node_list.splice(i, 1);
        break;
      }
    }
  }
  nodeControllerParentNew.getParams().node_list?.push(nodeControllerChild);
  nodeControllerChild.setParent(nodeControllerParentNew);
  nodeControllerParentNew.update();
  nodeControllerParent?.update();
  return nodeControllerChild;
}

/**
 * Modify the Visibility of a node
 * @param nodeController
 * @param visibility Show = 0 Hidden = 1 None = 2
 */
export function setNodeVisibility(nodeController: ViewNodeController, visibility: number) {
  nodeController.getParams().visibility = visibility as Visibility; // Show = 0 Hidden = 1 None = 2
  let nodeControllerParent: ViewNodeController | null = nodeController.getParent();
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

/**
 * Get the Rect of a node
 * @returns number[] [x, y, w, h]
 */
export function getNodeRect(nodeController: ViewNodeController): number[] {
  let x: number = Number(nodeController.getParams().position_x);
  let y: number = Number(nodeController.getParams().position_y);
  let w: number = nodeController.widthReal;
  let h: number = nodeController.heightReal;

  return [x, y, w, h];
}

/**
 * Move the position of a node in the parent node.
 * @param nodeController
 * @param position_x
 * @param position_y
 */
export function moveNode(nodeController: ViewNodeController, position_x: Length, position_y: Length) {
  // If the node is ROOT node, we need to move the window
  let entries = nodeMap.entries();
  for (let i = 0; i < nodeMap.size; i++) {
    let entry: IteratorResult<[number, ViewNodeController]> = entries.next();
    if (entry.value[1] == nodeController) {
      let windowId: number = entry.value[0];
      // moveWindowTo(x: number, y: number, callback: AsyncCallback<void>);

      return;
    }
  }
  nodeController.getParams().position_x = position_x;
  nodeController.getParams().position_y = position_y;
  let nodeControllerParent: ViewNodeController | null = nodeController.getParent();
  if (nodeControllerParent) {
    nodeControllerParent.update()
  }
}

export function getXcomponentId(nodeController: ViewNodeController):string {
  return nodeController.getParams().node_xcomponent.id;
}

/**
 * Obtains the Component object in a node.
 */
export function getObject() {

}
