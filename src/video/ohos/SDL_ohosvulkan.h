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

//#include "../../SDL_internal.h"

#ifndef SDL_ohosvulkan_h_
#define SDL_ohosvulkan_h_

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"
#include "../../core/ohos/SDL_ohos_xcomponent.h"

#if SDL_VIDEO_VULKAN && SDL_VIDEO_DRIVER_OHOS

int OHOS_Vulkan_LoadLibrary(_THIS, const char *path);
void OHOS_Vulkan_UnloadLibrary(_THIS);
SDL_bool OHOS_Vulkan_GetInstanceExtensions(_THIS,
                                          SDL_Window *window,
                                          unsigned *count,
                                          const char **names);
SDL_bool OHOS_Vulkan_CreateSurface(_THIS,
                                  SDL_Window *window,
                                  VkInstance instance,
                                  VkSurfaceKHR *surface);

#endif

#endif /* SDL_ohosvulkan_h_ */

/* vi: set ts=4 sw=4 expandtab: */
