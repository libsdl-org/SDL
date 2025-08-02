/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_x11toolkit_h_
#define SDL_x11toolkit_h_

#include "SDL_x11video.h"
#include "SDL_x11dyn.h"
#include "SDL_x11toolkit.h"

#ifdef SDL_VIDEO_DRIVER_X11

#define SDL_TOOLKIT_X11_ELEMENT_PADDING 4
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_2 12
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_3 8

typedef struct SDL_ToolkitWindowX11
{
    char *origlocale;

    SDL_Window *parent;
    Display *display;
    int screen;
    Window window;
    Drawable drawable;
    Visual *visual;
    Colormap cmap;
    GC ctx;
    bool utf8;
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    XdbeBackBuffer buf;
    bool xdbe; // Whether Xdbe is present or not
#endif
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    bool xrandr; // Whether Xrandr is present or not
#endif
    long event_mask;
    Atom wm_protocols;
    Atom wm_delete_message;
    bool close;
    
    int window_width;  // Window width.
    int window_height; // Window height.

    XFontSet font_set;        // for UTF-8 systems
    XFontStruct *font_struct; // Latin1 (ASCII) fallback.

    XColor xcolor[SDL_MESSAGEBOX_COLOR_COUNT];
    XColor xcolor_bevel_l1;
    XColor xcolor_bevel_l2;
    XColor xcolor_bevel_d;
    XColor xcolor_pressed;
        
    bool has_focus;
    struct SDL_ToolkitControlX11 *focused_control;
    
    struct SDL_ToolkitControlX11 **controls;
    size_t controls_sz;
} SDL_ToolkitWindowX11;

typedef enum SDL_ToolkitControlStateX11
{
    SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL,
    SDL_TOOLKIT_CONTROL_STATE_X11_HOVER,
    SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED,
} SDL_ToolkitControlStateX11;

typedef struct SDL_ToolkitControlX11
{
    SDL_ToolkitControlStateX11 state;
    SDL_ToolkitWindowX11 *window;
    SDL_Rect rect;
    bool selected;
    bool dynamic;
    bool is_default_enter;
    bool is_default_esc;
    void *data;
    void (*func_draw)(struct SDL_ToolkitControlX11 *);
    void (*func_upsize)(struct SDL_ToolkitControlX11 *);
    void (*func_on_state_change)(struct SDL_ToolkitControlX11 *);
    void (*func_free)(struct SDL_ToolkitControlX11 *);
} SDL_ToolkitControlX11;

/* WINDOW FUNCTIONS */
extern SDL_ToolkitWindowX11 *X11Toolkit_CreateWindow(SDL_Window *parent, const SDL_MessageBoxColor *colorhints, int w, int h, char *title);
extern SDL_ToolkitWindowX11 *X11Toolkit_CreateWindowStruct(SDL_Window *parent, const SDL_MessageBoxColor *colorhints);
extern bool X11Toolkit_CreateWindowRes(SDL_ToolkitWindowX11 *data, int w, int h, char *title);
extern void X11Toolkit_DoWindowEventLoop(SDL_ToolkitWindowX11 *data);
extern void X11Toolkit_ResizeWindow(SDL_ToolkitWindowX11 *data, int w, int h);
extern void X11Toolkit_DestroyWindow(SDL_ToolkitWindowX11 *data);
extern void X11Toolkit_SignalWindowClose(SDL_ToolkitWindowX11 *data);

/* GENERIC CONTROL FUNCTIONS */
extern bool X11Toolkit_NotifyUpsize(SDL_ToolkitControlX11 *control);

/* ICON CONTROL FUNCTIONS */
extern SDL_ToolkitControlX11 *X11Toolkit_CreateIconControl(SDL_ToolkitWindowX11 *window, SDL_MessageBoxFlags flags);
extern int X11Toolkit_GetIconControlCharY(SDL_ToolkitControlX11 *control);

/* LABEL CONTROL FUNCTIONS */
extern SDL_ToolkitControlX11 *X11Toolkit_CreateLabelControl(SDL_ToolkitWindowX11 *window, char *utf8);

/* BUTTON CONTROL FUNCTIONS */
extern SDL_ToolkitControlX11 *X11Toolkit_CreateButtonControl(SDL_ToolkitWindowX11 *window, const SDL_MessageBoxButtonData *data);
extern void X11Toolkit_RegisterCallbackForButtonControl(SDL_ToolkitControlX11 *control, void *data, void (*cb)(struct SDL_ToolkitControlX11 *, void *));
extern const SDL_MessageBoxButtonData *X11Toolkit_GetButtonControlData(SDL_ToolkitControlX11 *control);

#endif // SDL_VIDEO_DRIVER_X11

#endif // SDL_x11toolkit_h_
