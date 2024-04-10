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

#ifndef SDL_OHOSKEYBOARD_H
#define SDL_OHOSKEYBOARD_H

#include "SDL_ohosvideo.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern void OHOS_InitKeyboard(void);
extern int OHOS_OnKeyDown(int keycode);
extern int OHOS_OnKeyUp(int keycode);

extern SDL_bool OHOS_HasScreenKeyboardSupport(SDL_VideoDevice *_this);
extern SDL_bool OHOS_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window* window);

extern void OHOS_StartTextInput(SDL_VideoDevice *thisDevice);
extern void OHOS_StopTextInput(SDL_VideoDevice *thisDevice);
extern void OHOS_SetTextInputRect(SDL_VideoDevice *thisDevice, SDL_Rect *rect);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* SDL_OHOSKEYBOARD_H */

/* vi: set ts=4 sw=4 expandtab: */
