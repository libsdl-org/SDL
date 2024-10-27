/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_scancode_tables_c.h"
#include "../../core/linux/SDL_system_theme.h"
#include "../SDL_sysvideo.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandmouse.h"

#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "tablet-v2-client-protocol.h"
#include "primary-selection-unstable-v1-client-protocol.h"
#include "input-timestamps-unstable-v1-client-protocol.h"

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

#ifdef SDL_INPUT_LINUXEV
#include <linux/input.h>
#else
#define BTN_LEFT   (0x110)
#define BTN_RIGHT  (0x111)
#define BTN_MIDDLE (0x112)
#define BTN_SIDE   (0x113)
#define BTN_EXTRA  (0x114)
#endif
#include "../../events/SDL_keysym_to_scancode_c.h"
#include "../../events/imKStoUCS.h"
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#include "cursor-shape-v1-client-protocol.h"

// Weston uses a ratio of 10 units per scroll tick
#define WAYLAND_WHEEL_AXIS_UNIT 10

// xkbcommon as of 1.4.1 doesn't have a name macro for the mode key
#ifndef XKB_MOD_NAME_MODE
#define XKB_MOD_NAME_MODE "Mod5"
#endif

// Keyboard and mouse names to match XWayland
#define WAYLAND_DEFAULT_KEYBOARD_NAME "Virtual core keyboard"
#define WAYLAND_DEFAULT_POINTER_NAME "Virtual core pointer"

// Focus clickthrough timeout
#define WAYLAND_FOCUS_CLICK_TIMEOUT_NS SDL_MS_TO_NS(10)

struct SDL_WaylandTouchPoint
{
    SDL_TouchID id;
    wl_fixed_t fx;
    wl_fixed_t fy;
    struct wl_surface *surface;

    struct wl_list link;
};

static struct wl_list touch_points;

static void touch_add(SDL_TouchID id, wl_fixed_t fx, wl_fixed_t fy, struct wl_surface *surface)
{
    struct SDL_WaylandTouchPoint *tp = SDL_malloc(sizeof(struct SDL_WaylandTouchPoint));

    SDL_zerop(tp);
    tp->id = id;
    tp->fx = fx;
    tp->fy = fy;
    tp->surface = surface;

    WAYLAND_wl_list_insert(&touch_points, &tp->link);
}

static void touch_update(SDL_TouchID id, wl_fixed_t fx, wl_fixed_t fy, struct wl_surface **surface)
{
    struct SDL_WaylandTouchPoint *tp;

    wl_list_for_each (tp, &touch_points, link) {
        if (tp->id == id) {
            tp->fx = fx;
            tp->fy = fy;
            if (surface) {
                *surface = tp->surface;
            }
            break;
        }
    }
}

static void touch_del(SDL_TouchID id, wl_fixed_t *fx, wl_fixed_t *fy, struct wl_surface **surface)
{
    struct SDL_WaylandTouchPoint *tp;

    wl_list_for_each (tp, &touch_points, link) {
        if (tp->id == id) {
            if (fx) {
                *fx = tp->fx;
            }
            if (fy) {
                *fy = tp->fy;
            }
            if (surface) {
                *surface = tp->surface;
            }

            WAYLAND_wl_list_remove(&tp->link);
            SDL_free(tp);
            break;
        }
    }
}

static bool Wayland_SurfaceHasActiveTouches(struct wl_surface *surface)
{
    struct SDL_WaylandTouchPoint *tp;

    wl_list_for_each (tp, &touch_points, link) {
        if (tp->surface == surface) {
            return true;
        }
    }

    return false;
}

static Uint64 Wayland_GetEventTimestamp(Uint64 nsTimestamp)
{
    static Uint64 last;
    static Uint64 timestamp_offset;
    const Uint64 now = SDL_GetTicksNS();

    if (nsTimestamp < last) {
        // 32-bit timer rollover, bump the offset
        timestamp_offset += SDL_MS_TO_NS(0x100000000LLU);
    }
    last = nsTimestamp;

    if (!timestamp_offset) {
        timestamp_offset = (now - nsTimestamp);
    }
    nsTimestamp += timestamp_offset;

    if (nsTimestamp > now) {
        timestamp_offset -= (nsTimestamp - now);
        nsTimestamp = now;
    }

    return nsTimestamp;
}

static void Wayland_input_timestamp_listener(void *data, struct zwp_input_timestamps_v1 *zwp_input_timestamps_v1,
                                             uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    *((Uint64 *)data) = ((((Uint64)tv_sec_hi << 32) | (Uint64)tv_sec_lo) * SDL_NS_PER_SECOND) + tv_nsec;
}

static const struct zwp_input_timestamps_v1_listener timestamp_listener = {
    Wayland_input_timestamp_listener
};

static Uint64 Wayland_GetKeyboardTimestamp(struct SDL_WaylandInput *input, Uint32 wl_timestamp_ms)
{
    if (wl_timestamp_ms) {
        return Wayland_GetEventTimestamp(input->keyboard_timestamp_ns ? input->keyboard_timestamp_ns : SDL_MS_TO_NS(wl_timestamp_ms));
    }

    return 0;
}

static Uint64 Wayland_GetKeyboardTimestampRaw(struct SDL_WaylandInput *input, Uint32 wl_timestamp_ms)
{
    if (wl_timestamp_ms) {
        return input->keyboard_timestamp_ns ? input->keyboard_timestamp_ns : SDL_MS_TO_NS(wl_timestamp_ms);
    }

    return 0;
}

static Uint64 Wayland_GetPointerTimestamp(struct SDL_WaylandInput *input, Uint32 wl_timestamp_ms)
{
    if (wl_timestamp_ms) {
        return Wayland_GetEventTimestamp(input->pointer_timestamp_ns ? input->pointer_timestamp_ns : SDL_MS_TO_NS(wl_timestamp_ms));
    }

    return 0;
}

Uint64 Wayland_GetTouchTimestamp(struct SDL_WaylandInput *input, Uint32 wl_timestamp_ms)
{
    if (wl_timestamp_ms) {
        return Wayland_GetEventTimestamp(input->touch_timestamp_ns ? input->touch_timestamp_ns : SDL_MS_TO_NS(wl_timestamp_ms));
    }

    return 0;
}

void Wayland_RegisterTimestampListeners(struct SDL_WaylandInput *input)
{
    SDL_VideoData *viddata = input->display;

    if (viddata->input_timestamps_manager) {
        if (input->keyboard && !input->keyboard_timestamps) {
            input->keyboard_timestamps = zwp_input_timestamps_manager_v1_get_keyboard_timestamps(viddata->input_timestamps_manager, input->keyboard);
            zwp_input_timestamps_v1_add_listener(input->keyboard_timestamps, &timestamp_listener, &input->keyboard_timestamp_ns);
        }

        if (input->pointer && !input->pointer_timestamps) {
            input->pointer_timestamps = zwp_input_timestamps_manager_v1_get_pointer_timestamps(viddata->input_timestamps_manager, input->pointer);
            zwp_input_timestamps_v1_add_listener(input->pointer_timestamps, &timestamp_listener, &input->pointer_timestamp_ns);
        }

        if (input->touch && !input->touch_timestamps) {
            input->touch_timestamps = zwp_input_timestamps_manager_v1_get_touch_timestamps(viddata->input_timestamps_manager, input->touch);
            zwp_input_timestamps_v1_add_listener(input->touch_timestamps, &timestamp_listener, &input->touch_timestamp_ns);
        }
    }
}

void Wayland_CreateCursorShapeDevice(struct SDL_WaylandInput *input)
{
    SDL_VideoData *viddata = input->display;

    if (viddata->cursor_shape_manager) {
        if (input->pointer && !input->cursor_shape) {
            input->cursor_shape = wp_cursor_shape_manager_v1_get_pointer(viddata->cursor_shape_manager, input->pointer);
        }
    }
}

// Returns true if a key repeat event was due
static bool keyboard_repeat_handle(SDL_WaylandKeyboardRepeat *repeat_info, Uint64 elapsed)
{
    bool ret = false;
    while (elapsed >= repeat_info->next_repeat_ns) {
        if (repeat_info->scancode != SDL_SCANCODE_UNKNOWN) {
            const Uint64 timestamp = repeat_info->wl_press_time_ns + repeat_info->next_repeat_ns;
            SDL_SendKeyboardKeyIgnoreModifiers(Wayland_GetEventTimestamp(timestamp), repeat_info->keyboard_id, repeat_info->key, repeat_info->scancode, true);
        }
        if (repeat_info->text[0]) {
            SDL_SendKeyboardText(repeat_info->text);
        }
        repeat_info->next_repeat_ns += SDL_NS_PER_SECOND / (Uint64)repeat_info->repeat_rate;
        ret = true;
    }
    return ret;
}

static void keyboard_repeat_clear(SDL_WaylandKeyboardRepeat *repeat_info)
{
    if (!repeat_info->is_initialized) {
        return;
    }
    repeat_info->is_key_down = false;
}

static void keyboard_repeat_set(SDL_WaylandKeyboardRepeat *repeat_info, Uint32 keyboard_id, uint32_t key, Uint64 wl_press_time_ns,
                                uint32_t scancode, bool has_text, char text[8])
{
    if (!repeat_info->is_initialized || !repeat_info->repeat_rate) {
        return;
    }
    repeat_info->is_key_down = true;
    repeat_info->keyboard_id = keyboard_id;
    repeat_info->key = key;
    repeat_info->wl_press_time_ns = wl_press_time_ns;
    repeat_info->sdl_press_time_ns = SDL_GetTicksNS();
    repeat_info->next_repeat_ns = SDL_MS_TO_NS(repeat_info->repeat_delay_ms);
    repeat_info->scancode = scancode;
    if (has_text) {
        SDL_copyp(repeat_info->text, text);
    } else {
        repeat_info->text[0] = '\0';
    }
}

static uint32_t keyboard_repeat_get_key(SDL_WaylandKeyboardRepeat *repeat_info)
{
    if (repeat_info->is_initialized && repeat_info->is_key_down) {
        return repeat_info->key;
    }

    return 0;
}

static void keyboard_repeat_set_text(SDL_WaylandKeyboardRepeat *repeat_info, const char text[8])
{
    if (repeat_info->is_initialized) {
        SDL_copyp(repeat_info->text, text);
    }
}

static bool keyboard_repeat_is_set(SDL_WaylandKeyboardRepeat *repeat_info)
{
    return repeat_info->is_initialized && repeat_info->is_key_down;
}

static bool keyboard_repeat_key_is_set(SDL_WaylandKeyboardRepeat *repeat_info, uint32_t key)
{
    return repeat_info->is_initialized && repeat_info->is_key_down && key == repeat_info->key;
}

static void sync_done_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    // Nothing to do, just destroy the callback
    wl_callback_destroy(callback);
}

static struct wl_callback_listener sync_listener = {
    sync_done_handler
};

void Wayland_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *d = _this->internal;

    /* Queue a sync event to unblock the event queue fd if it's empty and being waited on.
     * TODO: Maybe use a pipe to avoid the compositor roundtrip?
     */
    struct wl_callback *cb = wl_display_sync(d->display);
    wl_callback_add_listener(cb, &sync_listener, NULL);
    WAYLAND_wl_display_flush(d->display);
}

static int dispatch_queued_events(SDL_VideoData *viddata)
{
    int rc;

    /*
     * NOTE: When reconnection is implemented, check if libdecor needs to be
     *       involved in the reconnection process.
     */
#ifdef HAVE_LIBDECOR_H
    if (viddata->shell.libdecor) {
        libdecor_dispatch(viddata->shell.libdecor, 0);
    }
#endif

    rc = WAYLAND_wl_display_dispatch_pending(viddata->display);
    return rc >= 0 ? 1 : rc;
}

int Wayland_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS)
{
    SDL_VideoData *d = _this->internal;
    struct SDL_WaylandInput *input = d->input;
    bool key_repeat_active = false;

    WAYLAND_wl_display_flush(d->display);

#ifdef SDL_USE_IME
    SDL_Window *keyboard_focus = SDL_GetKeyboardFocus();
    if (!d->text_input_manager && keyboard_focus && SDL_TextInputActive(keyboard_focus)) {
        SDL_IME_PumpEvents();
    }
#endif

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_PumpEvents();
#endif

    // If key repeat is active, we'll need to cap our maximum wait time to handle repeats
    if (input && keyboard_repeat_is_set(&input->keyboard_repeat)) {
        const Uint64 elapsed = SDL_GetTicksNS() - input->keyboard_repeat.sdl_press_time_ns;
        if (keyboard_repeat_handle(&input->keyboard_repeat, elapsed)) {
            // A repeat key event was already due
            return 1;
        } else {
            const Uint64 next_repeat_wait_time = (input->keyboard_repeat.next_repeat_ns - elapsed) + 1;
            if (timeoutNS >= 0) {
                timeoutNS = SDL_min(timeoutNS, next_repeat_wait_time);
            } else {
                timeoutNS = next_repeat_wait_time;
            }
            key_repeat_active = true;
        }
    }

    /* wl_display_prepare_read() will return -1 if the default queue is not empty.
     * If the default queue is empty, it will prepare us for our SDL_IOReady() call. */
    if (WAYLAND_wl_display_prepare_read(d->display) == 0) {
        // Use SDL_IOR_NO_RETRY to ensure SIGINT will break us out of our wait
        int err = SDL_IOReady(WAYLAND_wl_display_get_fd(d->display), SDL_IOR_READ | SDL_IOR_NO_RETRY, timeoutNS);
        if (err > 0) {
            // There are new events available to read
            WAYLAND_wl_display_read_events(d->display);
            return dispatch_queued_events(d);
        } else if (err == 0) {
            // No events available within the timeout
            WAYLAND_wl_display_cancel_read(d->display);

            // If key repeat is active, we might have woken up to generate a key event
            if (key_repeat_active) {
                const Uint64 elapsed = SDL_GetTicksNS() - input->keyboard_repeat.sdl_press_time_ns;
                if (keyboard_repeat_handle(&input->keyboard_repeat, elapsed)) {
                    return 1;
                }
            }

            return 0;
        } else {
            // Error returned from poll()/select()
            WAYLAND_wl_display_cancel_read(d->display);

            if (errno == EINTR) {
                /* If the wait was interrupted by a signal, we may have generated a
                 * SDL_EVENT_QUIT event. Let the caller know to call SDL_PumpEvents(). */
                return 1;
            } else {
                return err;
            }
        }
    } else {
        // We already had pending events
        return dispatch_queued_events(d);
    }
}

void Wayland_PumpEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *d = _this->internal;
    struct SDL_WaylandInput *input = d->input;
    int err;

#ifdef SDL_USE_IME
    SDL_Window *keyboard_focus = SDL_GetKeyboardFocus();
    if (!d->text_input_manager && keyboard_focus && SDL_TextInputActive(keyboard_focus)) {
        SDL_IME_PumpEvents();
    }
#endif

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_PumpEvents();
#endif

#ifdef HAVE_LIBDECOR_H
    if (d->shell.libdecor) {
        libdecor_dispatch(d->shell.libdecor, 0);
    }
#endif

    WAYLAND_wl_display_flush(d->display);

    /* wl_display_prepare_read() will return -1 if the default queue is not empty.
     * If the default queue is empty, it will prepare us for our SDL_IOReady() call. */
    if (WAYLAND_wl_display_prepare_read(d->display) == 0) {
        if (SDL_IOReady(WAYLAND_wl_display_get_fd(d->display), SDL_IOR_READ, 0) > 0) {
            WAYLAND_wl_display_read_events(d->display);
        } else {
            WAYLAND_wl_display_cancel_read(d->display);
        }
    }

    // Dispatch any pre-existing pending events or new events we may have read
    err = WAYLAND_wl_display_dispatch_pending(d->display);

    if (input && keyboard_repeat_is_set(&input->keyboard_repeat)) {
        const Uint64 elapsed = SDL_GetTicksNS() - input->keyboard_repeat.sdl_press_time_ns;
        keyboard_repeat_handle(&input->keyboard_repeat, elapsed);
    }

    if (err < 0 && !d->display_disconnected) {
        /* Something has failed with the Wayland connection -- for example,
         * the compositor may have shut down and closed its end of the socket,
         * or there is a library-specific error.
         *
         * Try to recover once, then quit.
         */
        if (!Wayland_VideoReconnect(_this)) {
            d->display_disconnected = 1;
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Wayland display connection closed by server (fatal)");

            /* Only send a single quit message, as application shutdown might call
             * SDL_PumpEvents
             */
            SDL_SendQuit();
        }
    }
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window_data = input->pointer_focus;
    SDL_Window *window = window_data ? window_data->sdlwindow : NULL;

    input->sx_w = sx_w;
    input->sy_w = sy_w;
    if (input->pointer_focus) {
        const float sx = (float)(wl_fixed_to_double(sx_w) * window_data->pointer_scale.x);
        const float sy = (float)(wl_fixed_to_double(sy_w) * window_data->pointer_scale.y);
        SDL_SendMouseMotion(Wayland_GetPointerTimestamp(input, time), window_data->sdlwindow, input->pointer_id, false, sx, sy);
    }

    if (window && window->hit_test) {
        const SDL_Point point = { (int)SDL_floor(wl_fixed_to_double(sx_w) * window_data->pointer_scale.x),
                                  (int)SDL_floor(wl_fixed_to_double(sy_w) * window_data->pointer_scale.y) };
        SDL_HitTestResult rc = window->hit_test(window, &point, window->hit_test_data);
        if (rc == window_data->hit_test_result) {
            return;
        }

        Wayland_SetHitTestCursor(rc);
        window_data->hit_test_result = rc;
    }
}

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window;

    if (!surface) {
        // enter event for a window we've just destroyed
        return;
    }

    /* This handler will be called twice in Wayland 1.4
     * Once for the window surface which has valid user data
     * and again for the mouse cursor surface which does not have valid user data
     * We ignore the later
     */
    window = Wayland_GetWindowDataForOwnedSurface(surface);

    if (window) {
        input->pointer_focus = window;
        input->pointer_enter_serial = serial;
        SDL_SetMouseFocus(window->sdlwindow);
        /* In the case of e.g. a pointer confine warp, we may receive an enter
         * event with no following motion event, but with the new coordinates
         * as part of the enter event.
         *
         * FIXME: This causes a movement event with an anomalous timestamp when
         *        the cursor enters the window.
         */
        pointer_handle_motion(data, pointer, 0, sx_w, sy_w);
        /* If the cursor was changed while our window didn't have pointer
         * focus, we might need to trigger another call to
         * wl_pointer_set_cursor() for the new cursor to be displayed. */
        Wayland_SetHitTestCursor(window->hit_test_result);
    }
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    struct SDL_WaylandInput *input = data;

    if (!surface) {
        return;
    }

    if (input->pointer_focus) {
        SDL_WindowData *wind = Wayland_GetWindowDataForOwnedSurface(surface);

        if (wind) {
            // Clear the capture flag and raise all buttons
            wind->sdlwindow->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;

            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, input->pointer_id, SDL_BUTTON_LEFT, false);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, input->pointer_id, SDL_BUTTON_RIGHT, false);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, input->pointer_id, SDL_BUTTON_MIDDLE, false);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, input->pointer_id, SDL_BUTTON_X1, false);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, input->pointer_id, SDL_BUTTON_X2, false);
        }


        /* A pointer leave event may be emitted if the compositor hides the pointer in response to receiving a touch event.
         * Don't relinquish focus if the surface has active touches, as the compositor is just transitioning from mouse to touch mode.
         */
        if (!Wayland_SurfaceHasActiveTouches(surface)) {
            SDL_SetMouseFocus(NULL);
        }
        input->pointer_focus = NULL;
    }
}

static bool ProcessHitTest(SDL_WindowData *window_data,
			       struct wl_seat *seat,
			       wl_fixed_t sx_w, wl_fixed_t sy_w,
			       uint32_t serial)
{
    SDL_Window *window = window_data->sdlwindow;

    if (window->hit_test) {
        static const uint32_t directions[] = {
            XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT, XDG_TOPLEVEL_RESIZE_EDGE_TOP,
            XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT, XDG_TOPLEVEL_RESIZE_EDGE_RIGHT,
            XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT, XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM,
            XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT, XDG_TOPLEVEL_RESIZE_EDGE_LEFT
        };

#ifdef HAVE_LIBDECOR_H
        static const uint32_t directions_libdecor[] = {
            LIBDECOR_RESIZE_EDGE_TOP_LEFT, LIBDECOR_RESIZE_EDGE_TOP,
            LIBDECOR_RESIZE_EDGE_TOP_RIGHT, LIBDECOR_RESIZE_EDGE_RIGHT,
            LIBDECOR_RESIZE_EDGE_BOTTOM_RIGHT, LIBDECOR_RESIZE_EDGE_BOTTOM,
            LIBDECOR_RESIZE_EDGE_BOTTOM_LEFT, LIBDECOR_RESIZE_EDGE_LEFT
        };
#endif

        switch (window_data->hit_test_result) {
        case SDL_HITTEST_DRAGGABLE:
#ifdef HAVE_LIBDECOR_H
            if (window_data->shell_surface_type == WAYLAND_SHELL_SURFACE_TYPE_LIBDECOR) {
                if (window_data->shell_surface.libdecor.frame) {
                    libdecor_frame_move(window_data->shell_surface.libdecor.frame,
                                        seat,
                                        serial);
                }
            } else
#endif
                if (window_data->shell_surface_type == WAYLAND_SHELL_SURFACE_TYPE_XDG_TOPLEVEL) {
                if (window_data->shell_surface.xdg.toplevel.xdg_toplevel) {
                    xdg_toplevel_move(window_data->shell_surface.xdg.toplevel.xdg_toplevel,
                                      seat,
                                      serial);
                }
            }
            return true;

        case SDL_HITTEST_RESIZE_TOPLEFT:
        case SDL_HITTEST_RESIZE_TOP:
        case SDL_HITTEST_RESIZE_TOPRIGHT:
        case SDL_HITTEST_RESIZE_RIGHT:
        case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
        case SDL_HITTEST_RESIZE_BOTTOM:
        case SDL_HITTEST_RESIZE_BOTTOMLEFT:
        case SDL_HITTEST_RESIZE_LEFT:
#ifdef HAVE_LIBDECOR_H
            if (window_data->shell_surface_type == WAYLAND_SHELL_SURFACE_TYPE_LIBDECOR) {
                if (window_data->shell_surface.libdecor.frame) {
                    libdecor_frame_resize(window_data->shell_surface.libdecor.frame,
                                          seat,
                                          serial,
                                          directions_libdecor[window_data->hit_test_result - SDL_HITTEST_RESIZE_TOPLEFT]);
                }
            } else
#endif
                if (window_data->shell_surface_type == WAYLAND_SHELL_SURFACE_TYPE_XDG_TOPLEVEL) {
                if (window_data->shell_surface.xdg.toplevel.xdg_toplevel) {
                    xdg_toplevel_resize(window_data->shell_surface.xdg.toplevel.xdg_toplevel,
                                        seat,
                                        serial,
                                        directions[window_data->hit_test_result - SDL_HITTEST_RESIZE_TOPLEFT]);
                }
            }
            return true;

        default:
            return false;
        }
    }

    return false;
}

static void pointer_handle_button_common(struct SDL_WaylandInput *input, uint32_t serial,
                                         uint32_t time, uint32_t button, uint32_t state_w)
{
    SDL_WindowData *window = input->pointer_focus;
    enum wl_pointer_button_state state = state_w;
    uint32_t sdl_button;
    bool ignore_click = false;

    if (window) {
        SDL_VideoData *viddata = window->waylandData;
        switch (button) {
        case BTN_LEFT:
            sdl_button = SDL_BUTTON_LEFT;
            if (ProcessHitTest(input->pointer_focus, input->seat, input->sx_w, input->sy_w, serial)) {
                return; // don't pass this event on to app.
            }
            break;
        case BTN_MIDDLE:
            sdl_button = SDL_BUTTON_MIDDLE;
            break;
        case BTN_RIGHT:
            sdl_button = SDL_BUTTON_RIGHT;
            break;
        case BTN_SIDE:
            sdl_button = SDL_BUTTON_X1;
            break;
        case BTN_EXTRA:
            sdl_button = SDL_BUTTON_X2;
            break;
        default:
            return;
        }

        // Possibly ignore this click if it was to gain focus.
        if (window->last_focus_event_time_ns) {
            if (state == WL_POINTER_BUTTON_STATE_PRESSED &&
                (SDL_GetTicksNS() - window->last_focus_event_time_ns) < WAYLAND_FOCUS_CLICK_TIMEOUT_NS) {
                ignore_click = !SDL_GetHintBoolean(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, false);
            }

            window->last_focus_event_time_ns = 0;
        }

        /* Wayland won't let you "capture" the mouse, but it will automatically track
         * the mouse outside the window if you drag outside of it, until you let go
         * of all buttons (even if you add or remove presses outside the window, as
         * long as any button is still down, the capture remains).
         */
        if (state) { // update our mask of currently-pressed buttons
            input->buttons_pressed |= SDL_BUTTON_MASK(sdl_button);
        } else {
            input->buttons_pressed &= ~(SDL_BUTTON_MASK(sdl_button));
        }

        // Don't modify the capture flag in relative mode.
        if (!viddata->relative_mouse_mode) {
            if (input->buttons_pressed != 0) {
                window->sdlwindow->flags |= SDL_WINDOW_MOUSE_CAPTURE;
            } else {
                window->sdlwindow->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;
            }
        }

        if (state) {
            Wayland_UpdateImplicitGrabSerial(input, serial);
        }

        if (!ignore_click) {
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, time), window->sdlwindow, input->pointer_id,
                                sdl_button, (state != 0));
        }
    }
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                                  uint32_t time, uint32_t button, uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;

    pointer_handle_button_common(input, serial, time, button, state_w);
}

static void pointer_handle_axis_common_v1(struct SDL_WaylandInput *input,
                                          uint32_t time, uint32_t axis, wl_fixed_t value)
{
    SDL_WindowData *window = input->pointer_focus;
    enum wl_pointer_axis a = axis;
    float x, y;

    if (input->pointer_focus) {
        switch (a) {
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            x = 0;
            y = 0 - (float)wl_fixed_to_double(value);
            break;
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            x = (float)wl_fixed_to_double(value);
            y = 0;
            break;
        default:
            return;
        }

        x /= WAYLAND_WHEEL_AXIS_UNIT;
        y /= WAYLAND_WHEEL_AXIS_UNIT;

        SDL_SendMouseWheel(Wayland_GetPointerTimestamp(input, time), window->sdlwindow, input->pointer_id, x, y, SDL_MOUSEWHEEL_NORMAL);
    }
}

static void pointer_handle_axis_common(struct SDL_WaylandInput *input, enum SDL_WaylandAxisEvent type,
                                       uint32_t axis, wl_fixed_t value)
{
    enum wl_pointer_axis a = axis;

    if (input->pointer_focus) {
        switch (a) {
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            switch (type) {
            case AXIS_EVENT_VALUE120:
                /*
                 * High resolution scroll event. The spec doesn't state that axis_value120
                 * events are limited to one per frame, so the values are accumulated.
                 */
                if (input->pointer_curr_axis_info.y_axis_type != AXIS_EVENT_VALUE120) {
                    input->pointer_curr_axis_info.y_axis_type = AXIS_EVENT_VALUE120;
                    input->pointer_curr_axis_info.y = 0.0f;
                }
                input->pointer_curr_axis_info.y += 0 - (float)wl_fixed_to_double(value);
                break;
            case AXIS_EVENT_DISCRETE:
                /*
                 * This is a discrete axis event, so we process it and set the
                 * flag to ignore future continuous axis events in this frame.
                 */
                if (input->pointer_curr_axis_info.y_axis_type != AXIS_EVENT_DISCRETE) {
                    input->pointer_curr_axis_info.y_axis_type = AXIS_EVENT_DISCRETE;
                    input->pointer_curr_axis_info.y = 0 - (float)wl_fixed_to_double(value);
                }
                break;
            case AXIS_EVENT_CONTINUOUS:
                // Only process continuous events if no discrete events have been received.
                if (input->pointer_curr_axis_info.y_axis_type == AXIS_EVENT_CONTINUOUS) {
                    input->pointer_curr_axis_info.y = 0 - (float)wl_fixed_to_double(value);
                }
                break;
            }
            break;
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            switch (type) {
            case AXIS_EVENT_VALUE120:
                /*
                 * High resolution scroll event. The spec doesn't state that axis_value120
                 * events are limited to one per frame, so the values are accumulated.
                 */
                if (input->pointer_curr_axis_info.x_axis_type != AXIS_EVENT_VALUE120) {
                    input->pointer_curr_axis_info.x_axis_type = AXIS_EVENT_VALUE120;
                    input->pointer_curr_axis_info.x = 0.0f;
                }
                input->pointer_curr_axis_info.x += (float)wl_fixed_to_double(value);
                break;
            case AXIS_EVENT_DISCRETE:
                /*
                 * This is a discrete axis event, so we process it and set the
                 * flag to ignore future continuous axis events in this frame.
                 */
                if (input->pointer_curr_axis_info.x_axis_type != AXIS_EVENT_DISCRETE) {
                    input->pointer_curr_axis_info.x_axis_type = AXIS_EVENT_DISCRETE;
                    input->pointer_curr_axis_info.x = (float)wl_fixed_to_double(value);
                }
                break;
            case AXIS_EVENT_CONTINUOUS:
                // Only process continuous events if no discrete events have been received.
                if (input->pointer_curr_axis_info.x_axis_type == AXIS_EVENT_CONTINUOUS) {
                    input->pointer_curr_axis_info.x = (float)wl_fixed_to_double(value);
                }
                break;
            }
            break;
        }
    }
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct SDL_WaylandInput *input = data;

    if (wl_seat_get_version(input->seat) >= WL_POINTER_FRAME_SINCE_VERSION) {
        input->pointer_curr_axis_info.timestamp_ns = Wayland_GetPointerTimestamp(input, time);
        pointer_handle_axis_common(input, AXIS_EVENT_CONTINUOUS, axis, value);
    } else {
        pointer_handle_axis_common_v1(input, time, axis, value);
    }
}

static void pointer_handle_axis_relative_direction(void *data, struct wl_pointer *pointer,
                                                   uint32_t axis, uint32_t axis_relative_direction)
{
    struct SDL_WaylandInput *input = data;
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
        return;
    }
    switch (axis_relative_direction) {
    case WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL:
        input->pointer_curr_axis_info.direction = SDL_MOUSEWHEEL_NORMAL;
        break;
    case WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED:
        input->pointer_curr_axis_info.direction = SDL_MOUSEWHEEL_FLIPPED;
        break;
    }
}

static void pointer_handle_frame(void *data, struct wl_pointer *pointer)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window = input->pointer_focus;
    float x, y;
    SDL_MouseWheelDirection direction = input->pointer_curr_axis_info.direction;

    switch (input->pointer_curr_axis_info.x_axis_type) {
    case AXIS_EVENT_CONTINUOUS:
        x = input->pointer_curr_axis_info.x / WAYLAND_WHEEL_AXIS_UNIT;
        break;
    case AXIS_EVENT_DISCRETE:
        x = input->pointer_curr_axis_info.x;
        break;
    case AXIS_EVENT_VALUE120:
        x = input->pointer_curr_axis_info.x / 120.0f;
        break;
    default:
        x = 0.0f;
        break;
    }

    switch (input->pointer_curr_axis_info.y_axis_type) {
    case AXIS_EVENT_CONTINUOUS:
        y = input->pointer_curr_axis_info.y / WAYLAND_WHEEL_AXIS_UNIT;
        break;
    case AXIS_EVENT_DISCRETE:
        y = input->pointer_curr_axis_info.y;
        break;
    case AXIS_EVENT_VALUE120:
        y = input->pointer_curr_axis_info.y / 120.0f;
        break;
    default:
        y = 0.0f;
        break;
    }

    // clear pointer_curr_axis_info for next frame
    SDL_memset(&input->pointer_curr_axis_info, 0, sizeof(input->pointer_curr_axis_info));

    if (x != 0.0f || y != 0.0f) {
        SDL_SendMouseWheel(input->pointer_curr_axis_info.timestamp_ns,
                           window->sdlwindow, input->pointer_id, x, y, direction);
    }
}

static void pointer_handle_axis_source(void *data, struct wl_pointer *pointer,
                                       uint32_t axis_source)
{
    // unimplemented
}

static void pointer_handle_axis_stop(void *data, struct wl_pointer *pointer,
                                     uint32_t time, uint32_t axis)
{
    // unimplemented
}

static void pointer_handle_axis_discrete(void *data, struct wl_pointer *pointer,
                                         uint32_t axis, int32_t discrete)
{
    struct SDL_WaylandInput *input = data;

    pointer_handle_axis_common(input, AXIS_EVENT_DISCRETE, axis, wl_fixed_from_int(discrete));
}

static void pointer_handle_axis_value120(void *data, struct wl_pointer *pointer,
                                         uint32_t axis, int32_t value120)
{
    struct SDL_WaylandInput *input = data;

    pointer_handle_axis_common(input, AXIS_EVENT_VALUE120, axis, wl_fixed_from_int(value120));
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
    pointer_handle_frame,                  // Version 5
    pointer_handle_axis_source,            // Version 5
    pointer_handle_axis_stop,              // Version 5
    pointer_handle_axis_discrete,          // Version 5
    pointer_handle_axis_value120,          // Version 8
    pointer_handle_axis_relative_direction // Version 9
};

static void touch_handler_down(void *data, struct wl_touch *touch, uint32_t serial,
                               uint32_t timestamp, struct wl_surface *surface,
                               int id, wl_fixed_t fx, wl_fixed_t fy)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)data;
    SDL_WindowData *window_data;

    // Check that this surface is valid.
    if (!surface) {
        return;
    }

    touch_add(id, fx, fy, surface);
    Wayland_UpdateImplicitGrabSerial(input, serial);
    window_data = Wayland_GetWindowDataForOwnedSurface(surface);

    if (window_data) {
        float x, y;

        if (window_data->current.logical_width <= 1) {
            x = 0.5f;
        } else {
            x = (float)wl_fixed_to_double(fx) / (window_data->current.logical_width - 1);
        }
        if (window_data->current.logical_height <= 1) {
            y = 0.5f;
        } else {
            y = (float)wl_fixed_to_double(fy) / (window_data->current.logical_height - 1);
        }

        SDL_SetMouseFocus(window_data->sdlwindow);

        SDL_SendTouch(Wayland_GetTouchTimestamp(input, timestamp), (SDL_TouchID)(uintptr_t)touch,
                      (SDL_FingerID)(id + 1), window_data->sdlwindow, true, x, y, 1.0f);
    }
}

static void touch_handler_up(void *data, struct wl_touch *touch, uint32_t serial,
                             uint32_t timestamp, int id)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)data;
    wl_fixed_t fx = 0, fy = 0;
    struct wl_surface *surface = NULL;

    touch_del(id, &fx, &fy, &surface);

    if (surface) {
        SDL_WindowData *window_data = (SDL_WindowData *)wl_surface_get_user_data(surface);

        if (window_data) {
            const float x = (float)wl_fixed_to_double(fx) / window_data->current.logical_width;
            const float y = (float)wl_fixed_to_double(fy) / window_data->current.logical_height;

            SDL_SendTouch(Wayland_GetTouchTimestamp(input, timestamp), (SDL_TouchID)(uintptr_t)touch,
                          (SDL_FingerID)(id + 1), window_data->sdlwindow, false, x, y, 0.0f);

            /* If the seat lacks pointer focus, the seat's keyboard focus is another window or NULL, this window currently
             * has mouse focus, and the surface has no active touch events, consider mouse focus to be lost.
             */
            if (!input->pointer_focus && input->keyboard_focus != window_data &&
                SDL_GetMouseFocus() == window_data->sdlwindow && !Wayland_SurfaceHasActiveTouches(surface)) {
                SDL_SetMouseFocus(NULL);
            }
        }
    }
}

static void touch_handler_motion(void *data, struct wl_touch *touch, uint32_t timestamp,
                                 int id, wl_fixed_t fx, wl_fixed_t fy)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)data;
    struct wl_surface *surface = NULL;

    touch_update(id, fx, fy, &surface);

    if (surface) {
        SDL_WindowData *window_data = (SDL_WindowData *)wl_surface_get_user_data(surface);

        if (window_data) {
            const float x = (float)wl_fixed_to_double(fx) / window_data->current.logical_width;
            const float y = (float)wl_fixed_to_double(fy) / window_data->current.logical_height;

            SDL_SendTouchMotion(Wayland_GetPointerTimestamp(input, timestamp), (SDL_TouchID)(uintptr_t)touch,
                                (SDL_FingerID)(id + 1), window_data->sdlwindow, x, y, 1.0f);
        }
    }
}

static void touch_handler_frame(void *data, struct wl_touch *touch)
{
}

static void touch_handler_cancel(void *data, struct wl_touch *touch)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)data;
    struct SDL_WaylandTouchPoint *tp, *temp;

    wl_list_for_each_safe (tp, temp, &touch_points, link) {
        bool removed = false;

        if (tp->surface) {
            SDL_WindowData *window_data = (SDL_WindowData *)wl_surface_get_user_data(tp->surface);

            if (window_data) {
                const float x = (float)(wl_fixed_to_double(tp->fx) / window_data->current.logical_width);
                const float y = (float)(wl_fixed_to_double(tp->fy) / window_data->current.logical_height);

                SDL_SendTouch(0, (SDL_TouchID)(uintptr_t)touch,
                              (SDL_FingerID)(tp->id + 1), window_data->sdlwindow, false, x, y, 0.0f);

                // Remove the touch from the list before checking for still-active touches on the surface.
                WAYLAND_wl_list_remove(&tp->link);
                removed = true;

                /* If the seat lacks pointer focus, the seat's keyboard focus is another window or NULL, this window currently
                 * has mouse focus, and the surface has no active touch events, consider mouse focus to be lost.
                 */
                if (!input->pointer_focus && input->keyboard_focus != window_data &&
                    SDL_GetMouseFocus() == window_data->sdlwindow && !Wayland_SurfaceHasActiveTouches(tp->surface)) {
                    SDL_SetMouseFocus(NULL);
                }
            }
        }

        if (!removed) {
            WAYLAND_wl_list_remove(&tp->link);
        }
        SDL_free(tp);
    }
}

static const struct wl_touch_listener touch_listener = {
    touch_handler_down,
    touch_handler_up,
    touch_handler_motion,
    touch_handler_frame,
    touch_handler_cancel,
    NULL, // shape
    NULL, // orientation
};

typedef struct Wayland_Keymap
{
    SDL_Keymap *keymap;
    struct xkb_state *state;
    SDL_Keymod modstate;
} Wayland_Keymap;

static void Wayland_keymap_iter(struct xkb_keymap *keymap, xkb_keycode_t key, void *data)
{
    Wayland_Keymap *sdlKeymap = (Wayland_Keymap *)data;
    const xkb_keysym_t *syms;
    SDL_Scancode scancode;

    scancode = SDL_GetScancodeFromTable(SDL_SCANCODE_TABLE_XFREE86_2, (key - 8));
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    if (WAYLAND_xkb_state_key_get_syms(sdlKeymap->state, key, &syms) > 0) {
        uint32_t keycode = SDL_KeySymToUcs4(syms[0]);

        if (!keycode) {
            const SDL_Scancode sc = SDL_GetScancodeFromKeySym(syms[0], key);

            // Note: The default SDL scancode table sets this to right alt instead of AltGr/Mode, so handle it separately.
            if (syms[0] != XKB_KEY_ISO_Level3_Shift) {
                keycode = SDL_GetKeymapKeycode(NULL, sc, sdlKeymap->modstate);
            } else {
                keycode = SDLK_MODE;
            }
        }

        if (!keycode) {
            switch (scancode) {
            case SDL_SCANCODE_RETURN:
                keycode = SDLK_RETURN;
                break;
            case SDL_SCANCODE_ESCAPE:
                keycode = SDLK_ESCAPE;
                break;
            case SDL_SCANCODE_BACKSPACE:
                keycode = SDLK_BACKSPACE;
                break;
            case SDL_SCANCODE_TAB:
                keycode = SDLK_TAB;
                break;
            case SDL_SCANCODE_DELETE:
                keycode = SDLK_DELETE;
                break;
            default:
                keycode = SDL_SCANCODE_TO_KEYCODE(scancode);
                break;
            }
        }

        SDL_SetKeymapEntry(sdlKeymap->keymap, scancode, sdlKeymap->modstate, keycode);
    }
}

static void Wayland_UpdateKeymap(struct SDL_WaylandInput *input)
{
    struct Keymod_masks
    {
        SDL_Keymod sdl_mask;
        xkb_mod_mask_t xkb_mask;
    } const keymod_masks[] = {
        { SDL_KMOD_NONE, 0 },
        { SDL_KMOD_SHIFT, input->xkb.idx_shift },
        { SDL_KMOD_CAPS, input->xkb.idx_caps },
        { SDL_KMOD_SHIFT | SDL_KMOD_CAPS, input->xkb.idx_shift | input->xkb.idx_caps },
        { SDL_KMOD_MODE, input->xkb.idx_mode },
        { SDL_KMOD_MODE | SDL_KMOD_SHIFT, input->xkb.idx_mode | input->xkb.idx_shift },
        { SDL_KMOD_MODE | SDL_KMOD_CAPS, input->xkb.idx_mode | input->xkb.idx_caps },
        { SDL_KMOD_MODE | SDL_KMOD_SHIFT | SDL_KMOD_CAPS, input->xkb.idx_mode | input->xkb.idx_shift | input->xkb.idx_caps }
    };

    if (!input->keyboard_is_virtual) {
        Wayland_Keymap keymap;

        keymap.keymap = SDL_CreateKeymap();
        if (!keymap.keymap) {
            return;
        }

        keymap.state = WAYLAND_xkb_state_new(input->xkb.keymap);
        if (!keymap.state) {
            SDL_SetError("failed to create XKB state");
            SDL_DestroyKeymap(keymap.keymap);
            return;
        }

        for (int i = 0; i < SDL_arraysize(keymod_masks); ++i) {
            keymap.modstate = keymod_masks[i].sdl_mask;
            WAYLAND_xkb_state_update_mask(keymap.state,
                                          keymod_masks[i].xkb_mask & (input->xkb.idx_shift | input->xkb.idx_mode), 0, keymod_masks[i].xkb_mask & input->xkb.idx_caps,
                                          0, 0, input->xkb.current_group);
            WAYLAND_xkb_keymap_key_for_each(input->xkb.keymap,
                                            Wayland_keymap_iter,
                                            &keymap);
        }

        WAYLAND_xkb_state_unref(keymap.state);
        SDL_SetKeymap(keymap.keymap, true);
    } else {
        // Virtual keyboards use the default keymap.
        SDL_SetKeymap(NULL, true);
    }
}

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int fd, uint32_t size)
{
    struct SDL_WaylandInput *input = data;
    char *map_str;
    const char *locale;

    if (!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    if (input->xkb.keymap != NULL) {
        /* if there's already a keymap loaded, throw it away rather than leaking it before
         * parsing the new one
         */
        WAYLAND_xkb_keymap_unref(input->xkb.keymap);
        input->xkb.keymap = NULL;
    }
    input->xkb.keymap = WAYLAND_xkb_keymap_new_from_string(input->display->xkb_context,
                                                           map_str,
                                                           XKB_KEYMAP_FORMAT_TEXT_V1,
                                                           0);
    munmap(map_str, size);
    close(fd);

    if (!input->xkb.keymap) {
        SDL_SetError("failed to compile keymap\n");
        return;
    }

#define GET_MOD_INDEX(mod) \
    WAYLAND_xkb_keymap_mod_get_index(input->xkb.keymap, XKB_MOD_NAME_##mod)
    input->xkb.idx_shift = 1 << GET_MOD_INDEX(SHIFT);
    input->xkb.idx_ctrl = 1 << GET_MOD_INDEX(CTRL);
    input->xkb.idx_alt = 1 << GET_MOD_INDEX(ALT);
    input->xkb.idx_gui = 1 << GET_MOD_INDEX(LOGO);
    input->xkb.idx_mode = 1 << GET_MOD_INDEX(MODE);
    input->xkb.idx_num = 1 << GET_MOD_INDEX(NUM);
    input->xkb.idx_caps = 1 << GET_MOD_INDEX(CAPS);
#undef GET_MOD_INDEX

    if (input->xkb.state != NULL) {
        /* if there's already a state, throw it away rather than leaking it before
         * trying to create a new one with the new keymap.
         */
        WAYLAND_xkb_state_unref(input->xkb.state);
        input->xkb.state = NULL;
    }
    input->xkb.state = WAYLAND_xkb_state_new(input->xkb.keymap);
    if (!input->xkb.state) {
        SDL_SetError("failed to create XKB state\n");
        WAYLAND_xkb_keymap_unref(input->xkb.keymap);
        input->xkb.keymap = NULL;
        return;
    }

    /*
     * Assume that a nameless layout implies a virtual keyboard with an arbitrary layout.
     * TODO: Use a better method of detection?
     */
    input->keyboard_is_virtual = WAYLAND_xkb_keymap_layout_get_name(input->xkb.keymap, 0) == NULL;

    // Update the keymap if changed.
    if (input->xkb.current_group != XKB_GROUP_INVALID) {
        Wayland_UpdateKeymap(input);
    }

    /*
     * See https://blogs.s-osg.org/compose-key-support-weston/
     * for further explanation on dead keys in Wayland.
     */

    // Look up the preferred locale, falling back to "C" as default
    locale = SDL_getenv("LC_ALL");
    if (!locale) {
        locale = SDL_getenv("LC_CTYPE");
        if (!locale) {
            locale = SDL_getenv("LANG");
            if (!locale) {
                locale = "C";
            }
        }
    }

    // Set up XKB compose table
    if (input->xkb.compose_table != NULL) {
        WAYLAND_xkb_compose_table_unref(input->xkb.compose_table);
        input->xkb.compose_table = NULL;
    }
    input->xkb.compose_table = WAYLAND_xkb_compose_table_new_from_locale(input->display->xkb_context,
                                                                         locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (input->xkb.compose_table) {
        // Set up XKB compose state
        if (input->xkb.compose_state != NULL) {
            WAYLAND_xkb_compose_state_unref(input->xkb.compose_state);
            input->xkb.compose_state = NULL;
        }
        input->xkb.compose_state = WAYLAND_xkb_compose_state_new(input->xkb.compose_table,
                                                                 XKB_COMPOSE_STATE_NO_FLAGS);
        if (!input->xkb.compose_state) {
            SDL_SetError("could not create XKB compose state\n");
            WAYLAND_xkb_compose_table_unref(input->xkb.compose_table);
            input->xkb.compose_table = NULL;
        }
    }
}

/*
 * Virtual keyboards can have arbitrary layouts, arbitrary scancodes/keycodes, etc...
 * Key presses from these devices must be looked up by their keysym value.
 */
static SDL_Scancode Wayland_get_scancode_from_key(struct SDL_WaylandInput *input, uint32_t key)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;

    if (!input->keyboard_is_virtual) {
        scancode = SDL_GetScancodeFromTable(SDL_SCANCODE_TABLE_XFREE86_2, key - 8);
    } else {
        const xkb_keysym_t *syms;
        if (WAYLAND_xkb_keymap_key_get_syms_by_level(input->xkb.keymap, key, input->xkb.current_group, 0, &syms) > 0) {
            scancode = SDL_GetScancodeFromKeySym(syms[0], key);
        }
    }

    return scancode;
}

static void Wayland_ReconcileModifiers(struct SDL_WaylandInput *input)
{
    // Handle pressed modifiers for virtual keyboards that may not send keystrokes.
    if (input->keyboard_is_virtual) {
        if (input->xkb.wl_pressed_modifiers & input->xkb.idx_shift) {
            input->pressed_modifiers |= SDL_KMOD_SHIFT;
        } else {
            input->pressed_modifiers &= ~SDL_KMOD_SHIFT;
        }

        if (input->xkb.wl_pressed_modifiers & input->xkb.idx_ctrl) {
            input->pressed_modifiers |= SDL_KMOD_CTRL;
        } else {
            input->pressed_modifiers &= ~SDL_KMOD_CTRL;
        }

        if (input->xkb.wl_pressed_modifiers & input->xkb.idx_alt) {
            input->pressed_modifiers |= SDL_KMOD_ALT;
        } else {
            input->pressed_modifiers &= ~SDL_KMOD_ALT;
        }

        if (input->xkb.wl_pressed_modifiers & input->xkb.idx_gui) {
            input->pressed_modifiers |= SDL_KMOD_GUI;
        } else {
            input->pressed_modifiers &= ~SDL_KMOD_GUI;
        }

        if (input->xkb.wl_pressed_modifiers & input->xkb.idx_mode) {
            input->pressed_modifiers |= SDL_KMOD_MODE;
        } else {
            input->pressed_modifiers &= ~SDL_KMOD_MODE;
        }
    }

    /*
     * If a latch or lock was activated by a keypress, the latch/lock will
     * be tied to the specific left/right key that initiated it. Otherwise,
     * the ambiguous left/right combo is used.
     *
     * The modifier will remain active until the latch/lock is released by
     * the system.
     */
    if (input->xkb.wl_locked_modifiers & input->xkb.idx_shift) {
        if (input->pressed_modifiers & SDL_KMOD_SHIFT) {
            input->locked_modifiers &= ~SDL_KMOD_SHIFT;
            input->locked_modifiers |= (input->pressed_modifiers & SDL_KMOD_SHIFT);
        } else if (!(input->locked_modifiers & SDL_KMOD_SHIFT)) {
            input->locked_modifiers |= SDL_KMOD_SHIFT;
        }
    } else {
        input->locked_modifiers &= ~SDL_KMOD_SHIFT;
    }

    if (input->xkb.wl_locked_modifiers & input->xkb.idx_ctrl) {
        if (input->pressed_modifiers & SDL_KMOD_CTRL) {
            input->locked_modifiers &= ~SDL_KMOD_CTRL;
            input->locked_modifiers |= (input->pressed_modifiers & SDL_KMOD_CTRL);
        } else if (!(input->locked_modifiers & SDL_KMOD_CTRL)) {
            input->locked_modifiers |= SDL_KMOD_CTRL;
        }
    } else {
        input->locked_modifiers &= ~SDL_KMOD_CTRL;
    }

    if (input->xkb.wl_locked_modifiers & input->xkb.idx_alt) {
        if (input->pressed_modifiers & SDL_KMOD_ALT) {
            input->locked_modifiers &= ~SDL_KMOD_ALT;
            input->locked_modifiers |= (input->pressed_modifiers & SDL_KMOD_ALT);
        } else if (!(input->locked_modifiers & SDL_KMOD_ALT)) {
            input->locked_modifiers |= SDL_KMOD_ALT;
        }
    } else {
        input->locked_modifiers &= ~SDL_KMOD_ALT;
    }

    if (input->xkb.wl_locked_modifiers & input->xkb.idx_gui) {
        if (input->pressed_modifiers & SDL_KMOD_GUI) {
            input->locked_modifiers &= ~SDL_KMOD_GUI;
            input->locked_modifiers |= (input->pressed_modifiers & SDL_KMOD_GUI);
        } else if (!(input->locked_modifiers & SDL_KMOD_GUI)) {
            input->locked_modifiers |= SDL_KMOD_GUI;
        }
    } else {
        input->locked_modifiers &= ~SDL_KMOD_GUI;
    }

    if (input->xkb.wl_locked_modifiers & input->xkb.idx_mode) {
        input->locked_modifiers |= SDL_KMOD_MODE;
    } else {
        input->locked_modifiers &= ~SDL_KMOD_MODE;
    }

    // Capslock and Numlock can only be locked, not pressed.
    if (input->xkb.wl_locked_modifiers & input->xkb.idx_caps) {
        input->locked_modifiers |= SDL_KMOD_CAPS;
    } else {
        input->locked_modifiers &= ~SDL_KMOD_CAPS;
    }

    if (input->xkb.wl_locked_modifiers & input->xkb.idx_num) {
        input->locked_modifiers |= SDL_KMOD_NUM;
    } else {
        input->locked_modifiers &= ~SDL_KMOD_NUM;
    }

    SDL_SetModState(input->pressed_modifiers | input->locked_modifiers);
}

static void Wayland_HandleModifierKeys(struct SDL_WaylandInput *input, SDL_Scancode scancode, bool pressed)
{
    const SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
    SDL_Keymod mod;

    switch (keycode) {
    case SDLK_LSHIFT:
        mod = SDL_KMOD_LSHIFT;
        break;
    case SDLK_RSHIFT:
        mod = SDL_KMOD_RSHIFT;
        break;
    case SDLK_LCTRL:
        mod = SDL_KMOD_LCTRL;
        break;
    case SDLK_RCTRL:
        mod = SDL_KMOD_RCTRL;
        break;
    case SDLK_LALT:
        mod = SDL_KMOD_LALT;
        break;
    case SDLK_RALT:
        mod = SDL_KMOD_RALT;
        break;
    case SDLK_LGUI:
        mod = SDL_KMOD_LGUI;
        break;
    case SDLK_RGUI:
        mod = SDL_KMOD_RGUI;
        break;
    case SDLK_MODE:
        mod = SDL_KMOD_MODE;
        break;
    default:
        return;
    }

    if (pressed) {
        input->pressed_modifiers |= mod;
    } else {
        input->pressed_modifiers &= ~mod;
    }

    Wayland_ReconcileModifiers(input);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window;
    uint32_t *key;

    if (!surface) {
        // enter event for a window we've just destroyed
        return;
    }

    window = Wayland_GetWindowDataForOwnedSurface(surface);

    if (!window) {
        return;
    }

    input->keyboard_focus = window;
    window->keyboard_device = input;

    // Restore the keyboard focus to the child popup that was holding it
    SDL_SetKeyboardFocus(window->keyboard_focus ? window->keyboard_focus : window->sdlwindow);

#ifdef SDL_USE_IME
    if (!input->text_input) {
        SDL_IME_SetFocus(true);
    }
#endif

    window->last_focus_event_time_ns = SDL_GetTicksNS();

    wl_array_for_each (key, keys) {
        const SDL_Scancode scancode = Wayland_get_scancode_from_key(input, *key + 8);
        const SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);

        switch (keycode) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        case SDLK_LCTRL:
        case SDLK_RCTRL:
        case SDLK_LALT:
        case SDLK_RALT:
        case SDLK_LGUI:
        case SDLK_RGUI:
        case SDLK_MODE:
            Wayland_HandleModifierKeys(input, scancode, true);
            SDL_SendKeyboardKeyIgnoreModifiers(0, input->keyboard_id, *key, scancode, true);
            break;
        default:
            break;
        }
    }
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *wind;
    SDL_Window *window = NULL;

    if (!surface) {
        return;
    }

    wind = Wayland_GetWindowDataForOwnedSurface(surface);
    if (!wind) {
        return;
    }

    wind->keyboard_device = NULL;
    window = wind->sdlwindow;

    // Stop key repeat before clearing keyboard focus
    keyboard_repeat_clear(&input->keyboard_repeat);

    // This will release any keys still pressed
    SDL_SetKeyboardFocus(NULL);
    input->keyboard_focus = NULL;

    // Clear the pressed modifiers.
    input->pressed_modifiers = SDL_KMOD_NONE;

#ifdef SDL_USE_IME
    if (!input->text_input) {
        SDL_IME_SetFocus(false);
    }
#endif

    /* If the surface had a pointer leave event while still having active touch events, it retained mouse focus.
     * Clear it now if all touch events are raised.
     */
    if (!input->pointer_focus && SDL_GetMouseFocus() == window && !Wayland_SurfaceHasActiveTouches(surface)) {
        SDL_SetMouseFocus(NULL);
    }
}

static bool keyboard_input_get_text(char text[8], const struct SDL_WaylandInput *input, uint32_t key, bool down, bool *handled_by_ime)
{
    SDL_WindowData *window = input->keyboard_focus;
    const xkb_keysym_t *syms;
    xkb_keysym_t sym;

    if (!window || window->keyboard_device != input || !input->xkb.state) {
        return false;
    }

    // TODO: Can this happen?
    if (WAYLAND_xkb_state_key_get_syms(input->xkb.state, key + 8, &syms) != 1) {
        return false;
    }
    sym = syms[0];

#ifdef SDL_USE_IME
    if (SDL_IME_ProcessKeyEvent(sym, key + 8, down)) {
        if (handled_by_ime) {
            *handled_by_ime = true;
        }
        return true;
    }
#endif

    if (!down) {
        return false;
    }

    if (input->xkb.compose_state && WAYLAND_xkb_compose_state_feed(input->xkb.compose_state, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
        switch (WAYLAND_xkb_compose_state_get_status(input->xkb.compose_state)) {
        case XKB_COMPOSE_COMPOSING:
            if (handled_by_ime) {
                *handled_by_ime = true;
            }
            return true;
        case XKB_COMPOSE_CANCELLED:
        default:
            sym = XKB_KEY_NoSymbol;
            break;
        case XKB_COMPOSE_NOTHING:
            break;
        case XKB_COMPOSE_COMPOSED:
            sym = WAYLAND_xkb_compose_state_get_one_sym(input->xkb.compose_state);
            break;
        }
    }

    return WAYLAND_xkb_keysym_to_utf8(sym, text, 8) > 0;
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time, uint32_t key,
                                uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;
    enum wl_keyboard_key_state state = state_w;
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    char text[8];
    bool has_text = false;
    bool handled_by_ime = false;
    const Uint64 timestamp_raw_ns = Wayland_GetKeyboardTimestampRaw(input, time);

    Wayland_UpdateImplicitGrabSerial(input, serial);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        SDL_Window *keyboard_focus = SDL_GetKeyboardFocus();
        if (keyboard_focus && SDL_TextInputActive(keyboard_focus)) {
            has_text = keyboard_input_get_text(text, input, key, true, &handled_by_ime);
        }
    } else {
        if (keyboard_repeat_key_is_set(&input->keyboard_repeat, key)) {
            /* Send any due key repeat events before stopping the repeat and generating the key up event.
             * Compute time based on the Wayland time, as it reports when the release event happened.
             * Using SDL_GetTicks would be wrong, as it would report when the release event is processed,
             * which may be off if the application hasn't pumped events for a while.
             */
            keyboard_repeat_handle(&input->keyboard_repeat, timestamp_raw_ns - input->keyboard_repeat.wl_press_time_ns);
            keyboard_repeat_clear(&input->keyboard_repeat);
        }
        keyboard_input_get_text(text, input, key, false, &handled_by_ime);
    }

    scancode = Wayland_get_scancode_from_key(input, key + 8);
    Wayland_HandleModifierKeys(input, scancode, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    SDL_SendKeyboardKeyIgnoreModifiers(Wayland_GetKeyboardTimestamp(input, time), input->keyboard_id, key, scancode, (state == WL_KEYBOARD_KEY_STATE_PRESSED));

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (has_text && !(SDL_GetModState() & SDL_KMOD_CTRL)) {
            if (!handled_by_ime) {
                SDL_SendKeyboardText(text);
            }
        }
        if (input->xkb.keymap && WAYLAND_xkb_keymap_key_repeats(input->xkb.keymap, key + 8)) {
            keyboard_repeat_set(&input->keyboard_repeat, input->keyboard_id, key, timestamp_raw_ns, scancode, has_text, text);
        }
    }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group)
{
    struct SDL_WaylandInput *input = data;

    if (input->xkb.state == NULL) {
        /* if we get a modifier notification before the keymap, there's nothing we can do with the information
        */
        return;
    }

    WAYLAND_xkb_state_update_mask(input->xkb.state, mods_depressed, mods_latched,
                                  mods_locked, 0, 0, group);

    input->xkb.wl_pressed_modifiers = mods_depressed;
    input->xkb.wl_locked_modifiers = mods_latched | mods_locked;

    Wayland_ReconcileModifiers(input);

    // If a key is repeating, update the text to apply the modifier.
    if (keyboard_repeat_is_set(&input->keyboard_repeat)) {
        char text[8];
        const uint32_t key = keyboard_repeat_get_key(&input->keyboard_repeat);

        if (keyboard_input_get_text(text, input, key, true, NULL)) {
            keyboard_repeat_set_text(&input->keyboard_repeat, text);
        }
    }

    if (group == input->xkb.current_group) {
        return;
    }

    // The layout changed, remap and fire an event. Virtual keyboards use the default keymap.
    input->xkb.current_group = group;
    Wayland_UpdateKeymap(input);
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    struct SDL_WaylandInput *input = data;
    input->keyboard_repeat.repeat_rate = SDL_clamp(rate, 0, 1000);
    input->keyboard_repeat.repeat_delay_ms = delay;
    input->keyboard_repeat.is_initialized = true;
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_handle_repeat_info, // Version 4
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps)
{
    struct SDL_WaylandInput *input = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
        input->pointer = wl_seat_get_pointer(seat);
        SDL_memset(&input->pointer_curr_axis_info, 0, sizeof(input->pointer_curr_axis_info));
        input->display->pointer = input->pointer;

        Wayland_CreateCursorShapeDevice(input);

        wl_pointer_set_user_data(input->pointer, input);
        wl_pointer_add_listener(input->pointer, &pointer_listener, input);

        input->pointer_id = SDL_GetNextObjectID();
        SDL_AddMouse(input->pointer_id, WAYLAND_DEFAULT_POINTER_NAME, true);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
        if (input->cursor_shape) {
            wp_cursor_shape_device_v1_destroy(input->cursor_shape);
            input->cursor_shape = NULL;
        }
        wl_pointer_destroy(input->pointer);
        input->pointer = NULL;
        input->display->pointer = NULL;

        SDL_RemoveMouse(input->pointer_id, true);
        input->pointer_id = 0;
    }

    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !input->touch) {
        input->touch = wl_seat_get_touch(seat);
        SDL_AddTouch((SDL_TouchID)(uintptr_t)input->touch, SDL_TOUCH_DEVICE_DIRECT, "wayland_touch");
        wl_touch_set_user_data(input->touch, input);
        wl_touch_add_listener(input->touch, &touch_listener,
                              input);
    } else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && input->touch) {
        SDL_DelTouch((SDL_TouchID)(intptr_t)input->touch);
        wl_touch_destroy(input->touch);
        input->touch = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
        input->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_set_user_data(input->keyboard, input);
        wl_keyboard_add_listener(input->keyboard, &keyboard_listener,
                                 input);

        input->keyboard_id = SDL_GetNextObjectID();
        SDL_AddKeyboard(input->keyboard_id, WAYLAND_DEFAULT_KEYBOARD_NAME, true);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
        wl_keyboard_destroy(input->keyboard);
        input->keyboard = NULL;

        SDL_RemoveKeyboard(input->keyboard_id, true);
        input->keyboard_id = 0;
    }

    Wayland_RegisterTimestampListeners(input);
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
    // unimplemented
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name, // Version 2
};

static void data_source_handle_target(void *data, struct wl_data_source *wl_data_source,
                                      const char *mime_type)
{
}

static void data_source_handle_send(void *data, struct wl_data_source *wl_data_source,
                                    const char *mime_type, int32_t fd)
{
    Wayland_data_source_send((SDL_WaylandDataSource *)data, mime_type, fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *wl_data_source)
{
    SDL_WaylandDataSource *source = data;
    if (source) {
        Wayland_data_source_destroy(source);
    }
}

static void data_source_handle_dnd_drop_performed(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_dnd_finished(void *data, struct wl_data_source *wl_data_source)
{
}

static void data_source_handle_action(void *data, struct wl_data_source *wl_data_source,
                                      uint32_t dnd_action)
{
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_handle_target,
    data_source_handle_send,
    data_source_handle_cancelled,
    data_source_handle_dnd_drop_performed, // Version 3
    data_source_handle_dnd_finished,       // Version 3
    data_source_handle_action,             // Version 3
};

static void primary_selection_source_send(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1,
                                          const char *mime_type, int32_t fd)
{
    Wayland_primary_selection_source_send((SDL_WaylandPrimarySelectionSource *)data,
                                          mime_type, fd);
}

static void primary_selection_source_cancelled(void *data, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
    Wayland_primary_selection_source_destroy(data);
}

static const struct zwp_primary_selection_source_v1_listener primary_selection_source_listener = {
    primary_selection_source_send,
    primary_selection_source_cancelled,
};

SDL_WaylandDataSource *Wayland_data_source_create(SDL_VideoDevice *_this)
{
    SDL_WaylandDataSource *data_source = NULL;
    SDL_VideoData *driver_data = NULL;
    struct wl_data_source *id = NULL;

    if (!_this || !_this->internal) {
        SDL_SetError("Video driver uninitialized");
    } else {
        driver_data = _this->internal;

        if (driver_data->data_device_manager) {
            id = wl_data_device_manager_create_data_source(
                driver_data->data_device_manager);
        }

        if (!id) {
            SDL_SetError("Wayland unable to create data source");
        } else {
            data_source = SDL_calloc(1, sizeof(*data_source));
            if (!data_source) {
                wl_data_source_destroy(id);
            } else {
                data_source->source = id;
                wl_data_source_set_user_data(id, data_source);
                wl_data_source_add_listener(id, &data_source_listener,
                                            data_source);
            }
        }
    }
    return data_source;
}

SDL_WaylandPrimarySelectionSource *Wayland_primary_selection_source_create(SDL_VideoDevice *_this)
{
    SDL_WaylandPrimarySelectionSource *primary_selection_source = NULL;
    SDL_VideoData *driver_data = NULL;
    struct zwp_primary_selection_source_v1 *id = NULL;

    if (!_this || !_this->internal) {
        SDL_SetError("Video driver uninitialized");
    } else {
        driver_data = _this->internal;

        if (driver_data->primary_selection_device_manager) {
            id = zwp_primary_selection_device_manager_v1_create_source(
                driver_data->primary_selection_device_manager);
        }

        if (!id) {
            SDL_SetError("Wayland unable to create primary selection source");
        } else {
            primary_selection_source = SDL_calloc(1, sizeof(*primary_selection_source));
            if (!primary_selection_source) {
                zwp_primary_selection_source_v1_destroy(id);
            } else {
                primary_selection_source->source = id;
                zwp_primary_selection_source_v1_add_listener(id, &primary_selection_source_listener,
                                                             primary_selection_source);
            }
        }
    }
    return primary_selection_source;
}

static void data_offer_handle_offer(void *data, struct wl_data_offer *wl_data_offer,
                                    const char *mime_type)
{
    SDL_WaylandDataOffer *offer = data;
    Wayland_data_offer_add_mime(offer, mime_type);
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_offer on data_offer 0x%08x for MIME '%s'\n",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 mime_type);
}

static void data_offer_handle_source_actions(void *data, struct wl_data_offer *wl_data_offer,
                                             uint32_t source_actions)
{
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_source_actions on data_offer 0x%08x for Source Actions '%d'\n",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 source_actions);
}

static void data_offer_handle_actions(void *data, struct wl_data_offer *wl_data_offer,
                                      uint32_t dnd_action)
{
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In wl_data_offer_listener . data_offer_handle_actions on data_offer 0x%08x for DND Actions '%d'\n",
                 (wl_data_offer ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)wl_data_offer) : -1),
                 dnd_action);
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_handle_source_actions, // Version 3
    data_offer_handle_actions,        // Version 3
};

static void primary_selection_offer_handle_offer(void *data, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1,
                                                 const char *mime_type)
{
    SDL_WaylandPrimarySelectionOffer *offer = data;
    Wayland_primary_selection_offer_add_mime(offer, mime_type);
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_offer_v1_listener . primary_selection_offer_handle_offer on primary_selection_offer 0x%08x for MIME '%s'\n",
                 (zwp_primary_selection_offer_v1 ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)zwp_primary_selection_offer_v1) : -1),
                 mime_type);
}

static const struct zwp_primary_selection_offer_v1_listener primary_selection_offer_listener = {
    primary_selection_offer_handle_offer,
};

static void data_device_handle_data_offer(void *data, struct wl_data_device *wl_data_device,
                                          struct wl_data_offer *id)
{
    SDL_WaylandDataOffer *data_offer = SDL_calloc(1, sizeof(*data_offer));
    if (data_offer) {
        data_offer->offer = id;
        data_offer->data_device = data;
        WAYLAND_wl_list_init(&(data_offer->mimes));
        wl_data_offer_set_user_data(id, data_offer);
        wl_data_offer_add_listener(id, &data_offer_listener, data_offer);
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_data_offer on data_offer 0x%08x\n",
                     (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
    }
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_data_device,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id)
{
    SDL_WaylandDataDevice *data_device = data;
    data_device->has_mime_file = false;
    data_device->has_mime_text = false;
    uint32_t dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

    data_device->drag_serial = serial;

    if (id) {
        data_device->drag_offer = wl_data_offer_get_user_data(id);

        // TODO: SDL Support more mime types
#ifdef SDL_USE_LIBDBUS
        if (Wayland_data_offer_has_mime(data_device->drag_offer, FILE_PORTAL_MIME)) {
            data_device->has_mime_file = true;
            wl_data_offer_accept(id, serial, FILE_PORTAL_MIME);
        }
#endif
        if (Wayland_data_offer_has_mime(data_device->drag_offer, FILE_MIME)) {
            data_device->has_mime_file = true;
            wl_data_offer_accept(id, serial, FILE_MIME);
        }

        if (Wayland_data_offer_has_mime(data_device->drag_offer, TEXT_MIME)) {
            data_device->has_mime_text = true;
            wl_data_offer_accept(id, serial, TEXT_MIME);
        }

        // SDL only supports "copy" style drag and drop
        if (data_device->has_mime_file || data_device->has_mime_text) {
            dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
        } else {
            // drag_mime is NULL this will decline the offer
            wl_data_offer_accept(id, serial, NULL);
        }
        if (wl_data_offer_get_version(data_device->drag_offer->offer) >=
            WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
            wl_data_offer_set_actions(data_device->drag_offer->offer,
                                      dnd_action, dnd_action);
        }

        // find the current window
        if (surface) {
            SDL_WindowData *window = Wayland_GetWindowDataForOwnedSurface(surface);
            if (window) {
                data_device->dnd_window = window->sdlwindow;
                const float dx = (float)wl_fixed_to_double(x);
                const float dy = (float)wl_fixed_to_double(y);
                SDL_SendDropPosition(data_device->dnd_window, dx, dy);
                SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                             ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d into window %d for serial %d\n",
                             WAYLAND_wl_proxy_get_id((struct wl_proxy *)id),
                             wl_fixed_to_int(x), wl_fixed_to_int(y), SDL_GetWindowID(data_device->dnd_window), serial);
            } else {
                data_device->dnd_window = NULL;
                SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                             ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d for serial %d\n",
                             WAYLAND_wl_proxy_get_id((struct wl_proxy *)id),
                             wl_fixed_to_int(x), wl_fixed_to_int(y), serial);
            }
        } else {
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d for serial %d\n",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)id),
                         wl_fixed_to_int(x), wl_fixed_to_int(y), serial);
        }
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_enter on data_offer 0x%08x at %d x %d for serial %d\n",
                     -1, wl_fixed_to_int(x), wl_fixed_to_int(y), serial);
    }
}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_data_device)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer) {
        if (data_device->dnd_window) {
            SDL_SendDropComplete(data_device->dnd_window);
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x from window %d for serial %d\n",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                         SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);
        } else {
            SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                         ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x for serial %d\n",
                         WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                         data_device->drag_serial);
        }
        Wayland_data_offer_destroy(data_device->drag_offer);
        data_device->drag_offer = NULL;
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_leave on data_offer 0x%08x for serial %d\n",
                     -1, -1);
    }
    data_device->has_mime_file = false;
    data_device->has_mime_text = false;
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_data_device,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window && (data_device->has_mime_file || data_device->has_mime_text)) {
        const float dx = (float)wl_fixed_to_double(x);
        const float dy = (float)wl_fixed_to_double(y);

        /* XXX: Send the filename here if the event system ever starts passing it though.
         *      Any future implementation should cache the filenames, as otherwise this could
         *      hammer the DBus interface hundreds or even thousands of times per second.
         */
        SDL_SendDropPosition(data_device->dnd_window, dx, dy);
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_motion on data_offer 0x%08x at %d x %d in window %d serial %d\n",
                     WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                     wl_fixed_to_int(x), wl_fixed_to_int(y),
                     SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_motion on data_offer 0x%08x at %d x %d serial %d\n",
                     -1, wl_fixed_to_int(x), wl_fixed_to_int(y), -1);
    }
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_data_device)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window && (data_device->has_mime_file || data_device->has_mime_text)) {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_drop on data_offer 0x%08x in window %d serial %d\n",
                     WAYLAND_wl_proxy_get_id((struct wl_proxy *)data_device->drag_offer->offer),
                     SDL_GetWindowID(data_device->dnd_window), data_device->drag_serial);
        // TODO: SDL Support more mime types
        size_t length;
        bool drop_handled = false;
#ifdef SDL_USE_LIBDBUS
        if (Wayland_data_offer_has_mime(data_device->drag_offer, FILE_PORTAL_MIME)) {
            void *buffer = Wayland_data_offer_receive(data_device->drag_offer,
                                                      FILE_PORTAL_MIME, &length);
            if (buffer) {
                SDL_DBusContext *dbus = SDL_DBus_GetContext();
                if (dbus) {
                    int path_count = 0;
                    char **paths = SDL_DBus_DocumentsPortalRetrieveFiles(buffer, &path_count);
                    // If dropped files contain a directory the list is empty
                    if (paths && path_count > 0) {
                        int i;
                        for (i = 0; i < path_count; i++) {
                            SDL_SendDropFile(data_device->dnd_window, NULL, paths[i]);
                        }
                        dbus->free_string_array(paths);
                        SDL_SendDropComplete(data_device->dnd_window);
                        drop_handled = true;
                    }
                }
                SDL_free(buffer);
            }
        }
#endif
        /* If XDG document portal fails fallback.
         * When running a flatpak sandbox this will most likely be a list of
         * non paths that are not visible to the application
         */
        if (!drop_handled) {
            const char *mime_type = data_device->has_mime_file ? FILE_MIME : (data_device->has_mime_text ? TEXT_MIME : "");
            void *buffer = Wayland_data_offer_receive(data_device->drag_offer,
                                                      mime_type, &length);
            if (data_device->has_mime_file) {
                if (buffer) {
                    char *saveptr = NULL;
                    char *token = SDL_strtok_r((char *)buffer, "\r\n", &saveptr);
                    while (token) {
                        if (SDL_URIToLocal(token, token) >= 0) {
                            SDL_SendDropFile(data_device->dnd_window, NULL, token);
                        }
                        token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                    }
                    SDL_free(buffer);
                    SDL_SendDropComplete(data_device->dnd_window);
                } else {
                    SDL_SendDropComplete(data_device->dnd_window);
                }
                drop_handled = true;
            } else if (data_device->has_mime_text) {
                if (buffer) {
                    char *saveptr = NULL;
                    char *token = SDL_strtok_r((char *)buffer, "\r\n", &saveptr);
                    while (token) {
                        SDL_SendDropText(data_device->dnd_window, token);
                        token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                    }
                    SDL_free(buffer);
                    SDL_SendDropComplete(data_device->dnd_window);
                } else {
                    /* Even though there has been a valid data offer,
                     *  and there have been valid Enter, Motion, and Drop callbacks,
                     *  Wayland_data_offer_receive may return an empty buffer,
                     *  because the data is actually in the primary selection device,
                     *  not in the data device.
                     */
                    SDL_SendDropComplete(data_device->dnd_window);
                }
                drop_handled = true;
            }
        }

        if (drop_handled && wl_data_offer_get_version(data_device->drag_offer->offer) >= WL_DATA_OFFER_FINISH_SINCE_VERSION) {
            wl_data_offer_finish(data_device->drag_offer->offer);
        }
    } else {
        SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                     ". In wl_data_device_listener . data_device_handle_drop on data_offer 0x%08x serial %d\n",
                     -1, -1);
    }

    Wayland_data_offer_destroy(data_device->drag_offer);
    data_device->drag_offer = NULL;
}

static void notifyFromMimes(struct wl_list *mimes) {
    int nformats = 0;
    char **new_mime_types = NULL;
    if (mimes) {
        nformats = WAYLAND_wl_list_length(mimes);
        size_t alloc_size = (nformats + 1) * sizeof(char *);

        /* do a first pass to compute allocation size */
        SDL_MimeDataList *item = NULL;
        wl_list_for_each(item, mimes, link) {
            alloc_size += SDL_strlen(item->mime_type) + 1;
        }

        new_mime_types = SDL_AllocateTemporaryMemory(alloc_size);
        if (!new_mime_types) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT, "unable to allocate new_mime_types");
            return;
        }

        /* second pass to fill*/
        char *strPtr = (char *)(new_mime_types + nformats + 1);
        item = NULL;
        int i = 0;
        wl_list_for_each(item, mimes, link) {
            new_mime_types[i] = strPtr;
            strPtr = stpcpy(strPtr, item->mime_type) + 1;
            i++;
        }
        new_mime_types[nformats] = NULL;
    }

    SDL_SendClipboardUpdate(false, new_mime_types, nformats);
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_data_device,
                                         struct wl_data_offer *id)
{
    SDL_WaylandDataDevice *data_device = data;
    SDL_WaylandDataOffer *offer = NULL;

    if (id) {
        offer = wl_data_offer_get_user_data(id);
    }

    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In data_device_listener . data_device_handle_selection on data_offer 0x%08x\n",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
    if (data_device->selection_offer != offer) {
        Wayland_data_offer_destroy(data_device->selection_offer);
        data_device->selection_offer = offer;
    }

    notifyFromMimes(offer ? &offer->mimes : NULL);
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_enter,
    data_device_handle_leave,
    data_device_handle_motion,
    data_device_handle_drop,
    data_device_handle_selection
};

static void primary_selection_device_handle_offer(void *data, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1,
                                                  struct zwp_primary_selection_offer_v1 *id)
{
    SDL_WaylandPrimarySelectionOffer *primary_selection_offer = SDL_calloc(1, sizeof(*primary_selection_offer));
    if (primary_selection_offer) {
        primary_selection_offer->offer = id;
        primary_selection_offer->primary_selection_device = data;
        WAYLAND_wl_list_init(&(primary_selection_offer->mimes));
        zwp_primary_selection_offer_v1_set_user_data(id, primary_selection_offer);
        zwp_primary_selection_offer_v1_add_listener(id, &primary_selection_offer_listener, primary_selection_offer);
    }
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_device_v1_listener . primary_selection_device_handle_offer on primary_selection_offer 0x%08x\n",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));
}

static void primary_selection_device_handle_selection(void *data, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1,
                                                      struct zwp_primary_selection_offer_v1 *id)
{
    SDL_WaylandPrimarySelectionDevice *primary_selection_device = data;
    SDL_WaylandPrimarySelectionOffer *offer = NULL;

    if (id) {
        offer = zwp_primary_selection_offer_v1_get_user_data(id);
    }

    if (primary_selection_device->selection_offer != offer) {
        Wayland_primary_selection_offer_destroy(primary_selection_device->selection_offer);
        primary_selection_device->selection_offer = offer;
    }
    SDL_LogTrace(SDL_LOG_CATEGORY_INPUT,
                 ". In zwp_primary_selection_device_v1_listener . primary_selection_device_handle_selection on primary_selection_offer 0x%08x\n",
                 (id ? WAYLAND_wl_proxy_get_id((struct wl_proxy *)id) : -1));

    notifyFromMimes(offer ? &offer->mimes : NULL);
}

static const struct zwp_primary_selection_device_v1_listener primary_selection_device_listener = {
    primary_selection_device_handle_offer,
    primary_selection_device_handle_selection
};

static void text_input_enter(void *data,
                             struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    // No-op
}

static void text_input_leave(void *data,
                             struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    // No-op
}

static void text_input_preedit_string(void *data,
                                      struct zwp_text_input_v3 *zwp_text_input_v3,
                                      const char *text,
                                      int32_t cursor_begin,
                                      int32_t cursor_end)
{
    SDL_WaylandTextInput *text_input = data;
    text_input->has_preedit = true;
    if (text) {
        int cursor_begin_utf8 = cursor_begin >= 0 ? (int)SDL_utf8strnlen(text, cursor_begin) : -1;
        int cursor_end_utf8 = cursor_end >= 0 ? (int)SDL_utf8strnlen(text, cursor_end) : -1;
        int cursor_size_utf8;
        if (cursor_end_utf8 >= 0) {
            if (cursor_begin_utf8 >= 0) {
                cursor_size_utf8 = cursor_end_utf8 - cursor_begin_utf8;
            } else {
                cursor_size_utf8 = cursor_end_utf8;
            }
        } else {
            cursor_size_utf8 = -1;
        }
        SDL_SendEditingText(text, cursor_begin_utf8, cursor_size_utf8);
    } else {
        SDL_SendEditingText("", 0, 0);
    }
}

static void text_input_commit_string(void *data,
                                     struct zwp_text_input_v3 *zwp_text_input_v3,
                                     const char *text)
{
    SDL_SendKeyboardText(text);
}

static void text_input_delete_surrounding_text(void *data,
                                               struct zwp_text_input_v3 *zwp_text_input_v3,
                                               uint32_t before_length,
                                               uint32_t after_length)
{
    // FIXME: Do we care about this event?
}

static void text_input_done(void *data,
                            struct zwp_text_input_v3 *zwp_text_input_v3,
                            uint32_t serial)
{
    SDL_WaylandTextInput *text_input = data;
    if (!text_input->has_preedit) {
        SDL_SendEditingText("", 0, 0);
    }
    text_input->has_preedit = false;
}

static const struct zwp_text_input_v3_listener text_input_listener = {
    text_input_enter,
    text_input_leave,
    text_input_preedit_string,
    text_input_commit_string,
    text_input_delete_surrounding_text,
    text_input_done
};

void Wayland_create_data_device(SDL_VideoData *d)
{
    SDL_WaylandDataDevice *data_device = NULL;

    if (!d->input->seat) {
        // No seat yet, will be initialized later.
        return;
    }

    data_device = SDL_calloc(1, sizeof(*data_device));
    if (!data_device) {
        return;
    }

    data_device->data_device = wl_data_device_manager_get_data_device(
        d->data_device_manager, d->input->seat);
    data_device->video_data = d;

    if (!data_device->data_device) {
        SDL_free(data_device);
    } else {
        wl_data_device_set_user_data(data_device->data_device, data_device);
        wl_data_device_add_listener(data_device->data_device,
                                    &data_device_listener, data_device);
        d->input->data_device = data_device;
    }
}

void Wayland_create_primary_selection_device(SDL_VideoData *d)
{
    SDL_WaylandPrimarySelectionDevice *primary_selection_device = NULL;

    if (!d->input->seat) {
        // No seat yet, will be initialized later.
        return;
    }

    primary_selection_device = SDL_calloc(1, sizeof(*primary_selection_device));
    if (!primary_selection_device) {
        return;
    }

    primary_selection_device->primary_selection_device = zwp_primary_selection_device_manager_v1_get_device(
        d->primary_selection_device_manager, d->input->seat);
    primary_selection_device->video_data = d;

    if (!primary_selection_device->primary_selection_device) {
        SDL_free(primary_selection_device);
    } else {
        zwp_primary_selection_device_v1_set_user_data(primary_selection_device->primary_selection_device,
                                                      primary_selection_device);
        zwp_primary_selection_device_v1_add_listener(primary_selection_device->primary_selection_device,
                                                     &primary_selection_device_listener, primary_selection_device);
        d->input->primary_selection_device = primary_selection_device;
    }
}

void Wayland_create_text_input(SDL_VideoData *d)
{
    SDL_WaylandTextInput *text_input = NULL;

    if (!d->input->seat) {
        // No seat yet, will be initialized later.
        return;
    }

    text_input = SDL_calloc(1, sizeof(*text_input));
    if (!text_input) {
        return;
    }

    text_input->text_input = zwp_text_input_manager_v3_get_text_input(
        d->text_input_manager, d->input->seat);

    if (!text_input->text_input) {
        SDL_free(text_input);
    } else {
        zwp_text_input_v3_set_user_data(text_input->text_input, text_input);
        zwp_text_input_v3_add_listener(text_input->text_input,
                                       &text_input_listener, text_input);
        d->input->text_input = text_input;
    }
}


// Pen/Tablet support...

typedef struct SDL_WaylandPenTool  // a stylus, etc, on a tablet.
{
    SDL_PenID instance_id;
    SDL_PenInfo info;
    SDL_Window *tool_focus;
    struct zwp_tablet_tool_v2 *wltool;
    float x;
    float y;
    bool frame_motion_set;
    float frame_axes[SDL_PEN_AXIS_COUNT];
    Uint32 frame_axes_set;
    int frame_pen_down;
    int frame_buttons[3];
} SDL_WaylandPenTool;

static void tablet_tool_handle_type(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t type)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    switch (type) {
        #define CASE(typ) case ZWP_TABLET_TOOL_V2_TYPE_##typ: sdltool->info.subtype = SDL_PEN_TYPE_##typ; return
        CASE(ERASER);
        CASE(PEN);
        CASE(PENCIL);
        CASE(AIRBRUSH);
        CASE(BRUSH);
        #undef CASE
        default: sdltool->info.subtype = SDL_PEN_TYPE_UNKNOWN;  // we'll decline to add this when the `done` event comes through.
    }
}

static void tablet_tool_handle_hardware_serial(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial_hi, uint32_t serial_lo)
{
    // don't care about this atm.
}

static void tablet_tool_handle_hardware_id_wacom(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t id_hi, uint32_t id_lo)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->info.wacom_id = id_lo;
}

static void tablet_tool_handle_capability(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t capability)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    switch (capability) {
        #define CASE(wltyp,sdltyp) case ZWP_TABLET_TOOL_V2_CAPABILITY_##wltyp: sdltool->info.capabilities |= sdltyp; return
        CASE(TILT, SDL_PEN_CAPABILITY_XTILT | SDL_PEN_CAPABILITY_YTILT);
        CASE(PRESSURE, SDL_PEN_CAPABILITY_PRESSURE);
        CASE(DISTANCE, SDL_PEN_CAPABILITY_DISTANCE);
        CASE(ROTATION, SDL_PEN_CAPABILITY_ROTATION);
        CASE(SLIDER, SDL_PEN_CAPABILITY_SLIDER);
        #undef CASE
        default: break;  // unsupported here.
    }
}

static void tablet_tool_handle_done(void *data, struct zwp_tablet_tool_v2 *tool)
{
}

static void tablet_tool_handle_removed(void *data, struct zwp_tablet_tool_v2 *tool)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    if (sdltool->instance_id) {
        SDL_RemovePenDevice(0, sdltool->instance_id);
    }
    zwp_tablet_tool_v2_destroy(tool);
    SDL_free(sdltool);
}

static void tablet_tool_handle_proximity_in(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial, struct zwp_tablet_v2 *tablet, struct wl_surface *surface)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    SDL_WindowData *windowdata = surface ? Wayland_GetWindowDataForOwnedSurface(surface) : NULL;
    sdltool->tool_focus = windowdata ? windowdata->sdlwindow : NULL;

    SDL_assert(sdltool->instance_id == 0);  // shouldn't be added at this point.
    if (sdltool->info.subtype != SDL_PEN_TYPE_UNKNOWN) {   // don't tell SDL about it if we don't know its role.
        sdltool->instance_id = SDL_AddPenDevice(0, NULL, &sdltool->info, sdltool);
    }

    // According to the docs, this should be followed by a motion event, where we'll send our SDL events.
}

static void tablet_tool_handle_proximity_out(void *data, struct zwp_tablet_tool_v2 *tool)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->tool_focus = NULL;

    if (sdltool->instance_id) {
        SDL_RemovePenDevice(0, sdltool->instance_id);
        sdltool->instance_id = 0;
    }
}

static void tablet_tool_handle_down(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_pen_down = 1;
}

static void tablet_tool_handle_up(void *data, struct zwp_tablet_tool_v2 *tool)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_pen_down = 0;
}

static void tablet_tool_handle_motion(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    SDL_Window *window = sdltool->tool_focus;
    if (window) {
        const SDL_WindowData *windowdata = window->internal;
        sdltool->x = (float)(wl_fixed_to_double(sx_w) * windowdata->pointer_scale.x);
        sdltool->y = (float)(wl_fixed_to_double(sy_w) * windowdata->pointer_scale.y);
        sdltool->frame_motion_set = true;
    }
}

static void tablet_tool_handle_pressure(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t pressure)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_axes[SDL_PEN_AXIS_PRESSURE] = ((float) pressure) / 65535.0f;
    sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_PRESSURE);
    if (pressure) {
        sdltool->frame_axes[SDL_PEN_AXIS_DISTANCE] = 0.0f;
        sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_DISTANCE);
    }
}

static void tablet_tool_handle_distance(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t distance)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_axes[SDL_PEN_AXIS_DISTANCE] = ((float) distance) / 65535.0f;
    sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_DISTANCE);
    if (distance) {
        sdltool->frame_axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
        sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_PRESSURE);
    }
}

static void tablet_tool_handle_tilt(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t xtilt, wl_fixed_t ytilt)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_axes[SDL_PEN_AXIS_XTILT] = (float)(wl_fixed_to_double(xtilt));
    sdltool->frame_axes[SDL_PEN_AXIS_YTILT] = (float)(wl_fixed_to_double(ytilt));
    sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_XTILT) | (1u << SDL_PEN_AXIS_YTILT);
}

static void tablet_tool_handle_button(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial, uint32_t button, uint32_t state)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    int sdlbutton;

    switch (button) {
    // see %{_includedir}/linux/input-event-codes.h
    case 0x14b: // BTN_STYLUS
        sdlbutton = 1;
        break;
    case 0x14c: // BTN_STYLUS2
        sdlbutton = 2;
        break;
    case 0x149: // BTN_STYLUS3
        sdlbutton = 3;
        break;
    default:
        return;  // don't care about this button, I guess.
    }

    SDL_assert((sdlbutton >= 1) && (sdlbutton <= SDL_arraysize(sdltool->frame_buttons)));
    sdltool->frame_buttons[sdlbutton-1] = (state == ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED) ? 1 : 0;
}

static void tablet_tool_handle_rotation(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t degrees)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    const float rotation = (float)(wl_fixed_to_double(degrees));
    sdltool->frame_axes[SDL_PEN_AXIS_ROTATION] = (rotation > 180.0f) ? (rotation - 360.0f) : rotation;  // map to -180.0f ... 179.0f range
    sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_ROTATION);
}

static void tablet_tool_handle_slider(void *data, struct zwp_tablet_tool_v2 *tool, int32_t position)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;
    sdltool->frame_axes[SDL_PEN_AXIS_SLIDER] = position / 65535.f;
    sdltool->frame_axes_set |= (1u << SDL_PEN_AXIS_SLIDER);
}

static void tablet_tool_handle_wheel(void *data, struct zwp_tablet_tool_v2 *tool, int32_t degrees, int32_t clicks)
{
    // not supported at the moment
}

static void tablet_tool_handle_frame(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t time)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) data;

    if (!sdltool->instance_id) {
        return;  // Not a pen we report on.
    }

    const Uint64 timestamp = Wayland_GetEventTimestamp(SDL_MS_TO_NS(time));
    const SDL_PenID instance_id = sdltool->instance_id;
    const SDL_Window *window = sdltool->tool_focus;

    // I don't know if this is necessary (or makes sense), but send motion before pen downs, but after pen ups, so you don't get unexpected lines drawn.
    if (sdltool->frame_motion_set && (sdltool->frame_pen_down != -1)) {
        if (sdltool->frame_pen_down) {
            SDL_SendPenMotion(timestamp, instance_id, window, sdltool->x, sdltool->y);
            SDL_SendPenTouch(timestamp, instance_id, window, false, true);  // !!! FIXME: how do we know what tip is in use?
        } else {
            SDL_SendPenTouch(timestamp, instance_id, window, false, false); // !!! FIXME: how do we know what tip is in use?
            SDL_SendPenMotion(timestamp, instance_id, window, sdltool->x, sdltool->y);
        }
    } else {
        if (sdltool->frame_pen_down != -1) {
            SDL_SendPenTouch(timestamp, instance_id, window, false, (sdltool->frame_pen_down != 0));  // !!! FIXME: how do we know what tip is in use?
        }

        if (sdltool->frame_motion_set) {
            SDL_SendPenMotion(timestamp, instance_id, window, sdltool->x, sdltool->y);
        }
    }

    for (SDL_PenAxis i = 0; i < SDL_PEN_AXIS_COUNT; i++) {
        if (sdltool->frame_axes_set & (1u << i)) {
            SDL_SendPenAxis(timestamp, instance_id, window, i, sdltool->frame_axes[i]);
        }
    }

    for (int i = 0; i < SDL_arraysize(sdltool->frame_buttons); i++) {
        const int state = sdltool->frame_buttons[i];
        if (state != -1) {
            SDL_SendPenButton(timestamp, instance_id, window, (Uint8)(i + 1), (state != 0));
            sdltool->frame_buttons[i] = -1;
        }
    }

    // reset for next frame.
    sdltool->frame_pen_down = -1;
    sdltool->frame_motion_set = false;
    sdltool->frame_axes_set = 0;
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
    tablet_tool_handle_type,
    tablet_tool_handle_hardware_serial,
    tablet_tool_handle_hardware_id_wacom,
    tablet_tool_handle_capability,
    tablet_tool_handle_done,
    tablet_tool_handle_removed,
    tablet_tool_handle_proximity_in,
    tablet_tool_handle_proximity_out,
    tablet_tool_handle_down,
    tablet_tool_handle_up,
    tablet_tool_handle_motion,
    tablet_tool_handle_pressure,
    tablet_tool_handle_distance,
    tablet_tool_handle_tilt,
    tablet_tool_handle_rotation,
    tablet_tool_handle_slider,
    tablet_tool_handle_wheel,
    tablet_tool_handle_button,
    tablet_tool_handle_frame
};


static void tablet_seat_handle_tablet_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_v2 *tablet)
{
    // don't care atm.
}

static void tablet_seat_handle_tool_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_tool_v2 *tool)
{
    SDL_WaylandPenTool *sdltool = SDL_calloc(1, sizeof(*sdltool));

    if (sdltool) {  // if allocation failed, oh well, we won't report this device.
        sdltool->wltool = tool;
        sdltool->info.max_tilt = -1.0f;
        sdltool->info.num_buttons = -1;
        sdltool->frame_pen_down = -1;
        for (int i = 0; i < SDL_arraysize(sdltool->frame_buttons); i++) {
            sdltool->frame_buttons[i] = -1;
        }

        // this will send a bunch of zwp_tablet_tool_v2 events right up front to tell
        // us device details, with a "done" event to let us know we have everything.
        zwp_tablet_tool_v2_add_listener(tool, &tablet_tool_listener, sdltool);
    }
}

static void tablet_seat_handle_pad_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_pad_v2 *pad)
{
    // we don't care atm.
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    tablet_seat_handle_tablet_added,
    tablet_seat_handle_tool_added,
    tablet_seat_handle_pad_added
};

void Wayland_input_init_tablet_support(struct SDL_WaylandInput *input, struct zwp_tablet_manager_v2 *tablet_manager)
{
    if (!tablet_manager || !input->seat) {
        return;
    }

    SDL_WaylandTabletInput *tablet_input = SDL_calloc(1, sizeof(*tablet_input));
    if (!tablet_input) {
        return;
    }

    tablet_input->input = input;
    tablet_input->seat = zwp_tablet_manager_v2_get_tablet_seat(tablet_manager, input->seat);

    zwp_tablet_seat_v2_add_listener(tablet_input->seat, &tablet_seat_listener, tablet_input);
}

static void Wayland_remove_all_pens_callback(SDL_PenID instance_id, void *handle, void *userdata)
{
    SDL_WaylandPenTool *sdltool = (SDL_WaylandPenTool *) handle;
    zwp_tablet_tool_v2_destroy(sdltool->wltool);
    SDL_free(sdltool);
}

void Wayland_input_quit_tablet_support(struct SDL_WaylandInput *input)
{
    SDL_RemoveAllPenDevices(Wayland_remove_all_pens_callback, NULL);

    if (input && input->tablet_input) {
        zwp_tablet_seat_v2_destroy(input->tablet_input->seat);
        SDL_free(input->tablet_input);
        input->tablet_input = NULL;
    }
}

void Wayland_input_initialize_seat(SDL_VideoData *d)
{
    struct SDL_WaylandInput *input = d->input;

    WAYLAND_wl_list_init(&touch_points);

    if (d->data_device_manager) {
        Wayland_create_data_device(d);
    }
    if (d->primary_selection_device_manager) {
        Wayland_create_primary_selection_device(d);
    }
    if (d->text_input_manager) {
        Wayland_create_text_input(d);
    }

    wl_seat_add_listener(input->seat, &seat_listener, input);
    wl_seat_set_user_data(input->seat, input);

    if (d->tablet_manager) {
        Wayland_input_init_tablet_support(d->input, d->tablet_manager);
    }

    WAYLAND_wl_display_flush(d->display);
}

void Wayland_display_destroy_input(SDL_VideoData *d)
{
    struct SDL_WaylandInput *input = d->input;

    if (input->keyboard_timestamps) {
        zwp_input_timestamps_v1_destroy(input->keyboard_timestamps);
    }
    if (input->pointer_timestamps) {
        zwp_input_timestamps_v1_destroy(input->pointer_timestamps);
    }
    if (input->touch_timestamps) {
        zwp_input_timestamps_v1_destroy(input->touch_timestamps);
    }

    if (input->data_device) {
        Wayland_data_device_clear_selection(input->data_device);
        if (input->data_device->selection_offer) {
            Wayland_data_offer_destroy(input->data_device->selection_offer);
        }
        if (input->data_device->drag_offer) {
            Wayland_data_offer_destroy(input->data_device->drag_offer);
        }
        if (input->data_device->data_device) {
            if (wl_data_device_get_version(input->data_device->data_device) >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
                wl_data_device_release(input->data_device->data_device);
            } else {
                wl_data_device_destroy(input->data_device->data_device);
            }
        }
        SDL_free(input->data_device);
    }

    if (input->primary_selection_device) {
        if (input->primary_selection_device->selection_offer) {
            Wayland_primary_selection_offer_destroy(input->primary_selection_device->selection_offer);
        }
        if (input->primary_selection_device->selection_source) {
            Wayland_primary_selection_source_destroy(input->primary_selection_device->selection_source);
        }
        if (input->primary_selection_device->primary_selection_device) {
            zwp_primary_selection_device_v1_destroy(input->primary_selection_device->primary_selection_device);
        }
        SDL_free(input->primary_selection_device);
    }

    if (input->text_input) {
        zwp_text_input_v3_destroy(input->text_input->text_input);
        SDL_free(input->text_input);
    }

    if (input->keyboard) {
        if (wl_keyboard_get_version(input->keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
            wl_keyboard_release(input->keyboard);
        } else {
            wl_keyboard_destroy(input->keyboard);
        }
    }

    if (input->cursor_shape) {
        wp_cursor_shape_device_v1_destroy(input->cursor_shape);
    }

    if (input->pointer) {
        if (wl_pointer_get_version(input->pointer) >= WL_POINTER_RELEASE_SINCE_VERSION) {
            wl_pointer_release(input->pointer);
        } else {
            wl_pointer_destroy(input->pointer);
        }
    }

    if (input->touch) {
        struct SDL_WaylandTouchPoint *tp, *tmp;

        SDL_DelTouch(1);
        if (wl_touch_get_version(input->touch) >= WL_TOUCH_RELEASE_SINCE_VERSION) {
            wl_touch_release(input->touch);
        } else {
            wl_touch_destroy(input->touch);
        }

        wl_list_for_each_safe (tp, tmp, &touch_points, link) {
            WAYLAND_wl_list_remove(&tp->link);
            SDL_free(tp);
        }
    }

    if (input->tablet_input) {
        Wayland_input_quit_tablet_support(input);
    }

    if (input->seat) {
        if (wl_seat_get_version(input->seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
            wl_seat_release(input->seat);
        } else {
            wl_seat_destroy(input->seat);
        }
    }

    if (input->xkb.compose_state) {
        WAYLAND_xkb_compose_state_unref(input->xkb.compose_state);
    }

    if (input->xkb.compose_table) {
        WAYLAND_xkb_compose_table_unref(input->xkb.compose_table);
    }

    if (input->xkb.state) {
        WAYLAND_xkb_state_unref(input->xkb.state);
    }

    if (input->xkb.keymap) {
        WAYLAND_xkb_keymap_unref(input->xkb.keymap);
    }

    SDL_free(input);
    d->input = NULL;
}

static void relative_pointer_handle_relative_motion(void *data,
                                                    struct zwp_relative_pointer_v1 *pointer,
                                                    uint32_t time_hi,
                                                    uint32_t time_lo,
                                                    wl_fixed_t dx_w,
                                                    wl_fixed_t dy_w,
                                                    wl_fixed_t dx_unaccel_w,
                                                    wl_fixed_t dy_unaccel_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_VideoData *d = input->display;
    SDL_WindowData *window = input->pointer_focus;
    double dx_unaccel;
    double dy_unaccel;

    // Relative pointer event times are in microsecond granularity.
    const Uint64 timestamp = SDL_US_TO_NS(((Uint64)time_hi << 32) | (Uint64)time_lo);

    dx_unaccel = wl_fixed_to_double(dx_unaccel_w);
    dy_unaccel = wl_fixed_to_double(dy_unaccel_w);

    if (input->pointer_focus && d->relative_mouse_mode) {
        SDL_SendMouseMotion(Wayland_GetEventTimestamp(timestamp), window->sdlwindow, input->pointer_id, true, (float)dx_unaccel, (float)dy_unaccel);
    }
}

static const struct zwp_relative_pointer_v1_listener relative_pointer_listener = {
    relative_pointer_handle_relative_motion,
};

static void locked_pointer_locked(void *data,
                                  struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static void locked_pointer_unlocked(void *data,
                                    struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static const struct zwp_locked_pointer_v1_listener locked_pointer_listener = {
    locked_pointer_locked,
    locked_pointer_unlocked,
};

bool Wayland_input_lock_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    SDL_WindowData *w = window->internal;
    SDL_VideoData *d = input->display;

    if (!d->pointer_constraints || !input->pointer) {
        return false;
    }

    if (!w->locked_pointer) {
        if (w->confined_pointer) {
            // If the pointer is already confined to the surface, the lock will fail with a protocol error.
            Wayland_input_unconfine_pointer(input, window);
        }

        w->locked_pointer = zwp_pointer_constraints_v1_lock_pointer(d->pointer_constraints,
                                                                    w->surface,
                                                                    input->pointer,
                                                                    NULL,
                                                                    ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
        zwp_locked_pointer_v1_add_listener(w->locked_pointer,
                                           &locked_pointer_listener,
                                           window);
    }

    return true;
}

bool Wayland_input_unlock_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    SDL_WindowData *w = window->internal;

    if (w->locked_pointer) {
        zwp_locked_pointer_v1_destroy(w->locked_pointer);
        w->locked_pointer = NULL;
    }

    // Restore existing pointer confinement.
    Wayland_input_confine_pointer(input, window);

    return true;
}

static void pointer_confine_destroy(SDL_Window *window)
{
    SDL_WindowData *w = window->internal;
    if (w->confined_pointer) {
        zwp_confined_pointer_v1_destroy(w->confined_pointer);
        w->confined_pointer = NULL;
    }
}

bool Wayland_input_enable_relative_pointer(struct SDL_WaylandInput *input)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = input->display;
    SDL_Window *window;
    struct zwp_relative_pointer_v1 *relative_pointer;

    if (!d->relative_pointer_manager) {
        return false;
    }

    if (!d->pointer_constraints) {
        return false;
    }

    if (!input->pointer) {
        return false;
    }

    /* If we have a pointer confine active, we must destroy it here because
     * creating a locked pointer otherwise would be a protocol error.
     */
    for (window = vd->windows; window; window = window->next) {
        pointer_confine_destroy(window);
    }

    if (!input->relative_pointer) {
        relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(d->relative_pointer_manager, input->pointer);
        zwp_relative_pointer_v1_add_listener(relative_pointer,
                                             &relative_pointer_listener,
                                             input);
        input->relative_pointer = relative_pointer;
    }

    for (window = vd->windows; window; window = window->next) {
        Wayland_input_lock_pointer(input, window);
    }

    d->relative_mouse_mode = 1;

    return true;
}

bool Wayland_input_disable_relative_pointer(struct SDL_WaylandInput *input)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = input->display;
    SDL_Window *window;

    for (window = vd->windows; window; window = window->next) {
        Wayland_input_unlock_pointer(input, window);
    }

    if (input->relative_pointer) {
        zwp_relative_pointer_v1_destroy(input->relative_pointer);
        input->relative_pointer = NULL;
    }

    d->relative_mouse_mode = 0;

    for (window = vd->windows; window; window = window->next) {
        Wayland_input_confine_pointer(input, window);
    }

    return true;
}

static void confined_pointer_confined(void *data,
                                      struct zwp_confined_pointer_v1 *confined_pointer)
{
}

static void confined_pointer_unconfined(void *data,
                                        struct zwp_confined_pointer_v1 *confined_pointer)
{
}

static const struct zwp_confined_pointer_v1_listener confined_pointer_listener = {
    confined_pointer_confined,
    confined_pointer_unconfined,
};

bool Wayland_input_confine_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    SDL_WindowData *w = window->internal;
    SDL_VideoData *d = input->display;
    struct zwp_confined_pointer_v1 *confined_pointer;
    struct wl_region *confine_rect;

    if (!d->pointer_constraints) {
        return SDL_SetError("Failed to confine pointer: compositor lacks support for the required zwp_pointer_constraints_v1 protocol");
    }

    if (!input->pointer) {
        return SDL_SetError("No pointer to confine");
    }

    /* A confine may already be active, in which case we should destroy it and
     * create a new one.
     */
    pointer_confine_destroy(window);

    /* We cannot create a confine if the pointer is already locked. Defer until
     * the pointer is unlocked.
     */
    if (d->relative_mouse_mode) {
        return true;
    }

    // Don't confine the pointer if it shouldn't be confined.
    if (SDL_RectEmpty(&window->mouse_rect) && !(window->flags & SDL_WINDOW_MOUSE_GRABBED)) {
        return true;
    }

    if (SDL_RectEmpty(&window->mouse_rect)) {
        confine_rect = NULL;
    } else {
        SDL_Rect scaled_mouse_rect;

        scaled_mouse_rect.x = (int)SDL_floor(window->mouse_rect.x / w->pointer_scale.x);
        scaled_mouse_rect.y = (int)SDL_floor(window->mouse_rect.y / w->pointer_scale.y);
        scaled_mouse_rect.w = (int)SDL_ceil(window->mouse_rect.w / w->pointer_scale.x);
        scaled_mouse_rect.h = (int)SDL_ceil(window->mouse_rect.h / w->pointer_scale.y);

        confine_rect = wl_compositor_create_region(d->compositor);
        wl_region_add(confine_rect,
                      scaled_mouse_rect.x,
                      scaled_mouse_rect.y,
                      scaled_mouse_rect.w,
                      scaled_mouse_rect.h);
    }

    confined_pointer =
        zwp_pointer_constraints_v1_confine_pointer(d->pointer_constraints,
                                                   w->surface,
                                                   input->pointer,
                                                   confine_rect,
                                                   ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_confined_pointer_v1_add_listener(confined_pointer,
                                         &confined_pointer_listener,
                                         window);

    if (confine_rect) {
        wl_region_destroy(confine_rect);
    }

    w->confined_pointer = confined_pointer;
    return true;
}

bool Wayland_input_unconfine_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    pointer_confine_destroy(window);
    return true;
}

bool Wayland_input_grab_keyboard(SDL_Window *window, struct SDL_WaylandInput *input)
{
    SDL_WindowData *w = window->internal;
    SDL_VideoData *d = input->display;

    if (!d->key_inhibitor_manager) {
        return SDL_SetError("Failed to grab keyboard: compositor lacks support for the required zwp_keyboard_shortcuts_inhibit_manager_v1 protocol");
    }

    if (w->key_inhibitor) {
        return true;
    }

    w->key_inhibitor =
        zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(d->key_inhibitor_manager,
                                                                    w->surface,
                                                                    input->seat);

    return true;
}

bool Wayland_input_ungrab_keyboard(SDL_Window *window)
{
    SDL_WindowData *w = window->internal;

    if (w->key_inhibitor) {
        zwp_keyboard_shortcuts_inhibitor_v1_destroy(w->key_inhibitor);
        w->key_inhibitor = NULL;
    }

    return true;
}

void Wayland_UpdateImplicitGrabSerial(struct SDL_WaylandInput *input, Uint32 serial)
{
    if (serial > input->last_implicit_grab_serial) {
        input->last_implicit_grab_serial = serial;
        Wayland_data_device_set_serial(input->data_device, serial);
        Wayland_primary_selection_device_set_serial(input->primary_selection_device, serial);
    }
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
