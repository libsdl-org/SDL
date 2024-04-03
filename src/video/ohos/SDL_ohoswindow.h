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

#ifndef SDL_ohoswindow_h_
#define SDL_ohoswindow_h_

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"


extern int OHOS_CreateWindow(_THIS, SDL_Window *window);
extern void OHOS_SetWindowTitle(_THIS, SDL_Window *window);
extern void OHOS_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
extern void OHOS_MinimizeWindow(_THIS, SDL_Window *window);

extern void OHOS_DestroyWindow(_THIS, SDL_Window *window);
extern SDL_bool OHOS_GetWindowWMInfo(_THIS, SDL_Window *window, struct SDL_SysWMinfo *info);
extern void OHOS_SetWindowResizable(_THIS, SDL_Window *window, SDL_bool resizable);
extern void OHOS_SetWindowSize(_THIS, SDL_Window *window);
extern int OHOS_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
extern char *OHOS_GetWindowTitle(_THIS, SDL_Window *window);
extern int SetupWindowData(_THIS, SDL_Window *window, SDL_Window *w);
extern SDL_Window *OHOS_Window;
extern SDL_atomic_t bWindowCreateFlag;
typedef struct
{
    EGLSurface egl_surface;
    EGLContext egl_context; /* We use this to preserve the context when losing focus */
    SDL_bool   backup_done;
    OHNativeWindow *native_window;
} SDL_WindowData;

#endif /* SDL_ohoswindow_h_ */

/* vi: set ts=4 sw=4 expandtab: */
