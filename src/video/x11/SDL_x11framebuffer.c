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
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_x11framebuffer.h"

#ifndef NO_SHARED_MEMORY

/* Shared memory error handler routine */
static int shm_error;
static int (*X_handler)(Display *, XErrorEvent *) = NULL;
static int shm_errhandler(Display *d, XErrorEvent *e)
{
    if (e->error_code == BadAccess) {
        shm_error = True;
        return 0;
    }
    return X_handler(d, e);
}

static SDL_bool have_mitshm(Display *dpy)
{
    /* Only use shared memory on local X servers */
    return X11_XShmQueryExtension(dpy) ? SDL_X11_HAVE_SHM : SDL_FALSE;
}

#endif /* !NO_SHARED_MEMORY */

int X11_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format,
                                void **pixels, int *pitch)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    Display *display = data->videodata->display;
    XGCValues gcv;
    XVisualInfo vinfo;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    /* Free the old framebuffer surface */
    X11_DestroyWindowFramebuffer(_this, window);

    /* Create the graphics context for drawing */
    gcv.graphics_exposures = False;
    data->gc = X11_XCreateGC(display, data->xwindow, GCGraphicsExposures, &gcv);
    if (!data->gc) {
        return SDL_SetError("Couldn't create graphics context");
    }

    /* Find out the pixel format and depth */
    if (X11_GetVisualInfoFromVisual(display, data->visual, &vinfo) < 0) {
        return SDL_SetError("Couldn't get window visual information");
    }

    *format = X11_GetPixelFormatFromVisualInfo(display, &vinfo);
    if (*format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_Log("X11 framebuffer: visual class=%d depth=%d red=0x%lx green=0x%lx blue=0x%lx",
            vinfo.class, vinfo.depth,
            vinfo.visual->red_mask, vinfo.visual->green_mask, vinfo.visual->blue_mask);
        return SDL_SetError("Unknown window pixel format");
    }

    /* StaticGray/GrayScale 8bpp (Kindle e-ink): the app renders in ARGB8888;
       we keep a private 8bpp Y8 buffer for the XImage and convert on every present. */
    if (*format == SDL_PIXELFORMAT_INDEX8 &&
        (vinfo.class == StaticGray || vinfo.class == GrayScale)) {
        data->grayscale_buf = (unsigned char *)SDL_malloc((size_t)w * h);
        if (!data->grayscale_buf) {
            return SDL_OutOfMemory();
        }
        SDL_memset(data->grayscale_buf, 0, (size_t)w * h);
        data->ximage = X11_XCreateImage(display, data->visual,
                                        vinfo.depth, ZPixmap, 0,
                                        (char *)data->grayscale_buf,
                                        w, h, 8, w);
        if (!data->ximage) {
            SDL_free(data->grayscale_buf);
            data->grayscale_buf = NULL;
            return SDL_SetError("Couldn't create grayscale XImage");
        }
        data->ximage->byte_order = (SDL_BYTEORDER == SDL_BIG_ENDIAN) ? MSBFirst : LSBFirst;
        *format = SDL_PIXELFORMAT_ARGB8888;
        *pitch  = w * 4;
        *pixels = SDL_malloc((size_t)h * (*pitch));
        if (!*pixels) {
            XDestroyImage(data->ximage); /* frees grayscale_buf via ximage->data */
            data->ximage = NULL;
            data->grayscale_buf = NULL;
            return SDL_OutOfMemory();
        }
        SDL_memset(*pixels, 0, (size_t)h * (*pitch));
        data->argb_buf = *pixels;
        return 0;
    }

    /* Calculate pitch */
    *pitch = (((w * SDL_BYTESPERPIXEL(*format)) + 3) & ~3);

    /* Create the actual image */
#ifndef NO_SHARED_MEMORY
    if (have_mitshm(display)) {
        XShmSegmentInfo *shminfo = &data->shminfo;

        shminfo->shmid = shmget(IPC_PRIVATE, (size_t)h * (*pitch), IPC_CREAT | 0777);
        if (shminfo->shmid >= 0) {
            shminfo->shmaddr = (char *)shmat(shminfo->shmid, 0, 0);
            shminfo->readOnly = False;
            if (shminfo->shmaddr != (char *)-1) {
                shm_error = False;
                X_handler = X11_XSetErrorHandler(shm_errhandler);
                X11_XShmAttach(display, shminfo);
                X11_XSync(display, False);
                X11_XSetErrorHandler(X_handler);
                if (shm_error) {
                    shmdt(shminfo->shmaddr);
                }
            } else {
                shm_error = True;
            }
            shmctl(shminfo->shmid, IPC_RMID, NULL);
        } else {
            shm_error = True;
        }
        if (!shm_error) {
            data->ximage = X11_XShmCreateImage(display, data->visual,
                                               vinfo.depth, ZPixmap,
                                               shminfo->shmaddr, shminfo,
                                               w, h);
            if (!data->ximage) {
                X11_XShmDetach(display, shminfo);
                X11_XSync(display, False);
                shmdt(shminfo->shmaddr);
            } else {
                /* Done! */
                data->ximage->byte_order = (SDL_BYTEORDER == SDL_BIG_ENDIAN) ? MSBFirst : LSBFirst;
                data->use_mitshm = SDL_TRUE;
                *pixels = shminfo->shmaddr;
                return 0;
            }
        }
    }
#endif /* not NO_SHARED_MEMORY */

    *pixels = SDL_malloc((size_t)h * (*pitch));
    if (!*pixels) {
        return SDL_OutOfMemory();
    }

    data->ximage = X11_XCreateImage(display, data->visual,
                                    vinfo.depth, ZPixmap, 0, (char *)(*pixels),
                                    w, h, 32, 0);
    if (!data->ximage) {
        SDL_free(*pixels);
        return SDL_SetError("Couldn't create XImage");
    }
    data->ximage->byte_order = (SDL_BYTEORDER == SDL_BIG_ENDIAN) ? MSBFirst : LSBFirst;
    return 0;
}

int X11_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects,
                                int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    Display *display = data->videodata->display;
    int i;
    int x, y, w, h;
    int window_w, window_h;

    SDL_GetWindowSizeInPixels(window, &window_w, &window_h);

    /* StaticGray path: convert dirty rects from ARGB8888 app surface to Y8 grayscale */
    if (data->grayscale_buf) {
        const Uint32 *src = (const Uint32 *)data->argb_buf;
        unsigned char *dst = data->grayscale_buf;
        int r, c;
        for (r = 0; r < window_h; ++r) {
            const Uint32 *row = src + r * window_w;
            unsigned char *grow = dst + r * window_w;
            for (c = 0; c < window_w; ++c) {
                Uint32 p = row[c];
                unsigned int R = (p >> 16) & 0xFF;
                unsigned int G = (p >>  8) & 0xFF;
                unsigned int B = (p      ) & 0xFF;
                /* BT.601 luma: Y = 0.299R + 0.587G + 0.114B (integer approx) */
                grow[c] = (unsigned char)((77 * R + 150 * G + 29 * B) >> 8);
            }
        }
    }

#ifndef NO_SHARED_MEMORY
    if (data->use_mitshm) {
        for (i = 0; i < numrects; ++i) {
            x = rects[i].x;
            y = rects[i].y;
            w = rects[i].w;
            h = rects[i].h;

            if (w <= 0 || h <= 0 || (x + w) <= 0 || (y + h) <= 0) {
                /* Clipped? */
                continue;
            }
            if (x < 0) {
                x += w;
                w += rects[i].x;
            }
            if (y < 0) {
                y += h;
                h += rects[i].y;
            }
            if (x + w > window_w) {
                w = window_w - x;
            }
            if (y + h > window_h) {
                h = window_h - y;
            }

            X11_XShmPutImage(display, data->xwindow, data->gc, data->ximage,
                             x, y, x, y, w, h, False);
        }
    } else
#endif /* !NO_SHARED_MEMORY */
    {
        for (i = 0; i < numrects; ++i) {
            x = rects[i].x;
            y = rects[i].y;
            w = rects[i].w;
            h = rects[i].h;

            if (w <= 0 || h <= 0 || (x + w) <= 0 || (y + h) <= 0) {
                /* Clipped? */
                continue;
            }
            if (x < 0) {
                x += w;
                w += rects[i].x;
            }
            if (y < 0) {
                y += h;
                h += rects[i].y;
            }
            if (x + w > window_w) {
                w = window_w - x;
            }
            if (y + h > window_h) {
                h = window_h - y;
            }

            X11_XPutImage(display, data->xwindow, data->gc, data->ximage,
                          x, y, x, y, w, h);
        }
    }

    X11_XSync(display, False);

    return 0;
}

void X11_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    Display *display;

    if (!data) {
        /* The window wasn't fully initialized */
        return;
    }

    display = data->videodata->display;

    if (data->ximage) {
        XDestroyImage(data->ximage); /* frees ximage->data (grayscale_buf or SHM addr) */

#ifndef NO_SHARED_MEMORY
        if (data->use_mitshm) {
            X11_XShmDetach(display, &data->shminfo);
            X11_XSync(display, False);
            shmdt(data->shminfo.shmaddr);
            data->use_mitshm = SDL_FALSE;
        }
#endif /* !NO_SHARED_MEMORY */

        data->ximage = NULL;
        data->grayscale_buf = NULL; /* freed by XDestroyImage above */
    }
    if (data->argb_buf) {
        SDL_free(data->argb_buf);
        data->argb_buf = NULL;
    }
    if (data->gc) {
        X11_XFreeGC(display, data->gc);
        data->gc = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */
