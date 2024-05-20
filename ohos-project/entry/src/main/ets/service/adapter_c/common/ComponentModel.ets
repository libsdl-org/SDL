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

import { Callback } from '@ohos.base';

export class XComponentModel {
  id: string;
  type: XComponentType = XComponentType.SURFACE;
  libraryname: string;
  focusable : boolean = true;
  onLoad : Callback<object> = (event? : object) => {
    // @todo something
  };
  onDestroy = () => {
    // @todo something
  };
  controller: XComponentController | null = null;

  constructor(id: string, type: XComponentType, libraryname: string) {
    this.id = id;
    this.type = type;
    this.libraryname = libraryname;
  }
}