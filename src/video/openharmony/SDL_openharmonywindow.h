/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifndef SDL_openharmonywindow_h_
#define SDL_openharmonywindow_h_

#include "../../core/openharmony/SDL_openharmony.h"
#include "../SDL_egl_c.h"

extern bool OPENHARMONY_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
extern void OPENHARMONY_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern SDL_FullscreenResult OPENHARMONY_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen);
extern void OPENHARMONY_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void OPENHARMONY_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, bool resizable);

extern void OPENHARMONY_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern SDL_Window *OPENHARMONY_Window;

struct SDL_WindowData
{
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
    EGLContext egl_context; // We use this to preserve the context when losing focus
    int has_swap_interval;  // Save/Restore the swap interval / vsync
    int swap_interval;
#endif
    bool backup_done;
    void *native_xcomponent;
    void *native_window;
};

#endif // SDL_openharmonywindow_h_
