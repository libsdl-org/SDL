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

#ifndef SDL_OHOSVIDEO_H
#define SDL_OHOSVIDEO_H

#include <EGL/eglplatform.h>
#include "../../core/ohos/SDL_ohos.h"
#include "SDL_mutex.h"
#include "SDL_rect.h"
#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"
#include "SDL_ohoswindow.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* Called by the JNI layer when the screen changes size or format */
extern void OHOS_SetScreenResolution(int deviceWidth, int deviceHeight, Uint32 format,
                                     float rate, double screenDensity);
extern void OHOS_SendResize(SDL_Window *window);
extern void OHOS_SetScreenSize(int surfaceWidth, int surfaceHeight);

/* Private display data */

typedef struct SDL_VideoData {
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;


extern int g_ohosSurfaceWidth;
extern int g_ohosSurfaceHeight;
extern int g_ohosDeviceWidth;
extern int g_ohosDeviceHeight;
extern SDL_sem *OHOS_PauseSem, *OHOS_ResumeSem;
extern SDL_mutex *OHOS_PageMutex;
extern double OHOS_ScreenDensity;

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* SDL_OHOSVIDEO_H */

/* vi: set ts=4 sw=4 expandtab: */
