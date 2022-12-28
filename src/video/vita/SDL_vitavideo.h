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

#ifndef SDL_vitavideo_h
#define SDL_vitavideo_h

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"
#include "../SDL_egl_c.h"

#include <psp2/types.h>
#include <psp2/display.h>
#include <psp2/ime_dialog.h>
#include <psp2/sysmodule.h>

typedef struct SDL_VideoData
{
    SDL_bool egl_initialized; /* OpenGL device initialization status */
    uint32_t egl_refcount;    /* OpenGL reference count              */

    SceWChar16 ime_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SDL_bool ime_active;
} SDL_VideoData;

typedef struct SDL_DisplayData
{

} SDL_DisplayData;

typedef struct SDL_WindowData
{
    SDL_bool uses_gles;
    SceUID buffer_uid;
    void *buffer;
#if defined(SDL_VIDEO_VITA_PVR)
    EGLSurface egl_surface;
    EGLContext egl_context;
#endif
} SDL_WindowData;

extern SDL_Window *Vita_Window;

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int VITA_VideoInit(THIS);
void VITA_VideoQuit(THIS);
void VITA_GetDisplayModes(THIS, SDL_VideoDisplay *display);
int VITA_SetDisplayMode(THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int VITA_CreateWindow(THIS, SDL_Window *window);
int VITA_CreateWindowFrom(THIS, SDL_Window *window, const void *data);
void VITA_SetWindowTitle(THIS, SDL_Window *window);
void VITA_SetWindowIcon(THIS, SDL_Window *window, SDL_Surface *icon);
void VITA_SetWindowPosition(THIS, SDL_Window *window);
void VITA_SetWindowSize(THIS, SDL_Window *window);
void VITA_ShowWindow(THIS, SDL_Window *window);
void VITA_HideWindow(THIS, SDL_Window *window);
void VITA_RaiseWindow(THIS, SDL_Window *window);
void VITA_MaximizeWindow(THIS, SDL_Window *window);
void VITA_MinimizeWindow(THIS, SDL_Window *window);
void VITA_RestoreWindow(THIS, SDL_Window *window);
void VITA_SetWindowGrab(THIS, SDL_Window *window, SDL_bool grabbed);
void VITA_DestroyWindow(THIS, SDL_Window *window);

#if SDL_VIDEO_DRIVER_VITA
#if defined(SDL_VIDEO_VITA_PVR_OGL)
/* OpenGL functions */
int VITA_GL_LoadLibrary(THIS, const char *path);
SDL_GLContext VITA_GL_CreateContext(THIS, SDL_Window *window);
void *VITA_GL_GetProcAddress(THIS, const char *proc);
#endif

/* OpenGLES functions */
int VITA_GLES_LoadLibrary(THIS, const char *path);
void *VITA_GLES_GetProcAddress(THIS, const char *proc);
void VITA_GLES_UnloadLibrary(THIS);
SDL_GLContext VITA_GLES_CreateContext(THIS, SDL_Window *window);
int VITA_GLES_MakeCurrent(THIS, SDL_Window *window, SDL_GLContext context);
int VITA_GLES_SetSwapInterval(THIS, int interval);
int VITA_GLES_GetSwapInterval(THIS);
int VITA_GLES_SwapWindow(THIS, SDL_Window *window);
void VITA_GLES_DeleteContext(THIS, SDL_GLContext context);
#endif

/* VITA on screen keyboard */
SDL_bool VITA_HasScreenKeyboardSupport(THIS);
void VITA_ShowScreenKeyboard(THIS, SDL_Window *window);
void VITA_HideScreenKeyboard(THIS, SDL_Window *window);
SDL_bool VITA_IsScreenKeyboardShown(THIS, SDL_Window *window);

void VITA_PumpEvents(THIS);

#endif /* SDL_pspvideo_h */
