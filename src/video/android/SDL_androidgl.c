/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_VIDEO_DRIVER_ANDROID) && defined(SDL_VIDEO_OPENGL_EGL)

/* Android SDL video driver implementation */

#include "../SDL_egl_c.h"
#include "SDL_androidwindow.h"

#include "SDL_androidvideo.h"
#include "SDL_androidgl.h"
#include "../../core/android/SDL_android.h"

#include <android/log.h>

#include <dlfcn.h>

int Android_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context)
{
    if (window && context) {
        return SDL_EGL_MakeCurrent(_this, window->driverdata->egl_surface, context);
    } else {
        return SDL_EGL_MakeCurrent(_this, NULL, NULL);
    }
}

SDL_GLContext Android_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_GLContext ret;

    Android_ActivityMutex_Lock_Running();

    ret = SDL_EGL_CreateContext(_this, window->driverdata->egl_surface);

    SDL_UnlockMutex(Android_ActivityMutex);

    return ret;
}

int Android_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    int retval;

    SDL_LockMutex(Android_ActivityMutex);

    /* The following two calls existed in the original Java code
     * If you happen to have a device that's affected by their removal,
     * please report to our bug tracker. -- Gabriel
     */

    /*_this->egl_data->eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    _this->egl_data->eglWaitGL();*/
    retval = SDL_EGL_SwapBuffers(_this, window->driverdata->egl_surface);

    SDL_UnlockMutex(Android_ActivityMutex);

    return retval;
}

int Android_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    return SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)0, 0);
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */
