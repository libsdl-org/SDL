/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#if SDL_VIDEO_DRIVER_X11

#include <unistd.h> /* For getpid() and readlink() */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"

#include "SDL_x11video.h"
#include "SDL_x11framebuffer.h"
#include "SDL_x11shape.h"
#include "SDL_x11touch.h"
#include "SDL_x11xinput2.h"

#if SDL_VIDEO_OPENGL_EGL
#include "SDL_x11opengles.h"
#endif

/* !!! FIXME: move dbus stuff to somewhere under src/core/linux ... */
#if SDL_USE_LIBDBUS
/* we never link directly to libdbus. */
#include "SDL_loadso.h"
static const char *dbus_library = "libdbus-1.so.3";
static void *dbus_handle = NULL;

/* !!! FIXME: this is kinda ugly. */
static SDL_bool
load_dbus_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(dbus_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

/* libdbus entry points... */
static DBusConnection *(*DBUS_dbus_bus_get_private)(DBusBusType, DBusError *) = NULL;
static void (*DBUS_dbus_connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t) = NULL;
static dbus_bool_t (*DBUS_dbus_connection_send)(DBusConnection *, DBusMessage *, dbus_uint32_t *) = NULL;
static void (*DBUS_dbus_connection_close)(DBusConnection *) = NULL;
static void (*DBUS_dbus_connection_unref)(DBusConnection *) = NULL;
static void (*DBUS_dbus_connection_flush)(DBusConnection *) = NULL;
static DBusMessage *(*DBUS_dbus_message_new_method_call)(const char *, const char *, const char *, const char *) = NULL;
static void (*DBUS_dbus_message_unref)(DBusMessage *) = NULL;
static void (*DBUS_dbus_error_init)(DBusError *) = NULL;
static dbus_bool_t (*DBUS_dbus_error_is_set)(const DBusError *) = NULL;
static void (*DBUS_dbus_error_free)(DBusError *) = NULL;

static int
load_dbus_syms(void)
{
    /* cast funcs to char* first, to please GCC's strict aliasing rules. */
    #define SDL_DBUS_SYM(x) \
        if (!load_dbus_sym(#x, (void **) (char *) &DBUS_##x)) return -1

    SDL_DBUS_SYM(dbus_bus_get_private);
    SDL_DBUS_SYM(dbus_connection_set_exit_on_disconnect);
    SDL_DBUS_SYM(dbus_connection_send);
    SDL_DBUS_SYM(dbus_connection_close);
    SDL_DBUS_SYM(dbus_connection_unref);
    SDL_DBUS_SYM(dbus_connection_flush);
    SDL_DBUS_SYM(dbus_message_new_method_call);
    SDL_DBUS_SYM(dbus_message_unref);
    SDL_DBUS_SYM(dbus_error_init);
    SDL_DBUS_SYM(dbus_error_is_set);
    SDL_DBUS_SYM(dbus_error_free);

    #undef SDL_DBUS_SYM

    return 0;
}

static void
UnloadDBUSLibrary(void)
{
    if (dbus_handle != NULL) {
        SDL_UnloadObject(dbus_handle);
        dbus_handle = NULL;
    }
}

static int
LoadDBUSLibrary(void)
{
    int retval = 0;
    if (dbus_handle == NULL) {
        dbus_handle = SDL_LoadObject(dbus_library);
        if (dbus_handle == NULL) {
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
        } else {
            retval = load_dbus_syms();
            if (retval < 0) {
                UnloadDBUSLibrary();
            }
        }
    }

    return retval;
}

static void
X11_InitDBus(_THIS)
{
    if (LoadDBUSLibrary() != -1) {
        SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
        DBusError err;
        DBUS_dbus_error_init(&err);
        data->dbus = DBUS_dbus_bus_get_private(DBUS_BUS_SESSION, &err);
        if (DBUS_dbus_error_is_set(&err)) {
            DBUS_dbus_error_free(&err);
            if (data->dbus) {
                DBUS_dbus_connection_unref(data->dbus);
                data->dbus = NULL;
            }
            return;  /* oh well */
        }
        DBUS_dbus_connection_set_exit_on_disconnect(data->dbus, 0);
    }
}

static void
X11_QuitDBus(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    if (data->dbus) {
        DBUS_dbus_connection_close(data->dbus);
        DBUS_dbus_connection_unref(data->dbus);
        data->dbus = NULL;
    }
}

void
SDL_dbus_screensaver_tickle(_THIS)
{
    const SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    DBusConnection *conn = data->dbus;
    if (conn != NULL) {
        DBusMessage *msg = DBUS_dbus_message_new_method_call("org.gnome.ScreenSaver",
                                                             "/org/gnome/ScreenSaver",
                                                             "org.gnome.ScreenSaver",
                                                             "SimulateUserActivity");
        if (msg != NULL) {
            if (DBUS_dbus_connection_send(conn, msg, NULL)) {
                DBUS_dbus_connection_flush(conn);
            }
            DBUS_dbus_message_unref(msg);
        }
    }
}
#endif

/* Initialization/Query functions */
static int X11_VideoInit(_THIS);
static void X11_VideoQuit(_THIS);

/* Find out what class name we should use */
static char *
get_classname()
{
    char *spot;
#if defined(__LINUX__) || defined(__FREEBSD__)
    char procfile[1024];
    char linkfile[1024];
    int linksize;
#endif

    /* First allow environment variable override */
    spot = SDL_getenv("SDL_VIDEO_X11_WMCLASS");
    if (spot) {
        return SDL_strdup(spot);
    }

    /* Next look at the application's executable name */
#if defined(__LINUX__) || defined(__FREEBSD__)
#if defined(__LINUX__)
    SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/exe", getpid());
#elif defined(__FREEBSD__)
    SDL_snprintf(procfile, SDL_arraysize(procfile), "/proc/%d/file",
                 getpid());
#else
#error Where can we find the executable name?
#endif
    linksize = readlink(procfile, linkfile, sizeof(linkfile) - 1);
    if (linksize > 0) {
        linkfile[linksize] = '\0';
        spot = SDL_strrchr(linkfile, '/');
        if (spot) {
            return SDL_strdup(spot + 1);
        } else {
            return SDL_strdup(linkfile);
        }
    }
#endif /* __LINUX__ || __FREEBSD__ */

    /* Finally use the default we've used forever */
    return SDL_strdup("SDL_App");
}

/* X11 driver bootstrap functions */

static int
X11_Available(void)
{
    Display *display = NULL;
    if (SDL_X11_LoadSymbols()) {
        display = X11_XOpenDisplay(NULL);
        if (display != NULL) {
            X11_XCloseDisplay(display);
        }
        SDL_X11_UnloadSymbols();
    }
    return (display != NULL);
}

static void
X11_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_VideoData *data = (SDL_VideoData *) device->driverdata;
    if (data->display) {
        X11_XCloseDisplay(data->display);
    }
    SDL_free(data->windowlist);
    SDL_free(device->driverdata);
    SDL_free(device);

    SDL_X11_UnloadSymbols();
}

/* An error handler to reset the vidmode and then call the default handler. */
static SDL_bool safety_net_triggered = SDL_FALSE;
static int (*orig_x11_errhandler) (Display *, XErrorEvent *) = NULL;
static int
X11_SafetyNetErrHandler(Display * d, XErrorEvent * e)
{
    SDL_VideoDevice *device = NULL;
    /* if we trigger an error in our error handler, don't try again. */
    if (!safety_net_triggered) {
        safety_net_triggered = SDL_TRUE;
        device = SDL_GetVideoDevice();
        if (device != NULL) {
            int i;
            for (i = 0; i < device->num_displays; i++) {
                SDL_VideoDisplay *display = &device->displays[i];
                if (SDL_memcmp(&display->current_mode, &display->desktop_mode,
                               sizeof (SDL_DisplayMode)) != 0) {
                    X11_SetDisplayMode(device, display, &display->desktop_mode);
                }
            }
        }
    }

    if (orig_x11_errhandler != NULL) {
        return orig_x11_errhandler(d, e);  /* probably terminate. */
    }

    return 0;
}

static SDL_VideoDevice *
X11_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;
    const char *display = NULL; /* Use the DISPLAY environment variable */

    if (!SDL_X11_LoadSymbols()) {
        return NULL;
    }

    /* Need for threading gl calls. This is also required for the proprietary
        nVidia driver to be threaded. */
    X11_XInitThreads();

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }
    data = (struct SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_free(device);
        SDL_OutOfMemory();
        return NULL;
    }
    device->driverdata = data;

    /* FIXME: Do we need this?
       if ( (SDL_strncmp(X11_XDisplayName(display), ":", 1) == 0) ||
       (SDL_strncmp(X11_XDisplayName(display), "unix:", 5) == 0) ) {
       local_X11 = 1;
       } else {
       local_X11 = 0;
       }
     */
    data->display = X11_XOpenDisplay(display);
#if defined(__osf__) && defined(SDL_VIDEO_DRIVER_X11_DYNAMIC)
    /* On Tru64 if linking without -lX11, it fails and you get following message.
     * Xlib: connection to ":0.0" refused by server
     * Xlib: XDM authorization key matches an existing client!
     *
     * It succeeds if retrying 1 second later
     * or if running xhost +localhost on shell.
     */
    if (data->display == NULL) {
        SDL_Delay(1000);
        data->display = X11_XOpenDisplay(display);
    }
#endif
    if (data->display == NULL) {
        SDL_free(device->driverdata);
        SDL_free(device);
        SDL_SetError("Couldn't open X11 display");
        return NULL;
    }
#ifdef X11_DEBUG
    X11_XSynchronize(data->display, True);
#endif

    /* Hook up an X11 error handler to recover the desktop resolution. */
    safety_net_triggered = SDL_FALSE;
    orig_x11_errhandler = X11_XSetErrorHandler(X11_SafetyNetErrHandler);

    /* Set the function pointers */
    device->VideoInit = X11_VideoInit;
    device->VideoQuit = X11_VideoQuit;
    device->GetDisplayModes = X11_GetDisplayModes;
    device->GetDisplayBounds = X11_GetDisplayBounds;
    device->SetDisplayMode = X11_SetDisplayMode;
    device->SuspendScreenSaver = X11_SuspendScreenSaver;
    device->PumpEvents = X11_PumpEvents;

    device->CreateWindow = X11_CreateWindow;
    device->CreateWindowFrom = X11_CreateWindowFrom;
    device->SetWindowTitle = X11_SetWindowTitle;
    device->SetWindowIcon = X11_SetWindowIcon;
    device->SetWindowPosition = X11_SetWindowPosition;
    device->SetWindowSize = X11_SetWindowSize;
    device->SetWindowMinimumSize = X11_SetWindowMinimumSize;
    device->SetWindowMaximumSize = X11_SetWindowMaximumSize;
    device->ShowWindow = X11_ShowWindow;
    device->HideWindow = X11_HideWindow;
    device->RaiseWindow = X11_RaiseWindow;
    device->MaximizeWindow = X11_MaximizeWindow;
    device->MinimizeWindow = X11_MinimizeWindow;
    device->RestoreWindow = X11_RestoreWindow;
    device->SetWindowBordered = X11_SetWindowBordered;
    device->SetWindowFullscreen = X11_SetWindowFullscreen;
    device->SetWindowGammaRamp = X11_SetWindowGammaRamp;
    device->SetWindowGrab = X11_SetWindowGrab;
    device->DestroyWindow = X11_DestroyWindow;
    device->CreateWindowFramebuffer = X11_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = X11_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = X11_DestroyWindowFramebuffer;
    device->GetWindowWMInfo = X11_GetWindowWMInfo;

    device->shape_driver.CreateShaper = X11_CreateShaper;
    device->shape_driver.SetWindowShape = X11_SetWindowShape;
    device->shape_driver.ResizeWindowShape = X11_ResizeWindowShape;

#if SDL_VIDEO_OPENGL_GLX
    device->GL_LoadLibrary = X11_GL_LoadLibrary;
    device->GL_GetProcAddress = X11_GL_GetProcAddress;
    device->GL_UnloadLibrary = X11_GL_UnloadLibrary;
    device->GL_CreateContext = X11_GL_CreateContext;
    device->GL_MakeCurrent = X11_GL_MakeCurrent;
    device->GL_SetSwapInterval = X11_GL_SetSwapInterval;
    device->GL_GetSwapInterval = X11_GL_GetSwapInterval;
    device->GL_SwapWindow = X11_GL_SwapWindow;
    device->GL_DeleteContext = X11_GL_DeleteContext;
#elif SDL_VIDEO_OPENGL_EGL
    device->GL_LoadLibrary = X11_GLES_LoadLibrary;
    device->GL_GetProcAddress = X11_GLES_GetProcAddress;
    device->GL_UnloadLibrary = X11_GLES_UnloadLibrary;
    device->GL_CreateContext = X11_GLES_CreateContext;
    device->GL_MakeCurrent = X11_GLES_MakeCurrent;
    device->GL_SetSwapInterval = X11_GLES_SetSwapInterval;
    device->GL_GetSwapInterval = X11_GLES_GetSwapInterval;
    device->GL_SwapWindow = X11_GLES_SwapWindow;
    device->GL_DeleteContext = X11_GLES_DeleteContext;
#endif

    device->SetClipboardText = X11_SetClipboardText;
    device->GetClipboardText = X11_GetClipboardText;
    device->HasClipboardText = X11_HasClipboardText;

    device->free = X11_DeleteDevice;

    return device;
}

VideoBootStrap X11_bootstrap = {
    "x11", "SDL X11 video driver",
    X11_Available, X11_CreateDevice
};

static int (*handler) (Display *, XErrorEvent *) = NULL;
static int
X11_CheckWindowManagerErrorHandler(Display * d, XErrorEvent * e)
{
    if (e->error_code == BadWindow) {
        return (0);
    } else {
        return (handler(d, e));
    }
}

static void
X11_CheckWindowManager(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    Display *display = data->display;
    Atom _NET_SUPPORTING_WM_CHECK;
    int status, real_format;
    Atom real_type;
    unsigned long items_read, items_left;
    unsigned char *propdata;
    Window wm_window = 0;
#ifdef DEBUG_WINDOW_MANAGER
    char *wm_name;
#endif

    /* Set up a handler to gracefully catch errors */
    X11_XSync(display, False);
    handler = X11_XSetErrorHandler(X11_CheckWindowManagerErrorHandler);

    _NET_SUPPORTING_WM_CHECK = X11_XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    status = X11_XGetWindowProperty(display, DefaultRootWindow(display), _NET_SUPPORTING_WM_CHECK, 0L, 1L, False, XA_WINDOW, &real_type, &real_format, &items_read, &items_left, &propdata);
    if (status == Success && items_read) {
        wm_window = ((Window*)propdata)[0];
    }
    if (propdata) {
        X11_XFree(propdata);
    }

    if (wm_window) {
        status = X11_XGetWindowProperty(display, wm_window, _NET_SUPPORTING_WM_CHECK, 0L, 1L, False, XA_WINDOW, &real_type, &real_format, &items_read, &items_left, &propdata);
        if (status != Success || !items_read || wm_window != ((Window*)propdata)[0]) {
            wm_window = None;
        }
        if (propdata) {
            X11_XFree(propdata);
        }
    }

    /* Reset the error handler, we're done checking */
    X11_XSync(display, False);
    X11_XSetErrorHandler(handler);

    if (!wm_window) {
#ifdef DEBUG_WINDOW_MANAGER
        printf("Couldn't get _NET_SUPPORTING_WM_CHECK property\n");
#endif
        return;
    }
    data->net_wm = SDL_TRUE;

#ifdef DEBUG_WINDOW_MANAGER
    wm_name = X11_GetWindowTitle(_this, wm_window);
    printf("Window manager: %s\n", wm_name);
    SDL_free(wm_name);
#endif
}


int
X11_VideoInit(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    /* Get the window class name, usually the name of the application */
    data->classname = get_classname();

    /* Get the process PID to be associated to the window */
    data->pid = getpid();

    /* Open a connection to the X input manager */
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        data->im =
            X11_XOpenIM(data->display, NULL, data->classname, data->classname);
    }
#endif

    /* Look up some useful Atoms */
#define GET_ATOM(X) data->X = X11_XInternAtom(data->display, #X, False)
    GET_ATOM(WM_PROTOCOLS);
    GET_ATOM(WM_DELETE_WINDOW);
    GET_ATOM(_NET_WM_STATE);
    GET_ATOM(_NET_WM_STATE_HIDDEN);
    GET_ATOM(_NET_WM_STATE_FOCUSED);
    GET_ATOM(_NET_WM_STATE_MAXIMIZED_VERT);
    GET_ATOM(_NET_WM_STATE_MAXIMIZED_HORZ);
    GET_ATOM(_NET_WM_STATE_FULLSCREEN);
    GET_ATOM(_NET_WM_ALLOWED_ACTIONS);
    GET_ATOM(_NET_WM_ACTION_FULLSCREEN);
    GET_ATOM(_NET_WM_NAME);
    GET_ATOM(_NET_WM_ICON_NAME);
    GET_ATOM(_NET_WM_ICON);
    GET_ATOM(_NET_WM_PING);
    GET_ATOM(_NET_ACTIVE_WINDOW);
    GET_ATOM(UTF8_STRING);
    GET_ATOM(PRIMARY);
    GET_ATOM(XdndEnter);
    GET_ATOM(XdndPosition);
    GET_ATOM(XdndStatus);
    GET_ATOM(XdndTypeList);
    GET_ATOM(XdndActionCopy);
    GET_ATOM(XdndDrop);
    GET_ATOM(XdndFinished);
    GET_ATOM(XdndSelection);

    /* Detect the window manager */
    X11_CheckWindowManager(_this);

    if (X11_InitModes(_this) < 0) {
        return -1;
    }

    X11_InitXinput2(_this);

    if (X11_InitKeyboard(_this) != 0) {
        return -1;
    }
    X11_InitMouse(_this);

    X11_InitTouch(_this);

#if SDL_USE_LIBDBUS
    X11_InitDBus(_this);
#endif

    return 0;
}

void
X11_VideoQuit(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    SDL_free(data->classname);
#ifdef X_HAVE_UTF8_STRING
    if (data->im) {
        X11_XCloseIM(data->im);
    }
#endif

    X11_QuitModes(_this);
    X11_QuitKeyboard(_this);
    X11_QuitMouse(_this);
    X11_QuitTouch(_this);

#if SDL_USE_LIBDBUS
    X11_QuitDBus(_this);
#endif
}

SDL_bool
X11_UseDirectColorVisuals(void)
{
    return SDL_getenv("SDL_VIDEO_X11_NODIRECTCOLOR") ? SDL_FALSE : SDL_TRUE;
}

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vim: set ts=4 sw=4 expandtab: */
