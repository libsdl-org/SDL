/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_ANDROID

/* Android SDL video driver implementation */

#include "SDL_video.h"
#include "../SDL_egl.h"
#include "SDL_androidwindow.h"

#include "SDL_androidvideo.h"
#include "../../core/android/SDL_android.h"

#include <android/log.h>

#include <dlfcn.h>

SDL_EGL_CreateContext_impl(Android)
SDL_EGL_MakeCurrent_impl(Android)

void
Android_GLES_SwapWindow(_THIS, SDL_Window * window)
{
    /* FIXME: These two functions were in the Java code, do we really need them? */
    _this->egl_data->eglWaitNative(EGL_CORE_NATIVE_ENGINE);
    _this->egl_data->eglWaitGL();
    SDL_EGL_SwapBuffers(_this, ((SDL_WindowData *) window->driverdata)->egl_surface);
}

int
Android_GLES_LoadLibrary(_THIS, const char *path) {
    return SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType) 0);
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
