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

#include "SDL_waylandframebuffer.h"
#include "SDL_waylandshmbuffer.h"
#include "SDL_waylandwindow.h"

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
    struct wl_event_queue *queue;
    void *pixels;
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    Wayland_Buffer *buffer = (Wayland_Buffer *)data;
    buffer->busy = false;
}

static struct wl_buffer_listener buffer_listener = {
    buffer_handle_release
};

bool Wayland_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_WindowData *wind = window->internal;
    if (wind->framebuffer) {
        Wayland_DestroyWindowFramebuffer(_this, window);
    }

    const int w = wind->current.pixel_width;
    const int h = wind->current.pixel_height;

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

    framebuffer->queue = Wayland_DisplayCreateQueue(_this->internal->display, "Wayland Framebuffer Queue");
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        framebuffer->buffer[i].wl_buffer = Wayland_AllocBufferFromPool(framebuffer->pool, w, h, &framebuffer->buffer[i].mem);
        WAYLAND_wl_proxy_set_queue((struct wl_proxy *)framebuffer->buffer[i].wl_buffer, framebuffer->queue);
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
    if (framebuffer->queue) {
        WAYLAND_wl_event_queue_destroy(framebuffer->queue);
    }
    SDL_free(framebuffer->pixels);
    SDL_free(framebuffer);

    return false;
}

bool Wayland_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_VideoData *viddata = _this->internal;
    SDL_WindowData *wind = window->internal;
    Wayland_Framebuffer *framebuffer = wind->framebuffer;

    // Process any queued buffer release events.
    WAYLAND_wl_display_dispatch_queue_pending(viddata->display, framebuffer->queue);

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

        /* If nothing is free, wait for a buffer release event.
         * Not all compositors flush on a release event, so use roundtrips, as they cause a flush.
         */
        if (WAYLAND_wl_display_roundtrip_queue(viddata->display, framebuffer->queue) < 0) {
            return false;
        }
    }

    // Copy the image data.
    SDL_memcpy(buffer->mem, framebuffer->pixels, wind->current.pixel_width * wind->current.pixel_height * 4);

    // Attach the buffer and commit the surface.
    buffer->busy = true;
    wl_surface_attach(wind->surface, buffer->wl_buffer, 0, 0);
    if (wl_surface_get_version(wind->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(wind->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
    } else {
        wl_surface_damage(wind->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
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

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        wl_buffer_destroy(framebuffer->buffer[i].wl_buffer);
    }

    Wayland_ReleaseSHMPool(framebuffer->pool);
    WAYLAND_wl_event_queue_destroy(framebuffer->queue);
    SDL_free(framebuffer->pixels);
    SDL_free(framebuffer);
}
