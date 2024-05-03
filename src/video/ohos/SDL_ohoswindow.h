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

#ifndef SDL_OHOSWINDOW_H
#define SDL_OHOSWINDOW_H

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohoshead.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

extern int OHOS_CreateWindow(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern void OHOS_SetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern void OHOS_SetWindowFullscreen(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_VideoDisplay *display,
                                     SDL_bool fullscreen);
extern void OHOS_MinimizeWindow(SDL_VideoDevice *thisDevice, SDL_Window *window);

extern void OHOS_DestroyWindow(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern SDL_bool OHOS_GetWindowWMInfo(SDL_VideoDevice *thisDevice, SDL_Window *window, struct SDL_SysWMinfo *info);
extern void OHOS_SetWindowResizable(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_bool resizable);
extern void OHOS_SetWindowSize(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern void OHOS_SetWindowPosition(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern int OHOS_CreateWindowFrom(SDL_VideoDevice *thisDevice, SDL_Window *window, const void *data);
extern char *OHOS_GetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window);
extern int SetupWindowData(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_Window *w, SDL_WindowData *data);

extern void OHOS_ResetWindowData(SDL_Window *window);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* SDL_OHOSWINDOW_H */

/* vi: set ts=4 sw=4 expandtab: */
