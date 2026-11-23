/*
  Simple DirectMedia Layer
  Copyright (C) 2026 BlackBerry Limited

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

#ifndef __SDL_QNX_H__
#define __SDL_QNX_H__

#include "../SDL_sysvideo.h"
#include <screen/screen.h>
#include <EGL/egl.h>

typedef struct SDL_DisplayData
{
    screen_display_t screen_display;
} SDL_DisplayData;

typedef struct SDL_DisplayModeData
{
    int                   screen_format;
    screen_display_mode_t screen_display_mode;
} SDL_DisplayModeData;

typedef struct SDL_WindowData
{
    screen_window_t window;
    EGLSurface      egl_surface;
    SDL_GLContext   context;
    bool            resize;
    bool            has_focus;
} SDL_WindowData;

typedef struct SDL_CursorData
{
    screen_session_t session;
    int              realized_shape;
    bool             is_visible;
} SDL_CursorData;

typedef struct SDL_MouseData
{
    int      x_prev;
    int      y_prev;
} SDL_MouseData;

extern screen_context_t * getContext();
extern screen_event_t * getEvent();

extern void handleKeyboardEvent(screen_event_t event);
extern void handlePointerEvent(screen_event_t event);

// extern bool glInitConfig(SDL_WindowData *impl, int *pformat);
extern bool QNX_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *egl_path);
extern SDL_FunctionPointer QNX_GLES_GetProcAddress(SDL_VideoDevice *_this, const char *proc);
extern SDL_GLContext QNX_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window);
extern bool QNX_GLES_SetSwapInterval(SDL_VideoDevice *_this, int interval);
extern bool QNX_GLES_GetSwapInterval(SDL_VideoDevice *_this, int *interval);
extern bool QNX_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern bool QNX_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window * window, SDL_GLContext context);
extern bool QNX_GLES_DeleteContext(SDL_VideoDevice *_this, SDL_GLContext context);
extern void QNX_GLES_UnloadLibrary(SDL_VideoDevice *_this);

extern SDL_PixelFormat screenToPixelFormat(int screen_format);
extern bool QNX_GetDisplayBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect);
extern bool QNX_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display);
extern bool QNX_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode);

extern void initMouse(SDL_VideoDevice *_this);
extern void quitMouse(SDL_VideoDevice *_this);

extern int QNX_ChooseFormat(SDL_VideoDevice *_this, EGLConfig egl_conf);

#endif
