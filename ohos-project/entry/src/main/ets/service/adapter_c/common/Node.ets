// Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
import { FrameNode, BuilderNode, NodeController, Size } from '@ohos.arkui.node';
import { WindowBuilderForView } from '../components/NodeComponent';
import { XComponentModel } from './ComponentModel';
import { UIContext } from '@ohos.arkui.UIContext';

export enum NodeType {
  XComponent, // xcomponent
  UIExtension, // uiextension not support now
  Container // notused
}

@Observed
export class NodeParams {
  public node_list: Array<ViewNodeController> = []; // Child Node Array
  public width: Length = '50%';
  public height: Length = '50%';
  public position_x: Length = 0; // X-coordinate of the offset relative to the upper left corner of the parent component
  public position_y: Length = 0; // Y-coordinate of the offset relative to the upper left corner of the parent component
  public border_color: string = "#FFFFFF"; // The default value needs to be changed.
  public border_width: Length = 0; // The default value needs to be changed.
  public node_type: NodeType = NodeType.XComponent; // Component type used in a node.
  public z_index: number = 1;
  public visibility: Visibility = Visibility.Visible;
  public node_xcomponent: XComponentModel = new XComponentModel("", XComponentType.SURFACE, "");
  // Other attributes are not supported now.

  constructor(x?: Length, y?: Length, w?: Length, h?: Length, node_type?: NodeType, node_xcomponent?: XComponentModel) {
    if (node_type != undefined) this.node_type = node_type;
    if (node_xcomponent != undefined) this.node_xcomponent = node_xcomponent;
    if (w != undefined) this.width = w;
    if (h != undefined) this.height = h;
    if (x != undefined) this.position_x = x;
    if (y != undefined) this.position_y = y;
  }
}

export class ViewNodeController extends NodeController {
  private builderNode: BuilderNode<NodeParams[]> | null = null;
  private wrapBuilder = new WrappedBuilder<[NodeParams]>(WindowBuilderForView);
  private uiContextP: UIContext;
  private params: NodeParams = new NodeParams();
  private parentNodeController: ViewNodeController | null = null;
  public widthReal: number = 0;
  public heightReal: number = 0;

  public getCurNode(): BuilderNode<NodeParams[]> | null {
    return this.builderNode;
  }

  public getParent() {
    return this.parentNodeController;
  }

  public setParent(parentNodeController: ViewNodeController) {
    this.parentNodeController = parentNodeController;
  }

  public getUIContext(): UIContext {
    return this.uiContextP;
  }

  public getParams(): NodeParams {
    return this.params;
  }

  public setParams(params: NodeParams) {
    this.params = params;
    this.update();
  }

  constructor(uiContextP: UIContext, params?: NodeParams) {
    super(); // Here, the makeNode function is executed once.
    this.uiContextP = uiContextP;
    if (params != undefined) {
      this.params = params as NodeParams;
    }

    // Use the UIContext parameter of the constructor to instantiate the builderNode
    // (that is, do not instantiate the builderNode in the makeNode).
    if (this.builderNode == null) {
      this.builderNode = new BuilderNode(this.uiContextP);
      this.builderNode.build(this.wrapBuilder, this.params as NodeParams);
    }
  }

  makeNode(uiContext: UIContext): FrameNode | null {
    // Check whether the builderNode is initialized and then determine whether to run the build command.
    if (this.builderNode != null) {
      return this.builderNode.getFrameNode();
    }
    return null;
  }

  aboutToResize(size: Size) {
    console.log("aboutToResize width : " + size.width + " height : " + size.height);
    this.widthReal = size.width;
    this.heightReal = size.height;
  }

  aboutToAppear() {
    console.log("aboutToAppear");
  }

  aboutToDisappear() {
    console.log("aboutToDisappear");
  }

  delete() {
    for (let i = 0; i < this.params.node_list.length; i++) {
      this.params.node_list[i].delete();
    }
    this.builderNode?.build(this.wrapBuilder, new NodeParams)
    this.builderNode = null;
  }

  update() {
    if (this.builderNode != null) {
      this.builderNode.update(this.params);
    }
  }
}