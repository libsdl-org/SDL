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

#ifndef SDL_ohosmouse_h_
#define SDL_ohosmouse_h_

#include "SDL_ohosvideo.h"

typedef struct MouseSize
{
    int state;
    int action;
    float x;
    float y;
}OHOS_Window_Size;

extern void OHOS_InitMouse(void);
extern void OHOS_OnMouse(SDL_Window *window, OHOS_Window_Size *windowsize, SDL_bool relative);
extern void OHOS_QuitMouse(void);

#endif /* SDL_ohosmouse_h_ */

/* vi: set ts=4 sw=4 expandtab: */
