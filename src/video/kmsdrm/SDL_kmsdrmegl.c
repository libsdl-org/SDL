/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_KMSDRM

#include "SDL_kmsdrmvideo.h"
#include "SDL_kmsdrmopengles.h"

#define OPENGL_REQUIRES_DLOPEN
#if defined(OPENGL_REQUIRES_DLOPEN) && defined(SDL_LOADSO_DLOPEN)
#include <dlfcn.h>
#define GL_LoadObject(X)    dlopen(X, (RTLD_NOW|RTLD_GLOBAL))
#define GL_LoadFunction     dlsym
#define GL_UnloadObject     dlclose
#else
#define GL_LoadObject   SDL_LoadObject
#define GL_LoadFunction SDL_LoadFunction
#define GL_UnloadObject SDL_UnloadObject
#endif

#define LOAD_FUNC(NAME) \
_this->egl_data->NAME = GL_LoadFunction(_this->egl_data->dll_handle, #NAME); \
if (!_this->egl_data->NAME) \
{ \
    return SDL_SetError("Could not retrieve EGL function " #NAME); \
}

#define LOAD_FUNC_EGLEXT(NAME) \
    _this->egl_data->NAME = _this->egl_data->eglGetProcAddress(#NAME);

static void
KMSDRM_EGL_GetVersion(_THIS) {
    if (_this->egl_data->eglQueryString) {
        const char *egl_version = _this->egl_data->eglQueryString(_this->egl_data->egl_display, EGL_VERSION);
        if (egl_version) {
            int major = 0, minor = 0;
            if (SDL_sscanf(egl_version, "%d.%d", &major, &minor) == 2) {
                _this->egl_data->egl_version_major = major;
                _this->egl_data->egl_version_minor = minor;
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Could not parse EGL version string: %s", egl_version);
            }
        }
    }
}

int
KMSDRM_EGL_LoadLibraryOnly(_THIS)
{
    /* No need to load libOpenGL, libGL or other GL dlls explicitly.
       Loading libEGL.so is enough for everything. */
    char *path = "libEGL.so";
    void *dll_handle = NULL;

    if (_this->egl_data) {
        return SDL_SetError("EGL context already created");
    }

    _this->egl_data = (struct SDL_EGL_VideoData *) SDL_calloc(1, sizeof(SDL_EGL_VideoData));
    if (!_this->egl_data) {
        return SDL_OutOfMemory();
    }

    dll_handle = GL_LoadObject(path);
    if (dll_handle == NULL) {
        return SDL_SetError("EGL library not found");
    }

    _this->egl_data->dll_handle = dll_handle;

    if (path) {
        SDL_strlcpy(_this->gl_config.driver_path, path, sizeof(_this->gl_config.driver_path) - 1);
    } else {
        *_this->gl_config.driver_path = '\0';
    }

    /* Load new function pointers */
    LOAD_FUNC(eglGetDisplay);
    LOAD_FUNC(eglInitialize);
    LOAD_FUNC(eglTerminate);
    LOAD_FUNC(eglGetProcAddress);
    LOAD_FUNC(eglChooseConfig);
    LOAD_FUNC(eglGetConfigAttrib);
    LOAD_FUNC(eglCreateContext);
    LOAD_FUNC(eglDestroyContext);
    LOAD_FUNC(eglCreatePbufferSurface);
    LOAD_FUNC(eglCreateWindowSurface);
    LOAD_FUNC(eglDestroySurface);
    LOAD_FUNC(eglMakeCurrent);
    LOAD_FUNC(eglSwapBuffers);
    LOAD_FUNC(eglSwapInterval);
    LOAD_FUNC(eglWaitNative);
    LOAD_FUNC(eglWaitGL);
    LOAD_FUNC(eglBindAPI);
    LOAD_FUNC(eglQueryAPI);
    LOAD_FUNC(eglQueryString);
    LOAD_FUNC(eglGetError);
    LOAD_FUNC_EGLEXT(eglQueryDevicesEXT);
    LOAD_FUNC_EGLEXT(eglGetPlatformDisplayEXT);

    #if 0 /* If these are ever needed again, they should be loaded here. */
    /* Atomic functions */
    LOAD_FUNC_EGLEXT(eglCreateSyncKHR);
    LOAD_FUNC_EGLEXT(eglDestroySyncKHR);
    LOAD_FUNC_EGLEXT(eglDupNativeFenceFDANDROID);
    LOAD_FUNC_EGLEXT(eglWaitSyncKHR);
    LOAD_FUNC_EGLEXT(eglClientWaitSyncKHR);
    /* Atomic functions end */
    #endif

    return 0;
}

int
KMSDRM_EGL_LoadLibrary(_THIS, NativeDisplayType native_display)
{
    int egl_version_major, egl_version_minor;
    int ret = KMSDRM_EGL_LoadLibraryOnly(_this);
    if (ret != 0) {
        return ret;
    }

    /* EGL 1.5 allows querying for client version with EGL_NO_DISPLAY */
    KMSDRM_EGL_GetVersion(_this);

    egl_version_major = _this->egl_data->egl_version_major;
    egl_version_minor = _this->egl_data->egl_version_minor;

    if (egl_version_major == 1 && egl_version_minor == 5) {
        LOAD_FUNC(eglGetPlatformDisplay);
    }

    _this->egl_data->egl_display = EGL_NO_DISPLAY;

    /* Use the implementation-specific eglGetDisplay */
    _this->egl_data->egl_display = _this->egl_data->eglGetDisplay(native_display);

    if (_this->egl_data->egl_display == EGL_NO_DISPLAY) {
        _this->gl_config.driver_loaded = 0;
        *_this->gl_config.driver_path = '\0';
        return SDL_SetError("Could not get EGL display");
    }
    
    if (_this->egl_data->eglInitialize(_this->egl_data->egl_display, NULL, NULL) != EGL_TRUE) {
        _this->gl_config.driver_loaded = 0;
        *_this->gl_config.driver_path = '\0';
        return SDL_SetError("Could not initialize EGL");
    }

    _this->egl_data->is_offscreen = 0;

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_KMSDRM */
