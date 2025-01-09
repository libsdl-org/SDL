/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_MX6 && SDL_VIDEO_OPENGL_EGL

#include "SDL_mx6opengles.h"
#include "SDL_loadso.h"
#include "SDL_mx6video.h"

#define DEFAULT_OGL "libGL.so.1"
#define DEFAULT_EGL "libEGL.so.1"
#define DEFAULT_OGL_ES2 "libGLESv2.so.2"
#define DEFAULT_OGL_ES "libGLESv1_CM.so.1"

#define LOAD_FUNC(NAME) \
*((void**)&_this->egl_data->NAME) = SDL_LoadFunction(_this->egl_data->dll_handle, #NAME); \
if (!_this->egl_data->NAME) \
{ \
    return SDL_SetError("Could not retrieve EGL function " #NAME); \
}

#define LOAD_VIV_FUNC(NAME) \
*((void**)&egl_viv_data->NAME) = SDL_LoadFunction(_this->egl_data->dll_handle, #NAME); \
if (!egl_viv_data->NAME) \
{ \
    return SDL_SetError("Could not retrieve EGL function " #NAME); \
}

/* EGL implementation of SDL OpenGL support */

int
MX6_GLES_LoadLibrary(_THIS, const char *egl_path) {
    /* The definitions of egl_dll_handle and dll_handle were interchanged for some reason.
       Just left them as is for compatibility */
    void *dll_handle = NULL, *egl_dll_handle = NULL;
    char *path = NULL;
    SDL_DisplayData *displaydata;

    if (_this->egl_data) {
        return SDL_SetError("OpenGL ES context already created");
    }

    _this->egl_data = (struct SDL_EGL_VideoData *) SDL_calloc(1, sizeof(SDL_EGL_VideoData));
    if (!_this->egl_data) {
        return SDL_OutOfMemory();
    }

    egl_viv_data = (struct MX6_EGL_VivanteData *) SDL_calloc(1, sizeof(MX6_EGL_VivanteData));
    if (!egl_viv_data) {
        return SDL_OutOfMemory();
    }

    path = SDL_getenv("SDL_VIDEO_GL_DRIVER");
    if (path != NULL) {
        egl_dll_handle = SDL_LoadObject(path);
    }

    if (egl_dll_handle == NULL) {
        if(_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
            if (_this->gl_config.major_version > 1) {
                path = DEFAULT_OGL_ES2;
                egl_dll_handle = SDL_LoadObject(path);
            }
            else {
                path = DEFAULT_OGL_ES;
                egl_dll_handle = SDL_LoadObject(path);
            }
        }      
        else {
            path = DEFAULT_OGL;
            egl_dll_handle = SDL_LoadObject(path);
        }     
    }
    _this->egl_data->egl_dll_handle = egl_dll_handle;

    if (egl_dll_handle == NULL) {
        return SDL_SetError("Could not initialize OpenGL / GLES library");
    }

    if (egl_path != NULL) {
        dll_handle = SDL_LoadObject(egl_path);
    }   
    
    if (SDL_LoadFunction(dll_handle, "eglChooseConfig") == NULL) {
        if (dll_handle != NULL) {
            SDL_UnloadObject(dll_handle);
        }
        path = SDL_getenv("SDL_VIDEO_EGL_DRIVER");
        if (path == NULL) {
            path = DEFAULT_EGL;
        }
        dll_handle = SDL_LoadObject(path);
        if (dll_handle == NULL) {
            return SDL_SetError("Could not load EGL library");
        }
    }

    _this->egl_data->dll_handle = dll_handle;

    /* Load new function pointers */
    LOAD_FUNC(eglGetDisplay);
    LOAD_FUNC(eglInitialize);
    LOAD_FUNC(eglTerminate);
    LOAD_FUNC(eglGetProcAddress);
    LOAD_FUNC(eglChooseConfig);
    LOAD_FUNC(eglGetConfigAttrib);
    LOAD_FUNC(eglCreateContext);
    LOAD_FUNC(eglDestroyContext);
    LOAD_FUNC(eglCreateWindowSurface);
    LOAD_FUNC(eglDestroySurface);
    LOAD_FUNC(eglMakeCurrent);
    LOAD_FUNC(eglSwapBuffers);
    LOAD_FUNC(eglSwapInterval);
    LOAD_FUNC(eglWaitNative);
    LOAD_FUNC(eglWaitGL);
    LOAD_FUNC(eglBindAPI);
    /* Functions from Vivante GPU SDK */
    LOAD_VIV_FUNC(fbGetDisplay);
    LOAD_VIV_FUNC(fbGetDisplayByIndex);
    LOAD_VIV_FUNC(fbGetDisplayGeometry);
    LOAD_VIV_FUNC(fbGetDisplayInfo);
    LOAD_VIV_FUNC(fbDestroyDisplay);
    LOAD_VIV_FUNC(fbCreateWindow);
    LOAD_VIV_FUNC(fbGetWindowGeometry);
    LOAD_VIV_FUNC(fbGetWindowInfo);
    LOAD_VIV_FUNC(fbDestroyWindow);
    LOAD_VIV_FUNC(fbCreatePixmap);
    LOAD_VIV_FUNC(fbCreatePixmapWithBpp);
    LOAD_VIV_FUNC(fbGetPixmapGeometry);
    LOAD_VIV_FUNC(fbGetPixmapInfo);
    LOAD_VIV_FUNC(fbDestroyPixmap);
   
    displaydata = SDL_GetDisplayDriverData(0);

    _this->egl_data->egl_display = _this->egl_data->eglGetDisplay(displaydata->native_display);
    if (!_this->egl_data->egl_display) {
        return SDL_SetError("Could not get EGL display");
    }
    
    if (_this->egl_data->eglInitialize(_this->egl_data->egl_display, NULL, NULL) != EGL_TRUE) {
        return SDL_SetError("Could not initialize EGL");
    }

    displaydata->egl_display = _this->egl_data->egl_display;

    _this->gl_config.driver_loaded = 1;

    if (path) {
        SDL_strlcpy(_this->gl_config.driver_path, path, sizeof(_this->gl_config.driver_path) - 1);
    } else {
        *_this->gl_config.driver_path = '\0';
    }
    
    return 0;
}

void
MX6_GLES_UnloadLibrary(_THIS)
{
    if (_this->egl_data) {
        if (_this->egl_data->egl_display) {
            _this->egl_data->eglTerminate(_this->egl_data->egl_display);
            _this->egl_data->egl_display = NULL;
        }

        if (_this->egl_data->dll_handle) {
            SDL_UnloadObject(_this->egl_data->dll_handle);
            _this->egl_data->dll_handle = NULL;
        }
        if (_this->egl_data->egl_dll_handle) {
            SDL_UnloadObject(_this->egl_data->egl_dll_handle);
            _this->egl_data->egl_dll_handle = NULL;
        }
        
        SDL_free(_this->egl_data);
        _this->egl_data = NULL;

        SDL_free(egl_viv_data);
        egl_viv_data = NULL;
    }
}

SDL_EGL_CreateContext_impl(MX6)
SDL_EGL_SwapWindow_impl(MX6)
SDL_EGL_MakeCurrent_impl(MX6)

#endif /* SDL_VIDEO_DRIVER_MX6 && SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */

