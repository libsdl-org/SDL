/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_stdinc.h"
#include "SDL_assert.h"
#include "SDL_log.h"

#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../../events/scancodes_xfree86.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"

#include "SDL_waylanddyn.h"

#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

#include <linux/input.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

struct SDL_WaylandInput {
    SDL_VideoData *display;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct zwp_relative_pointer_v1 *relative_pointer;
    SDL_WindowData *pointer_focus;
    SDL_WindowData *keyboard_focus;

    /* Last motion location */
    wl_fixed_t sx_w;
    wl_fixed_t sy_w;

    double dx_frac;
    double dy_frac;

    struct {
        struct xkb_keymap *keymap;
        struct xkb_state *state;
    } xkb;
};

void
Wayland_PumpEvents(_THIS)
{
    SDL_VideoData *d = _this->driverdata;
    struct pollfd pfd[1];

    pfd[0].fd = WAYLAND_wl_display_get_fd(d->display);
    pfd[0].events = POLLIN;
    poll(pfd, 1, 0);

    if (pfd[0].revents & POLLIN)
        WAYLAND_wl_display_dispatch(d->display);
    else
        WAYLAND_wl_display_dispatch_pending(d->display);
}

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
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

    window = (SDL_WindowData *)wl_surface_get_user_data(surface);
    
    if (window) {
        input->pointer_focus = window;
        SDL_SetMouseFocus(window->sdlwindow);
    }
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface)
{
    struct SDL_WaylandInput *input = data;

    if (input->pointer_focus) {
        SDL_SetMouseFocus(NULL);
        input->pointer_focus = NULL;
    }
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
                      uint32_t time, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window = input->pointer_focus;
    input->sx_w = sx_w;
    input->sy_w = sy_w;
    if (input->pointer_focus) {
        const int sx = wl_fixed_to_int(sx_w);
        const int sy = wl_fixed_to_int(sy_w);
        SDL_SendMouseMotion(window->sdlwindow, 0, 0, sx, sy);
    }
}

static SDL_bool
ProcessHitTest(struct SDL_WaylandInput *input, uint32_t serial)
{
    SDL_WindowData *window_data = input->pointer_focus;
    SDL_Window *window = window_data->sdlwindow;

    if (window->hit_test) {
        const SDL_Point point = { wl_fixed_to_int(input->sx_w), wl_fixed_to_int(input->sy_w) };
        const SDL_HitTestResult rc = window->hit_test(window, &point, window->hit_test_data);
        static const uint32_t directions[] = {
            WL_SHELL_SURFACE_RESIZE_TOP_LEFT, WL_SHELL_SURFACE_RESIZE_TOP,
            WL_SHELL_SURFACE_RESIZE_TOP_RIGHT, WL_SHELL_SURFACE_RESIZE_RIGHT,
            WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT, WL_SHELL_SURFACE_RESIZE_BOTTOM,
            WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT, WL_SHELL_SURFACE_RESIZE_LEFT
        };
        switch (rc) {
            case SDL_HITTEST_DRAGGABLE:
                wl_shell_surface_move(window_data->shell_surface, input->seat, serial);
                return SDL_TRUE;

            case SDL_HITTEST_RESIZE_TOPLEFT:
            case SDL_HITTEST_RESIZE_TOP:
            case SDL_HITTEST_RESIZE_TOPRIGHT:
            case SDL_HITTEST_RESIZE_RIGHT:
            case SDL_HITTEST_RESIZE_BOTTOMRIGHT:
            case SDL_HITTEST_RESIZE_BOTTOM:
            case SDL_HITTEST_RESIZE_BOTTOMLEFT:
            case SDL_HITTEST_RESIZE_LEFT:
                wl_shell_surface_resize(window_data->shell_surface, input->seat, serial, directions[rc - SDL_HITTEST_RESIZE_TOPLEFT]);
                return SDL_TRUE;

            default: return SDL_FALSE;
        }
    }

    return SDL_FALSE;
}

static void
pointer_handle_button_common(struct SDL_WaylandInput *input, uint32_t serial,
                             uint32_t time, uint32_t button, uint32_t state_w)
{
    SDL_WindowData *window = input->pointer_focus;
    enum wl_pointer_button_state state = state_w;
    uint32_t sdl_button;
    
    if  (input->pointer_focus) {
        switch (button) {
            case BTN_LEFT:
                sdl_button = SDL_BUTTON_LEFT;
                if (ProcessHitTest(input, serial)) {
                    return;  /* don't pass this event on to app. */
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

        SDL_SendMouseButton(window->sdlwindow, 0,
                            state ? SDL_PRESSED : SDL_RELEASED, sdl_button);
    }
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                      uint32_t time, uint32_t button, uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;

    pointer_handle_button_common(input, serial, time, button, state_w);
}

static void
pointer_handle_axis_common(struct SDL_WaylandInput *input,
                           uint32_t time, uint32_t axis, wl_fixed_t value)
{
    SDL_WindowData *window = input->pointer_focus;
    enum wl_pointer_axis a = axis;
    int x, y;

    if (input->pointer_focus) {
        switch (a) {
            case WL_POINTER_AXIS_VERTICAL_SCROLL:
                x = 0;
                y = wl_fixed_to_int(value);
                break;
            case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
                x = wl_fixed_to_int(value);
                y = 0;
                break;
            default:
                return;
        }

        SDL_SendMouseWheel(window->sdlwindow, 0, x, y, SDL_MOUSEWHEEL_NORMAL);
    }
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer,
                    uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct SDL_WaylandInput *input = data;

    pointer_handle_axis_common(input, time, axis, value);
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
    struct SDL_WaylandInput *input = data;
    char *map_str;

    if (!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    input->xkb.keymap = WAYLAND_xkb_keymap_new_from_string(input->display->xkb_context,
                                                map_str,
                                                XKB_KEYMAP_FORMAT_TEXT_V1,
                                                0);
    munmap(map_str, size);
    close(fd);

    if (!input->xkb.keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return;
    }

    input->xkb.state = WAYLAND_xkb_state_new(input->xkb.keymap);
    if (!input->xkb.state) {
        fprintf(stderr, "failed to create XKB state\n");
        WAYLAND_xkb_keymap_unref(input->xkb.keymap);
        input->xkb.keymap = NULL;
        return;
    }
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window;

    if (!surface) {
        /* enter event for a window we've just destroyed */
        return;
    }
 
    window = wl_surface_get_user_data(surface);

    if (window) {
        input->keyboard_focus = window;
        window->keyboard_device = input;
        SDL_SetKeyboardFocus(window->sdlwindow);
    }
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
    SDL_SetKeyboardFocus(NULL);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WindowData *window = input->keyboard_focus;
    enum wl_keyboard_key_state state = state_w;
    const xkb_keysym_t *syms;
    uint32_t scancode;
    char text[8];
    int size;

    if (key < SDL_arraysize(xfree86_scancode_table2)) {
        scancode = xfree86_scancode_table2[key];

        // TODO when do we get WL_KEYBOARD_KEY_STATE_REPEAT?
        if (scancode != SDL_SCANCODE_UNKNOWN)
            SDL_SendKeyboardKey(state == WL_KEYBOARD_KEY_STATE_PRESSED ?
                                SDL_PRESSED : SDL_RELEASED, scancode);
    }

    if (!window || window->keyboard_device != input || !input->xkb.state)
        return;

    // TODO can this happen?
    if (WAYLAND_xkb_state_key_get_syms(input->xkb.state, key + 8, &syms) != 1)
        return;

    if (state) {
        size = WAYLAND_xkb_keysym_to_utf8(syms[0], text, sizeof text);

        if (size > 0) {
            text[size] = 0;
            SDL_SendKeyboardText(text);
        }
    }
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
    struct SDL_WaylandInput *input = data;

    WAYLAND_xkb_state_update_mask(input->xkb.state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    struct SDL_WaylandInput *input = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
        input->pointer = wl_seat_get_pointer(seat);
        input->display->pointer = input->pointer;
        wl_pointer_set_user_data(input->pointer, input);
        wl_pointer_add_listener(input->pointer, &pointer_listener,
                                input);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
        wl_pointer_destroy(input->pointer);
        input->pointer = NULL;
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
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

void
Wayland_display_add_input(SDL_VideoData *d, uint32_t id)
{
    struct SDL_WaylandInput *input;

    input = SDL_calloc(1, sizeof *input);
    if (input == NULL)
        return;

    input->display = d;
    input->seat = wl_registry_bind(d->registry, id, &wl_seat_interface, 1);
    input->sx_w = wl_fixed_from_int(0);
    input->sy_w = wl_fixed_from_int(0);
    d->input = input;

    wl_seat_add_listener(input->seat, &seat_listener, input);
    wl_seat_set_user_data(input->seat, input);

    WAYLAND_wl_display_flush(d->display);
}

void Wayland_display_destroy_input(SDL_VideoData *d)
{
    struct SDL_WaylandInput *input = d->input;

    if (!input)
        return;

    if (input->keyboard)
        wl_keyboard_destroy(input->keyboard);

    if (input->pointer)
        wl_pointer_destroy(input->pointer);

    if (input->seat)
        wl_seat_destroy(input->seat);

    if (input->xkb.state)
        WAYLAND_xkb_state_unref(input->xkb.state);

    if (input->xkb.keymap)
        WAYLAND_xkb_keymap_unref(input->xkb.keymap);

    SDL_free(input);
    d->input = NULL;
}

void Wayland_display_add_relative_pointer_manager(SDL_VideoData *d, uint32_t id)
{
    d->relative_pointer_manager =
        wl_registry_bind(d->registry, id,
                         &zwp_relative_pointer_manager_v1_interface, 1);
}

void Wayland_display_destroy_relative_pointer_manager(SDL_VideoData *d)
{
    if (d->relative_pointer_manager)
        zwp_relative_pointer_manager_v1_destroy(d->relative_pointer_manager);
}

void Wayland_display_add_pointer_constraints(SDL_VideoData *d, uint32_t id)
{
    d->pointer_constraints =
        wl_registry_bind(d->registry, id,
                         &zwp_pointer_constraints_v1_interface, 1);
}

void Wayland_display_destroy_pointer_constraints(SDL_VideoData *d)
{
    if (d->pointer_constraints)
        zwp_pointer_constraints_v1_destroy(d->pointer_constraints);
}

static void
relative_pointer_handle_relative_motion(void *data,
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
    double dx;
    double dy;

    dx_unaccel = wl_fixed_to_double(dx_unaccel_w);
    dy_unaccel = wl_fixed_to_double(dy_unaccel_w);

    /* Add left over fraction from last event. */
    dx_unaccel += input->dx_frac;
    dy_unaccel += input->dy_frac;

    input->dx_frac = modf(dx_unaccel, &dx);
    input->dy_frac = modf(dy_unaccel, &dy);

    if (input->pointer_focus && d->relative_mouse_mode) {
        SDL_SendMouseMotion(window->sdlwindow, 0, 1, (int)dx, (int)dy);
    }
}

static const struct zwp_relative_pointer_v1_listener relative_pointer_listener = {
    relative_pointer_handle_relative_motion,
};

static void
locked_pointer_locked(void *data,
                      struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static void
locked_pointer_unlocked(void *data,
                        struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static const struct zwp_locked_pointer_v1_listener locked_pointer_listener = {
    locked_pointer_locked,
    locked_pointer_unlocked,
};

static void
lock_pointer_to_window(SDL_Window *window,
                       struct SDL_WaylandInput *input)
{
    SDL_WindowData *w = window->driverdata;
    SDL_VideoData *d = input->display;
    struct zwp_locked_pointer_v1 *locked_pointer;

    if (w->locked_pointer)
        return;

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

int Wayland_input_lock_pointer(struct SDL_WaylandInput *input)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *d = input->display;
    SDL_Window *window;
    struct zwp_relative_pointer_v1 *relative_pointer;

    if (!d->relative_pointer_manager)
        return -1;

    if (!d->pointer_constraints)
        return -1;

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

    for (window = vd->windows; window; window = window->next)
        lock_pointer_to_window(window, input);

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
        if (w->locked_pointer)
            zwp_locked_pointer_v1_destroy(w->locked_pointer);
        w->locked_pointer = NULL;
    }

    zwp_relative_pointer_v1_destroy(input->relative_pointer);
    input->relative_pointer = NULL;

    d->relative_mouse_mode = 0;

    return 0;
}

#endif /* SDL_VIDEO_DRIVER_WAYLAND */

/* vi: set ts=4 sw=4 expandtab: */
