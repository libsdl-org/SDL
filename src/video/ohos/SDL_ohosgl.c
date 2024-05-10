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

#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

/* OHOS SDL video driver implementation */

#include "SDL_video.h"
#include "../SDL_egl_c.h"
#include "SDL_ohoswindow.h"

#include "SDL_ohosvideo.h"
#include "SDL_ohosgl.h"
#include "../../core/ohos/SDL_ohos.h"

#include <dlfcn.h>

int OHOS_GLES_MakeCurrent(SDL_VideoDevice *thisDevice, SDL_Window * window, SDL_GLContext context)
{
    SDL_Log("sdlthread OHOS_GLES_MakeCurrent component = %p， native_window=%p",
            ((SDL_WindowData *)window->driverdata)->egl_xcomponent,
            ((SDL_WindowData *)window->driverdata)->native_window);
    if (window && context) {
        return SDL_EGL_MakeCurrent(thisDevice, ((SDL_WindowData *) window->driverdata)->egl_xcomponent, context);
    } else {
        return SDL_EGL_MakeCurrent(thisDevice, NULL, NULL);
    }
    SDL_Log("sdlthread OHOS_GLES_MakeCurrent after ----------------------- component = %p， native_window=%p",
            ((SDL_WindowData *)window->driverdata)->egl_xcomponent,
            ((SDL_WindowData *)window->driverdata)->native_window);
}


SDL_GLContext OHOS_GLES_CreateContext(SDL_VideoDevice *thisDevice, SDL_Window * window)
{
    SDL_GLContext ret;
    OHOS_PAGEMUTEX_LockRunning();
    SDL_Log("sdlthread SDL_EGL_CreateContext component = %p， native_window=%p", ((SDL_WindowData *)window->driverdata)->egl_xcomponent,
            ((SDL_WindowData *)window->driverdata)->native_window);
    ret = SDL_EGL_CreateContext(thisDevice, ((SDL_WindowData *) window->driverdata)->egl_xcomponent);
    SDL_Log("sdlthread SDL_EGL_CreateContext after ----------------------------- component = %p， native_window=%p",
            ((SDL_WindowData *)window->driverdata)->egl_xcomponent,
            ((SDL_WindowData *)window->driverdata)->native_window);
    SDL_UnlockMutex(OHOS_PageMutex);
    return ret;
}


int OHOS_GLES_SwapWindow(SDL_VideoDevice *thisDevice, SDL_Window * window)
{
    int retval;
    SDL_LockMutex(OHOS_PageMutex);
    OHOS_ResetWindowData(window);
//     SDL_Log("sdlthread OHOS_GLES_SwapWindow component = %p， native_window=%p window->driverdata = %p",
//             ((SDL_WindowData *)window->driverdata)->egl_xcomponent,
//             ((SDL_WindowData *)window->driverdata)->native_window, window->driverdata);
    /* The following two calls existed in the original Java code
     * If you happen to have a device that's affected by their removal,
     * please report to Bugzilla. -- Gabriel
     */
    retval = SDL_EGL_SwapBuffers(thisDevice, ((SDL_WindowData *) window->driverdata)->egl_xcomponent);
    SDL_UnlockMutex(OHOS_PageMutex);

    return retval;
}


int OHOS_GLES_LoadLibrary(SDL_VideoDevice *thisDevice, const char *path)
{
    return SDL_EGL_LoadLibrary(thisDevice, path, (NativeDisplayType) 0, 0);
}

#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
