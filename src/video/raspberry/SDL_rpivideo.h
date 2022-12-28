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

#ifndef SDL_rpivideo_h
#define SDL_rpivideo_h

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"

#include <bcm_host.h>
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

typedef struct SDL_VideoData
{
    uint32_t egl_refcount; /* OpenGL ES reference count              */
} SDL_VideoData;

typedef struct SDL_DisplayData
{
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
} SDL_DisplayData;

typedef struct SDL_WindowData
{
    EGL_DISPMANX_WINDOW_T dispman_window;
#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif

    /* Vsync callback cond and mutex */
    SDL_cond *vsync_cond;
    SDL_mutex *vsync_cond_mutex;
    SDL_bool double_buffer;

} SDL_WindowData;

#define SDL_RPI_VIDEOLAYER 10000 /* High enough so to occlude everything */
#define SDL_RPI_MOUSELAYER SDL_RPI_VIDEOLAYER + 1

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int RPI_VideoInit(THIS);
void RPI_VideoQuit(THIS);
void RPI_GetDisplayModes(THIS, SDL_VideoDisplay *display);
int RPI_SetDisplayMode(THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int RPI_CreateWindow(THIS, SDL_Window *window);
int RPI_CreateWindowFrom(THIS, SDL_Window *window, const void *data);
void RPI_SetWindowTitle(THIS, SDL_Window *window);
void RPI_SetWindowIcon(THIS, SDL_Window *window, SDL_Surface *icon);
void RPI_SetWindowPosition(THIS, SDL_Window *window);
void RPI_SetWindowSize(THIS, SDL_Window *window);
void RPI_ShowWindow(THIS, SDL_Window *window);
void RPI_HideWindow(THIS, SDL_Window *window);
void RPI_RaiseWindow(THIS, SDL_Window *window);
void RPI_MaximizeWindow(THIS, SDL_Window *window);
void RPI_MinimizeWindow(THIS, SDL_Window *window);
void RPI_RestoreWindow(THIS, SDL_Window *window);
void RPI_DestroyWindow(THIS, SDL_Window *window);

/* OpenGL/OpenGL ES functions */
int RPI_GLES_LoadLibrary(THIS, const char *path);
void *RPI_GLES_GetProcAddress(THIS, const char *proc);
void RPI_GLES_UnloadLibrary(THIS);
SDL_GLContext RPI_GLES_CreateContext(THIS, SDL_Window *window);
int RPI_GLES_MakeCurrent(THIS, SDL_Window *window, SDL_GLContext context);
int RPI_GLES_SetSwapInterval(THIS, int interval);
int RPI_GLES_GetSwapInterval(THIS);
int RPI_GLES_SwapWindow(THIS, SDL_Window *window);
void RPI_GLES_DeleteContext(THIS, SDL_GLContext context);

#endif /* SDL_rpivideo_h */
