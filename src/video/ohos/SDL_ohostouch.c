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

#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

#include "SDL_hints.h"
#include "SDL_events.h"
#include "SDL_log.h"
#include "SDL_ohostouch.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../core/ohos/SDL_ohos.h"

#define ACTION_DOWN 0
#define ACTION_UP 1
#define ACTION_MOVE 2
#define ACTION_CANCEL 3

#define FLOOR_HIGHT 10000

void OHOS_InitTouch(void)
{
}

void OHOS_QuitTouch(void)
{
}

void OHOS_OnTouch(SDL_Window *window, OhosTouchId *touchsize)
{
    SDL_TouchID touchDeviceId = 0;
    SDL_FingerID fingerId = 0;
    float tempX = 0.0;
    float tempY = 0.0;

    if (!window) {
        return;
    }

    touchDeviceId = (SDL_TouchID)touchsize->touchDeviceIdIn;
    if (SDL_AddTouch(touchDeviceId, SDL_TOUCH_DEVICE_DIRECT, "") < 0) {
        SDL_Log("error: can't add touch %s, %d", __FILE__, __LINE__);
    }

    fingerId = (SDL_FingerID)touchsize->pointerFingerIdIn;
    switch (touchsize->action) {
        case ACTION_DOWN:
//      case ACTION_POINTER_DOWN:
            if (window->w != 0 && window->h != 0) {
                tempX = floor(touchsize->x * FLOOR_HIGHT) / ((float)window->w * FLOOR_HIGHT);
                tempY = floor(touchsize->y * FLOOR_HIGHT) / ((float)window->h * FLOOR_HIGHT);
            } else {
                tempX = 0.0;
                tempY = 0.0;
            }
            SDL_SendTouch(touchDeviceId, fingerId, window, SDL_TRUE, tempX, tempY, touchsize->p);
            break;

        case ACTION_MOVE:
            SDL_SendTouchMotion(touchDeviceId, fingerId, window, touchsize->x, touchsize->y, touchsize->p);
            break;

        case ACTION_UP:
            SDL_SendTouch(touchDeviceId, fingerId, window, SDL_FALSE, touchsize->x, touchsize->y, touchsize->p);
            break;

        default:
            break;
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
