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
#include "../../core/ohos/SDL_ohosplugin_c.h"
#include "SDL_timer.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <locale.h>
#include <unistd.h>

#define OHOS_GETWINDOW_DELAY_TIME 2
#define TIMECONSTANT 3000

#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

#include "napi/native_api.h"
#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/ohos/SDL_ohos.h"
#include "../../core/ohos/SDL_ohosplugin_c.h"

#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_hints.h"
#include <pthread.h>

/* Currently only one window */

int OHOS_CreateWindow(SDL_VideoDevice *thisDevice, SDL_Window * window)
{
    napi_ref parentWindowNode = NULL;
    napi_ref childWindowNode = NULL;
    if (window->ohosHandle == NULL) {
        OHOS_GetRootNode(g_windowId, &parentWindowNode);
        if (parentWindowNode == NULL) {
            return -1;
        }
        OHOS_AddChildNode(parentWindowNode, &childWindowNode, window->x, window->y, window->w, window->h);
        napi_release_threadsafe_function(parentWindowNode, napi_tsfn_release);
        if (childWindowNode == NULL) {
            return -1;
        }
    } else {
        childWindowNode = window->ohosHandle;
    }
    OHOS_CreateWindowFrom(thisDevice, window, childWindowNode);
    return 0;
}

void OHOS_SetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    OHOS_NAPI_SetTitle(window->title);
}

void OHOS_SetWindowFullscreen(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_VideoDisplay *display,
                              SDL_bool fullscreen)
{
    SDL_WindowData *data;
    SDL_LockMutex(OHOS_PageMutex);

    /* If the window is being destroyed don't change visible state */
    if (!window->is_destroying) {
        OHOS_NAPI_SetWindowStyle(fullscreen);
    }

    data = (SDL_WindowData *)window->driverdata;

    if (!data || !data->native_window) {
        if (data && !data->native_window) {
            SDL_SetError("Missing native window");
        }
        goto endfunction;
    }

endfunction:

    SDL_UnlockMutex(OHOS_PageMutex);
}

void OHOS_MinimizeWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
}

void OHOS_DestroyWindow(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    SDL_LockMutex(OHOS_PageMutex);

    OHOS_RemoveChildNode(window->ohosHandle);

    if (window->driverdata) {
        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
        if (data->egl_xcomponent != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(thisDevice, data->egl_xcomponent);
        }
        data->egl_xcomponent = EGL_NO_SURFACE;
        SDL_free(window->driverdata);
        window->driverdata = NULL;
    }

    SDL_UnlockMutex(OHOS_PageMutex);
}

SDL_bool OHOS_GetWindowWMInfo(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_SysWMinfo *info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_OHOS;
        info->info.ohos.window = data->native_window;
        info->info.ohos.surface = data->egl_xcomponent;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

void OHOS_SetWindowResizable(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_bool resizable)
{
    if (resizable) {
        OHOS_NAPI_SetWindowResize(window->windowed.x, window->windowed.y, window->windowed.w, window->windowed.h);
    }
}

void OHOS_SetWindowSize(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
     OHOS_ResizeNode(window->ohosHandle, window->w, window->h);
}

void OHOS_SetWindowPosition(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
     OHOS_MoveNode(window->ohosHandle, window->x, window->y);
}



int OHOS_CreateWindowFrom(SDL_VideoDevice *thisDevice, SDL_Window *window, const void *data)
{
     char *strID = NULL;
     OH_NativeXComponent *nativeXComponent = NULL;
     OhosThreadLock *lock = NULL;
     SDL_WindowData *windowData = NULL;
     SDL_WindowData *sdlWindowData = NULL;
     pthread_t tid;

     if (data == NULL && window->ohosHandle == NULL) {
        return -1;
     }
     if (data != NULL && window->ohosHandle == NULL) {
        window->ohosHandle = data;
     }

     window->flags = (window->flags & SDL_WINDOW_OPENGL);
     strID = OHOS_GetXComponentId(window->ohosHandle);
     window->xcompentId = strID;
     // get thread id
     tid = pthread_self();
     OHOS_AddXcomPomentIdForThread(strID, tid);
     if (!OHOS_FindNativeXcomPoment(strID, &nativeXComponent)) {
        OHOS_FindOrCreateThreadLock(tid, &lock);
        if (lock == NULL) {
            return -1;
        }
        while (!OHOS_FindNativeXcomPoment(strID, &nativeXComponent)) {
            SDL_CondWait(lock->mCond, lock->mLock);
        }
     }

     SDL_Log("sdlthread OHOS_GetXComponent lllllsss over");

     if (!OHOS_FindNativeWindow(nativeXComponent, &windowData)) {
        if (lock == NULL) {
            OHOS_FindOrCreateThreadLock(tid, &lock);
            if (lock == NULL) {
                return -1;
            }
        }
        while (!OHOS_FindNativeWindow(nativeXComponent, &windowData)) {
            SDL_CondWait(lock->mCond, lock->mLock);
        }
     }
     sdlWindowData = (SDL_WindowData *)SDL_malloc(sizeof(SDL_WindowData));
     SDL_LockMutex(OHOS_PageMutex);
     window->x = windowData->x;
     window->y = windowData->y;
     window->w = windowData->width;
     window->h = windowData->height;
    
     sdlWindowData->native_window = windowData->native_window;
     if (!sdlWindowData->native_window) {
        SDL_free(data);
        goto endfunction;
     }
     SDL_Log("sdlthread OHOS_GetXComponent window_data over x = %d y = %d w = %d h = %d", window->x, window->y, window->w, window->h);
     if ((window->flags & SDL_WINDOW_OPENGL) != 0) {
        SDL_Log("sdlthread OHOS_GetXComponent window_data over windowData->egl_xcomponent = %p windowData->native_window = %p", windowData->egl_xcomponent, windowData->native_window);
        sdlWindowData->egl_xcomponent =
            (EGLSurface)SDL_EGL_CreateSurface(thisDevice, (NativeWindowType)windowData->native_window);
        if (sdlWindowData->egl_xcomponent == EGL_NO_SURFACE) {
            SDL_Log("sdlthread OHOS_GetXComponent window_data over failed -------------------------------");
            SDL_free(windowData);
            goto endfunction;
        }
     }
     window->driverdata = sdlWindowData;
     SDL_SendWindowEvent(window, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
endfunction:
     SDL_UnlockMutex(OHOS_PageMutex);
     return 0;
}

char *OHOS_GetWindowTitle(SDL_VideoDevice *thisDevice, SDL_Window *window)
{
    char *title = NULL;
    title = window->title;
    if (title) {
        return title;
    } else {
        return "Title is NULL";
    }
}
#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
