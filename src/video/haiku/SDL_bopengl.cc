/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_HAIKU

#include "SDL_bopengl.h"

#include <unistd.h>
#include <KernelKit.h>
#include <OpenGLKit.h>
#include "SDL_BWin.h"
#include "../../main/haiku/SDL_BApp.h"

#ifdef __cplusplus
extern "C" {
#endif


#define BGL_FLAGS BGL_RGB | BGL_DOUBLE

static SDL_INLINE SDL_BWin *_ToBeWin(SDL_Window *window) {
    return ((SDL_BWin*)(window->driverdata));
}

static SDL_INLINE SDL_BApp *_GetBeApp() {
    return ((SDL_BApp*)be_app);
}

/* Passing a NULL path means load pointers from the application */
int BE_GL_LoadLibrary(_THIS, const char *path)
{
/* FIXME: Is this working correctly? */
    image_info info;
            int32 cookie = 0;
    while (get_next_image_info(0, &cookie, &info) == B_OK) {
        void *location = NULL;
        if( get_image_symbol(info.id, "glBegin", B_SYMBOL_TYPE_ANY,
                &location) == B_OK) {

            _this->gl_config.dll_handle = (void *) info.id;
            _this->gl_config.driver_loaded = 1;
            SDL_strlcpy(_this->gl_config.driver_path, "libGL.so",
                    SDL_arraysize(_this->gl_config.driver_path));
        }
    }
    return 0;
}

void *BE_GL_GetProcAddress(_THIS, const char *proc)
{
    if (_this->gl_config.dll_handle != NULL) {
        void *location = NULL;
        status_t err;
        if ((err =
            get_image_symbol((image_id) _this->gl_config.dll_handle,
                              proc, B_SYMBOL_TYPE_ANY,
                              &location)) == B_OK) {
            return location;
        } else {
                SDL_SetError("Couldn't find OpenGL symbol");
                return NULL;
        }
    } else {
        SDL_SetError("OpenGL library not loaded");
        return NULL;
    }
}




int BE_GL_SwapWindow(_THIS, SDL_Window * window) {
    _ToBeWin(window)->SwapBuffers();
    return 0;
}

int BE_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context) {
    _GetBeApp()->SetCurrentContext(((SDL_BWin*)context)->GetGLView());
    return 0;
}


SDL_GLContext BE_GL_CreateContext(_THIS, SDL_Window * window) {
    /* FIXME: Not sure what flags should be included here; may want to have
       most of them */
    SDL_BWin *bwin = _ToBeWin(window);
    bwin->CreateGLView(BGL_FLAGS);
    return (SDL_GLContext)(bwin);
}

void BE_GL_DeleteContext(_THIS, SDL_GLContext context) {
    /* Currently, automatically unlocks the view */
    ((SDL_BWin*)context)->RemoveGLView();
}


int BE_GL_SetSwapInterval(_THIS, int interval) {
    /* TODO: Implement this, if necessary? */
    return 0;
}

int BE_GL_GetSwapInterval(_THIS) {
    /* TODO: Implement this, if necessary? */
    return 0;
}


void BE_GL_UnloadLibrary(_THIS) {
    /* TODO: Implement this, if necessary? */
}


/* FIXME: This function is meant to clear the OpenGL context when the video
   mode changes (see SDL_bmodes.cc), but it doesn't seem to help, and is not
   currently in use. */
void BE_GL_RebootContexts(_THIS) {
    SDL_Window *window = _this->windows;
    while(window) {
        SDL_BWin *bwin = _ToBeWin(window);
        if(bwin->GetGLView()) {
            bwin->LockLooper();
            bwin->RemoveGLView();
            bwin->CreateGLView(BGL_FLAGS);
            bwin->UnlockLooper();
        }
        window = window->next;
    }
}


#if 0 /* Functions from 1.2 that do not appear to be used in 1.3 */

    int BE_GL_GetAttribute(_THIS, SDL_GLattr attrib, int *value)
    {
        /*
           FIXME? Right now BE_GL_GetAttribute shouldn't be called between glBegin() and glEnd() - it doesn't use "cached" values
         */
        switch (attrib) {
        case SDL_GL_RED_SIZE:
            glGetIntegerv(GL_RED_BITS, (GLint *) value);
            break;
        case SDL_GL_GREEN_SIZE:
            glGetIntegerv(GL_GREEN_BITS, (GLint *) value);
            break;
        case SDL_GL_BLUE_SIZE:
            glGetIntegerv(GL_BLUE_BITS, (GLint *) value);
            break;
        case SDL_GL_ALPHA_SIZE:
            glGetIntegerv(GL_ALPHA_BITS, (GLint *) value);
            break;
        case SDL_GL_DOUBLEBUFFER:
            glGetBooleanv(GL_DOUBLEBUFFER, (GLboolean *) value);
            break;
        case SDL_GL_BUFFER_SIZE:
            int v;
            glGetIntegerv(GL_RED_BITS, (GLint *) & v);
            *value = v;
            glGetIntegerv(GL_GREEN_BITS, (GLint *) & v);
            *value += v;
            glGetIntegerv(GL_BLUE_BITS, (GLint *) & v);
            *value += v;
            glGetIntegerv(GL_ALPHA_BITS, (GLint *) & v);
            *value += v;
            break;
        case SDL_GL_DEPTH_SIZE:
            glGetIntegerv(GL_DEPTH_BITS, (GLint *) value);      /* Mesa creates 16 only? r5 always 32 */
            break;
        case SDL_GL_STENCIL_SIZE:
            glGetIntegerv(GL_STENCIL_BITS, (GLint *) value);
            break;
        case SDL_GL_ACCUM_RED_SIZE:
            glGetIntegerv(GL_ACCUM_RED_BITS, (GLint *) value);
            break;
        case SDL_GL_ACCUM_GREEN_SIZE:
            glGetIntegerv(GL_ACCUM_GREEN_BITS, (GLint *) value);
            break;
        case SDL_GL_ACCUM_BLUE_SIZE:
            glGetIntegerv(GL_ACCUM_BLUE_BITS, (GLint *) value);
            break;
        case SDL_GL_ACCUM_ALPHA_SIZE:
            glGetIntegerv(GL_ACCUM_ALPHA_BITS, (GLint *) value);
            break;
        case SDL_GL_STEREO:
        case SDL_GL_MULTISAMPLEBUFFERS:
        case SDL_GL_MULTISAMPLESAMPLES:
        default:
            *value = 0;
            return (-1);
        }
        return 0;
    }

#endif



#ifdef __cplusplus
}
#endif

#endif /* SDL_VIDEO_DRIVER_HAIKU */

/* vi: set ts=4 sw=4 expandtab: */
