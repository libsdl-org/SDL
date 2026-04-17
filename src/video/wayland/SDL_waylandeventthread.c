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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "../../core/unix/SDL_poll.h"
#include "SDL_waylandeventthread.h"
#include "SDL_waylandvideo.h"

#include <errno.h>

struct Wayland_EventThreadContext
{
    SDL_VideoData *wayland_data;
    SDL_Thread *thread;
    struct wl_event_queue *queue;
    SDL_Mutex *dispatch_lock;
    bool should_exit;
};

static void handle_event_thread_exit(void *data, struct wl_callback *wl_callback, uint32_t callback_data)
{
    Wayland_EventThreadContext *context = (Wayland_EventThreadContext *)data;
    wl_callback_destroy(wl_callback);
    context->should_exit = true;
}

static const struct wl_callback_listener event_thread_exit_listener = {
    handle_event_thread_exit
};

static int SDLCALL Wayland_EventThreadFunc(void *data)
{
    Wayland_EventThreadContext *context = (Wayland_EventThreadContext *)data;
    struct wl_display *display = context->wayland_data->display;
    const int display_fd = WAYLAND_wl_display_get_fd(display);
    int ret;

    /* The lock must be held whenever dispatching to avoid a race condition when adding
     * or destroying objects using the queue.
     *
     * Any error other than EAGAIN is fatal and causes the thread to exit.
     */
    while (!context->should_exit) {
        if (WAYLAND_wl_display_prepare_read_queue(display, context->queue) == 0) {
            Sint64 timeoutNS = -1;

            ret = WAYLAND_wl_display_flush(display);

            if (ret < 0) {
                if (errno == EAGAIN) {
                    // If the flush failed with EAGAIN, don't block as not to inhibit other threads from reading events.
                    timeoutNS = SDL_MS_TO_NS(1);
                } else {
                    WAYLAND_wl_display_cancel_read(display);
                    return -1;
                }
            }

            // Wait for a read/write operation to become possible.
            ret = SDL_IOReady(display_fd, SDL_IOR_READ, timeoutNS);

            if (ret <= 0) {
                WAYLAND_wl_display_cancel_read(display);
                if (ret < 0) {
                    return -1;
                }

                // Nothing to read, and woke to flush; try again.
                continue;
            }

            ret = WAYLAND_wl_display_read_events(display);
            if (ret == -1) {
                return -1;
            }
        }

        SDL_LockMutex(context->dispatch_lock);
        ret = WAYLAND_wl_display_dispatch_queue_pending(display, context->queue);
        SDL_UnlockMutex(context->dispatch_lock);

        if (ret < 0) {
            return -1;
        }
    }

    return 0;
}

Wayland_EventThreadContext *Wayland_CreateEventThread(SDL_VideoData *data, const char *queue_name)
{
    Wayland_EventThreadContext *context = SDL_calloc(1, sizeof(Wayland_EventThreadContext));
    if (!context) {
        return NULL;
    }

    context->wayland_data = data;
    context->queue = Wayland_DisplayCreateQueue(data->display, queue_name);
    if (!context->queue) {
        goto cleanup;
    }

    context->dispatch_lock = SDL_CreateMutex();
    if (!context->dispatch_lock) {
        goto cleanup;
    }

    context->thread = SDL_CreateThread(Wayland_EventThreadFunc, "wl_event_thread", context);
    if (!context->thread) {
        goto cleanup;
    }

    return context;

cleanup:
    if (context->dispatch_lock) {
        SDL_DestroyMutex(context->dispatch_lock);
    }

    if (context->queue) {
        WAYLAND_wl_event_queue_destroy(context->queue);
    }

    SDL_free(context);
    return NULL;
}

void Wayland_DestroyEventThread(Wayland_EventThreadContext *context)
{
    if (context->thread) {
        // Dispatch the exit event to unblock the thread and signal it to exit.
        struct wl_display *display = context->wayland_data->display;
        struct wl_proxy *display_wrapper = Wayland_CreateEventThreadProxyWrapper(context, display);

        SDL_LockMutex(context->dispatch_lock);
        struct wl_callback *cb = wl_display_sync((struct wl_display *)display_wrapper);
        wl_callback_add_listener(cb, &event_thread_exit_listener, context);
        SDL_UnlockMutex(context->dispatch_lock);

        WAYLAND_wl_proxy_wrapper_destroy(display_wrapper);

        int ret = WAYLAND_wl_display_flush(display);
        while (ret == -1 && errno == EAGAIN) {
            // Shutting down the thread requires a successful flush.
            ret = SDL_IOReady(WAYLAND_wl_display_get_fd(display), SDL_IOR_WRITE, -1);
            if (ret >= 0) {
                ret = WAYLAND_wl_display_flush(display);
            }
        }

        // Avoid a warning if the flush failed due to a broken connection.
        if (ret < 0) {
            wl_callback_destroy(cb);
        }

        // Wait for the thread to return; it will exit automatically on a broken connection.
        SDL_WaitThread(context->thread, NULL);

        WAYLAND_wl_event_queue_destroy(context->queue);
        SDL_DestroyMutex(context->dispatch_lock);
        SDL_free(context);
    }
}

void *Wayland_CreateEventThreadProxyWrapper(Wayland_EventThreadContext *context, void *obj)
{
    struct wl_proxy *wrapper = WAYLAND_wl_proxy_create_wrapper(obj);
    if (wrapper) {
        WAYLAND_wl_proxy_set_queue(wrapper, context->queue);
        return wrapper;
    }

    return NULL;
}

void Wayland_LockEventThread(Wayland_EventThreadContext *context)
{
    if (context) {
        SDL_LockMutex(context->dispatch_lock);
    }
}

void Wayland_UnlockEventThread(Wayland_EventThreadContext *context)
{
    if (context) {
        SDL_UnlockMutex(context->dispatch_lock);
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
