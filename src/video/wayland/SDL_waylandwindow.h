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

#include "../../SDL_internal.h"

#ifndef _SDL_waylandwindow_h
#define _SDL_waylandwindow_h

#include "../SDL_sysvideo.h"
#include "SDL_syswm.h"

#include "SDL_waylandvideo.h"

struct SDL_WaylandInput;

typedef struct {
    SDL_Window *sdlwindow;
    SDL_VideoData *waylandData;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_egl_window *egl_window;
    struct SDL_WaylandInput *keyboard_device;
    EGLSurface egl_surface;

#ifdef SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH
    struct qt_extended_surface *extended_surface;
#endif /* SDL_VIDEO_DRIVER_WAYLAND_QT_TOUCH */    
} SDL_WindowData;

extern void Wayland_ShowWindow(_THIS, SDL_Window *window);
extern void Wayland_SetWindowFullscreen(_THIS, SDL_Window * window,
                                        SDL_VideoDisplay * _display,
                                        SDL_bool fullscreen);
extern int Wayland_CreateWindow(_THIS, SDL_Window *window);
extern void Wayland_SetWindowSize(_THIS, SDL_Window * window);
extern void Wayland_DestroyWindow(_THIS, SDL_Window *window);

extern SDL_bool
Wayland_GetWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info);

#endif /* _SDL_waylandwindow_h */

/* vi: set ts=4 sw=4 expandtab: */
