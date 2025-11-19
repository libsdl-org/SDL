/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#include <errno.h>

#include "../SDL_sysvideo.h"
#include "../SDL_video_c.h"

#include "../../core/unix/SDL_poll.h"
#include "../../events/SDL_mouse_c.h"
#include "SDL_waylandvideo.h"
#include "../SDL_pixels_c.h"
#include "SDL_waylandevents_c.h"

#include "wayland-cursor.h"
#include "SDL_waylandmouse.h"
#include "SDL_waylandshmbuffer.h"

#include "cursor-shape-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "pointer-warp-v1-client-protocol.h"
#include "tablet-v2-client-protocol.h"

#include "../../SDL_hints_c.h"

static SDL_Cursor *sys_cursors[SDL_HITTEST_RESIZE_LEFT + 1];

static bool Wayland_SetRelativeMouseMode(bool enabled);

typedef struct
{
    int width;
    int height;
    struct wl_buffer *buffer;
} CustomCursorImage;

typedef struct
{
    // The base dimensions of the cursor.
    int width;
    int height;
    int hot_x;
    int hot_y;

    int images_per_frame;
    CustomCursorImage images[];
} Wayland_CustomCursor;

typedef struct
{
    int size;
    struct wl_list node;
    struct wl_buffer *buffers[];
} Wayland_CachedSystemCursor;

typedef struct
{
    SDL_SystemCursor id;
    struct wl_list cursor_buffer_cache;
} Wayland_SystemCursor;

struct SDL_CursorData
{
    // Cursor animation data.
    Uint32 *frame_durations_ms;
    Uint32 total_duration_ms;
    int num_frames;
    bool is_system_cursor;

    union
    {
        Wayland_CustomCursor custom;
        Wayland_SystemCursor system;
    } cursor_data;
};

static int dbus_cursor_size;
static char *dbus_cursor_theme;

static void Wayland_FreeCursorThemes(SDL_VideoData *vdata)
{
    for (int i = 0; i < vdata->num_cursor_themes; i += 1) {
        WAYLAND_wl_cursor_theme_destroy(vdata->cursor_themes[i].theme);
    }
    vdata->num_cursor_themes = 0;
    SDL_free(vdata->cursor_themes);
    vdata->cursor_themes = NULL;
}

#ifdef SDL_USE_LIBDBUS

#include "../../core/linux/SDL_dbus.h"

#define CURSOR_NODE        "org.freedesktop.portal.Desktop"
#define CURSOR_PATH        "/org/freedesktop/portal/desktop"
#define CURSOR_INTERFACE   "org.freedesktop.portal.Settings"
#define CURSOR_NAMESPACE   "org.gnome.desktop.interface"
#define CURSOR_SIGNAL_NAME "SettingChanged"
#define CURSOR_SIZE_KEY    "cursor-size"
#define CURSOR_THEME_KEY   "cursor-theme"

static DBusMessage *Wayland_ReadDBusProperty(SDL_DBusContext *dbus, const char *key)
{
    static const char *iface = "org.gnome.desktop.interface";

    DBusMessage *reply = NULL;
    DBusMessage *msg = dbus->message_new_method_call(CURSOR_NODE,
                                                     CURSOR_PATH,
                                                     CURSOR_INTERFACE,
                                                     "Read"); // Method

    if (msg) {
        if (dbus->message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID)) {
            reply = dbus->connection_send_with_reply_and_block(dbus->session_conn, msg, DBUS_TIMEOUT_USE_DEFAULT, NULL);
        }
        dbus->message_unref(msg);
    }

    return reply;
}

static bool Wayland_ParseDBusReply(SDL_DBusContext *dbus, DBusMessage *reply, int type, void *value)
{
    DBusMessageIter iter[3];

    dbus->message_iter_init(reply, &iter[0]);
    if (dbus->message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT) {
        return false;
    }

    dbus->message_iter_recurse(&iter[0], &iter[1]);
    if (dbus->message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT) {
        return false;
    }

    dbus->message_iter_recurse(&iter[1], &iter[2]);
    if (dbus->message_iter_get_arg_type(&iter[2]) != type) {
        return false;
    }

    dbus->message_iter_get_basic(&iter[2], value);

    return true;
}

static DBusHandlerResult Wayland_DBusCursorMessageFilter(DBusConnection *conn, DBusMessage *msg, void *data)
{
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    SDL_VideoData *vdata = (SDL_VideoData *)data;

    if (dbus->message_is_signal(msg, CURSOR_INTERFACE, CURSOR_SIGNAL_NAME)) {
        DBusMessageIter signal_iter, variant_iter;
        const char *namespace, *key;

        dbus->message_iter_init(msg, &signal_iter);
        // Check if the parameters are what we expect
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING) {
            goto not_our_signal;
        }
        dbus->message_iter_get_basic(&signal_iter, &namespace);
        if (SDL_strcmp(CURSOR_NAMESPACE, namespace) != 0) {
            goto not_our_signal;
        }
        if (!dbus->message_iter_next(&signal_iter)) {
            goto not_our_signal;
        }
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING) {
            goto not_our_signal;
        }
        dbus->message_iter_get_basic(&signal_iter, &key);
        if (SDL_strcmp(CURSOR_SIZE_KEY, key) == 0) {
            int new_cursor_size;

            if (!dbus->message_iter_next(&signal_iter)) {
                goto not_our_signal;
            }
            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_VARIANT) {
                goto not_our_signal;
            }
            dbus->message_iter_recurse(&signal_iter, &variant_iter);
            if (dbus->message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_INT32) {
                goto not_our_signal;
            }
            dbus->message_iter_get_basic(&variant_iter, &new_cursor_size);

            if (dbus_cursor_size != new_cursor_size) {
                dbus_cursor_size = new_cursor_size;
                SDL_RedrawCursor(); // Force cursor update
            }
        } else if (SDL_strcmp(CURSOR_THEME_KEY, key) == 0) {
            const char *new_cursor_theme = NULL;

            if (!dbus->message_iter_next(&signal_iter)) {
                goto not_our_signal;
            }
            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_VARIANT) {
                goto not_our_signal;
            }
            dbus->message_iter_recurse(&signal_iter, &variant_iter);
            if (dbus->message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_STRING) {
                goto not_our_signal;
            }
            dbus->message_iter_get_basic(&variant_iter, &new_cursor_theme);

            if (!dbus_cursor_theme || !new_cursor_theme || SDL_strcmp(dbus_cursor_theme, new_cursor_theme) != 0) {
                SDL_free(dbus_cursor_theme);
                if (new_cursor_theme) {
                    dbus_cursor_theme = SDL_strdup(new_cursor_theme);
                } else {
                    dbus_cursor_theme = NULL;
                }

                // Purge the current cached themes and force a cursor refresh.
                Wayland_FreeCursorThemes(vdata);
                SDL_RedrawCursor();
            }
        } else {
            goto not_our_signal;
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

not_our_signal:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void Wayland_DBusInitCursorProperties(SDL_VideoData *vdata)
{
    DBusMessage *reply;
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    bool add_filter = false;

    if (!dbus) {
        return;
    }

    if ((reply = Wayland_ReadDBusProperty(dbus, CURSOR_SIZE_KEY))) {
        if (Wayland_ParseDBusReply(dbus, reply, DBUS_TYPE_INT32, &dbus_cursor_size)) {
            add_filter = true;
        }
        dbus->message_unref(reply);
    }

    if ((reply = Wayland_ReadDBusProperty(dbus, CURSOR_THEME_KEY))) {
        const char *temp = NULL;
        if (Wayland_ParseDBusReply(dbus, reply, DBUS_TYPE_STRING, &temp)) {
            add_filter = true;

            if (temp) {
                dbus_cursor_theme = SDL_strdup(temp);
            }
        }
        dbus->message_unref(reply);
    }

    // Only add the filter if at least one of the settings we want is present.
    if (add_filter) {
        dbus->bus_add_match(dbus->session_conn,
                            "type='signal', interface='" CURSOR_INTERFACE "',"
                            "member='" CURSOR_SIGNAL_NAME "', arg0='" CURSOR_NAMESPACE "'",
                            NULL);
        dbus->connection_add_filter(dbus->session_conn, &Wayland_DBusCursorMessageFilter, vdata, NULL);
        dbus->connection_flush(dbus->session_conn);
    }
}

static void Wayland_DBusFinishCursorProperties(void)
{
    SDL_free(dbus_cursor_theme);
    dbus_cursor_theme = NULL;
}

#endif

static struct wl_buffer *Wayland_CursorStateGetFrame(SDL_WaylandCursorState *state, int frame_index)
{
    SDL_CursorData *data = state->current_cursor;

    if (data) {
        if (!data->is_system_cursor) {
            const int offset = data->cursor_data.custom.images_per_frame * frame_index;

            /* Find the closest image. Images that are larger than the
             * desired size are preferred over images that are smaller.
             */
            CustomCursorImage *closest = NULL;
            const int target_area = SDL_lround(data->cursor_data.custom.width * data->cursor_data.custom.height * state->scale);
            int closest_area = 0;
            for (int i = 0; i < data->cursor_data.custom.images_per_frame && closest_area < target_area && data->cursor_data.custom.images[offset + i].buffer; ++i) {
                closest = &data->cursor_data.custom.images[offset + i];
                closest_area = closest->width * closest->height;
            }

            return closest ? closest->buffer : NULL;
        } else {
            return ((Wayland_CachedSystemCursor *)(state->system_cursor_handle))->buffers[frame_index];
        }
    }

    return NULL;
}

static struct CursorThreadContext
{
    SDL_Thread *thread;
    struct wl_event_queue *queue;
    struct wl_proxy *compositor_wrapper;
    SDL_Mutex *lock;
    bool should_exit;
} cursor_thread_context;

static void handle_cursor_thread_exit(void *data, struct wl_callback *wl_callback, uint32_t callback_data)
{
    wl_callback_destroy(wl_callback);
    cursor_thread_context.should_exit = true;
}

static const struct wl_callback_listener cursor_thread_exit_listener = {
    handle_cursor_thread_exit
};

static int SDLCALL Wayland_CursorThreadFunc(void *data)
{
    struct wl_display *display = data;
    const int display_fd = WAYLAND_wl_display_get_fd(display);
    int ret;

    /* The lock must be held whenever dispatching to avoid a race condition when setting
     * or destroying cursor frame callbacks, as adding the callback followed by setting
     * the listener is not an atomic operation, and the callback proxy must not be
     * destroyed while in the callback handler.
     *
     * Any error other than EAGAIN is fatal and causes the thread to exit.
     */
    while (!cursor_thread_context.should_exit) {
        if (WAYLAND_wl_display_prepare_read_queue(display, cursor_thread_context.queue) == 0) {
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

        SDL_LockMutex(cursor_thread_context.lock);
        ret = WAYLAND_wl_display_dispatch_queue_pending(display, cursor_thread_context.queue);
        SDL_UnlockMutex(cursor_thread_context.lock);

        if (ret < 0) {
            return -1;
        }
    }

    return 0;
}

static bool Wayland_StartCursorThread(SDL_VideoData *data)
{
    if (!cursor_thread_context.thread) {
        cursor_thread_context.queue = Wayland_DisplayCreateQueue(data->display, "SDL Cursor Surface Queue");
        if (!cursor_thread_context.queue) {
            goto cleanup;
        }

        cursor_thread_context.compositor_wrapper = WAYLAND_wl_proxy_create_wrapper(data->compositor);
        if (!cursor_thread_context.compositor_wrapper) {
            goto cleanup;
        }
        WAYLAND_wl_proxy_set_queue(cursor_thread_context.compositor_wrapper, cursor_thread_context.queue);

        cursor_thread_context.lock = SDL_CreateMutex();
        if (!cursor_thread_context.lock) {
            goto cleanup;
        }

        cursor_thread_context.thread = SDL_CreateThread(Wayland_CursorThreadFunc, "wl_cursor_surface", data->display);
        if (!cursor_thread_context.thread) {
            goto cleanup;
        }

        return true;
    }

cleanup:
    if (cursor_thread_context.lock) {
        SDL_DestroyMutex(cursor_thread_context.lock);
    }

    if (cursor_thread_context.compositor_wrapper) {
        WAYLAND_wl_proxy_wrapper_destroy(cursor_thread_context.compositor_wrapper);
    }

    if (cursor_thread_context.queue) {
        WAYLAND_wl_event_queue_destroy(cursor_thread_context.queue);
    }

    SDL_zero(cursor_thread_context);

    return false;
}

static void Wayland_DestroyCursorThread(SDL_VideoData *data)
{
    if (cursor_thread_context.thread) {
        // Dispatch the exit event to unblock the animation thread and signal it to exit.
        struct wl_proxy *display_wrapper = WAYLAND_wl_proxy_create_wrapper(data->display);
        WAYLAND_wl_proxy_set_queue(display_wrapper, cursor_thread_context.queue);

        SDL_LockMutex(cursor_thread_context.lock);
        struct wl_callback *cb = wl_display_sync((struct wl_display *)display_wrapper);
        wl_callback_add_listener(cb, &cursor_thread_exit_listener, NULL);
        SDL_UnlockMutex(cursor_thread_context.lock);

        WAYLAND_wl_proxy_wrapper_destroy(display_wrapper);

        int ret = WAYLAND_wl_display_flush(data->display);
        while (ret == -1 && errno == EAGAIN) {
            // Shutting down the thread requires a successful flush.
            ret = SDL_IOReady(WAYLAND_wl_display_get_fd(data->display), SDL_IOR_WRITE, -1);
            if (ret >= 0) {
                ret = WAYLAND_wl_display_flush(data->display);
            }
        }

        // Avoid a warning if the flush failed due to a broken connection.
        if (ret < 0) {
            wl_callback_destroy(cb);
        }

        // Wait for the thread to return; it will exit automatically on a broken connection.
        SDL_WaitThread(cursor_thread_context.thread, NULL);

        WAYLAND_wl_proxy_wrapper_destroy(cursor_thread_context.compositor_wrapper);
        WAYLAND_wl_event_queue_destroy(cursor_thread_context.queue);
        SDL_DestroyMutex(cursor_thread_context.lock);
        SDL_zero(cursor_thread_context);
    }
}

static void cursor_frame_done(void *data, struct wl_callback *cb, uint32_t time);
static const struct wl_callback_listener cursor_frame_listener = {
    cursor_frame_done
};

static void cursor_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
    SDL_WaylandCursorState *state = (SDL_WaylandCursorState *)data;
    if (!state->current_cursor) {
        return;
    }

    Uint32 *frames = state->current_cursor->frame_durations_ms;
    SDL_CursorData *c = state->current_cursor;

    const Uint64 now = SDL_GetTicks();
    const Uint32 elapsed = (now - state->last_frame_callback_time_ms) % c->total_duration_ms;
    Uint32 advance = 0;
    int next = state->current_frame;

    state->current_frame_time_ms += elapsed;

    // Calculate the next frame based on the elapsed duration.
    for (Uint32 t = frames[next]; t <= state->current_frame_time_ms; t += frames[next]) {
        next = (next + 1) % c->num_frames;
        advance = t;

        // Make sure we don't end up in an infinite loop if a cursor has frame durations of 0.
        if (!frames[next]) {
            break;
        }
    }

    wl_callback_destroy(cb);
    state->frame_callback = NULL;

    // Don't queue another callback if this frame time is infinite.
    if (frames[next]) {
        state->frame_callback = wl_surface_frame(state->surface);
        wl_callback_add_listener(state->frame_callback, &cursor_frame_listener, data);
    }

    state->current_frame_time_ms -= advance;
    state->last_frame_callback_time_ms = now;
    state->current_frame = next;

    struct wl_buffer *buffer = Wayland_CursorStateGetFrame(state, next);
    wl_surface_attach(state->surface, buffer, 0, 0);

    if (wl_surface_get_version(state->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
        wl_surface_damage_buffer(state->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
    } else {
        wl_surface_damage(state->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
    }
    wl_surface_commit(state->surface);
}

void Wayland_CursorStateSetFrameCallback(SDL_WaylandCursorState *state, void *userdata)
{
    if (cursor_thread_context.lock) {
        SDL_LockMutex(cursor_thread_context.lock);
    }

    state->frame_callback = wl_surface_frame(state->surface);
    wl_callback_add_listener(state->frame_callback, &cursor_frame_listener, userdata);

    if (cursor_thread_context.lock) {
        SDL_UnlockMutex(cursor_thread_context.lock);
    }
}

void Wayland_CursorStateDestroyFrameCallback(SDL_WaylandCursorState *state)
{
    if (cursor_thread_context.lock) {
        SDL_LockMutex(cursor_thread_context.lock);
    }

    if (state->frame_callback) {
        wl_callback_destroy(state->frame_callback);
        state->frame_callback = NULL;
    }

    if (cursor_thread_context.lock) {
        SDL_UnlockMutex(cursor_thread_context.lock);
    }
}

static void Wayland_CursorStateResetAnimation(SDL_WaylandCursorState *state, bool lock)
{
    if (lock && cursor_thread_context.lock) {
        SDL_LockMutex(cursor_thread_context.lock);
    }

    state->last_frame_callback_time_ms = SDL_GetTicks();
    state->current_frame_time_ms = 0;
    state->current_frame = 0;

    if (lock && cursor_thread_context.lock) {
        SDL_UnlockMutex(cursor_thread_context.lock);
    }
}

void Wayland_CursorStateRelease(SDL_WaylandCursorState *state)
{
    Wayland_CursorStateDestroyFrameCallback(state);
    if (state->cursor_shape) {
        wp_cursor_shape_device_v1_destroy(state->cursor_shape);
    }
    if (state->viewport) {
        wp_viewport_destroy(state->viewport);
    }
    if (state->surface) {
        wl_surface_attach(state->surface, NULL, 0, 0);
        wl_surface_commit(state->surface);
        wl_surface_destroy(state->surface);
    }

    SDL_zerop(state);
}

static Wayland_CachedSystemCursor *Wayland_CacheSystemCursor(SDL_CursorData *cdata, struct wl_cursor *cursor, int size)
{
    Wayland_CachedSystemCursor *cache = NULL;

    // Is this cursor already cached at the target scale?
    if (!WAYLAND_wl_list_empty(&cdata->cursor_data.system.cursor_buffer_cache)) {
        Wayland_CachedSystemCursor *c = NULL;
        wl_list_for_each (c, &cdata->cursor_data.system.cursor_buffer_cache, node) {
            if (c->size == size) {
                cache = c;
                break;
            }
        }
    }

    if (!cache) {
        cache = SDL_calloc(1, sizeof(Wayland_CachedSystemCursor) + (sizeof(struct wl_buffer *) * cdata->num_frames));

        cache->size = size;
        for (int i = 0; i < cdata->num_frames; ++i) {
            cache->buffers[i] = WAYLAND_wl_cursor_image_get_buffer(cursor->images[i]);
        }

        WAYLAND_wl_list_insert(&cdata->cursor_data.system.cursor_buffer_cache, &cache->node);
    }

    return cache;
}

static bool Wayland_GetSystemCursor(SDL_CursorData *cdata, SDL_WaylandCursorState *state, int *dst_size, int *hot_x, int *hot_y)
{
    SDL_VideoData *vdata = SDL_GetVideoDevice()->internal;
    struct wl_cursor_theme *theme = NULL;
    const char *css_name = "default";
    const char *fallback_name = NULL;
    double scale_factor = state->scale;
    int theme_size = dbus_cursor_size;

    // Fallback envvar if the DBus properties don't exist
    if (theme_size <= 0) {
        const char *xcursor_size = SDL_getenv("XCURSOR_SIZE");
        if (xcursor_size) {
            theme_size = SDL_atoi(xcursor_size);
        }
    }
    if (theme_size <= 0) {
        theme_size = 24;
    }

    // First, find the appropriate theme based on the current scale...
    const int scaled_size = (int)SDL_lround(theme_size * scale_factor);
    for (int i = 0; i < vdata->num_cursor_themes; ++i) {
        if (vdata->cursor_themes[i].size == scaled_size) {
            theme = vdata->cursor_themes[i].theme;
            break;
        }
    }
    if (!theme) {
        const char *xcursor_theme = dbus_cursor_theme;

        SDL_WaylandCursorTheme *new_cursor_themes = SDL_realloc(vdata->cursor_themes,
                                                                sizeof(SDL_WaylandCursorTheme) * (vdata->num_cursor_themes + 1));
        if (!new_cursor_themes) {
            return false;
        }
        vdata->cursor_themes = new_cursor_themes;

        // Fallback envvar if the DBus properties don't exist
        if (!xcursor_theme) {
            xcursor_theme = SDL_getenv("XCURSOR_THEME");
        }

        theme = WAYLAND_wl_cursor_theme_load(xcursor_theme, scaled_size, vdata->shm);
        vdata->cursor_themes[vdata->num_cursor_themes].size = scaled_size;
        vdata->cursor_themes[vdata->num_cursor_themes++].theme = theme;
    }

    css_name = SDL_GetCSSCursorName(cdata->cursor_data.system.id, &fallback_name);
    struct wl_cursor *cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, css_name);
    if (!cursor && fallback_name) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, fallback_name);
    }

    // Fallback to the default cursor if the chosen one wasn't found
    if (!cursor) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "default");
    }
    // Try the old X11 name as a last resort
    if (!cursor) {
        cursor = WAYLAND_wl_cursor_theme_get_cursor(theme, "left_ptr");
    }
    if (!cursor) {
        return false;
    }

    // ... Set the cursor data, finally.
    cdata->num_frames = cursor->image_count;
    Wayland_CachedSystemCursor *c = Wayland_CacheSystemCursor(cdata, cursor, theme_size);
    state->system_cursor_handle = c;

    if (cursor->image_count > 1 && !cdata->frame_durations_ms) {
        cdata->total_duration_ms = 0;
        cdata->frame_durations_ms = SDL_calloc(cursor->image_count, sizeof(Uint32));

        for (int i = 0; i < cursor->image_count; ++i) {
            cdata->frame_durations_ms[i] = cursor->images[i]->delay;
            cdata->total_duration_ms += cursor->images[i]->delay;
        }
    }

    *dst_size = SDL_lround(cursor->images[0]->width / state->scale);
    *hot_x = SDL_lround(cursor->images[0]->hotspot_x / state->scale);
    *hot_y = SDL_lround(cursor->images[0]->hotspot_y / state->scale);

    return true;
}

static int surface_sort_callback(const void *a, const void *b)
{
    SDL_Surface *s1 = (SDL_Surface *)a;
    SDL_Surface *s2 = (SDL_Surface *)b;

    return (s1->w * s1->h) <= (s2->w * s2->h) ? -1 : 1;
}

static SDL_Cursor *Wayland_CreateAnimatedCursor(SDL_CursorFrameInfo *frames, int frame_count, int hot_x, int hot_y)
{
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));
    if (!cursor) {
        return NULL;
    }

    SDL_CursorData *data = NULL;
    Wayland_SHMPool *shm_pool = NULL;
    int pool_size = 0;
    int max_images = 0;
    bool is_stack = false;
    struct SurfaceArray
    {
        SDL_Surface **surfaces;
        int count;
    } *surfaces = SDL_small_alloc(struct SurfaceArray, frame_count, &is_stack);
    if (!surfaces) {
        goto failed;
    }
    SDL_memset(surfaces, 0, sizeof(struct SurfaceArray) * frame_count);

    // Calculate the total allocation size.
    for (int i = 0; i < frame_count; ++i) {
        surfaces[i].surfaces = SDL_GetSurfaceImages(frames[i].surface, &surfaces[i].count);
        if (!surfaces[i].surfaces) {
            goto failed;
        }

        SDL_qsort(surfaces[i].surfaces, surfaces[i].count, sizeof(SDL_Surface *), surface_sort_callback);
        max_images = SDL_max(max_images, surfaces[i].count);
        for (int j = 0; j < surfaces[i].count; ++j) {
            pool_size += surfaces[i].surfaces[j]->w * surfaces[i].surfaces[j]->h * 4;
        }
    }

    data = SDL_calloc(1, sizeof(*data) + (sizeof(CustomCursorImage) * max_images * frame_count));
    if (!data) {
        goto failed;
    }

    data->frame_durations_ms = SDL_calloc(frame_count, sizeof(Uint32));
    if (!data->frame_durations_ms) {
        goto failed;
    }

    shm_pool = Wayland_AllocSHMPool(pool_size);
    if (!shm_pool) {
        goto failed;
    }

    cursor->internal = data;
    data->cursor_data.custom.width = frames[0].surface->w;
    data->cursor_data.custom.height = frames[0].surface->h;
    data->cursor_data.custom.hot_x = hot_x;
    data->cursor_data.custom.hot_y = hot_y;
    data->cursor_data.custom.images_per_frame = max_images;
    data->num_frames = frame_count;

    for (int i = 0; i < frame_count; ++i) {
        data->frame_durations_ms[i] = frames[i].duration;
        if (data->total_duration_ms < SDL_MAX_UINT32) {
            if (data->frame_durations_ms[i] > 0) {
                data->total_duration_ms += data->frame_durations_ms[i];
            } else {
                data->total_duration_ms = SDL_MAX_UINT32;
            }
        }

        const int offset = i * max_images;
        for (int j = 0; j < surfaces[i].count; ++j) {
            SDL_Surface *surface = surfaces[i].surfaces[j];

            // Convert the surface format, if required.
            if (surface->format != SDL_PIXELFORMAT_ARGB8888) {
                surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB8888);
                if (!surface) {
                    goto failed;
                }
            }

            data->cursor_data.custom.images[offset + j].width = surface->w;
            data->cursor_data.custom.images[offset + j].height = surface->h;

            void *buf_data;
            data->cursor_data.custom.images[offset + j].buffer = Wayland_AllocBufferFromPool(shm_pool, surface->w, surface->h, &buf_data);
            // Wayland requires premultiplied alpha for its surfaces.
            SDL_PremultiplyAlpha(surface->w, surface->h,
                                 surface->format, surface->pixels, surface->pitch,
                                 SDL_PIXELFORMAT_ARGB8888, buf_data, surface->w * 4, true);

            if (surface != surfaces[i].surfaces[j]) {
                SDL_DestroySurface(surface);
            }
        }

        // Free the memory returned by SDL_GetSurfaceImages().
        SDL_free(surfaces[i].surfaces);
    }

    SDL_small_free(surfaces, is_stack);
    Wayland_ReleaseSHMPool(shm_pool);

    return cursor;

failed:
    Wayland_ReleaseSHMPool(shm_pool);
    if (data) {
        SDL_free(data->frame_durations_ms);
        for (int i = 0; i < data->cursor_data.custom.images_per_frame * frame_count; ++i) {
            if (data->cursor_data.custom.images[i].buffer) {
                wl_buffer_destroy(data->cursor_data.custom.images[i].buffer);
            }
        }
    }

    SDL_free(data);

    if (surfaces) {
        for (int i = 0; i < frame_count; ++i) {
            SDL_free(surfaces[i].surfaces);
        }
        SDL_small_free(surfaces, is_stack);
    }

    SDL_free(cursor);

    return NULL;
}

static SDL_Cursor *Wayland_CreateCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_CursorFrameInfo frame = {
        surface, 0
    };

    return Wayland_CreateAnimatedCursor(&frame, 1, hot_x, hot_y);
}

static SDL_Cursor *Wayland_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor *cursor = SDL_calloc(1, sizeof(*cursor));

    if (cursor) {
        SDL_CursorData *cdata = SDL_calloc(1, sizeof(*cdata));
        if (!cdata) {
            SDL_free(cursor);
            return NULL;
        }
        cursor->internal = cdata;

        WAYLAND_wl_list_init(&cdata->cursor_data.system.cursor_buffer_cache);
        cdata->cursor_data.system.id = id;
        cdata->is_system_cursor = true;
    }

    return cursor;
}

static SDL_Cursor *Wayland_CreateDefaultCursor(void)
{
    SDL_SystemCursor id = SDL_GetDefaultSystemCursor();
    return Wayland_CreateSystemCursor(id);
}

static void Wayland_FreeCursorData(SDL_CursorData *d)
{
    SDL_VideoDevice *video_device = SDL_GetVideoDevice();
    SDL_VideoData *video_data = video_device->internal;
    SDL_WaylandSeat *seat;

    // Stop any frame callbacks and detach buffers associated with the cursor being destroyed.
    wl_list_for_each (seat, &video_data->seat_list, link)
    {
        if (seat->pointer.current_cursor == d) {
            Wayland_CursorStateDestroyFrameCallback(&seat->pointer.cursor_state);

            if (seat->pointer.cursor_state.surface) {
                wl_surface_attach(seat->pointer.cursor_state.surface, NULL, 0, 0);
            }

            seat->pointer.current_cursor = NULL;
        }
    }

    if (d->is_system_cursor) {
        Wayland_CachedSystemCursor *c, *temp;
        wl_list_for_each_safe(c, temp, &d->cursor_data.system.cursor_buffer_cache, node) {
            SDL_free(c);
        }
    } else {
        for (int i = 0; i < d->num_frames * d->cursor_data.custom.images_per_frame; ++i) {
            if (d->cursor_data.custom.images[i].buffer) {
                wl_buffer_destroy(d->cursor_data.custom.images[i].buffer);
            }
        }
    }

    SDL_free(d->frame_durations_ms);
}

static void Wayland_FreeCursor(SDL_Cursor *cursor)
{
    if (!cursor) {
        return;
    }

    // Probably not a cursor we own
    if (!cursor->internal) {
        return;
    }

    Wayland_FreeCursorData(cursor->internal);

    SDL_free(cursor->internal);
    SDL_free(cursor);
}

static enum wp_cursor_shape_device_v1_shape Wayland_GetSystemCursorShape(SDL_SystemCursor id)
{
    Uint32 shape;

    switch (id) {
    case SDL_SYSTEM_CURSOR_DEFAULT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
        break;
    case SDL_SYSTEM_CURSOR_TEXT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
        break;
    case SDL_SYSTEM_CURSOR_WAIT:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
        break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR;
        break;
    case SDL_SYSTEM_CURSOR_PROGRESS:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS;
        break;
    case SDL_SYSTEM_CURSOR_NWSE_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_NESW_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_EW_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_NS_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_MOVE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL;
        break;
    case SDL_SYSTEM_CURSOR_NOT_ALLOWED:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
        break;
    case SDL_SYSTEM_CURSOR_POINTER:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
        break;
    case SDL_SYSTEM_CURSOR_NW_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_N_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_NE_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_E_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SE_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_S_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_SW_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE;
        break;
    case SDL_SYSTEM_CURSOR_W_RESIZE:
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE;
        break;
    default:
        SDL_assert(0); // Should never be here...
        shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    }

    return shape;
}

typedef struct Wayland_PointerObject
{
    union
    {
        struct wl_pointer *wl_pointer;
        struct zwp_tablet_tool_v2 *wl_tool;
    };

    bool is_pointer;
} Wayland_PointerObject;

static void Wayland_CursorStateSetCursor(SDL_WaylandCursorState *state, const Wayland_PointerObject *obj, SDL_WindowData *focus, Uint32 serial, SDL_Cursor *cursor)
{
    SDL_VideoData *viddata = SDL_GetVideoDevice()->internal;
    SDL_CursorData *cursor_data = cursor ? cursor->internal : NULL;
    int dst_width = 0;
    int dst_height = 0;
    int hot_x;
    int hot_y;

    // Stop the frame callback for old animated cursors.
    if (cursor_data != state->current_cursor) {
        Wayland_CursorStateDestroyFrameCallback(state);
    }

    if (cursor) {
        if (cursor_data == state->current_cursor) {
            // Restart the animation sequence if the cursor didn't change.
            if (cursor_data->num_frames > 1) {
                Wayland_CursorStateResetAnimation(state, true);
            }

            return;
        }

        if (cursor_data->is_system_cursor) {
            // If the cursor shape protocol is supported, the compositor will draw nicely scaled cursors for us, so nothing more to do.
            if (state->cursor_shape) {
                // Don't need the surface or viewport if using the cursor shape protocol.
                if (state->surface) {
                    wl_surface_attach(state->surface, NULL, 0, 0);
                    wl_surface_commit(state->surface);

                    if (obj->is_pointer) {
                        wl_pointer_set_cursor(obj->wl_pointer, serial, NULL, 0, 0);
                    } else {
                        zwp_tablet_tool_v2_set_cursor(obj->wl_tool, serial, NULL, 0, 0);
                    }

                    if (state->viewport) {
                        wp_viewport_destroy(state->viewport);
                        state->viewport = NULL;
                    }

                    wl_surface_destroy(state->surface);
                    state->surface = NULL;
                }

                const enum wp_cursor_shape_device_v1_shape shape = Wayland_GetSystemCursorShape(cursor_data->cursor_data.system.id);
                wp_cursor_shape_device_v1_set_shape(state->cursor_shape, serial, shape);
                state->current_cursor = cursor_data;

                return;
            }

            // If viewports aren't available, the scale is always 1.0.
            state->scale = viddata->viewporter && focus ? focus->scale_factor : 1.0;
            if (!Wayland_GetSystemCursor(cursor_data, state, &dst_width, &hot_x, &hot_y)) {
                return;
            }

            dst_height = dst_width;
        } else {
            // If viewports aren't available, the scale is always 1.0.
            state->scale = viddata->viewporter && focus ? focus->scale_factor : 1.0;
            dst_width = cursor_data->cursor_data.custom.width;
            dst_height = cursor_data->cursor_data.custom.height;
            hot_x = cursor_data->cursor_data.custom.hot_x;
            hot_y = cursor_data->cursor_data.custom.hot_y;
        }

        state->current_cursor = cursor_data;

        if (!state->surface) {
            if (cursor_thread_context.compositor_wrapper) {
                state->surface = wl_compositor_create_surface((struct wl_compositor *)cursor_thread_context.compositor_wrapper);
            } else {
                state->surface = wl_compositor_create_surface(viddata->compositor);
            }
        }

        struct wl_buffer *buffer = Wayland_CursorStateGetFrame(state, 0);
        wl_surface_attach(state->surface, buffer, 0, 0);

        if (state->scale != 1.0) {
            if (!state->viewport) {
                state->viewport = wp_viewporter_get_viewport(viddata->viewporter, state->surface);
            }

            wp_viewport_set_source(state->viewport, wl_fixed_from_int(-1), wl_fixed_from_int(-1), wl_fixed_from_int(-1), wl_fixed_from_int(-1));
            wp_viewport_set_destination(state->viewport, dst_width, dst_height);
        } else if (state->viewport) {
            wp_viewport_destroy(state->viewport);
            state->viewport = NULL;
        }

        if (obj->is_pointer) {
            wl_pointer_set_cursor(obj->wl_pointer, serial, state->surface, hot_x, hot_y);
        } else {
            zwp_tablet_tool_v2_set_cursor(obj->wl_tool, serial, state->surface, hot_x, hot_y);
        }

        if (wl_surface_get_version(state->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
            wl_surface_damage_buffer(state->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
        } else {
            wl_surface_damage(state->surface, 0, 0, SDL_MAX_SINT32, SDL_MAX_SINT32);
        }

        // If more than one frame is available, create a frame callback to run the animation.
        if (cursor_data->num_frames > 1) {
            Wayland_CursorStateResetAnimation(state, false);
            Wayland_CursorStateSetFrameCallback(state, state);
        }

        wl_surface_commit(state->surface);
    } else {
        Wayland_CursorStateDestroyFrameCallback(state);
        state->current_cursor = NULL;

        if (state->surface) {
            wl_surface_attach(state->surface, NULL, 0, 0);
            wl_surface_commit(state->surface);
        }

        if (obj->is_pointer) {
            wl_pointer_set_cursor(obj->wl_pointer, serial, NULL, 0, 0);
        } else {
            zwp_tablet_tool_v2_set_cursor(obj->wl_tool, serial, NULL, 0, 0);
        }
    }
}

static bool Wayland_ShowCursor(SDL_Cursor *cursor)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->internal;
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_WaylandSeat *seat;
    Wayland_PointerObject obj;

    wl_list_for_each (seat, &d->seat_list, link) {
        obj.wl_pointer = seat->pointer.wl_pointer;
        obj.is_pointer = true;
        if (mouse->focus && mouse->focus->internal == seat->pointer.focus) {
            Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, seat->pointer.focus, seat->pointer.enter_serial, cursor);
        } else if (!seat->pointer.focus) {
            Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, seat->pointer.focus, seat->pointer.enter_serial, NULL);
        }

        SDL_WaylandPenTool *tool;
        wl_list_for_each(tool, &seat->tablet.tool_list, link) {
            obj.wl_tool = tool->wltool;
            obj.is_pointer = false;

            /* The current cursor is explicitly set on tablet tools, as there may be no pointer device, or
             * the pointer may not have focus, which would instead cause the default cursor to be set.
             */
            if (tool->focus && (!mouse->focus || mouse->focus->internal == tool->focus)) {
                Wayland_CursorStateSetCursor(&tool->cursor_state, &obj, tool->focus, tool->proximity_serial, mouse->cur_cursor);
            } else if (!tool->focus) {
                Wayland_CursorStateSetCursor(&tool->cursor_state, &obj, tool->focus, tool->proximity_serial, NULL);
            }
        }
    }

    return true;
}

void Wayland_SeatWarpMouse(SDL_WaylandSeat *seat, SDL_WindowData *window, float x, float y)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->internal;

    if (seat->pointer.wl_pointer) {
        if (d->wp_pointer_warp_v1) {
            // It's a protocol error to warp the pointer outside of the surface, so clamp the position.
            const wl_fixed_t f_x = wl_fixed_from_double(SDL_clamp(x / window->pointer_scale.x, 0, window->current.logical_width));
            const wl_fixed_t f_y = wl_fixed_from_double(SDL_clamp(y / window->pointer_scale.y, 0, window->current.logical_height));
            wp_pointer_warp_v1_warp_pointer(d->wp_pointer_warp_v1, window->surface, seat->pointer.wl_pointer, f_x, f_y, seat->pointer.enter_serial);
        } else {
            bool update_grabs = false;

            // Pointers can only have one confinement type active on a surface at one time.
            if (seat->pointer.confined_pointer) {
                zwp_confined_pointer_v1_destroy(seat->pointer.confined_pointer);
                seat->pointer.confined_pointer = NULL;
                update_grabs = true;
            }
            if (seat->pointer.locked_pointer) {
                zwp_locked_pointer_v1_destroy(seat->pointer.locked_pointer);
                seat->pointer.locked_pointer = NULL;
                update_grabs = true;
            }

            /* The pointer confinement protocol allows setting a hint to warp the pointer,
             * but only when the pointer is locked.
             *
             * Lock the pointer, set the position hint, unlock, and hope for the best.
             */
            struct zwp_locked_pointer_v1 *warp_lock =
                zwp_pointer_constraints_v1_lock_pointer(d->pointer_constraints, window->surface,
                                                        seat->pointer.wl_pointer, NULL,
                                                        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);

            const wl_fixed_t f_x = wl_fixed_from_double(x / window->pointer_scale.x);
            const wl_fixed_t f_y = wl_fixed_from_double(y / window->pointer_scale.y);
            zwp_locked_pointer_v1_set_cursor_position_hint(warp_lock, f_x, f_y);
            wl_surface_commit(window->surface);

            zwp_locked_pointer_v1_destroy(warp_lock);

            if (update_grabs) {
                Wayland_SeatUpdatePointerGrab(seat);
            }

            /* NOTE: There is a pending warp event under discussion that should replace this when available.
             * https://gitlab.freedesktop.org/wayland/wayland/-/merge_requests/340
             */
            SDL_SendMouseMotion(0, window->sdlwindow, seat->pointer.sdl_id, false, x, y);
        }
    }
}

static bool Wayland_WarpMouseRelative(SDL_Window *window, float x, float y)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->internal;
    SDL_WindowData *wind = window->internal;
    SDL_WaylandSeat *seat;

    if (d->wp_pointer_warp_v1 || d->pointer_constraints) {
        wl_list_for_each (seat, &d->seat_list, link) {
            if (wind == seat->pointer.focus) {
                Wayland_SeatWarpMouse(seat, wind, x, y);
            }
        }
    } else {
        return SDL_SetError("wayland: mouse warp failed; compositor lacks support for the required wp_pointer_warp_v1 or zwp_pointer_confinement_v1 protocol");
    }

    return true;
}

static bool Wayland_WarpMouseGlobal(float x, float y)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->internal;
    SDL_WaylandSeat *seat;

    if (d->wp_pointer_warp_v1 || d->pointer_constraints) {
        wl_list_for_each (seat, &d->seat_list, link) {
            SDL_WindowData *wind = seat->pointer.focus ? seat->pointer.focus : seat->keyboard.focus;

            // If the client wants the coordinates warped to within a focused window, just convert the coordinates to relative.
            if (wind) {
                SDL_Window *window = wind->sdlwindow;

                int abs_x, abs_y;
                SDL_RelativeToGlobalForWindow(window, window->x, window->y, &abs_x, &abs_y);

                const SDL_FPoint p = { x, y };
                const SDL_FRect r = { abs_x, abs_y, window->w, window->h };

                // Try to warp the cursor if the point is within the seat's focused window.
                if (SDL_PointInRectFloat(&p, &r)) {
                    Wayland_SeatWarpMouse(seat, wind, p.x - abs_x, p.y - abs_y);
                }
            }
        }
    } else {
        return SDL_SetError("wayland: mouse warp failed; compositor lacks support for the required wp_pointer_warp_v1 or zwp_pointer_confinement_v1 protocol");
    }

    return true;
}

static bool Wayland_SetRelativeMouseMode(bool enabled)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *data = vd->internal;

    // Relative mode requires both the relative motion and pointer confinement protocols.
    if (!data->relative_pointer_manager) {
        return SDL_SetError("Failed to enable relative mode: compositor lacks support for the required zwp_relative_pointer_manager_v1 protocol");
    }
    if (!data->pointer_constraints) {
        return SDL_SetError("Failed to enable relative mode: compositor lacks support for the required zwp_pointer_constraints_v1 protocol");
    }

    // Windows have a relative mode flag, so just update the grabs on a state change.
    Wayland_DisplayUpdatePointerGrabs(data, NULL);
    return true;
}

/* Wayland doesn't support getting the true global cursor position, but it can
 * be faked well enough for what most applications use it for: querying the
 * global cursor coordinates and transforming them to the window-relative
 * coordinates manually.
 *
 * The global position is derived by taking the cursor position relative to the
 * toplevel window, and offsetting it by the origin of the output the window is
 * currently considered to be on. The cursor position and button state when the
 * cursor is outside an application window are unknown, but this gives 'correct'
 * coordinates when the window has focus, which is good enough for most
 * applications.
 */
static SDL_MouseButtonFlags SDLCALL Wayland_GetGlobalMouseState(float *x, float *y)
{
    const SDL_Mouse *mouse = SDL_GetMouse();
    SDL_MouseButtonFlags result = 0;

    // If there is no window with mouse focus, we have no idea what the actual position or button state is.
    if (mouse->focus) {
        SDL_VideoData *video_data = SDL_GetVideoDevice()->internal;
        SDL_WaylandSeat *seat;
        int off_x, off_y;
        SDL_RelativeToGlobalForWindow(mouse->focus, mouse->focus->x, mouse->focus->y, &off_x, &off_y);
        *x = mouse->x + off_x;
        *y = mouse->y + off_y;

        // Query the buttons from the seats directly, as this may be called from within a hit test handler.
        wl_list_for_each (seat, &video_data->seat_list, link) {
            result |= seat->pointer.buttons_pressed;
        }
    } else {
        *x = 0.f;
        *y = 0.f;
    }

    return result;
}

#if 0  // TODO RECONNECT: See waylandvideo.c for more information!
static void Wayland_RecreateCursor(SDL_Cursor *cursor, SDL_VideoData *vdata)
{
    SDL_CursorData *cdata = cursor->internal;

    // Probably not a cursor we own
    if (cdata == NULL) {
        return;
    }

    Wayland_FreeCursorData(cdata);

    // We're not currently freeing this, so... yolo?
    if (cdata->shm_data != NULL) {
        void *old_data_pointer = cdata->shm_data;
        int stride = cdata->w * 4;

        create_buffer_from_shm(cdata, cdata->w, cdata->h, WL_SHM_FORMAT_ARGB8888);

        SDL_memcpy(cdata->shm_data, old_data_pointer, stride * cdata->h);
    }
    cdata->surface = wl_compositor_create_surface(vdata->compositor);
    wl_surface_set_user_data(cdata->surface, NULL);
}

void Wayland_RecreateCursors(void)
{
    SDL_Cursor *cursor;
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_VideoData *vdata = SDL_GetVideoDevice()->internal;

    if (vdata && vdata->cursor_themes) {
        SDL_free(vdata->cursor_themes);
        vdata->cursor_themes = NULL;
        vdata->num_cursor_themes = 0;
    }

    if (mouse == NULL) {
        return;
    }

    for (cursor = mouse->cursors; cursor != NULL; cursor = cursor->next) {
        Wayland_RecreateCursor(cursor, vdata);
    }
    if (mouse->def_cursor) {
        Wayland_RecreateCursor(mouse->def_cursor, vdata);
    }
    if (mouse->cur_cursor) {
        Wayland_RecreateCursor(mouse->cur_cursor, vdata);
        if (mouse->cursor_visible) {
            Wayland_ShowCursor(mouse->cur_cursor);
        }
    }
}
#endif // 0

void Wayland_InitMouse(SDL_VideoData *data)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = Wayland_CreateCursor;
    mouse->CreateAnimatedCursor = Wayland_CreateAnimatedCursor;
    mouse->CreateSystemCursor = Wayland_CreateSystemCursor;
    mouse->ShowCursor = Wayland_ShowCursor;
    mouse->FreeCursor = Wayland_FreeCursor;
    mouse->WarpMouse = Wayland_WarpMouseRelative;
    mouse->WarpMouseGlobal = Wayland_WarpMouseGlobal;
    mouse->SetRelativeMouseMode = Wayland_SetRelativeMouseMode;
    mouse->GetGlobalMouseState = Wayland_GetGlobalMouseState;

    if (!Wayland_StartCursorThread(data)) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "wayland: Failed to start cursor animation event thread");
    }

    SDL_HitTestResult r = SDL_HITTEST_NORMAL;
    while (r <= SDL_HITTEST_RESIZE_LEFT) {
        switch (r) {
        case SDL_HITTEST_NORMAL:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
            break;
        case SDL_HITTEST_DRAGGABLE:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
            break;
        case SDL_HITTEST_RESIZE_TOPLEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_NW_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_TOP:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_N_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_TOPRIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_NE_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_RIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_E_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_SE_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_BOTTOM:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_S_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_BOTTOMLEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_SW_RESIZE);
            break;
        case SDL_HITTEST_RESIZE_LEFT:
            sys_cursors[r] = Wayland_CreateSystemCursor(SDL_SYSTEM_CURSOR_W_RESIZE);
            break;
        }
        r++;
    }

#ifdef SDL_USE_LIBDBUS
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = vd->internal;

    /* The D-Bus cursor properties are only needed when manually loading themes and system cursors.
     * If the cursor shape protocol is present, the compositor will handle it internally.
     */
    if (!d->cursor_shape_manager) {
        Wayland_DBusInitCursorProperties(d);
    }
#endif

    SDL_SetDefaultCursor(Wayland_CreateDefaultCursor());
}

void Wayland_FiniMouse(SDL_VideoData *data)
{
    for (int i = 0; i < SDL_arraysize(sys_cursors); i++) {
        Wayland_FreeCursor(sys_cursors[i]);
        sys_cursors[i] = NULL;
    }

    Wayland_DestroyCursorThread(data);
    Wayland_FreeCursorThemes(data);

#ifdef SDL_USE_LIBDBUS
    Wayland_DBusFinishCursorProperties();
#endif
}

void Wayland_SeatUpdatePointerCursor(SDL_WaylandSeat *seat)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_WindowData *pointer_focus = seat->pointer.focus;
    const Wayland_PointerObject obj = {
        .wl_pointer = seat->pointer.wl_pointer,
        .is_pointer = true
    };

    if (pointer_focus && mouse->cursor_visible) {
        if (!seat->pointer.relative_pointer || !mouse->relative_mode_hide_cursor) {
            const SDL_HitTestResult rc = pointer_focus->hit_test_result;

            if (seat->pointer.relative_pointer || rc == SDL_HITTEST_NORMAL || rc == SDL_HITTEST_DRAGGABLE) {
                Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, pointer_focus, seat->pointer.enter_serial, mouse->cur_cursor);
            } else {
                Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, pointer_focus, seat->pointer.enter_serial, sys_cursors[rc]);
            }
        } else {
            // Hide the cursor in relative mode, unless requested otherwise by the hint.
            Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, pointer_focus, seat->pointer.enter_serial, NULL);
        }
    } else {
        /* The spec states "The cursor actually changes only if the input device focus is one of the
         * requesting client's surfaces", so just clear the cursor if the seat has no pointer focus.
         */
        Wayland_CursorStateSetCursor(&seat->pointer.cursor_state, &obj, pointer_focus, seat->pointer.enter_serial, NULL);
    }
}

void Wayland_TabletToolUpdateCursor(SDL_WaylandPenTool *tool)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_WindowData *tool_focus = tool->focus;
    const Wayland_PointerObject obj = {
        .wl_tool = tool->wltool,
        .is_pointer = false
    };

    if (tool_focus && mouse->cursor_visible) {
        // Relative mode is only relevant if the tool sends pointer events.
        const bool relative = mouse->pen_mouse_events && (tool_focus->sdlwindow->flags & SDL_WINDOW_MOUSE_RELATIVE_MODE);

        if (!relative || !mouse->relative_mode_hide_cursor) {
            Wayland_CursorStateSetCursor(&tool->cursor_state, &obj, tool_focus, tool->proximity_serial, mouse->cur_cursor);
        } else {
            // Hide the cursor in relative mode, unless requested otherwise by the hint.
            Wayland_CursorStateSetCursor(&tool->cursor_state, &obj, tool_focus, tool->proximity_serial, NULL);
        }
    } else {
        Wayland_CursorStateSetCursor(&tool->cursor_state, &obj, tool_focus, tool->proximity_serial, NULL);
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
