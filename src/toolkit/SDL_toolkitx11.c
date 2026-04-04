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

#ifdef SDL_VIDEO_DRIVER_X11

#include "../../video/x11/SDL_x11video.h"
#include "../../video/x11/SDL_x11dyn.h"
#include "../../video/x11/SDL_x11modes.h"
#include "../../video/x11/SDL_x11settings.h"
#include "../../video/x11/xsettings-client.h"
#include "SDL_toolkitx11.h"
#include <locale.h>

#define X11_XUniqueContext() ((XContext)X11_XrmUniqueQuark())

#define ICON_FONT_XLFD       "-*-*-bold-r-normal-*-*-%d-*-*-*-*-iso8859-1[33 88 105]"
#define UI_FONT_LATIN1_XLFD  "-*-*-medium-r-normal--0-%d-*-*-p-0-iso8859-1"
#define ANY_FONT_LATIN1_XLFD "-*-*-*-*-*--*-*-*-*-*-*-iso8859-1"

static const char *ui_fontset_xlfds[] = {
    "-*-*-medium-r-normal--*-%d-*-*-*-*-iso10646-1,*", // explicitly unicode (iso10646-1)
    "-*-*-medium-r-*--*-%d-*-*-*-*-iso10646-1,*",      // explicitly unicode (iso10646-1)
    "-misc-*-*-*-*--*-*-*-*-*-*-iso10646-1,*",         // misc unicode (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso10646-1,*",            // just give me anything Unicode.
    "-*-*-medium-r-normal--*-%d-*-*-*-*-iso8859-1,*",  // explicitly latin1, in case low-ASCII works out.
    "-*-*-medium-r-*--*-%d-*-*-*-*-iso8859-1,*",       // explicitly latin1, in case low-ASCII works out.
    "-misc-*-*-*-*--*-*-*-*-*-*-iso8859-1,*",          // misc latin1 (fix for some systems)
    "-*-*-*-*-*--*-*-*-*-*-*-iso8859-1,*",             // just give me anything latin1.
    NULL
};

static Bool shm_err;
static int (*old_handler)(Display *, XErrorEvent *) = NULL;

static int SHMErrorHandler(Display *d, XErrorEvent *e)
{
    if (e->error_code == BadAccess || e->error_code == BadRequest) {
        shm_err = True;
        return 0;
    }

    return old_handler(d, e);
}

static void DestroyWindowBuffer(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    if (xwnd->img) {
        XDestroyImage(xwnd->img);
    }

    if (window->surface) {
        SDL_DestroySurface(window->surface);
    }

    if (xwnd->blit_gc != None) {
        X11_XFreeGC(xdrv->dpy, xwnd->blit_gc);
    }

    switch (window->buffer_type) {
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE:
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED) {
            X11_XdbeDeallocateBackBufferName(xdrv->dpy, xwnd->drw);
        }
#endif
        break;
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING:
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE:
        if (xwnd->drw != None) {
            X11_XFreePixmap(xdrv->dpy, xwnd->drw);
        }

#ifndef NO_SHARED_MEMORY
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED) {
            X11_XShmDetach(xdrv->dpy, &xwnd->shm);
            shmdt(xwnd->shm.shmaddr);
        }
#endif
        break;
    default:
        break;
    }

    window->buffer_type = SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NONE;
}

static void DestroyWindow(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    X11_XWithdrawWindow(xdrv->dpy, xwnd->wnd, xdrv->screen);
    DestroyWindowBuffer(window);
    X11_XDestroyWindow(xdrv->dpy, xwnd->wnd);
    X11_XFreeColormap(xdrv->dpy, xwnd->cmap);
    X11_XDeleteContext(xdrv->dpy, xwnd->wnd, xdrv->ctx);
    X11_XFlush(xdrv->dpy);
    SDL_free(window);
}

static void HideWindow(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    X11_XWithdrawWindow(xdrv->dpy, xwnd->wnd, xdrv->screen);
}

static void ShowWindow(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    X11_XMapRaised(xdrv->dpy, xwnd->wnd);
}

static bool UpdateWindowScale(SDL_ToolkitVideoWindow *window, float scale)
{
    if (window->scale == scale) {
        return false;
    } else {
        window->scale = scale;
        return true;
    }
}

#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
static bool GetParentRectRandr(SDL_ToolkitVideoDriverX11 *xdrv, SDL_Rect *rct)
{
    XRRScreenResources *screen_res;
    XRROutputInfo *out_info;
    XRRCrtcInfo *crtc_info;
    RROutput output;
    bool ret;

    screen_res = X11_XRRGetScreenResourcesCurrent(xdrv->dpy, xdrv->root);
    if (!screen_res) {
        return false;
    }

    ret = false;
    out_info = NULL;
    crtc_info = NULL;

    output = X11_XRRGetOutputPrimary(xdrv->dpy, xdrv->root);
    if (output != None) {
        out_info = X11_XRRGetOutputInfo(xdrv->dpy, screen_res, output);
        if (!out_info || out_info->connection != RR_Connected) {
            if (out_info) {
                X11_XRRFreeOutputInfo(out_info);
            }

            if (screen_res->noutput > 0) {
                out_info = X11_XRRGetOutputInfo(xdrv->dpy, screen_res, screen_res->outputs[0]);
            }
        }
    }

    if (out_info) {
        if (out_info->crtc != None) {
            crtc_info = X11_XRRGetCrtcInfo(xdrv->dpy, screen_res, out_info->crtc);
        } else if (out_info->ncrtc > 0) {
            crtc_info = X11_XRRGetCrtcInfo(xdrv->dpy, screen_res, out_info->crtcs[0]);
        }
    } else if (screen_res->ncrtc) {
        crtc_info = X11_XRRGetCrtcInfo(xdrv->dpy, screen_res, screen_res->crtcs[0]);
    }

    if (crtc_info) {
        rct->x = crtc_info->x;
        rct->y = crtc_info->y;
        rct->w = crtc_info->width;
        rct->h = crtc_info->height;
        ret = true;
    }

    if (crtc_info) {
        X11_XRRFreeCrtcInfo(crtc_info);
    }

    if (out_info) {
        X11_XRRFreeOutputInfo(out_info);
    }

    X11_XRRFreeScreenResources(screen_res);

    return ret;
}
#else
static bool GetParentRectRandr(SDL_ToolkitVideoDriverX11 *xdrv, SDL_Rect *rct)
{
    return false;
}
#endif

static void GetParentRect(SDL_ToolkitVideoWindow *window, SDL_Rect *rct)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    Window parent;
    bool use_randr;
    bool use_randr_choice;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;

    parent = None;
    if (window->parent && window->parent->type == SDL_TOOLKIT_VIDEO_WINDOW_PARENT_SDL && window->parent->parent.sdl && window->parent->parent.sdl->internal) {
        parent = window->parent->parent.sdl->internal->xwindow;
    }

    use_randr = use_randr_choice = false;
#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    use_randr = true;
#ifndef XRANDR_DISABLED_BY_DEFAULT
    use_randr_choice = true;
#endif
#endif

    if (parent != None) {
        XWindowAttributes attrib;
        Window dummy;

        X11_XGetWindowAttributes(xdrv->dpy, parent, &attrib);
        rct->w = attrib.width;
        rct->h = attrib.height;
        X11_XTranslateCoordinates(xdrv->dpy, parent, xdrv->root, attrib.x, attrib.y, &rct->x, &rct->y, &dummy);
    } else if (window->driver->parent_device && window->driver->parent_device->displays && window->driver->parent_device->num_displays > 0) {
        SDL_VideoDisplay *dpy;
        SDL_DisplayData *dpydata;

        dpy = window->driver->parent_device->displays[0];
        dpydata = dpy->internal;

        rct->w = dpy->current_mode->w;
        rct->h = dpy->current_mode->h;
        rct->x = dpydata->x;
        rct->y = dpydata->y;
    } else if (SDL_GetHintBoolean(SDL_HINT_VIDEO_X11_XRANDR, use_randr_choice) && use_randr && xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_RANDR_SUPPORTED) {
        if (!GetParentRectRandr(xdrv, rct)) {
            goto USE_DISPLAY_MACROS;
        }
    } else {
    USE_DISPLAY_MACROS:
        rct->x = rct->y = 0;
        rct->w = DisplayWidth(xdrv->dpy, xdrv->screen);
        rct->h = DisplayHeight(xdrv->dpy, xdrv->screen);
    }
}

static bool CreateSoftwareBuffer(SDL_ToolkitVideoDriverX11 *xdrv, SDL_ToolkitVideoWindowX11 *xwnd, SDL_ToolkitVideoWindow *wnd, SDL_ToolkitVideoWindowBufferType type, unsigned int w, unsigned int h)
{
    wnd->buffer_w = w;
    wnd->buffer_h = h;

    if (type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING) {
        xwnd->scaling_rct.x = xwnd->scaling_rct.y = 0;
        xwnd->scaling_rct.w = wnd->rct.w;
        xwnd->scaling_rct.h = wnd->rct.h;
    }

    if (xwnd->blit_gc == None) {
        xwnd->blit_gc = X11_XCreateGC(xdrv->dpy, xwnd->wnd, 0, NULL);
    }

    if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED) {
#ifndef NO_SHARED_MEMORY
        unsigned int bpl;

        xwnd->img = X11_XShmCreateImage(xdrv->dpy, xwnd->vi.visual, xwnd->vi.depth, ZPixmap, NULL, &xwnd->shm, w, h);
        if (xwnd->img) {
            bpl = xwnd->img->bytes_per_line;
            XDestroyImage(xwnd->img);
            xwnd->img = NULL;
        } else {
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        xwnd->shm.shmid = shmget(IPC_PRIVATE, bpl * h, IPC_CREAT | 0777);
        if (xwnd->shm.shmid < 0) {
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        xwnd->shm.readOnly = False;
        xwnd->shm.shmaddr = (char *)shmat(xwnd->shm.shmid, NULL, 0);
        if (((signed char *)xwnd->shm.shmaddr) == (signed char *)-1) {
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        shm_err = False;
        old_handler = X11_XSetErrorHandler(SHMErrorHandler);
        X11_XShmAttach(xdrv->dpy, &xwnd->shm);
        X11_XSync(xdrv->dpy, False);
        X11_XSetErrorHandler(old_handler);
        if (shm_err) {
            shmdt(xwnd->shm.shmaddr);
            shmctl(xwnd->shm.shmid, IPC_RMID, NULL);
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        xwnd->drw = X11_XShmCreatePixmap(xdrv->dpy, xwnd->wnd, xwnd->shm.shmaddr, &xwnd->shm, w, h, xwnd->vi.depth);
        if (xwnd->drw == None) {
            X11_XShmDetach(xdrv->dpy, &xwnd->shm);
            shmdt(xwnd->shm.shmaddr);
            shmctl(xwnd->shm.shmid, IPC_RMID, NULL);
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
            goto CREATE_SHM_IMAGE;
        }

        shmctl(xwnd->shm.shmid, IPC_RMID, NULL);
        wnd->surface = SDL_CreateSurfaceFrom(w, h, X11_GetPixelFormatFromVisualInfo(xdrv->dpy, &xwnd->vi), xwnd->shm.shmaddr, bpl);
        wnd->buffer_type = type;
        return true;
#else
        goto CREATE_XIMAGE_BUFFER;
#endif
    } else if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED) {
#ifndef NO_SHARED_MEMORY
    CREATE_SHM_IMAGE:
        xwnd->img = X11_XShmCreateImage(xdrv->dpy, xwnd->vi.visual, xwnd->vi.depth, ZPixmap, NULL, &xwnd->shm, w, h);
        if (!xwnd->img) {
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        xwnd->shm.shmid = shmget(IPC_PRIVATE, xwnd->img->bytes_per_line * xwnd->img->height, IPC_CREAT | 0777);
        if (xwnd->shm.shmid < 0) {
            XDestroyImage(xwnd->img);
            xwnd->img = NULL;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        xwnd->shm.readOnly = False;
        xwnd->shm.shmaddr = xwnd->img->data = (char *)shmat(xwnd->shm.shmid, NULL, 0);
        if (((signed char *)xwnd->shm.shmaddr) == (signed char *)-1) {
            XDestroyImage(xwnd->img);
            xwnd->img = NULL;
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        shm_err = False;
        old_handler = X11_XSetErrorHandler(SHMErrorHandler);
        X11_XShmAttach(xdrv->dpy, &xwnd->shm);
        X11_XSync(xdrv->dpy, False);
        X11_XSetErrorHandler(old_handler);
        if (shm_err) {
            XDestroyImage(xwnd->img);
            xwnd->img = NULL;
            shmdt(xwnd->shm.shmaddr);
            shmctl(xwnd->shm.shmid, IPC_RMID, NULL);
            xdrv->flags &= ~SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
            goto CREATE_XIMAGE_BUFFER;
        }

        shmctl(xwnd->shm.shmid, IPC_RMID, NULL);

        if (type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE) {
            wnd->buffer_type = type;
        } else {
            xwnd->drw = X11_XCreatePixmap(xdrv->dpy, xwnd->wnd, w, h, xwnd->vi.depth);
            wnd->buffer_type = type;
        }

        wnd->surface = SDL_CreateSurfaceFrom(w, h, X11_GetPixelFormatFromVisualInfo(xdrv->dpy, &xwnd->vi), xwnd->img->data, xwnd->img->bytes_per_line);
        return true;
#else
        goto CREATE_XIMAGE_BUFFER;
#endif
    } else {
    CREATE_XIMAGE_BUFFER:
        if (type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE) {
            wnd->surface = SDL_CreateSurface(w, h, X11_GetPixelFormatFromVisualInfo(xdrv->dpy, &xwnd->vi));
            xwnd->img = X11_XCreateImage(xdrv->dpy, xwnd->vi.visual, xwnd->vi.depth, ZPixmap, 0, wnd->surface->pixels, w, h, 32, wnd->surface->pitch);
            wnd->buffer_type = type;
        } else {
            xwnd->drw = X11_XCreatePixmap(xdrv->dpy, xwnd->wnd, w, h, xwnd->vi.depth);
            wnd->buffer_type = type;
        }
        return true;
    }

    return false;
}

static bool CreateWindowBuffer(SDL_ToolkitVideoWindow *window, SDL_ToolkitVideoWindowBufferType type, ...)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;
    unsigned int w;
    unsigned int h;
    va_list args;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    if (type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NONE || window->buffer_type != SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NONE) {
        return false;
    }

    w = window->rct.w;
    h = window->rct.h;

    switch (type) {
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE:
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED) {
            xwnd->drw = X11_XdbeAllocateBackBufferName(xdrv->dpy, xwnd->wnd, XdbeUndefined);
            window->buffer_type = SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE;
            return true;
        } else {
            return false;
        }
#else
        return false;
#endif
        break;
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SINGLE:
        window->buffer_type = SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SINGLE;
        return true;
        break;
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING:
        va_start(args, type);
        w = va_arg(args, unsigned int);
        h = va_arg(args, unsigned int);
        va_end(args);
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE:
        return CreateSoftwareBuffer(xdrv, xwnd, window, type, w, h);
        break;
    default:
        return false;
    }

    return false;
}

static void BeginWindowDraw(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED && window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE) {
        X11_XdbeBeginIdiom(xdrv->dpy);
    }
#else
    /* Nothing */
#endif
}

static void EndWindowDraw(SDL_ToolkitVideoWindow *window)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    switch (window->buffer_type) {
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE:
#ifdef SDL_VIDEO_DRIVER_X11_XDBE
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED) {
            XdbeSwapInfo swap_info;

            swap_info.swap_window = xwnd->wnd;
            swap_info.swap_action = XdbeUndefined;

            X11_XdbeSwapBuffers(xdrv->dpy, &swap_info, 1);
            X11_XdbeEndIdiom(xdrv->dpy);
        }
#endif
        break;
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING:
#ifndef NO_SHARED_MEMORY
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED) {
            X11_XFlush(xdrv->dpy);
            X11_XSync(xdrv->dpy, false);
            SDL_BlitSurfaceScaled(window->surface, NULL, window->surface, &xwnd->scaling_rct, SDL_SCALEMODE_LINEAR);
            X11_XCopyArea(xdrv->dpy, xwnd->drw, xwnd->wnd, xwnd->blit_gc, 0, 0, window->rct.w, window->rct.h, 0, 0);
        } else if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED) {
            X11_XShmGetImage(xdrv->dpy, xwnd->drw, xwnd->img, 0, 0, AllPlanes);
            SDL_BlitSurfaceScaled(window->surface, NULL, window->surface, &xwnd->scaling_rct, SDL_SCALEMODE_LINEAR);
            X11_XShmPutImage(xdrv->dpy, xwnd->wnd, xwnd->blit_gc, xwnd->img, 0, 0, 0, 0, window->rct.w, window->rct.h, False);
        } else
#endif
        {
            xwnd->img = X11_XGetImage(xdrv->dpy, xwnd->drw, 0, 0, window->buffer_w, window->buffer_h, AllPlanes, ZPixmap);
            window->surface = SDL_CreateSurfaceFrom(window->buffer_w, window->buffer_h, X11_GetPixelFormatFromVisualInfo(xdrv->dpy, &xwnd->vi), xwnd->img->data, xwnd->img->bytes_per_line);
            SDL_BlitSurfaceScaled(window->surface, NULL, window->surface, &xwnd->scaling_rct, SDL_SCALEMODE_LINEAR);
            X11_XPutImage(xdrv->dpy, xwnd->wnd, xwnd->blit_gc, xwnd->img, 0, 0, 0, 0, window->rct.w, window->rct.h);
            XDestroyImage(xwnd->img);
            SDL_DestroySurface(window->surface);
            window->surface = NULL;
            xwnd->img = NULL;
        }
        break;
    case SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE:
#ifndef NO_SHARED_MEMORY
        if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED) {
            X11_XCopyArea(xdrv->dpy, xwnd->drw, xwnd->wnd, xwnd->blit_gc, 0, 0, window->rct.w, window->rct.h, 0, 0);
        } else if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED) {
            X11_XShmPutImage(xdrv->dpy, xwnd->wnd, xwnd->blit_gc, xwnd->img, 0, 0, 0, 0, window->rct.w, window->rct.h, False);
        } else
#endif
        {
            X11_XPutImage(xdrv->dpy, xwnd->wnd, xwnd->blit_gc, xwnd->img, 0, 0, 0, 0, window->rct.w, window->rct.h);
        }
        break;
    default:
		break;
    }
}

static bool MapColorToWindow(SDL_ToolkitVideoWindow *window, SDL_ToolkitColor *color)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;
    XColor xcolor;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    xcolor.flags = DoRed | DoGreen | DoBlue;
    xcolor.red = color->rgba.r * 257;
    xcolor.green = color->rgba.g * 257;
    xcolor.blue = color->rgba.b * 257;
    if (X11_XAllocColor(xdrv->dpy, xwnd->cmap, &xcolor)) {
        color->rgba.r = xcolor.red / 257;
        color->rgba.g = xcolor.green / 257;
        color->rgba.b = xcolor.blue / 257;
        color->pixel = xcolor.pixel;
        return true;
    }

    return false;
}

static void XSettingsNotify(const char *name, XSettingsAction action, XSettingsSetting *setting, void *data)
{
    SDL_ToolkitVideoDriver *drv;
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ListNode *cursor;

    drv = data;
    xdrv = data;

    if (!SDL_strcmp(name, SDL_XSETTINGS_GDK_WINDOW_SCALING_FACTOR) || !SDL_strcmp(name, SDL_XSETTINGS_GDK_UNSCALED_DPI) || !SDL_strcmp(name, SDL_XSETTINGS_XFT_DPI)) {
        float new_scale;

        new_scale = X11_GetGlobalContentScale(xdrv->dpy, xdrv->xsettings);
        if (new_scale < 1) {
            new_scale = 1;
        }

        cursor = xdrv->scale_change_listeners;
        while (cursor) {
            SDL_ToolkitVideoScaleChangeListener *listener;

            listener = cursor->entry;
            listener->listener(drv, new_scale, listener->data);
            cursor = cursor->next;
        }
    }
}

static bool UpdateWindowRect(SDL_ToolkitVideoWindow *window, SDL_Rect *rct, ...)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;
    XSizeHints *sizehints;
    bool recreate;

    xdrv = (SDL_ToolkitVideoDriverX11 *)window->driver;
    xwnd = (SDL_ToolkitVideoWindowX11 *)window;

    if ((rct->w != window->rct.w || rct->h != window->rct.h) && (window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING || window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_SURFACE)) {
        recreate = true;
    } else {
        recreate = false;
    }

    window->rct = *rct;

    if (window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SINGLE || window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE) {
        window->buffer_w = window->rct.w;
        window->buffer_h = window->rct.h;
    }

    sizehints = X11_XAllocSizeHints();
    if (sizehints) {
        sizehints->flags = USPosition | USSize | PMaxSize | PMinSize;
        sizehints->x = window->rct.x;
        sizehints->y = window->rct.y;
        sizehints->width = sizehints->min_width = sizehints->max_width = window->rct.w;
        sizehints->height = sizehints->min_height = sizehints->max_height = window->rct.h;
        X11_XSetWMNormalHints(xdrv->dpy, xwnd->wnd, sizehints);
        X11_XFree(sizehints);
    }
    X11_XMoveResizeWindow(xdrv->dpy, xwnd->wnd, window->rct.x, window->rct.y, window->rct.w, window->rct.h);
    X11_XFlush(xdrv->dpy);

    if (recreate) {
        va_list args;
        unsigned int w;
        unsigned int h;

        if (window->buffer_type == SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_SCALING) {
            va_start(args, rct);
            w = va_arg(args, unsigned int);
            h = va_arg(args, unsigned int);
            va_end(args);
        } else {
            w = window->rct.w;
            h = window->rct.h;
        }

        DestroyWindowBuffer(window);
        CreateSoftwareBuffer(xdrv, xwnd, window, window->buffer_type, w, h);

        return true;
    } else {
        return false;
    }
}

static SDL_ToolkitVideoWindow *CreateWindow(SDL_ToolkitVideoDriver *driver, SDL_ToolkitVideoWindowType type, SDL_ToolkitVideoWindowParent *parent, SDL_Rect *rct, const char *title)
{
    SDL_ToolkitVideoWindowX11 *xwnd;
    SDL_ToolkitVideoWindow *ret;
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_WindowData *pdata;
    XSizeHints *sizehints;
    XSetWindowAttributes attr;
    Window pwin;
    Atom wndtype;
    Atom wndacttype;

    xdrv = (SDL_ToolkitVideoDriverX11 *)driver;
    xwnd = SDL_malloc(sizeof(SDL_ToolkitVideoWindowX11));
    ret = (SDL_ToolkitVideoWindow *)xwnd;
    if (!xwnd) {
        return NULL;
    }

    /* Set the screen and root window of the driver if not set yet */
    if (parent && parent->type == SDL_TOOLKIT_VIDEO_WINDOW_PARENT_SDL) {
        SDL_DisplayData *displaydata;

        displaydata = SDL_GetDisplayDriverDataForWindow(parent->parent.sdl);
        xdrv->screen = displaydata->screen;
    }

    if (xdrv->screen == -1) {
        xdrv->screen = DefaultScreen(xdrv->dpy);
    }

    if (xdrv->root == None) {
        xdrv->root = RootWindow(xdrv->dpy, xdrv->screen);
    }

    if (!xdrv->xsettings) {
        xdrv->xsettings = xsettings_client_new(xdrv->dpy, xdrv->screen, XSettingsNotify, NULL, xdrv);
    }

	ret->scale = X11_GetGlobalContentScale(xdrv->dpy, xdrv->xsettings);
	
    /* Visual and colormap */
    X11_GetVisualInfoFromVisual(xdrv->dpy, DefaultVisual(xdrv->dpy, xdrv->screen), &xwnd->vi);
    xwnd->cmap = X11_XCreateColormap(xdrv->dpy, xdrv->root, xwnd->vi.visual, AllocNone);

    /* Parent */
    pdata = NULL;
    pwin = xdrv->root;
    if (parent && parent->type == SDL_TOOLKIT_VIDEO_WINDOW_PARENT_SDL) {
        pdata = parent->parent.sdl->internal;
    }
    if (type == SDL_TOOLKIT_VIDEO_WINDOW_TYPE_EMBEDDED && pdata) {
        pwin = pdata->xwindow;
    }

    /* Create */
    attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask | PointerMotionMask;
    attr.colormap = xwnd->cmap;
    xwnd->wnd = X11_XCreateWindow(xdrv->dpy, pwin, rct->x, rct->y, rct->w, rct->h, 0, xwnd->vi.depth, InputOutput, xwnd->vi.visual, CWEventMask | CWColormap, &attr);
    SDL_X11_SetWindowTitle(xdrv->dpy, xwnd->wnd, (char *)title);
    X11_XSaveContext(xdrv->dpy, xwnd->wnd, xdrv->ctx, (XPointer)xwnd);

    /* Buffer */
    xwnd->blit_gc = None;
    ret->surface = NULL;
    xwnd->img = NULL;
    xwnd->drw = xwnd->wnd;
    ret->buffer_type = SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NONE;
    ret->buffer_w = rct->w;
    ret->buffer_h = rct->h;

    /* Set state hints */
    if (parent) {
        if (type == SDL_TOOLKIT_VIDEO_WINDOW_TYPE_DIALOG) {
            Atom stateatoms[16];
            Atom wmstate;
            size_t statecount;

            statecount = 0;
            wmstate = X11_XInternAtom(xdrv->dpy, "_NET_WM_STATE", False);

            stateatoms[statecount++] = X11_XInternAtom(xdrv->dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
            stateatoms[statecount++] = X11_XInternAtom(xdrv->dpy, "_NET_WM_STATE_SKIP_PAGER", False);
            stateatoms[statecount++] = X11_XInternAtom(xdrv->dpy, "_NET_WM_STATE_FOCUSED", False);
            stateatoms[statecount++] = X11_XInternAtom(xdrv->dpy, "_NET_WM_STATE_MODAL", False);
            SDL_assert(statecount <= SDL_arraysize(stateatoms));

            X11_XChangeProperty(xdrv->dpy, xwnd->wnd, wmstate, XA_ATOM, 32, PropModeReplace, (unsigned char *)stateatoms, statecount);
        }

        if (pdata) {
            X11_XSetTransientForHint(xdrv->dpy, xwnd->wnd, pdata->xwindow);
        }
    }

    /* Set window type */
    wndtype = X11_XInternAtom(xdrv->dpy, "_NET_WM_WINDOW_TYPE", False);
    if (type == SDL_TOOLKIT_VIDEO_WINDOW_TYPE_DIALOG) {
        wndacttype = X11_XInternAtom(xdrv->dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    }
    X11_XChangeProperty(xdrv->dpy, xwnd->wnd, wndtype, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wndacttype, 1);

    /* Allow the WM to close the window */
    xwnd->wm_delete_message = X11_XInternAtom(xdrv->dpy, "WM_DELETE_WINDOW", False);
    X11_XSetWMProtocols(xdrv->dpy, xwnd->wnd, &xwnd->wm_delete_message, 1);
    xwnd->wm_protocols = X11_XInternAtom(xdrv->dpy, "WM_PROTOCOLS", False);

    /* Set more WM hints */
    sizehints = X11_XAllocSizeHints();
    if (sizehints) {
        sizehints->flags = USPosition | USSize | PMaxSize | PMinSize;
        sizehints->x = rct->x;
        sizehints->y = rct->y;
        sizehints->width = sizehints->min_width = sizehints->max_width = rct->w;
        sizehints->height = sizehints->min_height = sizehints->max_height = rct->h;
        X11_XSetWMNormalHints(xdrv->dpy, xwnd->wnd, sizehints);
        X11_XFree(sizehints);
    }

    /* Populate parent type */
    ret->driver = driver;
    ret->parent = parent;
    ret->type = type;
    ret->rct = *rct;
    ret->Destroy = DestroyWindow;
    ret->Show = ShowWindow;
    ret->Hide = HideWindow;
    ret->CreateBuffer = CreateWindowBuffer;
    ret->BeginDraw = BeginWindowDraw;
    ret->EndDraw = EndWindowDraw;
    ret->MapColor = MapColorToWindow;
    ret->UpdateRect = UpdateWindowRect;
    ret->UpdateScale = UpdateWindowScale;
    ret->DestroyBuffer = DestroyWindowBuffer;
    ret->GetParentRect = GetParentRect;

    return ret;
}

static void DestroyVideoDriver(SDL_ToolkitVideoDriver *drv)
{
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv;
    if (xdrv->xsettings) {
        xsettings_client_destroy(xdrv->xsettings);
    }
    SDL_ListClear(&xdrv->scale_change_listeners);
    if (xdrv->flags & SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_CLOSE_DISPLAY) {
        X11_XCloseDisplay(xdrv->dpy);
    }
    SDL_X11_UnloadSymbols();
    SDL_free(drv);
}

static void SendEvent(SDL_ToolkitVideoDriver *drv, SDL_ToolkitVideoEvent *e)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;
    XEvent xe;

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv;
    xwnd = (SDL_ToolkitVideoWindowX11 *)e->wnd;

    switch (e->type) {
    case SDL_TOOLKIT_VIDEO_EVENT_CLOSE_REQUESTED:
        xe.xclient.type = ClientMessage;
        xe.xclient.window = xwnd->wnd;
        xe.xclient.message_type = xwnd->wm_protocols;
        xe.xclient.format = 32;
        xe.xclient.data.l[0] = xwnd->wm_delete_message;
        xe.xclient.data.l[1] = CurrentTime;
        break;
    case SDL_TOOLKIT_VIDEO_EVENT_EXPOSE:
        xe.xexpose.type = Expose;
        xe.xexpose.window = xwnd->wnd;
        xe.xexpose.count = 1;
        break;
    default:
        return;
    }

    X11_XSendEvent(xdrv->dpy, xwnd->wnd, False, NoEventMask, &xe);
}

static void NextEvent(SDL_ToolkitVideoDriver *drv, SDL_ToolkitVideoEvent *e)
{
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;
    XEvent xe;
    XPointer wnd_ptr;
    KeySym key;

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv;

    X11_XNextEvent(xdrv->dpy, &xe);

    xsettings_client_process_event(xdrv->xsettings, &xe);

    if ((xe.type != Expose) && X11_XFilterEvent(&xe, None)) {
        e->type = SDL_TOOLKIT_VIDEO_EVENT_NONE;
        e->wnd = NULL;
        return;
    }

    if (X11_XFindContext(xdrv->dpy, xe.xany.window, xdrv->ctx, &wnd_ptr) == XCNOENT) {
        e->type = SDL_TOOLKIT_VIDEO_EVENT_NONE;
        e->wnd = NULL;
    } else {
        e->wnd = (SDL_ToolkitVideoWindow *)wnd_ptr;
        xwnd = (SDL_ToolkitVideoWindowX11 *)wnd_ptr;
        switch (xe.type) {
        case Expose:
            e->type = SDL_TOOLKIT_VIDEO_EVENT_EXPOSE;
            break;
        case ClientMessage:
            if (xe.xclient.message_type == xwnd->wm_protocols && xe.xclient.format == 32 && xe.xclient.data.l[0] == xwnd->wm_delete_message) {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_CLOSE_REQUESTED;
            } else {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_NONE;
            }
            break;
        case FocusIn:
            e->type = SDL_TOOLKIT_VIDEO_EVENT_FOCUS_IN;
            break;
        case FocusOut:
            e->type = SDL_TOOLKIT_VIDEO_EVENT_FOCUS_OUT;
            break;
        case MotionNotify:
            e->type = SDL_TOOLKIT_VIDEO_EVENT_MOTION;
            e->data.motion.x = xe.xmotion.x;
            e->data.motion.y = xe.xmotion.y;
            break;
        case ButtonPress:
        case ButtonRelease:
            if (xe.type == ButtonPress) {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_BUTTON_PRESS;
            } else {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_BUTTON_RELEASE;
            }
            e->data.button.coords.x = xe.xbutton.x;
            e->data.button.coords.y = xe.xbutton.y;
            switch (xe.xbutton.button) {
            case Button1:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_ONE;
                break;
            case Button2:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_TWO;
                break;
            case Button3:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_THREE;
                break;
            case Button4:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_FOUR;
                break;
            case Button5:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_FIVE;
                break;
            default:
                e->data.button.button = SDL_TOOLKIT_VIDEO_BUTTON_UNKNOWN;
            }
            break;
        case KeyPress:
        case KeyRelease:
            if (xe.type == KeyPress) {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_KEY_PRESS;
            } else {
                e->type = SDL_TOOLKIT_VIDEO_EVENT_KEY_RELEASE;
            }
            key = X11_XLookupKeysym(&xe.xkey, 0);
            switch (key) {
            case XK_Escape:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_ESCAPE;
                break;
            case XK_Return:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_RETURN;
                break;
            case XK_KP_Enter:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_NUMERIC_ENTER;
                break;
            case XK_Tab:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_TAB;
                break;
            case XK_Left:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_LEFT;
                break;
            case XK_Right:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_RIGHT;
                break;
            default:
                e->data.key = SDL_TOOLKIT_VIDEO_KEY_UNKNOWN;
            }
            break;
        default:
            e->type = SDL_TOOLKIT_VIDEO_EVENT_NONE;
        }
    }
}

static void AddScaleChangeListener(SDL_ToolkitVideoDriver *drv, SDL_ToolkitVideoScaleChangeListener *listener)
{
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv;

    SDL_ListAdd(&xdrv->scale_change_listeners, listener);
}

static void RemoveScaleChangeListener(SDL_ToolkitVideoDriver *drv, SDL_ToolkitVideoScaleChangeListener *listener)
{
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv;

    SDL_ListRemove(&xdrv->scale_change_listeners, listener);
}

SDL_ToolkitVideoDriver *SDL_ToolkitVideoDriverX11_Create(SDL_VideoDevice *device)
{
    SDL_ToolkitVideoDriverX11 *drv;
    SDL_ToolkitVideoDriver *ret;
    int d1;
    int d2;

    if (device && !SDL_strcmp(device->name, "x11")) {
        device = NULL;
    }

    drv = SDL_malloc(sizeof(SDL_ToolkitVideoDriverX11));
    ret = (SDL_ToolkitVideoDriver *)drv;
    if (!drv) {
        return NULL;
    }

    if (!SDL_X11_LoadSymbols()) {
        SDL_free(drv);
        return NULL;
    }

    /* Clear flags, screen and root window are set later by CreateWindow */
    drv->flags = SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAGS_NONE;
    drv->screen = -1;
    drv->root = None;

    /* Reuse or open new display, create assoc context */
    X11_XInitThreads();
    if (device) {
        drv->dpy = device->internal->display;
    } else {
        drv->dpy = X11_XOpenDisplay(NULL);
        if (!drv->dpy) {
            SDL_X11_UnloadSymbols();
            SDL_free(drv);
        }
        drv->flags |= SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_CLOSE_DISPLAY;
    }
    drv->ctx = X11_XUniqueContext();
    drv->xsettings = NULL;
    drv->scale_change_listeners = NULL;

    /* Extensions */
#ifndef NO_SHARED_MEMORY
    if (SDL_X11_HAVE_SHM && X11_XShmQueryExtension(drv->dpy)) {
        Bool pixmap;

        drv->flags |= SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_SUPPORTED;
        X11_XShmQueryVersion(drv->dpy, &d1, &d2, &pixmap);
        if (X11_XShmPixmapFormat(drv->dpy) == ZPixmap && pixmap) {
            drv->flags |= SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_SHM_PIXMAPS_SUPPORTED;
        }
    }
#endif

#ifdef SDL_VIDEO_DRIVER_X11_XDBE
    if (SDL_X11_HAVE_XDBE && X11_XdbeQueryExtension(drv->dpy, &d1, &d2)) {
        drv->flags |= SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_DBE_SUPPORTED;
    }
#endif

#ifdef SDL_VIDEO_DRIVER_X11_XRANDR
    if (SDL_X11_HAVE_XRANDR && X11_XRRQueryExtension(drv->dpy, &d1, &d2)) {
        drv->flags |= SDL_TOOLKIT_VIDEO_DRIVER_X11_FLAG_RANDR_SUPPORTED;
    }
#endif

    /* Populate parent type */
    ret->type = SDL_TOOLKIT_DRIVER_TYPE_X11;
    ret->CreateWindow = CreateWindow;
    ret->NextEvent = NextEvent;
    ret->SendEvent = SendEvent;
    ret->Destroy = DestroyVideoDriver;
    ret->RemoveScaleChangeListener = RemoveScaleChangeListener;
    ret->AddScaleChangeListener = AddScaleChangeListener;
    ret->parent_device = device;

    return ret;
}

static void DestroyRenderDriver(SDL_ToolkitRenderDriver *drv)
{
    SDL_free(drv);
}

static void SetRenderContextColor(SDL_ToolkitRenderContext *context, SDL_ToolkitColor *color)
{
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)context->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)context;

    X11_XSetForeground(xdrv->dpy, xgc->gc, color->pixel);
}

static void RenderContextFillRect(SDL_ToolkitRenderContext *context, SDL_Rect *rct)
{
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)context->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)context;
    xwnd = (SDL_ToolkitVideoWindowX11 *)context->wnd;

    X11_XFillRectangle(xdrv->dpy, xwnd->drw, xgc->gc, rct->x, rct->y, rct->w, rct->h);
}

static void RenderContextFillCircle(SDL_ToolkitRenderContext *context, SDL_Rect *rct)
{
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitVideoWindowX11 *xwnd;

    xdrv = (SDL_ToolkitVideoDriverX11 *)context->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)context;
    xwnd = (SDL_ToolkitVideoWindowX11 *)context->wnd;

    X11_XFillArc(xdrv->dpy, xwnd->drw, xgc->gc, rct->x, rct->y, rct->w, rct->h, 0, 23040);
}

static void DestroyRenderContext(SDL_ToolkitRenderContext *context)
{
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoDriverX11 *xdrv;

    xdrv = (SDL_ToolkitVideoDriverX11 *)context->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)context;

    X11_XFreeGC(xdrv->dpy, xgc->gc);
    SDL_free(xgc);
}

static void RenderContextWindowBufferUpdated(SDL_ToolkitRenderContext *context)
{
    /* Do nothing, X11 GCs can be used on multiple drawables, we always use the drawable represnting the window buffer, which is automatically updated when the buffer setup is changed. */
}

static SDL_ToolkitRenderContext *CreateRenderContext(SDL_ToolkitRenderDriver *drv, SDL_ToolkitVideoWindow *wnd)
{
    SDL_ToolkitVideoWindowX11 *xwnd;
    SDL_ToolkitVideoDriverX11 *xdrv;
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitRenderContext *ret;

    if (wnd->driver != drv->viddrv) {
        return NULL;
    }

    xdrv = (SDL_ToolkitVideoDriverX11 *)drv->viddrv;
    xwnd = (SDL_ToolkitVideoWindowX11 *)wnd;
    xgc = SDL_malloc(sizeof(SDL_ToolkitRenderContextX11));
    ret = (SDL_ToolkitRenderContext *)xgc;
    if (!xgc) {
        return NULL;
    }

    /* Set up double buffering if possible */
    SDL_ToolkitVideoWindow_CreateBuffer(wnd, SDL_TOOLKIT_VIDEO_WINDOW_BUFFER_TYPE_NATIVE_DOUBLE, SDL_TOOLKIT_VARIADIC_PARAM_NONE);

    xgc->gc = X11_XCreateGC(xdrv->dpy, xwnd->wnd, 0, NULL);
    if (xgc->gc == None) {
        SDL_free(xgc);
        return NULL;
    }

    ret->drv = drv;
    ret->wnd = wnd;
    ret->SetColor = SetRenderContextColor;
    ret->FillRect = RenderContextFillRect;
    ret->FillCircle = RenderContextFillCircle;
    ret->WindowBufferUpdated = RenderContextWindowBufferUpdated;
    ret->Destroy = DestroyRenderContext;

    return ret;
}

SDL_ToolkitRenderDriver *SDL_ToolkitRenderDriverX11_Create(SDL_ToolkitVideoDriver *viddrv)
{
    SDL_ToolkitRenderDriver *drv;

    if (!viddrv) {
        return NULL;
    }

    if (viddrv->type != SDL_TOOLKIT_DRIVER_TYPE_X11) {
        return NULL;
    }

    drv = SDL_malloc(sizeof(SDL_ToolkitRenderDriver));
    if (!drv) {
        return NULL;
    }

    drv->viddrv = viddrv;
    drv->type = SDL_TOOLKIT_DRIVER_TYPE_X11;
    drv->Destroy = DestroyRenderDriver;
    drv->CreateContext = CreateRenderContext;

    return drv;
}

static void DestroyFont(SDL_ToolkitTextFont *fnt)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11 *xfnt;

    xvid = (SDL_ToolkitVideoDriverX11 *)fnt->drv->viddrv;
    xfnt = (SDL_ToolkitTextFontX11 *)fnt;

    X11_XFreeFont(xvid->dpy, xfnt->font);
    SDL_free(fnt);
}

#ifdef X_HAVE_UTF8_STRING
static void DestroyUnicodeFont(SDL_ToolkitTextFont *fnt)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11Unicode *xfnt;
    char *orig_locale;

    xvid = (SDL_ToolkitVideoDriverX11 *)fnt->drv->viddrv;
    xfnt = (SDL_ToolkitTextFontX11Unicode *)fnt;

    orig_locale = setlocale(LC_ALL, NULL);
    if (orig_locale) {
        orig_locale = SDL_strdup(orig_locale);
        if (orig_locale) {
            (void)setlocale(LC_ALL, "");
        }
    }

    X11_XFreeFontSet(xvid->dpy, xfnt->font);
    SDL_free(fnt);

    if (orig_locale) {
        (void)setlocale(LC_ALL, orig_locale);
        SDL_free(orig_locale);
    }
}

static SDL_ToolkitTextFont *CreateFontUnicode(SDL_ToolkitTextDriver *drv, const char **xlfds, unsigned int sz)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11Unicode *xfnt;
    SDL_ToolkitTextFont *ret;
    XFontSet font;
    XFontStruct **font_structs;
    XFontSetExtents *extents;
    char **font_names;
    char **missing;
    char *orig_locale;
    int num_missing;
    int font_sz;
    int i;
    unsigned int orig_sz;

    missing = NULL;
    num_missing = 0;
    orig_sz = sz;

    xvid = (SDL_ToolkitVideoDriverX11 *)drv->viddrv;

    orig_locale = setlocale(LC_ALL, NULL);
    if (orig_locale) {
        orig_locale = SDL_strdup(orig_locale);
        if (orig_locale) {
            (void)setlocale(LC_ALL, "");
        }
    }

    for (i = 0; xlfds[i]; ++i) {
        char *xlfd;

        if (SDL_strstr(xlfds[i], "%d")) {
        RETRY:
            SDL_asprintf(&xlfd, xlfds[i], sz * 10);
            font = X11_XCreateFontSet(xvid->dpy, xlfd, &missing, &num_missing, NULL);
            SDL_free(xlfd);

            if (!font) {
                sz -= 1;

                if (sz <= 0) {
                    sz = orig_sz;
                    continue;
                } else {
                    goto RETRY;
                }
            }

        } else {
            font = X11_XCreateFontSet(xvid->dpy, xlfds[i], &missing, &num_missing, NULL);
        }

        if (missing) {
            X11_XFreeStringList(missing);
        }

        if (font) {
            break;
        }
    }

    if (!font) {
        if (orig_locale) {
            (void)setlocale(LC_ALL, orig_locale);
            SDL_free(orig_locale);
        }
        return NULL;
    }

    xfnt = SDL_malloc(sizeof(SDL_ToolkitTextFontX11Unicode));
    ret = (SDL_ToolkitTextFont *)xfnt;
    if (!xfnt) {
        X11_XFreeFontSet(xvid->dpy, font);
        if (orig_locale) {
            (void)setlocale(LC_ALL, orig_locale);
            SDL_free(orig_locale);
        }
        return NULL;
    }

    /* TODO: What to do the XFontSet happens to have more than one Thai font? */
    xfnt->thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_NONE;
    xfnt->thai_font = SDL_TOOLKIT_THAI_FONT_X11_CELL;
    font_sz = X11_XFontsOfFontSet(font, &font_structs, &font_names);
    for (i = 0; i < font_sz; i++) {
        SDL_ToolkitThaiEncodingX11 thai_encoding;

        thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_NONE;
        if (SDL_strstr(font_names[i], "tis620-0")) {
            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_TIS;
        } else if (SDL_strstr(font_names[i], "tis620-1")) {
            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_TIS_MAC;
        } else if (SDL_strstr(font_names[i], "tis620-2")) {
            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_TIS_WIN;
        } else if (SDL_strstr(font_names[i], "iso8859-11")) {
            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_8859;
        } else if (SDL_strstr(font_names[i], "iso10646-1")) {
            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_UNICODE;
        }

        if (thai_encoding != SDL_TOOLKIT_THAI_ENCODING_X11_NONE) {
            XFontStruct *font_struct;

            font_struct = X11_XLoadQueryFont(xvid->dpy, font_names[i]);
            if (font_struct) {
                if (font_struct->per_char) {
                    int glyphs_sz;
                    int j;

                    glyphs_sz = (font_struct->max_char_or_byte2 - font_struct->min_char_or_byte2 + 1) * (font_struct->max_byte1 - font_struct->min_byte1 + 1);
                    if (thai_encoding == SDL_TOOLKIT_THAI_ENCODING_X11_UNICODE) {
                        if (glyphs_sz < 0x00000E00) {
                            thai_encoding = SDL_TOOLKIT_THAI_ENCODING_X11_NONE;
                        } else {
                            for (j = 0x00000E00; j < 0x00000E7F; j++) {
                                if (font_struct->per_char[j].lbearing < 0) {
                                    xfnt->thai_font = SDL_TOOLKIT_THAI_FONT_X11_OFFSET;
                                }
                            }
                        }
                    } else {
                        for (j = 0; j < glyphs_sz; j++) {
                            if (font_struct->per_char[j].lbearing < 0) {
                                xfnt->thai_font = SDL_TOOLKIT_THAI_FONT_X11_OFFSET;
                            }
                        }
                    }
                }
                X11_XFreeFont(xvid->dpy, font_struct);
            }
        }

        xfnt->thai_encoding = thai_encoding;
    }

    xfnt->do_shaping = !X11_XContextDependentDrawing(font);
    xfnt->font = font;
    ret->drv = drv;
    ret->type = SDL_TOOLKIT_FONT_TYPE_X11_UNICODE;
    ret->Destroy = DestroyUnicodeFont;

    extents = X11_XExtentsOfFontSet(font);
    ret->height = extents->max_logical_extent.height;

    if (orig_locale) {
        (void)setlocale(LC_ALL, orig_locale);
        SDL_free(orig_locale);
    }

    return ret;
}
#endif

static SDL_ToolkitTextFont *CreateFont(SDL_ToolkitTextDriver *drv, const char *xlfd, unsigned int sz)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11 *xfnt;
    SDL_ToolkitTextFont *ret;
    XFontStruct *font;
    char *xlfd_act;

    xvid = (SDL_ToolkitVideoDriverX11 *)drv->viddrv;
    font = NULL;
    xlfd_act = NULL;

RETRY:
    if (SDL_strstr(xlfd, "%d")) {
        SDL_asprintf(&xlfd_act, xlfd, sz * 10);
    } else {
        xlfd_act = SDL_strdup(xlfd);
    }

    font = X11_XLoadQueryFont(xvid->dpy, xlfd_act);
    SDL_free(xlfd_act);

    if (!font) {
        xlfd_act = NULL;

        sz -= 1;
        if (sz <= 0) {
            font = X11_XLoadQueryFont(xvid->dpy, ANY_FONT_LATIN1_XLFD);
            if (!font) {
                return NULL;
            }
        } else {
            goto RETRY;
        }
    }

    xfnt = SDL_malloc(sizeof(SDL_ToolkitTextFontX11));
    ret = (SDL_ToolkitTextFont *)xfnt;
    if (!xfnt) {
        X11_XFreeFont(xvid->dpy, font);
        return NULL;
    }

    ret->height = font->ascent + font->descent;
    xfnt->font = font;
    ret->drv = drv;
    ret->type = SDL_TOOLKIT_FONT_TYPE_X11;
    ret->Destroy = DestroyFont;

    return ret;
}

static SDL_ToolkitTextFont *CreateFontWrapper(SDL_ToolkitTextDriver *drv, unsigned int sz, bool icon)
{
    if (icon) {
        return CreateFont(drv, ICON_FONT_XLFD, sz);
    } else {
#ifdef X_HAVE_UTF8_STRING
        if (SDL_X11_HAVE_UTF8) {
            SDL_ToolkitTextFont *ret;

            ret = CreateFontUnicode(drv, ui_fontset_xlfds, sz);
            if (ret) {
                return ret;
            } else {
                goto CREATE_STANDARD_FONT;
            }
        } else {
        CREATE_STANDARD_FONT:

            return CreateFont(drv, UI_FONT_LATIN1_XLFD, sz);
        }
#else
        return CreateFont(drv, UI_FONT_LATIN1_XLFD, sz);
#endif
    }
}

void DestroyTextDriver(SDL_ToolkitTextDriver *drv)
{
    SDL_ToolkitTextDriverX11 *xdrv;

    xdrv = (SDL_ToolkitTextDriverX11 *)drv;

#ifdef HAVE_FRIBIDI_H
    SDL_FriBidi_Destroy(xdrv->fribidi);
#endif

#ifdef HAVE_LIBTHAI_H
    SDL_LibThai_Destroy(xdrv->th);
#endif

    SDL_free(xdrv);
}

static void DestroyObject(SDL_ToolkitTextObject *obj)
{
    SDL_ToolkitTextObjectX11 *xobj;

    xobj = (SDL_ToolkitTextObjectX11 *)obj;
    SDL_free(xobj->str);
    SDL_free(xobj);
}

static void DrawObject(SDL_ToolkitTextObject *obj, SDL_ToolkitRenderContext *ctx, int x, int y)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11 *xfnt;
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoWindowX11 *xwnd;
    SDL_ToolkitTextObjectX11 *xobj;

    xvid = (SDL_ToolkitVideoDriverX11 *)obj->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)ctx;
    xwnd = (SDL_ToolkitVideoWindowX11 *)ctx->wnd;
    xobj = (SDL_ToolkitTextObjectX11 *)obj;
    xfnt = (SDL_ToolkitTextFontX11 *)obj->fnt;

    X11_XSetFont(xvid->dpy, xgc->gc, xfnt->font->fid);
    X11_XDrawString(xvid->dpy, xwnd->drw, xgc->gc, x, y + xobj->asc, xobj->str, xobj->sz);
}

#ifdef X_HAVE_UTF8_STRING
static void DestroyUnicodeObject(SDL_ToolkitTextObject *obj)
{
    SDL_ToolkitTextObjectX11Unicode *xobj;
    SDL_ListNode *cursor;

    xobj = (SDL_ToolkitTextObjectX11Unicode *)obj;

    cursor = xobj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;
        SDL_ListNode *overlay_cursor;

        element = cursor->entry;
        overlay_cursor = element->overlays;
        while (overlay_cursor) {
            SDL_ToolkitTextObjectElementX11 *overlay;

            overlay = overlay_cursor->entry;
            SDL_free(overlay->str);
            SDL_free(overlay);
            overlay_cursor = overlay_cursor->next;
        }
        SDL_ListClear(&element->overlays);

        SDL_free(element->str);
        SDL_free(element);

        cursor = cursor->next;
    }
    SDL_ListClear(&xobj->elems);
    SDL_free(xobj);
}

static void DrawUnicodeObject(SDL_ToolkitTextObject *obj, SDL_ToolkitRenderContext *ctx, int x, int y)
{
    SDL_ToolkitVideoDriverX11 *xvid;
    SDL_ToolkitTextFontX11Unicode *xfnt;
    SDL_ToolkitRenderContextX11 *xgc;
    SDL_ToolkitVideoWindowX11 *xwnd;
    SDL_ToolkitTextObjectX11Unicode *xobj;
    SDL_ListNode *cursor;
    char *orig_locale;

    xvid = (SDL_ToolkitVideoDriverX11 *)obj->drv->viddrv;
    xgc = (SDL_ToolkitRenderContextX11 *)ctx;
    xwnd = (SDL_ToolkitVideoWindowX11 *)ctx->wnd;
    xobj = (SDL_ToolkitTextObjectX11Unicode *)obj;
    xfnt = (SDL_ToolkitTextFontX11Unicode *)obj->fnt;

    orig_locale = setlocale(LC_ALL, NULL);
    if (orig_locale) {
        orig_locale = SDL_strdup(orig_locale);
        if (orig_locale) {
            (void)setlocale(LC_ALL, "");
        }
    }

    cursor = xobj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;
        SDL_ListNode *overlay_cursor;

        element = cursor->entry;
        X11_Xutf8DrawString(xvid->dpy, xwnd->drw, xfnt->font, xgc->gc, x + element->rct.x, y + element->rct.y + element->asc, element->str, element->sz);

        overlay_cursor = element->overlays;
        while (overlay_cursor) {
            SDL_ToolkitTextObjectElementX11 *overlay;

            overlay = overlay_cursor->entry;
            X11_Xutf8DrawString(xvid->dpy, xwnd->drw, xfnt->font, xgc->gc, x + element->rct.x + overlay->rct.x, y + element->rct.y + overlay->rct.y + overlay->asc, overlay->str, overlay->sz);
            overlay_cursor = overlay_cursor->next;
        }

        cursor = cursor->next;
    }

    if (orig_locale) {
        (void)setlocale(LC_ALL, orig_locale);
        SDL_free(orig_locale);
    }
}

static SDL_ListNode *SeperateByScript(SDL_ToolkitTextDriverX11 *xdrv, SDL_ToolkitTextFontX11Unicode *xfnt, char *txt, size_t sz, SDL_ToolkitTextDirection *dir)
{
    SDL_ListNode *list;
    char *str;
    char *buffer;
    Uint32 cp;
    bool thai;
    bool free_txt;
#ifdef HAVE_FRIBIDI_H
    FriBidiParType par;
#endif

    free_txt = false;
    list = NULL;
    thai = 0;
    str = txt;
    buffer = SDL_malloc(1);
    buffer[0] = 0;

#ifdef HAVE_FRIBIDI_H
    par = FRIBIDI_PAR_LTR;
    if (xdrv->fribidi) {
        char *shaped_str;

        shaped_str = SDL_FriBidi_Process(xdrv->fribidi, txt, sz, xfnt->do_shaping, &par);
        if (shaped_str) {
            txt = shaped_str;
            str = shaped_str;
            sz = SDL_strlen(shaped_str);
            free_txt = true;
        }
    }

    switch (par) {
    case FRIBIDI_PAR_RTL:
        *dir = SDL_TOOLKIT_TEXT_DIRECTION_RTL;
        break;
    case FRIBIDI_PAR_ON:
        *dir = SDL_TOOLKIT_TEXT_DIRECTION_NEUTRAL;
        break;
    default:
        *dir = SDL_TOOLKIT_TEXT_DIRECTION_LTR;
    }
#else
    *dir = SDL_TOOLKIT_TEXT_DIRECTION_LTR;
#endif

    while (1) {
        char *new;
        char utf8[5];
        size_t csz;
        bool cond;

        SDL_zeroa(utf8);
        cp = SDL_StepUTF8((const char **)&str, &sz);
        cond = (0xe00 <= cp && cp <= 0xe7f) ? true : false;
        if (!cp || cond == (thai ? false : true)) {
            SDL_ToolkitTextObjectElementX11 *element;

            element = SDL_malloc(sizeof(SDL_ToolkitTextObjectElementX11));
            if (thai) {
                element->type = SDL_TOOLKIT_TEXT_OBJECT_ELEMENT_TYPE_X11_THAI;
            } else {
                element->type = SDL_TOOLKIT_TEXT_OBJECT_ELEMENT_TYPE_X11_GENERIC;
            }
            element->str = SDL_strdup(buffer);
            element->sz = SDL_strlen(buffer);
            element->overlays = NULL;
            element->rct.x = element->rct.y = 0;
            element->rct.w = element->rct.h = 0;

            SDL_ListAdd(&list, element);

            SDL_free(buffer);
            buffer = SDL_malloc(1);
            buffer[0] = 0;
            thai = thai ? false : true;
        }

        if (!cp) {
            break;
        }

        SDL_UCS4ToUTF8(cp, utf8);
        csz = SDL_strlen(buffer) + SDL_strlen(utf8) + 1;
        new = SDL_malloc(csz);
        SDL_strlcpy(new, buffer, csz);
        SDL_strlcat(new, utf8, csz);
        SDL_free(buffer);
        buffer = new;
    }

    SDL_free(buffer);
    if (free_txt) {
        SDL_free(txt);
    }
    return list;
}

static void ShapeElements(SDL_ToolkitTextObjectX11Unicode *obj, SDL_ToolkitTextDriverX11 *xdrv, SDL_ToolkitTextFontX11Unicode *xfnt)
{
    SDL_ListNode *cursor;

    cursor = obj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;
        XRectangle overall_ink;
        XRectangle overall_logical;

        element = cursor->entry;

        X11_Xutf8TextExtents(xfnt->font, element->str, element->sz, &overall_ink, &overall_logical);
        element->rct.w = overall_logical.width;
        element->rct.h = overall_logical.height;
        element->asc = -overall_logical.y;
        cursor = cursor->next;
    }
}

#ifdef HAVE_LIBTHAI_H
static void ShapeElementsThai(SDL_ToolkitTextObjectX11Unicode *obj, SDL_ToolkitTextDriverX11 *xdrv, SDL_ToolkitTextFontX11Unicode *xfnt)
{
    SDL_ListNode *cursor;

    cursor = obj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;
        XRectangle overall_ink;
        XRectangle overall_logical;

        element = cursor->entry;
        if (element->type == SDL_TOOLKIT_TEXT_OBJECT_ELEMENT_TYPE_X11_THAI) {
            struct thcell_t *cells;
            char *tis_str;
            char *base_tis_str;
            size_t cells_sz;
            size_t base_tis_str_sz;
            size_t tis_str_sz;
            int temp;

            tis_str = SDL_iconv_string("TIS-620", "UTF-8", element->str, element->sz);
            cells_sz = tis_str_sz = SDL_strlen(tis_str);

            cells = SDL_calloc(cells_sz, sizeof(struct thcell_t));
            xdrv->th->make_cells((const thchar_t *)tis_str, tis_str_sz, cells, &cells_sz, 0);

            base_tis_str_sz = cells_sz;
            base_tis_str = SDL_malloc(base_tis_str_sz + 1);
            for (temp = 0; temp < cells_sz; temp++) {
                base_tis_str[temp] = cells[temp].base;

                if (cells[temp].hilo) {
                    SDL_ToolkitTextObjectElementX11 *overlay;
                    char *pre;

                    overlay = SDL_malloc(sizeof(SDL_ToolkitTextObjectElementX11));
                    pre = SDL_iconv_string("UTF-8", "TIS-620", base_tis_str, temp);
                    overlay->str = SDL_iconv_string("UTF-8", "TIS-620", (const char *)&cells[temp].hilo, 1);
                    overlay->sz = SDL_strlen(overlay->str);

                    X11_Xutf8TextExtents(xfnt->font, pre, SDL_strlen(pre), &overall_ink, &overall_logical);
                    overlay->rct.x = overall_logical.width;
                    overlay->rct.y = 0;

                    X11_Xutf8TextExtents(xfnt->font, overlay->str, overlay->sz, &overall_ink, &overall_logical);
                    overlay->rct.w = overall_logical.width;
                    overlay->rct.h = overall_logical.height;
                    overlay->asc = -overall_logical.y;

                    SDL_ListAdd(&element->overlays, overlay);
                    SDL_free(pre);
                }

                if (cells[temp].top) {
                    SDL_ToolkitTextObjectElementX11 *overlay;
                    char *pre;

                    overlay = SDL_malloc(sizeof(SDL_ToolkitTextObjectElementX11));
                    pre = SDL_iconv_string("UTF-8", "TIS-620", base_tis_str, temp);
                    overlay->str = SDL_iconv_string("UTF-8", "TIS-620", (const char *)&cells[temp].top, 1);
                    overlay->sz = SDL_strlen(overlay->str);

                    X11_Xutf8TextExtents(xfnt->font, pre, SDL_strlen(pre), &overall_ink, &overall_logical);
                    overlay->rct.x = overall_logical.width;
                    overlay->rct.y = 0;

                    X11_Xutf8TextExtents(xfnt->font, overlay->str, overlay->sz, &overall_ink, &overall_logical);
                    overlay->rct.w = overall_logical.width;
                    overlay->rct.h = overall_logical.height;
                    overlay->asc = -overall_logical.y;

                    SDL_ListAdd(&element->overlays, overlay);
                    SDL_free(pre);
                }
            }
            base_tis_str[base_tis_str_sz] = '\0';

            SDL_free(element->str);
            element->str = SDL_iconv_string("UTF-8", "TIS-620", base_tis_str, base_tis_str_sz);
            element->sz = SDL_strlen(element->str);

            X11_Xutf8TextExtents(xfnt->font, element->str, element->sz, &overall_ink, &overall_logical);
            element->rct.w = overall_logical.width;
            element->rct.h = overall_logical.height;
            element->asc = -overall_logical.y;

            SDL_free(tis_str);
            SDL_free(cells);
            SDL_free(base_tis_str);
        } else {
            X11_Xutf8TextExtents(xfnt->font, element->str, element->sz, &overall_ink, &overall_logical);
            element->rct.w = overall_logical.width;
            element->rct.h = overall_logical.height;
            element->asc = -overall_logical.y;
        }
        cursor = cursor->next;
    }
}
#endif

static SDL_ToolkitTextObject *CreateUnicodeObject(SDL_ToolkitTextDriver *drv, SDL_ToolkitTextFont *fnt, char *txt, size_t sz)
{
    SDL_ToolkitTextDriverX11 *xdrv;
    SDL_ToolkitTextFontX11Unicode *xfnt;
    SDL_ToolkitTextObjectX11Unicode *obj;
    SDL_ToolkitTextObject *ret;
    SDL_ListNode *cursor;
    SDL_ToolkitTextObjectElementX11 *prev;
    char *orig_locale;

    xdrv = (SDL_ToolkitTextDriverX11 *)drv;
    xfnt = (SDL_ToolkitTextFontX11Unicode *)fnt;
    obj = SDL_malloc(sizeof(SDL_ToolkitTextObjectX11));
    ret = (SDL_ToolkitTextObject *)obj;
    if (!obj) {
        return NULL;
    }

    orig_locale = setlocale(LC_ALL, NULL);
    if (orig_locale) {
        orig_locale = SDL_strdup(orig_locale);
        if (orig_locale) {
            (void)setlocale(LC_ALL, "");
        }
    }

    ret->drv = drv;
    ret->fnt = fnt;

    obj->elems = SeperateByScript(xdrv, xfnt, txt, sz, &ret->dir);

#ifdef HAVE_LIBTHAI_H
    if (xdrv->th && xfnt->thai_encoding != SDL_TOOLKIT_THAI_ENCODING_X11_NONE && xfnt->thai_font == SDL_TOOLKIT_THAI_FONT_X11_CELL) {
        ShapeElementsThai(obj, xdrv, xfnt);
    } else {
        ShapeElements(obj, xdrv, xfnt);
    }
#else
    ShapeElements(obj, xdrv, xfnt);
#endif

    prev = NULL;
    cursor = obj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;

        element = cursor->entry;
        if (prev) {
            element->rct.x = prev->rct.x + prev->rct.w;
        } else {
            element->rct.x = 0;
        }

        prev = element;
        cursor = cursor->next;
    }

    ret->w = ret->h = 0;
    cursor = obj->elems;
    while (cursor) {
        SDL_ToolkitTextObjectElementX11 *element;

        element = cursor->entry;

        ret->w += element->rct.w;
        ret->h = SDL_max(ret->h, element->rct.h);
        cursor = cursor->next;
    }

    ret->Draw = DrawUnicodeObject;
    ret->Destroy = DestroyUnicodeObject;

    if (orig_locale) {
        (void)setlocale(LC_ALL, orig_locale);
        SDL_free(orig_locale);
    }

    return ret;
}
#endif

static SDL_ToolkitTextObject *CreateObject(SDL_ToolkitTextDriver *drv, SDL_ToolkitTextFont *fnt, char *txt, size_t sz)
{
    SDL_ToolkitTextFontX11 *xfnt;
    SDL_ToolkitTextObjectX11 *obj;
    SDL_ToolkitTextObject *ret;
    XCharStruct text_structure;
    int font_direction;
    int font_ascent;
    int font_descent;

    xfnt = (SDL_ToolkitTextFontX11 *)fnt;
    obj = SDL_malloc(sizeof(SDL_ToolkitTextObjectX11));
    ret = (SDL_ToolkitTextObject *)obj;
    if (!obj) {
        return NULL;
    }

    ret->dir = SDL_TOOLKIT_TEXT_DIRECTION_LTR;
    ret->drv = drv;
    ret->fnt = fnt;
    obj->str = SDL_strndup(txt, sz);
    obj->sz = sz;

    X11_XTextExtents(xfnt->font, txt, sz, &font_direction, &font_ascent, &font_descent, &text_structure);
    ret->w = text_structure.width;
    ret->h = text_structure.ascent + text_structure.descent;
    obj->asc = text_structure.ascent;

    ret->Draw = DrawObject;
    ret->Destroy = DestroyObject;

    return ret;
}

static SDL_ToolkitTextObject *CreateObjectWrapper(SDL_ToolkitTextDriver *drv, SDL_ToolkitTextFont *fnt, char *txt, size_t sz)
{
    if (fnt->type == SDL_TOOLKIT_FONT_TYPE_X11_UNICODE) {
#ifdef X_HAVE_UTF8_STRING
        return CreateUnicodeObject(drv, fnt, txt, sz);
#else
        return CreateObject(drv, fnt, txt, sz);
#endif
    } else {
        return CreateObject(drv, fnt, txt, sz);
    }
}

SDL_ToolkitTextDriver *SDL_ToolkitTextDriverX11_Create(SDL_ToolkitVideoDriver *viddrv, SDL_ToolkitRenderDriver *renddrv)
{
    SDL_ToolkitTextDriverX11 *drv;
    SDL_ToolkitTextDriver *ret;

    if (!viddrv || !renddrv) {
        return NULL;
    }

    if (viddrv->type != SDL_TOOLKIT_DRIVER_TYPE_X11 || renddrv->type != SDL_TOOLKIT_DRIVER_TYPE_X11) {
        return NULL;
    }

    drv = SDL_malloc(sizeof(SDL_ToolkitTextDriverX11));
    ret = (SDL_ToolkitTextDriver *)drv;
    if (!drv) {
        return NULL;
    }

#ifdef HAVE_FRIBIDI_H
    drv->fribidi = SDL_FriBidi_Create();
#endif

#ifdef HAVE_LIBTHAI_H
    drv->th = SDL_LibThai_Create();
#endif

    ret->type = SDL_TOOLKIT_DRIVER_TYPE_X11;
    ret->viddrv = viddrv;
    ret->CreateFont = CreateFontWrapper;
    ret->Destroy = DestroyTextDriver;
    ret->CreateObject = CreateObjectWrapper;

    return ret;
}

#endif // SDL_VIDEO_DRIVER_X11
