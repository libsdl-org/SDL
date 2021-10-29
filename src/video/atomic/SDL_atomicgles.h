/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>
  Atomic KMSDRM backend by Manuel Alfayate Corchete <redwindwanderer@gmail.com>

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

#ifndef SDL_atomicgles_h_
#define SDL_atomicgles_h_

#if SDL_VIDEO_DRIVER_ATOMIC

#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

/* OpenGLES functions */
#define ATOMIC_GLES_GetAttribute SDL_EGL_GetAttribute
#define ATOMIC_GLES_GetProcAddress SDL_EGL_GetProcAddress
#define ATOMIC_GLES_DeleteContext SDL_EGL_DeleteContext
#define ATOMIC_GLES_GetSwapInterval SDL_EGL_GetSwapInterval

extern void ATOMIC_GLES_DefaultProfileConfig(_THIS, int *mask, int *major, int *minor);
extern int ATOMIC_GLES_SetSwapInterval(_THIS, int interval);
extern int ATOMIC_GLES_LoadLibrary(_THIS, const char *path);
extern SDL_GLContext ATOMIC_GLES_CreateContext(_THIS, SDL_Window * window);
extern int ATOMIC_GLES_SwapWindow(_THIS, SDL_Window * window);
extern int ATOMIC_GLES_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

#endif /* SDL_atomicgles_h_ */

/* vi: set ts=4 sw=4 expandtab: */
