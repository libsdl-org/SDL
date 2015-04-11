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

#if SDL_VIDEO_DRIVER_X11

#include "SDL_hints.h"
#include "SDL_x11video.h"
#include "SDL_timer.h"
#include "edid.h"

/* #define X11MODES_DEBUG */

/* I'm becoming more and more convinced that the application should never
 * use XRandR, and it's the window manager's responsibility to track and
 * manage display modes for fullscreen windows.  Right now XRandR is completely
 * broken with respect to window manager behavior on every window manager that
 * I can find.  For example, on Unity 3D if you show a fullscreen window while
 * the resolution is changing (within ~250 ms) your window will retain the
 * fullscreen state hint but be decorated and windowed.
 *
 * However, many people swear by it, so let them swear at it. :)
*/
/* #define XRANDR_DISABLED_BY_DEFAULT */


static int
get_visualinfo(Display * display, int screen, XVisualInfo * vinfo)
{
    const char *visual_id = SDL_getenv("SDL_VIDEO_X11_VISUALID");
    int depth;

    /* Look for an exact visual, if requested */
    if (visual_id) {
        XVisualInfo *vi, template;
        int nvis;

        SDL_zero(template);
        template.visualid = SDL_strtol(visual_id, NULL, 0);
        vi = X11_XGetVisualInfo(display, VisualIDMask, &template, &nvis);
        if (vi) {
            *vinfo = *vi;
            X11_XFree(vi);
            return 0;
        }
    }

    depth = DefaultDepth(display, screen);
    if ((X11_UseDirectColorVisuals() &&
         X11_XMatchVisualInfo(display, screen, depth, DirectColor, vinfo)) ||
        X11_XMatchVisualInfo(display, screen, depth, TrueColor, vinfo) ||
        X11_XMatchVisualInfo(display, screen, depth, PseudoColor, vinfo) ||
        X11_XMatchVisualInfo(display, screen, depth, StaticColor, vinfo)) {
        return 0;
    }
    return -1;
}

int
X11_GetVisualInfoFromVisual(Display * display, Visual * visual, XVisualInfo * vinfo)
{
    XVisualInfo *vi;
    int nvis;

    vinfo->visualid = X11_XVisualIDFromVisual(visual);
    vi = X11_XGetVisualInfo(display, VisualIDMask, vinfo, &nvis);
    if (vi) {
        *vinfo = *vi;
        X11_XFree(vi);
        return 0;
    }
    return -1;
}

Uint32
X11_GetPixelFormatFromVisualInfo(Display * display, XVisualInfo * vinfo)
{
    if (vinfo->class == DirectColor || vinfo->class == TrueColor) {
        int bpp;
        Uint32 Rmask, Gmask, Bmask, Amask;

        Rmask = vinfo->visual->red_mask;
        Gmask = vinfo->visual->green_mask;
        Bmask = vinfo->visual->blue_mask;
        if (vinfo->depth == 32) {
            Amask = (0xFFFFFFFF & ~(Rmask | Gmask | Bmask));
        } else {
            Amask = 0;
        }

        bpp = vinfo->depth;
        if (bpp == 24) {
            int i, n;
            XPixmapFormatValues *p = X11_XListPixmapFormats(display, &n);
            if (p) {
                for (i = 0; i < n; ++i) {
                    if (p[i].depth == 24) {
                        bpp = p[i].bits_per_pixel;
                        break;
                    }
                }
                X11_XFree(p);
            }
        }

        return SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask);
    }

    if (vinfo->class == PseudoColor || vinfo->class == StaticColor) {
        switch (vinfo->depth) {
        case 8:
            return SDL_PIXELTYPE_INDEX8;
        case 4:
            if (BitmapBitOrder(display) == LSBFirst) {
                return SDL_PIXELFORMAT_INDEX4LSB;
            } else {
                return SDL_PIXELFORMAT_INDEX4MSB;
            }
            break;
        case 1:
            if (BitmapBitOrder(display) == LSBFirst) {
                return SDL_PIXELFORMAT_INDEX1LSB;
            } else {
                return SDL_PIXELFORMAT_INDEX1MSB;
            }
            break;
        }
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

/* Global for the error handler */
int vm_event, vm_error = -1;

#if SDL_VIDEO_DRIVER_X11_XINERAMA
static SDL_bool
CheckXinerama(Display * display, int *major, int *minor)
{
    int event_base = 0;
    int error_base = 0;
    const char *env;

    /* Default the extension not available */
    *major = *minor = 0;

    /* Allow environment override */
    env = SDL_GetHint(SDL_HINT_VIDEO_X11_XINERAMA);
    if (env && !SDL_atoi(env)) {
#ifdef X11MODES_DEBUG
        printf("Xinerama disabled due to hint\n");
#endif
        return SDL_FALSE;
    }

    if (!SDL_X11_HAVE_XINERAMA) {
#ifdef X11MODES_DEBUG
        printf("Xinerama support not available\n");
#endif
        return SDL_FALSE;
    }

    /* Query the extension version */
    if (!X11_XineramaQueryExtension(display, &event_base, &error_base) ||
        !X11_XineramaQueryVersion(display, major, minor) ||
        !X11_XineramaIsActive(display)) {
#ifdef X11MODES_DEBUG
        printf("Xinerama not active on the display\n");
#endif
        return SDL_FALSE;
    }
#ifdef X11MODES_DEBUG
    printf("Xinerama available at version %d.%d!\n", *major, *minor);
#endif
    return SDL_TRUE;
}
#endif /* SDL_VIDEO_DRIVER_X11_XINERAMA */

#if SDL_VIDEO_DRIVER_X11_XRANDR
static SDL_bool
CheckXRandR(Display * display, int *major, int *minor)
{
    const char *env;

    /* Default the extension not available */
    *major = *minor = 0;

    /* Allow environment override */
    env = SDL_GetHint(SDL_HINT_VIDEO_X11_XRANDR);
#ifdef XRANDR_DISABLED_BY_DEFAULT
    if (!env || !SDL_atoi(env)) {
#ifdef X11MODES_DEBUG
        printf("XRandR disabled by default due to window manager issues\n");
#endif
        return SDL_FALSE;
    }
#else
    if (env && !SDL_atoi(env)) {
#ifdef X11MODES_DEBUG
        printf("XRandR disabled due to hint\n");
#endif
        return SDL_FALSE;
    }
#endif /* XRANDR_ENABLED_BY_DEFAULT */

    if (!SDL_X11_HAVE_XRANDR) {
#ifdef X11MODES_DEBUG
        printf("XRandR support not available\n");
#endif
        return SDL_FALSE;
    }

    /* Query the extension version */
    if (!X11_XRRQueryVersion(display, major, minor)) {
#ifdef X11MODES_DEBUG
        printf("XRandR not active on the display\n");
#endif
        return SDL_FALSE;
    }
#ifdef X11MODES_DEBUG
    printf("XRandR available at version %d.%d!\n", *major, *minor);
#endif
    return SDL_TRUE;
}

#define XRANDR_ROTATION_LEFT    (1 << 1)
#define XRANDR_ROTATION_RIGHT   (1 << 3)

static int
CalculateXRandRRefreshRate(const XRRModeInfo *info)
{
    return (info->hTotal
            && info->vTotal) ? (info->dotClock / (info->hTotal * info->vTotal)) : 0;
}

static SDL_bool
SetXRandRModeInfo(Display *display, XRRScreenResources *res, XRROutputInfo *output_info,
                  RRMode modeID, SDL_DisplayMode *mode)
{
    int i;
    for (i = 0; i < res->nmode; ++i) {
        if (res->modes[i].id == modeID) {
            XRRCrtcInfo *crtc;
            Rotation rotation = 0;
            const XRRModeInfo *info = &res->modes[i];

            crtc = X11_XRRGetCrtcInfo(display, res, output_info->crtc);
            if (crtc) {
                rotation = crtc->rotation;
                X11_XRRFreeCrtcInfo(crtc);
            }

            if (rotation & (XRANDR_ROTATION_LEFT|XRANDR_ROTATION_RIGHT)) {
                mode->w = info->height;
                mode->h = info->width;
            } else {
                mode->w = info->width;
                mode->h = info->height;
            }
            mode->refresh_rate = CalculateXRandRRefreshRate(info);
            ((SDL_DisplayModeData*)mode->driverdata)->xrandr_mode = modeID;
#ifdef X11MODES_DEBUG
            printf("XRandR mode %d: %dx%d@%dHz\n", (int) modeID, mode->w, mode->h, mode->refresh_rate);
#endif
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}
#endif /* SDL_VIDEO_DRIVER_X11_XRANDR */

#if SDL_VIDEO_DRIVER_X11_XVIDMODE
static SDL_bool
CheckVidMode(Display * display, int *major, int *minor)
{
    const char *env;

    /* Default the extension not available */
    *major = *minor = 0;

    /* Allow environment override */
    env = SDL_GetHint(SDL_HINT_VIDEO_X11_XVIDMODE);
    if (env && !SDL_atoi(env)) {
#ifdef X11MODES_DEBUG
        printf("XVidMode disabled due to hint\n");
#endif
        return SDL_FALSE;
    }

    if (!SDL_X11_HAVE_XVIDMODE) {
#ifdef X11MODES_DEBUG
        printf("XVidMode support not available\n");
#endif
        return SDL_FALSE;
    }

    /* Query the extension version */
    vm_error = -1;
    if (!X11_XF86VidModeQueryExtension(display, &vm_event, &vm_error)
        || !X11_XF86VidModeQueryVersion(display, major, minor)) {
#ifdef X11MODES_DEBUG
        printf("XVidMode not active on the display\n");
#endif
        return SDL_FALSE;
    }
#ifdef X11MODES_DEBUG
    printf("XVidMode available at version %d.%d!\n", *major, *minor);
#endif
    return SDL_TRUE;
}

static
Bool XF86VidModeGetModeInfo(Display * dpy, int scr,
                                       XF86VidModeModeInfo* info)
{
    Bool retval;
    int dotclock;
    XF86VidModeModeLine l;
    SDL_zerop(info);
    SDL_zero(l);
    retval = X11_XF86VidModeGetModeLine(dpy, scr, &dotclock, &l);
    info->dotclock = dotclock;
    info->hdisplay = l.hdisplay;
    info->hsyncstart = l.hsyncstart;
    info->hsyncend = l.hsyncend;
    info->htotal = l.htotal;
    info->hskew = l.hskew;
    info->vdisplay = l.vdisplay;
    info->vsyncstart = l.vsyncstart;
    info->vsyncend = l.vsyncend;
    info->vtotal = l.vtotal;
    info->flags = l.flags;
    info->privsize = l.privsize;
    info->private = l.private;
    return retval;
}

static int
CalculateXVidModeRefreshRate(const XF86VidModeModeInfo * info)
{
    return (info->htotal
            && info->vtotal) ? (1000 * info->dotclock / (info->htotal *
                                                         info->vtotal)) : 0;
}

SDL_bool
SetXVidModeModeInfo(const XF86VidModeModeInfo *info, SDL_DisplayMode *mode)
{
    mode->w = info->hdisplay;
    mode->h = info->vdisplay;
    mode->refresh_rate = CalculateXVidModeRefreshRate(info);
    ((SDL_DisplayModeData*)mode->driverdata)->vm_mode = *info;
    return SDL_TRUE;
}
#endif /* SDL_VIDEO_DRIVER_X11_XVIDMODE */

int
X11_InitModes(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    int snum, screen, screencount;
#if SDL_VIDEO_DRIVER_X11_XINERAMA
    int xinerama_major, xinerama_minor;
    int use_xinerama = 0;
    XineramaScreenInfo *xinerama = NULL;
#endif
#if SDL_VIDEO_DRIVER_X11_XRANDR
    int xrandr_major, xrandr_minor;
    int use_xrandr = 0;
    XRRScreenResources *res = NULL;
#endif
#if SDL_VIDEO_DRIVER_X11_XVIDMODE
    int vm_major, vm_minor;
    int use_vidmode = 0;
#endif

#if SDL_VIDEO_DRIVER_X11_XINERAMA
    /* Query Xinerama extention
     * NOTE: This works with Nvidia Twinview correctly, but you need version 302.17 (released on June 2012)
     *       or newer of the Nvidia binary drivers
     */
    if (CheckXinerama(data->display, &xinerama_major, &xinerama_minor)) {
        xinerama = X11_XineramaQueryScreens(data->display, &screencount);
        if (xinerama) {
            use_xinerama = xinerama_major * 100 + xinerama_minor;
        }
    }
    if (!xinerama) {
        screencount = ScreenCount(data->display);
    }
#else
    screencount = ScreenCount(data->display);
#endif /* SDL_VIDEO_DRIVER_X11_XINERAMA */

#if SDL_VIDEO_DRIVER_X11_XRANDR
    /* require at least XRandR v1.2 */
    if (CheckXRandR(data->display, &xrandr_major, &xrandr_minor) &&
        (xrandr_major >= 2 || (xrandr_major == 1 && xrandr_minor >= 2))) {
        use_xrandr = xrandr_major * 100 + xrandr_minor;
    }
#endif /* SDL_VIDEO_DRIVER_X11_XRANDR */

#if SDL_VIDEO_DRIVER_X11_XVIDMODE
    if (CheckVidMode(data->display, &vm_major, &vm_minor)) {
        use_vidmode = vm_major * 100 + vm_minor;
    }
#endif /* SDL_VIDEO_DRIVER_X11_XVIDMODE */

    for (snum = 0; snum < screencount; ++snum) {
        XVisualInfo vinfo;
        SDL_VideoDisplay display;
        SDL_DisplayData *displaydata;
        SDL_DisplayMode mode;
        SDL_DisplayModeData *modedata;
        XPixmapFormatValues *pixmapFormats;
        char display_name[128];
        int i, n;

        /* Re-order screens to always put default screen first */
        if (snum == 0) {
            screen = DefaultScreen(data->display);
        } else if (snum == DefaultScreen(data->display)) {
            screen = 0;
        } else {
            screen = snum;
        }

#if SDL_VIDEO_DRIVER_X11_XINERAMA
        if (xinerama) {
            if (get_visualinfo(data->display, 0, &vinfo) < 0) {
                continue;
            }
        } else {
            if (get_visualinfo(data->display, screen, &vinfo) < 0) {
                continue;
            }
        }
#else
        if (get_visualinfo(data->display, screen, &vinfo) < 0) {
            continue;
        }
#endif

        displaydata = (SDL_DisplayData *) SDL_calloc(1, sizeof(*displaydata));
        if (!displaydata) {
            continue;
        }
        display_name[0] = '\0';

        mode.format = X11_GetPixelFormatFromVisualInfo(data->display, &vinfo);
        if (SDL_ISPIXELFORMAT_INDEXED(mode.format)) {
            /* We don't support palettized modes now */
            SDL_free(displaydata);
            continue;
        }
#if SDL_VIDEO_DRIVER_X11_XINERAMA
        if (xinerama) {
            mode.w = xinerama[screen].width;
            mode.h = xinerama[screen].height;
        } else {
            mode.w = DisplayWidth(data->display, screen);
            mode.h = DisplayHeight(data->display, screen);
        }
#else
        mode.w = DisplayWidth(data->display, screen);
        mode.h = DisplayHeight(data->display, screen);
#endif
        mode.refresh_rate = 0;

        modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
        if (!modedata) {
            SDL_free(displaydata);
            continue;
        }
        mode.driverdata = modedata;

#if SDL_VIDEO_DRIVER_X11_XINERAMA
        /* Most of SDL's calls to X11 are unwaware of Xinerama, and to X11 standard calls, when Xinerama is active,
         * there's only one screen available. So we force the screen number to zero and
         * let Xinerama specific code handle specific functionality using displaydata->xinerama_info
         */
        if (use_xinerama) {
            displaydata->screen = 0;
            displaydata->use_xinerama = use_xinerama;
            displaydata->xinerama_info = xinerama[screen];
            displaydata->xinerama_screen = screen;
        }
        else displaydata->screen = screen;
#else
        displaydata->screen = screen;
#endif
        displaydata->visual = vinfo.visual;
        displaydata->depth = vinfo.depth;

        displaydata->scanline_pad = SDL_BYTESPERPIXEL(mode.format) * 8;
        pixmapFormats = X11_XListPixmapFormats(data->display, &n);
        if (pixmapFormats) {
            for (i = 0; i < n; ++i) {
                if (pixmapFormats[i].depth == displaydata->depth) {
                    displaydata->scanline_pad = pixmapFormats[i].scanline_pad;
                    break;
                }
            }
            X11_XFree(pixmapFormats);
        }

#if SDL_VIDEO_DRIVER_X11_XINERAMA
        if (use_xinerama) {
            displaydata->x = xinerama[screen].x_org;
            displaydata->y = xinerama[screen].y_org;
        }
        else
#endif
        {
            displaydata->x = 0;
            displaydata->y = 0;
        }

#if SDL_VIDEO_DRIVER_X11_XRANDR
        if (use_xrandr) {
            res = X11_XRRGetScreenResources(data->display, RootWindow(data->display, displaydata->screen));
        }
        if (res) {
            XRROutputInfo *output_info;
            XRRCrtcInfo *crtc;
            int output;
            Atom EDID = X11_XInternAtom(data->display, "EDID", False);
            Atom *props;
            int nprop;
            unsigned long width_mm;
            unsigned long height_mm;
            int inches = 0;

            for (output = 0; output < res->noutput; output++) {
                output_info = X11_XRRGetOutputInfo(data->display, res, res->outputs[output]);
                if (!output_info || !output_info->crtc ||
                    output_info->connection == RR_Disconnected) {
                    X11_XRRFreeOutputInfo(output_info);
                    continue;
                }

                /* Is this the output that corresponds to the current screen?
                   We're checking the crtc position, but that may not be a valid test
                   in all cases.  Anybody want to give this some love?
                 */
                crtc = X11_XRRGetCrtcInfo(data->display, res, output_info->crtc);
                if (!crtc || crtc->x != displaydata->x || crtc->y != displaydata->y ||
                    crtc->width != mode.w || crtc->height != mode.h) {
                    X11_XRRFreeOutputInfo(output_info);
                    X11_XRRFreeCrtcInfo(crtc);
                    continue;
                }

                displaydata->use_xrandr = use_xrandr;
                displaydata->xrandr_output = res->outputs[output];
                SetXRandRModeInfo(data->display, res, output_info, crtc->mode, &mode);

                /* Get the name of this display */
                width_mm = output_info->mm_width;
                height_mm = output_info->mm_height;
                inches = (int)((sqrt(width_mm * width_mm +
                                     height_mm * height_mm) / 25.4f) + 0.5f);
                SDL_strlcpy(display_name, output_info->name, sizeof(display_name));

                /* See if we can get the EDID data for the real monitor name */
                props = X11_XRRListOutputProperties(data->display, res->outputs[output], &nprop);
                for (i = 0; i < nprop; ++i) {
                    unsigned char *prop;
                    int actual_format;
                    unsigned long nitems, bytes_after;
                    Atom actual_type;

                    if (props[i] == EDID) {
                        if (X11_XRRGetOutputProperty(data->display,
                                                 res->outputs[output], props[i],
                                                 0, 100, False, False,
                                                 AnyPropertyType,
                                                 &actual_type, &actual_format,
                                                 &nitems, &bytes_after, &prop) == Success ) {
                            MonitorInfo *info = decode_edid(prop);
                            if (info) {
    #ifdef X11MODES_DEBUG
                                printf("Found EDID data for %s\n", output_info->name);
                                dump_monitor_info(info);
    #endif
                                SDL_strlcpy(display_name, info->dsc_product_name, sizeof(display_name));
                                free(info);
                            }
                            X11_XFree(prop);
                        }
                        break;
                    }
                }
                if (props) {
                    X11_XFree(props);
                }

                if (*display_name && inches) {
                    size_t len = SDL_strlen(display_name);
                    SDL_snprintf(&display_name[len], sizeof(display_name)-len, " %d\"", inches);
                }
#ifdef X11MODES_DEBUG
                printf("Display name: %s\n", display_name);
#endif

                X11_XRRFreeOutputInfo(output_info);
                X11_XRRFreeCrtcInfo(crtc);
                break;
            }
#ifdef X11MODES_DEBUG
            if (output == res->noutput) {
                printf("Couldn't find XRandR CRTC at %d,%d\n", displaydata->x, displaydata->y);
            }
#endif
            X11_XRRFreeScreenResources(res);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XRANDR */

#if SDL_VIDEO_DRIVER_X11_XVIDMODE
        if (!displaydata->use_xrandr &&
#if SDL_VIDEO_DRIVER_X11_XINERAMA
            /* XVidMode only works on the screen at the origin */
            (!displaydata->use_xinerama ||
             (displaydata->x == 0 && displaydata->y == 0)) &&
#endif
            use_vidmode) {
            displaydata->use_vidmode = use_vidmode;
            if (displaydata->use_xinerama) {
                displaydata->vidmode_screen = 0;
            } else {
                displaydata->vidmode_screen = screen;
            }
            XF86VidModeGetModeInfo(data->display, displaydata->vidmode_screen, &modedata->vm_mode);
        }
#endif /* SDL_VIDEO_DRIVER_X11_XVIDMODE */

        SDL_zero(display);
        if (*display_name) {
            display.name = display_name;
        }
        display.desktop_mode = mode;
        display.current_mode = mode;
        display.driverdata = displaydata;
        SDL_AddVideoDisplay(&display);
    }

#if SDL_VIDEO_DRIVER_X11_XINERAMA
    if (xinerama) X11_XFree(xinerama);
#endif

    if (_this->num_displays == 0) {
        return SDL_SetError("No available displays");
    }
    return 0;
}

void
X11_GetDisplayModes(_THIS, SDL_VideoDisplay * sdl_display)
{
    Display *display = ((SDL_VideoData *) _this->driverdata)->display;
    SDL_DisplayData *data = (SDL_DisplayData *) sdl_display->driverdata;
#if SDL_VIDEO_DRIVER_X11_XVIDMODE
    int nmodes;
    XF86VidModeModeInfo ** modes;
#endif
    int screen_w;
    int screen_h;
    SDL_DisplayMode mode;

    /* Unfortunately X11 requires the window to be created with the correct
     * visual and depth ahead of time, but the SDL API allows you to create
     * a window before setting the fullscreen display mode.  This means that
     * we have to use the same format for all windows and all display modes.
     * (or support recreating the window with a new visual behind the scenes)
     */
    mode.format = sdl_display->current_mode.format;
    mode.driverdata = NULL;

    screen_w = DisplayWidth(display, data->screen);
    screen_h = DisplayHeight(display, data->screen);

#if SDL_VIDEO_DRIVER_X11_XINERAMA
    if (data->use_xinerama) {
        if (data->use_vidmode && !data->xinerama_info.x_org && !data->xinerama_info.y_org &&
           (screen_w > data->xinerama_info.width || screen_h > data->xinerama_info.height)) {
            SDL_DisplayModeData *modedata;
            /* Add the full (both screens combined) xinerama mode only on the display that starts at 0,0
             * if we're using vidmode.
             */
            mode.w = screen_w;
            mode.h = screen_h;
            mode.refresh_rate = 0;
            modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
            if (modedata) {
                *modedata = *(SDL_DisplayModeData *)sdl_display->desktop_mode.driverdata;
            }
            mode.driverdata = modedata;
            SDL_AddDisplayMode(sdl_display, &mode);
        }
        else if (!data->use_xrandr)
        {
            SDL_DisplayModeData *modedata;
            /* Add the current mode of each monitor otherwise if we can't get them from xrandr */
            mode.w = data->xinerama_info.width;
            mode.h = data->xinerama_info.height;
            mode.refresh_rate = 0;
            modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
            if (modedata) {
                *modedata = *(SDL_DisplayModeData *)sdl_display->desktop_mode.driverdata;
            }
            mode.driverdata = modedata;
            SDL_AddDisplayMode(sdl_display, &mode);
        }

    }
#endif /* SDL_VIDEO_DRIVER_X11_XINERAMA */

#if SDL_VIDEO_DRIVER_X11_XRANDR
    if (data->use_xrandr) {
        XRRScreenResources *res;

        res = X11_XRRGetScreenResources (display, RootWindow(display, data->screen));
        if (res) {
            SDL_DisplayModeData *modedata;
            XRROutputInfo *output_info;
            int i;

            output_info = X11_XRRGetOutputInfo(display, res, data->xrandr_output);
            if (output_info && output_info->connection != RR_Disconnected) {
                for (i = 0; i < output_info->nmode; ++i) {
                    modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
                    if (!modedata) {
                        continue;
                    }
                    mode.driverdata = modedata;

                    if (SetXRandRModeInfo(display, res, output_info, output_info->modes[i], &mode)) {
                        SDL_AddDisplayMode(sdl_display, &mode);
                    } else {
                        SDL_free(modedata);
                    }
                }
            }
            X11_XRRFreeOutputInfo(output_info);
            X11_XRRFreeScreenResources(res);
        }
        return;
    }
#endif /* SDL_VIDEO_DRIVER_X11_XRANDR */

#if SDL_VIDEO_DRIVER_X11_XVIDMODE
    if (data->use_vidmode &&
        X11_XF86VidModeGetAllModeLines(display, data->vidmode_screen, &nmodes, &modes)) {
        int i;
        SDL_DisplayModeData *modedata;

#ifdef X11MODES_DEBUG
        printf("VidMode modes: (unsorted)\n");
        for (i = 0; i < nmodes; ++i) {
            printf("Mode %d: %d x %d @ %d, flags: 0x%x\n", i,
                   modes[i]->hdisplay, modes[i]->vdisplay,
                   CalculateXVidModeRefreshRate(modes[i]), modes[i]->flags);
        }
#endif
        for (i = 0; i < nmodes; ++i) {
            modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
            if (!modedata) {
                continue;
            }
            mode.driverdata = modedata;

            if (SetXVidModeModeInfo(modes[i], &mode)) {
                SDL_AddDisplayMode(sdl_display, &mode);
            } else {
                SDL_free(modedata);
            }
        }
        X11_XFree(modes);
        return;
    }
#endif /* SDL_VIDEO_DRIVER_X11_XVIDMODE */

    if (!data->use_xrandr && !data->use_vidmode) {
        SDL_DisplayModeData *modedata;
        /* Add the desktop mode */
        mode = sdl_display->desktop_mode;
        modedata = (SDL_DisplayModeData *) SDL_calloc(1, sizeof(SDL_DisplayModeData));
        if (modedata) {
            *modedata = *(SDL_DisplayModeData *)sdl_display->desktop_mode.driverdata;
        }
        mode.driverdata = modedata;
        SDL_AddDisplayMode(sdl_display, &mode);
    }
}

int
X11_SetDisplayMode(_THIS, SDL_VideoDisplay * sdl_display, SDL_DisplayMode * mode)
{
    SDL_VideoData *viddata = (SDL_VideoData *) _this->driverdata;
    Display *display = viddata->display;
    SDL_DisplayData *data = (SDL_DisplayData *) sdl_display->driverdata;
    SDL_DisplayModeData *modedata = (SDL_DisplayModeData *)mode->driverdata;

    viddata->last_mode_change_deadline = SDL_GetTicks() + (PENDING_FOCUS_TIME * 2);

#if SDL_VIDEO_DRIVER_X11_XRANDR
    if (data->use_xrandr) {
        XRRScreenResources *res;
        XRROutputInfo *output_info;
        XRRCrtcInfo *crtc;
        Status status;

        res = X11_XRRGetScreenResources (display, RootWindow(display, data->screen));
        if (!res) {
            return SDL_SetError("Couldn't get XRandR screen resources");
        }

        output_info = X11_XRRGetOutputInfo(display, res, data->xrandr_output);
        if (!output_info || output_info->connection == RR_Disconnected) {
            X11_XRRFreeScreenResources(res);
            return SDL_SetError("Couldn't get XRandR output info");
        }

        crtc = X11_XRRGetCrtcInfo(display, res, output_info->crtc);
        if (!crtc) {
            X11_XRRFreeOutputInfo(output_info);
            X11_XRRFreeScreenResources(res);
            return SDL_SetError("Couldn't get XRandR crtc info");
        }

        status = X11_XRRSetCrtcConfig (display, res, output_info->crtc, CurrentTime,
          crtc->x, crtc->y, modedata->xrandr_mode, crtc->rotation,
          &data->xrandr_output, 1);

        X11_XRRFreeCrtcInfo(crtc);
        X11_XRRFreeOutputInfo(output_info);
        X11_XRRFreeScreenResources(res);

        if (status != Success) {
            return SDL_SetError("X11_XRRSetCrtcConfig failed");
        }
    }
#endif /* SDL_VIDEO_DRIVER_X11_XRANDR */

#if SDL_VIDEO_DRIVER_X11_XVIDMODE
    if (data->use_vidmode) {
        X11_XF86VidModeSwitchToMode(display, data->vidmode_screen, &modedata->vm_mode);
    }
#endif /* SDL_VIDEO_DRIVER_X11_XVIDMODE */

    return 0;
}

void
X11_QuitModes(_THIS)
{
}

int
X11_GetDisplayBounds(_THIS, SDL_VideoDisplay * sdl_display, SDL_Rect * rect)
{
    SDL_DisplayData *data = (SDL_DisplayData *) sdl_display->driverdata;

    rect->x = data->x;
    rect->y = data->y;
    rect->w = sdl_display->current_mode.w;
    rect->h = sdl_display->current_mode.h;

#if SDL_VIDEO_DRIVER_X11_XINERAMA
    /* Get the real current bounds of the display */
    if (data->use_xinerama) {
        Display *display = ((SDL_VideoData *) _this->driverdata)->display;
        int screencount;
        XineramaScreenInfo *xinerama = X11_XineramaQueryScreens(display, &screencount);
        if (xinerama) {
            rect->x = xinerama[data->xinerama_screen].x_org;
            rect->y = xinerama[data->xinerama_screen].y_org;
            X11_XFree(xinerama);
        }
    }
#endif /* SDL_VIDEO_DRIVER_X11_XINERAMA */
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */
