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

/* General mouse handling code for SDL */

#include "../SDL_hints_c.h"
#include "../video/SDL_sysvideo.h"
#include "SDL_events_c.h"
#include "SDL_mouse_c.h"
#include "SDL_pen_c.h"
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)
#include "../core/windows/SDL_windows.h" // For GetDoubleClickTime()
#endif

/* #define DEBUG_MOUSE */

typedef struct SDL_MouseInstance
{
    SDL_MouseID instance_id;
    char *name;
} SDL_MouseInstance;

/* The mouse state */
static SDL_Mouse SDL_mouse;
static int SDL_mouse_count;
static SDL_MouseInstance *SDL_mice;

/* for mapping mouse events to touch */
static SDL_bool track_mouse_down = SDL_FALSE;

static int SDL_PrivateSendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, SDL_bool relative, float x, float y);

static void SDLCALL SDL_MouseDoubleClickTimeChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    if (hint && *hint) {
        mouse->double_click_time = SDL_atoi(hint);
    } else {
#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
        mouse->double_click_time = GetDoubleClickTime();
#else
        mouse->double_click_time = 500;
#endif
    }
}

static void SDLCALL SDL_MouseRelativeClipIntervalChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    if (hint && *hint) {
        mouse->relative_mode_clip_interval = SDL_atoi(hint);
    } else {
        mouse->relative_mode_clip_interval = 3000;
    }
}

static void SDLCALL SDL_MouseDoubleClickRadiusChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    if (hint && *hint) {
        mouse->double_click_radius = SDL_atoi(hint);
    } else {
        mouse->double_click_radius = 32; /* 32 pixels seems about right for touch interfaces */
    }
}

static void SDLCALL SDL_MouseNormalSpeedScaleChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    if (hint && *hint) {
        mouse->enable_normal_speed_scale = SDL_TRUE;
        mouse->normal_speed_scale = (float)SDL_atof(hint);
    } else {
        mouse->enable_normal_speed_scale = SDL_FALSE;
        mouse->normal_speed_scale = 1.0f;
    }
}

static void SDLCALL SDL_MouseRelativeSpeedScaleChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    if (hint && *hint) {
        mouse->enable_relative_speed_scale = SDL_TRUE;
        mouse->relative_speed_scale = (float)SDL_atof(hint);
    } else {
        mouse->enable_relative_speed_scale = SDL_FALSE;
        mouse->relative_speed_scale = 1.0f;
    }
}

static void SDLCALL SDL_MouseRelativeSystemScaleChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    mouse->enable_relative_system_scale = SDL_GetStringBoolean(hint, SDL_FALSE);
}

static void SDLCALL SDL_TouchMouseEventsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    mouse->touch_mouse_events = SDL_GetStringBoolean(hint, SDL_TRUE);
}

#ifdef SDL_PLATFORM_VITA
static void SDLCALL SDL_VitaTouchMouseDeviceChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;
    if (hint) {
        switch (*hint) {
        default:
        case '0':
            mouse->vita_touch_mouse_device = 0;
            break;
        case '1':
            mouse->vita_touch_mouse_device = 1;
            break;
        case '2':
            mouse->vita_touch_mouse_device = 2;
            break;
        }
    }
}
#endif

static void SDLCALL SDL_MouseTouchEventsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;
    SDL_bool default_value;

#if defined(SDL_PLATFORM_ANDROID) || (defined(SDL_PLATFORM_IOS) && !defined(SDL_PLATFORM_TVOS))
    default_value = SDL_TRUE;
#else
    default_value = SDL_FALSE;
#endif
    mouse->mouse_touch_events = SDL_GetStringBoolean(hint, default_value);

    if (mouse->mouse_touch_events) {
        SDL_AddTouch(SDL_MOUSE_TOUCHID, SDL_TOUCH_DEVICE_DIRECT, "mouse_input");
    }
}

static void SDLCALL SDL_MouseAutoCaptureChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;
    SDL_bool auto_capture = SDL_GetStringBoolean(hint, SDL_TRUE);

    if (auto_capture != mouse->auto_capture) {
        mouse->auto_capture = auto_capture;
        SDL_UpdateMouseCapture(SDL_FALSE);
    }
}

static void SDLCALL SDL_MouseRelativeWarpMotionChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    mouse->relative_mode_warp_motion = SDL_GetStringBoolean(hint, SDL_FALSE);
}

static void SDLCALL SDL_MouseRelativeCursorVisibleChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Mouse *mouse = (SDL_Mouse *)userdata;

    mouse->relative_mode_cursor_visible = SDL_GetStringBoolean(hint, SDL_FALSE);
}

/* Public functions */
int SDL_PreInitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    SDL_zerop(mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME,
                        SDL_MouseDoubleClickTimeChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS,
                        SDL_MouseDoubleClickRadiusChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_NORMAL_SPEED_SCALE,
                        SDL_MouseNormalSpeedScaleChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE,
                        SDL_MouseRelativeSpeedScaleChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,
                        SDL_MouseRelativeSystemScaleChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_TOUCH_MOUSE_EVENTS,
                        SDL_TouchMouseEventsChanged, mouse);

#ifdef SDL_PLATFORM_VITA
    SDL_AddHintCallback(SDL_HINT_VITA_TOUCH_MOUSE_DEVICE,
                        SDL_VitaTouchMouseDeviceChanged, mouse);
#endif

    SDL_AddHintCallback(SDL_HINT_MOUSE_TOUCH_EVENTS,
                        SDL_MouseTouchEventsChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_AUTO_CAPTURE,
                        SDL_MouseAutoCaptureChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_WARP_MOTION,
                        SDL_MouseRelativeWarpMotionChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE,
                        SDL_MouseRelativeCursorVisibleChanged, mouse);

    SDL_AddHintCallback(SDL_HINT_MOUSE_RELATIVE_CLIP_INTERVAL,
                        SDL_MouseRelativeClipIntervalChanged, mouse);

    mouse->was_touch_mouse_events = SDL_FALSE; /* no touch to mouse movement event pending */

    mouse->cursor_shown = SDL_TRUE;
    return 0;
}

void SDL_PostInitMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    /* Create a dummy mouse cursor for video backends that don't support true cursors,
     * so that mouse grab and focus functionality will work.
     */
    if (!mouse->def_cursor) {
        SDL_Surface *surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ARGB8888);
        if (surface) {
            SDL_memset(surface->pixels, 0, (size_t)surface->h * surface->pitch);
            SDL_SetDefaultCursor(SDL_CreateColorCursor(surface, 0, 0));
            SDL_DestroySurface(surface);
        }
    }

    SDL_PenInit();
}

SDL_bool SDL_IsMouse(Uint16 vendor, Uint16 product)
{
    /* Eventually we'll have a blacklist of devices that enumerate as mice but aren't really */
    return SDL_TRUE;
}

static int SDL_GetMouseIndex(SDL_MouseID mouseID)
{
    for (int i = 0; i < SDL_mouse_count; ++i) {
        if (mouseID == SDL_mice[i].instance_id) {
            return i;
        }
    }
    return -1;
}

void SDL_AddMouse(SDL_MouseID mouseID, const char *name, SDL_bool send_event)
{
    int mouse_index = SDL_GetMouseIndex(mouseID);
    if (mouse_index >= 0) {
        /* We already know about this mouse */
        return;
    }

    SDL_assert(mouseID != 0);

    SDL_MouseInstance *mice = (SDL_MouseInstance *)SDL_realloc(SDL_mice, (SDL_mouse_count + 1) * sizeof(*mice));
    if (!mice) {
        return;
    }
    SDL_MouseInstance *instance = &mice[SDL_mouse_count];
    instance->instance_id = mouseID;
    instance->name = SDL_strdup(name ? name : "");
    SDL_mice = mice;
    ++SDL_mouse_count;

    if (send_event) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_MOUSE_ADDED;
        event.mdevice.which = mouseID;
        SDL_PushEvent(&event);
    }
}

void SDL_RemoveMouse(SDL_MouseID mouseID, SDL_bool send_event)
{
    int mouse_index = SDL_GetMouseIndex(mouseID);
    if (mouse_index < 0) {
        /* We don't know about this mouse */
        return;
    }

    SDL_FreeLater(SDL_mice[mouse_index].name);  // SDL_GetMouseInstanceName returns this pointer.

    if (mouse_index != SDL_mouse_count - 1) {
        SDL_memcpy(&SDL_mice[mouse_index], &SDL_mice[mouse_index + 1], (SDL_mouse_count - mouse_index - 1) * sizeof(SDL_mice[mouse_index]));
    }
    --SDL_mouse_count;

    /* Remove any mouse input sources for this mouseID */
    SDL_Mouse *mouse = SDL_GetMouse();
    for (int i = 0; i < mouse->num_sources; ++i) {
        SDL_MouseInputSource *source = &mouse->sources[i];
        if (source->mouseID == mouseID) {
            if (i != mouse->num_sources - 1) {
                SDL_memcpy(&mouse->sources[i], &mouse->sources[i + 1], (mouse->num_sources - i - 1) * sizeof(mouse->sources[i]));
            }
            --mouse->num_sources;
            break;
        }
    }

    if (send_event) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_MOUSE_REMOVED;
        event.mdevice.which = mouseID;
        SDL_PushEvent(&event);
    }
}

SDL_bool SDL_HasMouse(void)
{
    return (SDL_mouse_count > 0);
}

SDL_MouseID *SDL_GetMice(int *count)
{
    int i;
    SDL_MouseID *mice;

    mice = (SDL_JoystickID *)SDL_malloc((SDL_mouse_count + 1) * sizeof(*mice));
    if (mice) {
        if (count) {
            *count = SDL_mouse_count;
        }

        for (i = 0; i < SDL_mouse_count; ++i) {
            mice[i] = SDL_mice[i].instance_id;
        }
        mice[i] = 0;
    } else {
        if (count) {
            *count = 0;
        }
    }

    return mice;
}

const char *SDL_GetMouseInstanceName(SDL_MouseID instance_id)
{
    int mouse_index = SDL_GetMouseIndex(instance_id);
    if (mouse_index < 0) {
        return NULL;
    }
    return SDL_mice[mouse_index].name;
}

void SDL_SetDefaultCursor(SDL_Cursor *cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (cursor == mouse->def_cursor) {
        return;
    }

    if (mouse->def_cursor) {
        SDL_Cursor *default_cursor = mouse->def_cursor;
        SDL_Cursor *prev, *curr;

        if (mouse->cur_cursor == mouse->def_cursor) {
            mouse->cur_cursor = NULL;
        }
        mouse->def_cursor = NULL;

        for (prev = NULL, curr = mouse->cursors; curr;
             prev = curr, curr = curr->next) {
            if (curr == default_cursor) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    mouse->cursors = curr->next;
                }

                break;
            }
        }

        if (mouse->FreeCursor && default_cursor->driverdata) {
            mouse->FreeCursor(default_cursor);
        } else {
            SDL_free(default_cursor);
        }
    }

    mouse->def_cursor = cursor;

    if (!mouse->cur_cursor) {
        SDL_SetCursor(cursor);
    }
}

SDL_Mouse *SDL_GetMouse(void)
{
    return &SDL_mouse;
}

static SDL_MouseButtonFlags SDL_GetMouseButtonState(SDL_Mouse *mouse, SDL_MouseID mouseID, SDL_bool include_touch)
{
    int i;
    SDL_MouseButtonFlags buttonstate = 0;

    for (i = 0; i < mouse->num_sources; ++i) {
        if (mouseID == SDL_GLOBAL_MOUSE_ID || mouseID == SDL_TOUCH_MOUSEID) {
            if (include_touch || mouse->sources[i].mouseID != SDL_TOUCH_MOUSEID) {
                buttonstate |= mouse->sources[i].buttonstate;
            }
        } else {
            if (mouseID == mouse->sources[i].mouseID) {
                buttonstate |= mouse->sources[i].buttonstate;
                break;
            }
        }
    }
    return buttonstate;
}

SDL_Window *SDL_GetMouseFocus(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    return mouse->focus;
}

/* TODO RECONNECT: Hello from the Wayland video driver!
 * This was once removed from SDL, but it's been added back in comment form
 * because we will need it when Wayland adds compositor reconnect support.
 * If you need this before we do, great! Otherwise, leave this alone, we'll
 * uncomment it at the right time.
 * -flibit
 */
#if 0
void SDL_ResetMouse(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    Uint32 buttonState = SDL_GetMouseButtonState(mouse, SDL_GLOBAL_MOUSE_ID, SDL_FALSE);
    int i;

    for (i = 1; i <= sizeof(buttonState)*8; ++i) {
        if (buttonState & SDL_BUTTON(i)) {
            SDL_SendMouseButton(0, mouse->focus, mouse->mouseID, SDL_RELEASED, i);
        }
    }
    SDL_assert(SDL_GetMouseButtonState(mouse, SDL_GLOBAL_MOUSE_ID, SDL_FALSE) == 0);
}
#endif /* 0 */

void SDL_SetMouseFocus(SDL_Window *window)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->focus == window) {
        return;
    }

    /* Actually, this ends up being a bad idea, because most operating
       systems have an implicit grab when you press the mouse button down
       so you can drag things out of the window and then get the mouse up
       when it happens.  So, #if 0...
    */
#if 0
    if (mouse->focus && !window) {
        /* We won't get anymore mouse messages, so reset mouse state */
        SDL_ResetMouse();
    }
#endif

    /* See if the current window has lost focus */
    if (mouse->focus) {
        SDL_SendWindowEvent(mouse->focus, SDL_EVENT_WINDOW_MOUSE_LEAVE, 0, 0);
    }

    mouse->focus = window;
    mouse->has_position = SDL_FALSE;

    if (mouse->focus) {
        SDL_SendWindowEvent(mouse->focus, SDL_EVENT_WINDOW_MOUSE_ENTER, 0, 0);
    }

    /* Update cursor visibility */
    SDL_SetCursor(NULL);
}

SDL_bool SDL_MousePositionInWindow(SDL_Window *window, float x, float y)
{
    if (!window) {
        return SDL_FALSE;
    }

    if (window && !(window->flags & SDL_WINDOW_MOUSE_CAPTURE)) {
        if (x < 0.0f || y < 0.0f || x >= (float)window->w || y >= (float)window->h) {
            return SDL_FALSE;
        }
    }
    return SDL_TRUE;
}

/* Check to see if we need to synthesize focus events */
static SDL_bool SDL_UpdateMouseFocus(SDL_Window *window, float x, float y, Uint32 buttonstate, SDL_bool send_mouse_motion)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_bool inWindow = SDL_MousePositionInWindow(window, x, y);

    if (!inWindow) {
        if (window == mouse->focus) {
#ifdef DEBUG_MOUSE
            SDL_Log("Mouse left window, synthesizing move & focus lost event\n");
#endif
            if (send_mouse_motion) {
                SDL_PrivateSendMouseMotion(0, window, SDL_GLOBAL_MOUSE_ID, SDL_FALSE, x, y);
            }
            SDL_SetMouseFocus(NULL);
        }
        return SDL_FALSE;
    }

    if (window != mouse->focus) {
#ifdef DEBUG_MOUSE
        SDL_Log("Mouse entered window, synthesizing focus gain & move event\n");
#endif
        SDL_SetMouseFocus(window);
        if (send_mouse_motion) {
            SDL_PrivateSendMouseMotion(0, window, SDL_GLOBAL_MOUSE_ID, SDL_FALSE, x, y);
        }
    }
    return SDL_TRUE;
}

int SDL_SendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, SDL_bool relative, float x, float y)
{
    if (window && !relative) {
        SDL_Mouse *mouse = SDL_GetMouse();
        if (!SDL_UpdateMouseFocus(window, x, y, SDL_GetMouseButtonState(mouse, mouseID, SDL_TRUE), (mouseID != SDL_TOUCH_MOUSEID))) {
            return 0;
        }
    }

    return SDL_PrivateSendMouseMotion(timestamp, window, mouseID, relative, x, y);
}

static float CalculateSystemScale(SDL_Mouse *mouse, SDL_Window *window, const float *x, const float *y)
{
    int i;
    int n = mouse->num_system_scale_values;
    float *v = mouse->system_scale_values;
    float speed, coef, scale;

    /* If we're using a single scale value, return that */
    if (n == 1) {
        scale = v[0];
    } else {
        speed = SDL_sqrtf((*x * *x) + (*y * *y));
        for (i = 0; i < (n - 2); i += 2) {
            if (speed < v[i + 2]) {
                break;
            }
        }
        if (i == (n - 2)) {
            scale = v[n - 1];
        } else if (speed <= v[i]) {
            scale = v[i + 1];
        } else {
            coef = (speed - v[i]) / (v[i + 2] - v[i]);
            scale = v[i + 1] + (coef * (v[i + 3] - v[i + 1]));
        }
    }
#ifdef SDL_PLATFORM_WIN32
    {
        /* On Windows the mouse speed is affected by the content scale */
        SDL_VideoDisplay *display;

        if (window) {
            display = SDL_GetVideoDisplayForWindow(window);
        } else {
            display = SDL_GetVideoDisplay(SDL_GetPrimaryDisplay());
        }
        if (display) {
            scale *= display->content_scale;
        }
    }
#endif
    return scale;
}

/* You can set either a single scale, or a set of {speed, scale} values in ascending order */
int SDL_SetMouseSystemScale(int num_values, const float *values)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    float *v;

    if (num_values == mouse->num_system_scale_values &&
        SDL_memcmp(values, mouse->system_scale_values, num_values * sizeof(*values)) == 0) {
        /* Nothing has changed */
        return 0;
    }

    if (num_values < 1) {
        return SDL_SetError("You must have at least one scale value");
    }

    if (num_values > 1) {
        /* Validate the values */
        int i;

        if (num_values < 4 || (num_values % 2) != 0) {
            return SDL_SetError("You must pass a set of {speed, scale} values");
        }

        for (i = 0; i < (num_values - 2); i += 2) {
            if (values[i] >= values[i + 2]) {
                return SDL_SetError("Speed values must be in ascending order");
            }
        }
    }

    v = (float *)SDL_realloc(mouse->system_scale_values, num_values * sizeof(*values));
    if (!v) {
        return -1;
    }
    SDL_memcpy(v, values, num_values * sizeof(*values));

    mouse->num_system_scale_values = num_values;
    mouse->system_scale_values = v;
    return 0;
}

static void GetScaledMouseDeltas(SDL_Mouse *mouse, SDL_Window *window, float *x, float *y)
{
    if (mouse->relative_mode) {
        if (mouse->enable_relative_speed_scale) {
            *x *= mouse->relative_speed_scale;
            *y *= mouse->relative_speed_scale;
        } else if (mouse->enable_relative_system_scale && mouse->num_system_scale_values > 0) {
            float relative_system_scale = CalculateSystemScale(mouse, window, x, y);
            *x *= relative_system_scale;
            *y *= relative_system_scale;
        }
    } else {
        if (mouse->enable_normal_speed_scale) {
            *x *= mouse->normal_speed_scale;
            *y *= mouse->normal_speed_scale;
        }
    }
}

static void ConstrainMousePosition(SDL_Mouse *mouse, SDL_Window *window, float *x, float *y)
{
    /* make sure that the pointers find themselves inside the windows,
       unless we have the mouse captured. */
    if (window && !(window->flags & SDL_WINDOW_MOUSE_CAPTURE)) {
        int x_min = 0, x_max = window->w - 1;
        int y_min = 0, y_max = window->h - 1;
        const SDL_Rect *confine = SDL_GetWindowMouseRect(window);

        if (confine) {
            SDL_Rect window_rect;
            SDL_Rect mouse_rect;

            window_rect.x = 0;
            window_rect.y = 0;
            window_rect.w = x_max + 1;
            window_rect.h = y_max + 1;
            if (SDL_GetRectIntersection(confine, &window_rect, &mouse_rect)) {
                x_min = mouse_rect.x;
                y_min = mouse_rect.y;
                x_max = x_min + mouse_rect.w - 1;
                y_max = y_min + mouse_rect.h - 1;
            }
        }

        if (*x >= (float)(x_max + 1)) {
            *x = SDL_max((float)x_max, mouse->last_x);
        }
        if (*x < (float)x_min) {
            *x = (float)x_min;
        }

        if (*y >= (float)(y_max + 1)) {
            *y = SDL_max((float)y_max, mouse->last_y);
        }
        if (*y < (float)y_min) {
            *y = (float)y_min;
        }
    }
}

static int SDL_PrivateSendMouseMotion(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, SDL_bool relative, float x, float y)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    int posted;
    float xrel = 0.0f;
    float yrel = 0.0f;

    if (!mouse->relative_mode && mouseID != SDL_TOUCH_MOUSEID && mouseID != SDL_PEN_MOUSEID) {
        /* We're not in relative mode, so all mouse events are global mouse events */
        mouseID = SDL_GLOBAL_MOUSE_ID;
    }

    /* SDL_HINT_MOUSE_TOUCH_EVENTS: controlling whether mouse events should generate synthetic touch events */
    if (mouse->mouse_touch_events) {
        if (mouseID != SDL_TOUCH_MOUSEID && !relative && track_mouse_down) {
            if (window) {
                float normalized_x = x / (float)window->w;
                float normalized_y = y / (float)window->h;
                SDL_SendTouchMotion(timestamp, SDL_MOUSE_TOUCHID, SDL_BUTTON_LEFT, window, normalized_x, normalized_y, 1.0f);
            }
        }
    }

    /* SDL_HINT_TOUCH_MOUSE_EVENTS: if not set, discard synthetic mouse events coming from platform layer */
    if (!mouse->touch_mouse_events && mouseID == SDL_TOUCH_MOUSEID) {
        return 0;
    }

    if (mouseID != SDL_TOUCH_MOUSEID && mouse->relative_mode_warp) {
        int w = 0, h = 0;
        float center_x, center_y;
        SDL_GetWindowSize(window, &w, &h);
        center_x = (float)w / 2.0f;
        center_y = (float)h / 2.0f;
        if (x >= SDL_floorf(center_x) && x <= SDL_ceilf(center_x) &&
            y >= SDL_floorf(center_y) && y <= SDL_ceilf(center_y)) {
            mouse->last_x = center_x;
            mouse->last_y = center_y;
            if (!mouse->relative_mode_warp_motion) {
                return 0;
            }
        } else {
            if (window && (window->flags & SDL_WINDOW_INPUT_FOCUS)) {
                if (mouse->WarpMouse) {
                    mouse->WarpMouse(window, center_x, center_y);
                } else {
                    SDL_PrivateSendMouseMotion(timestamp, window, mouseID, SDL_FALSE, center_x, center_y);
                }
            }
        }
    }

    if (relative) {
        GetScaledMouseDeltas(mouse, window, &x, &y);
        xrel = x;
        yrel = y;
        x = (mouse->last_x + xrel);
        y = (mouse->last_y + yrel);
        ConstrainMousePosition(mouse, window, &x, &y);
    } else {
        ConstrainMousePosition(mouse, window, &x, &y);

        if (mouse->has_position) {
            xrel = x - mouse->last_x;
            yrel = y - mouse->last_y;
        }
    }

    if (mouse->has_position && xrel == 0.0f && yrel == 0.0f) { /* Drop events that don't change state */
#ifdef DEBUG_MOUSE
        SDL_Log("Mouse event didn't change state - dropped!\n");
#endif
        return 0;
    }

    /* Ignore relative motion positioning the first touch */
    if (mouseID == SDL_TOUCH_MOUSEID && !SDL_GetMouseButtonState(mouse, mouseID, SDL_TRUE)) {
        xrel = 0.0f;
        yrel = 0.0f;
    }

    if (mouse->has_position) {
        /* Update internal mouse coordinates */
        if (!mouse->relative_mode) {
            mouse->x = x;
            mouse->y = y;
        } else {
            mouse->x += xrel;
            mouse->y += yrel;
            ConstrainMousePosition(mouse, window, &mouse->x, &mouse->y);
        }
    } else {
        mouse->x = x;
        mouse->y = y;
        mouse->has_position = SDL_TRUE;
    }

    mouse->xdelta += xrel;
    mouse->ydelta += yrel;

    /* Move the mouse cursor, if needed */
    if (mouse->cursor_shown && !mouse->relative_mode &&
        mouse->MoveCursor && mouse->cur_cursor) {
        mouse->MoveCursor(mouse->cur_cursor);
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_MOUSE_MOTION)) {
        SDL_Event event;
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.common.timestamp = timestamp;
        event.motion.windowID = mouse->focus ? mouse->focus->id : 0;
        event.motion.which = mouseID;
        /* Set us pending (or clear during a normal mouse movement event) as having triggered */
        mouse->was_touch_mouse_events = (mouseID == SDL_TOUCH_MOUSEID);
        event.motion.state = SDL_GetMouseButtonState(mouse, mouseID, SDL_TRUE);
        event.motion.x = mouse->x;
        event.motion.y = mouse->y;
        event.motion.xrel = xrel;
        event.motion.yrel = yrel;
        posted = (SDL_PushEvent(&event) > 0);
    }
    if (relative) {
        mouse->last_x = mouse->x;
        mouse->last_y = mouse->y;
    } else {
        /* Use unclamped values if we're getting events outside the window */
        mouse->last_x = x;
        mouse->last_y = y;
    }
    return posted;
}

static SDL_MouseInputSource *GetMouseInputSource(SDL_Mouse *mouse, SDL_MouseID mouseID, Uint8 state, Uint8 button)
{
    SDL_MouseInputSource *source, *match = NULL, *sources;
    int i;

    for (i = 0; i < mouse->num_sources; ++i) {
        source = &mouse->sources[i];
        if (source->mouseID == mouseID) {
            match = source;
            break;
        }
    }

    if (!state && (!match || !(match->buttonstate & SDL_BUTTON(button)))) {
        /* This might be a button release from a transition between mouse messages and raw input.
         * See if there's another mouse source that already has that button down and use that.
         */
        for (i = 0; i < mouse->num_sources; ++i) {
            source = &mouse->sources[i];
            if ((source->buttonstate & SDL_BUTTON(button))) {
                match = source;
                break;
            }
        }
    }
    if (match) {
        return match;
    }

    sources = (SDL_MouseInputSource *)SDL_realloc(mouse->sources, (mouse->num_sources + 1) * sizeof(*mouse->sources));
    if (sources) {
        mouse->sources = sources;
        ++mouse->num_sources;
        source = &sources[mouse->num_sources - 1];
        source->mouseID = mouseID;
        source->buttonstate = 0;
        return source;
    }
    return NULL;
}

static SDL_MouseClickState *GetMouseClickState(SDL_Mouse *mouse, Uint8 button)
{
    if (button >= mouse->num_clickstates) {
        int i, count = button + 1;
        SDL_MouseClickState *clickstate = (SDL_MouseClickState *)SDL_realloc(mouse->clickstate, count * sizeof(*mouse->clickstate));
        if (!clickstate) {
            return NULL;
        }
        mouse->clickstate = clickstate;

        for (i = mouse->num_clickstates; i < count; ++i) {
            SDL_zero(mouse->clickstate[i]);
        }
        mouse->num_clickstates = count;
    }
    return &mouse->clickstate[button];
}

static int SDL_PrivateSendMouseButton(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button, int clicks)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    int posted;
    Uint32 type;
    Uint32 buttonstate;
    SDL_MouseInputSource *source;

    if (!mouse->relative_mode && mouseID != SDL_TOUCH_MOUSEID && mouseID != SDL_PEN_MOUSEID) {
        /* We're not in relative mode, so all mouse events are global mouse events */
        mouseID = SDL_GLOBAL_MOUSE_ID;
    }

    source = GetMouseInputSource(mouse, mouseID, state, button);
    if (!source) {
        return 0;
    }
    buttonstate = source->buttonstate;

    /* SDL_HINT_MOUSE_TOUCH_EVENTS: controlling whether mouse events should generate synthetic touch events */
    if (mouse->mouse_touch_events) {
        if (mouseID != SDL_TOUCH_MOUSEID && button == SDL_BUTTON_LEFT) {
            if (state == SDL_PRESSED) {
                track_mouse_down = SDL_TRUE;
            } else {
                track_mouse_down = SDL_FALSE;
            }
            if (window) {
                float normalized_x = mouse->x / (float)window->w;
                float normalized_y = mouse->y / (float)window->h;
                SDL_SendTouch(timestamp, SDL_MOUSE_TOUCHID, SDL_BUTTON_LEFT, window, track_mouse_down, normalized_x, normalized_y, 1.0f);
            }
        }
    }

    /* SDL_HINT_TOUCH_MOUSE_EVENTS: if not set, discard synthetic mouse events coming from platform layer */
    if (mouse->touch_mouse_events == 0) {
        if (mouseID == SDL_TOUCH_MOUSEID) {
            return 0;
        }
    }

    /* Figure out which event to perform */
    switch (state) {
    case SDL_PRESSED:
        type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        buttonstate |= SDL_BUTTON(button);
        break;
    case SDL_RELEASED:
        type = SDL_EVENT_MOUSE_BUTTON_UP;
        buttonstate &= ~SDL_BUTTON(button);
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }

    /* We do this after calculating buttonstate so button presses gain focus */
    if (window && state == SDL_PRESSED) {
        SDL_UpdateMouseFocus(window, mouse->x, mouse->y, buttonstate, SDL_TRUE);
    }

    if (buttonstate == source->buttonstate) {
        /* Ignore this event, no state change */
        return 0;
    }
    source->buttonstate = buttonstate;

    if (clicks < 0) {
        SDL_MouseClickState *clickstate = GetMouseClickState(mouse, button);
        if (clickstate) {
            if (state == SDL_PRESSED) {
                Uint64 now = SDL_GetTicks();

                if (now >= (clickstate->last_timestamp + mouse->double_click_time) ||
                    SDL_fabs((double)mouse->x - clickstate->last_x) > mouse->double_click_radius ||
                    SDL_fabs((double)mouse->y - clickstate->last_y) > mouse->double_click_radius) {
                    clickstate->click_count = 0;
                }
                clickstate->last_timestamp = now;
                clickstate->last_x = mouse->x;
                clickstate->last_y = mouse->y;
                if (clickstate->click_count < 255) {
                    ++clickstate->click_count;
                }
            }
            clicks = clickstate->click_count;
        } else {
            clicks = 1;
        }
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(type)) {
        SDL_Event event;
        event.type = type;
        event.common.timestamp = timestamp;
        event.button.windowID = mouse->focus ? mouse->focus->id : 0;
        event.button.which = source->mouseID;
        event.button.state = state;
        event.button.button = button;
        event.button.clicks = (Uint8)SDL_min(clicks, 255);
        event.button.x = mouse->x;
        event.button.y = mouse->y;
        posted = (SDL_PushEvent(&event) > 0);
    }

    /* We do this after dispatching event so button releases can lose focus */
    if (window && state == SDL_RELEASED) {
        SDL_UpdateMouseFocus(window, mouse->x, mouse->y, buttonstate, SDL_TRUE);
    }

    /* Automatically capture the mouse while buttons are pressed */
    if (mouse->auto_capture) {
        SDL_UpdateMouseCapture(SDL_FALSE);
    }

    return posted;
}

int SDL_SendMouseButtonClicks(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button, int clicks)
{
    clicks = SDL_max(clicks, 0);
    return SDL_PrivateSendMouseButton(timestamp, window, mouseID, state, button, clicks);
}

int SDL_SendMouseButton(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, Uint8 state, Uint8 button)
{
    return SDL_PrivateSendMouseButton(timestamp, window, mouseID, state, button, -1);
}

int SDL_SendMouseWheel(Uint64 timestamp, SDL_Window *window, SDL_MouseID mouseID, float x, float y, SDL_MouseWheelDirection direction)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    int posted;

    if (window) {
        SDL_SetMouseFocus(window);
    }

    if (x == 0.0f && y == 0.0f) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_MOUSE_WHEEL)) {
        SDL_Event event;
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.common.timestamp = timestamp;
        event.wheel.windowID = mouse->focus ? mouse->focus->id : 0;
        event.wheel.which = mouseID;
        event.wheel.x = x;
        event.wheel.y = y;
        event.wheel.direction = direction;
        event.wheel.mouse_x = mouse->x;
        event.wheel.mouse_y = mouse->y;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

void SDL_QuitMouse(void)
{
    SDL_Cursor *cursor, *next;
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->CaptureMouse) {
        SDL_CaptureMouse(SDL_FALSE);
        SDL_UpdateMouseCapture(SDL_TRUE);
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_ShowCursor();
    SDL_PenQuit();

    if (mouse->def_cursor) {
        SDL_SetDefaultCursor(NULL);
    }

    cursor = mouse->cursors;
    while (cursor) {
        next = cursor->next;
        SDL_DestroyCursor(cursor);
        cursor = next;
    }
    mouse->cursors = NULL;
    mouse->cur_cursor = NULL;

    if (mouse->sources) {
        SDL_free(mouse->sources);
        mouse->sources = NULL;
    }
    mouse->num_sources = 0;

    if (mouse->clickstate) {
        SDL_free(mouse->clickstate);
        mouse->clickstate = NULL;
    }
    mouse->num_clickstates = 0;

    if (mouse->system_scale_values) {
        SDL_free(mouse->system_scale_values);
        mouse->system_scale_values = NULL;
    }
    mouse->num_system_scale_values = 0;

    SDL_DelHintCallback(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME,
                        SDL_MouseDoubleClickTimeChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS,
                        SDL_MouseDoubleClickRadiusChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_NORMAL_SPEED_SCALE,
                        SDL_MouseNormalSpeedScaleChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE,
                        SDL_MouseRelativeSpeedScaleChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,
                        SDL_MouseRelativeSystemScaleChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_TOUCH_MOUSE_EVENTS,
                        SDL_TouchMouseEventsChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_TOUCH_EVENTS,
                        SDL_MouseTouchEventsChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_AUTO_CAPTURE,
                        SDL_MouseAutoCaptureChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_WARP_MOTION,
                        SDL_MouseRelativeWarpMotionChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE,
                        SDL_MouseRelativeCursorVisibleChanged, mouse);

    SDL_DelHintCallback(SDL_HINT_MOUSE_RELATIVE_CLIP_INTERVAL,
                        SDL_MouseRelativeClipIntervalChanged, mouse);

    for (int i = SDL_mouse_count; i--; ) {
        SDL_RemoveMouse(SDL_mice[i].instance_id, SDL_FALSE);
    }
    SDL_free(SDL_mice);
    SDL_mice = NULL;
}

SDL_MouseButtonFlags SDL_GetMouseState(float *x, float *y)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (x) {
        *x = mouse->x;
    }
    if (y) {
        *y = mouse->y;
    }
    return SDL_GetMouseButtonState(mouse, SDL_GLOBAL_MOUSE_ID, SDL_TRUE);
}

SDL_MouseButtonFlags SDL_GetRelativeMouseState(float *x, float *y)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (x) {
        *x = mouse->xdelta;
    }
    if (y) {
        *y = mouse->ydelta;
    }
    mouse->xdelta = 0.0f;
    mouse->ydelta = 0.0f;
    return SDL_GetMouseButtonState(mouse, SDL_GLOBAL_MOUSE_ID, SDL_TRUE);
}

SDL_MouseButtonFlags SDL_GetGlobalMouseState(float *x, float *y)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->GetGlobalMouseState) {
        float tmpx, tmpy;

        /* make sure these are never NULL for the backend implementations... */
        if (!x) {
            x = &tmpx;
        }
        if (!y) {
            y = &tmpy;
        }

        *x = *y = 0.0f;

        return mouse->GetGlobalMouseState(x, y);
    } else {
        return SDL_GetMouseState(x, y);
    }
}

void SDL_PerformWarpMouseInWindow(SDL_Window *window, float x, float y, SDL_bool ignore_relative_mode)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!window) {
        window = mouse->focus;
    }

    if (!window) {
        return;
    }

    if ((window->flags & SDL_WINDOW_MINIMIZED) == SDL_WINDOW_MINIMIZED) {
        return;
    }

    /* Ignore the previous position when we warp */
    mouse->last_x = x;
    mouse->last_y = y;
    mouse->has_position = SDL_FALSE;

    if (mouse->relative_mode && !ignore_relative_mode) {
        /* 2.0.22 made warping in relative mode actually functional, which
         * surprised many applications that weren't expecting the additional
         * mouse motion.
         *
         * So for now, warping in relative mode adjusts the absolution position
         * but doesn't generate motion events, unless SDL_HINT_MOUSE_RELATIVE_WARP_MOTION is set.
         */
        if (!mouse->relative_mode_warp_motion) {
            mouse->x = x;
            mouse->y = y;
            mouse->has_position = SDL_TRUE;
            return;
        }
    }

    if (mouse->WarpMouse &&
        (!mouse->relative_mode || mouse->relative_mode_warp)) {
        mouse->WarpMouse(window, x, y);
    } else {
        SDL_PrivateSendMouseMotion(0, window, SDL_GLOBAL_MOUSE_ID, SDL_FALSE, x, y);
    }
}

void SDL_WarpMouseInWindow(SDL_Window *window, float x, float y)
{
    SDL_PerformWarpMouseInWindow(window, x, y, SDL_FALSE);
}

int SDL_WarpMouseGlobal(float x, float y)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->WarpMouseGlobal) {
        return mouse->WarpMouseGlobal(x, y);
    }

    return SDL_Unsupported();
}

static SDL_bool SDL_ShouldUseRelativeModeWarp(SDL_Mouse *mouse)
{
    if (!mouse->WarpMouse) {
        /* Need this functionality for relative mode warp implementation */
        return SDL_FALSE;
    }

    return SDL_GetHintBoolean(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, SDL_FALSE);
}

int SDL_SetRelativeMouseMode(SDL_bool enabled)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_Window *focusWindow = SDL_GetKeyboardFocus();

    if (enabled == mouse->relative_mode) {
        return 0;
    }

    /* Set the relative mode */
    if (!enabled && mouse->relative_mode_warp) {
        mouse->relative_mode_warp = SDL_FALSE;
    } else if (enabled && SDL_ShouldUseRelativeModeWarp(mouse)) {
        mouse->relative_mode_warp = SDL_TRUE;
    } else if (!mouse->SetRelativeMouseMode || mouse->SetRelativeMouseMode(enabled) < 0) {
        if (enabled) {
            /* Fall back to warp mode if native relative mode failed */
            if (!mouse->WarpMouse) {
                return SDL_SetError("No relative mode implementation available");
            }
            mouse->relative_mode_warp = SDL_TRUE;
        }
    }
    mouse->relative_mode = enabled;

    if (enabled) {
        /* Update cursor visibility before we potentially warp the mouse */
        SDL_SetCursor(NULL);
    }

    if (enabled && focusWindow) {
        SDL_SetMouseFocus(focusWindow);

        if (mouse->relative_mode_warp) {
            float center_x = (float)focusWindow->w / 2.0f;
            float center_y = (float)focusWindow->h / 2.0f;
            SDL_PerformWarpMouseInWindow(focusWindow, center_x, center_y, SDL_TRUE);
        }
    }

    if (focusWindow) {
        SDL_UpdateWindowGrab(focusWindow);

        /* Put the cursor back to where the application expects it */
        if (!enabled) {
            SDL_PerformWarpMouseInWindow(focusWindow, mouse->x, mouse->y, SDL_TRUE);
        }

        SDL_UpdateMouseCapture(SDL_FALSE);
    }

    if (!enabled) {
        /* Update cursor visibility after we restore the mouse position */
        SDL_SetCursor(NULL);
    }

    /* Flush pending mouse motion - ideally we would pump events, but that's not always safe */
    SDL_FlushEvent(SDL_EVENT_MOUSE_MOTION);

    return 0;
}

SDL_bool SDL_GetRelativeMouseMode(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    return mouse->relative_mode;
}

int SDL_UpdateMouseCapture(SDL_bool force_release)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_Window *capture_window = NULL;

    if (!mouse->CaptureMouse) {
        return 0;
    }

    if (!force_release) {
        if (SDL_GetMessageBoxCount() == 0 &&
            (mouse->capture_desired || (mouse->auto_capture && SDL_GetMouseButtonState(mouse, SDL_GLOBAL_MOUSE_ID, SDL_FALSE) != 0))) {
            if (!mouse->relative_mode) {
                capture_window = mouse->focus;
            }
        }
    }

    if (capture_window != mouse->capture_window) {
        /* We can get here recursively on Windows, so make sure we complete
         * all of the window state operations before we change the capture state
         * (e.g. https://github.com/libsdl-org/SDL/pull/5608)
         */
        SDL_Window *previous_capture = mouse->capture_window;

        if (previous_capture) {
            previous_capture->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;
        }

        if (capture_window) {
            capture_window->flags |= SDL_WINDOW_MOUSE_CAPTURE;
        }

        mouse->capture_window = capture_window;

        if (mouse->CaptureMouse(capture_window) < 0) {
            /* CaptureMouse() will have set an error, just restore the state */
            if (previous_capture) {
                previous_capture->flags |= SDL_WINDOW_MOUSE_CAPTURE;
            }
            if (capture_window) {
                capture_window->flags &= ~SDL_WINDOW_MOUSE_CAPTURE;
            }
            mouse->capture_window = previous_capture;

            return -1;
        }
    }
    return 0;
}

int SDL_CaptureMouse(SDL_bool enabled)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!mouse->CaptureMouse) {
        return SDL_Unsupported();
    }

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    /* Windows mouse capture is tied to the current thread, and must be called
     * from the thread that created the window being captured. Since we update
     * the mouse capture state from the event processing, any application state
     * changes must be processed on that thread as well.
     */
    if (!SDL_OnVideoThread()) {
        return SDL_SetError("SDL_CaptureMouse() must be called on the main thread");
    }
#endif /* defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK) */

    if (enabled && SDL_GetKeyboardFocus() == NULL) {
        return SDL_SetError("No window has focus");
    }
    mouse->capture_desired = enabled;

    return SDL_UpdateMouseCapture(SDL_FALSE);
}

SDL_Cursor *SDL_CreateCursor(const Uint8 *data, const Uint8 *mask, int w, int h, int hot_x, int hot_y)
{
    SDL_Surface *surface;
    SDL_Cursor *cursor;
    int x, y;
    Uint32 *pixel;
    Uint8 datab = 0, maskb = 0;
    const Uint32 black = 0xFF000000;
    const Uint32 white = 0xFFFFFFFF;
    const Uint32 transparent = 0x00000000;
#if defined(SDL_PLATFORM_WIN32)
    /* Only Windows backend supports inverted pixels in mono cursors. */
    const Uint32 inverted = 0x00FFFFFF;
#else
    const Uint32 inverted = 0xFF000000;
#endif /* defined(SDL_PLATFORM_WIN32) */

    /* Make sure the width is a multiple of 8 */
    w = ((w + 7) & ~7);

    /* Create the surface from a bitmap */
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return NULL;
    }
    for (y = 0; y < h; ++y) {
        pixel = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
        for (x = 0; x < w; ++x) {
            if ((x % 8) == 0) {
                datab = *data++;
                maskb = *mask++;
            }
            if (maskb & 0x80) {
                *pixel++ = (datab & 0x80) ? black : white;
            } else {
                *pixel++ = (datab & 0x80) ? inverted : transparent;
            }
            datab <<= 1;
            maskb <<= 1;
        }
    }

    cursor = SDL_CreateColorCursor(surface, hot_x, hot_y);

    SDL_DestroySurface(surface);

    return cursor;
}

SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *surface, int hot_x, int hot_y)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_Surface *temp = NULL;
    SDL_Cursor *cursor;

    if (!surface) {
        SDL_InvalidParamError("surface");
        return NULL;
    }

    /* Sanity check the hot spot */
    if ((hot_x < 0) || (hot_y < 0) ||
        (hot_x >= surface->w) || (hot_y >= surface->h)) {
        SDL_SetError("Cursor hot spot doesn't lie within cursor");
        return NULL;
    }

    if (surface->format->format != SDL_PIXELFORMAT_ARGB8888) {
        temp = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888);
        if (!temp) {
            return NULL;
        }
        surface = temp;
    }

    if (mouse->CreateCursor) {
        cursor = mouse->CreateCursor(surface, hot_x, hot_y);
    } else {
        cursor = (SDL_Cursor *)SDL_calloc(1, sizeof(*cursor));
    }
    if (cursor) {
        cursor->next = mouse->cursors;
        mouse->cursors = cursor;
    }

    SDL_DestroySurface(temp);

    return cursor;
}

SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_Cursor *cursor;

    if (!mouse->CreateSystemCursor) {
        SDL_SetError("CreateSystemCursor is not currently supported");
        return NULL;
    }

    cursor = mouse->CreateSystemCursor(id);
    if (cursor) {
        cursor->next = mouse->cursors;
        mouse->cursors = cursor;
    }

    return cursor;
}

/* SDL_SetCursor(NULL) can be used to force the cursor redraw,
   if this is desired for any reason.  This is used when setting
   the video mode and when the SDL window gains the mouse focus.
 */
int SDL_SetCursor(SDL_Cursor *cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    /* Return immediately if setting the cursor to the currently set one (fixes #7151) */
    if (cursor == mouse->cur_cursor) {
        return 0;
    }

    /* Set the new cursor */
    if (cursor) {
        /* Make sure the cursor is still valid for this mouse */
        if (cursor != mouse->def_cursor) {
            SDL_Cursor *found;
            for (found = mouse->cursors; found; found = found->next) {
                if (found == cursor) {
                    break;
                }
            }
            if (!found) {
                return SDL_SetError("Cursor not associated with the current mouse");
            }
        }
        mouse->cur_cursor = cursor;
    } else {
        if (mouse->focus) {
            cursor = mouse->cur_cursor;
        } else {
            cursor = mouse->def_cursor;
        }
    }

    if (cursor && (!mouse->focus || (mouse->cursor_shown && (!mouse->relative_mode || mouse->relative_mode_cursor_visible)))) {
        if (mouse->ShowCursor) {
            mouse->ShowCursor(cursor);
        }
    } else {
        if (mouse->ShowCursor) {
            mouse->ShowCursor(NULL);
        }
    }
    return 0;
}

SDL_Cursor *SDL_GetCursor(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!mouse) {
        return NULL;
    }
    return mouse->cur_cursor;
}

SDL_Cursor *SDL_GetDefaultCursor(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!mouse) {
        return NULL;
    }
    return mouse->def_cursor;
}

void SDL_DestroyCursor(SDL_Cursor *cursor)
{
    SDL_Mouse *mouse = SDL_GetMouse();
    SDL_Cursor *curr, *prev;

    if (!cursor) {
        return;
    }

    if (cursor == mouse->def_cursor) {
        return;
    }
    if (cursor == mouse->cur_cursor) {
        SDL_SetCursor(mouse->def_cursor);
    }

    for (prev = NULL, curr = mouse->cursors; curr;
         prev = curr, curr = curr->next) {
        if (curr == cursor) {
            if (prev) {
                prev->next = curr->next;
            } else {
                mouse->cursors = curr->next;
            }

            if (mouse->FreeCursor && curr->driverdata) {
                mouse->FreeCursor(curr);
            } else {
                SDL_free(curr);
            }
            return;
        }
    }
}

int SDL_ShowCursor(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (!mouse->cursor_shown) {
        mouse->cursor_shown = SDL_TRUE;
        SDL_SetCursor(NULL);
    }
    return 0;
}

int SDL_HideCursor(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    if (mouse->cursor_shown) {
        mouse->cursor_shown = SDL_FALSE;
        SDL_SetCursor(NULL);
    }
    return 0;
}

SDL_bool SDL_CursorVisible(void)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    return mouse->cursor_shown;
}
