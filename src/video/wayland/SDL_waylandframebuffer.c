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

#include <time.h>

#include "../../core/unix/SDL_poll.h"
#include "SDL_waylandframebuffer.h"
#include "SDL_waylandshmbuffer.h"
#include "SDL_waylandwindow.h"

#include "commit-timing-v1-client-protocol.h"
#include "fifo-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"

#define BUFFER_COUNT 3

typedef struct Wayland_Buffer
{
    struct wl_buffer *wl_buffer;
    void *mem;
    bool busy;
} Wayland_Buffer;

struct Wayland_Framebuffer
{
    Wayland_Buffer buffer[BUFFER_COUNT];
    Wayland_SHMPool *pool;
    void *pixels;
    struct wp_presentation_feedback *feedback;
    Uint64 last_presentation_time;
    Uint32 refresh;
};

static void handle_sync_output(void *data, struct wp_presentation_feedback *wp_presentation_feedback, struct wl_output *output)
{
    // NOP
}

static void handle_presented(void *data, struct wp_presentation_feedback *wp_presentation_feedback,
                             uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh,
                             uint32_t seq_hi, uint32_t seq_lo, uint32_t flags)
{
    Wayland_Framebuffer *framebuffer = data;

    framebuffer->last_presentation_time = SDL_SECONDS_TO_NS(((Uint64)tv_sec_hi << 32) | (Uint64)tv_sec_lo) + tv_nsec;
    framebuffer->refresh = refresh;

    wp_presentation_feedback_destroy(framebuffer->feedback);
    framebuffer->feedback = NULL;
}

static void handle_discarded(void *data, struct wp_presentation_feedback *wp_presentation_feedback)
{
    SDL_VideoData *viddata = SDL_GetVideoDevice()->internal;
    Wayland_Framebuffer *framebuffer = data;

    struct timespec now;
    clock_gettime(viddata->presentation_clock_id, &now);
    framebuffer->last_presentation_time = SDL_SECONDS_TO_NS(now.tv_sec) + now.tv_nsec;

    wp_presentation_feedback_destroy(framebuffer->feedback);
    framebuffer->feedback = NULL;
}

static const struct wp_presentation_feedback_listener presentation_feedback_listener = {
    handle_sync_output,
    handle_presented,
    handle_discarded
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    Wayland_Buffer *buffer = (Wayland_Buffer *)data;
    buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release
};

static const struct wl_callback_listener swap_frame_listener;

static void swap_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    SDL_WindowData *wind = (SDL_WindowData *)data;
    SDL_SetAtomicInt(&wind->presentation.swap_interval_ready, 1); // Mark window as ready to present again.

    // Reset this callback to fire again once a new frame was presented and the compositor wants the next one.
    wind->presentation.frame_callback = wl_surface_frame(wind->presentation.surface_wrapper);
    wl_callback_destroy(cb);
    wl_callback_add_listener(wind->presentation.frame_callback, &swap_frame_listener, data);
}

static const struct wl_callback_listener swap_frame_listener = {
    swap_frame_done
};

static void CreatePresentationQueue(SDL_WindowData *wind)
{
    if (!wind->presentation.queue) {
        wind->presentation.queue = Wayland_CreateQueue(wind->waylandData->display, "SDL Surface Presentation Queue");
        wind->presentation.surface_wrapper = WAYLAND_wl_proxy_create_wrapper(wind->surface);
        WAYLAND_wl_proxy_set_queue((struct wl_proxy *)wind->presentation.surface_wrapper, wind->presentation.queue);
    }
}

void Wayland_WaitForSwapInterval(SDL_WindowData *wind)
{
    SDL_VideoData *videodata = wind->waylandData;

    if (!wind->presentation.frame_callback) {
        CreatePresentationQueue(wind);
        wind->presentation.frame_callback = wl_surface_frame(wind->presentation.surface_wrapper);
        wl_callback_add_listener(wind->presentation.frame_callback, &swap_frame_listener, wind);
    } else if (wind->shell_surface_status == WAYLAND_SHELL_SURFACE_STATUS_SHOWN) {
        // 20hz, so we'll progress even if throttled to zero.
        const Uint64 max_wait = SDL_GetTicksNS() + (SDL_NS_PER_SECOND / 20);
        while (SDL_GetAtomicInt(&wind->presentation.swap_interval_ready) == 0) {
            WAYLAND_wl_display_flush(videodata->display);

            /* wl_display_prepare_read_queue() will return false if the event queue is not empty.
             * If the event queue is empty, it will prepare us for our SDL_IOReady() call.
             */
            if (WAYLAND_wl_display_prepare_read_queue(videodata->display, wind->presentation.queue) != 0) {
                // We have some pending events. Check if the frame callback happened.
                WAYLAND_wl_display_dispatch_queue_pending(videodata->display, wind->presentation.queue);
                continue;
            }

            // Beyond this point, we must either call wl_display_cancel_read() or wl_display_read_events()
            const Uint64 now = SDL_GetTicksNS();
            if (now >= max_wait) {
                // Timeout expired. Cancel the read.
                WAYLAND_wl_display_cancel_read(videodata->display);
                break;
            }

            if (SDL_IOReady(WAYLAND_wl_display_get_fd(videodata->display), SDL_IOR_READ, max_wait - now) <= 0) {
                // Error or timeout expired without any events for us. Cancel the read.
                WAYLAND_wl_display_cancel_read(videodata->display);
                break;
            }

            // We have events. Read and dispatch them.
            WAYLAND_wl_display_read_events(videodata->display);
            WAYLAND_wl_display_dispatch_queue_pending(videodata->display, wind->presentation.queue);
        }
        SDL_SetAtomicInt(&wind->presentation.swap_interval_ready, 0);
    }
}

bool Wayland_SetWindowFramebufferVSync(SDL_VideoDevice *_this, SDL_Window *window, int vsync)
{
    if (vsync < 0) {
        return SDL_SetError("Unsupported vsync type");
    }

    SDL_VideoData *viddata = _this->internal;
    SDL_WindowData *wind = window->internal;

    wind->presentation.vsync = vsync;
    if (vsync > 1 && !viddata->wp_commit_timing_manager) {
        SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "The compositor lacks support for the commit-timing protocol; swap intervals above 1 are unsupported");
    }

    return true;
}

bool Wayland_GetWindowFramebufferVSync(SDL_VideoDevice *_this, SDL_Window *window, int *vsync)
{
    if (vsync) {
        SDL_WindowData *wind = window->internal;
        *vsync = wind->presentation.vsync;
    }
    return true;
}

bool Wayland_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_VideoData *viddata = _this->internal;
    SDL_WindowData *wind = window->internal;
    if (wind->framebuffer) {
        Wayland_DestroyWindowFramebuffer(_this, window);
    }

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    Wayland_Framebuffer *framebuffer = SDL_calloc(1, sizeof(Wayland_Framebuffer));
    if (!framebuffer) {
        return false;
    }

    framebuffer->pixels = SDL_malloc(w * h * 4);
    if (!framebuffer->pixels) {
        goto failed;
    }

    framebuffer->pool = Wayland_AllocSHMPool(w * h * 4 * BUFFER_COUNT);
    if (!framebuffer->pool) {
        goto failed;
    }

    CreatePresentationQueue(wind);

    // Presentation feedback and FIFO are required for proper vsync, with commit-timing required for swap intervals above 1.
    if (viddata->wp_fifo_manager && viddata->wp_presentation) {
        if (!wind->presentation.fifo) {
            wind->presentation.fifo = wp_fifo_manager_v1_get_fifo(viddata->wp_fifo_manager, wind->surface);
        }
        if (viddata->wp_commit_timing_manager && !wind->presentation.commit_timer) {
            wind->presentation.commit_timer = wp_commit_timing_manager_v1_get_timer(viddata->wp_commit_timing_manager, wind->surface);
        }
    }

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        framebuffer->buffer[i].wl_buffer = Wayland_AllocBufferFromPool(framebuffer->pool, w, h, &framebuffer->buffer[i].mem);

        // This isn't racy, as the only buffer event is a release, which is only sent after the buffer is attached.
        WAYLAND_wl_proxy_set_queue((struct wl_proxy *)framebuffer->buffer[i].wl_buffer, wind->presentation.queue);
        wl_buffer_add_listener(framebuffer->buffer[i].wl_buffer, &buffer_listener, &framebuffer->buffer[i]);
    }

    // ARGB8888 is guaranteed to be supported on all compositors.
    *format = SDL_PIXELFORMAT_ARGB8888;
    *pixels = framebuffer->pixels;
    *pitch = w * 4;

    wind->framebuffer = framebuffer;

    return true;

failed:
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (framebuffer->buffer[i].wl_buffer) {
            wl_buffer_destroy(framebuffer->buffer[i].wl_buffer);
        }
    }

    Wayland_ReleaseSHMPool(framebuffer->pool);
    SDL_free(framebuffer->pixels);
    SDL_free(framebuffer);

    return false;
}

bool Wayland_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_VideoData *viddata = _this->internal;
    SDL_WindowData *wind = window->internal;
    Wayland_Framebuffer *framebuffer = wind->framebuffer;

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    // Process any events already in the presentation queue.
    if (WAYLAND_wl_display_dispatch_queue_pending(viddata->display, wind->presentation.queue) < 0) {
        return false;
    }

    // Wait for feedback to get the next presentation time, and avoid getting too far ahead.
    if (wind->presentation.vsync) {
        while (framebuffer->feedback) {
            if (WAYLAND_wl_display_dispatch_queue(viddata->display, wind->presentation.queue) < 0) {
                return false;
            }
        }
    }

    // Find a free buffer.
    Wayland_Buffer *buffer = NULL;
    while (!buffer) {
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            if (!framebuffer->buffer[i].busy) {
                buffer = &framebuffer->buffer[i];
                break;
            }
        }

        if (buffer) {
            break;
        }

        // If there are no free buffers, wait for a buffer release event.
        if (WAYLAND_wl_display_dispatch_queue(viddata->display, wind->presentation.queue) < 0) {
            return false;
        }
    }

    // Copy the image data.
    SDL_memcpy(buffer->mem, framebuffer->pixels, w * h * 4);

    // Legacy synchronization that waits for the frame callback.
    if (wind->presentation.vsync && !wind->presentation.fifo) {
        Wayland_WaitForSwapInterval(wind);
    }

    // Attach the buffer and set the surface damage region.
    buffer->busy = true;

    wl_surface_attach(wind->surface, buffer->wl_buffer, 0, 0);
    if (wl_surface_get_version(wind->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(wind->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
    } else {
        wl_surface_damage(wind->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
    }

    // If vsync is active, and fifo and presentation feedback are supported, use them to ensure proper frame pacing.
    if (wind->presentation.vsync && wind->presentation.fifo) {
        // Queue presentation feedback for this frame.
        struct wp_presentation *presentation_wrapper = WAYLAND_wl_proxy_create_wrapper(viddata->wp_presentation);
        WAYLAND_wl_proxy_set_queue((struct wl_proxy *)presentation_wrapper, wind->presentation.queue);
        framebuffer->feedback = wp_presentation_feedback(presentation_wrapper, wind->surface);
        wp_presentation_feedback_add_listener(framebuffer->feedback, &presentation_feedback_listener, framebuffer);
        WAYLAND_wl_proxy_wrapper_destroy(presentation_wrapper);

        // Set a FIFO barrier to wait for the next swap interval.
        wp_fifo_v1_set_barrier(wind->presentation.fifo);

        if (wind->presentation.commit_timer && framebuffer->last_presentation_time) {
            const Uint32 interval = framebuffer->refresh * wind->presentation.vsync;

            // Account for the rounding of refresh intervals by compositors.
            if (interval >= SDL_MS_TO_NS(1)) {
                const Uint64 next = framebuffer->last_presentation_time + interval - SDL_MS_TO_NS(1);
                const Uint64 sec = SDL_NS_TO_SECONDS(next);
                const Uint32 ns = (Uint32)(next % SDL_NS_PER_SECOND);
                wp_commit_timer_v1_set_timestamp(wind->presentation.commit_timer, (Uint32)(sec >> 32), (Uint32)(sec & 0xFFFFFFFF), ns);
            }
        }

        /* This weird double-commit is necessary to ensure that presented/discarded
         * events are received under all conditions.
         *
         * Under normal conditions, where the frame is displayed, a presented event
         * will be received as normal. If the window is occluded, the second commit
         * will trigger a discarded event.
         *
         * It's ugly, but Mesa and Nvidia do the same thing...
         */
        wp_fifo_v1_wait_barrier(wind->presentation.fifo);
        wl_surface_commit(wind->surface);

        wp_fifo_v1_wait_barrier(wind->presentation.fifo);
    }

    wl_surface_commit(wind->surface);
    WAYLAND_wl_display_flush(viddata->display);

    return true;
}

void Wayland_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wind = window->internal;
    if (!wind || !wind->framebuffer) {
        return;
    }

    Wayland_Framebuffer *framebuffer = wind->framebuffer;
    wind->framebuffer = NULL;

    if (framebuffer->feedback) {
        wp_presentation_feedback_destroy(framebuffer->feedback);
    }

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        wl_buffer_destroy(framebuffer->buffer[i].wl_buffer);
    }

    Wayland_ReleaseSHMPool(framebuffer->pool);
    SDL_free(framebuffer->pixels);
    SDL_free(framebuffer);
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
