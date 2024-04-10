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

#ifndef SDL_OHOSGL_H
#define SDL_OHOSGL_H

#include "SDL_ohosvideo.h"
#include "../../SDL_internal.h"

SDL_GLContext OHOS_GLES_CreateContext(SDL_VideoDevice *thisDevice, SDL_Window* window);
int OHOS_GLES_MakeCurrent(SDL_VideoDevice *thisDevice, SDL_Window* window, SDL_GLContext context);
int OHOS_GLES_SwapWindow(SDL_VideoDevice *thisDevice, SDL_Window* window);
int OHOS_GLES_LoadLibrary(SDL_VideoDevice *thisDevice, const char *path);

#endif /* SDL_OHOSGL_H */

/* vi: set ts=4 sw=4 expandtab: */
