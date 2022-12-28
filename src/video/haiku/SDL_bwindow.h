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

#ifndef SDL_BWINDOW_H
#define SDL_BWINDOW_H

#include "../SDL_sysvideo.h"

extern int HAIKU_CreateWindow(THIS, SDL_Window *window);
extern int HAIKU_CreateWindowFrom(THIS, SDL_Window *window, const void *data);
extern void HAIKU_SetWindowTitle(THIS, SDL_Window *window);
extern void HAIKU_SetWindowIcon(THIS, SDL_Window *window, SDL_Surface *icon);
extern void HAIKU_SetWindowPosition(THIS, SDL_Window *window);
extern void HAIKU_SetWindowSize(THIS, SDL_Window *window);
extern void HAIKU_SetWindowMinimumSize(THIS, SDL_Window *window);
extern void HAIKU_ShowWindow(THIS, SDL_Window *window);
extern void HAIKU_HideWindow(THIS, SDL_Window *window);
extern void HAIKU_RaiseWindow(THIS, SDL_Window *window);
extern void HAIKU_MaximizeWindow(THIS, SDL_Window *window);
extern void HAIKU_MinimizeWindow(THIS, SDL_Window *window);
extern void HAIKU_RestoreWindow(THIS, SDL_Window *window);
extern void HAIKU_SetWindowBordered(THIS, SDL_Window *window, SDL_bool bordered);
extern void HAIKU_SetWindowResizable(THIS, SDL_Window *window, SDL_bool resizable);
extern void HAIKU_SetWindowFullscreen(THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen);
extern void HAIKU_SetWindowMouseGrab(THIS, SDL_Window *window, SDL_bool grabbed);
extern void HAIKU_DestroyWindow(THIS, SDL_Window *window);
extern int HAIKU_GetWindowWMInfo(THIS, SDL_Window *window, struct SDL_SysWMinfo *info);

#endif
