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

#include "../../SDL_internal.h"

#ifndef SDL_ohostouch_h_
#define SDL_ohostouch_h_

#include "SDL_ohosvideo.h"

extern void OHOS_InitTouch(void);
extern void OHOS_QuitTouch(void);
extern void OHOS_OnTouch(SDL_Window *window, int touch_device_id_in, int pointer_finger_id_in, int action, float x, float y, float p);

#endif /* SDL_ohostouch_h_ */

/* vi: set ts=4 sw=4 expandtab: */
