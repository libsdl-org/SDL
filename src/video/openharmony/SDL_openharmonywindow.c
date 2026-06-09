/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_OPENHARMONY

// !!! FIXME: these are defined as "const uint32_t VARNAME = VALUE;" in native_interface_xcomponent.h, which becomes a global variable in _our_ C code! Maybe C++ handles this differently...?
#define OH_XCOMPONENT_ID_LEN_MAX sdl_ohoswindow_OH_XCOMPONENT_ID_LEN_MAX
#define OH_MAX_TOUCH_POINTS_NUMBER sdl_ohoswindow_OH_MAX_TOUCH_POINTS_NUMBER
#include <ace/xcomponent/native_interface_xcomponent.h>

#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/openharmony/SDL_openharmony.h"

#include "SDL_openharmonyvideo.h"
#include "SDL_openharmonyevents.h"
#include "SDL_openharmonywindow.h"


// Currently only one window
SDL_Window *OPENHARMONY_Window = NULL;

bool OPENHARMONY_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *data;
    bool result = true;

    // !!! FIXME: Android has locking here; maybe it isn't necessary for OpenHarmony? What happens if we're in a background thread?
    //if (!OPENHARMONY_WaitActiveAndLockActivity()) {
    //    return false;
    //}

    if (OPENHARMONY_Window) {
        result = SDL_SetError("SDL on OpenHarmony (currently) only supports one window");
        goto endfunction;
    }

    void *voidxcomp, *native_window;
    SDL_OpenHarmonyGetNativeWindowPointers(&voidxcomp, &native_window);
    OH_NativeXComponent *native_xcomponent = (OH_NativeXComponent *) voidxcomp;
    if (!native_xcomponent || !native_window) {
        result = SDL_SetError("Don't have Native XComponent or Native Window");
        goto endfunction;
    }

#if 0  // !!! FIXME
    // Set orientation
    Android_JNI_SetOrientation(window->w, window->h, window->flags & SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));
#endif

    uint64_t w, h;
    OH_NativeXComponent_GetXComponentSize(native_xcomponent, native_window, &w, &h);

    // Adjust the window data to match the screen
    window->x = 0;
    window->y = 0;
    window->w = (int) w;
    window->h = (int) h;

    // One window, it always has focus
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        result = false;
        goto endfunction;
    }

    data->native_xcomponent = native_xcomponent;
    data->native_window = native_window;
    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_OPENHARMONY_XCOMPONENT_POINTER, data->native_xcomponent);
    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_OPENHARMONY_WINDOW_POINTER, data->native_window);

    /* Do not create EGLSurface for Vulkan window since it will then make the window
       incompatible with vkCreateAndroidSurfaceKHR */
#ifdef SDL_VIDEO_OPENGL_EGL
    if (window->flags & SDL_WINDOW_OPENGL) {
        data->egl_surface = SDL_EGL_CreateSurface(_this, window, (NativeWindowType)data->native_window);
        if (data->egl_surface == EGL_NO_SURFACE) {
            SDL_free(data);
            result = false;
            goto endfunction;
        }
    }
    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_OPENHARMONY_SURFACE_POINTER, data->egl_surface);
#endif

#if 0  // !!! FIXME
    SDL_SetWindowSafeAreaInsets(window, Android_SafeInsetLeft, Android_SafeInsetRight, Android_SafeInsetTop, Android_SafeInsetBottom);
#endif

    window->internal = data;
    OPENHARMONY_Window = window;

endfunction:

    // !!! FIXME: Android has locking here; maybe it isn't necessary for OpenHarmony? What happens if we're in a background thread?
    //Android_UnlockActivityMutex();

    return result;
}

void OPENHARMONY_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
#if 0  // !!! FIXME
    Android_JNI_SetActivityTitle(window->title);
#endif
}

SDL_FullscreenResult OPENHARMONY_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen)
{
    if (window == OPENHARMONY_Window) {
        const bool show_bars = (fullscreen == SDL_FULLSCREEN_OP_LEAVE);
        if (!SDL_OpenHarmonyToggleSystemBars(show_bars, show_bars)) {
            return SDL_FULLSCREEN_FAILED;
        }
        // !!! FIXME: make sure this causes an XComponent resize event, that we can pick up later.
        return SDL_FULLSCREEN_PENDING;
    }
    return SDL_FULLSCREEN_FAILED;
}

void OPENHARMONY_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
#if 0  // !!! FIXME
    Android_JNI_MinimizeWindow();
#endif
}

void OPENHARMONY_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, bool resizable)
{
#if 0  // !!! FIXME
    // Set orientation
    Android_JNI_SetOrientation(window->w, window->h, window->flags & SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));
#endif
}

void OPENHARMONY_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    // !!! FIXME: Android has locking here; maybe it isn't necessary for OpenHarmony? What happens if we're in a background thread?
    //Android_LockActivityMutex();

    if (window == OPENHARMONY_Window) {
        OPENHARMONY_Window = NULL;

        if (window->internal) {
            SDL_WindowData *data = window->internal;

#ifdef SDL_VIDEO_OPENGL_EGL
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_DestroySurface(_this, data->egl_surface);
            }
#endif

            // the XComponent and native window live past the SDL_Window; it can be reused for future windows.

            SDL_free(window->internal);
            window->internal = NULL;
        }
    }

    //Android_UnlockActivityMutex();
}

#endif // SDL_VIDEO_DRIVER_OPENHARMONY
