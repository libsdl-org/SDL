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

#ifndef SDL_ohosvideo_h_
#define SDL_ohosvideo_h_

#include "SDL_mutex.h"
#include "SDL_rect.h"
#include "../SDL_sysvideo.h"

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"
#include <EGL/eglplatform.h>
#include "../../core/ohos/SDL_ohos_xcomponent.h"
#include "SDL_ohoswindow.h"

/* Called by the JNI layer when the screen changes size or format */
extern void OHOS_SetScreenResolution(int deviceWidth, int deviceHeight, Uint32 format, float rate);
extern void OHOS_SendResize(SDL_Window *window);
extern void OHOS_SetScreenSize(int surfaceWidth, int surfaceHeight);

/* Private display data */

typedef struct SDL_VideoData
{
    SDL_Rect textRect;
    int      isPaused;
    int      isPausing;
} SDL_VideoData;

extern int OHOS_SurfaceWidth;
extern int OHOS_SurfaceHeight;
extern int OHOS_DeviceWidth;
extern int OHOS_DeviceHeight;
extern SDL_sem *OHOS_PauseSem, *OHOS_ResumeSem;
extern SDL_mutex *OHOS_PageMutex;
extern SDL_Window *OHOS_Window;

#endif /* SDL_ohosvideo_h_ */

/* vi: set ts=4 sw=4 expandtab: */
