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

#ifndef SDL_pspvideo_h_
#define SDL_pspvideo_h_

#include <GLES/egl.h>

#include "SDL_internal.h"
#include "../SDL_sysvideo.h"

struct SDL_VideoData
{
    SDL_bool egl_initialized; /* OpenGL ES device initialization status */
    uint32_t egl_refcount;    /* OpenGL ES reference count              */

};

struct SDL_WindowData
{
    SDL_bool uses_gles; /* if true window must support OpenGL ES */

};

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int PSP_VideoInit(SDL_VideoDevice *_this);
void PSP_VideoQuit(SDL_VideoDevice *_this);
int PSP_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display);
int PSP_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
int PSP_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
void PSP_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
int PSP_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_HideWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);

/* OpenGL/OpenGL ES functions */
int PSP_GL_LoadLibrary(SDL_VideoDevice *_this, const char *path);
SDL_FunctionPointer PSP_GL_GetProcAddress(SDL_VideoDevice *_this, const char *proc);
void PSP_GL_UnloadLibrary(SDL_VideoDevice *_this);
SDL_GLContext PSP_GL_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
int PSP_GL_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context);
int PSP_GL_SetSwapInterval(SDL_VideoDevice *_this, int interval);
int PSP_GL_GetSwapInterval(SDL_VideoDevice *_this, int *interval);
int PSP_GL_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window);
int PSP_GL_DeleteContext(SDL_VideoDevice *_this, SDL_GLContext context);

/* PSP on screen keyboard */
SDL_bool PSP_HasScreenKeyboardSupport(SDL_VideoDevice *_this);
void PSP_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window);
void PSP_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window);
SDL_bool PSP_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window);

#endif /* SDL_pspvideo_h_ */
