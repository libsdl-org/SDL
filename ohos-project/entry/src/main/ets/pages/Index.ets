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

import CommonConstants from '../common/CommonConstants'
import common from '@ohos.app.ability.common'
import {
  notifySdlPageShow,
  notifySdlAboutToAppear,
  notifySdlAboutToDisappear,
  notifySdlPageHide,
} from '../service/SdlModule'


@Entry
@Component
struct Index {

  aboutToAppear(): void {
    notifySdlAboutToAppear(getContext(this) as common.UIAbilityContext, getContext(this))
  }

  aboutToDisappear(): void {
    notifySdlAboutToDisappear()
  }

  onPageShow(): void {
    notifySdlPageShow()
  }

  onPageHide(): void {
    notifySdlPageHide()
  }

  build() {
    Stack() {
    Column() {
      XComponent({
        id: CommonConstants.XCOMPONENT_ID,
        type: CommonConstants.XCOMPONENT_TYPE,
        libraryname: CommonConstants.XCOMPONENT_LIBRARY_NAME
      })
      .focusable(true)
      .defaultFocus(true)
    }
    .width(CommonConstants.FULL_PARENT)
    .height(CommonConstants.FULL_PARENT)
    }
  }
}
