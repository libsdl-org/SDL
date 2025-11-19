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

#ifndef SDL_waylandevents_h_
#define SDL_waylandevents_h_

#include "../../events/SDL_keymap_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_pen_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylanddatamanager.h"

enum SDL_WaylandAxisEvent
{
    SDL_WAYLAND_AXIS_EVENT_CONTINUOUS = 0,
    SDL_WAYLAND_AXIS_EVENT_DISCRETE,
    SDL_WAYLAND_AXIS_EVENT_VALUE120
};

typedef struct
{
    Sint32 repeat_rate;     // Repeat rate in range of [1, 1000] character(s) per second
    Sint32 repeat_delay_ms; // Time to first repeat event in milliseconds
    Uint32 keyboard_id;     // ID of the source keyboard.
    bool is_initialized;

    bool is_key_down;
    Uint32 key;
    Uint32 wl_press_time_ms;  // Key press time as reported by the Wayland API in milliseconds
    Uint64 base_time_ns;      // Key press time as reported by the Wayland API in nanoseconds
    Uint64 sdl_press_time_ns; // Key press time expressed in SDL ticks
    Uint64 next_repeat_ns;    // Next repeat event in nanoseconds
    Uint32 scancode;
    char text[8];
} SDL_WaylandKeyboardRepeat;

typedef struct SDL_WaylandCursorState
{
    SDL_CursorData *current_cursor;
    struct wp_cursor_shape_device_v1 *cursor_shape;
    struct wl_surface *surface;
    struct wp_viewport *viewport;

    double scale;

    // Pointer to the internal data for system cursors.
    void *system_cursor_handle;

    // The cursor animation thread lock must be held when modifying this.
    struct wl_callback *frame_callback;

    Uint64 last_frame_callback_time_ms;
    Uint32 current_frame_time_ms;
    int current_frame;
    SDL_HitTestResult hit_test_result;
} SDL_WaylandCursorState;

typedef struct SDL_WaylandPenTool  // a stylus, etc, on a tablet.
{
    SDL_PenID instance_id;
    SDL_PenInfo info;
    SDL_WindowData *focus;
    struct zwp_tablet_tool_v2 *wltool;
    Uint32 proximity_serial;

    struct
    {
        float x;
        float y;

        float axes[SDL_PEN_AXIS_COUNT];
        Uint32 axes_set;

        enum
        {
            WAYLAND_TABLET_TOOL_BUTTON_NONE = 0,
            WAYLAND_TABLET_TOOL_BUTTON_DOWN,
            WAYLAND_TABLET_TOOL_BUTTON_UP
        } buttons[3];

        enum
        {
            WAYLAND_TABLET_TOOL_STATE_NONE = 0,
            WAYLAND_TABLET_TOOL_STATE_DOWN,
            WAYLAND_TABLET_TOOL_STATE_UP
        } tool_state;

        bool in_proximity;

        bool have_motion;
        bool have_proximity;
    } frame;

    SDL_WaylandCursorState cursor_state;
    struct wl_list link;
} SDL_WaylandPenTool;

typedef struct SDL_WaylandSeat
{
    SDL_VideoData *display;
    struct wl_seat *wl_seat;
    SDL_WaylandDataDevice *data_device;
    SDL_WaylandPrimarySelectionDevice *primary_selection_device;
    char *name;
    struct wl_list link;

    Uint32 last_implicit_grab_serial; // The serial of the last implicit grab event for window activation and selection data.
    Uint32 registry_id;               // The ID of the Wayland seat object,

    struct
    {
        struct wl_keyboard *wl_keyboard;
        struct zwp_input_timestamps_v1 *timestamps;
        struct zwp_keyboard_shortcuts_inhibitor_v1 *key_inhibitor;
        SDL_WindowData *focus;
        SDL_Keymap **sdl_keymap;
        char *current_locale;

        SDL_WaylandKeyboardRepeat repeat;
        Uint64 highres_timestamp_ns;

        // Current SDL modifier flags
        SDL_Keymod pressed_modifiers;
        SDL_Keymod locked_modifiers;

        SDL_KeyboardID sdl_id;
        bool is_virtual;

        struct
        {
            struct xkb_keymap *keymap;
            struct xkb_state *state;
            struct xkb_compose_table *compose_table;
            struct xkb_compose_state *compose_state;

            // Current keyboard layout (aka 'group')
            xkb_layout_index_t num_layouts;
            xkb_layout_index_t current_layout;

            // Modifier bitshift values
            xkb_mod_mask_t shift_mask;
            xkb_mod_mask_t ctrl_mask;
            xkb_mod_mask_t alt_mask;
            xkb_mod_mask_t gui_mask;
            xkb_mod_mask_t level3_mask;
            xkb_mod_mask_t level5_mask;
            xkb_mod_mask_t num_mask;
            xkb_mod_mask_t caps_mask;

            // Current system modifier flags
            xkb_mod_mask_t wl_pressed_modifiers;
            xkb_mod_mask_t wl_latched_modifiers;
            xkb_mod_mask_t wl_locked_modifiers;
        } xkb;
    } keyboard;

    struct
    {
        struct wl_pointer *wl_pointer;
        struct zwp_relative_pointer_v1 *relative_pointer;
        struct zwp_input_timestamps_v1 *timestamps;
        struct zwp_locked_pointer_v1 *locked_pointer;
        struct zwp_confined_pointer_v1 *confined_pointer;
        struct zwp_pointer_gesture_pinch_v1 *gesture_pinch;

        SDL_WindowData *focus;
        SDL_CursorData *current_cursor;

        // According to the spec, a seat can only have one active gesture of any type at a time.
        SDL_WindowData *gesture_focus;

        Uint64 highres_timestamp_ns;
        Uint32 enter_serial;
        SDL_MouseButtonFlags buttons_pressed;
        SDL_Point last_motion;
        bool is_confined;

        SDL_MouseID sdl_id;

        // Information about axis events on the current frame
        struct
        {
            bool have_absolute;
            bool have_relative;
            bool have_axis;
            bool have_enter;

            struct
            {
                wl_fixed_t sx;
                wl_fixed_t sy;
            } absolute;

            struct
            {
                wl_fixed_t dx;
                wl_fixed_t dy;
                wl_fixed_t dx_unaccel;
                wl_fixed_t dy_unaccel;
            } relative;

            struct
            {
                enum SDL_WaylandAxisEvent x_axis_type;
                float x;

                enum SDL_WaylandAxisEvent y_axis_type;
                float y;

                SDL_MouseWheelDirection direction;
            } axis;

            // Event timestamp in nanoseconds
            Uint64 timestamp_ns;
        } pending_frame;

        SDL_WaylandCursorState cursor_state;
    } pointer;

    struct
    {
        struct wl_touch *wl_touch;
        struct zwp_input_timestamps_v1 *timestamps;
        Uint64 highres_timestamp_ns;
        struct wl_list points;
    } touch;

    struct
    {
        struct zwp_text_input_v3 *zwp_text_input;
        SDL_Rect text_input_rect;
        int text_input_cursor;
        bool enabled;
        bool has_preedit;
    } text_input;

    struct
    {
        struct zwp_tablet_seat_v2 *wl_tablet_seat;
        struct wl_list tool_list;
    } tablet;
} SDL_WaylandSeat;


extern Uint64 Wayland_GetTouchTimestamp(struct SDL_WaylandSeat *seat, Uint32 wl_timestamp_ms);

extern void Wayland_PumpEvents(SDL_VideoDevice *_this);
extern void Wayland_SendWakeupEvent(SDL_VideoDevice *_this, SDL_Window *window);
extern int Wayland_WaitEventTimeout(SDL_VideoDevice *_this, Sint64 timeoutNS);

extern void Wayland_DisplayInitInputTimestampManager(SDL_VideoData *display);
extern void Wayland_DisplayInitCursorShapeManager(SDL_VideoData *display);
extern void Wayland_DisplayInitPointerGestureManager(SDL_VideoData *display);
extern void Wayland_DisplayInitTabletManager(SDL_VideoData *display);
extern void Wayland_DisplayInitDataDeviceManager(SDL_VideoData *display);
extern void Wayland_DisplayInitPrimarySelectionDeviceManager(SDL_VideoData *display);

extern void Wayland_DisplayCreateTextInputManager(SDL_VideoData *d, uint32_t id);

extern void Wayland_DisplayCreateSeat(SDL_VideoData *display, struct wl_seat *wl_seat, Uint32 id);
extern void Wayland_SeatDestroy(SDL_WaylandSeat *seat, bool shutting_down);

extern void Wayland_SeatUpdatePointerGrab(SDL_WaylandSeat *seat);
extern void Wayland_DisplayUpdatePointerGrabs(SDL_VideoData *display, SDL_WindowData *window);
extern void Wayland_DisplayUpdateKeyboardGrabs(SDL_VideoData *display, SDL_WindowData *window);
extern void Wayland_DisplayRemoveWindowReferencesFromSeats(SDL_VideoData *display, SDL_WindowData *window);

/* The implicit grab serial needs to be updated on:
 * - Keyboard key down/up
 * - Mouse button down
 * - Touch event down
 * - Tablet tool down
 * - Tablet tool button down/up
 */
extern void Wayland_UpdateImplicitGrabSerial(struct SDL_WaylandSeat *seat, Uint32 serial);

#endif // SDL_waylandevents_h_
