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

#ifndef SDL_waylandevents_h_
#define SDL_waylandevents_h_

#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_pen_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylanddatamanager.h"
#include "SDL_waylandkeyboard.h"

enum SDL_WaylandAxisEvent
{
    AXIS_EVENT_CONTINUOUS = 0,
    AXIS_EVENT_DISCRETE,
    AXIS_EVENT_VALUE120
};

struct SDL_WaylandTabletSeat;

struct SDL_WaylandTabletObjectListNode
{
    void *object;
    struct SDL_WaylandTabletObjectListNode *next;
};

struct SDL_WaylandTabletInput
{
    struct SDL_WaylandInput *sdlWaylandInput;
    struct zwp_tablet_seat_v2 *seat;

    struct SDL_WaylandTabletObjectListNode *tablets;
    struct SDL_WaylandTabletObjectListNode *tools;
    struct SDL_WaylandTabletObjectListNode *pads;

    Uint32 id;
    Uint32 num_pens; /* next pen ID is num_pens+1 */
    struct SDL_WaylandCurrentPen
    {
        SDL_Pen *builder;                /* pen that is being defined or receiving updates, if any */
        SDL_bool builder_guid_complete;  /* have complete/precise GUID information */
        SDL_PenStatusInfo update_status; /* collects pen update information before sending event */
	Uint16 buttons_pressed;        /* Mask of newly pressed buttons, plus SDL_PEN_DOWN_MASK for PEN_DOWN */
        Uint16 buttons_released;       /* Mask of newly pressed buttons, plus SDL_PEN_DOWN_MASK for PEN_UP */
        Uint32 serial;                 /* Most recent serial event number observed, or 0 */
        SDL_WindowData *update_window; /* NULL while no event is in progress, otherwise the affected window */
    } current_pen;

    SDL_WindowData *tool_focus;
    uint32_t tool_prox_serial;

    /* Last motion end location (kept separate from sx_w, sy_w for the mouse pointer) */
    wl_fixed_t sx_w;
    wl_fixed_t sy_w;
};

typedef struct
{
    int32_t repeat_rate;     /* Repeat rate in range of [1, 1000] character(s) per second */
    int32_t repeat_delay_ms; /* Time to first repeat event in milliseconds */
    SDL_bool is_initialized;

    SDL_bool is_key_down;
    uint32_t key;
    Uint64 wl_press_time_ns;  /* Key press time as reported by the Wayland API */
    Uint64 sdl_press_time_ns; /* Key press time expressed in SDL ticks */
    Uint64 next_repeat_ns;    /* Next repeat event in nanoseconds */
    uint32_t scancode;
    char text[8];
} SDL_WaylandKeyboardRepeat;

struct SDL_WaylandInput
{
    SDL_VideoData *display;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_touch *touch;
    struct wl_keyboard *keyboard;
    SDL_WaylandDataDevice *data_device;
    SDL_WaylandPrimarySelectionDevice *primary_selection_device;
    SDL_WaylandTextInput *text_input;
    struct zwp_relative_pointer_v1 *relative_pointer;
    struct zwp_input_timestamps_v1 *keyboard_timestamps;
    struct zwp_input_timestamps_v1 *pointer_timestamps;
    struct zwp_input_timestamps_v1 *touch_timestamps;
    SDL_WindowData *pointer_focus;
    SDL_WindowData *keyboard_focus;
    uint32_t pointer_enter_serial;

    /* High-resolution event timestamps */
    Uint64 keyboard_timestamp_ns;
    Uint64 pointer_timestamp_ns;
    Uint64 touch_timestamp_ns;

    /* Last motion location */
    wl_fixed_t sx_w;
    wl_fixed_t sy_w;

    uint32_t buttons_pressed;

    /* The serial of the last implicit grab event for window activation and selection data. */
    Uint32 last_implicit_grab_serial;

    struct
    {
        struct xkb_keymap *keymap;
        struct xkb_state *state;
        struct xkb_compose_table *compose_table;
        struct xkb_compose_state *compose_state;

        /* Keyboard layout "group" */
        uint32_t current_group;

        /* Modifier bitshift values */
        uint32_t idx_shift;
        uint32_t idx_ctrl;
        uint32_t idx_alt;
        uint32_t idx_gui;
        uint32_t idx_mode;
        uint32_t idx_num;
        uint32_t idx_caps;

        /* Current system modifier flags */
        uint32_t wl_pressed_modifiers;
        uint32_t wl_locked_modifiers;
    } xkb;

    /* information about axis events on current frame */
    struct
    {
        enum SDL_WaylandAxisEvent x_axis_type;
        float x;

        enum SDL_WaylandAxisEvent y_axis_type;
        float y;

        /* Event timestamp in nanoseconds */
        Uint64 timestamp_ns;
        SDL_MouseWheelDirection direction;
    } pointer_curr_axis_info;

    SDL_WaylandKeyboardRepeat keyboard_repeat;

    struct SDL_WaylandTabletInput *tablet;

    /* are we forcing relative mouse mode? */
    SDL_bool cursor_visible;
    SDL_bool relative_mode_override;
    SDL_bool warp_emulation_prohibited;
    SDL_bool keyboard_is_virtual;

    /* Current SDL modifier flags */
    SDL_Keymod pressed_modifiers;
    SDL_Keymod locked_modifiers;
};

struct SDL_WaylandTool
{
    SDL_PenID penid;
    struct SDL_WaylandTabletInput *tablet;
};

extern Uint64 Wayland_GetTouchTimestamp(struct SDL_WaylandInput *input, Uint32 wl_timestamp_ms);

extern void Wayland_PumpEvents(SDL_VideoDevice *_this);
extern void Wayland_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window);
extern int Wayland_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS);

extern void Wayland_add_data_device_manager(SDL_VideoData *d, uint32_t id, uint32_t version);
extern void Wayland_add_primary_selection_device_manager(SDL_VideoData *d, uint32_t id, uint32_t version);
extern void Wayland_add_text_input_manager(SDL_VideoData *d, uint32_t id, uint32_t version);

extern void Wayland_display_add_input(SDL_VideoData *d, uint32_t id, uint32_t version);
extern void Wayland_display_destroy_input(SDL_VideoData *d);

extern int Wayland_input_lock_pointer(struct SDL_WaylandInput *input);
extern int Wayland_input_unlock_pointer(struct SDL_WaylandInput *input);

extern int Wayland_input_confine_pointer(struct SDL_WaylandInput *input, SDL_Window *window);
extern int Wayland_input_unconfine_pointer(struct SDL_WaylandInput *input, SDL_Window *window);

extern int Wayland_input_grab_keyboard(SDL_Window *window, struct SDL_WaylandInput *input);
extern int Wayland_input_ungrab_keyboard(SDL_Window *window);

extern void Wayland_input_add_tablet(struct SDL_WaylandInput *input, struct SDL_WaylandTabletManager *tablet_manager);
extern void Wayland_input_destroy_tablet(struct SDL_WaylandInput *input);

extern void Wayland_RegisterTimestampListeners(struct SDL_WaylandInput *input);

/* The implicit grab serial needs to be updated on:
 * - Keyboard key down/up
 * - Mouse button down
 * - Touch event down
 * - Tablet tool down
 * - Tablet tool button down/up
 */
extern void Wayland_UpdateImplicitGrabSerial(struct SDL_WaylandInput *input, Uint32 serial);

#endif /* SDL_waylandevents_h_ */
