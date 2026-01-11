/*
  Simple DirectMedia Layer
  Copyright (C) 2025 BlackBerry Limited

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
#include "SDL_qnx.h"
#include "SDL_qnxscreenconsts.h"

static EGLDisplay   egl_disp;

int                 screen_format = SCREEN_FORMAT_RGBX8888;
SDL_PixelFormat     pixel_format = SDL_PIXELFORMAT_RGBX8888;

struct DummyConfig
{
    int red_size;
    int green_size;
    int blue_size;
    int alpha_size;
    int native_id;
};

static struct DummyConfig getDummyConfigFromScreenSettings(int format)
{
    int rc;
    struct DummyConfig dummyConfig= {};

    dummyConfig.native_id = format;
    switch (format) {
         case SCREEN_FORMAT_RGBX4444:
            dummyConfig.red_size = 4;
            dummyConfig.green_size = 4;
            dummyConfig.blue_size = 4;
            dummyConfig.alpha_size = 4;
            break;
         case SCREEN_FORMAT_RGBA5551:
            dummyConfig.red_size = 5;
            dummyConfig.green_size = 5;
            dummyConfig.blue_size = 5;
            dummyConfig.alpha_size = 1;
            break;
         case SCREEN_FORMAT_RGB565:
            dummyConfig.red_size = 5;
            dummyConfig.green_size = 6;
            dummyConfig.blue_size = 5;
            dummyConfig.alpha_size = 0;
            break;
         case SCREEN_FORMAT_RGB888:
            dummyConfig.red_size = 8;
            dummyConfig.green_size = 8;
            dummyConfig.blue_size = 8;
            dummyConfig.alpha_size = 0;
            break;
            case SCREEN_FORMAT_BGRA8888:
            case SCREEN_FORMAT_BGRX8888:
            case SCREEN_FORMAT_RGBA8888:
         case SCREEN_FORMAT_RGBX8888:
            dummyConfig.red_size = 8;
            dummyConfig.green_size = 8;
            dummyConfig.blue_size = 8;
            dummyConfig.alpha_size = 8;
            break;
            default:
                break;
    }
    return dummyConfig;
}

static EGLConfig chooseConfig(struct DummyConfig dummyConfig, EGLConfig* egl_configs, EGLint egl_num_configs)
{
   EGLConfig glConfig = (EGLConfig)0;

    for (size_t ii = 0; ii < egl_num_configs; ii++) {
        EGLint val;

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_SURFACE_TYPE, &val);
        if (!(val & EGL_WINDOW_BIT)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_RENDERABLE_TYPE, &val);
        if (!(val & EGL_OPENGL_ES2_BIT)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_DEPTH_SIZE, &val);
        if (val == 0) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_RED_SIZE, &val);
        if (val != dummyConfig.red_size) {
           continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_GREEN_SIZE, &val);
        if (val != dummyConfig.green_size) {
           continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_BLUE_SIZE, &val);
        if (val != dummyConfig.blue_size) {
           continue;
        }


        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_ALPHA_SIZE, &val);
        if (val != dummyConfig.alpha_size) {
            continue;
        }
        if(!glConfig)
        {
            glConfig = egl_configs[ii];
        }

        eglGetConfigAttrib(egl_disp, egl_configs[ii], EGL_NATIVE_VISUAL_ID, &val);
        if ((val != 0) && (val == dummyConfig.native_id)) {
            return egl_configs[ii];
        }
    }
    return glConfig;
}

/**
 * Detertmines the pixel format to use based on the current display and EGL
 * configuration.
 * @param   egl_conf    EGL configuration to use
 * @return  A SCREEN_FORMAT* constant for the pixel format to use
 */
static int chooseFormat(EGLConfig egl_conf)
{
    EGLint buffer_bit_depth;
    EGLint alpha_bit_depth;

    eglGetConfigAttrib(egl_disp, egl_conf, EGL_BUFFER_SIZE, &buffer_bit_depth);
    eglGetConfigAttrib(egl_disp, egl_conf, EGL_ALPHA_SIZE, &alpha_bit_depth);

    switch (buffer_bit_depth) {
        case 32:
            return SCREEN_FORMAT_RGBX8888;
        case 24:
            return SCREEN_FORMAT_RGB888;
        case 16:
            switch (alpha_bit_depth) {
                case 4:
                    return SCREEN_FORMAT_RGBX4444;
                case 1:
                    return SCREEN_FORMAT_RGBA5551;
                default:
                    return SCREEN_FORMAT_RGB565;
            }
        default:
            return 0;
    }
}

// All indeces not already assigned will be zero'd to SDL_PIXELFORMAT_UNKNOWN.
//
// TODO: Move this stuff into a different file...
static const SDL_PixelFormat _format_map[] = {
    [SCREEN_FORMAT_RGBA4444] = SDL_PIXELFORMAT_RGBA4444,
    [SCREEN_FORMAT_RGBA5551] = SDL_PIXELFORMAT_RGBA5551,
    [SCREEN_FORMAT_RGB565] = SDL_PIXELFORMAT_RGB565,
    [SCREEN_FORMAT_RGBA8888] = SDL_PIXELFORMAT_RGBA8888,
    [SCREEN_FORMAT_RGBX8888] = SDL_PIXELFORMAT_RGBX8888,
    [SCREEN_FORMAT_NV12] = SDL_PIXELFORMAT_NV12,
    [SCREEN_FORMAT_YV12] = SDL_PIXELFORMAT_YV12,
    [SCREEN_FORMAT_UYVY] = SDL_PIXELFORMAT_UYVY,
    [SCREEN_FORMAT_YUY2] = SDL_PIXELFORMAT_YUY2,
    [SCREEN_FORMAT_YVYU] = SDL_PIXELFORMAT_YVYU,
    [SCREEN_FORMAT_P010] = SDL_PIXELFORMAT_P010,
    [SCREEN_FORMAT_BGRA8888] = SDL_PIXELFORMAT_BGRA8888,
    [SCREEN_FORMAT_BGRX8888] = SDL_PIXELFORMAT_BGRX8888,
};

SDL_PixelFormat screenToPixelFormat(int screen_format)
{
    if ((screen_format < 0) || (screen_format >= SDL_arraysize(_format_map))) {
        return SDL_PIXELFORMAT_UNKNOWN;
    }

    return _format_map[screen_format];
}

/**
 * Enumerates the supported EGL configurations and chooses a suitable one.
 * @param[out]  pformat The chosen pixel format
 * @return true if successful, false on error
 */
bool glInitConfig(SDL_WindowData *impl, int *pformat)
{
    EGLConfig egl_conf = (EGLConfig)0;
    EGLConfig *egl_configs;
    EGLint egl_num_configs;
    EGLint val;
    EGLBoolean rc;
    struct DummyConfig dummyconfig = {};

    // Determine the number of configurations.
    rc = eglGetConfigs(egl_disp, NULL, 0, &egl_num_configs);
    if (rc != EGL_TRUE) {
        return false;
    }

    if (egl_num_configs == 0) {
        return false;
    }

    // Allocate enough memory for all configurations.
    egl_configs = SDL_malloc(egl_num_configs * sizeof(*egl_configs));
    if (!egl_configs) {
        return false;
    }

    // Get the list of configurations.
    rc = eglGetConfigs(egl_disp, egl_configs, egl_num_configs,
                       &egl_num_configs);
    if (rc != EGL_TRUE) {
        SDL_free(egl_configs);
        return false;
    }

    dummyconfig = getDummyConfigFromScreenSettings(*pformat);
    egl_conf = chooseConfig(dummyconfig, egl_configs, egl_num_configs);
    screen_format = chooseFormat(egl_conf);
    pixel_format = screenToPixelFormat(screen_format);
    *pformat = screen_format;

    SDL_free(egl_configs);
    impl->conf = egl_conf;

    return true;
}

/**
 * Initializes the EGL library.
 * @param   SDL_VideoDevice *_this
 * @param   name    unused
 * @return  true if successful, false on error
 */
bool glLoadLibrary(SDL_VideoDevice *_this, const char *name)
{
    EGLNativeDisplayType    disp_id = EGL_DEFAULT_DISPLAY;

    egl_disp = eglGetDisplay(disp_id);
    if (egl_disp == EGL_NO_DISPLAY) {
        return false;
    }

    if (eglInitialize(egl_disp, NULL, NULL) == EGL_FALSE) {
        return false;
    }

    return true;
}

/**
 * Finds the address of an EGL extension function.
 * @param   proc    Function name
 * @return  Function address
 */
SDL_FunctionPointer glGetProcAddress(SDL_VideoDevice *_this, const char *proc)
{
    return eglGetProcAddress(proc);
}

/**
 * Associates the given window with the necessary EGL structures for drawing and
 * displaying content.
 * @param   SDL_VideoDevice *_this
 * @param   window  The SDL window to create the context for
 * @return  A pointer to the created context, if successful, NULL on error
 */
SDL_GLContext glCreateContext(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    EGLContext      context;
    EGLSurface      surface;

    struct {
        EGLint client_version[2];
        EGLint none;
    } egl_ctx_attr = {
        .client_version = { EGL_CONTEXT_CLIENT_VERSION, 2 },
        .none = EGL_NONE
    };

    struct {
        EGLint render_buffer[2];
        EGLint none;
    } egl_surf_attr = {
        .render_buffer = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER },
        .none = EGL_NONE
    };

    context = eglCreateContext(egl_disp, impl->conf, EGL_NO_CONTEXT,
                               (EGLint *)&egl_ctx_attr);
    if (context == EGL_NO_CONTEXT) {
        return NULL;
    }

    surface = eglCreateWindowSurface(egl_disp, impl->conf,
                                     (EGLNativeWindowType)impl->window,
                                     (EGLint *)&egl_surf_attr);
    if (surface == EGL_NO_SURFACE) {
        return NULL;
    }

    eglMakeCurrent(egl_disp, surface, surface, context);

    impl->surface = surface;
    impl->context = context;

    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_QNX_SURFACE_POINTER, impl->surface);

    return context;
}

/**
 * Sets a new value for the number of frames to display before swapping buffers.
 * @param   SDL_VideoDevice *_this
 * @param   interval    New interval value
 * @return  true if successful, false on error
 */
bool glSetSwapInterval(SDL_VideoDevice *_this, int interval)
{
    if (eglSwapInterval(egl_disp, interval) != EGL_TRUE) {
        return false;
    }

    return true;
}

/**
 * Swaps the EGL buffers associated with the given window
 * @param   SDL_VideoDevice *_this
 * @param   window  Window to swap buffers for
 * @return  true if successful, false on error
 */
bool glSwapWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    // !!! FIXME: should we migrate this all over to use SDL_egl.c?
    SDL_WindowData   *impl = (SDL_WindowData *)window->internal;
    {
        if (impl->resize) {
            EGLSurface surface;
            struct {
                EGLint render_buffer[2];
                EGLint none;
            } egl_surf_attr = {
                .render_buffer = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER },
                .none = EGL_NONE
            };

            if (eglMakeCurrent(egl_disp, NULL, NULL, impl->context) != EGL_TRUE) {
                return false;
            }
            if (eglDestroySurface(egl_disp, impl->surface) != EGL_TRUE) {
            }

            surface = eglCreateWindowSurface(egl_disp, impl->conf, impl->window,
                                     (EGLint *)&egl_surf_attr);
            if (surface == EGL_NO_SURFACE) {
                return false;
            }

            if (eglMakeCurrent(egl_disp, surface, surface, impl->context) != EGL_TRUE) {
                return false;
            }

            impl->surface = surface;
            impl->resize = 0;
        }
    }

    return eglSwapBuffers(egl_disp, impl->surface) == EGL_TRUE ? true : false;
}

/**
 * Makes the given context the current one for drawing operations.
 * @param   SDL_VideoDevice *_this
 * @param   window  SDL window associated with the context (maybe NULL)
 * @param   context The context to activate
 * @return  true if successful, false on error
 */
bool glMakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context)
{
    SDL_WindowData   *impl;
    EGLSurface      surface = NULL;

    if (window) {
        impl = (SDL_WindowData *)window->internal;
        surface = impl->surface;
    }

    if (eglMakeCurrent(egl_disp, surface, surface, context) != EGL_TRUE) {
        return false;
    }

    return true;
}

/**
 * Destroys a context.
 * @param   SDL_VideoDevice *_this
 * @param   context The context to destroy
 */
bool glDeleteContext(SDL_VideoDevice *_this, SDL_GLContext context)
{
    eglDestroyContext(egl_disp, context);
    return true;
}

/**
 * Terminates access to the EGL library.
 * @param   SDL_VideoDevice *_this
 */
void glUnloadLibrary(SDL_VideoDevice *_this)
{
    eglTerminate(egl_disp);
}
