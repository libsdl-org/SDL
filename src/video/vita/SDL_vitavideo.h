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

#ifndef SDL_vitavideo_h
#define SDL_vitavideo_h

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

#include <psp2/types.h>
#include <psp2/display.h>
#include <psp2/ime_dialog.h>
#include <psp2/sysmodule.h>

struct SDL_VideoData
{
    SDL_bool egl_initialized; /* OpenGL device initialization status */
    uint32_t egl_refcount;    /* OpenGL reference count              */

    SceWChar16 ime_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SDL_bool ime_active;
};

struct SDL_WindowData
{
    SDL_bool uses_gles;
    SceUID buffer_uid;
    void *buffer;
#ifdef SDL_VIDEO_VITA_PVR
    EGLSurface egl_surface;
    EGLContext egl_context;
#endif
};

extern SDL_Window *Vita_Window;

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int VITA_VideoInit(SDL_VideoDevice *_this);
void VITA_VideoQuit(SDL_VideoDevice *_this);
int VITA_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display);
int VITA_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int VITA_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window);
int VITA_CreateWindowFrom(SDL_VideoDevice *_this, SDL_Window *window, const void *data);
void VITA_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
int VITA_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_HideWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_SetWindowGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed);
void VITA_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);

#ifdef SDL_VIDEO_DRIVER_VITA
#ifdef SDL_VIDEO_VITA_PVR_OGL
/* OpenGL functions */
int VITA_GL_LoadLibrary(SDL_VideoDevice *_this, const char *path);
SDL_GLContext VITA_GL_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
SDL_FunctionPointer VITA_GL_GetProcAddress(SDL_VideoDevice *_this, const char *proc);
#endif

/* OpenGLES functions */
int VITA_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path);
SDL_FunctionPointer VITA_GLES_GetProcAddress(SDL_VideoDevice *_this, const char *proc);
void VITA_GLES_UnloadLibrary(SDL_VideoDevice *_this);
SDL_GLContext VITA_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
int VITA_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context);
int VITA_GLES_SetSwapInterval(SDL_VideoDevice *_this, int interval);
int VITA_GLES_GetSwapInterval(SDL_VideoDevice *_this, int *interval);
int VITA_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window);
int VITA_GLES_DeleteContext(SDL_VideoDevice *_this, SDL_GLContext context);
#endif

/* VITA on screen keyboard */
SDL_bool VITA_HasScreenKeyboardSupport(SDL_VideoDevice *_this);
void VITA_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window);
void VITA_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window);
SDL_bool VITA_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window);

void VITA_PumpEvents(SDL_VideoDevice *_this);

#endif /* SDL_pspvideo_h */
