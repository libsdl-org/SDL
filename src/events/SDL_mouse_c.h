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

#ifndef SDL_mouse_c_h_
#define SDL_mouse_c_h_

/* Mouse events not associated with a specific input device */
#define SDL_GLOBAL_MOUSE_ID     0

/* The default mouse input device, for platforms that don't have multiple mice */
#define SDL_DEFAULT_MOUSE_ID    1

struct SDL_Cursor
{
    struct SDL_Cursor *next;
    void *driverdata;
};

typedef struct
{
    SDL_MouseID mouseID;
    Uint32 buttonstate;
} SDL_MouseInputSource;

typedef struct
{
    float last_x, last_y;
    Uint64 last_timestamp;
    Uint8 click_count;
} SDL_MouseClickState;

typedef struct
{
    /* Create a cursor from a surface */
    SDL_Cursor *(*CreateCursor)(SDL_Surface *surface, int hot_x, int hot_y);

    /* Create a system cursor */
    SDL_Cursor *(*CreateSystemCursor)(SDL_SystemCursor id);

    /* Show the specified cursor, or hide if cursor is NULL */
    int (*ShowCursor)(SDL_Cursor *cursor);

    /* This is called when a mouse motion event occurs */
    int (*MoveCursor)(SDL_Cursor *cursor);

    /* Free a window manager cursor */
    void (*FreeCursor)(SDL_Cursor *cursor);

    /* Warp the mouse to (x,y) within a window */
    int (*WarpMouse)(SDL_Window *window, float x, float y);

    /* Warp the mouse to (x,y) in screen space */
    int (*WarpMouseGlobal)(float x, float y);

    /* Set relative mode */
    int (*SetRelativeMouseMode)(SDL_bool enabled);

    /* Set mouse capture */
    int (*CaptureMouse)(SDL_Window *window);

    /* Get absolute mouse coordinates. (x) and (y) are never NULL and set to zero before call. */
    Uint32 (*GetGlobalMouseState)(float *x, float *y);

    /* Data common to all mice */
    SDL_Window *focus;
    float x;
    float y;
    float xdelta;
    float ydelta;
    float last_x, last_y; /* the last reported x and y coordinates */
    SDL_bool has_position;
    SDL_bool relative_mode;
    SDL_bool relative_mode_warp;
    SDL_bool relative_mode_warp_motion;
    SDL_bool enable_normal_speed_scale;
    float normal_speed_scale;
    SDL_bool enable_relative_speed_scale;
    float relative_speed_scale;
    SDL_bool enable_relative_system_scale;
    int num_system_scale_values;
    float *system_scale_values;
    Uint32 double_click_time;
    int double_click_radius;
    SDL_bool touch_mouse_events;
    SDL_bool mouse_touch_events;
    SDL_bool was_touch_mouse_events; /* Was a touch-mouse event pending? */
#ifdef SDL_PLATFORM_VITA
    Uint8 vita_touch_mouse_device;
#endif
    SDL_bool auto_capture;
    SDL_bool capture_desired;
    SDL_Window *capture_window;

    /* Data for input source state */
    int num_sources;
    SDL_MouseInputSource *sources;

    /* Data for double-click tracking */
    int num_clickstates;
    SDL_MouseClickState *clickstate;

    SDL_Cursor *cursors;
    SDL_Cursor *def_cursor;
    SDL_Cursor *cur_cursor;
    SDL_bool cursor_shown;

    /* Driver-dependent data. */
    void *driverdata;
} SDL_Mouse;

/* Initialize the mouse subsystem, called before the main video driver is initialized */
extern int SDL_PreInitMouse(void);

/* Finish initializing the mouse subsystem, called after the main video driver was initialized */
extern void SDL_PostInitMouse(void);

/* Return whether a device is actually a mouse */
extern SDL_bool SDL_IsMouse(Uint16 vendor, Uint16 product);

/* A mouse has been added to the system */
extern void SDL_AddMouse(SDL_MouseID mouseID, const char *name, SDL_bool send_event);

/* A mouse has been removed from the system */
extern void SDL_RemoveMouse(SDL_MouseID mouseID);

/* Get the mouse state structure */
extern SDL_Mouse *SDL_GetMouse(void);

/* Set the default mouse cursor */
extern void SDL_SetDefaultCursor(SDL_Cursor *cursor);

/* Set the mouse focus window */
extern void SDL_SetMouseFocus(SDL_Window *window);

/* Update the mouse capture window */
extern int SDL_UpdateMouseCapture(SDL_bool force_release);

/* Get the current mouse button state for a mouse */
Uint32 SDL_GetMouseButtonState(SDL_Mouse *mouse, SDL_MouseID mouseID, SDL_bool include_touch);

/* You can set either a single scale, or a set of {speed, scale} values in sorted order */
extern int SDL_SetMouseSystemScale(int num_values, const float *values);

/* Send a mouse motion event */
extern int SDL_SendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, SDL_bool relative, float x, float y);

/* Send a mouse button event */
extern int SDL_SendMouseButton(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button);

/* Send a mouse button event with a click count */
extern int SDL_SendMouseButtonClicks(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button, int clicks);

/* Send a mouse wheel event */
extern int SDL_SendMouseWheel(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, float x, float y, SDL_MouseWheelDirection direction);

/* Warp the mouse within the window, potentially overriding relative mode */
extern void SDL_PerformWarpMouseInWindow(SDL_Window *window, float x, float y, SDL_bool ignore_relative_mode);

/* TODO RECONNECT: Set mouse state to "zero" */
#if 0
extern void SDL_ResetMouse(void);
#endif /* 0 */

/* Check if mouse position is within window or captured by window */
extern SDL_bool SDL_MousePositionInWindow(SDL_Window *window, float x, float y);

/* Shutdown the mouse subsystem */
extern void SDL_QuitMouse(void);

#endif /* SDL_mouse_c_h_ */
