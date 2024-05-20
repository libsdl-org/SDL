/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { ViewNodeController, NodeParams, NodeType } from '../common/Node';

@Builder
export function WindowBuilderForView(params: NodeParams) {
  Stack({ alignContent: Alignment.TopStart }) {
    if (params?.node_type == NodeType.XComponent) {
      // XComponent
      XComponent({
        id: params?.node_xcomponent.id,
        type: params?.node_xcomponent.type,
        libraryname: params?.node_xcomponent.libraryname,
        controller: params?.node_xcomponent.controller
      })
        .width('100%')
        .height('100%')
        .focusable(true)
        .onLoad(params?.node_xcomponent.onLoad)
        .onDestroy(params?.node_xcomponent.onDestroy)
        .focusOnTouch(true)
    } else if (params?.node_type == NodeType.UIExtension) {
      // @todo UIExtension is not support now
    }

    ForEach(params.node_list, (item: ViewNodeController) => {
      NodeContainer(item)
        .zIndex(item.getParams().z_index)
        .position({
          x: (Number.isNaN(Number(item.getParams().position_x.toString())) ? item.getParams().position_x : item.getParams().position_x + "px"),
          y: (Number.isNaN(Number(item.getParams().position_y.toString())) ? item.getParams().position_y : item.getParams().position_y + "px")
        })
        .width(Number.isNaN(Number(item.getParams().width.toString())) ? item.getParams().width : item.getParams().width + "px")
        .height(Number.isNaN(Number(item.getParams().height.toString())) ? item.getParams().height : item.getParams().height + "px")
        .borderColor(item.getParams().border_color)
        .borderWidth(item.getParams().border_width)
        .visibility(item.getParams().visibility)
    }, (_item: ViewNodeController, index: number) => index.toString())
  }
}