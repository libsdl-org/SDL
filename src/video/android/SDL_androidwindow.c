/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_ANDROID

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"

#include "SDL_androidvideo.h"
#include "SDL_androidwindow.h"

int
Android_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data;
    
    if (Android_Window) {
        return SDL_SetError("Android only supports one window");
    }
    
    Android_PauseSem = SDL_CreateSemaphore(0);
    Android_ResumeSem = SDL_CreateSemaphore(0);

    /* Adjust the window data to match the screen */
    window->x = 0;
    window->y = 0;
    window->w = Android_ScreenWidth;
    window->h = Android_ScreenHeight;

    window->flags &= ~SDL_WINDOW_RESIZABLE;     /* window is NEVER resizeable */
    window->flags |= SDL_WINDOW_FULLSCREEN;     /* window is always fullscreen */
    window->flags &= ~SDL_WINDOW_HIDDEN;
    window->flags |= SDL_WINDOW_SHOWN;          /* only one window on Android */
    window->flags |= SDL_WINDOW_INPUT_FOCUS;    /* always has input focus */

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    
    data = (SDL_WindowData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    
    data->native_window = Android_JNI_GetNativeWindow();
    
    if (!data->native_window) {
        SDL_free(data);
        return SDL_SetError("Could not fetch native window");
    }
    
    data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) data->native_window);

    if (data->egl_surface == EGL_NO_SURFACE) {
        ANativeWindow_release(data->native_window);
        SDL_free(data);
        return SDL_SetError("Could not create GLES window surface");
    }

    window->driverdata = data;
    Android_Window = window;
    
    return 0;
}

void
Android_SetWindowTitle(_THIS, SDL_Window * window)
{
    Android_JNI_SetActivityTitle(window->title);
}

void
Android_DestroyWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data;
    
    if (window == Android_Window) {
        Android_Window = NULL;
        if (Android_PauseSem) SDL_DestroySemaphore(Android_PauseSem);
        if (Android_ResumeSem) SDL_DestroySemaphore(Android_ResumeSem);
        Android_PauseSem = NULL;
        Android_ResumeSem = NULL;
        
        if(window->driverdata) {
            data = (SDL_WindowData *) window->driverdata;
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_DestroySurface(_this, data->egl_surface);
            }
            if (data->native_window) {
                ANativeWindow_release(data->native_window);
            }
            SDL_free(window->driverdata);
            window->driverdata = NULL;
        }
    }
}

SDL_bool
Android_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_ANDROID;
        info->info.android.window = data->native_window;
        info->info.android.surface = data->egl_surface;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
