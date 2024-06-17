/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_windowswindow_h_
#define SDL_windowswindow_h_

#ifdef SDL_VIDEO_OPENGL_EGL
#include "../SDL_egl_c.h"
#else
#include "../SDL_sysvideo.h"
#endif

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum SDL_WindowRect
{
    SDL_WINDOWRECT_CURRENT,
    SDL_WINDOWRECT_WINDOWED,
    SDL_WINDOWRECT_FLOATING
} SDL_WindowRect;

typedef enum SDL_WindowEraseBackgroundMode
{
    SDL_ERASEBACKGROUNDMODE_NEVER,
    SDL_ERASEBACKGROUNDMODE_INITIAL,
    SDL_ERASEBACKGROUNDMODE_ALWAYS,
} SDL_WindowEraseBackgroundMode;

struct SDL_WindowData
{
    SDL_Window *window;
    HWND hwnd;
    HWND parent;
    HDC hdc;
    HDC mdc;
    HINSTANCE hinstance;
    HBITMAP hbm;
    WNDPROC wndproc;
    HHOOK keyboard_hook;
    WPARAM mouse_button_flags;
    LPARAM last_pointer_update;
    WCHAR high_surrogate;
    SDL_bool initializing;
    SDL_bool expected_resize;
    SDL_bool in_border_change;
    SDL_bool in_title_click;
    SDL_bool floating_rect_pending;
    Uint8 focus_click_pending;
    SDL_bool skip_update_clipcursor;
    Uint64 last_updated_clipcursor;
    SDL_bool mouse_relative_mode_center;
    SDL_bool windowed_mode_was_maximized;
    SDL_bool in_window_deactivation;
    RECT cursor_clipped_rect;
    UINT windowed_mode_corner_rounding;
    COLORREF dwma_border_color;
    SDL_bool mouse_tracked;
    SDL_bool destroy_parent_with_window;
    SDL_DisplayID last_displayID;
    WCHAR *ICMFileName;
    SDL_Window *keyboard_focus;
    SDL_WindowEraseBackgroundMode hint_erase_background_mode;
    struct SDL_VideoData *videodata;
#ifdef SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif

    /* Whether we retain the content of the window when changing state */
    UINT copybits_flag;
};

extern int WIN_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props);
extern void WIN_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window);
extern int WIN_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon);
extern int WIN_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window);
extern int WIN_GetWindowBordersSize(SDL_VideoDevice *_this, SDL_Window *window, int *top, int *left, int *bottom, int *right);
extern void WIN_GetWindowSizeInPixels(SDL_VideoDevice *_this, SDL_Window *window, int *width, int *height);
extern int WIN_SetWindowOpacity(SDL_VideoDevice *_this, SDL_Window *window, float opacity);
extern void WIN_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_HideWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered);
extern void WIN_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable);
extern void WIN_SetWindowAlwaysOnTop(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool on_top);
extern int WIN_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen);
extern void WIN_UpdateWindowICCProfile(SDL_Window *window, SDL_bool send_event);
extern void *WIN_GetWindowICCProfile(SDL_VideoDevice *_this, SDL_Window *window, size_t *size);
extern int WIN_SetWindowMouseRect(SDL_VideoDevice *_this, SDL_Window *window);
extern int WIN_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed);
extern int WIN_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed);
extern void WIN_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_OnWindowEnter(SDL_VideoDevice *_this, SDL_Window *window);
extern void WIN_UpdateClipCursor(SDL_Window *window);
extern int WIN_SetWindowHitTest(SDL_Window *window, SDL_bool enabled);
extern void WIN_AcceptDragAndDrop(SDL_Window *window, SDL_bool accept);
extern int WIN_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation);
extern void WIN_UpdateDarkModeForHWND(HWND hwnd);
extern int WIN_SetWindowPositionInternal(SDL_Window *window, UINT flags, SDL_WindowRect rect_type);
extern void WIN_ShowWindowSystemMenu(SDL_Window *window, int x, int y);
extern int WIN_SetWindowFocusable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool focusable);
extern int WIN_AdjustWindowRect(SDL_Window *window, int *x, int *y, int *width, int *height, SDL_WindowRect rect_type);
extern int WIN_AdjustWindowRectForHWND(HWND hwnd, LPRECT lpRect, UINT frame_dpi);
extern int WIN_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_windowswindow_h_ */
