/*
 *  Simple DirectMedia Layer
 *  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>
 * 
 *  This software is provided 'as-is', without any express or implied
 *  warranty.  In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 * 
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 * 
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 */
#include "SDL_config.h"

#if SDL_VIDEO_OPENGL_EGL

#include "SDL_sysvideo.h"
#include "SDL_egl.h"


#if SDL_VIDEO_DRIVER_RPI
/* Raspbian places the OpenGL ES/EGL binaries in a non standard path */
#define DEFAULT_EGL "/opt/vc/lib/libEGL.so"
#define DEFAULT_OGL_ES2 "/opt/vc/lib/libGLESv2.so"
#define DEFAULT_OGL_ES_PVR "/opt/vc/lib/libGLES_CM.so"
#define DEFAULT_OGL_ES "/opt/vc/lib/libGLESv1_CM.so"

#elif SDL_VIDEO_DRIVER_ANDROID
/* Android */
#define DEFAULT_EGL "libEGL.so"
#define DEFAULT_OGL_ES2 "libGLESv2.so"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.so"
#define DEFAULT_OGL_ES "libGLESv1_CM.so"

#else
/* Desktop Linux */
#define DEFAULT_EGL "libEGL.so.1"
#define DEFAULT_OGL_ES2 "libGLESv2.so.2"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.so.1"
#define DEFAULT_OGL_ES "libGLESv1_CM.so.1"
#endif /* SDL_VIDEO_DRIVER_RPI */

#define LOAD_FUNC(NAME) \
*((void**)&_this->egl_data->NAME) = dlsym(dll_handle, #NAME); \
if (!_this->egl_data->NAME) \
{ \
    return SDL_SetError("Could not retrieve EGL function " #NAME); \
}
    
/* EGL implementation of SDL OpenGL ES support */

void *
SDL_EGL_GetProcAddress(_THIS, const char *proc)
{
    static char procname[1024];
    void *handle;
    void *retval;
    
    /* eglGetProcAddress is busted on Android http://code.google.com/p/android/issues/detail?id=7681 */
#if !defined(SDL_VIDEO_DRIVER_ANDROID)
    handle = _this->egl_data->egl_dll_handle;
    if (_this->egl_data->eglGetProcAddress) {
        retval = _this->egl_data->eglGetProcAddress(proc);
        if (retval) {
            return retval;
        }
    }
#endif
    
    handle = _this->gl_config.dll_handle;
    #if defined(__OpenBSD__) && !defined(__ELF__)
    #undef dlsym(x,y);
    #endif
    retval = dlsym(handle, proc);
    if (!retval && strlen(proc) <= 1022) {
        procname[0] = '_';
        strcpy(procname + 1, proc);
        retval = dlsym(handle, procname);
    }
    return retval;
}

void
SDL_EGL_UnloadLibrary(_THIS)
{
    if (_this->egl_data) {
        if (_this->egl_data->egl_display) {
            _this->egl_data->eglTerminate(_this->egl_data->egl_display);
            _this->egl_data->egl_display = NULL;
        }

        if (_this->gl_config.dll_handle) {
            dlclose(_this->gl_config.dll_handle);
            _this->gl_config.dll_handle = NULL;
        }
        if (_this->egl_data->egl_dll_handle) {
            dlclose(_this->egl_data->egl_dll_handle);
            _this->egl_data->egl_dll_handle = NULL;
        }
        
        SDL_free(_this->egl_data);
        _this->egl_data = NULL;
    }
}

int
SDL_EGL_LoadLibrary(_THIS, const char *egl_path, NativeDisplayType native_display)
{
    void *dll_handle, *egl_dll_handle; /* The naming is counter intuitive, but hey, I just work here -- Gabriel */
    char *path;
    int dlopen_flags;
    
    if (_this->egl_data) {
        return SDL_SetError("OpenGL ES context already created");
    }

    _this->egl_data = (struct SDL_EGL_VideoData *) SDL_calloc(1, sizeof(SDL_EGL_VideoData));
    if (!_this->egl_data) {
        return SDL_OutOfMemory();
    }

#ifdef RTLD_GLOBAL
    dlopen_flags = RTLD_LAZY | RTLD_GLOBAL;
#else
    dlopen_flags = RTLD_LAZY;
#endif

    /* A funny thing, loading EGL.so first does not work on the Raspberry, so we load libGL* first */
    path = getenv("SDL_VIDEO_GL_DRIVER");
    egl_dll_handle = dlopen(path, dlopen_flags);
    if ((path == NULL) | (egl_dll_handle == NULL)) {
        if (_this->gl_config.major_version > 1) {
            path = DEFAULT_OGL_ES2;
            egl_dll_handle = dlopen(path, dlopen_flags);
        } else {
            path = DEFAULT_OGL_ES;
            egl_dll_handle = dlopen(path, dlopen_flags);
            if (egl_dll_handle == NULL) {
                path = DEFAULT_OGL_ES_PVR;
                egl_dll_handle = dlopen(path, dlopen_flags);
            }
        }
    }
    _this->egl_data->egl_dll_handle = egl_dll_handle;

    if (egl_dll_handle == NULL) {
        return SDL_SetError("Could not initialize OpenGL ES library: %s", dlerror());
    }
    
    /* Loading libGL* in the previous step took care of loading libEGL.so, but we future proof by double checking */
    dll_handle = dlopen(egl_path, dlopen_flags);
    /* Catch the case where the application isn't linked with EGL */
    if ((dlsym(dll_handle, "eglChooseConfig") == NULL) && (egl_path == NULL)) {
        dlclose(dll_handle);
        path = getenv("SDL_VIDEO_EGL_DRIVER");
        if (path == NULL) {
            path = DEFAULT_EGL;
        }
        dll_handle = dlopen(path, dlopen_flags);
    }
    _this->gl_config.dll_handle = dll_handle;

    if (dll_handle == NULL) {
        return SDL_SetError("Could not load EGL library: %s", dlerror());
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
    LOAD_FUNC(eglCreateWindowSurface);
    LOAD_FUNC(eglDestroySurface);
    LOAD_FUNC(eglMakeCurrent);
    LOAD_FUNC(eglSwapBuffers);
    LOAD_FUNC(eglSwapInterval);
    LOAD_FUNC(eglWaitNative);
    LOAD_FUNC(eglWaitGL);
    
    _this->egl_data->egl_display = _this->egl_data->eglGetDisplay(native_display);
    if (!_this->egl_data->egl_display) {
        return SDL_SetError("Could not get EGL display");
    }
    
    if (_this->egl_data->eglInitialize(_this->egl_data->egl_display, NULL, NULL) != EGL_TRUE) {
        return SDL_SetError("Could not initialize EGL");
    }

    _this->gl_config.dll_handle = dll_handle;
    _this->egl_data->egl_dll_handle = egl_dll_handle;
    _this->gl_config.driver_loaded = 1;

    if (path) {
        strncpy(_this->gl_config.driver_path, path, sizeof(_this->gl_config.driver_path) - 1);
    } else {
        strcpy(_this->gl_config.driver_path, "");
    }
    
    /* We need to select a config here to satisfy some video backends such as X11 */
    SDL_EGL_ChooseConfig(_this);
    
    return 0;
}

int
SDL_EGL_ChooseConfig(_THIS) 
{
    /* 64 seems nice. */
    EGLint attribs[64];
    EGLint found_configs = 0;
    int i;
    
    if (!_this->egl_data) {
        /* The EGL library wasn't loaded, SDL_GetError() should have info */
        return -1;
    }
  
    /* Get a valid EGL configuration */
    i = 0;
    attribs[i++] = EGL_RED_SIZE;
    attribs[i++] = _this->gl_config.red_size;
    attribs[i++] = EGL_GREEN_SIZE;
    attribs[i++] = _this->gl_config.green_size;
    attribs[i++] = EGL_BLUE_SIZE;
    attribs[i++] = _this->gl_config.blue_size;
    
    if (_this->gl_config.alpha_size) {
        attribs[i++] = EGL_ALPHA_SIZE;
        attribs[i++] = _this->gl_config.alpha_size;
    }
    
    if (_this->gl_config.buffer_size) {
        attribs[i++] = EGL_BUFFER_SIZE;
        attribs[i++] = _this->gl_config.buffer_size;
    }
    
    attribs[i++] = EGL_DEPTH_SIZE;
    attribs[i++] = _this->gl_config.depth_size;
    
    if (_this->gl_config.stencil_size) {
        attribs[i++] = EGL_STENCIL_SIZE;
        attribs[i++] = _this->gl_config.stencil_size;
    }
    
    if (_this->gl_config.multisamplebuffers) {
        attribs[i++] = EGL_SAMPLE_BUFFERS;
        attribs[i++] = _this->gl_config.multisamplebuffers;
    }
    
    if (_this->gl_config.multisamplesamples) {
        attribs[i++] = EGL_SAMPLES;
        attribs[i++] = _this->gl_config.multisamplesamples;
    }
    
    attribs[i++] = EGL_RENDERABLE_TYPE;
    if (_this->gl_config.major_version == 2) {
        attribs[i++] = EGL_OPENGL_ES2_BIT;
    } else {
        attribs[i++] = EGL_OPENGL_ES_BIT;
    }
    
    attribs[i++] = EGL_NONE;
    
    if (_this->egl_data->eglChooseConfig(_this->egl_data->egl_display,
        attribs,
        &_this->egl_data->egl_config, 1,
        &found_configs) == EGL_FALSE ||
        found_configs == 0) {
        return SDL_SetError("Couldn't find matching EGL config");
    }
    
    return 0;
}

SDL_GLContext
SDL_EGL_CreateContext(_THIS, EGLSurface egl_surface)
{
    EGLint context_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        1,
        EGL_NONE
    };
    
    EGLContext egl_context;
    
    if (!_this->egl_data) {
        /* The EGL library wasn't loaded, SDL_GetError() should have info */
        return NULL;
    }
    
    if (_this->gl_config.major_version) {
        context_attrib_list[1] = _this->gl_config.major_version;
    }

    egl_context =
    _this->egl_data->eglCreateContext(_this->egl_data->egl_display,
                                      _this->egl_data->egl_config,
                                      EGL_NO_CONTEXT, context_attrib_list);
    
    if (egl_context == EGL_NO_CONTEXT) {
        SDL_SetError("Could not create EGL context");
        return NULL;
    }
    
    _this->egl_data->egl_swapinterval = 0;
    
    if (SDL_EGL_MakeCurrent(_this, egl_surface, egl_context) < 0) {
        SDL_EGL_DeleteContext(_this, egl_context);
        SDL_SetError("Could not make EGL context current");
        return NULL;
    }
  
    return (SDL_GLContext) egl_context;
}

int
SDL_EGL_MakeCurrent(_THIS, EGLSurface egl_surface, SDL_GLContext context)
{
    EGLContext egl_context = (EGLContext) context;

    if (!_this->egl_data) {
        return SDL_SetError("OpenGL not initialized");
    }
    
    /* The android emulator crashes badly if you try to eglMakeCurrent 
     * with a valid context and invalid surface, so we have to check for both here.
     */
    if (!egl_context || !egl_surface) {
         _this->egl_data->eglMakeCurrent(_this->egl_data->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    else {
        if (!_this->egl_data->eglMakeCurrent(_this->egl_data->egl_display,
            egl_surface, egl_surface, egl_context)) {
            return SDL_SetError("Unable to make EGL context current");
        }
    }
      
    return 0;
}

int
SDL_EGL_SetSwapInterval(_THIS, int interval)
{
    EGLBoolean status;
    
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }
    
    status = _this->egl_data->eglSwapInterval(_this->egl_data->egl_display, interval);
    if (status == EGL_TRUE) {
        _this->egl_data->egl_swapinterval = interval;
        return 0;
    }
    
    return SDL_SetError("Unable to set the EGL swap interval");
}

int
SDL_EGL_GetSwapInterval(_THIS)
{
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }
    
    return _this->egl_data->egl_swapinterval;
}

void
SDL_EGL_SwapBuffers(_THIS, EGLSurface egl_surface)
{
    _this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, egl_surface);
}

void
SDL_EGL_DeleteContext(_THIS, SDL_GLContext context)
{
    EGLContext egl_context = (EGLContext) context;

    /* Clean up GLES and EGL */
    if (!_this->egl_data) {
        return;
    }
    
    if (!egl_context && egl_context != EGL_NO_CONTEXT) {
        SDL_EGL_MakeCurrent(_this, NULL, NULL);
        _this->egl_data->eglDestroyContext(_this->egl_data->egl_display, egl_context);
    }
        
    /* FIXME: This "crappy fix" comes from the X11 code, 
     * it's required so you can create a GLX context, destroy it and create a EGL one */
    SDL_EGL_UnloadLibrary(_this);
}

EGLSurface *
SDL_EGL_CreateSurface(_THIS, NativeWindowType nw) 
{
    return _this->egl_data->eglCreateWindowSurface(
            _this->egl_data->egl_display,
            _this->egl_data->egl_config,
            nw, NULL);
}

void
SDL_EGL_DestroySurface(_THIS, EGLSurface egl_surface) 
{
    if (!_this->egl_data) {
        return;
    }
    
    if (egl_surface != EGL_NO_SURFACE) {
        _this->egl_data->eglDestroySurface(_this->egl_data->egl_display, egl_surface);
    }
}

#endif /* SDL_VIDEO_OPENGL_EGL */

/* vi: set ts=4 sw=4 expandtab: */
    
