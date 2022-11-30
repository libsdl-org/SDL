/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_EMSCRIPTEN

#include <emscripten/emscripten.h>
#include <emscripten/html5_webgl.h>
#include <GLES2/gl2.h>

#include "SDL_emscriptenvideo.h"
#include "SDL_emscriptenopengles.h"

int Emscripten_GLES_LoadLibrary(_THIS, const char *path)
{
    return 0;
}

void Emscripten_GLES_UnloadLibrary(_THIS)
{
}

void *
Emscripten_GLES_GetProcAddress(_THIS, const char *proc)
{
    return emscripten_webgl_get_proc_address(proc);
}

int Emscripten_GLES_SetSwapInterval(_THIS, int interval)
{
    if (interval < 0) {
        return SDL_SetError("Late swap tearing currently unsupported");
    } else if (interval == 0) {
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 0);
    } else {
        emscripten_set_main_loop_timing(EM_TIMING_RAF, interval);
    }

    return 0;
}

int Emscripten_GLES_GetSwapInterval(_THIS)
{
    int mode, value;

    emscripten_get_main_loop_timing(&mode, &value);

    if (mode == EM_TIMING_RAF)
        return value;

    return 0;
}

SDL_GLContext
Emscripten_GLES_CreateContext(_THIS, SDL_Window *window)
{
    SDL_WindowData *window_data;

    EmscriptenWebGLContextAttributes attribs;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;

    emscripten_webgl_init_context_attributes(&attribs);

    attribs.alpha = _this->gl_config.alpha_size > 0;
    attribs.depth = _this->gl_config.depth_size > 0;
    attribs.stencil = _this->gl_config.stencil_size > 0;
    attribs.antialias = _this->gl_config.multisamplebuffers == 1;

    if (_this->gl_config.major_version == 3)
        attribs.majorVersion = 2; /* WebGL 2.0 ~= GLES 3.0 */

    window_data = (SDL_WindowData *)window->driverdata;

    if (window_data->gl_context) {
        SDL_SetError("Cannot create multiple webgl contexts per window");
        return NULL;
    }

    context = emscripten_webgl_create_context(window_data->canvas_id, &attribs);

    if (context < 0) {
        SDL_SetError("Could not create webgl context");
        return NULL;
    }

    if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS) {
        emscripten_webgl_destroy_context(context);
        return NULL;
    }

    window_data->gl_context = (SDL_GLContext)context;

    return (SDL_GLContext)context;
}

void Emscripten_GLES_DeleteContext(_THIS, SDL_GLContext context)
{
    SDL_Window *window;

    /* remove the context from its window */
    for (window = _this->windows; window != NULL; window = window->next) {
        SDL_WindowData *window_data;
        window_data = (SDL_WindowData *)window->driverdata;

        if (window_data->gl_context == context) {
            window_data->gl_context = NULL;
        }
    }

    emscripten_webgl_destroy_context((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context);
}

int Emscripten_GLES_SwapWindow(_THIS, SDL_Window *window)
{
    if (emscripten_has_asyncify() && SDL_GetHintBoolean(SDL_HINT_EMSCRIPTEN_ASYNCIFY, SDL_TRUE)) {
        /* give back control to browser for screen refresh */
        emscripten_sleep(0);
    }
    return 0;
}

int Emscripten_GLES_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    /* it isn't possible to reuse contexts across canvases */
    if (window && context) {
        SDL_WindowData *window_data;
        window_data = (SDL_WindowData *)window->driverdata;

        if (context != window_data->gl_context) {
            return SDL_SetError("Cannot make context current to another window");
        }
    }

    if (emscripten_webgl_make_context_current((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context) != EMSCRIPTEN_RESULT_SUCCESS) {
        return SDL_SetError("Unable to make context current");
    }
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_EMSCRIPTEN */

/* vi: set ts=4 sw=4 expandtab: */
