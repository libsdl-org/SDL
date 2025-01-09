/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifndef __SDL_MX6VIDEO_H__
#define __SDL_MX6VIDEO_H__

#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"

#include "SDL_egl.h"

typedef struct SDL_VideoData
{
} SDL_VideoData;

typedef struct SDL_DisplayData
{
    EGLNativeDisplayType native_display;
    EGLDisplay egl_display;
} SDL_DisplayData;

typedef struct SDL_WindowData
{
    EGLNativeWindowType native_window;
    EGLSurface egl_surface;
} SDL_WindowData;

/****************************************************************************/
/* SDL_VideoDevice functions declaration                                    */
/****************************************************************************/

/* Display and window functions */
int MX6_VideoInit(_THIS);
void MX6_VideoQuit(_THIS);
void MX6_GetDisplayModes(_THIS, SDL_VideoDisplay * display);
int MX6_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
int MX6_CreateWindow(_THIS, SDL_Window * window);
int MX6_CreateWindowFrom(_THIS, SDL_Window * window, const void *data);
void MX6_SetWindowTitle(_THIS, SDL_Window * window);
void MX6_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon);
void MX6_SetWindowPosition(_THIS, SDL_Window * window);
void MX6_SetWindowSize(_THIS, SDL_Window * window);
void MX6_ShowWindow(_THIS, SDL_Window * window);
void MX6_HideWindow(_THIS, SDL_Window * window);
void MX6_RaiseWindow(_THIS, SDL_Window * window);
void MX6_MaximizeWindow(_THIS, SDL_Window * window);
void MX6_MinimizeWindow(_THIS, SDL_Window * window);
void MX6_RestoreWindow(_THIS, SDL_Window * window);
void MX6_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed);
void MX6_DestroyWindow(_THIS, SDL_Window * window);

/* Window manager function */
SDL_bool MX6_GetWindowWMInfo(_THIS, SDL_Window * window,
                             struct SDL_SysWMinfo *info);

#endif /* __SDL_MX6VIDEO_H__ */

/* vi: set ts=4 sw=4 expandtab: */
