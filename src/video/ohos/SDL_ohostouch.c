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

#if SDL_VIDEO_DRIVER_OHOS

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

void OHOS_InitTouch(void)
{
}

void OHOS_QuitTouch(void)
{
}

void OHOS_OnTouch(SDL_Window *window, int touch_device_id_in, int pointer_finger_id_in, int action, float x, float y, float p)
{
    SDL_TouchID touchDeviceId = 0;
    SDL_FingerID fingerId = 0;
    float tempX = 0.0, tempY = 0.0;

    if (!window) {
        return;
    }

    touchDeviceId = (SDL_TouchID)touch_device_id_in;
    if (SDL_AddTouch(touchDeviceId, SDL_TOUCH_DEVICE_DIRECT, "") < 0) {
        SDL_Log("error: can't add touch %s, %d", __FILE__, __LINE__);
    }

    fingerId = (SDL_FingerID)pointer_finger_id_in;
    switch (action) {
        case ACTION_DOWN:
//      case ACTION_POINTER_DOWN:
            if (window->w != 0 && window->h != 0) {
                tempX = floor(x * 10000) / ((float)window->w * 10000);
                tempY = floor(y * 10000) / ((float)window->h * 10000);
            } else {
                tempX = 0.0;
                tempY = 0.0;
            }
            SDL_SendTouch(touchDeviceId, fingerId, window, SDL_TRUE, tempX, tempY, p);
            break;

        case ACTION_MOVE:
            SDL_SendTouchMotion(touchDeviceId, fingerId, window, x, y, p);
            break;

        case ACTION_UP:
            SDL_SendTouch(touchDeviceId, fingerId, window, SDL_FALSE, x, y, p);
            break;

        default:
            break;
    }
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
