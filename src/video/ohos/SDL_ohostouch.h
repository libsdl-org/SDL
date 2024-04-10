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


#ifndef SDL_OHOSTOUCH_H_
#define SDL_OHOSTOUCH_H_

#include "SDL_ohosvideo.h"

typedef struct TouchID{
    int touchDeviceIdIn;
    int pointerFingerIdIn;
    int action;
    float x;
    float y;
    float p;
}OhosTouchId;

extern void OHOS_InitTouch(void);
extern void OHOS_QuitTouch(void);
extern void OHOS_OnTouch(SDL_Window *window, OhosTouchId *touchsize);

#endif /* SDL_ohostouch_h_ */

/* vi: set ts=4 sw=4 expandtab: */
