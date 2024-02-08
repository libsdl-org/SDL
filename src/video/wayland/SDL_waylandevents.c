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
#include "tablet-unstable-v2-client-protocol.h"
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
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include "../../events/imKStoUCS.h"
#include "../../events/SDL_keysym_to_scancode_c.h"

/* Weston uses a ratio of 10 units per scroll tick */
#define WAYLAND_WHEEL_AXIS_UNIT 10

/* xkbcommon as of 1.4.1 doesn't have a name macro for the mode key */
#ifndef XKB_MOD_NAME_MODE
#define XKB_MOD_NAME_MODE "Mod5"
#endif


struct SDL_WaylandTouchPoint
{
    SDL_TouchID id;
    wl_fixed_t fx;
    wl_fixed_t fy;
    struct wl_surface *surface;

    struct wl_list link;
};

static struct wl_list touch_points;

static char *Wayland_URIToLocal(char *uri);

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

static SDL_bool Wayland_SurfaceHasActiveTouches(struct wl_surface *surface)
{
    struct SDL_WaylandTouchPoint *tp;

    wl_list_for_each (tp, &touch_points, link) {
        if (tp->surface == surface) {
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

static Uint64 Wayland_GetEventTimestamp(Uint64 nsTimestamp)
{
    static Uint64 last;
    static Uint64 timestamp_offset;
    const Uint64 now = SDL_GetTicksNS();

    if (nsTimestamp < last) {
        /* 32-bit timer rollover, bump the offset */
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

/* Returns SDL_TRUE if a key repeat event was due */
static SDL_bool keyboard_repeat_handle(SDL_WaylandKeyboardRepeat *repeat_info, Uint64 elapsed)
{
    SDL_bool ret = SDL_FALSE;
    while (elapsed >= repeat_info->next_repeat_ns) {
        if (repeat_info->scancode != SDL_SCANCODE_UNKNOWN) {
            const Uint64 timestamp = repeat_info->wl_press_time_ns + repeat_info->next_repeat_ns;
            SDL_SendKeyboardKeyIgnoreModifiers(Wayland_GetEventTimestamp(timestamp), SDL_PRESSED, repeat_info->scancode);
        }
        if (repeat_info->text[0]) {
            SDL_SendKeyboardText(repeat_info->text);
        }
        repeat_info->next_repeat_ns += SDL_NS_PER_SECOND / (Uint64)repeat_info->repeat_rate;
        ret = SDL_TRUE;
    }
    return ret;
}

static void keyboard_repeat_clear(SDL_WaylandKeyboardRepeat *repeat_info)
{
    if (!repeat_info->is_initialized) {
        return;
    }
    repeat_info->is_key_down = SDL_FALSE;
}

static void keyboard_repeat_set(SDL_WaylandKeyboardRepeat *repeat_info, uint32_t key, Uint64 wl_press_time_ns,
                                uint32_t scancode, SDL_bool has_text, char text[8])
{
    if (!repeat_info->is_initialized || !repeat_info->repeat_rate) {
        return;
    }
    repeat_info->is_key_down = SDL_TRUE;
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

static SDL_bool keyboard_repeat_is_set(SDL_WaylandKeyboardRepeat *repeat_info)
{
    return repeat_info->is_initialized && repeat_info->is_key_down;
}

static SDL_bool keyboard_repeat_key_is_set(SDL_WaylandKeyboardRepeat *repeat_info, uint32_t key)
{
    return repeat_info->is_initialized && repeat_info->is_key_down && key == repeat_info->key;
}

static void sync_done_handler(void *data, struct wl_callback *callback, uint32_t callback_data)
{
    /* Nothing to do, just destroy the callback */
    wl_callback_destroy(callback);
}

static struct wl_callback_listener sync_listener = {
    sync_done_handler
};

void Wayland_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *d = _this->driverdata;

    /* Queue a sync event to unblock the event queue fd if it's empty and being waited on.
     * TODO: Maybe use a pipe to avoid the compositor roundtrip?
     */
    struct wl_callback *cb = wl_display_sync(d->display);
    wl_callback_add_listener(cb, &sync_listener, NULL);
    WAYLAND_wl_display_flush(d->display);
}

static int dispatch_queued_events(SDL_VideoData *viddata)
{
    int ret;

    /*
     * NOTE: When reconnection is implemented, check if libdecor needs to be
     *       involved in the reconnection process.
     */
#ifdef HAVE_LIBDECOR_H
    if (viddata->shell.libdecor) {
        libdecor_dispatch(viddata->shell.libdecor, 0);
    }
#endif

    ret = WAYLAND_wl_display_dispatch_pending(viddata->display);
    return ret >= 0 ? 1 : ret;
}

int Wayland_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS)
{
    SDL_VideoData *d = _this->driverdata;
    struct SDL_WaylandInput *input = d->input;
    SDL_bool key_repeat_active = SDL_FALSE;

    WAYLAND_wl_display_flush(d->display);

#ifdef SDL_USE_IME
    if (!d->text_input_manager && SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
        SDL_IME_PumpEvents();
    }
#endif

#ifdef SDL_USE_LIBDBUS
    SDL_DBus_PumpEvents();
#endif

    /* If key repeat is active, we'll need to cap our maximum wait time to handle repeats */
    if (input && keyboard_repeat_is_set(&input->keyboard_repeat)) {
        const Uint64 elapsed = SDL_GetTicksNS() - input->keyboard_repeat.sdl_press_time_ns;
        if (keyboard_repeat_handle(&input->keyboard_repeat, elapsed)) {
            /* A repeat key event was already due */
            return 1;
        } else {
            const Uint64 next_repeat_wait_time = (input->keyboard_repeat.next_repeat_ns - elapsed) + 1;
            if (timeoutNS >= 0) {
                timeoutNS = SDL_min(timeoutNS, next_repeat_wait_time);
            } else {
                timeoutNS = next_repeat_wait_time;
            }
            key_repeat_active = SDL_TRUE;
        }
    }

    /* wl_display_prepare_read() will return -1 if the default queue is not empty.
     * If the default queue is empty, it will prepare us for our SDL_IOReady() call. */
    if (WAYLAND_wl_display_prepare_read(d->display) == 0) {
        /* Use SDL_IOR_NO_RETRY to ensure SIGINT will break us out of our wait */
        int err = SDL_IOReady(WAYLAND_wl_display_get_fd(d->display), SDL_IOR_READ | SDL_IOR_NO_RETRY, timeoutNS);
        if (err > 0) {
            /* There are new events available to read */
            WAYLAND_wl_display_read_events(d->display);
            return dispatch_queued_events(d);
        } else if (err == 0) {
            /* No events available within the timeout */
            WAYLAND_wl_display_cancel_read(d->display);

            /* If key repeat is active, we might have woken up to generate a key event */
            if (key_repeat_active) {
                const Uint64 elapsed = SDL_GetTicksNS() - input->keyboard_repeat.sdl_press_time_ns;
                if (keyboard_repeat_handle(&input->keyboard_repeat, elapsed)) {
                    return 1;
                }
            }

            return 0;
        } else {
            /* Error returned from poll()/select() */
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
        /* We already had pending events */
        return dispatch_queued_events(d);
    }
}

void Wayland_PumpEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *d = _this->driverdata;
    struct SDL_WaylandInput *input = d->input;
    int err;

#ifdef SDL_USE_IME
    if (!d->text_input_manager && SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
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

    /* Dispatch any pre-existing pending events or new events we may have read */
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
        float sx = (float)(wl_fixed_to_double(sx_w) * window_data->pointer_scale.x);
        float sy = (float)(wl_fixed_to_double(sy_w) * window_data->pointer_scale.y);
        SDL_SendMouseMotion(Wayland_GetPointerTimestamp(input, time), window_data->sdlwindow, 0, 0, sx, sy);
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
        /* enter event for a window we've just destroyed */
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
            /* Clear the capture flag and raise all buttons */
            wind->sdlwindow->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;

            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, 0, SDL_RELEASED, SDL_BUTTON_LEFT);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, 0, SDL_RELEASED, SDL_BUTTON_RIGHT);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, 0, SDL_RELEASED, SDL_BUTTON_MIDDLE);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, 0, SDL_RELEASED, SDL_BUTTON_X1);
            SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, 0), wind->sdlwindow, 0, SDL_RELEASED, SDL_BUTTON_X2);
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

static SDL_bool ProcessHitTest(SDL_WindowData *window_data,
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
            if (window_data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
                if (window_data->shell_surface.libdecor.frame) {
                    libdecor_frame_move(window_data->shell_surface.libdecor.frame,
                                        seat,
                                        serial);
                }
            } else
#endif
                if (window_data->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
                if (window_data->shell_surface.xdg.roleobj.toplevel) {
                    xdg_toplevel_move(window_data->shell_surface.xdg.roleobj.toplevel,
                                      seat,
                                      serial);
                }
            }
            return SDL_TRUE;

        case SDL_HITTEST_RESIZE_TOPLEFT:
        case SDL_HITTEST_RESIZE_TOP:
        case SDL_HITTEST_RESIZE_TOPRIGHT:
        case SDL_HITTEST_RESIZE_RIGHT:
        case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
        case SDL_HITTEST_RESIZE_BOTTOM:
        case SDL_HITTEST_RESIZE_BOTTOMLEFT:
        case SDL_HITTEST_RESIZE_LEFT:
#ifdef HAVE_LIBDECOR_H
            if (window_data->shell_surface_type == WAYLAND_SURFACE_LIBDECOR) {
                if (window_data->shell_surface.libdecor.frame) {
                    libdecor_frame_resize(window_data->shell_surface.libdecor.frame,
                                          seat,
                                          serial,
                                          directions_libdecor[window_data->hit_test_result - SDL_HITTEST_RESIZE_TOPLEFT]);
                }
            } else
#endif
                if (window_data->shell_surface_type == WAYLAND_SURFACE_XDG_TOPLEVEL) {
                if (window_data->shell_surface.xdg.roleobj.toplevel) {
                    xdg_toplevel_resize(window_data->shell_surface.xdg.roleobj.toplevel,
                                        seat,
                                        serial,
                                        directions[window_data->hit_test_result - SDL_HITTEST_RESIZE_TOPLEFT]);
                }
            }
            return SDL_TRUE;

        default:
            return SDL_FALSE;
        }
    }

    return SDL_FALSE;
}

static void pointer_handle_button_common(struct SDL_WaylandInput *input, uint32_t serial,
                                         uint32_t time, uint32_t button, uint32_t state_w)
{
    SDL_WindowData *window = input->pointer_focus;
    enum wl_pointer_button_state state = state_w;
    uint32_t sdl_button;

    if (window) {
        SDL_VideoData *viddata = window->waylandData;
        switch (button) {
        case BTN_LEFT:
            sdl_button = SDL_BUTTON_LEFT;
            if (ProcessHitTest(input->pointer_focus, input->seat, input->sx_w, input->sy_w, serial)) {
                return; /* don't pass this event on to app. */
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

        /* Wayland won't let you "capture" the mouse, but it will
           automatically track the mouse outside the window if you
           drag outside of it, until you let go of all buttons (even
           if you add or remove presses outside the window, as long
           as any button is still down, the capture remains) */
        if (state) { /* update our mask of currently-pressed buttons */
            input->buttons_pressed |= SDL_BUTTON(sdl_button);
        } else {
            input->buttons_pressed &= ~(SDL_BUTTON(sdl_button));
        }

        /* Don't modify the capture flag in relative mode. */
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

        SDL_SendMouseButton(Wayland_GetPointerTimestamp(input, time), window->sdlwindow, 0,
                            state ? SDL_PRESSED : SDL_RELEASED, sdl_button);
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

        SDL_SendMouseWheel(Wayland_GetPointerTimestamp(input, time), window->sdlwindow, 0, x, y, SDL_MOUSEWHEEL_NORMAL);
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
                /* Only process continuous events if no discrete events have been received. */
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
                /* Only process continuous events if no discrete events have been received. */
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

    /* clear pointer_curr_axis_info for next frame */
    SDL_memset(&input->pointer_curr_axis_info, 0, sizeof(input->pointer_curr_axis_info));

    if (x != 0.0f || y != 0.0f) {
        SDL_SendMouseWheel(input->pointer_curr_axis_info.timestamp_ns,
                           window->sdlwindow, 0, x, y, direction);
    }
}

static void pointer_handle_axis_source(void *data, struct wl_pointer *pointer,
                                       uint32_t axis_source)
{
    /* unimplemented */
}

static void pointer_handle_axis_stop(void *data, struct wl_pointer *pointer,
                                     uint32_t time, uint32_t axis)
{
    /* unimplemented */
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
    pointer_handle_frame,                  /* Version 5 */
    pointer_handle_axis_source,            /* Version 5 */
    pointer_handle_axis_stop,              /* Version 5 */
    pointer_handle_axis_discrete,          /* Version 5 */
    pointer_handle_axis_value120,          /* Version 8 */
    pointer_handle_axis_relative_direction /* Version 9 */
};

static void touch_handler_down(void *data, struct wl_touch *touch, uint32_t serial,
                               uint32_t timestamp, struct wl_surface *surface,
                               int id, wl_fixed_t fx, wl_fixed_t fy)
{
    struct SDL_WaylandInput *input = (struct SDL_WaylandInput *)data;
    SDL_WindowData *window_data;

    /* Check that this surface is valid. */
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
            x = wl_fixed_to_double(fx) / (window_data->current.logical_width - 1);
        }
        if (window_data->current.logical_height <= 1) {
            y = 0.5f;
        } else {
            y = wl_fixed_to_double(fy) / (window_data->current.logical_height - 1);
        }

        SDL_SetMouseFocus(window_data->sdlwindow);

        SDL_SendTouch(Wayland_GetTouchTimestamp(input, timestamp), (SDL_TouchID)(uintptr_t)touch,
                      (SDL_FingerID)(id + 1), window_data->sdlwindow, SDL_TRUE, x, y, 1.0f);
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
            const float x = wl_fixed_to_double(fx) / window_data->current.logical_width;
            const float y = wl_fixed_to_double(fy) / window_data->current.logical_height;

            SDL_SendTouch(Wayland_GetTouchTimestamp(input, timestamp), (SDL_TouchID)(uintptr_t)touch,
                          (SDL_FingerID)(id + 1), window_data->sdlwindow, SDL_FALSE, x, y, 0.0f);

            /* If the seat lacks pointer focus, the seat's keyboard focus is another window or NULL, this window curently
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
            const float x = wl_fixed_to_double(fx) / window_data->current.logical_width;
            const float y = wl_fixed_to_double(fy) / window_data->current.logical_height;

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
}

static const struct wl_touch_listener touch_listener = {
    touch_handler_down,
    touch_handler_up,
    touch_handler_motion,
    touch_handler_frame,
    touch_handler_cancel,
    NULL, /* shape */
    NULL, /* orientation */
};

typedef struct Wayland_Keymap
{
    xkb_layout_index_t layout;
    SDL_Keycode keymap[SDL_NUM_SCANCODES];
} Wayland_Keymap;

static void Wayland_keymap_iter(struct xkb_keymap *keymap, xkb_keycode_t key, void *data)
{
    const xkb_keysym_t *syms;
    Wayland_Keymap *sdlKeymap = (Wayland_Keymap *)data;
    SDL_Scancode scancode;

    scancode = SDL_GetScancodeFromTable(SDL_SCANCODE_TABLE_XFREE86_2, (key - 8));
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return;
    }

    if (WAYLAND_xkb_keymap_key_get_syms_by_level(keymap, key, sdlKeymap->layout, 0, &syms) > 0) {
        uint32_t keycode = SDL_KeySymToUcs4(syms[0]);

        if (!keycode) {
            const SDL_Scancode sc = SDL_GetScancodeFromKeySym(syms[0], key);

            /* Note: The default SDL keymap always sets this to right alt instead of AltGr/Mode, so handle it separately. */
            if (syms[0] != XKB_KEY_ISO_Level3_Shift) {
                keycode = SDL_GetDefaultKeyFromScancode(sc);
            } else {
                keycode = SDLK_MODE;
            }
        }

        if (keycode) {
            sdlKeymap->keymap[scancode] = keycode;
        } else {
            switch (scancode) {
            case SDL_SCANCODE_RETURN:
                sdlKeymap->keymap[scancode] = SDLK_RETURN;
                break;
            case SDL_SCANCODE_ESCAPE:
                sdlKeymap->keymap[scancode] = SDLK_ESCAPE;
                break;
            case SDL_SCANCODE_BACKSPACE:
                sdlKeymap->keymap[scancode] = SDLK_BACKSPACE;
                break;
            case SDL_SCANCODE_TAB:
                sdlKeymap->keymap[scancode] = SDLK_TAB;
                break;
            case SDL_SCANCODE_DELETE:
                sdlKeymap->keymap[scancode] = SDLK_DELETE;
                break;
            default:
                sdlKeymap->keymap[scancode] = SDL_SCANCODE_TO_KEYCODE(scancode);
                break;
            }
        }
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

    /* Update the keymap if changed. Virtual keyboards use the default keymap. */
    if (input->xkb.current_group != XKB_GROUP_INVALID) {
        Wayland_Keymap keymap;
        keymap.layout = input->xkb.current_group;
        SDL_GetDefaultKeymap(keymap.keymap);
        if (!input->keyboard_is_virtual) {
            WAYLAND_xkb_keymap_key_for_each(input->xkb.keymap,
                                            Wayland_keymap_iter,
                                            &keymap);
        }
        SDL_SetKeymap(0, keymap.keymap, SDL_NUM_SCANCODES, SDL_TRUE);
    }

    /*
     * See https://blogs.s-osg.org/compose-key-support-weston/
     * for further explanation on dead keys in Wayland.
     */

    /* Look up the preferred locale, falling back to "C" as default */
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

    /* Set up XKB compose table */
    if (input->xkb.compose_table != NULL) {
        WAYLAND_xkb_compose_table_unref(input->xkb.compose_table);
        input->xkb.compose_table = NULL;
    }
    input->xkb.compose_table = WAYLAND_xkb_compose_table_new_from_locale(input->display->xkb_context,
                                                                         locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (input->xkb.compose_table) {
        /* Set up XKB compose state */
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
    /* Handle pressed modifiers for virtual keyboards that may not send keystrokes. */
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

    /* Capslock and Numlock can only be locked, not pressed. */
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

static void Wayland_HandleModifierKeys(struct SDL_WaylandInput *input, SDL_Scancode scancode, SDL_bool pressed)
{
    const SDL_KeyCode keycode = SDL_GetKeyFromScancode(scancode);
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
        /* enter event for a window we've just destroyed */
        return;
    }

    window = Wayland_GetWindowDataForOwnedSurface(surface);

    if (!window) {
        return;
    }

    input->keyboard_focus = window;
    window->keyboard_device = input;

    /* Restore the keyboard focus to the child popup that was holding it */
    SDL_SetKeyboardFocus(window->keyboard_focus ? window->keyboard_focus : window->sdlwindow);

#ifdef SDL_USE_IME
    if (!input->text_input) {
        SDL_IME_SetFocus(SDL_TRUE);
    }
#endif

    wl_array_for_each (key, keys) {
        const SDL_Scancode scancode = Wayland_get_scancode_from_key(input, *key + 8);
        const SDL_KeyCode keycode = SDL_GetKeyFromScancode(scancode);

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
            Wayland_HandleModifierKeys(input, scancode, SDL_TRUE);
            SDL_SendKeyboardKeyIgnoreModifiers(0, SDL_PRESSED, scancode);
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
    window->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;

    /* Stop key repeat before clearing keyboard focus */
    keyboard_repeat_clear(&input->keyboard_repeat);

    /* This will release any keys still pressed */
    SDL_SetKeyboardFocus(NULL);
    input->keyboard_focus = NULL;

    /* Clear the pressed modifiers. */
    input->pressed_modifiers = SDL_KMOD_NONE;

#ifdef SDL_USE_IME
    if (!input->text_input) {
        SDL_IME_SetFocus(SDL_FALSE);
    }
#endif

    /* If the surface had a pointer leave event while still having active touch events, it retained mouse focus.
     * Clear it now if all touch events are raised.
     */
    if (!input->pointer_focus && SDL_GetMouseFocus() == window && !Wayland_SurfaceHasActiveTouches(surface)) {
        SDL_SetMouseFocus(NULL);
    }
}

static SDL_bool keyboard_input_get_text(char text[8], const struct SDL_WaylandInput *input, uint32_t key, Uint8 state, SDL_bool *handled_by_ime)
{
    SDL_WindowData *window = input->keyboard_focus;
    const xkb_keysym_t *syms;
    xkb_keysym_t sym;

    if (!window || window->keyboard_device != input || !input->xkb.state) {
        return SDL_FALSE;
    }

    /* TODO: Can this happen? */
    if (WAYLAND_xkb_state_key_get_syms(input->xkb.state, key + 8, &syms) != 1) {
        return SDL_FALSE;
    }
    sym = syms[0];

#ifdef SDL_USE_IME
    if (SDL_IME_ProcessKeyEvent(sym, key + 8, state)) {
        if (handled_by_ime) {
            *handled_by_ime = SDL_TRUE;
        }
        return SDL_TRUE;
    }
#endif

    if (state == SDL_RELEASED) {
        return SDL_FALSE;
    }

    if (input->xkb.compose_state && WAYLAND_xkb_compose_state_feed(input->xkb.compose_state, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
        switch (WAYLAND_xkb_compose_state_get_status(input->xkb.compose_state)) {
        case XKB_COMPOSE_COMPOSING:
            if (handled_by_ime) {
                *handled_by_ime = SDL_TRUE;
            }
            return SDL_TRUE;
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
    SDL_bool has_text = SDL_FALSE;
    SDL_bool handled_by_ime = SDL_FALSE;
    const Uint64 timestamp_raw_ns = Wayland_GetKeyboardTimestampRaw(input, time);

    Wayland_UpdateImplicitGrabSerial(input, serial);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        has_text = keyboard_input_get_text(text, input, key, SDL_PRESSED, &handled_by_ime);
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
        keyboard_input_get_text(text, input, key, SDL_RELEASED, &handled_by_ime);
    }

    if (!handled_by_ime) {
        scancode = Wayland_get_scancode_from_key(input, key + 8);
        Wayland_HandleModifierKeys(input, scancode, state == WL_KEYBOARD_KEY_STATE_PRESSED);
        SDL_SendKeyboardKeyIgnoreModifiers(Wayland_GetKeyboardTimestamp(input, time), state == WL_KEYBOARD_KEY_STATE_PRESSED ? SDL_PRESSED : SDL_RELEASED, scancode);
    }

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (has_text && !(SDL_GetModState() & SDL_KMOD_CTRL)) {
            if (!handled_by_ime) {
                SDL_SendKeyboardText(text);
            }
        }
        if (input->xkb.keymap && WAYLAND_xkb_keymap_key_repeats(input->xkb.keymap, key + 8)) {
            keyboard_repeat_set(&input->keyboard_repeat, key, timestamp_raw_ns, scancode, has_text, text);
        }
    }
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group)
{
    struct SDL_WaylandInput *input = data;
    Wayland_Keymap keymap;

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

    /* If a key is repeating, update the text to apply the modifier. */
    if (keyboard_repeat_is_set(&input->keyboard_repeat)) {
        char text[8];
        const uint32_t key = keyboard_repeat_get_key(&input->keyboard_repeat);

        if (keyboard_input_get_text(text, input, key, SDL_PRESSED, NULL)) {
            keyboard_repeat_set_text(&input->keyboard_repeat, text);
        }
    }

    if (group == input->xkb.current_group) {
        return;
    }

    /* The layout changed, remap and fire an event. Virtual keyboards use the default keymap. */
    input->xkb.current_group = group;
    keymap.layout = group;
    SDL_GetDefaultKeymap(keymap.keymap);
    if (!input->keyboard_is_virtual) {
        WAYLAND_xkb_keymap_key_for_each(input->xkb.keymap,
                                        Wayland_keymap_iter,
                                        &keymap);
    }
    SDL_SetKeymap(0, keymap.keymap, SDL_NUM_SCANCODES, SDL_TRUE);
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                        int32_t rate, int32_t delay)
{
    struct SDL_WaylandInput *input = data;
    input->keyboard_repeat.repeat_rate = SDL_clamp(rate, 0, 1000);
    input->keyboard_repeat.repeat_delay_ms = delay;
    input->keyboard_repeat.is_initialized = SDL_TRUE;
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
    keyboard_handle_repeat_info, /* Version 4 */
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps)
{
    struct SDL_WaylandInput *input = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
        input->pointer = wl_seat_get_pointer(seat);
        SDL_memset(&input->pointer_curr_axis_info, 0, sizeof(input->pointer_curr_axis_info));
        input->display->pointer = input->pointer;
        wl_pointer_set_user_data(input->pointer, input);
        wl_pointer_add_listener(input->pointer, &pointer_listener,
                                input);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
        wl_pointer_destroy(input->pointer);
        input->pointer = NULL;
        input->display->pointer = NULL;
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
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
        wl_keyboard_destroy(input->keyboard);
        input->keyboard = NULL;
    }

    Wayland_RegisterTimestampListeners(input);
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name)
{
    /* unimplemented */
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name, /* Version 2 */
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
    data_source_handle_dnd_drop_performed, /* Version 3 */
    data_source_handle_dnd_finished,       /* Version 3 */
    data_source_handle_action,             /* Version 3 */
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

    if (!_this || !_this->driverdata) {
        SDL_SetError("Video driver uninitialized");
    } else {
        driver_data = _this->driverdata;

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

    if (!_this || !_this->driverdata) {
        SDL_SetError("Video driver uninitialized");
    } else {
        driver_data = _this->driverdata;

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
}

static void data_offer_handle_source_actions(void *data, struct wl_data_offer *wl_data_offer,
                                             uint32_t source_actions)
{
}

static void data_offer_handle_actions(void *data, struct wl_data_offer *wl_data_offer,
                                      uint32_t dnd_action)
{
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_handle_offer,
    data_offer_handle_source_actions, /* Version 3 */
    data_offer_handle_actions,        /* Version 3 */
};

static void primary_selection_offer_handle_offer(void *data, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1,
                                                 const char *mime_type)
{
    SDL_WaylandPrimarySelectionOffer *offer = data;
    Wayland_primary_selection_offer_add_mime(offer, mime_type);
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
    }
}

static void data_device_handle_enter(void *data, struct wl_data_device *wl_data_device,
                                     uint32_t serial, struct wl_surface *surface,
                                     wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *id)
{
    SDL_WaylandDataDevice *data_device = data;
    SDL_bool has_mime = SDL_FALSE;
    uint32_t dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

    data_device->drag_serial = serial;

    if (id) {
        data_device->drag_offer = wl_data_offer_get_user_data(id);

        /* TODO: SDL Support more mime types */
#ifdef SDL_USE_LIBDBUS
        if (Wayland_data_offer_has_mime(data_device->drag_offer, FILE_PORTAL_MIME)) {
            has_mime = SDL_TRUE;
            wl_data_offer_accept(id, serial, FILE_PORTAL_MIME);
        }
#endif
        if (Wayland_data_offer_has_mime(data_device->drag_offer, FILE_MIME)) {
            has_mime = SDL_TRUE;
            wl_data_offer_accept(id, serial, FILE_MIME);
        }

        /* SDL only supports "copy" style drag and drop */
        if (has_mime) {
            dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
        } else {
            /* drag_mime is NULL this will decline the offer */
            wl_data_offer_accept(id, serial, NULL);
        }
        if (wl_data_offer_get_version(data_device->drag_offer->offer) >=
            WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION) {
            wl_data_offer_set_actions(data_device->drag_offer->offer,
                                      dnd_action, dnd_action);
        }

        /* find the current window */
        if (surface) {
            SDL_WindowData *window = Wayland_GetWindowDataForOwnedSurface(surface);
            if (window) {
                data_device->dnd_window = window->sdlwindow;
            } else {
                data_device->dnd_window = NULL;
            }
        }
    }
}

static void data_device_handle_leave(void *data, struct wl_data_device *wl_data_device)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer) {
        Wayland_data_offer_destroy(data_device->drag_offer);
        data_device->drag_offer = NULL;
    }
}

static void data_device_handle_motion(void *data, struct wl_data_device *wl_data_device,
                                      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window) {
        const float dx = (float)wl_fixed_to_double(x);
        const float dy = (float)wl_fixed_to_double(y);

        /* XXX: Send the filename here if the event system ever starts passing it though.
         *      Any future implementation should cache the filenames, as otherwise this could
         *      hammer the DBus interface hundreds or even thousands of times per second.
         */
        SDL_SendDropPosition(data_device->dnd_window, dx, dy);
    }
}

/* Decodes URI escape sequences in string buf of len bytes
 * (excluding the terminating NULL byte) in-place. Since
 * URI-encoded characters take three times the space of
 * normal characters, this should not be an issue.
 *
 * Returns the number of decoded bytes that wound up in
 * the buffer, excluding the terminating NULL byte.
 *
 * The buffer is guaranteed to be NULL-terminated but
 * may contain embedded NULL bytes.
 *
 * On error, -1 is returned.
 *
 * FIXME: This was shamelessly copied from SDL_x11events.c
 */
static int Wayland_URIDecode(char *buf, int len)
{
    int ri, wi, di;
    char decode = '\0';
    if (!buf || len < 0) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        len = SDL_strlen(buf);
    }
    for (ri = 0, wi = 0, di = 0; ri < len && wi < len; ri += 1) {
        if (di == 0) {
            /* start decoding */
            if (buf[ri] == '%') {
                decode = '\0';
                di += 1;
                continue;
            }
            /* normal write */
            buf[wi] = buf[ri];
            wi += 1;
            continue;
        } else if (di == 1 || di == 2) {
            char off = '\0';
            char isa = buf[ri] >= 'a' && buf[ri] <= 'f';
            char isA = buf[ri] >= 'A' && buf[ri] <= 'F';
            char isn = buf[ri] >= '0' && buf[ri] <= '9';
            if (!(isa || isA || isn)) {
                /* not a hexadecimal */
                int sri;
                for (sri = ri - di; sri <= ri; sri += 1) {
                    buf[wi] = buf[sri];
                    wi += 1;
                }
                di = 0;
                continue;
            }
            /* itsy bitsy magicsy */
            if (isn) {
                off = 0 - '0';
            } else if (isa) {
                off = 10 - 'a';
            } else if (isA) {
                off = 10 - 'A';
            }
            decode |= (buf[ri] + off) << (2 - di) * 4;
            if (di == 2) {
                buf[wi] = decode;
                wi += 1;
                di = 0;
            } else {
                di += 1;
            }
            continue;
        }
    }
    buf[wi] = '\0';
    return wi;
}

/* Convert URI to local filename
 * return filename if possible, else NULL
 *
 *  FIXME: This was shamelessly copied from SDL_x11events.c
 */
static char *Wayland_URIToLocal(char *uri)
{
    char *file = NULL;
    SDL_bool local;

    if (SDL_memcmp(uri, "file:/", 6) == 0) {
        uri += 6; /* local file? */
    } else if (SDL_strstr(uri, ":/") != NULL) {
        return file; /* wrong scheme */
    }

    local = uri[0] != '/' || (uri[0] != '\0' && uri[1] == '/');

    /* got a hostname? */
    if (!local && uri[0] == '/' && uri[2] != '/') {
        char *hostname_end = SDL_strchr(uri + 1, '/');
        if (hostname_end) {
            char hostname[257];
            if (gethostname(hostname, 255) == 0) {
                hostname[256] = '\0';
                if (SDL_memcmp(uri + 1, hostname, hostname_end - (uri + 1)) == 0) {
                    uri = hostname_end + 1;
                    local = SDL_TRUE;
                }
            }
        }
    }
    if (local) {
        file = uri;
        /* Convert URI escape sequences to real characters */
        Wayland_URIDecode(file, 0);
        if (uri[1] == '/') {
            file++;
        } else {
            file--;
        }
    }
    return file;
}

static void data_device_handle_drop(void *data, struct wl_data_device *wl_data_device)
{
    SDL_WaylandDataDevice *data_device = data;

    if (data_device->drag_offer && data_device->dnd_window) {
        /* TODO: SDL Support more mime types */
        size_t length;
        SDL_bool drop_handled = SDL_FALSE;
#ifdef SDL_USE_LIBDBUS
        if (Wayland_data_offer_has_mime(
            data_device->drag_offer, FILE_PORTAL_MIME)) {
            void *buffer = Wayland_data_offer_receive(data_device->drag_offer,
                                                      FILE_PORTAL_MIME, &length);
            if (buffer) {
                SDL_DBusContext *dbus = SDL_DBus_GetContext();
                if (dbus) {
                    int path_count = 0;
                    char **paths = SDL_DBus_DocumentsPortalRetrieveFiles(buffer, &path_count);
                    /* If dropped files contain a directory the list is empty */
                    if (paths && path_count > 0) {
                        for (int i = 0; i < path_count; i++) {
                            SDL_SendDropFile(data_device->dnd_window, NULL, paths[i]);
                        }
                        dbus->free_string_array(paths);
                        SDL_SendDropComplete(data_device->dnd_window);
                        drop_handled = SDL_TRUE;
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
        if (!drop_handled && Wayland_data_offer_has_mime(
                                                         data_device->drag_offer, FILE_MIME)) {
            void *buffer = Wayland_data_offer_receive(data_device->drag_offer,
                                                      FILE_MIME, &length);
            if (buffer) {
                char *saveptr = NULL;
                char *token = SDL_strtok_r((char *)buffer, "\r\n", &saveptr);
                while (token) {
                    char *fn = Wayland_URIToLocal(token);
                    if (fn) {
                        SDL_SendDropFile(data_device->dnd_window, NULL, fn);
                    }
                    token = SDL_strtok_r(NULL, "\r\n", &saveptr);
                }
                SDL_SendDropComplete(data_device->dnd_window);
                SDL_free(buffer);
                drop_handled = SDL_TRUE;
            }
        }

        if (drop_handled && wl_data_offer_get_version(data_device->drag_offer->offer) >=
            WL_DATA_OFFER_FINISH_SINCE_VERSION) {
            wl_data_offer_finish(data_device->drag_offer->offer);
        }
    }

    Wayland_data_offer_destroy(data_device->drag_offer);
    data_device->drag_offer = NULL;
}

static void data_device_handle_selection(void *data, struct wl_data_device *wl_data_device,
                                         struct wl_data_offer *id)
{
    SDL_WaylandDataDevice *data_device = data;
    SDL_WaylandDataOffer *offer = NULL;

    if (id) {
        offer = wl_data_offer_get_user_data(id);
    }

    if (data_device->selection_offer != offer) {
        Wayland_data_offer_destroy(data_device->selection_offer);
        data_device->selection_offer = offer;
    }

    SDL_SendClipboardUpdate();
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

    SDL_SendClipboardUpdate();
}

static const struct zwp_primary_selection_device_v1_listener primary_selection_device_listener = {
    primary_selection_device_handle_offer,
    primary_selection_device_handle_selection
};

static void text_input_enter(void *data,
                             struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    /* No-op */
}

static void text_input_leave(void *data,
                             struct zwp_text_input_v3 *zwp_text_input_v3,
                             struct wl_surface *surface)
{
    /* No-op */
}

static void text_input_preedit_string(void *data,
                                      struct zwp_text_input_v3 *zwp_text_input_v3,
                                      const char *text,
                                      int32_t cursor_begin,
                                      int32_t cursor_end)
{
    SDL_WaylandTextInput *text_input = data;
    char buf[SDL_TEXTEDITINGEVENT_TEXT_SIZE];
    text_input->has_preedit = SDL_TRUE;
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
        buf[0] = '\0';
        SDL_SendEditingText(buf, 0, 0);
    }
}

static void text_input_commit_string(void *data,
                                     struct zwp_text_input_v3 *zwp_text_input_v3,
                                     const char *text)
{
    if (text && *text) {
        char buf[SDL_TEXTINPUTEVENT_TEXT_SIZE];
        size_t text_bytes = SDL_strlen(text), i = 0;

        while (i < text_bytes) {
            size_t sz = SDL_utf8strlcpy(buf, text + i, sizeof(buf));
            SDL_SendKeyboardText(buf);

            i += sz;
        }
    }
}

static void text_input_delete_surrounding_text(void *data,
                                               struct zwp_text_input_v3 *zwp_text_input_v3,
                                               uint32_t before_length,
                                               uint32_t after_length)
{
    /* FIXME: Do we care about this event? */
}

static void text_input_done(void *data,
                            struct zwp_text_input_v3 *zwp_text_input_v3,
                            uint32_t serial)
{
    SDL_WaylandTextInput *text_input = data;
    if (!text_input->has_preedit) {
        SDL_SendEditingText("", 0, 0);
    }
    text_input->has_preedit = SDL_FALSE;
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
        /* No seat yet, will be initialized later. */
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
        /* No seat yet, will be initialized later. */
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
        /* No seat yet, will be initialized later. */
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

static SDL_PenID Wayland_get_penid(void *data, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTool *sdltool = data;
    return sdltool->penid;
}

/* For registering pens */
static SDL_Pen *Wayland_get_current_pen(void *data, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;

    if (!input->current_pen.builder) {
        /* Starting new pen or updating one? */
        SDL_PenID penid = sdltool->penid;

        if (penid == 0) {
            /* Found completely new pen? */
            penid = ++input->num_pens;
            sdltool->penid = penid;
        }
        input->current_pen.builder = SDL_GetPenPtr(penid);
        if (!input->current_pen.builder) {
            /* Must register as new pen */
            input->current_pen.builder = SDL_PenModifyBegin(penid);
        }
    }
    return input->current_pen.builder;
}

static void tablet_tool_handle_type(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t type)
{
    SDL_Pen *pen = Wayland_get_current_pen(data, tool);

    switch (type) {
    case ZWP_TABLET_TOOL_V2_TYPE_ERASER:
        pen->type = SDL_PEN_TYPE_ERASER;
        break;

    case ZWP_TABLET_TOOL_V2_TYPE_PEN:
        pen->type = SDL_PEN_TYPE_PEN;
        break;

    case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
        pen->type = SDL_PEN_TYPE_PENCIL;
        break;

    case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
        pen->type = SDL_PEN_TYPE_AIRBRUSH;
        break;

    case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
        pen->type = SDL_PEN_TYPE_BRUSH;
        break;

    case ZWP_TABLET_TOOL_V2_TYPE_FINGER:
    case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
    case ZWP_TABLET_TOOL_V2_TYPE_LENS:
    default:
        pen->type = SDL_PEN_TYPE_NONE; /* Mark for deregistration */
    }

    SDL_PenUpdateGUIDForType(&pen->guid, pen->type);
}

static void tablet_tool_handle_hardware_serial(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial_hi, uint32_t serial_lo)
{
#if !(SDL_PEN_DEBUG_NOID)
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;

    if (!input->current_pen.builder_guid_complete) {
        SDL_Pen *pen = Wayland_get_current_pen(data, tool);
        SDL_PenUpdateGUIDForGeneric(&pen->guid, serial_hi, serial_lo);
        if (serial_hi || serial_lo) {
            input->current_pen.builder_guid_complete = SDL_TRUE;
        }
    }
#endif
}

static void tablet_tool_handle_hardware_id_wacom(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t id_hi, uint32_t id_lo)
{
#if !(SDL_PEN_DEBUG_NOID | SDL_PEN_DEBUG_NONWACOM)
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_Pen *pen = Wayland_get_current_pen(data, tool);
    Uint32 axis_flags;

#if SDL_PEN_DEBUG_NOSERIAL_WACOM /* Check: have we disabled pen serial ID decoding for testing? */
    id_hi = 0;
#endif

    SDL_PenUpdateGUIDForWacom(&pen->guid, id_lo, id_hi);
    if (id_hi) { /* Have a serial number? */
        input->current_pen.builder_guid_complete = SDL_TRUE;
    }

    if (SDL_PenModifyForWacomID(pen, id_lo, &axis_flags)) {
        SDL_PenModifyAddCapabilities(pen, axis_flags);
    }
#endif
}

static void tablet_tool_handle_capability(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t capability)
{
    SDL_Pen *pen = Wayland_get_current_pen(data, tool);

    switch (capability) {
    case ZWP_TABLET_TOOL_V2_CAPABILITY_TILT:
        SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK);
        break;

    case ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE:
        SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_PRESSURE_MASK);
        break;

    case ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE:
        SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_DISTANCE_MASK);
        break;

    case ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION:
        SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_ROTATION_MASK);
        break;

    case ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER:
        SDL_PenModifyAddCapabilities(pen, SDL_PEN_AXIS_SLIDER_MASK);
        break;

    case ZWP_TABLET_TOOL_V2_CAPABILITY_WHEEL:
        /* Presumably for tools other than pens? */
        break;

    default:
        break;
    }
}

static void Wayland_tool_builder_reset(struct SDL_WaylandTabletInput *input)
{
    input->current_pen.builder = NULL;
    input->current_pen.builder_guid_complete = SDL_FALSE;
}

static void tablet_tool_handle_done(void *data, struct zwp_tablet_tool_v2 *tool)
{
    SDL_Pen *pen = Wayland_get_current_pen(data, tool);
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;

    if (!input->current_pen.builder_guid_complete) {
        /* No complete GUID?  Use tablet and tool device index */
        SDL_PenUpdateGUIDForGeneric(&pen->guid, input->id, sdltool->penid);
    }

    SDL_PenModifyEnd(pen, SDL_TRUE);

    Wayland_tool_builder_reset(input);
}

static void Wayland_tool_destroy(struct zwp_tablet_tool_v2 *tool)
{
    if (tool) {
        struct SDL_WaylandTool *waypen = zwp_tablet_tool_v2_get_user_data(tool);
        if (waypen) {
            SDL_free(waypen);
        }
        zwp_tablet_tool_v2_destroy(tool);
    }
}

static void tablet_object_list_remove(struct SDL_WaylandTabletObjectListNode *head, void *object);

static void tablet_tool_handle_removed(void *data, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTool *waypen = zwp_tablet_tool_v2_get_user_data(tool);
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_Pen *pen = Wayland_get_current_pen(data, tool);
    if (pen) {
        SDL_PenModifyEnd(pen, SDL_FALSE);
        Wayland_tool_builder_reset(waypen->tablet);
        Wayland_tool_destroy(tool);
    } else {
        zwp_tablet_tool_v2_destroy(tool);
    }

    tablet_object_list_remove(input->tools, tool);
}

static void tablet_tool_handle_proximity_in(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial, struct zwp_tablet_v2 *tablet, struct wl_surface *surface)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_WindowData *window;
    SDL_PenID penid = Wayland_get_penid(data, tool);

    if (!surface) {
        return;
    }

    window = Wayland_GetWindowDataForOwnedSurface(surface);

    if (window) {
        input->tool_focus = window;
        input->tool_prox_serial = serial;

        if (penid) {
            SDL_SendPenWindowEvent(0, penid, window->sdlwindow);
        } else {
            SDL_SetMouseFocus(window->sdlwindow);
        }
        SDL_SetCursor(NULL);
    }
}

static void tablet_tool_handle_proximity_out(void *data, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_PenID penid = Wayland_get_penid(data, tool);
    if (input->tool_focus) {
        if (penid) {
            SDL_SendPenWindowEvent(0, penid, NULL);
        } else {
            SDL_SetMouseFocus(NULL);
        }
        input->tool_focus = NULL;
    }
}

static void tablet_tool_handle_down(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;

    input->current_pen.buttons_pressed |= SDL_PEN_DOWN_MASK;

    input->current_pen.serial = serial;
}

static void tablet_tool_handle_up(void *data, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    input->current_pen.buttons_released |= SDL_PEN_DOWN_MASK;
}

static void tablet_tool_handle_motion(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_WindowData *window = input->tool_focus;
    SDL_PenID penid = Wayland_get_penid(data, tool);

    input->sx_w = sx_w;
    input->sy_w = sy_w;

    if (input->tool_focus) {
        const float sx_f = (float)wl_fixed_to_double(sx_w);
        const float sy_f = (float)wl_fixed_to_double(sy_w);
        const float sx = sx_f * window->pointer_scale.x;
        const float sy = sy_f * window->pointer_scale.y;

        if (penid != SDL_PEN_INVALID) {
            input->current_pen.update_status.x = sx;
            input->current_pen.update_status.y = sy;
            input->current_pen.update_window = window;
        } else {
            /* Plain mouse event */
            SDL_SendMouseMotion(0, window->sdlwindow, 0, 0, sx, sy);
        }
    }
}

static void tablet_tool_handle_pressure(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t pressure)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    input->current_pen.update_status.axes[SDL_PEN_AXIS_PRESSURE] = pressure / 65535.0f;
    if (pressure) {
        input->current_pen.update_status.axes[SDL_PEN_AXIS_DISTANCE] = 0.0f;
    }
}

static void tablet_tool_handle_distance(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t distance)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    input->current_pen.update_status.axes[SDL_PEN_AXIS_DISTANCE] = distance / 65535.0f;
    if (distance) {
        input->current_pen.update_status.axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
    }
}

static void tablet_tool_handle_tilt(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t xtilt, wl_fixed_t ytilt)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;

    input->current_pen.update_status.axes[SDL_PEN_AXIS_XTILT] = (float)(wl_fixed_to_double(xtilt));
    input->current_pen.update_status.axes[SDL_PEN_AXIS_YTILT] = (float)(wl_fixed_to_double(ytilt));
}

static void tablet_tool_handle_button(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t serial, uint32_t button, uint32_t state)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    Uint16 mask = 0;
    SDL_bool pressed = state == ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED ? SDL_TRUE : SDL_FALSE;

    /* record event serial number to report it later in tablet_tool_handle_frame() */
    input->current_pen.serial = serial;

    switch (button) {
    /* see %{_includedir}/linux/input-event-codes.h */
    case 0x14b: /* BTN_STYLUS */
        mask = SDL_BUTTON_LMASK;
        break;
    case 0x14c: /* BTN_STYLUS2 */
        mask = SDL_BUTTON_MMASK;
        break;
    case 0x149: /* BTN_STYLUS3 */
        mask = SDL_BUTTON_RMASK;
        break;
    }

    if (pressed) {
        input->current_pen.buttons_pressed |= mask;
    } else {
        input->current_pen.buttons_released |= mask;
    }
}

static void tablet_tool_handle_rotation(void *data, struct zwp_tablet_tool_v2 *tool, wl_fixed_t degrees)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    float rotation = (float)(wl_fixed_to_double(degrees));

    /* map to -180.0f ... 179.0f range: */
    input->current_pen.update_status.axes[SDL_PEN_AXIS_ROTATION] = rotation > 180.0f ? rotation - 360.0f : rotation;
}

static void tablet_tool_handle_slider(void *data, struct zwp_tablet_tool_v2 *tool, int32_t position)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    input->current_pen.update_status.axes[SDL_PEN_AXIS_SLIDER] = position / 65535.0;
}

static void tablet_tool_handle_wheel(void *data, struct zwp_tablet_tool_v2 *tool, int32_t degrees, int32_t clicks)
{
    /* not supported at the moment */
}

static void tablet_tool_handle_frame(void *data, struct zwp_tablet_tool_v2 *tool, uint32_t time)
{
    struct SDL_WaylandTool *sdltool = data;
    struct SDL_WaylandTabletInput *input = sdltool->tablet;
    SDL_PenID penid = Wayland_get_penid(data, tool);
    SDL_WindowData *window = input->current_pen.update_window;
    SDL_PenStatusInfo *status = &input->current_pen.update_status;
    int button;
    int button_mask;
    Uint64 timestamp = Wayland_GetEventTimestamp(SDL_MS_TO_NS(time));

    if (penid == 0 || !window) { /* Not a pen, or event reported out of focus */
        return;
    }
    /* window == input->tool_focus */

    /* All newly released buttons + PEN_UP event */
    button_mask = input->current_pen.buttons_released;
    if (button_mask & SDL_PEN_DOWN_MASK) {
	/* Perform hit test, if appropriate */
	if (!SDL_PenPerformHitTest()
	    || !ProcessHitTest(window, input->sdlWaylandInput->seat, input->sx_w, input->sy_w, input->current_pen.serial)) {
	    SDL_SendPenTipEvent(timestamp, penid, SDL_RELEASED);
	}
    }
    button_mask &= ~SDL_PEN_DOWN_MASK;

    for (button = 1; button_mask; ++button, button_mask >>= 1) {
        if (button_mask & 1) {
            SDL_SendPenButton(timestamp, penid, SDL_RELEASED, button);
        }
    }

    /* All newly pressed buttons + PEN_DOWN event */
    button_mask = input->current_pen.buttons_pressed;
    if (button_mask & SDL_PEN_DOWN_MASK) {
	/* Perform hit test, if appropriate */
	if (!SDL_PenPerformHitTest()
	    || !ProcessHitTest(window, input->sdlWaylandInput->seat, input->sx_w, input->sy_w, input->current_pen.serial)) {
	    SDL_SendPenTipEvent(timestamp, penid, SDL_PRESSED);
	}
    }
    button_mask &= ~SDL_PEN_DOWN_MASK;

    for (button = 1; button_mask; ++button, button_mask >>= 1) {
        if (button_mask & 1) {
            SDL_SendPenButton(timestamp, penid, SDL_PRESSED, button);
        }
    }

    SDL_SendPenMotion(timestamp, penid, SDL_TRUE, status);

    /* Wayland_UpdateImplicitGrabSerial will ignore serial 0, so it is safe to call with the default value */
    Wayland_UpdateImplicitGrabSerial(input->sdlWaylandInput, input->current_pen.serial);

    /* Reset masks for next tool frame */
    input->current_pen.buttons_pressed = 0;
    input->current_pen.buttons_released = 0;
    input->current_pen.serial = 0;
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

static struct SDL_WaylandTabletObjectListNode *tablet_object_list_new_node(void *object)
{
    struct SDL_WaylandTabletObjectListNode *node;

    node = SDL_calloc(1, sizeof(*node));
    if (!node) {
        return NULL;
    }

    node->next = NULL;
    node->object = object;

    return node;
}

static void tablet_object_list_append(struct SDL_WaylandTabletObjectListNode *head, void *object)
{
    if (!head->object) {
        head->object = object;
        return;
    }

    while (head->next) {
        head = head->next;
    }

    head->next = tablet_object_list_new_node(object);
}

static void tablet_object_list_destroy(struct SDL_WaylandTabletObjectListNode *head, void (*deleter)(void *object))
{
    while (head) {
        struct SDL_WaylandTabletObjectListNode *next = head->next;
        if (head->object) {
            (*deleter)(head->object);
        }
        SDL_free(head);
        head = next;
    }
}

void tablet_object_list_remove(struct SDL_WaylandTabletObjectListNode *head, void *object)
{
    struct SDL_WaylandTabletObjectListNode **head_p = &head;
    while (*head_p && (*head_p)->object != object) {
        head_p = &((*head_p)->next);
    }

    if (*head_p) {
        struct SDL_WaylandTabletObjectListNode *object_head = *head_p;

        if (object_head == head) {
            /* Must not remove head node */
            head->object = NULL;
        } else {
            *head_p = object_head->next;
            SDL_free(object_head);
        }
    }
}

static void tablet_seat_handle_tablet_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_v2 *tablet)
{
    struct SDL_WaylandTabletInput *input = data;

    tablet_object_list_append(input->tablets, tablet);
}

static void tablet_seat_handle_tool_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_tool_v2 *tool)
{
    struct SDL_WaylandTabletInput *input = data;
    struct SDL_WaylandTool *sdltool = SDL_calloc(1, sizeof(struct SDL_WaylandTool));

    zwp_tablet_tool_v2_add_listener(tool, &tablet_tool_listener, sdltool);
    zwp_tablet_tool_v2_set_user_data(tool, sdltool);

    sdltool->tablet = input;

    tablet_object_list_append(input->tools, tool);
}

static void tablet_seat_handle_pad_added(void *data, struct zwp_tablet_seat_v2 *seat, struct zwp_tablet_pad_v2 *pad)
{
    struct SDL_WaylandTabletInput *input = data;

    tablet_object_list_append(input->pads, pad);
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
    tablet_seat_handle_tablet_added,
    tablet_seat_handle_tool_added,
    tablet_seat_handle_pad_added
};

void Wayland_input_add_tablet(struct SDL_WaylandInput *input, struct SDL_WaylandTabletManager *tablet_manager)
{
    struct SDL_WaylandTabletInput *tablet_input;
    static Uint32 num_tablets = 0;

    if (!tablet_manager || !input->seat) {
        return;
    }

    tablet_input = SDL_calloc(1, sizeof(*tablet_input));
    if (!tablet_input) {
        return;
    }

    input->tablet = tablet_input;

    tablet_input->sdlWaylandInput = input;
    tablet_input->seat = zwp_tablet_manager_v2_get_tablet_seat((struct zwp_tablet_manager_v2 *)tablet_manager, input->seat);

    tablet_input->tablets = tablet_object_list_new_node(NULL);
    tablet_input->tools = tablet_object_list_new_node(NULL);
    tablet_input->pads = tablet_object_list_new_node(NULL);
    tablet_input->id = num_tablets++;

    zwp_tablet_seat_v2_add_listener((struct zwp_tablet_seat_v2 *)tablet_input->seat, &tablet_seat_listener, tablet_input);
}

#define TABLET_OBJECT_LIST_DELETER(fun) (void (*)(void *)) fun
void Wayland_input_destroy_tablet(struct SDL_WaylandInput *input)
{
    tablet_object_list_destroy(input->tablet->pads, TABLET_OBJECT_LIST_DELETER(zwp_tablet_pad_v2_destroy));
    tablet_object_list_destroy(input->tablet->tools, TABLET_OBJECT_LIST_DELETER(Wayland_tool_destroy));
    tablet_object_list_destroy(input->tablet->tablets, TABLET_OBJECT_LIST_DELETER(zwp_tablet_v2_destroy));

    zwp_tablet_seat_v2_destroy(input->tablet->seat);

    SDL_free(input->tablet);
    input->tablet = NULL;
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
        Wayland_input_add_tablet(input, d->tablet_manager);
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

        wl_list_for_each_safe(tp, tmp, &touch_points, link)
        {
            WAYLAND_wl_list_remove(&tp->link);
            SDL_free(tp);
        }
    }

    if (input->tablet) {
        Wayland_input_destroy_tablet(input);
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

    /* Relative pointer event times are in microsecond granularity. */
    const Uint64 timestamp = SDL_US_TO_NS(((Uint64)time_hi << 32) | (Uint64)time_lo);

    dx_unaccel = wl_fixed_to_double(dx_unaccel_w);
    dy_unaccel = wl_fixed_to_double(dy_unaccel_w);

    if (input->pointer_focus && d->relative_mouse_mode) {
        SDL_SendMouseMotion(Wayland_GetEventTimestamp(timestamp), window->sdlwindow, 0, 1, (float)dx_unaccel, (float)dy_unaccel);
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

static void lock_pointer_to_window(SDL_Window *window,
                                   struct SDL_WaylandInput *input)
{
    SDL_WindowData *w = window->driverdata;
    SDL_VideoData *d = input->display;
    struct zwp_locked_pointer_v1 *locked_pointer;

    if (!d->pointer_constraints || !input->pointer) {
        return;
    }

    if (w->locked_pointer) {
        return;
    }

    locked_pointer =
        zwp_pointer_constraints_v1_lock_pointer(d->pointer_constraints,
                                                w->surface,
                                                input->pointer,
                                                NULL,
                                                ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_locked_pointer_v1_add_listener(locked_pointer,
                                       &locked_pointer_listener,
                                       window);

    w->locked_pointer = locked_pointer;
}

static void pointer_confine_destroy(SDL_Window *window)
{
    SDL_WindowData *w = window->driverdata;
    if (w->confined_pointer) {
        zwp_confined_pointer_v1_destroy(w->confined_pointer);
        w->confined_pointer = NULL;
    }
}

int Wayland_input_lock_pointer(struct SDL_WaylandInput *input)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = input->display;
    SDL_Window *window;
    struct zwp_relative_pointer_v1 *relative_pointer;

    if (!d->relative_pointer_manager) {
        return -1;
    }

    if (!d->pointer_constraints) {
        return -1;
    }

    if (!input->pointer) {
        return -1;
    }

    /* If we have a pointer confine active, we must destroy it here because
     * creating a locked pointer otherwise would be a protocol error.
     */
    for (window = vd->windows; window; window = window->next) {
        pointer_confine_destroy(window);
    }

    if (!input->relative_pointer) {
        relative_pointer =
            zwp_relative_pointer_manager_v1_get_relative_pointer(
                d->relative_pointer_manager,
                input->pointer);
        zwp_relative_pointer_v1_add_listener(relative_pointer,
                                             &relative_pointer_listener,
                                             input);
        input->relative_pointer = relative_pointer;
    }

    for (window = vd->windows; window; window = window->next) {
        lock_pointer_to_window(window, input);
    }

    d->relative_mouse_mode = 1;

    return 0;
}

int Wayland_input_unlock_pointer(struct SDL_WaylandInput *input)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = input->display;
    SDL_Window *window;
    SDL_WindowData *w;

    for (window = vd->windows; window; window = window->next) {
        w = window->driverdata;
        if (w->locked_pointer) {
            zwp_locked_pointer_v1_destroy(w->locked_pointer);
        }
        w->locked_pointer = NULL;
    }

    if (input->relative_pointer) {
        zwp_relative_pointer_v1_destroy(input->relative_pointer);
        input->relative_pointer = NULL;
    }

    d->relative_mouse_mode = 0;

    for (window = vd->windows; window; window = window->next) {
        Wayland_input_confine_pointer(input, window);
    }

    return 0;
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

int Wayland_input_confine_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    SDL_WindowData *w = window->driverdata;
    SDL_VideoData *d = input->display;
    struct zwp_confined_pointer_v1 *confined_pointer;
    struct wl_region *confine_rect;

    if (!d->pointer_constraints) {
        return -1;
    }

    if (!input->pointer) {
        return -1;
    }

    /* A confine may already be active, in which case we should destroy it and
     * create a new one.
     */
    pointer_confine_destroy(window);

    /* We cannot create a confine if the pointer is already locked. Defer until
     * the pointer is unlocked.
     */
    if (d->relative_mouse_mode) {
        return 0;
    }

    /* Don't confine the pointer if it shouldn't be confined. */
    if (SDL_RectEmpty(&window->mouse_rect) && !(window->flags & SDL_WINDOW_MOUSE_GRABBED)) {
        return 0;
    }

    if (SDL_RectEmpty(&window->mouse_rect)) {
        confine_rect = NULL;
    } else {
        SDL_Rect scaled_mouse_rect;

        scaled_mouse_rect.x = (int)SDL_floorf((float)window->mouse_rect.x / w->pointer_scale.x);
        scaled_mouse_rect.y = (int)SDL_floorf((float)window->mouse_rect.y / w->pointer_scale.y);
        scaled_mouse_rect.w = (int)SDL_ceilf((float)window->mouse_rect.w / w->pointer_scale.x);
        scaled_mouse_rect.h = (int)SDL_ceilf((float)window->mouse_rect.h / w->pointer_scale.y);

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
    return 0;
}

int Wayland_input_unconfine_pointer(struct SDL_WaylandInput *input, SDL_Window *window)
{
    pointer_confine_destroy(window);
    return 0;
}

int Wayland_input_grab_keyboard(SDL_Window *window, struct SDL_WaylandInput *input)
{
    SDL_WindowData *w = window->driverdata;
    SDL_VideoData *d = input->display;

    if (!d->key_inhibitor_manager) {
        return -1;
    }

    if (w->key_inhibitor) {
        return 0;
    }

    w->key_inhibitor =
        zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(d->key_inhibitor_manager,
                                                                    w->surface,
                                                                    input->seat);

    return 0;
}

int Wayland_input_ungrab_keyboard(SDL_Window *window)
{
    SDL_WindowData *w = window->driverdata;

    if (w->key_inhibitor) {
        zwp_keyboard_shortcuts_inhibitor_v1_destroy(w->key_inhibitor);
        w->key_inhibitor = NULL;
    }

    return 0;
}

void Wayland_UpdateImplicitGrabSerial(struct SDL_WaylandInput *input, Uint32 serial)
{
    if (serial > input->last_implicit_grab_serial) {
        input->last_implicit_grab_serial = serial;
        Wayland_data_device_set_serial(input->data_device, serial);
        Wayland_primary_selection_device_set_serial(input->primary_selection_device, serial);
    }
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */
