/*
 *  Simple DirectMedia Layer
 *  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>
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
#include "../SDL_internal.h"

#if SDL_VIDEO_OPENGL_EGL

#if SDL_VIDEO_DRIVER_WINDOWS || SDL_VIDEO_DRIVER_WINRT
#include "../core/windows/SDL_windows.h"
#endif

#include "SDL_sysvideo.h"
#include "SDL_egl_c.h"
#include "SDL_loadso.h"
#include "SDL_hints.h"

#ifdef EGL_KHR_create_context
/* EGL_OPENGL_ES3_BIT_KHR was added in version 13 of the extension. */
#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif
#endif /* EGL_KHR_create_context */

#if SDL_VIDEO_DRIVER_RPI
/* Raspbian places the OpenGL ES/EGL binaries in a non standard path */
#define DEFAULT_EGL "/opt/vc/lib/libEGL.so"
#define DEFAULT_OGL_ES2 "/opt/vc/lib/libGLESv2.so"
#define DEFAULT_OGL_ES_PVR "/opt/vc/lib/libGLES_CM.so"
#define DEFAULT_OGL_ES "/opt/vc/lib/libGLESv1_CM.so"

#elif SDL_VIDEO_DRIVER_ANDROID || SDL_VIDEO_DRIVER_VIVANTE
/* Android */
#define DEFAULT_EGL "libEGL.so"
#define DEFAULT_OGL_ES2 "libGLESv2.so"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.so"
#define DEFAULT_OGL_ES "libGLESv1_CM.so"

#elif SDL_VIDEO_DRIVER_WINDOWS || SDL_VIDEO_DRIVER_WINRT
/* EGL AND OpenGL ES support via ANGLE */
#define DEFAULT_EGL "libEGL.dll"
#define DEFAULT_OGL_ES2 "libGLESv2.dll"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.dll"
#define DEFAULT_OGL_ES "libGLESv1_CM.dll"

#else
/* Desktop Linux */
#define DEFAULT_OGL "libGL.so.1"
#define DEFAULT_EGL "libEGL.so.1"
#define DEFAULT_OGL_ES2 "libGLESv2.so.2"
#define DEFAULT_OGL_ES_PVR "libGLES_CM.so.1"
#define DEFAULT_OGL_ES "libGLESv1_CM.so.1"
#endif /* SDL_VIDEO_DRIVER_RPI */

#define LOAD_FUNC(NAME) \
_this->egl_data->NAME = SDL_LoadFunction(_this->egl_data->dll_handle, #NAME); \
if (!_this->egl_data->NAME) \
{ \
    return SDL_SetError("Could not retrieve EGL function " #NAME); \
}
    
/* EGL implementation of SDL OpenGL ES support */
#ifdef EGL_KHR_create_context        
static int SDL_EGL_HasExtension(_THIS, const char *ext)
{
    int i;
    int len = 0;
    int ext_len;
    const char *exts;
    const char *ext_word;

    ext_len = SDL_strlen(ext);
    exts = _this->egl_data->eglQueryString(_this->egl_data->egl_display, EGL_EXTENSIONS);

    if (exts) {
        ext_word = exts;

        for (i = 0; exts[i] != 0; i++) {
            if (exts[i] == ' ') {
                if (ext_len == len && !SDL_strncmp(ext_word, ext, len)) {
                    return 1;
                }

                len = 0;
                ext_word = &exts[i + 1];
            } else {
                len++;
            }
        }
    }

    return 0;
}
#endif /* EGL_KHR_create_context */

void *
SDL_EGL_GetProcAddress(_THIS, const char *proc)
{
    static char procname[1024];
    void *retval;
    
    /* eglGetProcAddress is busted on Android http://code.google.com/p/android/issues/detail?id=7681 */
#if !defined(SDL_VIDEO_DRIVER_ANDROID) && !defined(SDL_VIDEO_DRIVER_MIR) 
    if (_this->egl_data->eglGetProcAddress) {
        retval = _this->egl_data->eglGetProcAddress(proc);
        if (retval) {
            return retval;
        }
    }
#endif
    
    retval = SDL_LoadFunction(_this->egl_data->egl_dll_handle, proc);
    if (!retval && SDL_strlen(proc) <= 1022) {
        procname[0] = '_';
        SDL_strlcpy(procname + 1, proc, 1022);
        retval = SDL_LoadFunction(_this->egl_data->egl_dll_handle, procname);
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
    }
}

int
SDL_EGL_LoadLibrary(_THIS, const char *egl_path, NativeDisplayType native_display)
{
    void *dll_handle = NULL, *egl_dll_handle = NULL; /* The naming is counter intuitive, but hey, I just work here -- Gabriel */
    char *path = NULL;
#if SDL_VIDEO_DRIVER_WINDOWS || SDL_VIDEO_DRIVER_WINRT
    const char *d3dcompiler;
#endif

    if (_this->egl_data) {
        return SDL_SetError("OpenGL ES context already created");
    }

    _this->egl_data = (struct SDL_EGL_VideoData *) SDL_calloc(1, sizeof(SDL_EGL_VideoData));
    if (!_this->egl_data) {
        return SDL_OutOfMemory();
    }

#if SDL_VIDEO_DRIVER_WINDOWS || SDL_VIDEO_DRIVER_WINRT
    d3dcompiler = SDL_GetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER);
    if (!d3dcompiler) {
        if (WIN_IsWindowsVistaOrGreater()) {
            d3dcompiler = "d3dcompiler_46.dll";
        } else {
            d3dcompiler = "d3dcompiler_43.dll";
        }
    }
    if (SDL_strcasecmp(d3dcompiler, "none") != 0) {
        SDL_LoadObject(d3dcompiler);
    }
#endif

    /* A funny thing, loading EGL.so first does not work on the Raspberry, so we load libGL* first */
    path = SDL_getenv("SDL_VIDEO_GL_DRIVER");
    if (path != NULL) {
        egl_dll_handle = SDL_LoadObject(path);
    }

    if (egl_dll_handle == NULL) {
        if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
            if (_this->gl_config.major_version > 1) {
                path = DEFAULT_OGL_ES2;
                egl_dll_handle = SDL_LoadObject(path);
            } else {
                path = DEFAULT_OGL_ES;
                egl_dll_handle = SDL_LoadObject(path);
                if (egl_dll_handle == NULL) {
                    path = DEFAULT_OGL_ES_PVR;
                    egl_dll_handle = SDL_LoadObject(path);
                }
            }
        }
#ifdef DEFAULT_OGL         
        else {
            path = DEFAULT_OGL;
            egl_dll_handle = SDL_LoadObject(path);
        }
#endif        
    }
    _this->egl_data->egl_dll_handle = egl_dll_handle;

    if (egl_dll_handle == NULL) {
        return SDL_SetError("Could not initialize OpenGL / GLES library");
    }

    /* Loading libGL* in the previous step took care of loading libEGL.so, but we future proof by double checking */
    if (egl_path != NULL) {
        dll_handle = SDL_LoadObject(egl_path);
    }   
    /* Try loading a EGL symbol, if it does not work try the default library paths */
    if (dll_handle == NULL || SDL_LoadFunction(dll_handle, "eglChooseConfig") == NULL) {
        if (dll_handle != NULL) {
            SDL_UnloadObject(dll_handle);
        }
        path = SDL_getenv("SDL_VIDEO_EGL_DRIVER");
        if (path == NULL) {
            path = DEFAULT_EGL;
        }
        dll_handle = SDL_LoadObject(path);
        if (dll_handle == NULL || SDL_LoadFunction(dll_handle, "eglChooseConfig") == NULL) {
            if (dll_handle != NULL) {
                SDL_UnloadObject(dll_handle);
            }
            return SDL_SetError("Could not load EGL library");
        }
        SDL_ClearError();
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
    LOAD_FUNC(eglQueryString);
    
#if !defined(__WINRT__)
    _this->egl_data->egl_display = _this->egl_data->eglGetDisplay(native_display);
    if (!_this->egl_data->egl_display) {
        return SDL_SetError("Could not get EGL display");
    }
    
    if (_this->egl_data->eglInitialize(_this->egl_data->egl_display, NULL, NULL) != EGL_TRUE) {
        return SDL_SetError("Could not initialize EGL");
    }
#endif

    _this->gl_config.driver_loaded = 1;

    if (path) {
        SDL_strlcpy(_this->gl_config.driver_path, path, sizeof(_this->gl_config.driver_path) - 1);
    } else {
        *_this->gl_config.driver_path = '\0';
    }
    
    return 0;
}

int
SDL_EGL_ChooseConfig(_THIS) 
{
    /* 64 seems nice. */
    EGLint attribs[64];
    EGLint found_configs = 0, value;
    /* 128 seems even nicer here */
    EGLConfig configs[128];
    int i, j, best_bitdiff = -1, bitdiff;
    
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
    if (_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) {
#ifdef EGL_KHR_create_context
        if (_this->gl_config.major_version >= 3 &&
            SDL_EGL_HasExtension(_this, "EGL_KHR_create_context")) {
            attribs[i++] = EGL_OPENGL_ES3_BIT_KHR;
        } else
#endif
        if (_this->gl_config.major_version >= 2) {
            attribs[i++] = EGL_OPENGL_ES2_BIT;
        } else {
            attribs[i++] = EGL_OPENGL_ES_BIT;
        }
        _this->egl_data->eglBindAPI(EGL_OPENGL_ES_API);
    } else {
        attribs[i++] = EGL_OPENGL_BIT;
        _this->egl_data->eglBindAPI(EGL_OPENGL_API);
    }
    
    attribs[i++] = EGL_NONE;
   
    if (_this->egl_data->eglChooseConfig(_this->egl_data->egl_display,
        attribs,
        configs, SDL_arraysize(configs),
        &found_configs) == EGL_FALSE ||
        found_configs == 0) {
        return SDL_SetError("Couldn't find matching EGL config");
    }
    
    /* eglChooseConfig returns a number of configurations that match or exceed the requested attribs. */
    /* From those, we select the one that matches our requirements more closely via a makeshift algorithm */

    for (i = 0; i < found_configs; i++ ) {
        bitdiff = 0;
        for (j = 0; j < SDL_arraysize(attribs) - 1; j += 2) {
            if (attribs[j] == EGL_NONE) {
               break;
            }
            
            if ( attribs[j+1] != EGL_DONT_CARE && (
                attribs[j] == EGL_RED_SIZE ||
                attribs[j] == EGL_GREEN_SIZE ||
                attribs[j] == EGL_BLUE_SIZE ||
                attribs[j] == EGL_ALPHA_SIZE ||
                attribs[j] == EGL_DEPTH_SIZE ||
                attribs[j] == EGL_STENCIL_SIZE)) {
                _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display, configs[i], attribs[j], &value);
                bitdiff += value - attribs[j + 1]; /* value is always >= attrib */
            }
        }

        if (bitdiff < best_bitdiff || best_bitdiff == -1) {
            _this->egl_data->egl_config = configs[i];
            
            best_bitdiff = bitdiff;
        }

        if (bitdiff == 0) {
            break; /* we found an exact match! */
        }
    }
    
    return 0;
}

SDL_GLContext
SDL_EGL_CreateContext(_THIS, EGLSurface egl_surface)
{
    /* max 14 values plus terminator. */
    EGLint attribs[15];
    int attr = 0;

    EGLContext egl_context, share_context = EGL_NO_CONTEXT;
    EGLint profile_mask = _this->gl_config.profile_mask;
    EGLint major_version = _this->gl_config.major_version;
    EGLint minor_version = _this->gl_config.minor_version;
    SDL_bool profile_es = (profile_mask == SDL_GL_CONTEXT_PROFILE_ES);

    if (!_this->egl_data) {
        /* The EGL library wasn't loaded, SDL_GetError() should have info */
        return NULL;
    }

    if (_this->gl_config.share_with_current_context) {
        share_context = (EGLContext)SDL_GL_GetCurrentContext();
    }

    /* Set the context version and other attributes. */
    if ((major_version < 3 || (minor_version == 0 && profile_es)) &&
        _this->gl_config.flags == 0 &&
        (profile_mask == 0 || profile_es)) {
        /* Create a context without using EGL_KHR_create_context attribs.
         * When creating a GLES context without EGL_KHR_create_context we can
         * only specify the major version. When creating a desktop GL context
         * we can't specify any version, so we only try in that case when the
         * version is less than 3.0 (matches SDL's GLX/WGL behavior.)
         */
        if (profile_es) {
            attribs[attr++] = EGL_CONTEXT_CLIENT_VERSION;
            attribs[attr++] = SDL_max(major_version, 1);
        }
    } else {
#ifdef EGL_KHR_create_context
        /* The Major/minor version, context profiles, and context flags can
         * only be specified when this extension is available.
         */
        if (SDL_EGL_HasExtension(_this, "EGL_KHR_create_context")) {
            attribs[attr++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
            attribs[attr++] = major_version;
            attribs[attr++] = EGL_CONTEXT_MINOR_VERSION_KHR;
            attribs[attr++] = minor_version;

            /* SDL profile bits match EGL profile bits. */
            if (profile_mask != 0 && profile_mask != SDL_GL_CONTEXT_PROFILE_ES) {
                attribs[attr++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
                attribs[attr++] = profile_mask;
            }

            /* SDL flags match EGL flags. */
            if (_this->gl_config.flags != 0) {
                attribs[attr++] = EGL_CONTEXT_FLAGS_KHR;
                attribs[attr++] = _this->gl_config.flags;
            }
        } else
#endif /* EGL_KHR_create_context */
        {
            SDL_SetError("Could not create EGL context (context attributes are not supported)");
            return NULL;
        }
    }

    attribs[attr++] = EGL_NONE;

    /* Bind the API */
    if (profile_es) {
        _this->egl_data->eglBindAPI(EGL_OPENGL_ES_API);
    } else {
        _this->egl_data->eglBindAPI(EGL_OPENGL_API);
    }

    egl_context = _this->egl_data->eglCreateContext(_this->egl_data->egl_display,
                                      _this->egl_data->egl_config,
                                      share_context, attribs);

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
    } else {
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
    
    if (egl_context != NULL && egl_context != EGL_NO_CONTEXT) {
        SDL_EGL_MakeCurrent(_this, NULL, NULL);
        _this->egl_data->eglDestroyContext(_this->egl_data->egl_display, egl_context);
    }
        
}

EGLSurface *
SDL_EGL_CreateSurface(_THIS, NativeWindowType nw) 
{
    if (SDL_EGL_ChooseConfig(_this) != 0) {
        return EGL_NO_SURFACE;
    }
    
#if __ANDROID__
    {
        /* Android docs recommend doing this!
         * Ref: http://developer.android.com/reference/android/app/NativeActivity.html 
         */
        EGLint format;
        _this->egl_data->eglGetConfigAttrib(_this->egl_data->egl_display,
                                            _this->egl_data->egl_config, 
                                            EGL_NATIVE_VISUAL_ID, &format);

        ANativeWindow_setBuffersGeometry(nw, 0, 0, format);
    }
#endif    
    
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
    
