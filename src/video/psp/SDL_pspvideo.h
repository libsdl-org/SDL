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

#ifndef SDL_pspvideo_h_
#define SDL_pspvideo_h_

#include <GLES/egl.h>

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"

typedef struct SDL_VideoData
{
    SDL_bool egl_initialized; /* OpenGL ES device initialization status */
    uint32_t egl_refcount;    /* OpenGL ES reference count              */

} SDL_VideoData;

typedef struct SDL_DisplayData
{

} SDL_DisplayData;

typedef struct SDL_WindowData
{
    SDL_bool uses_gles; /* if true window must support OpenGL ES */

} SDL_WindowData;

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int PSP_VideoInit(THIS);
void PSP_VideoQuit(THIS);
void PSP_GetDisplayModes(THIS, SDL_VideoDisplay *display);
int PSP_SetDisplayMode(THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int PSP_CreateWindow(THIS, SDL_Window *window);
int PSP_CreateWindowFrom(THIS, SDL_Window *window, const void *data);
void PSP_SetWindowTitle(THIS, SDL_Window *window);
void PSP_SetWindowIcon(THIS, SDL_Window *window, SDL_Surface *icon);
void PSP_SetWindowPosition(THIS, SDL_Window *window);
void PSP_SetWindowSize(THIS, SDL_Window *window);
void PSP_ShowWindow(THIS, SDL_Window *window);
void PSP_HideWindow(THIS, SDL_Window *window);
void PSP_RaiseWindow(THIS, SDL_Window *window);
void PSP_MaximizeWindow(THIS, SDL_Window *window);
void PSP_MinimizeWindow(THIS, SDL_Window *window);
void PSP_RestoreWindow(THIS, SDL_Window *window);
void PSP_DestroyWindow(THIS, SDL_Window *window);

/* OpenGL/OpenGL ES functions */
int PSP_GL_LoadLibrary(THIS, const char *path);
void *PSP_GL_GetProcAddress(THIS, const char *proc);
void PSP_GL_UnloadLibrary(THIS);
SDL_GLContext PSP_GL_CreateContext(THIS, SDL_Window *window);
int PSP_GL_MakeCurrent(THIS, SDL_Window *window, SDL_GLContext context);
int PSP_GL_SetSwapInterval(THIS, int interval);
int PSP_GL_GetSwapInterval(THIS);
int PSP_GL_SwapWindow(THIS, SDL_Window *window);
void PSP_GL_DeleteContext(THIS, SDL_GLContext context);

/* PSP on screen keyboard */
SDL_bool PSP_HasScreenKeyboardSupport(THIS);
void PSP_ShowScreenKeyboard(THIS, SDL_Window *window);
void PSP_HideScreenKeyboard(THIS, SDL_Window *window);
SDL_bool PSP_IsScreenKeyboardShown(THIS, SDL_Window *window);

#endif /* SDL_pspvideo_h_ */
