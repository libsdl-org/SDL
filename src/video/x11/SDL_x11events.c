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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_X11

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h> /* For INT_MAX */

#include "SDL_x11video.h"
#include "SDL_x11touch.h"
#include "SDL_x11xinput2.h"
#include "SDL_x11xfixes.h"
#include "../SDL_clipboard_c.h"
#include "../../core/unix/SDL_poll.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../core/linux/SDL_system_theme.h"
#include "../../SDL_utils_c.h"
#include "../SDL_sysvideo.h"

#include <stdio.h>

/*#define DEBUG_XEVENTS*/

#ifndef _NET_WM_MOVERESIZE_SIZE_TOPLEFT
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_TOP
#define _NET_WM_MOVERESIZE_SIZE_TOP 1
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_TOPRIGHT
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_RIGHT
#define _NET_WM_MOVERESIZE_SIZE_RIGHT 3
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_BOTTOM
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#endif

#ifndef _NET_WM_MOVERESIZE_SIZE_LEFT
#define _NET_WM_MOVERESIZE_SIZE_LEFT 7
#endif

#ifndef _NET_WM_MOVERESIZE_MOVE
#define _NET_WM_MOVERESIZE_MOVE 8
#endif

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

/* Find text-uri-list in a list of targets and return it's atom
   if available, else return None */
static Atom X11_PickTarget(Display *disp, Atom list[], int list_count)
{
    Atom request = None;
    char *name;
    int i;
    for (i = 0; i < list_count && request == None; i++) {
        name = X11_XGetAtomName(disp, list[i]);
        if ((SDL_strcmp("text/uri-list", name) == 0) || (SDL_strcmp("text/plain", name) == 0)) {
            request = list[i];
        }
        X11_XFree(name);
    }
    return request;
}

/* Wrapper for X11_PickTarget for a maximum of three targets, a special
   case in the Xdnd protocol */
static Atom X11_PickTargetFromAtoms(Display *disp, Atom a0, Atom a1, Atom a2)
{
    int count = 0;
    Atom atom[3];
    if (a0 != None) {
        atom[count++] = a0;
    }
    if (a1 != None) {
        atom[count++] = a1;
    }
    if (a2 != None) {
        atom[count++] = a2;
    }
    return X11_PickTarget(disp, atom, count);
}

struct KeyRepeatCheckData
{
    XEvent *event;
    SDL_bool found;
};

static Bool X11_KeyRepeatCheckIfEvent(Display *display, XEvent *chkev,
                                      XPointer arg)
{
    struct KeyRepeatCheckData *d = (struct KeyRepeatCheckData *)arg;
    if (chkev->type == KeyPress && chkev->xkey.keycode == d->event->xkey.keycode && chkev->xkey.time - d->event->xkey.time < 2) {
        d->found = SDL_TRUE;
    }
    return False;
}

/* Check to see if this is a repeated key.
   (idea shamelessly lifted from GII -- thanks guys! :)
 */
static SDL_bool X11_KeyRepeat(Display *display, XEvent *event)
{
    XEvent dummyev;
    struct KeyRepeatCheckData d;
    d.event = event;
    d.found = SDL_FALSE;
    if (X11_XPending(display)) {
        X11_XCheckIfEvent(display, &dummyev, X11_KeyRepeatCheckIfEvent, (XPointer)&d);
    }
    return d.found;
}

static SDL_bool X11_IsWheelEvent(Display *display, int button, int *xticks, int *yticks)
{
    /* according to the xlib docs, no specific mouse wheel events exist.
       However, the defacto standard is that the vertical wheel is X buttons
       4 (up) and 5 (down) and a horizontal wheel is 6 (left) and 7 (right). */

    /* Xlib defines "Button1" through 5, so we just use literals here. */
    switch (button) {
    case 4:
        *yticks = 1;
        return SDL_TRUE;
    case 5:
        *yticks = -1;
        return SDL_TRUE;
    case 6:
        *xticks = 1;
        return SDL_TRUE;
    case 7:
        *xticks = -1;
        return SDL_TRUE;
    default:
        break;
    }
    return SDL_FALSE;
}

/* Decodes URI escape sequences in string buf of len bytes
   (excluding the terminating NULL byte) in-place. Since
   URI-encoded characters take three times the space of
   normal characters, this should not be an issue.

   Returns the number of decoded bytes that wound up in
   the buffer, excluding the terminating NULL byte.

   The buffer is guaranteed to be NULL-terminated but
   may contain embedded NULL bytes.

   On error, -1 is returned.
 */
static int X11_URIDecode(char *buf, int len)
{
    int ri, wi, di;
    char decode = '\0';
    if (!buf || len < 0) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        len = SDL_strlen(buf);
    }
    for (ri = 0, wi = 0, di = 0; ri < len && wi < len; ri += 1) {
        if (di == 0) {
            /* start decoding */
            if (buf[ri] == '%') {
                decode = '\0';
                di += 1;
                continue;
            }
            /* normal write */
            buf[wi] = buf[ri];
            wi += 1;
            continue;
        } else if (di == 1 || di == 2) {
            char off = '\0';
            char isa = buf[ri] >= 'a' && buf[ri] <= 'f';
            char isA = buf[ri] >= 'A' && buf[ri] <= 'F';
            char isn = buf[ri] >= '0' && buf[ri] <= '9';
            if (!(isa || isA || isn)) {
                /* not a hexadecimal */
                int sri;
                for (sri = ri - di; sri <= ri; sri += 1) {
                    buf[wi] = buf[sri];
                    wi += 1;
                }
                di = 0;
                continue;
            }
            /* itsy bitsy magicsy */
            if (isn) {
                off = 0 - '0';
            } else if (isa) {
                off = 10 - 'a';
            } else if (isA) {
                off = 10 - 'A';
            }
            decode |= (buf[ri] + off) << (2 - di) * 4;
            if (di == 2) {
                buf[wi] = decode;
                wi += 1;
                di = 0;
            } else {
                di += 1;
            }
            continue;
        }
    }
    buf[wi] = '\0';
    return wi;
}

/* Convert URI to local filename
   return filename if possible, else NULL
*/
static char *X11_URIToLocal(char *uri)
{
    char *file = NULL;
    SDL_bool local;

    if (SDL_memcmp(uri, "file:/", 6) == 0) {
        uri += 6; /* local file? */
    } else if (SDL_strstr(uri, ":/") != NULL) {
        return file; /* wrong scheme */
    }

    local = uri[0] != '/' || (uri[0] != '\0' && uri[1] == '/');

    /* got a hostname? */
    if (!local && uri[0] == '/' && uri[2] != '/') {
        char *hostname_end = SDL_strchr(uri + 1, '/');
        if (hostname_end) {
            char hostname[257];
            if (gethostname(hostname, 255) == 0) {
                hostname[256] = '\0';
                if (SDL_memcmp(uri + 1, hostname, hostname_end - (uri + 1)) == 0) {
                    uri = hostname_end + 1;
                    local = SDL_TRUE;
                }
            }
        }
    }
    if (local) {
        file = uri;
        /* Convert URI escape sequences to real characters */
        X11_URIDecode(file, 0);
        if (uri[1] == '/') {
            file++;
        } else {
            file--;
        }
    }
    return file;
}

/* An X11 event hook */
static SDL_X11EventHook g_X11EventHook = NULL;
static void *g_X11EventHookData = NULL;

void SDL_SetX11EventHook(SDL_X11EventHook callback, void *userdata)
{
    g_X11EventHook = callback;
    g_X11EventHookData = userdata;
}

#ifdef SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS
static void X11_HandleGenericEvent(SDL_VideoDevice *_this, XEvent *xev)
{
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;

    /* event is a union, so cookie == &event, but this is type safe. */
    XGenericEventCookie *cookie = &xev->xcookie;
    if (X11_XGetEventData(videodata->display, cookie)) {
        if (!g_X11EventHook || g_X11EventHook(g_X11EventHookData, xev)) {
            X11_HandleXinput2Event(_this, cookie);
        }
        X11_XFreeEventData(videodata->display, cookie);
    }
}
#endif /* SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS */

static unsigned X11_GetNumLockModifierMask(SDL_VideoDevice *_this)
{
    SDL_VideoData *viddata = _this->driverdata;
    Display *display = viddata->display;
    unsigned num_mask = 0;
    int i, j;
    XModifierKeymap *xmods;
    unsigned n;

    xmods = X11_XGetModifierMapping(display);
    n = xmods->max_keypermod;
    for (i = 3; i < 8; i++) {
        for (j = 0; j < n; j++) {
            KeyCode kc = xmods->modifiermap[i * n + j];
            if (viddata->key_layout[kc] == SDL_SCANCODE_NUMLOCKCLEAR) {
                num_mask = 1 << i;
                break;
            }
        }
    }
    X11_XFreeModifiermap(xmods);

    return num_mask;
}

static unsigned X11_GetScrollLockModifierMask(SDL_VideoDevice *_this)
{
    SDL_VideoData *viddata = _this->driverdata;
    Display *display = viddata->display;
    unsigned num_mask = 0;
    int i, j;
    XModifierKeymap *xmods;
    unsigned n;

    xmods = X11_XGetModifierMapping(display);
    n = xmods->max_keypermod;
    for (i = 3; i < 8; i++) {
        for (j = 0; j < n; j++) {
            KeyCode kc = xmods->modifiermap[i * n + j];
            if (viddata->key_layout[kc] == SDL_SCANCODE_SCROLLLOCK) {
                num_mask = 1 << i;
                break;
            }
        }
    }
    X11_XFreeModifiermap(xmods);

    return num_mask;
}

void X11_ReconcileKeyboardState(SDL_VideoDevice *_this)
{
    SDL_VideoData *viddata = _this->driverdata;
    Display *display = viddata->display;
    char keys[32];
    int keycode;
    Window junk_window;
    int x, y;
    unsigned int mask;
    const Uint8 *keyboardState;

    X11_XQueryKeymap(display, keys);

    /* Sync up the keyboard modifier state */
    if (X11_XQueryPointer(display, DefaultRootWindow(display), &junk_window, &junk_window, &x, &y, &x, &y, &mask)) {
        SDL_ToggleModState(SDL_KMOD_CAPS, (mask & LockMask) ? SDL_TRUE : SDL_FALSE);
        SDL_ToggleModState(SDL_KMOD_NUM, (mask & X11_GetNumLockModifierMask(_this)) ? SDL_TRUE : SDL_FALSE);
        SDL_ToggleModState(SDL_KMOD_SCROLL, (mask & X11_GetScrollLockModifierMask(_this)) ? SDL_TRUE : SDL_FALSE);
    }

    keyboardState = SDL_GetKeyboardState(0);
    for (keycode = 0; keycode < SDL_arraysize(viddata->key_layout); ++keycode) {
        SDL_Scancode scancode = viddata->key_layout[keycode];
        SDL_bool x11KeyPressed = (keys[keycode / 8] & (1 << (keycode % 8))) != 0;
        SDL_bool sdlKeyPressed = keyboardState[scancode] == SDL_PRESSED;

        if (x11KeyPressed && !sdlKeyPressed) {
            /* Only update modifier state for keys that are pressed in another application */
            switch (SDL_GetKeyFromScancode(scancode)) {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
            case SDLK_LALT:
            case SDLK_RALT:
            case SDLK_LGUI:
            case SDLK_RGUI:
            case SDLK_MODE:
                SDL_SendKeyboardKey(0, SDL_PRESSED, scancode);
                break;
            default:
                break;
            }
        } else if (!x11KeyPressed && sdlKeyPressed) {
            SDL_SendKeyboardKey(0, SDL_RELEASED, scancode);
        }
    }
}

static void X11_DispatchFocusIn(SDL_VideoDevice *_this, SDL_WindowData *data)
{
#ifdef DEBUG_XEVENTS
    printf("window %p: Dispatching FocusIn\n", data);
#endif
    SDL_SetKeyboardFocus(data->window);
    X11_ReconcileKeyboardState(_this);
#ifdef X_HAVE_UTF8_STRING
    if (data->ic) {
        X11_XSetICFocus(data->ic);
    }
#endif
#ifdef SDL_USE_IME
    SDL_IME_SetFocus(SDL_TRUE);
#endif
    if (data->flashing_window) {
        X11_FlashWindow(_this, data->window, SDL_FLASH_CANCEL);
    }
}

static void X11_DispatchFocusOut(SDL_VideoDevice *_this, SDL_WindowData *data)
{
#ifdef DEBUG_XEVENTS
    printf("window %p: Dispatching FocusOut\n", data);
#endif
    /* If another window has already processed a focus in, then don't try to
     * remove focus here.  Doing so will incorrectly remove focus from that
     * window, and the focus lost event for this window will have already
     * been dispatched anyway. */
    if (data->window == SDL_GetKeyboardFocus()) {
        SDL_SetKeyboardFocus(NULL);
    }
#ifdef X_HAVE_UTF8_STRING
    if (data->ic) {
        X11_XUnsetICFocus(data->ic);
    }
#endif
#ifdef SDL_USE_IME
    SDL_IME_SetFocus(SDL_FALSE);
#endif
}

static void X11_DispatchMapNotify(SDL_WindowData *data)
{
    SDL_Window *window = data->window;
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_SHOWN, 0, 0);
    if (!(window->flags & SDL_WINDOW_HIDDEN) && (window->flags & SDL_WINDOW_INPUT_FOCUS)) {
        SDL_UpdateWindowGrab(window);
    }
}

static void X11_DispatchUnmapNotify(SDL_WindowData *data)
{
    SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_HIDDEN, 0, 0);
    SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
}

static void InitiateWindowMove(SDL_VideoDevice *_this, const SDL_WindowData *data, const SDL_Point *point)
{
    SDL_VideoData *viddata = _this->driverdata;
    SDL_Window *window = data->window;
    Display *display = viddata->display;
    XEvent evt;

    /* !!! FIXME: we need to regrab this if necessary when the drag is done. */
    X11_XUngrabPointer(display, 0L);
    X11_XFlush(display);

    evt.xclient.type = ClientMessage;
    evt.xclient.window = data->xwindow;
    evt.xclient.message_type = X11_XInternAtom(display, "_NET_WM_MOVERESIZE", True);
    evt.xclient.format = 32;
    evt.xclient.data.l[0] = (size_t)window->x + point->x;
    evt.xclient.data.l[1] = (size_t)window->y + point->y;
    evt.xclient.data.l[2] = _NET_WM_MOVERESIZE_MOVE;
    evt.xclient.data.l[3] = Button1;
    evt.xclient.data.l[4] = 0;
    X11_XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &evt);

    X11_XSync(display, 0);
}

static void InitiateWindowResize(SDL_VideoDevice *_this, const SDL_WindowData *data, const SDL_Point *point, int direction)
{
    SDL_VideoData *viddata = _this->driverdata;
    SDL_Window *window = data->window;
    Display *display = viddata->display;
    XEvent evt;

    if (direction < _NET_WM_MOVERESIZE_SIZE_TOPLEFT || direction > _NET_WM_MOVERESIZE_SIZE_LEFT) {
        return;
    }

    /* !!! FIXME: we need to regrab this if necessary when the drag is done. */
    X11_XUngrabPointer(display, 0L);
    X11_XFlush(display);

    evt.xclient.type = ClientMessage;
    evt.xclient.window = data->xwindow;
    evt.xclient.message_type = X11_XInternAtom(display, "_NET_WM_MOVERESIZE", True);
    evt.xclient.format = 32;
    evt.xclient.data.l[0] = (size_t)window->x + point->x;
    evt.xclient.data.l[1] = (size_t)window->y + point->y;
    evt.xclient.data.l[2] = direction;
    evt.xclient.data.l[3] = Button1;
    evt.xclient.data.l[4] = 0;
    X11_XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &evt);

    X11_XSync(display, 0);
}

SDL_bool X11_ProcessHitTest(SDL_VideoDevice *_this, SDL_WindowData *data, const float x, const float y, SDL_bool force_new_result)
{
    SDL_Window *window = data->window;
    if (!window->hit_test) return SDL_FALSE;
    const SDL_Point point = { x, y };
    SDL_HitTestResult rc = window->hit_test(window, &point, window->hit_test_data);
    if (!force_new_result && rc == data->hit_test_result) {
        return SDL_TRUE;
    }
    X11_SetHitTestCursor(rc);
    data->hit_test_result = rc;
    return SDL_TRUE;
}

SDL_bool X11_TriggerHitTestAction(SDL_VideoDevice *_this, const SDL_WindowData *data, const float x, const float y)
{
    SDL_Window *window = data->window;

    if (window->hit_test) {
        const SDL_Point point = { x, y };
        static const int directions[] = {
            _NET_WM_MOVERESIZE_SIZE_TOPLEFT, _NET_WM_MOVERESIZE_SIZE_TOP,
            _NET_WM_MOVERESIZE_SIZE_TOPRIGHT, _NET_WM_MOVERESIZE_SIZE_RIGHT,
            _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, _NET_WM_MOVERESIZE_SIZE_BOTTOM,
            _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT, _NET_WM_MOVERESIZE_SIZE_LEFT
        };

        switch (data->hit_test_result) {
        case SDL_HITTEST_DRAGGABLE:
            InitiateWindowMove(_this, data, &point);
            return SDL_TRUE;

        case SDL_HITTEST_RESIZE_TOPLEFT:
        case SDL_HITTEST_RESIZE_TOP:
        case SDL_HITTEST_RESIZE_TOPRIGHT:
        case SDL_HITTEST_RESIZE_RIGHT:
        case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
        case SDL_HITTEST_RESIZE_BOTTOM:
        case SDL_HITTEST_RESIZE_BOTTOMLEFT:
        case SDL_HITTEST_RESIZE_LEFT:
            InitiateWindowResize(_this, data, &point, directions[data->hit_test_result - SDL_HITTEST_RESIZE_TOPLEFT]);
            return SDL_TRUE;

        default:
            return SDL_FALSE;
        }
    }

    return SDL_FALSE;
}

static void X11_UpdateUserTime(SDL_WindowData *data, const unsigned long latest)
{
    if (latest && (latest != data->user_time)) {
        SDL_VideoData *videodata = data->videodata;
        Display *display = videodata->display;
        X11_XChangeProperty(display, data->xwindow, videodata->_NET_WM_USER_TIME,
                            XA_CARDINAL, 32, PropModeReplace,
                            (const unsigned char *)&latest, 1);
#ifdef DEBUG_XEVENTS
        printf("window %p: updating _NET_WM_USER_TIME to %lu\n", data, latest);
#endif
        data->user_time = latest;
    }
}

static void X11_HandleClipboardEvent(SDL_VideoDevice *_this, const XEvent *xevent)
{
    int i;
    SDL_VideoData *videodata = _this->driverdata;
    Display *display = videodata->display;

    SDL_assert(videodata->clipboard_window != None);
    SDL_assert(xevent->xany.window == videodata->clipboard_window);

    switch (xevent->type) {
        /* Copy the selection from our own CUTBUFFER to the requested property */
    case SelectionRequest:
    {
        const XSelectionRequestEvent *req = &xevent->xselectionrequest;
        XEvent sevent;
        int mime_formats;
        unsigned char *seln_data;
        size_t seln_length = 0;
        Atom XA_TARGETS = X11_XInternAtom(display, "TARGETS", 0);
        SDLX11_ClipboardData *clipboard;

#ifdef DEBUG_XEVENTS
        char *atom_name;
        atom_name = X11_XGetAtomName(display, req->target);
        printf("window CLIPBOARD: SelectionRequest (requestor = %ld, target = %ld, mime_type = %s)\n",
               req->requestor, req->target, atom_name);
        if (atom_name) {
            X11_XFree(atom_name);
        }
#endif

        if (req->selection == XA_PRIMARY) {
            clipboard = &videodata->primary_selection;
        } else {
            clipboard = &videodata->clipboard;
        }

        SDL_zero(sevent);
        sevent.xany.type = SelectionNotify;
        sevent.xselection.selection = req->selection;
        sevent.xselection.target = None;
        sevent.xselection.property = None; /* tell them no by default */
        sevent.xselection.requestor = req->requestor;
        sevent.xselection.time = req->time;

        /* !!! FIXME: We were probably storing this on the root window
           because an SDL window might go away...? but we don't have to do
           this now (or ever, really). */

        if (req->target == XA_TARGETS) {
            Atom *supportedFormats;
            supportedFormats = SDL_malloc((clipboard->mime_count + 1) * sizeof(Atom));
            supportedFormats[0] = XA_TARGETS;
            mime_formats = 1;
            for (i = 0; i < clipboard->mime_count; ++i) {
                supportedFormats[mime_formats++] = X11_XInternAtom(display, clipboard->mime_types[i], False);
            }
            X11_XChangeProperty(display, req->requestor, req->property,
                                XA_ATOM, 32, PropModeReplace,
                                (unsigned char *)supportedFormats,
                                mime_formats);
            sevent.xselection.property = req->property;
            sevent.xselection.target = XA_TARGETS;
            SDL_free(supportedFormats);
        } else {
            if (clipboard->callback) {
                for (i = 0; i < clipboard->mime_count; ++i) {
                    const char *mime_type = clipboard->mime_types[i];
                    if (X11_XInternAtom(display, mime_type, False) != req->target) {
                        continue;
                    }

                    /* FIXME: We don't support the X11 INCR protocol for large clipboards. Do we want that? - Yes, yes we do. */
                    /* This is a safe cast, XChangeProperty() doesn't take a const value, but it doesn't modify the data */
                    seln_data = (unsigned char *)clipboard->callback(clipboard->userdata, mime_type, &seln_length);
                    if (seln_data) {
                        X11_XChangeProperty(display, req->requestor, req->property,
                                            req->target, 8, PropModeReplace,
                                            seln_data, seln_length);
                        sevent.xselection.property = req->property;
                        sevent.xselection.target = req->target;
                    }
                    break;
                }
            }
        }
        X11_XSendEvent(display, req->requestor, False, 0, &sevent);
        X11_XSync(display, False);
    } break;

    case SelectionNotify:
    {
#ifdef DEBUG_XEVENTS
        printf("window CLIPBOARD: SelectionNotify (requestor = %ld, target = %ld)\n",
               xevent->xselection.requestor, xevent->xselection.target);
#endif
        videodata->selection_waiting = SDL_FALSE;
    } break;

    case SelectionClear:
    {
        /* !!! FIXME: cache atoms */
        Atom XA_CLIPBOARD = X11_XInternAtom(display, "CLIPBOARD", 0);
        SDLX11_ClipboardData *clipboard = NULL;

#ifdef DEBUG_XEVENTS
        printf("window CLIPBOARD: SelectionClear (requestor = %ld, target = %ld)\n",
               xevent->xselection.requestor, xevent->xselection.target);
#endif

        if (xevent->xselectionclear.selection == XA_PRIMARY) {
            clipboard = &videodata->primary_selection;
        } else if (XA_CLIPBOARD != None && xevent->xselectionclear.selection == XA_CLIPBOARD) {
            clipboard = &videodata->clipboard;
        }
        if (clipboard && clipboard->callback) {
            if (clipboard->sequence) {
                SDL_CancelClipboardData(clipboard->sequence);
            } else {
                SDL_free(clipboard->userdata);
            }
            SDL_zerop(clipboard);
        }
    } break;
    }
}

static Bool isMapNotify(Display *display, XEvent *ev, XPointer arg)
{
    XUnmapEvent *unmap;

    unmap = (XUnmapEvent *)arg;

    return ev->type == MapNotify &&
           ev->xmap.window == unmap->window &&
           ev->xmap.serial == unmap->serial;
}

static Bool isReparentNotify(Display *display, XEvent *ev, XPointer arg)
{
    XUnmapEvent *unmap;

    unmap = (XUnmapEvent *)arg;

    return ev->type == ReparentNotify &&
           ev->xreparent.window == unmap->window &&
           ev->xreparent.serial == unmap->serial;
}

static SDL_bool IsHighLatin1(const char *string, int length)
{
    while (length-- > 0) {
        Uint8 ch = (Uint8)*string;
        if (ch >= 0x80) {
            return SDL_TRUE;
        }
        ++string;
    }
    return SDL_FALSE;
}

static int XLookupStringAsUTF8(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return, XComposeStatus *status_in_out)
{
    int result = X11_XLookupString(event_struct, buffer_return, bytes_buffer, keysym_return, status_in_out);
    if (IsHighLatin1(buffer_return, result)) {
        char *utf8_text = SDL_iconv_string("UTF-8", "ISO-8859-1", buffer_return, result + 1);
        if (utf8_text) {
            SDL_strlcpy(buffer_return, utf8_text, bytes_buffer);
            SDL_free(utf8_text);
            return SDL_strlen(buffer_return);
        } else {
            return 0;
        }
    }
    return result;
}

SDL_WindowData *X11_FindWindow(SDL_VideoDevice *_this, Window window)
{
    const SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    int i;

    if (videodata && videodata->windowlist) {
        for (i = 0; i < videodata->numwindows; ++i) {
            if ((videodata->windowlist[i] != NULL) &&
                (videodata->windowlist[i]->xwindow == window)) {
                return videodata->windowlist[i];
            }
        }
    }
    return NULL;
}

void X11_HandleButtonPress(SDL_VideoDevice *_this, SDL_WindowData *windowdata, int button, const float x, const float y, const unsigned long time)
{
    SDL_Window *window = windowdata->window;
    const SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    Display *display = videodata->display;
    int xticks = 0, yticks = 0;
#ifdef DEBUG_XEVENTS
    printf("window %p: ButtonPress (X11 button = %d)\n", window, button);
#endif
    if (X11_IsWheelEvent(display, button, &xticks, &yticks)) {
        SDL_SendMouseWheel(0, window, 0, (float)-xticks, (float)yticks, SDL_MOUSEWHEEL_NORMAL);
    } else {
        SDL_bool ignore_click = SDL_FALSE;
        if (button == Button1) {
            if (X11_TriggerHitTestAction(_this, windowdata, x, y)) {
                SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_HIT_TEST, 0, 0);
                return; /* don't pass this event on to app. */
            }
        } else if (button > 7) {
            /* X button values 4-7 are used for scrolling, so X1 is 8, X2 is 9, ...
               => subtract (8-SDL_BUTTON_X1) to get value SDL expects */
            button -= (8 - SDL_BUTTON_X1);
        }
        if (windowdata->last_focus_event_time) {
            const int X11_FOCUS_CLICK_TIMEOUT = 10;
            if (SDL_GetTicks() < (windowdata->last_focus_event_time + X11_FOCUS_CLICK_TIMEOUT)) {
                ignore_click = !SDL_GetHintBoolean(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, SDL_FALSE);
            }
            windowdata->last_focus_event_time = 0;
        }
        if (!ignore_click) {
            SDL_SendMouseButton(0, window, 0, SDL_PRESSED, button);
        }
    }
    X11_UpdateUserTime(windowdata, time);
}

void X11_HandleButtonRelease(SDL_VideoDevice *_this, SDL_WindowData *windowdata, int button)
{
    SDL_Window *window = windowdata->window;
    const SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;
    Display *display = videodata->display;
    /* The X server sends a Release event for each Press for wheels. Ignore them. */
    int xticks = 0, yticks = 0;
#ifdef DEBUG_XEVENTS
    printf("window %p: ButtonRelease (X11 button = %d)\n", data, xevent->xbutton.button);
#endif
    if (!X11_IsWheelEvent(display, button, &xticks, &yticks)) {
        if (button > 7) {
            /* see explanation at case ButtonPress */
            button -= (8 - SDL_BUTTON_X1);
        }
        SDL_SendMouseButton(0, window, 0, SDL_RELEASED, button);
    }
}

void X11_GetBorderValues(SDL_WindowData *data)
{
    SDL_VideoData *videodata = data->videodata;
    Display *display = videodata->display;

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *property;

    /* Some compositors will send extents even when the border hint is turned off. Ignore them in this case. */
    if (!(data->window->flags & SDL_WINDOW_BORDERLESS)) {
        if (X11_XGetWindowProperty(display, data->xwindow, videodata->_NET_FRAME_EXTENTS, 0, 16, 0, XA_CARDINAL, &type, &format, &nitems, &bytes_after, &property) == Success) {
            if (type != None && nitems == 4) {
                data->border_left = (int)((long *)property)[0];
                data->border_right = (int)((long *)property)[1];
                data->border_top = (int)((long *)property)[2];
                data->border_bottom = (int)((long *)property)[3];
            }
            X11_XFree(property);

#ifdef DEBUG_XEVENTS
            printf("New _NET_FRAME_EXTENTS: left=%d right=%d, top=%d, bottom=%d\n", data->border_left, data->border_right, data->border_top, data->border_bottom);
#endif
        }
    } else {
        data->border_left = data->border_top = data->border_right = data->border_bottom = 0;
    }
}

static void X11_DispatchEvent(SDL_VideoDevice *_this, XEvent *xevent)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display;
    SDL_WindowData *data;
    int orig_event_type;
    KeyCode orig_keycode;
    XClientMessageEvent m;
    int i;

    SDL_assert(videodata != NULL);
    display = videodata->display;

    /* Save the original keycode for dead keys, which are filtered out by
       the XFilterEvent() call below.
    */
    orig_event_type = xevent->type;
    if (orig_event_type == KeyPress || orig_event_type == KeyRelease) {
        orig_keycode = xevent->xkey.keycode;
    } else {
        orig_keycode = 0;
    }

    /* filter events catches XIM events and sends them to the correct handler */
    if (X11_XFilterEvent(xevent, None) == True) {
#if 0
        printf("Filtered event type = %d display = %d window = %d\n",
               xevent->type, xevent->xany.display, xevent->xany.window);
#endif
        /* Make sure dead key press/release events are sent */
        /* But only if we're using one of the DBus IMEs, otherwise
           some XIM IMEs will generate duplicate events */
        if (orig_keycode) {
#if defined(HAVE_IBUS_IBUS_H) || defined(HAVE_FCITX)
            SDL_Scancode scancode = videodata->key_layout[orig_keycode];
            videodata->filter_code = orig_keycode;
            videodata->filter_time = xevent->xkey.time;

            if (orig_event_type == KeyPress) {
                SDL_SendKeyboardKey(0, SDL_PRESSED, scancode);
            } else {
                SDL_SendKeyboardKey(0, SDL_RELEASED, scancode);
            }
#endif
        }
        return;
    }

#ifdef SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS
    if (xevent->type == GenericEvent) {
        X11_HandleGenericEvent(_this, xevent);
        return;
    }
#endif

    /* Calling the event hook for generic events happens in X11_HandleGenericEvent(), where the event data is available */
    if (g_X11EventHook) {
        if (!g_X11EventHook(g_X11EventHookData, xevent)) {
            return;
        }
    }

#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    if (videodata->xrandr_event_base && (xevent->type == (videodata->xrandr_event_base + RRNotify))) {
        X11_HandleXRandREvent(_this, xevent);
    }
#endif

#if 0
    printf("type = %d display = %d window = %d\n",
           xevent->type, xevent->xany.display, xevent->xany.window);
#endif

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
    if (SDL_X11_HAVE_XFIXES &&
        xevent->type == X11_GetXFixesSelectionNotifyEvent()) {
        XFixesSelectionNotifyEvent *ev = (XFixesSelectionNotifyEvent *)xevent;

        /* !!! FIXME: cache atoms */
        Atom XA_CLIPBOARD = X11_XInternAtom(display, "CLIPBOARD", 0);

#ifdef DEBUG_XEVENTS
        printf("window CLIPBOARD: XFixesSelectionNotify (selection = %s)\n",
               X11_XGetAtomName(display, ev->selection));
#endif

        if (ev->selection == XA_PRIMARY ||
            (XA_CLIPBOARD != None && ev->selection == XA_CLIPBOARD)) {
            SDL_SendClipboardUpdate();
            return;
        }
    }
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */

    if ((videodata->clipboard_window != None) &&
        (videodata->clipboard_window == xevent->xany.window)) {
        X11_HandleClipboardEvent(_this, xevent);
        return;
    }

    data = X11_FindWindow(_this, xevent->xany.window);

    if (!data) {
        /* The window for KeymapNotify, etc events is 0 */
        if (xevent->type == KeymapNotify) {
#ifdef DEBUG_XEVENTS
            printf("window %p: KeymapNotify!\n", data);
#endif
            if (SDL_GetKeyboardFocus() != NULL) {
                X11_ReconcileKeyboardState(_this);
            }
        } else if (xevent->type == MappingNotify) {
            /* Has the keyboard layout changed? */
            const int request = xevent->xmapping.request;

#ifdef DEBUG_XEVENTS
            printf("window %p: MappingNotify!\n", data);
#endif
            if ((request == MappingKeyboard) || (request == MappingModifier)) {
                X11_XRefreshKeyboardMapping(&xevent->xmapping);
            }

            X11_UpdateKeymap(_this, SDL_TRUE);
        } else if (xevent->type == PropertyNotify && videodata && videodata->windowlist) {
            char *name_of_atom = X11_XGetAtomName(display, xevent->xproperty.atom);

            if (SDL_strncmp(name_of_atom, "_ICC_PROFILE", sizeof("_ICC_PROFILE") - 1) == 0) {
                XWindowAttributes attrib;
                int screennum;
                for (i = 0; i < videodata->numwindows; ++i) {
                    if (videodata->windowlist[i] != NULL) {
                        data = videodata->windowlist[i];
                        X11_XGetWindowAttributes(display, data->xwindow, &attrib);
                        screennum = X11_XScreenNumberOfScreen(attrib.screen);
                        if (screennum == 0 && SDL_strcmp(name_of_atom, "_ICC_PROFILE") == 0) {
                            SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_ICCPROF_CHANGED, 0, 0);
                        } else if (SDL_strncmp(name_of_atom, "_ICC_PROFILE_", sizeof("_ICC_PROFILE_") - 1) == 0 && SDL_strlen(name_of_atom) > sizeof("_ICC_PROFILE_") - 1) {
                            int iccscreennum = SDL_atoi(&name_of_atom[sizeof("_ICC_PROFILE_") - 1]);

                            if (screennum == iccscreennum) {
                                SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_ICCPROF_CHANGED, 0, 0);
                            }
                        }
                    }
                }
            }

            if (name_of_atom) {
                X11_XFree(name_of_atom);
            }
        }
        return;
    }

    switch (xevent->type) {

        /* Gaining mouse coverage? */
    case EnterNotify:
    {
        SDL_Mouse *mouse = SDL_GetMouse();
#ifdef DEBUG_XEVENTS
        printf("window %p: EnterNotify! (%d,%d,%d)\n", data,
               xevent->xcrossing.x,
               xevent->xcrossing.y,
               xevent->xcrossing.mode);
        if (xevent->xcrossing.mode == NotifyGrab) {
            printf("Mode: NotifyGrab\n");
        }
        if (xevent->xcrossing.mode == NotifyUngrab) {
            printf("Mode: NotifyUngrab\n");
        }
#endif
        SDL_SetMouseFocus(data->window);

        mouse->last_x = xevent->xcrossing.x;
        mouse->last_y = xevent->xcrossing.y;

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
        {
            /* Only create the barriers if we have input focus */
            SDL_WindowData *windowdata = data->window->driverdata;
            if ((data->pointer_barrier_active == SDL_TRUE) && windowdata->window->flags & SDL_WINDOW_INPUT_FOCUS) {
                X11_ConfineCursorWithFlags(_this, windowdata->window, &windowdata->barrier_rect, X11_BARRIER_HANDLED_BY_EVENT);
            }
        }
#endif

        if (!mouse->relative_mode) {
            SDL_SendMouseMotion(0, data->window, 0, 0, (float)xevent->xcrossing.x, (float)xevent->xcrossing.y);
        }

        /* We ungrab in LeaveNotify, so we may need to grab again here */
        SDL_UpdateWindowGrab(data->window);

        X11_ProcessHitTest(_this, data, mouse->last_x, mouse->last_y, SDL_TRUE);
    } break;
        /* Losing mouse coverage? */
    case LeaveNotify:
    {
#ifdef DEBUG_XEVENTS
        printf("window %p: LeaveNotify! (%d,%d,%d)\n", data,
               xevent->xcrossing.x,
               xevent->xcrossing.y,
               xevent->xcrossing.mode);
        if (xevent->xcrossing.mode == NotifyGrab) {
            printf("Mode: NotifyGrab\n");
        }
        if (xevent->xcrossing.mode == NotifyUngrab) {
            printf("Mode: NotifyUngrab\n");
        }
#endif
        if (!SDL_GetMouse()->relative_mode) {
            SDL_SendMouseMotion(0, data->window, 0, 0, (float)xevent->xcrossing.x, (float)xevent->xcrossing.y);
        }

        if (xevent->xcrossing.mode != NotifyGrab &&
            xevent->xcrossing.mode != NotifyUngrab &&
            xevent->xcrossing.detail != NotifyInferior) {

            /* In order for interaction with the window decorations and menu to work properly
               on Mutter, we need to ungrab the keyboard when the the mouse leaves. */
            if (!(data->window->flags & SDL_WINDOW_FULLSCREEN)) {
                X11_SetWindowKeyboardGrab(_this, data->window, SDL_FALSE);
            }

            SDL_SetMouseFocus(NULL);
        }
    } break;

        /* Gaining input focus? */
    case FocusIn:
    {
        if (xevent->xfocus.mode == NotifyGrab || xevent->xfocus.mode == NotifyUngrab) {
            /* Someone is handling a global hotkey, ignore it */
#ifdef DEBUG_XEVENTS
            printf("window %p: FocusIn (NotifyGrab/NotifyUngrab, ignoring)\n", data);
#endif
            break;
        }

        if (xevent->xfocus.detail == NotifyInferior || xevent->xfocus.detail == NotifyPointer) {
#ifdef DEBUG_XEVENTS
            printf("window %p: FocusIn (NotifyInferior/NotifyPointer, ignoring)\n", data);
#endif
            break;
        }
#ifdef DEBUG_XEVENTS
        printf("window %p: FocusIn!\n", data);
#endif
        if (!videodata->last_mode_change_deadline) /* no recent mode changes */ {
            data->pending_focus = PENDING_FOCUS_NONE;
            data->pending_focus_time = 0;
            X11_DispatchFocusIn(_this, data);
        } else {
            data->pending_focus = PENDING_FOCUS_IN;
            data->pending_focus_time = SDL_GetTicks() + PENDING_FOCUS_TIME;
        }
        data->last_focus_event_time = SDL_GetTicks();
    } break;

        /* Losing input focus? */
    case FocusOut:
    {
        if (xevent->xfocus.mode == NotifyGrab || xevent->xfocus.mode == NotifyUngrab) {
            /* Someone is handling a global hotkey, ignore it */
#ifdef DEBUG_XEVENTS
            printf("window %p: FocusOut (NotifyGrab/NotifyUngrab, ignoring)\n", data);
#endif
            break;
        }
        if (xevent->xfocus.detail == NotifyInferior || xevent->xfocus.detail == NotifyPointer) {
            /* We still have focus if a child gets focus. We also don't
               care about the position of the pointer when the keyboard
               focus changed. */
#ifdef DEBUG_XEVENTS
            printf("window %p: FocusOut (NotifyInferior/NotifyPointer, ignoring)\n", data);
#endif
            break;
        }
#ifdef DEBUG_XEVENTS
        printf("window %p: FocusOut!\n", data);
#endif
        if (!videodata->last_mode_change_deadline) /* no recent mode changes */ {
            data->pending_focus = PENDING_FOCUS_NONE;
            data->pending_focus_time = 0;
            X11_DispatchFocusOut(_this, data);
        } else {
            data->pending_focus = PENDING_FOCUS_OUT;
            data->pending_focus_time = SDL_GetTicks() + PENDING_FOCUS_TIME;
        }

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
        /* Disable confinement if it is activated. */
        if (data->pointer_barrier_active == SDL_TRUE) {
            X11_ConfineCursorWithFlags(_this, data->window, NULL, X11_BARRIER_HANDLED_BY_EVENT);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */
    } break;

        /* Key press/release? */
    case KeyPress:
    case KeyRelease:
    {
        KeyCode keycode = xevent->xkey.keycode;
        KeySym keysym = NoSymbol;
        char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
        Status status = 0;
        SDL_bool handled_by_ime = SDL_FALSE;

#ifdef DEBUG_XEVENTS
        printf("window %p: %s (X11 keycode = 0x%X)\n", data, (xevent->type == KeyPress ? "KeyPress" : "KeyRelease"), xevent->xkey.keycode);
#endif
#ifdef DEBUG_SCANCODES
        if (videodata->key_layout[keycode] == SDL_SCANCODE_UNKNOWN && keycode) {
            int min_keycode, max_keycode;
            X11_XDisplayKeycodes(display, &min_keycode, &max_keycode);
            keysym = X11_KeyCodeToSym(_this, keycode, xevent->xkey.state >> 13);
            SDL_Log("The key you just pressed is not recognized by SDL. To help get this fixed, please report this to the SDL forums/mailing list <https://discourse.libsdl.org/> X11 KeyCode %d (%d), X11 KeySym 0x%lX (%s).\n",
                    keycode, keycode - min_keycode, keysym,
                    X11_XKeysymToString(keysym));
        }
#endif /* DEBUG SCANCODES */

        SDL_zeroa(text);
#ifdef X_HAVE_UTF8_STRING
        if (data->ic && xevent->type == KeyPress) {
            X11_Xutf8LookupString(data->ic, &xevent->xkey, text, sizeof(text),
                                  &keysym, &status);
        } else {
            XLookupStringAsUTF8(&xevent->xkey, text, sizeof(text), &keysym, NULL);
        }
#else
        XLookupStringAsUTF8(&xevent->xkey, text, sizeof(text), &keysym, NULL);
#endif

#ifdef SDL_USE_IME
        if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
            handled_by_ime = SDL_IME_ProcessKeyEvent(keysym, keycode, (xevent->type == KeyPress ? SDL_PRESSED : SDL_RELEASED));
        }
#endif
        if (!handled_by_ime) {
            if (xevent->type == KeyPress) {
                /* Don't send the key if it looks like a duplicate of a filtered key sent by an IME */
                if (xevent->xkey.keycode != videodata->filter_code || xevent->xkey.time != videodata->filter_time) {
                    SDL_SendKeyboardKey(0, SDL_PRESSED, videodata->key_layout[keycode]);
                }
                if (*text) {
                    SDL_SendKeyboardText(text);
                }
            } else {
                if (X11_KeyRepeat(display, xevent)) {
                    /* We're about to get a repeated key down, ignore the key up */
                    break;
                }
                SDL_SendKeyboardKey(0, SDL_RELEASED, videodata->key_layout[keycode]);
            }
        }

        if (xevent->type == KeyPress) {
            X11_UpdateUserTime(data, xevent->xkey.time);
        }
    } break;

        /* Have we been iconified? */
    case UnmapNotify:
    {
        XEvent ev;

#ifdef DEBUG_XEVENTS
        printf("window %p: UnmapNotify!\n", data);
#endif

        if (X11_XCheckIfEvent(display, &ev, &isReparentNotify, (XPointer)&xevent->xunmap)) {
            X11_XCheckIfEvent(display, &ev, &isMapNotify, (XPointer)&xevent->xunmap);
        } else {
            X11_DispatchUnmapNotify(data);
        }

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
        /* Disable confinement if the window gets hidden. */
        if (data->pointer_barrier_active == SDL_TRUE) {
            X11_ConfineCursorWithFlags(_this, data->window, NULL, X11_BARRIER_HANDLED_BY_EVENT);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */
    } break;

        /* Have we been restored? */
    case MapNotify:
    {
#ifdef DEBUG_XEVENTS
        printf("window %p: MapNotify!\n", data);
#endif
        X11_DispatchMapNotify(data);

#ifdef SDL_VIDEO_DRIVER_X11_XFIXES
        /* Enable confinement if it was activated. */
        if (data->pointer_barrier_active == SDL_TRUE) {
            X11_ConfineCursorWithFlags(_this, data->window, &data->barrier_rect, X11_BARRIER_HANDLED_BY_EVENT);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XFIXES */
    } break;

        /* Have we been resized or moved? */
    case ConfigureNotify:
    {
#ifdef DEBUG_XEVENTS
        printf("window %p: ConfigureNotify! (position: %d,%d, size: %dx%d)\n", data,
               xevent->xconfigure.x, xevent->xconfigure.y,
               xevent->xconfigure.width, xevent->xconfigure.height);
#endif
            /* Real configure notify events are relative to the parent, synthetic events are absolute. */
            if (!xevent->xconfigure.send_event)
        {
            unsigned int NumChildren;
            Window ChildReturn, Root, Parent;
            Window *Children;
            /* Translate these coordinates back to relative to root */
            X11_XQueryTree(data->videodata->display, xevent->xconfigure.window, &Root, &Parent, &Children, &NumChildren);
            X11_XTranslateCoordinates(xevent->xconfigure.display,
                                      Parent, DefaultRootWindow(xevent->xconfigure.display),
                                      xevent->xconfigure.x, xevent->xconfigure.y,
                                      &xevent->xconfigure.x, &xevent->xconfigure.y,
                                      &ChildReturn);
        }

        if (xevent->xconfigure.x != data->last_xconfigure.x ||
            xevent->xconfigure.y != data->last_xconfigure.y) {
            if (!data->disable_size_position_events) {
                SDL_Window *w;
                int x = xevent->xconfigure.x;
                int y = xevent->xconfigure.y;

                data->pending_operation &= ~X11_PENDING_OP_MOVE;
                SDL_GlobalToRelativeForWindow(data->window, x, y, &x, &y);
                SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_MOVED, x, y);

#ifdef SDL_USE_IME
                if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
                    /* Update IME candidate list position */
                    SDL_IME_UpdateTextRect(NULL);
                }
#endif
                for (w = data->window->first_child; w; w = w->next_sibling) {
                    /* Don't update hidden child windows, their relative position doesn't change */
                    if (!(w->flags & SDL_WINDOW_HIDDEN)) {
                        X11_UpdateWindowPosition(w, SDL_TRUE);
                    }
                }
            }
        }
        if (xevent->xconfigure.width != data->last_xconfigure.width ||
            xevent->xconfigure.height != data->last_xconfigure.height) {
            if (!data->disable_size_position_events) {
                data->pending_operation &= ~X11_PENDING_OP_RESIZE;
                SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_RESIZED,
                                    xevent->xconfigure.width,
                                    xevent->xconfigure.height);
            }
        }

        data->last_xconfigure = xevent->xconfigure;
    } break;

        /* Have we been requested to quit (or another client message?) */
    case ClientMessage:
    {

        static int xdnd_version = 0;

        if (xevent->xclient.message_type == videodata->XdndEnter) {

            SDL_bool use_list = xevent->xclient.data.l[1] & 1;
            data->xdnd_source = xevent->xclient.data.l[0];
            xdnd_version = (xevent->xclient.data.l[1] >> 24);
#ifdef DEBUG_XEVENTS
            printf("XID of source window : %ld\n", data->xdnd_source);
            printf("Protocol version to use : %d\n", xdnd_version);
            printf("More then 3 data types : %d\n", (int)use_list);
#endif

            if (use_list) {
                /* fetch conversion targets */
                SDL_x11Prop p;
                X11_ReadProperty(&p, display, data->xdnd_source, videodata->XdndTypeList);
                /* pick one */
                data->xdnd_req = X11_PickTarget(display, (Atom *)p.data, p.count);
                X11_XFree(p.data);
            } else {
                /* pick from list of three */
                data->xdnd_req = X11_PickTargetFromAtoms(display, xevent->xclient.data.l[2], xevent->xclient.data.l[3], xevent->xclient.data.l[4]);
            }
        } else if (xevent->xclient.message_type == videodata->XdndPosition) {

#ifdef DEBUG_XEVENTS
            Atom act = videodata->XdndActionCopy;
            if (xdnd_version >= 2) {
                act = xevent->xclient.data.l[4];
            }
            printf("Action requested by user is : %s\n", X11_XGetAtomName(display, act));
#endif
            {
                /* Drag and Drop position */
                int root_x, root_y, window_x, window_y;
                Window ChildReturn;
                root_x = xevent->xclient.data.l[2] >> 16;
                root_y = xevent->xclient.data.l[2] & 0xffff;
                /* Translate from root to current window position */
                X11_XTranslateCoordinates(display, DefaultRootWindow(display), data->xwindow,
                                          root_x, root_y, &window_x, &window_y, &ChildReturn);

                SDL_SendDropPosition(data->window, (float)window_x, (float)window_y);
            }

            /* reply with status */
            SDL_memset(&m, 0, sizeof(XClientMessageEvent));
            m.type = ClientMessage;
            m.display = xevent->xclient.display;
            m.window = xevent->xclient.data.l[0];
            m.message_type = videodata->XdndStatus;
            m.format = 32;
            m.data.l[0] = data->xwindow;
            m.data.l[1] = (data->xdnd_req != None);
            m.data.l[2] = 0; /* specify an empty rectangle */
            m.data.l[3] = 0;
            m.data.l[4] = videodata->XdndActionCopy; /* we only accept copying anyway */

            X11_XSendEvent(display, xevent->xclient.data.l[0], False, NoEventMask, (XEvent *)&m);
            X11_XFlush(display);
        } else if (xevent->xclient.message_type == videodata->XdndDrop) {
            if (data->xdnd_req == None) {
                /* say again - not interested! */
                SDL_memset(&m, 0, sizeof(XClientMessageEvent));
                m.type = ClientMessage;
                m.display = xevent->xclient.display;
                m.window = xevent->xclient.data.l[0];
                m.message_type = videodata->XdndFinished;
                m.format = 32;
                m.data.l[0] = data->xwindow;
                m.data.l[1] = 0;
                m.data.l[2] = None; /* fail! */
                X11_XSendEvent(display, xevent->xclient.data.l[0], False, NoEventMask, (XEvent *)&m);
            } else {
                /* convert */
                if (xdnd_version >= 1) {
                    X11_XConvertSelection(display, videodata->XdndSelection, data->xdnd_req, videodata->PRIMARY, data->xwindow, xevent->xclient.data.l[2]);
                } else {
                    X11_XConvertSelection(display, videodata->XdndSelection, data->xdnd_req, videodata->PRIMARY, data->xwindow, CurrentTime);
                }
            }
        } else if ((xevent->xclient.message_type == videodata->WM_PROTOCOLS) &&
                   (xevent->xclient.format == 32) &&
                   (xevent->xclient.data.l[0] == videodata->_NET_WM_PING)) {
            Window root = DefaultRootWindow(display);

#ifdef DEBUG_XEVENTS
            printf("window %p: _NET_WM_PING\n", data);
#endif
            xevent->xclient.window = root;
            X11_XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, xevent);
            break;
        }

        else if ((xevent->xclient.message_type == videodata->WM_PROTOCOLS) &&
                 (xevent->xclient.format == 32) &&
                 (xevent->xclient.data.l[0] == videodata->WM_DELETE_WINDOW)) {

#ifdef DEBUG_XEVENTS
            printf("window %p: WM_DELETE_WINDOW\n", data);
#endif
            SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
            break;
        } else if ((xevent->xclient.message_type == videodata->WM_PROTOCOLS) &&
                   (xevent->xclient.format == 32) &&
                   (xevent->xclient.data.l[0] == videodata->WM_TAKE_FOCUS)) {

#ifdef DEBUG_XEVENTS
            printf("window %p: WM_TAKE_FOCUS\n", data);
#endif
            SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_TAKE_FOCUS, 0, 0);
            break;
        }
    } break;

        /* Do we need to refresh ourselves? */
    case Expose:
    {
#ifdef DEBUG_XEVENTS
        printf("window %p: Expose (count = %d)\n", data, xevent->xexpose.count);
#endif
        SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_EXPOSED, 0, 0);
    } break;

    /* Use XInput2 instead of the xevents API if possible, for:
       - MotionNotify
       - ButtonPress
       - ButtonRelease
       XInput2 has more precise information, e.g., to distinguish different input devices. */
#ifndef SDL_VIDEO_DRIVER_X11_XINPUT2
    case MotionNotify:
    {
        SDL_Mouse *mouse = SDL_GetMouse();
        if (!mouse->relative_mode || mouse->relative_mode_warp) {
#ifdef DEBUG_MOTION
            printf("window %p: X11 motion: %d,%d\n", data, xevent->xmotion.x, xevent->xmotion.y);
#endif

            X11_ProcessHitTest(_this, data, (float)xevent->xmotion.x, (float)xevent->xmotion.y, SDL_FALSE);
            SDL_SendMouseMotion(0, data->window, 0, 0, (float)xevent->xmotion.x, (float)xevent->xmotion.y);
        }
    } break;

    case ButtonPress:
    {
        X11_HandleButtonPress(_this, data, xevent->xbutton.button,
                              xevent->xbutton.x, xevent->xbutton.y, xevent->xbutton.time);
    } break;

    case ButtonRelease:
    {
        X11_HandleButtonRelease(_this, data, xevent->xbutton.button);
    } break;
#endif /* !SDL_VIDEO_DRIVER_X11_XINPUT2 */

    case PropertyNotify:
    {
#ifdef DEBUG_XEVENTS
        unsigned char *propdata;
        int status, real_format;
        Atom real_type;
        unsigned long items_read, items_left;

        char *name = X11_XGetAtomName(display, xevent->xproperty.atom);
        if (name) {
            printf("window %p: PropertyNotify: %s %s time=%lu\n", data, name, (xevent->xproperty.state == PropertyDelete) ? "deleted" : "changed", xevent->xproperty.time);
            X11_XFree(name);
        }

        status = X11_XGetWindowProperty(display, data->xwindow, xevent->xproperty.atom, 0L, 8192L, False, AnyPropertyType, &real_type, &real_format, &items_read, &items_left, &propdata);
        if (status == Success && items_read > 0) {
            if (real_type == XA_INTEGER) {
                int *values = (int *)propdata;

                printf("{");
                for (i = 0; i < items_read; i++) {
                    printf(" %d", values[i]);
                }
                printf(" }\n");
            } else if (real_type == XA_CARDINAL) {
                if (real_format == 32) {
                    Uint32 *values = (Uint32 *)propdata;

                    printf("{");
                    for (i = 0; i < items_read; i++) {
                        printf(" %d", values[i]);
                    }
                    printf(" }\n");
                } else if (real_format == 16) {
                    Uint16 *values = (Uint16 *)propdata;

                    printf("{");
                    for (i = 0; i < items_read; i++) {
                        printf(" %d", values[i]);
                    }
                    printf(" }\n");
                } else if (real_format == 8) {
                    Uint8 *values = (Uint8 *)propdata;

                    printf("{");
                    for (i = 0; i < items_read; i++) {
                        printf(" %d", values[i]);
                    }
                    printf(" }\n");
                }
            } else if (real_type == XA_STRING ||
                       real_type == videodata->UTF8_STRING) {
                printf("{ \"%s\" }\n", propdata);
            } else if (real_type == XA_ATOM) {
                Atom *atoms = (Atom *)propdata;

                printf("{");
                for (i = 0; i < items_read; i++) {
                    char *atomname = X11_XGetAtomName(display, atoms[i]);
                    if (atomname) {
                        printf(" %s", atomname);
                        X11_XFree(atomname);
                    }
                }
                printf(" }\n");
            } else {
                char *atomname = X11_XGetAtomName(display, real_type);
                printf("Unknown type: %ld (%s)\n", real_type, atomname ? atomname : "UNKNOWN");
                if (atomname) {
                    X11_XFree(atomname);
                }
            }
        }
        if (status == Success) {
            X11_XFree(propdata);
        }
#endif /* DEBUG_XEVENTS */

        /* Take advantage of this moment to make sure user_time has a
            valid timestamp from the X server, so if we later try to
            raise/restore this window, _NET_ACTIVE_WINDOW can have a
            non-zero timestamp, even if there's never been a mouse or
            key press to this window so far. Note that we don't try to
            set _NET_WM_USER_TIME here, though. That's only for legit
            user interaction with the window. */
        if (!data->user_time) {
            data->user_time = xevent->xproperty.time;
        }

        if (xevent->xproperty.atom == data->videodata->_NET_WM_STATE) {
            /* Get the new state from the window manager.
               Compositing window managers can alter visibility of windows
               without ever mapping / unmapping them, so we handle that here,
               because they use the NETWM protocol to notify us of changes.
             */
            const Uint32 flags = X11_GetNetWMState(_this, data->window, xevent->xproperty.window);
            const Uint32 changed = flags ^ data->window->flags;

            if ((changed & (SDL_WINDOW_HIDDEN | SDL_WINDOW_FULLSCREEN)) != 0) {
                if (flags & SDL_WINDOW_HIDDEN) {
                    X11_DispatchUnmapNotify(data);
                } else {
                    X11_DispatchMapNotify(data);
                }
            }

            if (!SDL_WINDOW_IS_POPUP(data->window)) {
                if (changed & SDL_WINDOW_FULLSCREEN) {
                    data->pending_operation &= ~X11_PENDING_OP_FULLSCREEN;

                    if (flags & SDL_WINDOW_FULLSCREEN) {
                        if (!(flags & SDL_WINDOW_MINIMIZED)) {
                            const SDL_bool commit = data->requested_fullscreen_mode.displayID == 0 ||
                                                    SDL_memcmp(&data->window->current_fullscreen_mode, &data->requested_fullscreen_mode, sizeof(SDL_DisplayMode)) != 0;

                            SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_ENTER_FULLSCREEN, 0, 0);
                            if (commit) {
                                /* This was initiated by the compositor, or the mode was changed between the request and the window
                                 * becoming fullscreen. Switch to the application requested mode if necessary.
                                 */
                                SDL_copyp(&data->window->current_fullscreen_mode, &data->window->requested_fullscreen_mode);
                                SDL_UpdateFullscreenMode(data->window, SDL_TRUE, SDL_TRUE);
                            } else {
                                SDL_UpdateFullscreenMode(data->window, SDL_TRUE, SDL_FALSE);
                            }
                        }
                    } else {
                        SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_LEAVE_FULLSCREEN, 0, 0);
                        SDL_UpdateFullscreenMode(data->window, SDL_FALSE, SDL_FALSE);

                        /* Need to restore or update any limits changed while the window was fullscreen. */
                        X11_SetWindowMinMax(data->window, !!(flags & SDL_WINDOW_MAXIMIZED));
                    }

                    if ((flags & SDL_WINDOW_FULLSCREEN) &&
                        (data->border_top || data->border_left || data->border_bottom || data->border_right)) {
                        /* If the window is entering fullscreen and the borders are
                         * non-zero sized, turn off size events until the borders are
                         * shut off to avoid bogus window sizes and positions, and
                         * note that the old borders were non-zero for restoration.
                         */
                        data->disable_size_position_events = SDL_TRUE;
                        data->previous_borders_nonzero = SDL_TRUE;
                    } else if (!(flags & SDL_WINDOW_FULLSCREEN) &&
                               data->previous_borders_nonzero &&
                               (!data->border_top && !data->border_left && !data->border_bottom && !data->border_right)) {
                        /* If the window is leaving fullscreen and the current borders
                         * are zero sized, but weren't when entering fullscreen, turn
                         * off size events until the borders come back to avoid bogus
                         * window sizes and positions.
                         */
                        data->disable_size_position_events = SDL_TRUE;
                        data->previous_borders_nonzero = SDL_FALSE;
                    } else {
                        data->disable_size_position_events = SDL_FALSE;
                        data->previous_borders_nonzero = SDL_FALSE;

                        if (!(data->window->flags & SDL_WINDOW_FULLSCREEN) && data->toggle_borders) {
                            data->toggle_borders = SDL_FALSE;
                            X11_SetWindowBordered(_this, data->window, !(data->window->flags & SDL_WINDOW_BORDERLESS));
                        }
                    }
                }
                if ((changed & SDL_WINDOW_MAXIMIZED) && ((flags & SDL_WINDOW_MAXIMIZED) && !(flags & SDL_WINDOW_MINIMIZED))) {
                    data->pending_operation &= ~X11_PENDING_OP_MAXIMIZE;
                    if ((changed & SDL_WINDOW_MINIMIZED)) {
                        data->pending_operation &= ~X11_PENDING_OP_RESTORE;
                        /* If coming out of minimized, send a restore event before sending maximized. */
                        SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_RESTORED, 0, 0);
                    }
                    SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_MAXIMIZED, 0, 0);
                }
                if ((changed & SDL_WINDOW_MINIMIZED) && (flags & SDL_WINDOW_MINIMIZED)) {
                    data->pending_operation &= ~X11_PENDING_OP_MINIMIZE;
                    SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_MINIMIZED, 0, 0);
                }
                if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))) {
                    data->pending_operation &= ~X11_PENDING_OP_RESTORE;
                    SDL_SendWindowEvent(data->window, SDL_EVENT_WINDOW_RESTORED, 0, 0);

                    /* Restore the last known floating state if leaving maximized mode */
                    if (!(flags & SDL_WINDOW_FULLSCREEN)) {
                        data->pending_operation |= X11_PENDING_OP_MOVE | X11_PENDING_OP_RESIZE;
                        data->expected.x = data->window->floating.x - data->border_left;
                        data->expected.y = data->window->floating.y - data->border_top;
                        data->expected.w = data->window->floating.w;
                        data->expected.h = data->window->floating.h;
                        X11_XMoveWindow(display, data->xwindow, data->window->floating.x - data->border_left, data->window->floating.y - data->border_top);
                        X11_XResizeWindow(display, data->xwindow, data->window->floating.w, data->window->floating.h);
                    }
                }
            }
            if (changed & SDL_WINDOW_OCCLUDED) {
                SDL_SendWindowEvent(data->window, (flags & SDL_WINDOW_OCCLUDED) ? SDL_EVENT_WINDOW_OCCLUDED : SDL_EVENT_WINDOW_EXPOSED, 0, 0);
            }
        } else if (xevent->xproperty.atom == videodata->XKLAVIER_STATE) {
            /* Hack for Ubuntu 12.04 (etc) that doesn't send MappingNotify
               events when the keyboard layout changes (for example,
               changing from English to French on the menubar's keyboard
               icon). Since it changes the XKLAVIER_STATE property, we
               notice and reinit our keymap here. This might not be the
               right approach, but it seems to work. */
            X11_UpdateKeymap(_this, SDL_TRUE);
        } else if (xevent->xproperty.atom == videodata->_NET_FRAME_EXTENTS) {
            /* Re-enable size events if they were turned off waiting for the borders to come back
             * when leaving fullscreen.
             */
            data->disable_size_position_events = SDL_FALSE;
            X11_GetBorderValues(data);
            if (data->border_top != 0 || data->border_left != 0 || data->border_right != 0 || data->border_bottom != 0) {
                /* Adjust if the window size/position changed to accommodate the borders. */
                if (data->window->flags & SDL_WINDOW_MAXIMIZED) {
                    data->pending_operation |= X11_PENDING_OP_RESIZE;
                    data->expected.w = data->window->windowed.w;
                    data->expected.h = data->window->windowed.h;
                    X11_XResizeWindow(display, data->xwindow, data->window->windowed.w, data->window->windowed.h);
                } else {
                    data->pending_operation |= X11_PENDING_OP_RESIZE | X11_PENDING_OP_MOVE;
                    data->expected.w = data->window->floating.w;
                    data->expected.h = data->window->floating.h;
                    X11_XMoveWindow(display, data->xwindow, data->window->floating.x - data->border_left, data->window->floating.y - data->border_top);
                    X11_XResizeWindow(display, data->xwindow, data->window->floating.w, data->window->floating.h);
                }
            }
            if (!(data->window->flags & SDL_WINDOW_FULLSCREEN) && data->toggle_borders) {
                data->toggle_borders = SDL_FALSE;
                X11_SetWindowBordered(_this, data->window, !(data->window->flags & SDL_WINDOW_BORDERLESS));
            }
        }
    } break;

    case SelectionNotify:
    {
        Atom target = xevent->xselection.target;
#ifdef DEBUG_XEVENTS
        printf("window %p: SelectionNotify (requestor = %ld, target = %ld)\n", data,
               xevent->xselection.requestor, xevent->xselection.target);
#endif
        if (target == data->xdnd_req) {
            /* read data */
            SDL_x11Prop p;
            X11_ReadProperty(&p, display, data->xwindow, videodata->PRIMARY);

            if (p.format == 8) {
                char *saveptr = NULL;
                char *name = X11_XGetAtomName(display, target);
                if (name) {
                    char *token = SDL_strtok_r((char *)p.data, "\r\n", &saveptr);
                    while (token) {
                        if (SDL_strcmp("text/plain", name) == 0) {
                            SDL_SendDropText(data->window, token);
                        } else if (SDL_strcmp("text/uri-list", name) == 0) {
                            char *fn = X11_URIToLocal(token);
                            if (fn) {
                                SDL_SendDropFile(data->window, NULL, fn);
                            }
                        }
                        token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                    }
                    X11_XFree(name);
                }
                SDL_SendDropComplete(data->window);
            }
            X11_XFree(p.data);

            /* send reply */
            SDL_memset(&m, 0, sizeof(XClientMessageEvent));
            m.type = ClientMessage;
            m.display = display;
            m.window = data->xdnd_source;
            m.message_type = videodata->XdndFinished;
            m.format = 32;
            m.data.l[0] = data->xwindow;
            m.data.l[1] = 1;
            m.data.l[2] = videodata->XdndActionCopy;
            X11_XSendEvent(display, data->xdnd_source, False, NoEventMask, (XEvent *)&m);

            X11_XSync(display, False);
        }
    } break;

    default:
    {
#ifdef DEBUG_XEVENTS
        printf("window %p: Unhandled event %d\n", data, xevent->type);
#endif
    } break;
    }
}

static void X11_HandleFocusChanges(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->driverdata;
    int i;

    if (videodata && videodata->windowlist) {
        for (i = 0; i < videodata->numwindows; ++i) {
            SDL_WindowData *data = videodata->windowlist[i];
            if (data && data->pending_focus != PENDING_FOCUS_NONE) {
                Uint64 now = SDL_GetTicks();
                if (now >= data->pending_focus_time) {
                    if (data->pending_focus == PENDING_FOCUS_IN) {
                        X11_DispatchFocusIn(_this, data);
                    } else {
                        X11_DispatchFocusOut(_this, data);
                    }
                    data->pending_focus = PENDING_FOCUS_NONE;
                }
            }
        }
    }
}

static Bool isAnyEvent(Display *display, XEvent *ev, XPointer arg)
{
    return True;
}

static SDL_bool X11_PollEvent(Display *display, XEvent *event)
{
    if (!X11_XCheckIfEvent(display, event, isAnyEvent, NULL)) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void X11_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->driverdata;
    Display *req_display = data->request_display;
    Window xwindow = window->driverdata->xwindow;
    XClientMessageEvent event;

    SDL_memset(&event, 0, sizeof(XClientMessageEvent));
    event.type = ClientMessage;
    event.display = req_display;
    event.send_event = True;
    event.message_type = data->_SDL_WAKEUP;
    event.format = 8;

    X11_XSendEvent(req_display, xwindow, False, NoEventMask, (XEvent *)&event);
    /* XSendEvent returns a status and it could be BadValue or BadWindow. If an
      error happens it is an SDL's internal error and there is nothing we can do here. */
    X11_XFlush(req_display);
}

int X11_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS)
{
    SDL_VideoData *videodata = _this->driverdata;
    Display *display;
    XEvent xevent;
    display = videodata->display;

    SDL_zero(xevent);

    /* Flush and poll to grab any events already read and queued */
    X11_XFlush(display);
    if (X11_PollEvent(display, &xevent)) {
        /* Fall through */
    } else if (timeoutNS == 0) {
        return 0;
    } else {
        /* Use SDL_IOR_NO_RETRY to ensure SIGINT will break us out of our wait */
        int err = SDL_IOReady(ConnectionNumber(display), SDL_IOR_READ | SDL_IOR_NO_RETRY, timeoutNS);
        if (err > 0) {
            if (!X11_PollEvent(display, &xevent)) {
                /* Someone may have beat us to reading the fd. Return 1 here to
                 * trigger the normal spurious wakeup logic in the event core. */
                return 1;
            }
        } else if (err == 0) {
            /* Timeout */
            return 0;
        } else {
            /* Error returned from poll()/select() */

            if (errno == EINTR) {
                /* If the wait was interrupted by a signal, we may have generated a
                 * SDL_EVENT_QUIT event. Let the caller know to call SDL_PumpEvents(). */
                return 1;
            } else {
                return err;
            }
        }
    }

    X11_DispatchEvent(_this, &xevent);

#ifdef SDL_USE_IME
    if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
        SDL_IME_PumpEvents();
    }
#endif

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_PumpEvents();
#endif
    return 1;
}

void X11_PumpEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;
    XEvent xevent;
    int i;

    /* Check if a display had the mode changed and is waiting for a window to asynchronously become
     * fullscreen. If there is no fullscreen window past the elapsed timeout, revert the mode switch.
     */
    for (i = 0; i < _this->num_displays; ++i) {
        if (_this->displays[i]->driverdata->mode_switch_deadline_ns &&
            SDL_GetTicksNS() >= _this->displays[i]->driverdata->mode_switch_deadline_ns) {
            if (_this->displays[i]->fullscreen_window) {
                _this->displays[i]->driverdata->mode_switch_deadline_ns = 0;
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                             "Time out elapsed after mode switch on display %" SDL_PRIu32 " with no window becoming fullscreen; reverting", _this->displays[i]->id);
                SDL_SetDisplayModeForDisplay(_this->displays[i], NULL);
            }
        }
    }

    if (data->last_mode_change_deadline) {
        if (SDL_GetTicks() >= data->last_mode_change_deadline) {
            data->last_mode_change_deadline = 0; /* assume we're done. */
        }
    }

    /* Update activity every 30 seconds to prevent screensaver */
    if (_this->suspend_screensaver) {
        Uint64 now = SDL_GetTicks();
        if (!data->screensaver_activity || now >= (data->screensaver_activity + 30000)) {
            X11_XResetScreenSaver(data->display);

#ifdef SDL_USE_LIBDBUS
            SDL_DBus_ScreensaverTickle();
#endif

            data->screensaver_activity = now;
        }
    }

    SDL_zero(xevent);

    /* Keep processing pending events */
    while (X11_PollEvent(data->display, &xevent)) {
        X11_DispatchEvent(_this, &xevent);
    }

#ifdef SDL_USE_IME
    if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
        SDL_IME_PumpEvents();
    }
#endif

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_PumpEvents();
#endif

    /* FIXME: Only need to do this when there are pending focus changes */
    X11_HandleFocusChanges(_this);

    /* FIXME: Only need to do this when there are flashing windows */
    for (i = 0; i < data->numwindows; ++i) {
        if (data->windowlist[i] != NULL &&
            data->windowlist[i]->flash_cancel_time &&
            SDL_GetTicks() >= data->windowlist[i]->flash_cancel_time) {
            X11_FlashWindow(_this, data->windowlist[i]->window, SDL_FLASH_CANCEL);
        }
    }
}

int X11_SuspendScreenSaver(SDL_VideoDevice *_this)
{
#ifdef SDL_VIDEO_DRIVER_X11_XSCRNSAVER
    SDL_VideoData *data = _this->driverdata;
    int dummy;
    int major_version, minor_version;
#endif /* SDL_VIDEO_DRIVER_X11_XSCRNSAVER */

#ifdef SDL_USE_LIBDBUS
    if (SDL_DBus_ScreensaverInhibit(_this->suspend_screensaver)) {
        return 0;
    }

    if (_this->suspend_screensaver) {
        SDL_DBus_ScreensaverTickle();
    }
#endif

#ifdef SDL_VIDEO_DRIVER_X11_XSCRNSAVER
    if (SDL_X11_HAVE_XSS) {
        /* X11_XScreenSaverSuspend was introduced in MIT-SCREEN-SAVER 1.1 */
        if (!X11_XScreenSaverQueryExtension(data->display, &dummy, &dummy) ||
            !X11_XScreenSaverQueryVersion(data->display,
                                          &major_version, &minor_version) ||
            major_version < 1 || (major_version == 1 && minor_version < 1)) {
            return SDL_Unsupported();
        }

        X11_XScreenSaverSuspend(data->display, _this->suspend_screensaver);
        X11_XResetScreenSaver(data->display);
        return 0;
    }
#endif
    return SDL_Unsupported();
}

#endif /* SDL_VIDEO_DRIVER_X11 */
