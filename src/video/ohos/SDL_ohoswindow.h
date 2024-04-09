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

#ifndef SDL_OHOSWINDOW_H_
#define SDL_OHOSWINDOW_H_

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"


extern int OHOS_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void OHOS_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern void OHOS_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
extern void OHOS_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);

extern void OHOS_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern SDL_bool OHOS_GetWindowWMInfo(SDL_VideoDevice *_this, SDL_Window *window, struct SDL_SysWMinfo *info);
extern void OHOS_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable);
extern void OHOS_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
extern int OHOS_CreateWindowFrom(SDL_VideoDevice *_this, SDL_Window *window, const void *data);
extern char *OHOS_GetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern int SetupWindowData(SDL_VideoDevice *_this, SDL_Window *window, SDL_Window *w);
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
