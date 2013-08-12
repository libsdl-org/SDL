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

#if SDL_VIDEO_DRIVER_X11 && SDL_VIDEO_OPENGL_ES

#include "SDL_x11video.h"
#include "SDL_x11opengles.h"
#include "SDL_x11opengl.h"

#define DEFAULT_EGL "libEGL.so"
#define DEFAULT_OGL_ES2 "libGLESv2.so"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.so"
#define DEFAULT_OGL_ES "libGLESv1_CM.so"

#define LOAD_FUNC(NAME) \
    *((void**)&_this->gles_data->NAME) = dlsym(handle, #NAME); \
    if (!_this->gles_data->NAME) \
    { \
        return SDL_SetError("Could not retrieve EGL function " #NAME); \
    }

/* GLES implementation of SDL OpenGL support */

void *
X11_GLES_GetProcAddress(_THIS, const char *proc)
{
    static char procname[1024];
    void *handle;
    void *retval;

    handle = _this->gles_data->egl_dll_handle;
    if (_this->gles_data->eglGetProcAddress) {
        retval = _this->gles_data->eglGetProcAddress(proc);
        if (retval) {
            return retval;
        }
    }

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
X11_GLES_UnloadLibrary(_THIS)
{
    if ((_this->gles_data) && (_this->gl_config.driver_loaded)) {
        _this->gles_data->eglTerminate(_this->gles_data->egl_display);

        dlclose(_this->gl_config.dll_handle);
        dlclose(_this->gles_data->egl_dll_handle);

        SDL_free(_this->gles_data);
        _this->gles_data = NULL;

        _this->gl_config.dll_handle = NULL;
        _this->gl_config.driver_loaded = 0;
    }
}

int
X11_GLES_LoadLibrary(_THIS, const char *path)
{
    void *handle;
    int dlopen_flags;

    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (_this->gles_data) {
        return SDL_SetError("OpenGL ES context already created");
    }

    /* If SDL_GL_CONTEXT_EGL has been changed to 0, switch over to X11_GL functions  */
    if (_this->gl_config.use_egl == 0) {
#if SDL_VIDEO_OPENGL_GLX
        _this->GL_LoadLibrary = X11_GL_LoadLibrary;
        _this->GL_GetProcAddress = X11_GL_GetProcAddress;
        _this->GL_UnloadLibrary = X11_GL_UnloadLibrary;
        _this->GL_CreateContext = X11_GL_CreateContext;
        _this->GL_MakeCurrent = X11_GL_MakeCurrent;
        _this->GL_SetSwapInterval = X11_GL_SetSwapInterval;
        _this->GL_GetSwapInterval = X11_GL_GetSwapInterval;
        _this->GL_SwapWindow = X11_GL_SwapWindow;
        _this->GL_DeleteContext = X11_GL_DeleteContext;
        return X11_GL_LoadLibrary(_this, path);
#else
        return SDL_SetError("SDL not configured with OpenGL/GLX support");
#endif
    }

#ifdef RTLD_GLOBAL
    dlopen_flags = RTLD_LAZY | RTLD_GLOBAL;
#else
    dlopen_flags = RTLD_LAZY;
#endif
    handle = dlopen(path, dlopen_flags);
    /* Catch the case where the application isn't linked with EGL */
    if ((dlsym(handle, "eglChooseConfig") == NULL) && (path == NULL)) {

        dlclose(handle);
        path = getenv("SDL_VIDEO_EGL_DRIVER");
        if (path == NULL) {
            path = DEFAULT_EGL;
        }
        handle = dlopen(path, dlopen_flags);
    }

    if (handle == NULL) {
        return SDL_SetError("Could not load OpenGL ES/EGL library");
    }

    /* Unload the old driver and reset the pointers */
    X11_GLES_UnloadLibrary(_this);

    _this->gles_data = (struct SDL_PrivateGLESData *) SDL_calloc(1, sizeof(SDL_PrivateGLESData));
    if (!_this->gles_data) {
        return SDL_OutOfMemory();
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

    _this->gles_data->egl_display =
        _this->gles_data->eglGetDisplay((NativeDisplayType) data->display);

    if (!_this->gles_data->egl_display) {
        return SDL_SetError("Could not get EGL display");
    }

    if (_this->gles_data->
        eglInitialize(_this->gles_data->egl_display, NULL,
                      NULL) != EGL_TRUE) {
        return SDL_SetError("Could not initialize EGL");
    }

    _this->gles_data->egl_dll_handle = handle;

    path = getenv("SDL_VIDEO_GL_DRIVER");
    handle = dlopen(path, dlopen_flags);
    if ((path == NULL) | (handle == NULL)) {
        if (_this->gl_config.major_version > 1) {
            path = DEFAULT_OGL_ES2;
            handle = dlopen(path, dlopen_flags);
        } else {
            path = DEFAULT_OGL_ES;
            handle = dlopen(path, dlopen_flags);
            if (handle == NULL) {
                path = DEFAULT_OGL_ES_PVR;
                handle = dlopen(path, dlopen_flags);
            }
        }
    }

    if (handle == NULL) {
        return SDL_SetError("Could not initialize OpenGL ES library");
    }

    _this->gl_config.dll_handle = handle;
    _this->gl_config.driver_loaded = 1;

    if (path) {
        strncpy(_this->gl_config.driver_path, path,
                sizeof(_this->gl_config.driver_path) - 1);
    } else {
        strcpy(_this->gl_config.driver_path, "");
    }
    return 0;
}

XVisualInfo *
X11_GLES_GetVisual(_THIS, Display * display, int screen)
{
    /* 64 seems nice. */
    EGLint attribs[64];
    EGLint found_configs = 0;
    EGLint visual_id;
    int i;

    if (!_this->gles_data) {
        /* The EGL library wasn't loaded, SDL_GetError() should have info */
        return NULL;
    }

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

    if (_this->gles_data->eglChooseConfig(_this->gles_data->egl_display,
                                          attribs,
                                          &_this->gles_data->egl_config, 1,
                                          &found_configs) == EGL_FALSE ||
        found_configs == 0) {
        SDL_SetError("Couldn't find matching EGL config");
        return NULL;
    }

    if (_this->gles_data->eglGetConfigAttrib(_this->gles_data->egl_display,
                                             _this->gles_data->egl_config,
                                             EGL_NATIVE_VISUAL_ID,
                                             &visual_id) ==
        EGL_FALSE || !visual_id) {
        /* Use the default visual when all else fails */
        XVisualInfo vi_in;
        int out_count;
        vi_in.screen = screen;

        _this->gles_data->egl_visualinfo = XGetVisualInfo(display,
                                                          VisualScreenMask,
                                                          &vi_in, &out_count);
    } else {
        XVisualInfo vi_in;
        int out_count;

        vi_in.screen = screen;
        vi_in.visualid = visual_id;
        _this->gles_data->egl_visualinfo = XGetVisualInfo(display,
                                                          VisualScreenMask |
                                                          VisualIDMask,
                                                          &vi_in, &out_count);
    }

    return _this->gles_data->egl_visualinfo;
}

SDL_GLContext
X11_GLES_CreateContext(_THIS, SDL_Window * window)
{
    EGLint context_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        1,
        EGL_NONE
    };

    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
    SDL_GLContext context = (SDL_GLContext)1;

    XSync(display, False);

    if (_this->gl_config.major_version) {
        context_attrib_list[1] = _this->gl_config.major_version;
    }

    _this->gles_data->egl_context =
        _this->gles_data->eglCreateContext(_this->gles_data->egl_display,
                                           _this->gles_data->egl_config,
                                           EGL_NO_CONTEXT, context_attrib_list);
    XSync(display, False);

    if (_this->gles_data->egl_context == EGL_NO_CONTEXT) {
        SDL_SetError("Could not create EGL context");
        return NULL;
    }

    _this->gles_data->egl_swapinterval = 0;

    if (X11_GLES_MakeCurrent(_this, window, context) < 0) {
        X11_GLES_DeleteContext(_this, context);
        return NULL;
    }

    return context;
}

int
X11_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context)
{
/*
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;
*/

    if (!_this->gles_data) {
        return SDL_SetError("OpenGL not initialized");
    }

    if (!_this->gles_data->eglMakeCurrent(_this->gles_data->egl_display,
                                          _this->gles_data->egl_surface,
                                          _this->gles_data->egl_surface,
                                          _this->gles_data->egl_context)) {
        return SDL_SetError("Unable to make EGL context current");
    }

/*
    XSync(display, False);
*/

    return 1;
}

int
X11_GLES_SetSwapInterval(_THIS, int interval)
{
    if (_this->gles_data) {
        return SDL_SetError("OpenGL ES context not active");
    }

    EGLBoolean status;
    status = _this->gles_data->eglSwapInterval(_this->gles_data->egl_display, interval);
    if (status == EGL_TRUE) {
        _this->gles_data->egl_swapinterval = interval;
        return 0;
    }

    return SDL_SetError("Unable to set the EGL swap interval");
}

int
X11_GLES_GetSwapInterval(_THIS)
{
    if (_this->gles_data) {
        return SDL_SetError("OpenGL ES context not active");
    }

    return _this->gles_data->egl_swapinterval;
}

void
X11_GLES_SwapWindow(_THIS, SDL_Window * window)
{
    _this->gles_data->eglSwapBuffers(_this->gles_data->egl_display,
                                     _this->gles_data->egl_surface);
}

void
X11_GLES_DeleteContext(_THIS, SDL_GLContext context)
{
    /* Clean up GLES and EGL */
    if (!_this->gles_data) {
        return;
    }

    if (_this->gles_data->egl_context != EGL_NO_CONTEXT ||
        _this->gles_data->egl_surface != EGL_NO_SURFACE) {
        _this->gles_data->eglMakeCurrent(_this->gles_data->egl_display,
                                         EGL_NO_SURFACE, EGL_NO_SURFACE,
                                         EGL_NO_CONTEXT);

        if (_this->gles_data->egl_context != EGL_NO_CONTEXT) {
            _this->gles_data->eglDestroyContext(_this->gles_data->egl_display,
                                                _this->gles_data->
                                                egl_context);
            _this->gles_data->egl_context = EGL_NO_CONTEXT;
        }

        if (_this->gles_data->egl_surface != EGL_NO_SURFACE) {
            _this->gles_data->eglDestroySurface(_this->gles_data->egl_display,
                                                _this->gles_data->
                                                egl_surface);
            _this->gles_data->egl_surface = EGL_NO_SURFACE;
        }
    }

    /* crappy fix */
    X11_GLES_UnloadLibrary(_this);
}

#endif /* SDL_VIDEO_DRIVER_X11 && SDL_VIDEO_OPENGL_ES */

/* vi: set ts=4 sw=4 expandtab: */
