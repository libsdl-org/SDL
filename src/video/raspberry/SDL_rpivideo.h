/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_rpivideo_h
#define SDL_rpivideo_h

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"

#include <bcm_host.h>
#include <SDL3/SDL_egl.h>

struct SDL_VideoData
{
    uint32_t egl_refcount; /* OpenGL ES reference count              */
};

struct SDL_DisplayData
{
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
};

struct SDL_WindowData
{
    EGL_DISPMANX_WINDOW_T dispman_window;
#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif

    /* Vsync callback cond and mutex */
    SDL_cond *vsync_cond;
    SDL_mutex *vsync_cond_mutex;
    SDL_bool double_buffer;
};

#define SDL_RPI_VIDEOLAYER 10000 /* High enough so to occlude everything */
#define SDL_RPI_MOUSELAYER SDL_RPI_VIDEOLAYER + 1

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int RPI_VideoInit(_THIS);
void RPI_VideoQuit(_THIS);
int RPI_GetDisplayModes(_THIS, SDL_VideoDisplay *display);
int RPI_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int RPI_CreateWindow(_THIS, SDL_Window *window);
int RPI_CreateWindowFrom(_THIS, SDL_Window *window, const void *data);
void RPI_SetWindowTitle(_THIS, SDL_Window *window);
void RPI_SetWindowPosition(_THIS, SDL_Window *window);
void RPI_SetWindowSize(_THIS, SDL_Window *window);
void RPI_ShowWindow(_THIS, SDL_Window *window);
void RPI_HideWindow(_THIS, SDL_Window *window);
void RPI_RaiseWindow(_THIS, SDL_Window *window);
void RPI_MaximizeWindow(_THIS, SDL_Window *window);
void RPI_MinimizeWindow(_THIS, SDL_Window *window);
void RPI_RestoreWindow(_THIS, SDL_Window *window);
void RPI_DestroyWindow(_THIS, SDL_Window *window);

/* OpenGL/OpenGL ES functions */
int RPI_GLES_LoadLibrary(_THIS, const char *path);
SDL_FunctionPointer RPI_GLES_GetProcAddress(_THIS, const char *proc);
void RPI_GLES_UnloadLibrary(_THIS);
SDL_GLContext RPI_GLES_CreateContext(_THIS, SDL_Window *window);
int RPI_GLES_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context);
int RPI_GLES_SetSwapInterval(_THIS, int interval);
int RPI_GLES_GetSwapInterval(_THIS);
int RPI_GLES_SwapWindow(_THIS, SDL_Window *window);
int RPI_GLES_DeleteContext(_THIS, SDL_GLContext context);

#endif /* SDL_rpivideo_h */
