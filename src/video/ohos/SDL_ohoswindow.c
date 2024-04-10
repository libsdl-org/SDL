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

#define OHOS_GETWINDOW_DELAY_TIME 2
#define TIMECONSTANT 3000

#ifdef SDL_VIDEO_DRIVER_OHOS
#if SDL_VIDEO_DRIVER_OHOS
#endif

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/ohos/SDL_ohos.h"

#include "SDL_ohosvideo.h"
#include "SDL_ohoswindow.h"
#include "SDL_hints.h"

/* Currently only one window */
SDL_Window *g_ohosWindow = NULL;
SDL_atomic_t bWindowCreateFlag;

int OHOS_CreateWindow(SDL_VideoDevice *thisDevice, SDL_Window * window)
{
    SDL_WindowData *data;
    int retval = 0;
    unsigned int delaytime = 0;
    
    while (SDL_AtomicGet(&bWindowCreateFlag) == SDL_FALSE) {
        SDL_Delay(OHOS_GETWINDOW_DELAY_TIME);
    }

    if (g_ohosWindow) {
        retval = SDL_SetError("OHOS only supports one window");
        goto endfunction;
    }

    if ((window->flags & SDL_WINDOW_RESIZABLE) != 0) {
        window->flags &= ~SDL_WINDOW_RESIZABLE;
    }
    
    /* Adjust the window data to match the screen */
    window->x = window->windowed.x;
    window->y = window->windowed.y;
    window->w = g_ohosSurfaceWidth;
    window->h = g_ohosSurfaceHeight;
    
    window->flags &= ~SDL_WINDOW_HIDDEN;
    window->flags |= SDL_WINDOW_SHOWN; /* only one window on OHOS */

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        retval = SDL_OutOfMemory();
        goto endfunction;
    }
    
    data->native_window = gNative_window;
    
    if (!data->native_window) {
        SDL_free(data);
        retval = SDL_SetError("Could not fetch native window");
        goto endfunction;
    }

    /* Do not create EGLSurface for Vulkan window since it will then make the window
       incompatible with vkCreateOHOSSurfaceKHR */
    if ((window->flags & SDL_WINDOW_OPENGL) != 0) {
        data->egl_xcomponent = (EGLSurface)SDL_EGL_CreateSurface(thisDevice, (NativeWindowType)data->native_window);
    
        if (data->egl_xcomponent == EGL_NO_SURFACE) {
            SDL_free(data->native_window);
            SDL_free(data);
            retval = -1;
            goto endfunction;
        }
    }

    window->driverdata = data;
    g_ohosWindow = window;
    SDL_SendWindowEvent(g_ohosWindow, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);
    
endfunction :
    SDL_UnlockMutex(OHOS_PageMutex);

    return retval;
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

    if (window == g_ohosWindow) {
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

    if (window == g_ohosWindow) {
        g_ohosWindow = NULL;

        if (window->driverdata) {
            SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
            if (data->egl_xcomponent != EGL_NO_SURFACE) {
                SDL_EGL_DestroySurface(thisDevice, data->egl_xcomponent);
            }
            if (data->native_window) {
            }
            SDL_free(window->driverdata);
            window->driverdata = NULL;
        }
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
    if ((window->flags & SDL_WINDOW_RESIZABLE) != 0) {
        window->flags &= ~SDL_WINDOW_RESIZABLE;
    }
    SDL_SetWindowResizable(window, SDL_TRUE);
    while (SDL_TRUE) {
        if ((window->w == g_ohosSurfaceWidth) && (window->h == g_ohosSurfaceHeight)) {
            break;
        }
        SDL_Delay(OHOS_GETWINDOW_DELAY_TIME);
    }
}

int OHOS_CreateWindowFrom(SDL_VideoDevice *thisDevice, SDL_Window *window, const void *data)
{
    SDL_Window *w = (SDL_Window *)data;

    window->title = OHOS_GetWindowTitle(thisDevice, w);

    if (SetupWindowData(thisDevice, window, w) < 0) {
        return -1;
    }
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

int SetupWindowData(SDL_VideoDevice *thisDevice, SDL_Window *window, SDL_Window *w)
{
    SDL_WindowData *data;
    unsigned int delaytime = 0;

    OHOS_PAGEMUTEX_LockRunning();
    /* Allocate the window data */
    window->flags = w->flags;
    window->x = w->x;
    window->y = w->y;
    window->w = w->w;
    window->h = w->h;

    OHOS_DestroyWindow(thisDevice, w);
    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    while (gNative_window == NULL && delaytime < TIMECONSTANT) {
        delaytime += OHOS_GETWINDOW_DELAY_TIME;
        SDL_Delay(OHOS_GETWINDOW_DELAY_TIME);
    }
    data->native_window = gNative_window;
    if (!data->native_window) {
        SDL_free(data);
        goto endfunction;
    }

    if ((window->flags & SDL_WINDOW_OPENGL) != 0) {
        data->egl_xcomponent = (EGLSurface)SDL_EGL_CreateSurface(thisDevice, (NativeWindowType)data->native_window);

        if (data->egl_xcomponent == EGL_NO_SURFACE) {
            SDL_free(data->native_window);
            SDL_free(data);
            goto endfunction;
        }
    }
    window->driverdata = data;
    SDL_SendWindowEvent(g_ohosWindow, SDL_WINDOWEVENT_FOCUS_GAINED, 0, 0);

endfunction:
    SDL_UnlockMutex(OHOS_PageMutex);

    /* All done! */
    return 0;
}
#endif /* SDL_VIDEO_DRIVER_OHOS */

/* vi: set ts=4 sw=4 expandtab: */
