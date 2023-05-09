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

#if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(SDL_VIDEO_OPENGL_EGL)

#include "../../core/unix/SDL_poll.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_windowevents_c.h"
#include "SDL_waylandvideo.h"
#include "SDL_waylandopengles.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandevents_c.h"

#include "xdg-shell-client-protocol.h"

/* EGL implementation of SDL OpenGL ES support */

int Wayland_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    int ret;
    SDL_VideoData *data = _this->driverdata;

    ret = SDL_EGL_LoadLibrary(_this, path, (NativeDisplayType)data->display, _this->gl_config.egl_platform);

    Wayland_PumpEvents(_this);
    WAYLAND_wl_display_flush(data->display);

    return ret;
}

SDL_GLContext Wayland_GLES_CreateContext(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_GLContext context;
    context = SDL_EGL_CreateContext(_this, window->driverdata->egl_surface);
    WAYLAND_wl_display_flush(_this->driverdata->display);

    return context;
}

/* Wayland wants to tell you when to provide new frames, and if you have a non-zero
   swap interval, Mesa will block until a callback tells it to do so. On some
   compositors, they might decide that a minimized window _never_ gets a callback,
   which causes apps to hang during swapping forever. So we always set the official
   eglSwapInterval to zero to avoid blocking inside EGL, and manage this ourselves.
   If a swap blocks for too long waiting on a callback, we just go on, under the
   assumption the frame will be wasted, but this is better than freezing the app.
   I frown upon platforms that dictate this sort of control inversion (the callback
   is intended for _rendering_, not stalling until vsync), but we can work around
   this for now.  --ryan. */
/* Addendum: several recent APIs demand this sort of control inversion: Emscripten,
   libretro, Wayland, probably others...it feels like we're eventually going to have
   to give in with a future SDL API revision, since we can bend the other APIs to
   this style, but this style is much harder to bend the other way.  :/ */
int Wayland_GLES_SetSwapInterval(SDL_VideoDevice *_this, int interval)
{
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }

    /* technically, this is _all_ adaptive vsync (-1), because we can't
       actually wait for the _next_ vsync if you set 1, but things that
       request 1 probably won't care _that_ much. I hope. No matter what
       you do, though, you never see tearing on Wayland. */
    if (interval > 1) {
        interval = 1;
    } else if (interval < -1) {
        interval = -1;
    }

    /* !!! FIXME: technically, this should be per-context, right? */
    _this->egl_data->egl_swapinterval = interval;
    _this->egl_data->eglSwapInterval(_this->egl_data->egl_display, 0);
    return 0;
}

int Wayland_GLES_GetSwapInterval(SDL_VideoDevice *_this, int *interval)
{
    if (!_this->egl_data) {
        return SDL_SetError("EGL not initialized");
    }

    *interval =_this->egl_data->egl_swapinterval;
    return 0;
}

int Wayland_GLES_SwapWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *data = window->driverdata;
    const int swap_interval = _this->egl_data->egl_swapinterval;

    /* For windows that we know are hidden, skip swaps entirely, if we don't do
     * this compositors will intentionally stall us indefinitely and there's no
     * way for an end user to show the window, unlike other situations (i.e.
     * the window is minimized, behind another window, etc.).
     *
     * FIXME: Request EGL_WAYLAND_swap_buffers_with_timeout.
     * -flibit
     */
    if (data->surface_status != WAYLAND_SURFACE_STATUS_SHOWN &&
        data->surface_status != WAYLAND_SURFACE_STATUS_WAITING_FOR_FRAME) {
        return 0;
    }

    /* Control swap interval ourselves. See comments on Wayland_GLES_SetSwapInterval */
    if (swap_interval != 0 && data->surface_status == WAYLAND_SURFACE_STATUS_SHOWN) {
        SDL_VideoData *videodata = _this->driverdata;
        struct wl_display *display = videodata->display;
        /* 1 sec, so we'll progress even if throttled to zero. */
        const Uint64 max_wait = SDL_GetTicksNS() + SDL_NS_PER_SECOND;
        while (SDL_AtomicGet(&data->swap_interval_ready) == 0) {
            Uint64 now;

            WAYLAND_wl_display_flush(display);

            /* wl_display_prepare_read_queue() will return -1 if the event queue is not empty.
             * If the event queue is empty, it will prepare us for our SDL_IOReady() call. */
            if (WAYLAND_wl_display_prepare_read_queue(display, data->gles_swap_frame_event_queue) != 0) {
                /* We have some pending events. Check if the frame callback happened. */
                WAYLAND_wl_display_dispatch_queue_pending(display, data->gles_swap_frame_event_queue);
                continue;
            }

            /* Beyond this point, we must either call wl_display_cancel_read() or wl_display_read_events() */

            now = SDL_GetTicksNS();
            if (now >= max_wait) {
                /* Timeout expired. Cancel the read. */
                WAYLAND_wl_display_cancel_read(display);
                break;
            }

            if (SDL_IOReady(WAYLAND_wl_display_get_fd(display), SDL_IOR_READ, max_wait - now) <= 0) {
                /* Error or timeout expired without any events for us. Cancel the read. */
                WAYLAND_wl_display_cancel_read(display);
                break;
            }

            /* We have events. Read and dispatch them. */
            WAYLAND_wl_display_read_events(display);
            WAYLAND_wl_display_dispatch_queue_pending(display, data->gles_swap_frame_event_queue);
        }
        SDL_AtomicSet(&data->swap_interval_ready, 0);
    }

    /* Feed the frame to Wayland. This will set it so the wl_surface_frame callback can fire again. */
    if (!_this->egl_data->eglSwapBuffers(_this->egl_data->egl_display, data->egl_surface)) {
        return SDL_EGL_SetError("unable to show color buffer in an OS-native window", "eglSwapBuffers");
    }

    WAYLAND_wl_display_flush(data->waylandData->display);

    return 0;
}

int Wayland_GLES_MakeCurrent(SDL_VideoDevice *_this, SDL_Window *window, SDL_GLContext context)
{
    int ret;

    if (window && context) {
        ret = SDL_EGL_MakeCurrent(_this, window->driverdata->egl_surface, context);
    } else {
        ret = SDL_EGL_MakeCurrent(_this, NULL, NULL);
    }

    WAYLAND_wl_display_flush(_this->driverdata->display);

    _this->egl_data->eglSwapInterval(_this->egl_data->egl_display, 0); /* see comments on Wayland_GLES_SetSwapInterval. */

    return ret;
}

int Wayland_GLES_DeleteContext(SDL_VideoDevice *_this, SDL_GLContext context)
{
    SDL_EGL_DeleteContext(_this, context);
    WAYLAND_wl_display_flush(_this->driverdata->display);
    return 0;
}

EGLSurface Wayland_GLES_GetEGLSurface(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *windowdata = window->driverdata;

    return windowdata->egl_surface;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND && SDL_VIDEO_OPENGL_EGL */
