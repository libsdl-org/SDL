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

#ifdef SDL_VIDEO_DRIVER_X11

#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_events_c.h"
#include "../../core/unix/SDL_appid.h"

#include "SDL_x11video.h"
#include "SDL_x11mouse.h"
#include "SDL_x11xinput2.h"
#include "SDL_x11xfixes.h"

#ifdef SDL_VIDEO_OPENGL_EGL
#include "SDL_x11opengles.h"
#endif

#define _NET_WM_STATE_REMOVE 0l
#define _NET_WM_STATE_ADD    1l

#define CHECK_WINDOW_DATA(window)                                       \
    if (!window) {                                                      \
        return SDL_SetError("Invalid window");                          \
    }                                                                   \
    if (!window->driverdata) {                                          \
        return SDL_SetError("Invalid window driver data");              \
    }

#define CHECK_DISPLAY_DATA(display)                                     \
    if (!_display) {                                                    \
        return SDL_SetError("Invalid display");                         \
    }                                                                   \
    if (!_display->driverdata) {                                        \
        return SDL_SetError("Invalid display driver data");             \
    }

static Bool isMapNotify(Display *dpy, XEvent *ev, XPointer win) /* NOLINT(readability-non-const-parameter): cannot make XPointer a const pointer due to typedef */
{
    return ev->type == MapNotify && ev->xmap.window == *((Window *)win);
}
static Bool isUnmapNotify(Display *dpy, XEvent *ev, XPointer win) /* NOLINT(readability-non-const-parameter): cannot make XPointer a const pointer due to typedef */
{
    return ev->type == UnmapNotify && ev->xunmap.window == *((Window *)win);
}

/*
static Bool isConfigureNotify(Display *dpy, XEvent *ev, XPointer win)
{
    return ev->type == ConfigureNotify && ev->xconfigure.window == *((Window*)win);
}
static Bool X11_XIfEventTimeout(Display *display, XEvent *event_return, Bool (*predicate)(), XPointer arg, int timeoutMS)
{
    Uint64 start = SDL_GetTicks();

    while (!X11_XCheckIfEvent(display, event_return, predicate, arg)) {
        if (SDL_GetTicks() >= (start + timeoutMS)) {
            return False;
        }
    }
    return True;
}
*/

static SDL_bool X11_IsWindowMapped(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    SDL_VideoData *videodata = _this->driverdata;
    XWindowAttributes attr;

    X11_XGetWindowAttributes(videodata->display, data->xwindow, &attr);
    if (attr.map_state != IsUnmapped) {
        return SDL_TRUE;
    } else {
        return SDL_FALSE;
    }
}

#if 0
static SDL_bool X11_IsActionAllowed(SDL_Window *window, Atom action)
{
    SDL_WindowData *data = window->driverdata;
    Atom _NET_WM_ALLOWED_ACTIONS = data->videodata->_NET_WM_ALLOWED_ACTIONS;
    Atom type;
    Display *display = data->videodata->display;
    int form;
    unsigned long remain;
    unsigned long len, i;
    Atom *list;
    SDL_bool ret = SDL_FALSE;

    if (X11_XGetWindowProperty(display, data->xwindow, _NET_WM_ALLOWED_ACTIONS, 0, 1024, False, XA_ATOM, &type, &form, &len, &remain, (unsigned char **)&list) == Success) {
        for (i=0; i<len; ++i) {
            if (list[i] == action) {
                ret = SDL_TRUE;
                break;
            }
        }
        X11_XFree(list);
    }
    return ret;
}
#endif /* 0 */

void X11_SetNetWMState(SDL_VideoDevice *_this, Window xwindow, SDL_WindowFlags flags)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display = videodata->display;
    /* !!! FIXME: just dereference videodata below instead of copying to locals. */
    Atom _NET_WM_STATE = videodata->_NET_WM_STATE;
    /* Atom _NET_WM_STATE_HIDDEN = videodata->_NET_WM_STATE_HIDDEN; */
    Atom _NET_WM_STATE_FOCUSED = videodata->_NET_WM_STATE_FOCUSED;
    Atom _NET_WM_STATE_MAXIMIZED_VERT = videodata->_NET_WM_STATE_MAXIMIZED_VERT;
    Atom _NET_WM_STATE_MAXIMIZED_HORZ = videodata->_NET_WM_STATE_MAXIMIZED_HORZ;
    Atom _NET_WM_STATE_FULLSCREEN = videodata->_NET_WM_STATE_FULLSCREEN;
    Atom _NET_WM_STATE_ABOVE = videodata->_NET_WM_STATE_ABOVE;
    Atom _NET_WM_STATE_SKIP_TASKBAR = videodata->_NET_WM_STATE_SKIP_TASKBAR;
    Atom _NET_WM_STATE_SKIP_PAGER = videodata->_NET_WM_STATE_SKIP_PAGER;
    Atom _NET_WM_STATE_MODAL = videodata->_NET_WM_STATE_MODAL;
    Atom atoms[16];
    int count = 0;

    /* The window manager sets this property, we shouldn't set it.
       If we did, this would indicate to the window manager that we don't
       actually want to be mapped during X11_XMapRaised(), which would be bad.
     *
    if ((flags & SDL_WINDOW_HIDDEN) != 0) {
        atoms[count++] = _NET_WM_STATE_HIDDEN;
    }
    */

    if (flags & SDL_WINDOW_ALWAYS_ON_TOP) {
        atoms[count++] = _NET_WM_STATE_ABOVE;
    }
    if (flags & SDL_WINDOW_UTILITY) {
        atoms[count++] = _NET_WM_STATE_SKIP_TASKBAR;
        atoms[count++] = _NET_WM_STATE_SKIP_PAGER;
    }
    if (flags & SDL_WINDOW_INPUT_FOCUS) {
        atoms[count++] = _NET_WM_STATE_FOCUSED;
    }
    if (flags & SDL_WINDOW_MAXIMIZED) {
        atoms[count++] = _NET_WM_STATE_MAXIMIZED_VERT;
        atoms[count++] = _NET_WM_STATE_MAXIMIZED_HORZ;
    }
    if (flags & SDL_WINDOW_FULLSCREEN) {
        atoms[count++] = _NET_WM_STATE_FULLSCREEN;
    }
    if (flags & SDL_WINDOW_MODAL) {
        atoms[count++] = _NET_WM_STATE_MODAL;
    }

    SDL_assert(count <= SDL_arraysize(atoms));

    if (count > 0) {
        X11_XChangeProperty(display, xwindow, _NET_WM_STATE, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)atoms, count);
    } else {
        X11_XDeleteProperty(display, xwindow, _NET_WM_STATE);
    }
}

static void X11_ConstrainPopup(SDL_Window *window)
{
    /* Clamp popup windows to the output borders */
    if (SDL_WINDOW_IS_POPUP(window)) {
        SDL_Window *w;
        SDL_DisplayID displayID;
        SDL_Rect rect;
        int abs_x = window->floating.x;
        int abs_y = window->floating.y;
        int offset_x = 0, offset_y = 0;

        /* Calculate the total offset from the parents */
        for (w = window->parent; w->parent; w = w->parent) {
            offset_x += w->x;
            offset_y += w->y;
        }

        offset_x += w->x;
        offset_y += w->y;
        abs_x += offset_x;
        abs_y += offset_y;

        displayID = SDL_GetDisplayForWindow(w);

        SDL_GetDisplayBounds(displayID, &rect);
        if (abs_x + window->w > rect.x + rect.w) {
            abs_x -= (abs_x + window->w) - (rect.x + rect.w);
        }
        if (abs_y + window->h > rect.y + rect.h) {
            abs_y -= (abs_y + window->h) - (rect.y + rect.h);
        }
        abs_x = SDL_max(abs_x, rect.x);
        abs_y = SDL_max(abs_y, rect.y);

        window->floating.x = window->windowed.x = abs_x - offset_x;
        window->floating.y = window->windowed.y = abs_y - offset_y;
    }
}

static void X11_SetKeyboardFocus(SDL_Window *window)
{
    SDL_Window *topmost = window;

    /* Find the topmost parent */
    while (topmost->parent) {
        topmost = topmost->parent;
    }

    topmost->driverdata->keyboard_focus = window;
    SDL_SetKeyboardFocus(window);
}

Uint32 X11_GetNetWMState(SDL_VideoDevice *_this, SDL_Window *window, Window xwindow)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display = videodata->display;
    Atom _NET_WM_STATE = videodata->_NET_WM_STATE;
    Atom _NET_WM_STATE_HIDDEN = videodata->_NET_WM_STATE_HIDDEN;
    Atom _NET_WM_STATE_FOCUSED = videodata->_NET_WM_STATE_FOCUSED;
    Atom _NET_WM_STATE_MAXIMIZED_VERT = videodata->_NET_WM_STATE_MAXIMIZED_VERT;
    Atom _NET_WM_STATE_MAXIMIZED_HORZ = videodata->_NET_WM_STATE_MAXIMIZED_HORZ;
    Atom _NET_WM_STATE_FULLSCREEN = videodata->_NET_WM_STATE_FULLSCREEN;
    Atom actualType;
    int actualFormat;
    unsigned long i, numItems, bytesAfter;
    unsigned char *propertyValue = NULL;
    long maxLength = 1024;
    SDL_WindowFlags flags = 0;

    if (X11_XGetWindowProperty(display, xwindow, _NET_WM_STATE,
                               0l, maxLength, False, XA_ATOM, &actualType,
                               &actualFormat, &numItems, &bytesAfter,
                               &propertyValue) == Success) {
        Atom *atoms = (Atom *)propertyValue;
        int maximized = 0;
        int fullscreen = 0;

        for (i = 0; i < numItems; ++i) {
            if (atoms[i] == _NET_WM_STATE_HIDDEN) {
                flags |= SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED;
            } else if (atoms[i] == _NET_WM_STATE_FOCUSED) {
                flags |= SDL_WINDOW_INPUT_FOCUS;
            } else if (atoms[i] == _NET_WM_STATE_MAXIMIZED_VERT) {
                maximized |= 1;
            } else if (atoms[i] == _NET_WM_STATE_MAXIMIZED_HORZ) {
                maximized |= 2;
            } else if (atoms[i] == _NET_WM_STATE_FULLSCREEN) {
                fullscreen = 1;
            }
        }

        if (fullscreen == 1) {
            if (window->flags & SDL_WINDOW_FULLSCREEN) {
                /* Pick whatever state the window expects */
                flags |= (window->flags & SDL_WINDOW_FULLSCREEN);
            } else {
                /* Assume we're fullscreen desktop */
                flags |= SDL_WINDOW_FULLSCREEN;
            }
        }

        if (maximized == 3) {
            /* Fullscreen windows are maximized on some window managers,
               and this is functional behavior - if maximized is removed,
               the windows remain floating centered and not covering the
               rest of the desktop. So we just won't change the maximize
               state for fullscreen windows here, otherwise SDL would think
               we're always maximized when fullscreen and not restore the
               correct state when leaving fullscreen.
            */
            if (fullscreen) {
                flags |= (window->flags & SDL_WINDOW_MAXIMIZED);
            } else {
                flags |= SDL_WINDOW_MAXIMIZED;
            }
        }

        /* If the window is unmapped, numItems will be zero and _NET_WM_STATE_HIDDEN
         * will not be set. Do an additional check to see if the window is unmapped
         * and mark it as SDL_WINDOW_HIDDEN if it is.
         */
        {
            XWindowAttributes attr;
            SDL_memset(&attr, 0, sizeof(attr));
            X11_XGetWindowAttributes(videodata->display, xwindow, &attr);
            if (attr.map_state == IsUnmapped) {
                flags |= SDL_WINDOW_HIDDEN;
            }
        }
        X11_XFree(propertyValue);
    }

    /* FIXME, check the size hints for resizable */
    /* flags |= SDL_WINDOW_RESIZABLE; */

    return flags;
}

static int SetupWindowData(SDL_VideoDevice *_this, SDL_Window *window, Window w)
{
    SDL_VideoData *videodata = _this->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    SDL_WindowData *data;
    int numwindows = videodata->numwindows;
    int windowlistlength = videodata->windowlistlength;
    SDL_WindowData **windowlist = videodata->windowlist;

    /* Allocate the window data */
    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return -1;
    }
    data->window = window;
    data->xwindow = w;
    data->hit_test_result = SDL_HITTEST_NORMAL;

#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8 && videodata->im) {
        data->ic =
            X11_XCreateIC(videodata->im, XNClientWindow, w, XNFocusWindow, w,
                          XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                          NULL);
    }
#endif
    data->videodata = videodata;

    /* Associate the data with the window */

    if (numwindows < windowlistlength) {
        windowlist[numwindows] = data;
        videodata->numwindows++;
    } else {
        SDL_WindowData ** new_windowlist = (SDL_WindowData **)SDL_realloc(windowlist, (numwindows + 1) * sizeof(*windowlist));
        if (!new_windowlist) {
            SDL_free(data);
            return -1;
        }
        windowlist = new_windowlist;
        windowlist[numwindows] = data;
        videodata->numwindows++;
        videodata->windowlistlength++;
        videodata->windowlist = windowlist;
    }

    /* Fill in the SDL window with the window data */
    {
        XWindowAttributes attrib;

        X11_XGetWindowAttributes(data->videodata->display, w, &attrib);
        if (!SDL_WINDOW_IS_POPUP(window)) {
            window->x = data->expected.x = attrib.x;
            window->y = data->expected.y = attrib.y - data->border_top;
        }
        window->w = data->expected.w = attrib.width;
        window->h = data->expected.h = attrib.height;
        if (attrib.map_state != IsUnmapped) {
            window->flags &= ~SDL_WINDOW_HIDDEN;
        } else {
            window->flags |= SDL_WINDOW_HIDDEN;
        }
        data->visual = attrib.visual;
        data->colormap = attrib.colormap;
    }

    window->flags |= X11_GetNetWMState(_this, window, w);

    {
        Window FocalWindow;
        int RevertTo = 0;
        X11_XGetInputFocus(data->videodata->display, &FocalWindow, &RevertTo);
        if (FocalWindow == w) {
            window->flags |= SDL_WINDOW_INPUT_FOCUS;
        }

        if (window->flags & SDL_WINDOW_INPUT_FOCUS) {
            SDL_SetKeyboardFocus(data->window);
        }

        if (window->flags & SDL_WINDOW_MOUSE_GRABBED) {
            /* Tell x11 to clip mouse */
        }
    }

    if (window->flags & SDL_WINDOW_EXTERNAL) {
        /* Query the title from the existing window */
        window->title = X11_GetWindowTitle(_this, w);
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    int screen = (displaydata ? displaydata->screen : 0);
    SDL_SetProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, data->videodata->display);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_X11_SCREEN_NUMBER, screen);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, data->xwindow);

    /* All done! */
    window->driverdata = data;
    return 0;
}

static void SetWindowBordered(Display *display, int screen, Window window, SDL_bool border)
{
    /*
     * this code used to check for KWM_WIN_DECORATION, but KDE hasn't
     *  supported it for years and years. It now respects _MOTIF_WM_HINTS.
     *  Gnome is similar: just use the Motif atom.
     */

    Atom WM_HINTS = X11_XInternAtom(display, "_MOTIF_WM_HINTS", True);
    if (WM_HINTS != None) {
        /* Hints used by Motif compliant window managers */
        struct
        {
            unsigned long flags;
            unsigned long functions;
            unsigned long decorations;
            long input_mode;
            unsigned long status;
        } MWMHints = {
            (1L << 1), 0, border ? 1 : 0, 0, 0
        };

        X11_XChangeProperty(display, window, WM_HINTS, WM_HINTS, 32,
                            PropModeReplace, (unsigned char *)&MWMHints,
                            sizeof(MWMHints) / sizeof(long));
    } else { /* set the transient hints instead, if necessary */
        X11_XSetTransientForHint(display, window, RootWindow(display, screen));
    }
}

int X11_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    Window w = (Window)SDL_GetNumberProperty(create_props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                (Window)SDL_GetProperty(create_props, "sdl2-compat.external_window", NULL));
    if (w) {
        window->flags |= SDL_WINDOW_EXTERNAL;

        if (SetupWindowData(_this, window, w) < 0) {
            return -1;
        }
        return 0;
    }

    SDL_VideoData *data = _this->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    if (!displaydata) {
        return SDL_SetError("Could not find display info");
    }

    const SDL_bool force_override_redirect = SDL_GetHintBoolean(SDL_HINT_X11_FORCE_OVERRIDE_REDIRECT, SDL_FALSE);
    SDL_WindowData *windowdata;
    Display *display = data->display;
    int screen = displaydata->screen;
    Visual *visual;
    int depth;
    XSetWindowAttributes xattr;
    XSizeHints *sizehints;
    XWMHints *wmhints;
    XClassHint *classhints;
    Atom _NET_WM_BYPASS_COMPOSITOR;
    Atom _NET_WM_WINDOW_TYPE;
    Atom wintype;
    const char *wintype_name = NULL;
    long compositor = 1;
    Atom _NET_WM_PID;
    long fevent = 0;
    const char *hint = NULL;
    int win_x, win_y;
    SDL_bool undefined_position = SDL_FALSE;

#if defined(SDL_VIDEO_OPENGL_GLX) || defined(SDL_VIDEO_OPENGL_EGL)
    const int transparent = (window->flags & SDL_WINDOW_TRANSPARENT) ? SDL_TRUE : SDL_FALSE;
    const char *forced_visual_id = SDL_GetHint(SDL_HINT_VIDEO_X11_WINDOW_VISUALID);

    if (forced_visual_id && forced_visual_id[0] != '\0') {
        XVisualInfo *vi, template;
        int nvis;

        SDL_zero(template);
        template.visualid = SDL_strtol(forced_visual_id, NULL, 0);
        vi = X11_XGetVisualInfo(display, VisualIDMask, &template, &nvis);
        if (vi) {
            visual = vi->visual;
            depth = vi->depth;
            X11_XFree(vi);
        } else {
            return -1;
        }
    } else if ((window->flags & SDL_WINDOW_OPENGL) &&
               !SDL_getenv("SDL_VIDEO_X11_VISUALID")) {
        XVisualInfo *vinfo = NULL;

#ifdef SDL_VIDEO_OPENGL_EGL
        if (((_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) ||
             SDL_GetHintBoolean(SDL_HINT_VIDEO_FORCE_EGL, SDL_FALSE))
#ifdef SDL_VIDEO_OPENGL_GLX
            && (!_this->gl_data || X11_GL_UseEGL(_this))
#endif
        ) {
            vinfo = X11_GLES_GetVisual(_this, display, screen, transparent);
        } else
#endif
        {
#ifdef SDL_VIDEO_OPENGL_GLX
            vinfo = X11_GL_GetVisual(_this, display, screen, transparent);
#endif
        }

        if (!vinfo) {
            return -1;
        }
        visual = vinfo->visual;
        depth = vinfo->depth;
        X11_XFree(vinfo);
    } else
#endif
    {
        visual = displaydata->visual;
        depth = displaydata->depth;
    }

    xattr.override_redirect = ((window->flags & SDL_WINDOW_TOOLTIP) || (window->flags & SDL_WINDOW_POPUP_MENU) || force_override_redirect) ? True : False;
    xattr.backing_store = NotUseful;
    xattr.background_pixmap = None;
    xattr.border_pixel = 0;

    if (visual->class == DirectColor) {
        XColor *colorcells;
        int i;
        int ncolors;
        int rmax, gmax, bmax;
        int rmask, gmask, bmask;
        int rshift, gshift, bshift;

        xattr.colormap =
            X11_XCreateColormap(display, RootWindow(display, screen),
                                visual, AllocAll);

        /* If we can't create a colormap, then we must die */
        if (!xattr.colormap) {
            return SDL_SetError("Could not create writable colormap");
        }

        /* OK, we got a colormap, now fill it in as best as we can */
        colorcells = SDL_malloc(visual->map_entries * sizeof(XColor));
        if (!colorcells) {
            return -1;
        }
        ncolors = visual->map_entries;
        rmax = 0xffff;
        gmax = 0xffff;
        bmax = 0xffff;

        rshift = 0;
        rmask = visual->red_mask;
        while (0 == (rmask & 1)) {
            rshift++;
            rmask >>= 1;
        }

        gshift = 0;
        gmask = visual->green_mask;
        while (0 == (gmask & 1)) {
            gshift++;
            gmask >>= 1;
        }

        bshift = 0;
        bmask = visual->blue_mask;
        while (0 == (bmask & 1)) {
            bshift++;
            bmask >>= 1;
        }

        /* build the color table pixel values */
        for (i = 0; i < ncolors; i++) {
            Uint32 red = (rmax * i) / (ncolors - 1);
            Uint32 green = (gmax * i) / (ncolors - 1);
            Uint32 blue = (bmax * i) / (ncolors - 1);

            Uint32 rbits = (rmask * i) / (ncolors - 1);
            Uint32 gbits = (gmask * i) / (ncolors - 1);
            Uint32 bbits = (bmask * i) / (ncolors - 1);

            Uint32 pix =
                (rbits << rshift) | (gbits << gshift) | (bbits << bshift);

            colorcells[i].pixel = pix;

            colorcells[i].red = red;
            colorcells[i].green = green;
            colorcells[i].blue = blue;

            colorcells[i].flags = DoRed | DoGreen | DoBlue;
        }

        X11_XStoreColors(display, xattr.colormap, colorcells, ncolors);

        SDL_free(colorcells);
    } else {
        xattr.colormap =
            X11_XCreateColormap(display, RootWindow(display, screen),
                                visual, AllocNone);
    }

    if (window->undefined_x && window->undefined_y &&
        window->last_displayID == SDL_GetPrimaryDisplay()) {
        undefined_position = SDL_TRUE;
    }

    if (SDL_WINDOW_IS_POPUP(window)) {
        X11_ConstrainPopup(window);
    }
    SDL_RelativeToGlobalForWindow(window,
                                  window->floating.x, window->floating.y,
                                  &win_x, &win_y);

    /* Always create this with the window->floating.* fields; if we're creating a windowed mode window,
     * that's fine. If we're creating a maximized or fullscreen window, the window manager will want to
     * know these values so it can use them if we go _back_ to the base floating windowed mode. SDL manages
     * migration to fullscreen after CreateSDLWindow returns, which will put all the SDL_Window fields and
     * system state as expected.
     */
    w = X11_XCreateWindow(display, RootWindow(display, screen),
                          win_x, win_y, window->floating.w, window->floating.h,
                          0, depth, InputOutput, visual,
                          (CWOverrideRedirect | CWBackPixmap | CWBorderPixel |
                           CWBackingStore | CWColormap),
                          &xattr);
    if (!w) {
        return SDL_SetError("Couldn't create window");
    }

    /* Don't set the borderless flag if we're about to go fullscreen.
     * This prevents the window manager from moving a full-screen borderless
     * window to a different display before we actually go fullscreen.
     */
    if (!(window->pending_flags & SDL_WINDOW_FULLSCREEN)) {
        SetWindowBordered(display, screen, w, !(window->flags & SDL_WINDOW_BORDERLESS));
    }

    sizehints = X11_XAllocSizeHints();
    /* Setup the normal size hints */
    sizehints->flags = 0;
    if (!(window->flags & SDL_WINDOW_RESIZABLE)) {
        sizehints->min_width = sizehints->max_width = window->floating.w;
        sizehints->min_height = sizehints->max_height = window->floating.h;
        sizehints->flags |= (PMaxSize | PMinSize);
    }
    if (!undefined_position) {
        sizehints->x = win_x;
        sizehints->y = win_y;
        sizehints->flags |= USPosition;
    }

    /* Setup the input hints so we get keyboard input */
    wmhints = X11_XAllocWMHints();
    wmhints->input = !(window->flags & SDL_WINDOW_NOT_FOCUSABLE) ? True : False;
    wmhints->window_group = data->window_group;
    wmhints->flags = InputHint | WindowGroupHint;

    /* Setup the class hints so we can get an icon (AfterStep) */
    classhints = X11_XAllocClassHint();
    classhints->res_name = (char *)SDL_GetExeName();
    classhints->res_class = (char *)SDL_GetAppID();

    /* Set the size, input and class hints, and define WM_CLIENT_MACHINE and WM_LOCALE_NAME */
    X11_XSetWMProperties(display, w, NULL, NULL, NULL, 0, sizehints, wmhints, classhints);

    X11_XFree(sizehints);
    X11_XFree(wmhints);
    X11_XFree(classhints);
    /* Set the PID related to the window for the given hostname, if possible */
    if (data->pid > 0) {
        long pid = (long)data->pid;
        _NET_WM_PID = X11_XInternAtom(display, "_NET_WM_PID", False);
        X11_XChangeProperty(display, w, _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace,
                            (unsigned char *)&pid, 1);
    }

    /* Set the window manager state */
    X11_SetNetWMState(_this, w, window->flags);

    compositor = 2; /* don't disable compositing except for "normal" windows */
    hint = SDL_GetHint(SDL_HINT_X11_WINDOW_TYPE);
    if (window->flags & SDL_WINDOW_UTILITY) {
        wintype_name = "_NET_WM_WINDOW_TYPE_UTILITY";
    } else if (window->flags & SDL_WINDOW_TOOLTIP) {
        wintype_name = "_NET_WM_WINDOW_TYPE_TOOLTIP";
    } else if (window->flags & SDL_WINDOW_POPUP_MENU) {
        wintype_name = "_NET_WM_WINDOW_TYPE_POPUP_MENU";
    } else if (hint && *hint) {
        wintype_name = hint;
    } else {
        wintype_name = "_NET_WM_WINDOW_TYPE_NORMAL";
        compositor = 1; /* disable compositing for "normal" windows */
    }

    /* Let the window manager know what type of window we are. */
    _NET_WM_WINDOW_TYPE = X11_XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    wintype = X11_XInternAtom(display, wintype_name, False);
    X11_XChangeProperty(display, w, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&wintype, 1);
    if (SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, SDL_TRUE)) {
        _NET_WM_BYPASS_COMPOSITOR = X11_XInternAtom(display, "_NET_WM_BYPASS_COMPOSITOR", False);
        X11_XChangeProperty(display, w, _NET_WM_BYPASS_COMPOSITOR, XA_CARDINAL, 32,
                            PropModeReplace,
                            (unsigned char *)&compositor, 1);
    }

    {
        Atom protocols[3];
        int proto_count = 0;

        protocols[proto_count++] = data->WM_DELETE_WINDOW; /* Allow window to be deleted by the WM */
        protocols[proto_count++] = data->WM_TAKE_FOCUS;    /* Since we will want to set input focus explicitly */

        /* Default to using ping if there is no hint */
        if (SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_NET_WM_PING, SDL_TRUE)) {
            protocols[proto_count++] = data->_NET_WM_PING; /* Respond so WM knows we're alive */
        }

        SDL_assert(proto_count <= sizeof(protocols) / sizeof(protocols[0]));

        X11_XSetWMProtocols(display, w, protocols, proto_count);
    }

    if (SetupWindowData(_this, window, w) < 0) {
        X11_XDestroyWindow(display, w);
        return -1;
    }
    windowdata = window->driverdata;

    /* Set the flag if the borders were forced on when creating a fullscreen window for later removal. */
    windowdata->fullscreen_borders_forced_on = !!(window->pending_flags & SDL_WINDOW_FULLSCREEN) &&
                                               !!(window->flags & SDL_WINDOW_BORDERLESS);

#if defined(SDL_VIDEO_OPENGL_ES) || defined(SDL_VIDEO_OPENGL_ES2) || defined(SDL_VIDEO_OPENGL_EGL)
    if ((window->flags & SDL_WINDOW_OPENGL) &&
        ((_this->gl_config.profile_mask == SDL_GL_CONTEXT_PROFILE_ES) ||
         SDL_GetHintBoolean(SDL_HINT_VIDEO_FORCE_EGL, SDL_FALSE))
#ifdef SDL_VIDEO_OPENGL_GLX
        && (!_this->gl_data || X11_GL_UseEGL(_this))
#endif
    ) {
#ifdef SDL_VIDEO_OPENGL_EGL
        if (!_this->egl_data) {
            return -1;
        }

        /* Create the GLES window surface */
        windowdata->egl_surface = SDL_EGL_CreateSurface(_this, window, (NativeWindowType)w);

        if (windowdata->egl_surface == EGL_NO_SURFACE) {
            return SDL_SetError("Could not create GLES window surface");
        }
#else
        return SDL_SetError("Could not create GLES window surface (EGL support not configured)");
#endif /* SDL_VIDEO_OPENGL_EGL */
    }
#endif

#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8 && windowdata->ic) {
        X11_XGetICValues(windowdata->ic, XNFilterEvents, &fevent, NULL);
    }
#endif

#ifdef SDL_VIDEO_DRIVER_X11_XSHAPE
    /* Tooltips do not receive input */
    if (window->flags & SDL_WINDOW_TOOLTIP) {
        Region region = X11_XCreateRegion();
        X11_XShapeCombineRegion(display, w, ShapeInput, 0, 0, region, ShapeSet);
        X11_XDestroyRegion(region);
    }
#endif

    X11_Xinput2SelectTouch(_this, window);

    {
        unsigned int x11_keyboard_events = KeyPressMask | KeyReleaseMask;
        unsigned int x11_pointer_events = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

        X11_Xinput2SelectMouseAndKeyboard(_this, window);

        /* If XInput2 can handle pointer and keyboard events, we don't track them here */
        if (windowdata->xinput2_keyboard_enabled) {
            x11_keyboard_events = 0;
        }
        if (windowdata->xinput2_mouse_enabled) {
            x11_pointer_events = 0;
        }

        X11_XSelectInput(display, w,
                         (FocusChangeMask | EnterWindowMask | LeaveWindowMask | ExposureMask |
                          x11_keyboard_events | x11_pointer_events |
                          PropertyChangeMask | StructureNotifyMask |
                          KeymapStateMask | fevent));
    }

    /* For _ICC_PROFILE. */
    X11_XSelectInput(display, RootWindow(display, screen), PropertyChangeMask);

    X11_XFlush(display);

    return 0;
}

char *X11_GetWindowTitle(SDL_VideoDevice *_this, Window xwindow)
{
    SDL_VideoData *data = _this->driverdata;
    Display *display = data->display;
    int status, real_format;
    Atom real_type;
    unsigned long items_read, items_left;
    unsigned char *propdata;
    char *title = NULL;

    status = X11_XGetWindowProperty(display, xwindow, data->_NET_WM_NAME,
                                    0L, 8192L, False, data->UTF8_STRING, &real_type, &real_format,
                                    &items_read, &items_left, &propdata);
    if (status == Success && propdata) {
        title = SDL_strdup(SDL_static_cast(char *, propdata));
        X11_XFree(propdata);
    } else {
        status = X11_XGetWindowProperty(display, xwindow, XA_WM_NAME,
                                        0L, 8192L, False, XA_STRING, &real_type, &real_format,
                                        &items_read, &items_left, &propdata);
        if (status == Success && propdata) {
            title = SDL_iconv_string("UTF-8", "", SDL_static_cast(char *, propdata), items_read + 1);
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Failed to convert WM_NAME title expecting UTF8! Title: %s", title);
            X11_XFree(propdata);
        } else {
            SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "Could not get any window title response from Xorg, returning empty string!");
            title = SDL_strdup("");
        }
    }
    return title;
}

void X11_SetWindowTitle(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    Window xwindow = data->xwindow;
    Display *display = data->videodata->display;
    char *title = window->title ? window->title : "";

    SDL_X11_SetWindowTitle(display, xwindow, title);
}

static SDL_bool caught_x11_error = SDL_FALSE;
static int X11_CatchAnyError(Display *d, XErrorEvent *e)
{
    /* this may happen during tumultuous times when we are polling anyhow,
        so just note we had an error and return control. */
    caught_x11_error = SDL_TRUE;
    return 0;
}

/* Wait a brief time, or not, to see if the window manager decided to move/resize the window.
 * Send MOVED and RESIZED window events */
static int X11_SyncWindowTimeout(SDL_VideoDevice *_this, SDL_Window *window, Uint64 param_timeout)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    int (*prev_handler)(Display *, XErrorEvent *);
    Uint64 timeout = 0;
    int ret = 0;
    SDL_bool force_exit = SDL_FALSE;

    X11_XSync(display, False);
    prev_handler = X11_XSetErrorHandler(X11_CatchAnyError);

    if (param_timeout) {
        timeout = SDL_GetTicksNS() + param_timeout;
    }

    while (SDL_TRUE) {
        X11_XSync(display, False);
        X11_PumpEvents(_this);

        if ((data->pending_operation & X11_PENDING_OP_MOVE) && (window->x == data->expected.x + data->border_left && window->y == data->expected.y + data->border_top)) {
            data->pending_operation &= ~X11_PENDING_OP_MOVE;
        }
        if ((data->pending_operation & X11_PENDING_OP_RESIZE) && (window->w == data->expected.w && window->h == data->expected.h)) {
            data->pending_operation &= ~X11_PENDING_OP_RESIZE;
        }

        if (data->pending_operation == X11_PENDING_OP_NONE) {
            if (force_exit ||
                (window->x == data->expected.x + data->border_left && window->y == data->expected.y + data->border_top &&
                 window->w == data->expected.w && window->h == data->expected.h)) {
                /* The window is in the expected state and nothing is pending. Done. */
                break;
            }

            /* No operations are pending, but the window still isn't in the expected state.
             * Try one more time before exiting.
             */
            force_exit = SDL_TRUE;
        }

        if (SDL_GetTicksNS() >= timeout) {
            /* Timed out without the expected values. Update the requested data so future sync calls won't block. */
            data->expected.x = window->x;
            data->expected.y = window->y;
            data->expected.w = window->w;
            data->expected.h = window->h;

            ret = 1;
            break;
        }

        SDL_Delay(10);
    }

    data->pending_operation = X11_PENDING_OP_NONE;

    if (!caught_x11_error) {
        X11_PumpEvents(_this);
    } else {
        ret = -1;
    }

    X11_XSetErrorHandler(prev_handler);
    caught_x11_error = SDL_FALSE;

    return ret;
}

int X11_SetWindowIcon(SDL_VideoDevice *_this, SDL_Window *window, SDL_Surface *icon)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    Atom _NET_WM_ICON = data->videodata->_NET_WM_ICON;
    int rc = 0;
    int (*prevHandler)(Display *, XErrorEvent *) = NULL;

    if (icon) {
        int x, y;
        int propsize;
        long *propdata;
        Uint32 *src;
        long *dst;

        /* Set the _NET_WM_ICON property */
        SDL_assert(icon->format->format == SDL_PIXELFORMAT_ARGB8888);
        propsize = 2 + (icon->w * icon->h);
        propdata = SDL_malloc(propsize * sizeof(long));

        if (!propdata) {
            return -1;
        }

        X11_XSync(display, False);
        prevHandler = X11_XSetErrorHandler(X11_CatchAnyError);

        propdata[0] = icon->w;
        propdata[1] = icon->h;
        dst = &propdata[2];

        for (y = 0; y < icon->h; ++y) {
            src = (Uint32 *)((Uint8 *)icon->pixels + y * icon->pitch);
            for (x = 0; x < icon->w; ++x) {
                *dst++ = *src++;
            }
        }

        X11_XChangeProperty(display, data->xwindow, _NET_WM_ICON, XA_CARDINAL,
                            32, PropModeReplace, (unsigned char *)propdata,
                            propsize);
        SDL_free(propdata);

        if (caught_x11_error) {
            rc = SDL_SetError("An error occurred while trying to set the window's icon");
        }
    }

    X11_XFlush(display);

    if (prevHandler) {
        X11_XSetErrorHandler(prevHandler);
        caught_x11_error = SDL_FALSE;
    }

    return rc;
}

void X11_UpdateWindowPosition(SDL_Window *window, SDL_bool use_current_position)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    const int rel_x = use_current_position ? window->x : window->floating.x;
    const int rel_y = use_current_position ? window->y : window->floating.y;

    SDL_RelativeToGlobalForWindow(window,
                                  rel_x - data->border_left, rel_y - data->border_top,
                                  &data->expected.x, &data->expected.y);

    /* Attempt to move the window */
    data->pending_operation |= X11_PENDING_OP_MOVE;
    X11_XMoveWindow(display, data->xwindow, data->expected.x, data->expected.y);
}

int X11_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    /* Sync any pending fullscreen or maximize events. */
    if (window->driverdata->pending_operation & (X11_PENDING_OP_FULLSCREEN | X11_PENDING_OP_MAXIMIZE)) {
        X11_SyncWindow(_this, window);
    }

    /* Position will be set when window is de-maximized */
    if (window->flags & SDL_WINDOW_MAXIMIZED) {
        return 0;
    }

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        if (SDL_WINDOW_IS_POPUP(window)) {
            X11_ConstrainPopup(window);
        }
        X11_UpdateWindowPosition(window, SDL_FALSE);
    } else {
        SDL_UpdateFullscreenMode(window, SDL_FULLSCREEN_OP_UPDATE, SDL_TRUE);
    }
    return 0;
}

static void X11_SetWMNormalHints(SDL_VideoDevice *_this, SDL_Window *window, XSizeHints *sizehints)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    int dest_x, dest_y;

    X11_XSetWMNormalHints(display, data->xwindow, sizehints);

    /* From Pierre-Loup:
       WMs each have their little quirks with that.  When you change the
       size hints, they get a ConfigureNotify event with the
       WM_NORMAL_SIZE_HINTS Atom.  They all save the hints then, but they
       don't all resize the window right away to enforce the new hints.

       Some of them resize only after:
        - A user-initiated move or resize
        - A code-initiated move or resize
        - Hiding & showing window (Unmap & map)

       The following move & resize seems to help a lot of WMs that didn't
       properly update after the hints were changed. We don't do a
       hide/show, because there are supposedly subtle problems with doing so
       and transitioning from windowed to fullscreen in Unity.
     */
    X11_XResizeWindow(display, data->xwindow, window->floating.w, window->floating.h);
    SDL_RelativeToGlobalForWindow(window,
                                  window->floating.x - data->border_left,
                                  window->floating.y - data->border_top,
                                  &dest_x, &dest_y);
    X11_XMoveWindow(display, data->xwindow, dest_x, dest_y);
    X11_XRaiseWindow(display, data->xwindow);
}

void X11_SetWindowMinMax(SDL_Window *window, SDL_bool use_current)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    XSizeHints *sizehints = X11_XAllocSizeHints();
    long hint_flags = 0;

    X11_XGetWMNormalHints(display, data->xwindow, sizehints, &hint_flags);
    sizehints->flags &= ~(PMinSize | PMaxSize | PAspect);

    if (data->window->flags & SDL_WINDOW_RESIZABLE) {
        if (data->window->min_w || data->window->min_h) {
            sizehints->flags |= PMinSize;
            sizehints->min_width = data->window->min_w;
            sizehints->min_height = data->window->min_h;
        }
        if (data->window->max_w || data->window->max_h) {
            sizehints->flags |= PMaxSize;
            sizehints->max_width = data->window->max_w;
            sizehints->max_height = data->window->max_h;
        }
        if (data->window->min_aspect > 0.0f || data->window->max_aspect > 0.0f) {
            sizehints->flags |= PAspect;
            SDL_CalculateFraction(data->window->min_aspect, &sizehints->min_aspect.x, &sizehints->min_aspect.y);
            SDL_CalculateFraction(data->window->max_aspect, &sizehints->max_aspect.x, &sizehints->max_aspect.y);
        }
    } else {
        /* Set the min/max to the same values to make the window non-resizable */
        sizehints->flags |= PMinSize | PMaxSize;
        sizehints->min_width = sizehints->max_width = use_current ? data->window->floating.w : window->windowed.w;
        sizehints->min_height = sizehints->max_height = use_current ? data->window->floating.h : window->windowed.h;
    }

    X11_XSetWMNormalHints(display, data->xwindow, sizehints);
    X11_XFree(sizehints);
}

void X11_SetWindowMinimumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (window->driverdata->pending_operation & X11_PENDING_OP_FULLSCREEN) {
        X11_SyncWindow(_this, window);
    }

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        X11_SetWindowMinMax(window, SDL_TRUE);
    }
}

void X11_SetWindowMaximumSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (window->driverdata->pending_operation & X11_PENDING_OP_FULLSCREEN) {
        X11_SyncWindow(_this, window);
    }

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        X11_SetWindowMinMax(window, SDL_TRUE);
    }
}

void X11_SetWindowAspectRatio(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (window->driverdata->pending_operation & X11_PENDING_OP_FULLSCREEN) {
        X11_SyncWindow(_this, window);
    }

    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        X11_SetWindowMinMax(window, SDL_TRUE);
    }
}

void X11_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;

    /* Wait for pending maximize operations to complete, or the window can end up in a weird,
     * partially-maximized state.
     */
    if (data->pending_operation & (X11_PENDING_OP_MAXIMIZE | X11_PENDING_OP_FULLSCREEN)) {
        X11_SyncWindow(_this, window);
    }

    /* Don't try to resize a maximized or fullscreen window, it will be done on restore. */
    if (window->flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN)) {
        return;
    }

    if (!(window->flags & SDL_WINDOW_RESIZABLE)) {
        if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
            /* Apparently, if the X11 Window is set to a 'non-resizable' window, you cannot resize it using the X11_XResizeWindow, thus
             * we must set the size hints to adjust the window size.
             */
            XSizeHints *sizehints = X11_XAllocSizeHints();
            long userhints;

            X11_XGetWMNormalHints(display, data->xwindow, sizehints, &userhints);

            sizehints->min_width = sizehints->max_width = window->floating.w;
            sizehints->min_height = sizehints->max_height = window->floating.h;
            sizehints->flags |= PMinSize | PMaxSize;

            X11_SetWMNormalHints(_this, window, sizehints);

            X11_XFree(sizehints);
        }
    } else {
        data->expected.w = window->floating.w;
        data->expected.h = window->floating.h;
        data->pending_operation |= X11_PENDING_OP_RESIZE;
        X11_XResizeWindow(display, data->xwindow, data->expected.w, data->expected.h);
    }
}

int X11_GetWindowBordersSize(SDL_VideoDevice *_this, SDL_Window *window, int *top, int *left, int *bottom, int *right)
{
    SDL_WindowData *data = window->driverdata;

    *left = data->border_left;
    *right = data->border_right;
    *top = data->border_top;
    *bottom = data->border_bottom;

    return 0;
}

int X11_SetWindowOpacity(SDL_VideoDevice *_this, SDL_Window *window, float opacity)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    Atom _NET_WM_WINDOW_OPACITY = data->videodata->_NET_WM_WINDOW_OPACITY;

    if (opacity == 1.0f) {
        X11_XDeleteProperty(display, data->xwindow, _NET_WM_WINDOW_OPACITY);
    } else {
        const Uint32 FullyOpaque = 0xFFFFFFFF;
        const long alpha = (long)((double)opacity * (double)FullyOpaque);
        X11_XChangeProperty(display, data->xwindow, _NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                            PropModeReplace, (unsigned char *)&alpha, 1);
    }

    return 0;
}

int X11_SetWindowModalFor(SDL_VideoDevice *_this, SDL_Window *modal_window, SDL_Window *parent_window)
{
    SDL_WindowData *data = modal_window->driverdata;
    SDL_WindowData *parent_data = parent_window ? parent_window->driverdata : NULL;
    SDL_VideoData *video_data = _this->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(modal_window);
    Display *display = video_data->display;
    Uint32 flags = modal_window->flags;
    Atom _NET_WM_STATE = data->videodata->_NET_WM_STATE;
    Atom _NET_WM_STATE_MODAL = data->videodata->_NET_WM_STATE_MODAL;

    if (parent_data) {
        flags |= SDL_WINDOW_MODAL;
        X11_XSetTransientForHint(display, data->xwindow, parent_data->xwindow);
    } else {
        flags &= ~SDL_WINDOW_MODAL;
        X11_XDeleteProperty(display, data->xwindow, video_data->WM_TRANSIENT_FOR);
    }

    if (X11_IsWindowMapped(_this, modal_window)) {
        XEvent e;

        SDL_zero(e);
        e.xany.type = ClientMessage;
        e.xclient.message_type = _NET_WM_STATE;
        e.xclient.format = 32;
        e.xclient.window = data->xwindow;
        e.xclient.data.l[0] =
            parent_data ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        e.xclient.data.l[1] = _NET_WM_STATE_MODAL;
        e.xclient.data.l[3] = 0l;

        X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                       SubstructureNotifyMask | SubstructureRedirectMask, &e);
    } else {
        X11_SetNetWMState(_this, data->xwindow, flags);
    }

    X11_XFlush(display);

    return 0;
}

void X11_SetWindowBordered(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool bordered)
{
    const SDL_bool focused = (window->flags & SDL_WINDOW_INPUT_FOCUS) ? SDL_TRUE : SDL_FALSE;
    const SDL_bool visible = (!(window->flags & SDL_WINDOW_HIDDEN)) ? SDL_TRUE : SDL_FALSE;
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;
    XEvent event;

    if (data->pending_operation & X11_PENDING_OP_FULLSCREEN) {
        X11_SyncWindow(_this, window);
    }

    /* If the window is fullscreen, the resize capability will be set/cleared when it is returned to windowed mode. */
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        SetWindowBordered(display, displaydata->screen, data->xwindow, bordered);
        X11_XFlush(display);

        if (visible) {
            XWindowAttributes attr;
            do {
                X11_XSync(display, False);
                X11_XGetWindowAttributes(display, data->xwindow, &attr);
            } while (attr.map_state != IsViewable);

            if (focused) {
                X11_XSetInputFocus(display, data->xwindow, RevertToParent, CurrentTime);
            }
        }

        /* make sure these don't make it to the real event queue if they fired here. */
        X11_XSync(display, False);
        X11_XCheckIfEvent(display, &event, &isUnmapNotify, (XPointer)&data->xwindow);
        X11_XCheckIfEvent(display, &event, &isMapNotify, (XPointer)&data->xwindow);

        /* Turning the borders off doesn't send an extent event, so they must be cleared here. */
        X11_GetBorderValues(data);

        /* Make sure the window manager didn't resize our window for the difference. */
        X11_XResizeWindow(display, data->xwindow, window->floating.w, window->floating.h);
        X11_XSync(display, False);
    } else {
        /* If fullscreen, set a flag to toggle the borders when returning to windowed mode. */
        data->toggle_borders = SDL_TRUE;
        data->fullscreen_borders_forced_on = SDL_FALSE;
    }
}

void X11_SetWindowResizable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool resizable)
{
    SDL_WindowData *data = window->driverdata;

    if (data->pending_operation & X11_PENDING_OP_FULLSCREEN) {
        X11_SyncWindow(_this, window);
    }

    /* If the window is fullscreen, the resize capability will be set/cleared when it is returned to windowed mode. */
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        X11_SetWindowMinMax(window, SDL_TRUE);
    }
}

void X11_SetWindowAlwaysOnTop(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool on_top)
{
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;
    Atom _NET_WM_STATE = data->videodata->_NET_WM_STATE;
    Atom _NET_WM_STATE_ABOVE = data->videodata->_NET_WM_STATE_ABOVE;

    if (X11_IsWindowMapped(_this, window)) {
        XEvent e;

        SDL_zero(e);
        e.xany.type = ClientMessage;
        e.xclient.message_type = _NET_WM_STATE;
        e.xclient.format = 32;
        e.xclient.window = data->xwindow;
        e.xclient.data.l[0] =
            on_top ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        e.xclient.data.l[1] = _NET_WM_STATE_ABOVE;
        e.xclient.data.l[3] = 0l;

        X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                       SubstructureNotifyMask | SubstructureRedirectMask, &e);
    } else {
        X11_SetNetWMState(_this, data->xwindow, window->flags);
    }
    X11_XFlush(display);
}

void X11_ShowWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, SDL_TRUE);
    XEvent event;

    if (window->parent) {
        /* Update our position in case our parent moved while we were hidden */
        X11_UpdateWindowPosition(window, SDL_TRUE);
    }

    /* Whether XMapRaised focuses the window is based on the window type and it is
     * wm specific. There isn't much we can do here */
    (void)bActivate;

    if (!X11_IsWindowMapped(_this, window)) {
        X11_XMapRaised(display, data->xwindow);
        /* Blocking wait for "MapNotify" event.
         * We use X11_XIfEvent because pXWindowEvent takes a mask rather than a type,
         * and XCheckTypedWindowEvent doesn't block */
        if (!(window->flags & SDL_WINDOW_EXTERNAL)) {
            X11_XIfEvent(display, &event, &isMapNotify, (XPointer)&data->xwindow);
        }
        X11_XFlush(display);
    }

    if (!data->videodata->net_wm) {
        /* no WM means no FocusIn event, which confuses us. Force it. */
        X11_XSync(display, False);
        X11_XSetInputFocus(display, data->xwindow, RevertToNone, CurrentTime);
        X11_XFlush(display);
    }

    /* Popup menus grab the keyboard */
    if (window->flags & SDL_WINDOW_POPUP_MENU) {
        if (window->parent == SDL_GetKeyboardFocus()) {
            X11_SetKeyboardFocus(window);
        }
    }

    /* Get some valid border values, if we haven't received them yet */
    if (data->border_left == 0 && data->border_right == 0 && data->border_top == 0 && data->border_bottom == 0) {
        X11_GetBorderValues(data);
    }

    /* Some window managers can send garbage coordinates while mapping the window, and need the position sent again
     * after mapping or the window may not be positioned properly.
     *
     * Don't emit size and position events during the initial configure events, they will be sent afterwards, when the
     * final coordinates are available to avoid sending garbage values.
     */
    data->disable_size_position_events = SDL_TRUE;
    X11_XSync(display, False);
    X11_PumpEvents(_this);

    /* If a configure event was received (type is non-zero), send the final window size and coordinates. */
    if (data->last_xconfigure.type) {
        int x = data->last_xconfigure.x;
        int y = data->last_xconfigure.y;
        SDL_GlobalToRelativeForWindow(data->window, x, y, &x, &y);

        /* If the borders appeared, this happened automatically in the event system, otherwise, set the position now. */
        if (data->disable_size_position_events && (window->x != x || window->y != y)) {
            data->pending_operation = X11_PENDING_OP_MOVE;
            X11_XMoveWindow(display, data->xwindow, window->x, window->y);
        }

        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, data->last_xconfigure.width, data->last_xconfigure.height);
        SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, x, y);
    }

    data->disable_size_position_events = SDL_FALSE;
}

void X11_HideWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    int screen = (displaydata ? displaydata->screen : 0);
    Display *display = data->videodata->display;
    XEvent event;

    if (X11_IsWindowMapped(_this, window)) {
        X11_XWithdrawWindow(display, data->xwindow, screen);
        /* Blocking wait for "UnmapNotify" event */
        if (!(window->flags & SDL_WINDOW_EXTERNAL)) {
            X11_XIfEvent(display, &event, &isUnmapNotify, (XPointer)&data->xwindow);
        }
        X11_XFlush(display);
    }

    /* Transfer keyboard focus back to the parent */
    if (window->flags & SDL_WINDOW_POPUP_MENU) {
        if (window == SDL_GetKeyboardFocus()) {
            SDL_Window *new_focus = window->parent;

            /* Find the highest level window that isn't being hidden or destroyed. */
            while (new_focus->parent && (new_focus->is_hiding || new_focus->is_destroying)) {
                new_focus = new_focus->parent;
            }

            X11_SetKeyboardFocus(new_focus);
        }
    }

    X11_XSync(display, False);
    X11_PumpEvents(_this);
}

static int X11_SetWindowActive(SDL_VideoDevice *_this, SDL_Window *window)
{
    CHECK_WINDOW_DATA(window);

    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;
    Atom _NET_ACTIVE_WINDOW = data->videodata->_NET_ACTIVE_WINDOW;

    if (X11_IsWindowMapped(_this, window)) {
        XEvent e;

        /*printf("SDL Window %p: sending _NET_ACTIVE_WINDOW with timestamp %lu\n", window, data->user_time);*/

        SDL_zero(e);
        e.xany.type = ClientMessage;
        e.xclient.message_type = _NET_ACTIVE_WINDOW;
        e.xclient.format = 32;
        e.xclient.window = data->xwindow;
        e.xclient.data.l[0] = 1; /* source indication. 1 = application */
        e.xclient.data.l[1] = data->user_time;
        e.xclient.data.l[2] = 0;

        X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                       SubstructureNotifyMask | SubstructureRedirectMask, &e);

        X11_XFlush(display);
    }
    return 0;
}

void X11_RaiseWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    SDL_bool bActivate = SDL_GetHintBoolean(SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, SDL_TRUE);

    X11_XRaiseWindow(display, data->xwindow);
    if (bActivate) {
        X11_SetWindowActive(_this, window);
    }
    X11_XFlush(display);
}

static int X11_SetWindowMaximized(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool maximized)
{
    CHECK_WINDOW_DATA(window);

    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;
    Atom _NET_WM_STATE = data->videodata->_NET_WM_STATE;
    Atom _NET_WM_STATE_MAXIMIZED_VERT = data->videodata->_NET_WM_STATE_MAXIMIZED_VERT;
    Atom _NET_WM_STATE_MAXIMIZED_HORZ = data->videodata->_NET_WM_STATE_MAXIMIZED_HORZ;

    if (!maximized && window->flags & SDL_WINDOW_FULLSCREEN) {
        /* Fullscreen windows are maximized on some window managers,
           and this is functional behavior, so don't remove that state
           now, we'll take care of it when we leave fullscreen mode.
         */
        return 0;
    }

    if (X11_IsWindowMapped(_this, window)) {
        XEvent e;

        SDL_zero(e);
        e.xany.type = ClientMessage;
        e.xclient.message_type = _NET_WM_STATE;
        e.xclient.format = 32;
        e.xclient.window = data->xwindow;
        e.xclient.data.l[0] =
            maximized ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        e.xclient.data.l[1] = _NET_WM_STATE_MAXIMIZED_VERT;
        e.xclient.data.l[2] = _NET_WM_STATE_MAXIMIZED_HORZ;
        e.xclient.data.l[3] = 0l;

        if (maximized) {
            SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
            SDL_Rect bounds;

            SDL_zero(bounds);
            SDL_GetDisplayUsableBounds(displayID, &bounds);

            data->expected.x = bounds.x + data->border_left;
            data->expected.y = bounds.y + data->border_top;
            data->expected.w = bounds.w - (data->border_left + data->border_right);
            data->expected.h = bounds.h - (data->border_top + data->border_bottom);
        } else {
            data->expected.x = window->floating.x;
            data->expected.y = window->floating.y;
            data->expected.w = window->floating.w;
            data->expected.h = window->floating.h;
        }

        X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                       SubstructureNotifyMask | SubstructureRedirectMask, &e);
    } else {
        X11_SetNetWMState(_this, data->xwindow, window->flags);
    }
    X11_XFlush(display);

    return 0;
}

void X11_MaximizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (window->driverdata->pending_operation & (X11_PENDING_OP_FULLSCREEN | X11_PENDING_OP_MINIMIZE)) {
        SDL_SyncWindow(window);
    }

    if (!(window->flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED))) {
        window->driverdata->pending_operation |= X11_PENDING_OP_MAXIMIZE;
        X11_SetWindowMaximized(_this, window, SDL_TRUE);
    }
}

void X11_MinimizeWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;

    data->pending_operation |= X11_PENDING_OP_MINIMIZE;
    data->window_was_maximized = !!(window->flags & SDL_WINDOW_MAXIMIZED);
    X11_XIconifyWindow(display, data->xwindow, displaydata->screen);
    X11_XFlush(display);
}

void X11_RestoreWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    if (window->driverdata->pending_operation & (X11_PENDING_OP_FULLSCREEN | X11_PENDING_OP_MAXIMIZE | X11_PENDING_OP_MINIMIZE)) {
        SDL_SyncWindow(window);
    }

    if (window->flags & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED) ||
        (window->driverdata->pending_operation & X11_PENDING_OP_MINIMIZE)) {
        window->driverdata->pending_operation |= X11_PENDING_OP_RESTORE;
    }

    /* If the window was minimized while maximized, restore as maximized. */
    const SDL_bool maximize = !!(window->flags & SDL_WINDOW_MINIMIZED) &&  window->driverdata->window_was_maximized;
    window->driverdata->window_was_maximized = SDL_FALSE;
    X11_SetWindowMaximized(_this, window, maximize);
    X11_ShowWindow(_this, window);
    X11_SetWindowActive(_this, window);
}

/* This asks the Window Manager to handle fullscreen for us. This is the modern way. */
static int X11_SetWindowFullscreenViaWM(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *_display, SDL_FullscreenOp fullscreen)
{
    CHECK_WINDOW_DATA(window);
    CHECK_DISPLAY_DATA(_display);

    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = _display->driverdata;
    Display *display = data->videodata->display;
    Atom _NET_WM_STATE = data->videodata->_NET_WM_STATE;
    Atom _NET_WM_STATE_FULLSCREEN = data->videodata->_NET_WM_STATE_FULLSCREEN;

    if (X11_IsWindowMapped(_this, window)) {
        XEvent e;

        /* Flush any pending fullscreen events. */
        if (data->pending_operation & (X11_PENDING_OP_FULLSCREEN | X11_PENDING_OP_MAXIMIZE | X11_PENDING_OP_MOVE)) {
            X11_SyncWindow(_this, window);
        }

        if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
            if (fullscreen == SDL_FULLSCREEN_OP_UPDATE) {
                /* Request was out of date; set -1 to signal the video core to undo a mode switch. */
                return -1;
            } else if (fullscreen == SDL_FULLSCREEN_OP_LEAVE) {
                /* Nothing to do. */
                return 0;
            }
        }

        if (fullscreen && !(window->flags & SDL_WINDOW_RESIZABLE)) {
            /* Compiz refuses fullscreen toggle if we're not resizable, so update the hints so we
               can be resized to the fullscreen resolution (or reset so we're not resizable again) */
            XSizeHints *sizehints = X11_XAllocSizeHints();
            long flags = 0;
            X11_XGetWMNormalHints(display, data->xwindow, sizehints, &flags);
            /* we are going fullscreen so turn the flags off */
            sizehints->flags &= ~(PMinSize | PMaxSize | PAspect);
            X11_XSetWMNormalHints(display, data->xwindow, sizehints);
            X11_XFree(sizehints);
        }

        SDL_zero(e);
        e.xany.type = ClientMessage;
        e.xclient.message_type = _NET_WM_STATE;
        e.xclient.format = 32;
        e.xclient.window = data->xwindow;
        e.xclient.data.l[0] =
            fullscreen ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        e.xclient.data.l[1] = _NET_WM_STATE_FULLSCREEN;
        e.xclient.data.l[3] = 0l;

        X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                       SubstructureNotifyMask | SubstructureRedirectMask, &e);

        if (!!(window->flags & SDL_WINDOW_FULLSCREEN) != fullscreen) {
            data->pending_operation |= X11_PENDING_OP_FULLSCREEN;
        }

        /* Set the position so the window will be on the target display */
        if (fullscreen) {
            SDL_DisplayID current = SDL_GetDisplayForWindowPosition(window);
            SDL_copyp(&data->requested_fullscreen_mode, &window->current_fullscreen_mode);
            if (fullscreen != !!(window->flags & SDL_WINDOW_FULLSCREEN)) {
                data->window_was_maximized = !!(window->flags & SDL_WINDOW_MAXIMIZED);
            }
            data->expected.x = displaydata->x;
            data->expected.y = displaydata->y;
            data->expected.w = _display->current_mode->w;
            data->expected.h = _display->current_mode->h;

            /* Only move the window if it isn't fullscreen or already on the target display. */
            if (!(window->flags & SDL_WINDOW_FULLSCREEN) || (!current || current != _display->id)) {
                X11_XMoveWindow(display, data->xwindow, displaydata->x, displaydata->y);
                data->pending_operation |= X11_PENDING_OP_MOVE;
            }
        } else {
            SDL_zero(data->requested_fullscreen_mode);

            /* Fullscreen windows sometimes end up being marked maximized by
             * window managers. Force it back to how we expect it to be.
             */
            SDL_zero(e);
            e.xany.type = ClientMessage;
            e.xclient.message_type = _NET_WM_STATE;
            e.xclient.format = 32;
            e.xclient.window = data->xwindow;
            if (data->window_was_maximized) {
                e.xclient.data.l[0] = _NET_WM_STATE_ADD;
            } else {
                e.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
            }
            e.xclient.data.l[1] = data->videodata->_NET_WM_STATE_MAXIMIZED_VERT;
            e.xclient.data.l[2] = data->videodata->_NET_WM_STATE_MAXIMIZED_HORZ;
            e.xclient.data.l[3] = 0l;
            X11_XSendEvent(display, RootWindow(display, displaydata->screen), 0,
                           SubstructureNotifyMask | SubstructureRedirectMask, &e);
        }
    } else {
        SDL_WindowFlags flags;

        flags = window->flags;
        if (fullscreen) {
            flags |= SDL_WINDOW_FULLSCREEN;
        } else {
            flags &= ~SDL_WINDOW_FULLSCREEN;
        }
        X11_SetNetWMState(_this, data->xwindow, flags);
    }

    if (data->visual->class == DirectColor) {
        if (fullscreen) {
            X11_XInstallColormap(display, data->colormap);
        } else {
            X11_XUninstallColormap(display, data->colormap);
        }
    }

    return 1;
}

int X11_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *_display, SDL_FullscreenOp fullscreen)
{
    return X11_SetWindowFullscreenViaWM(_this, window, _display, fullscreen);
}

typedef struct
{
    unsigned char *data;
    int format, count;
    Atom type;
} SDL_x11Prop;

/* Reads property
   Must call X11_XFree on results
 */
static void X11_ReadProperty(SDL_x11Prop *p, Display *disp, Window w, Atom prop)
{
    unsigned char *ret = NULL;
    Atom type;
    int fmt;
    unsigned long count;
    unsigned long bytes_left;
    int bytes_fetch = 0;

    do {
        if (ret) {
            X11_XFree(ret);
        }
        X11_XGetWindowProperty(disp, w, prop, 0, bytes_fetch, False, AnyPropertyType, &type, &fmt, &count, &bytes_left, &ret);
        bytes_fetch += bytes_left;
    } while (bytes_left != 0);

    p->data = ret;
    p->format = fmt;
    p->count = count;
    p->type = type;
}

void *X11_GetWindowICCProfile(SDL_VideoDevice *_this, SDL_Window *window, size_t *size)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    XWindowAttributes attributes;
    Atom icc_profile_atom;
    char icc_atom_string[sizeof("_ICC_PROFILE_") + 12];
    void *ret_icc_profile_data = NULL;
    CARD8 *icc_profile_data;
    int real_format;
    unsigned long real_nitems;
    SDL_x11Prop atomProp;

    X11_XGetWindowAttributes(display, data->xwindow, &attributes);
    if (X11_XScreenNumberOfScreen(attributes.screen) > 0) {
        (void)SDL_snprintf(icc_atom_string, sizeof("_ICC_PROFILE_") + 12, "%s%d", "_ICC_PROFILE_", X11_XScreenNumberOfScreen(attributes.screen));
    } else {
        SDL_strlcpy(icc_atom_string, "_ICC_PROFILE", sizeof("_ICC_PROFILE"));
    }
    X11_XGetWindowAttributes(display, RootWindowOfScreen(attributes.screen), &attributes);

    icc_profile_atom = X11_XInternAtom(display, icc_atom_string, True);
    if (icc_profile_atom == None) {
        SDL_SetError("Screen is not calibrated.\n");
        return NULL;
    }

    X11_ReadProperty(&atomProp, display, RootWindowOfScreen(attributes.screen), icc_profile_atom);
    real_format = atomProp.format;
    real_nitems = atomProp.count;
    icc_profile_data = atomProp.data;
    if (real_format == None) {
        SDL_SetError("Screen is not calibrated.\n");
        return NULL;
    }

    ret_icc_profile_data = SDL_malloc(real_nitems);
    if (!ret_icc_profile_data) {
        return NULL;
    }

    SDL_memcpy(ret_icc_profile_data, icc_profile_data, real_nitems);
    *size = real_nitems;
    X11_XFree(icc_profile_data);

    return ret_icc_profile_data;
}

int X11_SetWindowMouseGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    SDL_WindowData *data = window->driverdata;
    Display *display;

    if (!data) {
        return SDL_SetError("Invalid window data");
    }
    data->mouse_grabbed = SDL_FALSE;

    display = data->videodata->display;

    if (grabbed) {
        /* If the window is unmapped, XGrab calls return GrabNotViewable,
           so when we get a MapNotify later, we'll try to update the grab as
           appropriate. */
        if (window->flags & SDL_WINDOW_HIDDEN) {
            return 0;
        }

        /* If XInput2 is enabled, it will grab the pointer on button presses,
         * which results in XGrabPointer returning AlreadyGrabbed. If buttons
         * are currently pressed, clear any existing grabs before attempting
         * the confinement grab.
         */
        if (data->xinput2_mouse_enabled && SDL_GetMouseState(NULL, NULL)) {
            X11_XUngrabPointer(display, CurrentTime);
        }

        /* Try to grab the mouse */
        if (!data->videodata->broken_pointer_grab) {
            const unsigned int mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
            int attempts;
            int result = 0;

            /* Try for up to 5000ms (5s) to grab. If it still fails, stop trying. */
            for (attempts = 0; attempts < 100; attempts++) {
                result = X11_XGrabPointer(display, data->xwindow, False, mask, GrabModeAsync,
                                          GrabModeAsync, data->xwindow, None, CurrentTime);
                if (result == GrabSuccess) {
                    data->mouse_grabbed = SDL_TRUE;
                    break;
                }
                SDL_Delay(50);
            }

            if (result != GrabSuccess) {
                data->videodata->broken_pointer_grab = SDL_TRUE; /* don't try again. */
            }
        }

        X11_Xinput2GrabTouch(_this, window);

        /* Raise the window if we grab the mouse */
        X11_XRaiseWindow(display, data->xwindow);
    } else {
        X11_XUngrabPointer(display, CurrentTime);

        X11_Xinput2UngrabTouch(_this, window);
    }
    X11_XSync(display, False);

    if (!data->videodata->broken_pointer_grab) {
        return 0;
    } else {
        return SDL_SetError("The X server refused to let us grab the mouse. You might experience input bugs.");
    }
}

int X11_SetWindowKeyboardGrab(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool grabbed)
{
    SDL_WindowData *data = window->driverdata;
    Display *display;

    if (!data) {
        return SDL_SetError("Invalid window data");
    }

    display = data->videodata->display;

    if (grabbed) {
        /* If the window is unmapped, XGrab calls return GrabNotViewable,
           so when we get a MapNotify later, we'll try to update the grab as
           appropriate. */
        if (window->flags & SDL_WINDOW_HIDDEN) {
            return 0;
        }

        X11_XGrabKeyboard(display, data->xwindow, True, GrabModeAsync,
                          GrabModeAsync, CurrentTime);
    } else {
        X11_XUngrabKeyboard(display, CurrentTime);
    }
    X11_XSync(display, False);

    return 0;
}

void X11_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;

    if (data) {
        SDL_VideoData *videodata = data->videodata;
        Display *display = videodata->display;
        int numwindows = videodata->numwindows;
        SDL_WindowData **windowlist = videodata->windowlist;
        int i;

        if (windowlist) {
            for (i = 0; i < numwindows; ++i) {
                if (windowlist[i] && (windowlist[i]->window == window)) {
                    windowlist[i] = windowlist[numwindows - 1];
                    windowlist[numwindows - 1] = NULL;
                    videodata->numwindows--;
                    break;
                }
            }
        }
#ifdef X_HAVE_UTF8_STRING
        if (data->ic) {
            X11_XDestroyIC(data->ic);
        }
#endif
        if (!(window->flags & SDL_WINDOW_EXTERNAL)) {
            X11_XDestroyWindow(display, data->xwindow);
            X11_XFlush(display);
        }
        SDL_free(data);

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
        /* If the pointer barriers are active for this, deactivate it.*/
        if (videodata->active_cursor_confined_window == window) {
            X11_DestroyPointerBarrier(_this, window);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */
    }
    window->driverdata = NULL;
}

int X11_SetWindowHitTest(SDL_Window *window, SDL_bool enabled)
{
    return 0; /* just succeed, the real work is done elsewhere. */
}

void X11_AcceptDragAndDrop(SDL_Window *window, SDL_bool accept)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    Atom XdndAware = X11_XInternAtom(display, "XdndAware", False);

    if (accept) {
        Atom xdnd_version = 5;
        X11_XChangeProperty(display, data->xwindow, XdndAware, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)&xdnd_version, 1);
    } else {
        X11_XDeleteProperty(display, data->xwindow, XdndAware);
    }
}

int X11_FlashWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_FlashOperation operation)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    XWMHints *wmhints;

    wmhints = X11_XGetWMHints(display, data->xwindow);
    if (!wmhints) {
        return SDL_SetError("Couldn't get WM hints");
    }

    wmhints->flags &= ~XUrgencyHint;
    data->flashing_window = SDL_FALSE;
    data->flash_cancel_time = 0;

    switch (operation) {
    case SDL_FLASH_CANCEL:
        /* Taken care of above */
        break;
    case SDL_FLASH_BRIEFLY:
        if (!(window->flags & SDL_WINDOW_INPUT_FOCUS)) {
            wmhints->flags |= XUrgencyHint;
            data->flashing_window = SDL_TRUE;
            /* On Ubuntu 21.04 this causes a dialog to pop up, so leave it up for a full second so users can see it */
            data->flash_cancel_time = SDL_GetTicks() + 1000;
        }
        break;
    case SDL_FLASH_UNTIL_FOCUSED:
        if (!(window->flags & SDL_WINDOW_INPUT_FOCUS)) {
            wmhints->flags |= XUrgencyHint;
            data->flashing_window = SDL_TRUE;
        }
        break;
    default:
        break;
    }

    X11_XSetWMHints(display, data->xwindow, wmhints);
    X11_XFree(wmhints);
    return 0;
}

int SDL_X11_SetWindowTitle(Display *display, Window xwindow, char *title)
{
    Atom _NET_WM_NAME = X11_XInternAtom(display, "_NET_WM_NAME", False);
    XTextProperty titleprop;
    int conv = X11_XmbTextListToTextProperty(display, &title, 1, XTextStyle, &titleprop);
    Status status;

    if (X11_XSupportsLocale() != True) {
        return SDL_SetError("Current locale not supported by X server, cannot continue.");
    }

    if (conv == 0) {
        X11_XSetTextProperty(display, xwindow, &titleprop, XA_WM_NAME);
        X11_XFree(titleprop.value);
        /* we know this can't be a locale error as we checked X locale validity */
    } else if (conv < 0) {
        return SDL_OutOfMemory();
    } else { /* conv > 0 */
        SDL_LogDebug(SDL_LOG_CATEGORY_VIDEO, "%d characters were not convertible to the current locale!", conv);
        return 0;
    }

#ifdef X_HAVE_UTF8_STRING
    status = X11_Xutf8TextListToTextProperty(display, &title, 1, XUTF8StringStyle, &titleprop);
    if (status == Success) {
        X11_XSetTextProperty(display, xwindow, &titleprop, _NET_WM_NAME);
        X11_XFree(titleprop.value);
    } else {
        return SDL_SetError("Failed to convert title to UTF8! Bad encoding, or bad Xorg encoding? Window title: «%s»", title);
    }
#endif

    X11_XFlush(display);
    return 0;
}

void X11_ShowWindowSystemMenu(SDL_Window *window, int x, int y)
{
    SDL_WindowData *data = window->driverdata;
    SDL_DisplayData *displaydata = SDL_GetDisplayDriverDataForWindow(window);
    Display *display = data->videodata->display;
    Window root = RootWindow(display, displaydata->screen);
    XClientMessageEvent e;
    Window childReturn;
    int wx, wy;

    SDL_zero(e);
    X11_XTranslateCoordinates(display, data->xwindow, root, x, y, &wx, &wy, &childReturn);

    e.type = ClientMessage;
    e.window = data->xwindow;
    e.message_type = X11_XInternAtom(display, "_GTK_SHOW_WINDOW_MENU", 0);
    e.data.l[0] = 0;  /* GTK device ID (unused) */
    e.data.l[1] = wx; /* X coordinate relative to root */
    e.data.l[2] = wy; /* Y coordinate relative to root */
    e.format = 32;

    X11_XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&e);
    X11_XFlush(display);
}

int X11_SyncWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    const Uint64 current_time = SDL_GetTicksNS();
    Uint64 timeout = 0;

    /* Allow time for any pending mode switches to complete. */
    for (int i = 0; i < _this->num_displays; ++i) {
        if (_this->displays[i]->driverdata->mode_switch_deadline_ns &&
            current_time < _this->displays[i]->driverdata->mode_switch_deadline_ns) {
            timeout = SDL_max(_this->displays[i]->driverdata->mode_switch_deadline_ns - current_time, timeout);
        }
    }

    /* 100ms is fine for most cases, but, for some reason, maximizing
     * a window can take a very long time.
     */
    timeout += window->driverdata->pending_operation & X11_PENDING_OP_MAXIMIZE ? SDL_MS_TO_NS(1000) : SDL_MS_TO_NS(100);

    return X11_SyncWindowTimeout(_this, window, timeout);
}

int X11_SetWindowFocusable(SDL_VideoDevice *_this, SDL_Window *window, SDL_bool focusable)
{
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;
    XWMHints *wmhints;

    wmhints = X11_XGetWMHints(display, data->xwindow);
    if (!wmhints) {
        return SDL_SetError("Couldn't get WM hints");
    }

    wmhints->input = focusable ? True : False;
    wmhints->flags |= InputHint;

    X11_XSetWMHints(display, data->xwindow, wmhints);
    X11_XFree(wmhints);

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_X11 */
