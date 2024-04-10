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

#ifndef SDL_OHOSVULKAN_H
#define SDL_OHOSVULKAN_H

#include "../../core/ohos/SDL_ohos.h"
#include "../SDL_egl_c.h"

typedef enum OhosVkStructureType {
    VK_STRUCTURE_TYPE_OSOS_XCOMPONENT_CREATE_INFO_KHR = 1000008000,
}OhosType;


#if SDL_VIDEO_VULKAN && SDL_VIDEO_DRIVER_OHOS

int OHOS_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path);
void OHOS_Vulkan_UnloadLibrary(SDL_VideoDevice *_this);
SDL_bool OHOS_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this,
                                           SDL_Window *window,
                                           unsigned *count,
                                           const char **names);
SDL_bool OHOS_Vulkan_CreateSurface(SDL_VideoDevice *_this,
                                   SDL_Window *window,
                                   VkInstance instance,
                                   VkSurfaceKHR *xcomponent);

#endif

#endif /* SDL_ohosvulkan_h_ */

/* vi: set ts=4 sw=4 expandtab: */
