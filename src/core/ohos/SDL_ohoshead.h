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

#ifndef SDL_OHOSHEAD_H
#define SDL_OHOSHEAD_H

#include "SDL_mutex.h"
#include "../../core/ohos/SDL_ohos.h"
#include "../src/video/SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"

typedef struct {
    SDL_mutex *mLock;
    SDL_cond *mCond;
} OhosThreadLock;

typedef struct {
    EGLSurface egl_xcomponent;
    EGLContext egl_context; /* We use this to preserve the context when losing focus */
    SDL_bool   backup_done;
    OHNativeWindow *native_window;
    uint64_t width;
    uint64_t height;
    double x;
    double y;
} SDL_WindowData;

#endif