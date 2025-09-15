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

#include "../../SDL_list.h"
#include "SDL_x11video.h"
#include "SDL_x11dyn.h"
#include "SDL_x11settings.h"
#include "SDL_x11toolkit.h"
#include "xsettings-client.h"

#ifdef SDL_VIDEO_DRIVER_X11

/* Various predefined paddings */
#define SDL_TOOLKIT_X11_ELEMENT_PADDING 4
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_2 12
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_3 8
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_4 16
#define SDL_TOOLKIT_X11_ELEMENT_PADDING_5 3

typedef enum SDL_ToolkitChildModeX11
{
    SDL_TOOLKIT_WINDOW_MODE_X11_DIALOG,
    SDL_TOOLKIT_WINDOW_MODE_X11_CHILD, /* For embedding into a normal SDL_Window */
    SDL_TOOLKIT_WINDOW_MODE_X11_MENU,
    SDL_TOOLKIT_WINDOW_MODE_X11_TOOLTIP
} SDL_ToolkitWindowModeX11;

typedef struct SDL_ToolkitWindowX11
{
    /* Locale */
    char *origlocale;

    /* Mode */
    SDL_ToolkitWindowModeX11 mode;

    /* Display */
    Display *display;
    int screen;
    bool display_close;

    /* Parent */
    SDL_VideoDevice *parent_device;
    SDL_Window *parent;
    struct SDL_ToolkitWindowX11 *tk_parent;

    /* Window */
    Window window;
    Drawable drawable;
#ifndef NO_SHARED_MEMORY
	XImage *image;
	XShmSegmentInfo shm_info;
	int shm_bytes_per_line;
#endif

    /* Visuals and drawing */
    Visual *visual;
    XVisualInfo vi;
    Colormap cmap;
    GC ctx;
    int depth;
    bool pixmap;

    /* X11 extensions */
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    XdbeBackBuffer buf;
    bool xdbe; // Whether Xdbe is present or not
#endif
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    bool xrandr; // Whether Xrandr is present or not
#endif
#ifndef NO_SHARED_MEMORY
	bool shm;
	Bool shm_pixmap;
#endif
    bool utf8;

    /* Atoms */
    Atom wm_protocols;
    Atom wm_delete_message;

    /* Window and pixmap sizes */
    int window_width;  // Window width.
    int window_height; // Window height.
    int pixmap_width;  
    int pixmap_height;
    int window_x;
    int window_y;

    /* XSettings and scaling */
    XSettingsClient *xsettings;
    bool xsettings_first_time;
    int iscale;
    float scale;

    /* Font */
    XFontSet font_set;        // for UTF-8 systems
    XFontStruct *font_struct; // Latin1 (ASCII) fallback.

    /* Control colors */
    const SDL_MessageBoxColor *color_hints;
    XColor xcolor[SDL_MESSAGEBOX_COLOR_COUNT];
    XColor xcolor_bevel_l1;
    XColor xcolor_bevel_l2;
    XColor xcolor_bevel_d;
    XColor xcolor_pressed;
    XColor xcolor_disabled_text;

    /* Control list */
    bool has_focus;
    struct SDL_ToolkitControlX11 *focused_control;  
    struct SDL_ToolkitControlX11 *fiddled_control;
    struct SDL_ToolkitControlX11 **controls;
    size_t controls_sz;  
    struct SDL_ToolkitControlX11 **dyn_controls;
    size_t dyn_controls_sz;

    /* User callbacks */
    void *cb_data;
    void (*cb_on_scale_change)(struct SDL_ToolkitWindowX11 *, void *);

    /* Popup windows */
    SDL_ListNode *popup_windows;

    /* Event loop */
    XEvent *e;
    struct SDL_ToolkitControlX11 *previous_control;
    struct SDL_ToolkitControlX11 *key_control_esc;
    struct SDL_ToolkitControlX11 *key_control_enter;
    KeySym last_key_pressed;
    int ev_i;
    float ev_scale;
    float ev_iscale;
    bool draw;
    bool close;
    long event_mask;
} SDL_ToolkitWindowX11;

typedef enum SDL_ToolkitControlStateX11
{
    SDL_TOOLKIT_CONTROL_STATE_X11_NORMAL,
    SDL_TOOLKIT_CONTROL_STATE_X11_HOVER,
    SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED, /* Key/Button Up */
    SDL_TOOLKIT_CONTROL_STATE_X11_PRESSED_HELD, /* Key/Button Down */
    SDL_TOOLKIT_CONTROL_STATE_X11_DISABLED
} SDL_ToolkitControlStateX11;

typedef struct SDL_ToolkitControlX11
{
    SDL_ToolkitWindowX11 *window;
    SDL_ToolkitControlStateX11 state;
    SDL_Rect rect;
    bool selected;
    bool dynamic;
    bool is_default_enter;
    bool is_default_esc;

    /* User data */
    void *data;

    /* Virtual functions */
    void (*func_draw)(struct SDL_ToolkitControlX11 *);
    void (*func_calc_size)(struct SDL_ToolkitControlX11 *);
    void (*func_on_scale_change)(struct SDL_ToolkitControlX11 *);
    void (*func_on_state_change)(struct SDL_ToolkitControlX11 *);
    void (*func_free)(struct SDL_ToolkitControlX11 *);
} SDL_ToolkitControlX11;

typedef struct SDL_ToolkitMenuItemX11
{
    const char *utf8;
    bool checkbox;
    bool checked;
    bool disabled;
    void *cb_data;
    void (*cb)(struct SDL_ToolkitMenuItemX11 *, void *);
    SDL_ListNode *sub_menu;

    /* Internal use */
    SDL_Rect utf8_rect;
    SDL_Rect hover_rect;
    SDL_Rect check_rect;
    SDL_ToolkitControlStateX11 state;
    int arrow_x;
    int arrow_y;
    bool reverse_arrows;
} SDL_ToolkitMenuItemX11;

/* WINDOW FUNCTIONS */
extern SDL_ToolkitWindowX11 *X11Toolkit_CreateWindowStruct(SDL_Window *parent, SDL_ToolkitWindowX11 *tkparent, SDL_ToolkitWindowModeX11 mode, const SDL_MessageBoxColor *colorhints, bool create_new_display);
extern bool X11Toolkit_CreateWindowRes(SDL_ToolkitWindowX11 *data, int w, int h, int cx, int cy, char *title);
extern void X11Toolkit_DoWindowEventLoop(SDL_ToolkitWindowX11 *data);
extern void X11Toolkit_ResizeWindow(SDL_ToolkitWindowX11 *data, int w, int h);
extern void X11Toolkit_DestroyWindow(SDL_ToolkitWindowX11 *data);
extern void X11Toolkit_SignalWindowClose(SDL_ToolkitWindowX11 *data);

/* GENERIC CONTROL FUNCTIONS */
extern bool X11Toolkit_NotifyControlOfSizeChange(SDL_ToolkitControlX11 *control);

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
