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

#include "SDL_androidvideo.h"
#include "../../core/android/SDL_android.h"

#include <android/log.h>

#include <dlfcn.h>

static void* Android_GLHandle = NULL;

/* GL functions */
int
Android_GL_LoadLibrary(_THIS, const char *path)
{
    if (!Android_GLHandle) {
        Android_GLHandle = dlopen("libGLESv1_CM.so",RTLD_GLOBAL);
        if (!Android_GLHandle) {
            return SDL_SetError("Could not initialize GL ES library\n");
        }
    }
    return 0;
}

void *
Android_GL_GetProcAddress(_THIS, const char *proc)
{
    /*
       !!! FIXME: this _should_ use eglGetProcAddress(), but it appears to be
       !!! FIXME:  busted on Android at the moment...
       !!! FIXME:  http://code.google.com/p/android/issues/detail?id=7681
       !!! FIXME: ...so revisit this later.  --ryan.
    */
    return dlsym(Android_GLHandle, proc);
}

void
Android_GL_UnloadLibrary(_THIS)
{
    if(Android_GLHandle) {
        dlclose(Android_GLHandle);
        Android_GLHandle = NULL;
    }
}

SDL_GLContext
Android_GL_CreateContext(_THIS, SDL_Window * window)
{
    if (!Android_JNI_CreateContext(_this->gl_config.major_version,
                                   _this->gl_config.minor_version,
                                   _this->gl_config.red_size,
                                   _this->gl_config.green_size,
                                   _this->gl_config.blue_size,
                                   _this->gl_config.alpha_size,
                                   _this->gl_config.buffer_size,
                                   _this->gl_config.depth_size,
                                   _this->gl_config.stencil_size,
                                   _this->gl_config.multisamplebuffers,
                                   _this->gl_config.multisamplesamples)) {
        SDL_SetError("Couldn't create OpenGL context - see Android log for details");
        return NULL;
    }
    return (SDL_GLContext)1;
}

int
Android_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
    /* There's only one context, nothing to do... */
    return 0;
}

int
Android_GL_SetSwapInterval(_THIS, int interval)
{
    __android_log_print(ANDROID_LOG_INFO, "SDL", "[STUB] GL_SetSwapInterval\n");
    return 0;
}

int
Android_GL_GetSwapInterval(_THIS)
{
    __android_log_print(ANDROID_LOG_INFO, "SDL", "[STUB] GL_GetSwapInterval\n");
    return 0;
}

void
Android_GL_SwapWindow(_THIS, SDL_Window * window)
{
    Android_JNI_SwapWindow();
}

void
Android_GL_DeleteContext(_THIS, SDL_GLContext context)
{
    if (context) {
        Android_JNI_DeleteContext();
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
