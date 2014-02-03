/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

/*
  Contributed by Brandon Schaefer, <brandon.schaefer@canonical.com>
*/

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MIR

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../events/scancodes_xfree86.h"

#include "SDL_mirevents.h"
#include "SDL_mirwindow.h"

#include <xkbcommon/xkbcommon.h>

#include "SDL_mirdyn.h"

static void
HandleKeyText(int32_t key_code)
{
    char text[8];
    int size = 0;

    size = MIR_xkb_keysym_to_utf8(key_code, text, sizeof text);

    if (size > 0) {
        text[size] = '\0';
        SDL_SendKeyboardText(text);
    }
}

static void
CheckKeyboardFocus(SDL_Window* sdl_window)
{
    SDL_Window* keyboard_window = SDL_GetKeyboardFocus();

    if (keyboard_window != sdl_window)
        SDL_SetKeyboardFocus(sdl_window);
}


/* FIXME
   Mir still needs to implement its IM API, for now we assume
   a single key press produces a character.
*/
static void
HandleKeyEvent(MirKeyEvent const ev, SDL_Window* window)
{
    uint32_t scancode = SDL_SCANCODE_UNKNOWN;
    Uint8 key_state = ev.action == mir_key_action_up ? SDL_RELEASED : SDL_PRESSED;

    CheckKeyboardFocus(window);

    if (ev.scan_code < SDL_arraysize(xfree86_scancode_table2))
        scancode = xfree86_scancode_table2[ev.scan_code];

    if (scancode != SDL_SCANCODE_UNKNOWN)
        SDL_SendKeyboardKey(key_state, scancode);

    if (key_state == SDL_PRESSED)
        HandleKeyText(ev.key_code);
}

static void
HandleMouseButton(SDL_Window* sdl_window, Uint8 state, MirMotionButton button_state)
{
    static uint32_t last_sdl_button;
    uint32_t sdl_button;

    switch (button_state) {
        case mir_motion_button_primary:
            sdl_button = SDL_BUTTON_LEFT;
            break;
        case mir_motion_button_secondary:
            sdl_button = SDL_BUTTON_RIGHT;
            break;
        case mir_motion_button_tertiary:
            sdl_button = SDL_BUTTON_MIDDLE;
            break;
        case mir_motion_button_forward:
            sdl_button = SDL_BUTTON_X1;
            break;
        case mir_motion_button_back:
            sdl_button = SDL_BUTTON_X2;
            break;
        default:
            sdl_button = last_sdl_button;
            break;
    }

    last_sdl_button = sdl_button;
    SDL_SendMouseButton(sdl_window, 0, state, sdl_button);
}

static void
HandleTouchPress(int device_id, int source_id, SDL_bool down, float x, float y, float pressure)
{
    SDL_SendTouch(device_id, source_id, down, x, y, pressure);
}

static void
HandleTouchMotion(int device_id, int source_id, float x, float y, float pressure)
{
    SDL_SendTouchMotion(device_id, source_id, x, y, pressure);
}

static void
HandleMouseMotion(SDL_Window* sdl_window, int x, int y)
{
    SDL_SendMouseMotion(sdl_window, 0, 0, x, y);
}

static void
HandleMouseScroll(SDL_Window* sdl_window, int hscroll, int vscroll)
{
    SDL_SendMouseWheel(sdl_window, 0, hscroll, vscroll);
}

static void
AddTouchDevice(int device_id)
{
    if (SDL_AddTouch(device_id, "") < 0)
        SDL_SetError("Error: can't add touch %s, %d", __FILE__, __LINE__);
}

static void
HandleTouchEvent(MirMotionEvent const motion, int cord_index, SDL_Window* sdl_window)
{
    int device_id = motion.device_id;
    int id = motion.pointer_coordinates[cord_index].id;

    int width  = sdl_window->w;
    int height = sdl_window->h;
    float x   = motion.pointer_coordinates[cord_index].x;
    float y   = motion.pointer_coordinates[cord_index].y;

    float n_x = x / width;
    float n_y = y / height;
    float pressure = motion.pointer_coordinates[cord_index].pressure;

    AddTouchDevice(motion.device_id);

    switch (motion.action) {
        case mir_motion_action_down:
        case mir_motion_action_pointer_down:
            HandleTouchPress(device_id, id, SDL_TRUE, n_x, n_y, pressure);
            break;
        case mir_motion_action_up:
        case mir_motion_action_pointer_up:
            HandleTouchPress(device_id, id, SDL_FALSE, n_x, n_y, pressure);
            break;
        case mir_motion_action_hover_move:
        case mir_motion_action_move:
            HandleTouchMotion(device_id, id, n_x, n_y, pressure);
            break;
        default:
            break;
    }
}

static void
HandleMouseEvent(MirMotionEvent const motion, int cord_index, SDL_Window* sdl_window)
{
    SDL_SetMouseFocus(sdl_window);

    switch (motion.action) {
        case mir_motion_action_down:
        case mir_motion_action_pointer_down:
            HandleMouseButton(sdl_window, SDL_PRESSED, motion.button_state);
            break;
        case mir_motion_action_up:
        case mir_motion_action_pointer_up:
            HandleMouseButton(sdl_window, SDL_RELEASED, motion.button_state);
            break;
        case mir_motion_action_hover_move:
        case mir_motion_action_move:
            HandleMouseMotion(sdl_window,
                              motion.pointer_coordinates[cord_index].x,
                              motion.pointer_coordinates[cord_index].y);
            break;
        case mir_motion_action_outside:
            SDL_SetMouseFocus(NULL);
            break;
#if 0  /* !!! FIXME: needs a newer set of dev headers than Ubuntu 13.10 is shipping atm. */
        case mir_motion_action_scroll:
            HandleMouseScroll(sdl_window,
                              motion.pointer_coordinates[cord_index].hscroll,
                              motion.pointer_coordinates[cord_index].vscroll);
            break;
#endif
        case mir_motion_action_cancel:
        case mir_motion_action_hover_enter:
        case mir_motion_action_hover_exit:
            break;
        default:
            break;
    }
}

static void
HandleMotionEvent(MirMotionEvent const motion, SDL_Window* sdl_window)
{
    int cord_index;
    for (cord_index = 0; cord_index < motion.pointer_count; cord_index++) {
#if 0  /* !!! FIXME: needs a newer set of dev headers than Ubuntu 13.10 is shipping atm. */
        if (motion.pointer_coordinates[cord_index].tool_type == mir_motion_tool_type_mouse) {
            HandleMouseEvent(motion, cord_index, sdl_window);
        }
        else if (motion.pointer_coordinates[cord_index].tool_type == mir_motion_tool_type_finger) {
            HandleTouchEvent(motion, cord_index, sdl_window);
        }
#else
        HandleMouseEvent(motion, cord_index, sdl_window);
#endif
    }
}

void
MIR_HandleInput(MirSurface* surface, MirEvent const* ev, void* context)
{
    SDL_Window* window = (SDL_Window*)context;
    switch (ev->type) {
        case (mir_event_type_key):
            HandleKeyEvent(ev->key, window);
            break;
        case (mir_event_type_motion):
            HandleMotionEvent(ev->motion, window);
            break;
        default:
            break;
    }
}

#endif /* SDL_VIDEO_DRIVER_MIR */

/* vi: set ts=4 sw=4 expandtab: */
