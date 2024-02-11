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

/* General event handling code for SDL */

#include "SDL_events_c.h"
#include "../SDL_hints_c.h"
#include "../audio/SDL_audio_c.h"
#include "../timer/SDL_timer_c.h"
#ifndef SDL_JOYSTICK_DISABLED
#include "../joystick/SDL_joystick_c.h"
#endif
#ifndef SDL_SENSOR_DISABLED
#include "../sensor/SDL_sensor_c.h"
#endif
#include "../video/SDL_sysvideo.h"

#undef SDL_PRIs64
#if (defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK)) && !defined(SDL_PLATFORM_CYGWIN)
#define SDL_PRIs64 "I64d"
#else
#define SDL_PRIs64 "lld"
#endif

/* An arbitrary limit so we don't have unbounded growth */
#define SDL_MAX_QUEUED_EVENTS 65535

/* Determines how often we wake to call SDL_PumpEvents() in SDL_WaitEventTimeout_Device() */
#define PERIODIC_POLL_INTERVAL_NS (3 * SDL_NS_PER_SECOND)

typedef struct SDL_EventWatcher
{
    SDL_EventFilter callback;
    void *userdata;
    SDL_bool removed;
} SDL_EventWatcher;

static SDL_Mutex *SDL_event_watchers_lock;
static SDL_EventWatcher SDL_EventOK;
static SDL_EventWatcher *SDL_event_watchers = NULL;
static int SDL_event_watchers_count = 0;
static SDL_bool SDL_event_watchers_dispatching = SDL_FALSE;
static SDL_bool SDL_event_watchers_removed = SDL_FALSE;
static SDL_AtomicInt SDL_sentinel_pending;
static Uint32 SDL_last_event_id = 0;

typedef struct
{
    Uint32 bits[8];
} SDL_DisabledEventBlock;

static SDL_DisabledEventBlock *SDL_disabled_events[256];
static Uint32 SDL_userevents = SDL_EVENT_USER;

/* Private data -- event queue */
typedef struct SDL_EventEntry
{
    SDL_Event event;
    struct SDL_EventEntry *prev;
    struct SDL_EventEntry *next;
} SDL_EventEntry;

static struct
{
    SDL_Mutex *lock;
    SDL_bool active;
    SDL_AtomicInt count;
    int max_events_seen;
    SDL_EventEntry *head;
    SDL_EventEntry *tail;
    SDL_EventEntry *free;
} SDL_EventQ = { NULL, SDL_FALSE, { 0 }, 0, NULL, NULL, NULL };

typedef struct SDL_EventMemory
{
    Uint32 eventID;
    void *memory;
    struct SDL_EventMemory *next;
} SDL_EventMemory;

static SDL_Mutex *SDL_event_memory_lock;
static SDL_EventMemory *SDL_event_memory_head;
static SDL_EventMemory *SDL_event_memory_tail;

void *SDL_AllocateEventMemory(size_t size)
{
    void *memory = SDL_malloc(size);
    if (!memory) {
        return NULL;
    }

    SDL_LockMutex(SDL_event_memory_lock);
    {
        SDL_EventMemory *entry = (SDL_EventMemory *)SDL_malloc(sizeof(*entry));
        if (entry) {
            entry->eventID = SDL_last_event_id;
            entry->memory = memory;
            entry->next = NULL;

            if (SDL_event_memory_tail) {
                SDL_event_memory_tail->next = entry;
            } else {
                SDL_event_memory_head = entry;
            }
            SDL_event_memory_tail = entry;
        } else {
            SDL_free(memory);
            memory = NULL;
        }
    }
    SDL_UnlockMutex(SDL_event_memory_lock);

    return memory;
}

static void SDL_FlushEventMemory(Uint32 eventID)
{
    SDL_LockMutex(SDL_event_memory_lock);
    {
        if (SDL_event_memory_head) {
            while (SDL_event_memory_head) {
                SDL_EventMemory *entry = SDL_event_memory_head;

                if (eventID && (Sint32)(eventID - entry->eventID) < 0) {
                    break;
                }

                /* If you crash here, your application has memory corruption
                 * or freed memory in an event, which is no longer necessary.
                 */
                SDL_event_memory_head = entry->next;
                SDL_free(entry->memory);
                SDL_free(entry);
            }
            if (!SDL_event_memory_head) {
                SDL_event_memory_tail = NULL;
            }
        }
    }
    SDL_UnlockMutex(SDL_event_memory_lock);
}

#ifndef SDL_JOYSTICK_DISABLED

static SDL_bool SDL_update_joysticks = SDL_TRUE;

static void SDLCALL SDL_AutoUpdateJoysticksChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_update_joysticks = SDL_GetStringBoolean(hint, SDL_TRUE);
}

#endif /* !SDL_JOYSTICK_DISABLED */

#ifndef SDL_SENSOR_DISABLED

static SDL_bool SDL_update_sensors = SDL_TRUE;

static void SDLCALL SDL_AutoUpdateSensorsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_update_sensors = SDL_GetStringBoolean(hint, SDL_TRUE);
}

#endif /* !SDL_SENSOR_DISABLED */

static void SDLCALL SDL_PollSentinelChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_SetEventEnabled(SDL_EVENT_POLL_SENTINEL, SDL_GetStringBoolean(hint, SDL_TRUE));
}

/**
 * Verbosity of logged events as defined in SDL_HINT_EVENT_LOGGING:
 *  - 0: (default) no logging
 *  - 1: logging of most events
 *  - 2: as above, plus mouse, pen, and finger motion
 */
static int SDL_EventLoggingVerbosity = 0;

static void SDLCALL SDL_EventLoggingChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_EventLoggingVerbosity = (hint && *hint) ? SDL_clamp(SDL_atoi(hint), 0, 3) : 0;
}

static void SDL_LogEvent(const SDL_Event *event)
{
    char name[64];
    char details[128];

    /* sensor/mouse/pen/finger motion are spammy, ignore these if they aren't demanded. */
    if ((SDL_EventLoggingVerbosity < 2) &&
        ((event->type == SDL_EVENT_MOUSE_MOTION) ||
         (event->type == SDL_EVENT_FINGER_MOTION) ||
         (event->type == SDL_EVENT_PEN_MOTION) ||
         (event->type == SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION) ||
         (event->type == SDL_EVENT_GAMEPAD_SENSOR_UPDATE) ||
         (event->type == SDL_EVENT_SENSOR_UPDATE))) {
        return;
    }

/* this is to make (void)SDL_snprintf() calls cleaner. */
#define uint unsigned int

    name[0] = '\0';
    details[0] = '\0';

    /* !!! FIXME: This code is kinda ugly, sorry. */

    if ((event->type >= SDL_EVENT_USER) && (event->type <= SDL_EVENT_LAST)) {
        char plusstr[16];
        SDL_strlcpy(name, "SDL_EVENT_USER", sizeof(name));
        if (event->type > SDL_EVENT_USER) {
            (void)SDL_snprintf(plusstr, sizeof(plusstr), "+%u", ((uint)event->type) - SDL_EVENT_USER);
        } else {
            plusstr[0] = '\0';
        }
        (void)SDL_snprintf(details, sizeof(details), "%s (timestamp=%u windowid=%u code=%d data1=%p data2=%p)",
                           plusstr, (uint)event->user.timestamp, (uint)event->user.windowID,
                           (int)event->user.code, event->user.data1, event->user.data2);
    }

    switch (event->type) {
#define SDL_EVENT_CASE(x) \
    case x:               \
        SDL_strlcpy(name, #x, sizeof(name));
        SDL_EVENT_CASE(SDL_EVENT_FIRST)
        SDL_strlcpy(details, " (THIS IS PROBABLY A BUG!)", sizeof(details));
        break;
        SDL_EVENT_CASE(SDL_EVENT_QUIT)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u)", (uint)event->quit.timestamp);
        break;
        SDL_EVENT_CASE(SDL_EVENT_TERMINATING)
        break;
        SDL_EVENT_CASE(SDL_EVENT_LOW_MEMORY)
        break;
        SDL_EVENT_CASE(SDL_EVENT_WILL_ENTER_BACKGROUND)
        break;
        SDL_EVENT_CASE(SDL_EVENT_DID_ENTER_BACKGROUND)
        break;
        SDL_EVENT_CASE(SDL_EVENT_WILL_ENTER_FOREGROUND)
        break;
        SDL_EVENT_CASE(SDL_EVENT_DID_ENTER_FOREGROUND)
        break;
        SDL_EVENT_CASE(SDL_EVENT_LOCALE_CHANGED)
        break;
        SDL_EVENT_CASE(SDL_EVENT_SYSTEM_THEME_CHANGED)
        break;
        SDL_EVENT_CASE(SDL_EVENT_KEYMAP_CHANGED)
        break;
        SDL_EVENT_CASE(SDL_EVENT_CLIPBOARD_UPDATE)
        break;
        SDL_EVENT_CASE(SDL_EVENT_RENDER_TARGETS_RESET)
        break;
        SDL_EVENT_CASE(SDL_EVENT_RENDER_DEVICE_RESET)
        break;

#define SDL_DISPLAYEVENT_CASE(x)               \
    case x:                                    \
        SDL_strlcpy(name, #x, sizeof(name));   \
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u display=%u event=%s data1=%d)", \
                           (uint)event->display.timestamp, (uint)event->display.displayID, name, (int)event->display.data1); \
        break
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_ORIENTATION);
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_ADDED);
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_REMOVED);
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_MOVED);
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED);
        SDL_DISPLAYEVENT_CASE(SDL_EVENT_DISPLAY_HDR_STATE_CHANGED);
#undef SDL_DISPLAYEVENT_CASE

#define SDL_WINDOWEVENT_CASE(x)                \
    case x:                                    \
        SDL_strlcpy(name, #x, sizeof(name)); \
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u event=%s data1=%d data2=%d)", \
                           (uint)event->window.timestamp, (uint)event->window.windowID, name, (int)event->window.data1, (int)event->window.data2); \
        break
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_SHOWN);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_HIDDEN);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_EXPOSED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_MOVED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_RESIZED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_MINIMIZED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_MAXIMIZED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_RESTORED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_MOUSE_ENTER);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_MOUSE_LEAVE);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_PEN_ENTER);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_PEN_LEAVE);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_FOCUS_GAINED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_FOCUS_LOST);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_TAKE_FOCUS);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_HIT_TEST);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_ICCPROF_CHANGED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_OCCLUDED);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
        SDL_WINDOWEVENT_CASE(SDL_EVENT_WINDOW_DESTROYED);
#undef SDL_WINDOWEVENT_CASE

#define PRINT_KEY_EVENT(event)                                                                                                   \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u state=%s repeat=%s scancode=%u keycode=%u mod=%u)", \
                       (uint)event->key.timestamp, (uint)event->key.windowID,                                                    \
                       event->key.state == SDL_PRESSED ? "pressed" : "released",                                                 \
                       event->key.repeat ? "true" : "false",                                                                     \
                       (uint)event->key.keysym.scancode,                                                                         \
                       (uint)event->key.keysym.sym,                                                                              \
                       (uint)event->key.keysym.mod)
        SDL_EVENT_CASE(SDL_EVENT_KEY_DOWN)
        PRINT_KEY_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_KEY_UP)
        PRINT_KEY_EVENT(event);
        break;
#undef PRINT_KEY_EVENT

        SDL_EVENT_CASE(SDL_EVENT_TEXT_EDITING)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u text='%s' start=%d length=%d)",
                           (uint)event->edit.timestamp, (uint)event->edit.windowID,
                           event->edit.text, (int)event->edit.start, (int)event->edit.length);
        break;

        SDL_EVENT_CASE(SDL_EVENT_TEXT_INPUT)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u text='%s')", (uint)event->text.timestamp, (uint)event->text.windowID, event->text.text);
        break;

        SDL_EVENT_CASE(SDL_EVENT_MOUSE_MOTION)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u state=%u x=%g y=%g xrel=%g yrel=%g)",
                           (uint)event->motion.timestamp, (uint)event->motion.windowID,
                           (uint)event->motion.which, (uint)event->motion.state,
                           event->motion.x, event->motion.y,
                           event->motion.xrel, event->motion.yrel);
        break;

#define PRINT_MBUTTON_EVENT(event)                                                                                              \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u button=%u state=%s clicks=%u x=%g y=%g)", \
                       (uint)event->button.timestamp, (uint)event->button.windowID,                                             \
                       (uint)event->button.which, (uint)event->button.button,                                                   \
                       event->button.state == SDL_PRESSED ? "pressed" : "released",                                             \
                       (uint)event->button.clicks, event->button.x, event->button.y)
        SDL_EVENT_CASE(SDL_EVENT_MOUSE_BUTTON_DOWN)
        PRINT_MBUTTON_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_MOUSE_BUTTON_UP)
        PRINT_MBUTTON_EVENT(event);
        break;
#undef PRINT_MBUTTON_EVENT

        SDL_EVENT_CASE(SDL_EVENT_MOUSE_WHEEL)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u x=%g y=%g direction=%s)",
                           (uint)event->wheel.timestamp, (uint)event->wheel.windowID,
                           (uint)event->wheel.which, event->wheel.x, event->wheel.y,
                           event->wheel.direction == SDL_MOUSEWHEEL_NORMAL ? "normal" : "flipped");
        break;

        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_AXIS_MOTION)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d axis=%u value=%d)",
                           (uint)event->jaxis.timestamp, (int)event->jaxis.which,
                           (uint)event->jaxis.axis, (int)event->jaxis.value);
        break;

        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_HAT_MOTION)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d hat=%u value=%u)",
                           (uint)event->jhat.timestamp, (int)event->jhat.which,
                           (uint)event->jhat.hat, (uint)event->jhat.value);
        break;

#define PRINT_JBUTTON_EVENT(event)                                                              \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d button=%u state=%s)", \
                       (uint)event->jbutton.timestamp, (int)event->jbutton.which,               \
                       (uint)event->jbutton.button, event->jbutton.state == SDL_PRESSED ? "pressed" : "released")
        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_BUTTON_DOWN)
        PRINT_JBUTTON_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_BUTTON_UP)
        PRINT_JBUTTON_EVENT(event);
        break;
#undef PRINT_JBUTTON_EVENT

#define PRINT_JOYDEV_EVENT(event) (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d)", (uint)event->jdevice.timestamp, (int)event->jdevice.which)
        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_ADDED)
        PRINT_JOYDEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_JOYSTICK_REMOVED)
        PRINT_JOYDEV_EVENT(event);
        break;
#undef PRINT_JOYDEV_EVENT

        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_AXIS_MOTION)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d axis=%u value=%d)",
                           (uint)event->gaxis.timestamp, (int)event->gaxis.which,
                           (uint)event->gaxis.axis, (int)event->gaxis.value);
        break;

#define PRINT_CBUTTON_EVENT(event)                                                              \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d button=%u state=%s)", \
                       (uint)event->gbutton.timestamp, (int)event->gbutton.which,               \
                       (uint)event->gbutton.button, event->gbutton.state == SDL_PRESSED ? "pressed" : "released")
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        PRINT_CBUTTON_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_BUTTON_UP)
        PRINT_CBUTTON_EVENT(event);
        break;
#undef PRINT_CBUTTON_EVENT

#define PRINT_GAMEPADDEV_EVENT(event) (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d)", (uint)event->gdevice.timestamp, (int)event->gdevice.which)
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_ADDED)
        PRINT_GAMEPADDEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_REMOVED)
        PRINT_GAMEPADDEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_REMAPPED)
        PRINT_GAMEPADDEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)
        PRINT_GAMEPADDEV_EVENT(event);
        break;
#undef PRINT_GAMEPADDEV_EVENT

#define PRINT_CTOUCHPAD_EVENT(event)                                                                                     \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d touchpad=%d finger=%d x=%f y=%f pressure=%f)", \
                       (uint)event->gtouchpad.timestamp, (int)event->gtouchpad.which,                                    \
                       (int)event->gtouchpad.touchpad, (int)event->gtouchpad.finger,                                     \
                       event->gtouchpad.x, event->gtouchpad.y, event->gtouchpad.pressure)
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
        PRINT_CTOUCHPAD_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_TOUCHPAD_UP)
        PRINT_CTOUCHPAD_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION)
        PRINT_CTOUCHPAD_EVENT(event);
        break;
#undef PRINT_CTOUCHPAD_EVENT

        SDL_EVENT_CASE(SDL_EVENT_GAMEPAD_SENSOR_UPDATE)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d sensor=%d data[0]=%f data[1]=%f data[2]=%f)",
                           (uint)event->gsensor.timestamp, (int)event->gsensor.which, (int)event->gsensor.sensor,
                           event->gsensor.data[0], event->gsensor.data[1], event->gsensor.data[2]);
        break;

#define PRINT_FINGER_EVENT(event)                                                                                                                      \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u touchid=%" SDL_PRIu64 " fingerid=%" SDL_PRIu64 " x=%f y=%f dx=%f dy=%f pressure=%f)", \
                       (uint)event->tfinger.timestamp, event->tfinger.touchID,                                                              \
                       event->tfinger.fingerID, event->tfinger.x, event->tfinger.y,                                                         \
                       event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure)
        SDL_EVENT_CASE(SDL_EVENT_FINGER_DOWN)
        PRINT_FINGER_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_FINGER_UP)
        PRINT_FINGER_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_FINGER_MOTION)
        PRINT_FINGER_EVENT(event);
        break;
#undef PRINT_FINGER_EVENT

#define PRINT_PTIP_EVENT(event)                                                                                    \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u tip=%u state=%s x=%g y=%g)", \
                       (uint)event->ptip.timestamp, (uint)event->ptip.windowID,                                    \
                       (uint)event->ptip.which, (uint)event->ptip.tip,                                             \
                       event->ptip.state == SDL_PRESSED ? "down" : "up",                                           \
                       event->ptip.x, event->ptip.y)
        SDL_EVENT_CASE(SDL_EVENT_PEN_DOWN)
        PRINT_PTIP_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_PEN_UP)
        PRINT_PTIP_EVENT(event);
        break;
#undef PRINT_PTIP_EVENT

        SDL_EVENT_CASE(SDL_EVENT_PEN_MOTION)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u state=%08x x=%g y=%g [%g, %g, %g, %g, %g, %g])",
                           (uint)event->pmotion.timestamp, (uint)event->pmotion.windowID,
                           (uint)event->pmotion.which, (uint)event->pmotion.pen_state,
                           event->pmotion.x, event->pmotion.y,
                           event->pmotion.axes[SDL_PEN_AXIS_PRESSURE],
                           event->pmotion.axes[SDL_PEN_AXIS_XTILT],
                           event->pmotion.axes[SDL_PEN_AXIS_YTILT],
                           event->pmotion.axes[SDL_PEN_AXIS_DISTANCE],
                           event->pmotion.axes[SDL_PEN_AXIS_ROTATION],
                           event->pmotion.axes[SDL_PEN_AXIS_SLIDER]);
        break;

#define PRINT_PBUTTON_EVENT(event)                                                                                                               \
    (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u windowid=%u which=%u tip=%u state=%s x=%g y=%g axes=[%g, %g, %g, %g, %g, %g])", \
                       (uint)event->pbutton.timestamp, (uint)event->pbutton.windowID,                                                            \
                       (uint)event->pbutton.which, (uint)event->pbutton.button,                                                                  \
                       event->pbutton.state == SDL_PRESSED ? "pressed" : "released",                                                             \
                       event->pbutton.x, event->pbutton.y,                                                                                       \
                       event->pbutton.axes[SDL_PEN_AXIS_PRESSURE],                                                                               \
                       event->pbutton.axes[SDL_PEN_AXIS_XTILT],                                                                                  \
                       event->pbutton.axes[SDL_PEN_AXIS_YTILT],                                                                                  \
                       event->pbutton.axes[SDL_PEN_AXIS_DISTANCE],                                                                               \
                       event->pbutton.axes[SDL_PEN_AXIS_ROTATION],                                                                               \
                       event->pbutton.axes[SDL_PEN_AXIS_SLIDER])
        SDL_EVENT_CASE(SDL_EVENT_PEN_BUTTON_DOWN)
        PRINT_PBUTTON_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_PEN_BUTTON_UP)
        PRINT_PBUTTON_EVENT(event);
        break;
#undef PRINT_PBUTTON_EVENT

#define PRINT_DROP_EVENT(event) (void)SDL_snprintf(details, sizeof(details), " (data='%s' timestamp=%u windowid=%u x=%f y=%f)", event->drop.data, (uint)event->drop.timestamp, (uint)event->drop.windowID, event->drop.x, event->drop.y)
        SDL_EVENT_CASE(SDL_EVENT_DROP_FILE)
        PRINT_DROP_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_DROP_TEXT)
        PRINT_DROP_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_DROP_BEGIN)
        PRINT_DROP_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_DROP_COMPLETE)
        PRINT_DROP_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_DROP_POSITION)
        PRINT_DROP_EVENT(event);
        break;
#undef PRINT_DROP_EVENT

#define PRINT_AUDIODEV_EVENT(event) (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%u iscapture=%s)", (uint)event->adevice.timestamp, (uint)event->adevice.which, event->adevice.iscapture ? "true" : "false")
        SDL_EVENT_CASE(SDL_EVENT_AUDIO_DEVICE_ADDED)
        PRINT_AUDIODEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_AUDIO_DEVICE_REMOVED)
        PRINT_AUDIODEV_EVENT(event);
        break;
        SDL_EVENT_CASE(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED)
        PRINT_AUDIODEV_EVENT(event);
        break;
#undef PRINT_AUDIODEV_EVENT

        SDL_EVENT_CASE(SDL_EVENT_SENSOR_UPDATE)
        (void)SDL_snprintf(details, sizeof(details), " (timestamp=%u which=%d data[0]=%f data[1]=%f data[2]=%f data[3]=%f data[4]=%f data[5]=%f)",
                           (uint)event->sensor.timestamp, (int)event->sensor.which,
                           event->sensor.data[0], event->sensor.data[1], event->sensor.data[2],
                           event->sensor.data[3], event->sensor.data[4], event->sensor.data[5]);
        break;

#undef SDL_EVENT_CASE

    case SDL_EVENT_POLL_SENTINEL:
        /* No logging necessary for this one */
        break;

    default:
        if (!name[0]) {
            SDL_strlcpy(name, "UNKNOWN", sizeof(name));
            (void)SDL_snprintf(details, sizeof(details), " #%u! (Bug? FIXME?)", (uint)event->type);
        }
        break;
    }

    if (name[0]) {
        SDL_Log("SDL EVENT: %s%s", name, details);
    }

#undef uint
}

void SDL_StopEventLoop(void)
{
    const char *report = SDL_GetHint("SDL_EVENT_QUEUE_STATISTICS");
    int i;
    SDL_EventEntry *entry;

    SDL_LockMutex(SDL_EventQ.lock);

    SDL_EventQ.active = SDL_FALSE;

    if (report && SDL_atoi(report)) {
        SDL_Log("SDL EVENT QUEUE: Maximum events in-flight: %d\n",
                SDL_EventQ.max_events_seen);
    }

    /* Clean out EventQ */
    for (entry = SDL_EventQ.head; entry;) {
        SDL_EventEntry *next = entry->next;
        SDL_free(entry);
        entry = next;
    }
    for (entry = SDL_EventQ.free; entry;) {
        SDL_EventEntry *next = entry->next;
        SDL_free(entry);
        entry = next;
    }

    SDL_AtomicSet(&SDL_EventQ.count, 0);
    SDL_EventQ.max_events_seen = 0;
    SDL_EventQ.head = NULL;
    SDL_EventQ.tail = NULL;
    SDL_EventQ.free = NULL;
    SDL_AtomicSet(&SDL_sentinel_pending, 0);

    SDL_FlushEventMemory(0);

    /* Clear disabled event state */
    for (i = 0; i < SDL_arraysize(SDL_disabled_events); ++i) {
        SDL_free(SDL_disabled_events[i]);
        SDL_disabled_events[i] = NULL;
    }

    if (SDL_event_memory_lock) {
        SDL_DestroyMutex(SDL_event_memory_lock);
        SDL_event_memory_lock = NULL;
    }
    if (SDL_event_watchers_lock) {
        SDL_DestroyMutex(SDL_event_watchers_lock);
        SDL_event_watchers_lock = NULL;
    }
    if (SDL_event_watchers) {
        SDL_free(SDL_event_watchers);
        SDL_event_watchers = NULL;
        SDL_event_watchers_count = 0;
    }
    SDL_zero(SDL_EventOK);

    SDL_UnlockMutex(SDL_EventQ.lock);

    if (SDL_EventQ.lock) {
        SDL_DestroyMutex(SDL_EventQ.lock);
        SDL_EventQ.lock = NULL;
    }
}

/* This function (and associated calls) may be called more than once */
int SDL_StartEventLoop(void)
{
    /* We'll leave the event queue alone, since we might have gotten
       some important events at launch (like SDL_EVENT_DROP_FILE)

       FIXME: Does this introduce any other bugs with events at startup?
     */

    /* Create the lock and set ourselves active */
#ifndef SDL_THREADS_DISABLED
    if (!SDL_EventQ.lock) {
        SDL_EventQ.lock = SDL_CreateMutex();
        if (SDL_EventQ.lock == NULL) {
            return -1;
        }
    }
    SDL_LockMutex(SDL_EventQ.lock);

    if (SDL_event_watchers_lock == NULL) {
        SDL_event_watchers_lock = SDL_CreateMutex();
        if (SDL_event_watchers_lock == NULL) {
            SDL_UnlockMutex(SDL_EventQ.lock);
            return -1;
        }
    }

    if (SDL_event_memory_lock == NULL) {
        SDL_event_memory_lock = SDL_CreateMutex();
        if (SDL_event_memory_lock == NULL) {
            SDL_UnlockMutex(SDL_EventQ.lock);
            return -1;
        }
    }
#endif /* !SDL_THREADS_DISABLED */

    /* Process most event types */
    SDL_SetEventEnabled(SDL_EVENT_TEXT_INPUT, SDL_FALSE);
    SDL_SetEventEnabled(SDL_EVENT_TEXT_EDITING, SDL_FALSE);
#if 0 /* Leave these events enabled so apps can respond to items being dragged onto them at startup */
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, SDL_FALSE);
    SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, SDL_FALSE);
#endif

    SDL_EventQ.active = SDL_TRUE;
    SDL_UnlockMutex(SDL_EventQ.lock);
    return 0;
}

/* Add an event to the event queue -- called with the queue locked */
static int SDL_AddEvent(SDL_Event *event)
{
    SDL_EventEntry *entry;
    const int initial_count = SDL_AtomicGet(&SDL_EventQ.count);
    int final_count;

    if (initial_count >= SDL_MAX_QUEUED_EVENTS) {
        SDL_SetError("Event queue is full (%d events)", initial_count);
        return 0;
    }

    if (SDL_EventQ.free == NULL) {
        entry = (SDL_EventEntry *)SDL_malloc(sizeof(*entry));
        if (entry == NULL) {
            return 0;
        }
    } else {
        entry = SDL_EventQ.free;
        SDL_EventQ.free = entry->next;
    }

    if (SDL_EventLoggingVerbosity > 0) {
        SDL_LogEvent(event);
    }

    SDL_copyp(&entry->event, event);
    if (event->type == SDL_EVENT_POLL_SENTINEL) {
        SDL_AtomicAdd(&SDL_sentinel_pending, 1);
    }

    if (SDL_EventQ.tail) {
        SDL_EventQ.tail->next = entry;
        entry->prev = SDL_EventQ.tail;
        SDL_EventQ.tail = entry;
        entry->next = NULL;
    } else {
        SDL_assert(!SDL_EventQ.head);
        SDL_EventQ.head = entry;
        SDL_EventQ.tail = entry;
        entry->prev = NULL;
        entry->next = NULL;
    }

    final_count = SDL_AtomicAdd(&SDL_EventQ.count, 1) + 1;
    if (final_count > SDL_EventQ.max_events_seen) {
        SDL_EventQ.max_events_seen = final_count;
    }

    ++SDL_last_event_id;

    return 1;
}

/* Remove an event from the queue -- called with the queue locked */
static void SDL_CutEvent(SDL_EventEntry *entry)
{
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }

    if (entry == SDL_EventQ.head) {
        SDL_assert(entry->prev == NULL);
        SDL_EventQ.head = entry->next;
    }
    if (entry == SDL_EventQ.tail) {
        SDL_assert(entry->next == NULL);
        SDL_EventQ.tail = entry->prev;
    }

    if (entry->event.type == SDL_EVENT_POLL_SENTINEL) {
        SDL_AtomicAdd(&SDL_sentinel_pending, -1);
    }

    entry->next = SDL_EventQ.free;
    SDL_EventQ.free = entry;
    SDL_assert(SDL_AtomicGet(&SDL_EventQ.count) > 0);
    SDL_AtomicAdd(&SDL_EventQ.count, -1);
}

static int SDL_SendWakeupEvent(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (_this == NULL || !_this->SendWakeupEvent) {
        return 0;
    }

    SDL_LockMutex(_this->wakeup_lock);
    {
        if (_this->wakeup_window) {
            _this->SendWakeupEvent(_this, _this->wakeup_window);

            /* No more wakeup events needed until we enter a new wait */
            _this->wakeup_window = NULL;
        }
    }
    SDL_UnlockMutex(_this->wakeup_lock);

    return 0;
}

/* Lock the event queue, take a peep at it, and unlock it */
static int SDL_PeepEventsInternal(SDL_Event *events, int numevents, SDL_eventaction action,
                                  Uint32 minType, Uint32 maxType, SDL_bool include_sentinel)
{
    int i, used, sentinels_expected = 0;

    /* Lock the event queue */
    used = 0;

    SDL_LockMutex(SDL_EventQ.lock);
    {
        /* Don't look after we've quit */
        if (!SDL_EventQ.active) {
            /* We get a few spurious events at shutdown, so don't warn then */
            if (action == SDL_GETEVENT) {
                SDL_SetError("The event system has been shut down");
            }
            SDL_UnlockMutex(SDL_EventQ.lock);
            return -1;
        }
        if (action == SDL_ADDEVENT) {
            for (i = 0; i < numevents; ++i) {
                used += SDL_AddEvent(&events[i]);
            }
        } else {
            SDL_EventEntry *entry, *next;
            Uint32 type;

            for (entry = SDL_EventQ.head; entry && (events == NULL || used < numevents); entry = next) {
                next = entry->next;
                type = entry->event.type;
                if (minType <= type && type <= maxType) {
                    if (events) {
                        SDL_copyp(&events[used], &entry->event);

                        if (action == SDL_GETEVENT) {
                            SDL_CutEvent(entry);
                        }
                    }
                    if (type == SDL_EVENT_POLL_SENTINEL) {
                        /* Special handling for the sentinel event */
                        if (!include_sentinel) {
                            /* Skip it, we don't want to include it */
                            continue;
                        }
                        if (events == NULL || action != SDL_GETEVENT) {
                            ++sentinels_expected;
                        }
                        if (SDL_AtomicGet(&SDL_sentinel_pending) > sentinels_expected) {
                            /* Skip it, there's another one pending */
                            continue;
                        }
                    }
                    ++used;
                }
            }
        }
    }
    SDL_UnlockMutex(SDL_EventQ.lock);

    if (used > 0 && action == SDL_ADDEVENT) {
        SDL_SendWakeupEvent();
    }

    return used;
}
int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action,
                   Uint32 minType, Uint32 maxType)
{
    return SDL_PeepEventsInternal(events, numevents, action, minType, maxType, SDL_FALSE);
}

SDL_bool SDL_HasEvent(Uint32 type)
{
    return SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, type, type) > 0;
}

SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType)
{
    return SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, minType, maxType) > 0;
}

void SDL_FlushEvent(Uint32 type)
{
    SDL_FlushEvents(type, type);
}

void SDL_FlushEvents(Uint32 minType, Uint32 maxType)
{
    SDL_EventEntry *entry, *next;
    Uint32 type;

    /* Make sure the events are current */
#if 0
    /* Actually, we can't do this since we might be flushing while processing
       a resize event, and calling this might trigger further resize events.
    */
    SDL_PumpEvents();
#endif

    /* Lock the event queue */
    SDL_LockMutex(SDL_EventQ.lock);
    {
        /* Don't look after we've quit */
        if (!SDL_EventQ.active) {
            SDL_UnlockMutex(SDL_EventQ.lock);
            return;
        }
        for (entry = SDL_EventQ.head; entry; entry = next) {
            next = entry->next;
            type = entry->event.type;
            if (minType <= type && type <= maxType) {
                SDL_CutEvent(entry);
            }
        }
    }
    SDL_UnlockMutex(SDL_EventQ.lock);
}

/* Run the system dependent event loops */
static void SDL_PumpEventsInternal(SDL_bool push_sentinel)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    /* Free old event memory */
    /*SDL_FlushEventMemory(SDL_last_event_id - SDL_MAX_QUEUED_EVENTS);*/
    if (SDL_AtomicGet(&SDL_EventQ.count) == 0) {
        SDL_FlushEventMemory(SDL_last_event_id);
    }

    /* Release any keys held down from last frame */
    SDL_ReleaseAutoReleaseKeys();

    /* Get events from the video subsystem */
    if (_this) {
        _this->PumpEvents(_this);
    }

#ifndef SDL_AUDIO_DISABLED
    SDL_UpdateAudio();
#endif

#ifndef SDL_SENSOR_DISABLED
    /* Check for sensor state change */
    if (SDL_update_sensors) {
        SDL_UpdateSensors();
    }
#endif

#ifndef SDL_JOYSTICK_DISABLED
    /* Check for joystick state change */
    if (SDL_update_joysticks) {
        SDL_UpdateJoysticks();
    }
#endif

    SDL_SendPendingSignalEvents(); /* in case we had a signal handler fire, etc. */

    if (push_sentinel && SDL_EventEnabled(SDL_EVENT_POLL_SENTINEL)) {
        SDL_Event sentinel;

        /* Make sure we don't already have a sentinel in the queue, and add one to the end */
        if (SDL_AtomicGet(&SDL_sentinel_pending) > 0) {
            SDL_PeepEventsInternal(&sentinel, 1, SDL_GETEVENT, SDL_EVENT_POLL_SENTINEL, SDL_EVENT_POLL_SENTINEL, SDL_TRUE);
        }

        sentinel.type = SDL_EVENT_POLL_SENTINEL;
        sentinel.common.timestamp = 0;
        SDL_PushEvent(&sentinel);
    }
}

void SDL_PumpEvents(void)
{
    SDL_PumpEventsInternal(SDL_FALSE);
}

/* Public functions */

SDL_bool SDL_PollEvent(SDL_Event *event)
{
    return SDL_WaitEventTimeoutNS(event, 0);
}

static SDL_bool SDL_events_need_periodic_poll(void)
{
    SDL_bool need_periodic_poll = SDL_FALSE;

#ifndef SDL_JOYSTICK_DISABLED
    need_periodic_poll = SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_update_joysticks;
#endif

    return need_periodic_poll;
}

static int SDL_WaitEventTimeout_Device(SDL_VideoDevice *_this, SDL_Window *wakeup_window, SDL_Event *event, Uint64 start, Sint64 timeoutNS)
{
    Sint64 loop_timeoutNS = timeoutNS;
    SDL_bool need_periodic_poll = SDL_events_need_periodic_poll();

    for (;;) {
        int status;
        /* Pump events on entry and each time we wake to ensure:
           a) All pending events are batch processed after waking up from a wait
           b) Waiting can be completely skipped if events are already available to be pumped
           c) Periodic processing that takes place in some platform PumpEvents() functions happens
           d) Signals received in WaitEventTimeout() are turned into SDL events
        */
        SDL_PumpEventsInternal(SDL_TRUE);

        SDL_LockMutex(_this->wakeup_lock);
        {
            status = SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
            /* If status == 0 we are going to block so wakeup will be needed. */
            if (status == 0) {
                _this->wakeup_window = wakeup_window;
            } else {
                _this->wakeup_window = NULL;
            }
        }
        SDL_UnlockMutex(_this->wakeup_lock);

        if (status < 0) {
            /* Got an error: return */
            break;
        }
        if (status > 0) {
            /* There is an event, we can return. */
            return 1;
        }
        /* No events found in the queue, call WaitEventTimeout to wait for an event. */
        if (timeoutNS > 0) {
            Sint64 elapsed = SDL_GetTicksNS() - start;
            if (elapsed >= timeoutNS) {
                /* Set wakeup_window to NULL without holding the lock. */
                _this->wakeup_window = NULL;
                return 0;
            }
            loop_timeoutNS = (timeoutNS - elapsed);
        }
        if (need_periodic_poll) {
            if (loop_timeoutNS >= 0) {
                loop_timeoutNS = SDL_min(loop_timeoutNS, PERIODIC_POLL_INTERVAL_NS);
            } else {
                loop_timeoutNS = PERIODIC_POLL_INTERVAL_NS;
            }
        }
        status = _this->WaitEventTimeout(_this, loop_timeoutNS);
        /* Set wakeup_window to NULL without holding the lock. */
        _this->wakeup_window = NULL;
        if (status == 0 && need_periodic_poll && loop_timeoutNS == PERIODIC_POLL_INTERVAL_NS) {
            /* We may have woken up to poll. Try again */
            continue;
        } else if (status <= 0) {
            /* There is either an error or the timeout is elapsed: return */
            return status;
        }
        /* An event was found and pumped into the SDL events queue. Continue the loop
          to let SDL_PeepEvents pick it up .*/
    }
    return 0;
}

static SDL_bool SDL_events_need_polling(void)
{
    SDL_bool need_polling = SDL_FALSE;

#ifndef SDL_JOYSTICK_DISABLED
    need_polling = SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_update_joysticks && SDL_JoysticksOpened();
#endif

#ifndef SDL_SENSOR_DISABLED
    need_polling = need_polling ||
                   (SDL_WasInit(SDL_INIT_SENSOR) && SDL_update_sensors && SDL_SensorsOpened());
#endif

    return need_polling;
}

static SDL_Window *SDL_find_active_window(SDL_VideoDevice *_this)
{
    SDL_Window *window;
    for (window = _this->windows; window; window = window->next) {
        if (!window->is_destroying) {
            return window;
        }
    }
    return NULL;
}

SDL_bool SDL_WaitEvent(SDL_Event *event)
{
    return SDL_WaitEventTimeoutNS(event, -1);
}

SDL_bool SDL_WaitEventTimeout(SDL_Event *event, Sint32 timeoutMS)
{
    Sint64 timeoutNS;

    if (timeoutMS > 0) {
        timeoutNS = SDL_MS_TO_NS(timeoutMS);
    } else {
        timeoutNS = timeoutMS;
    }
    return SDL_WaitEventTimeoutNS(event, timeoutNS);
}

SDL_bool SDL_WaitEventTimeoutNS(SDL_Event *event, Sint64 timeoutNS)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    SDL_Window *wakeup_window;
    Uint64 start, expiration;
    SDL_bool include_sentinel = (timeoutNS == 0);
    int result;

    if (timeoutNS > 0) {
        start = SDL_GetTicksNS();
        expiration = start + timeoutNS;
    } else {
        start = 0;
        expiration = 0;
    }

    /* If there isn't a poll sentinel event pending, pump events and add one */
    if (SDL_AtomicGet(&SDL_sentinel_pending) == 0) {
        SDL_PumpEventsInternal(SDL_TRUE);
    }

    /* First check for existing events */
    result = SDL_PeepEventsInternal(event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST, include_sentinel);
    if (result < 0) {
        return SDL_FALSE;
    }
    if (include_sentinel) {
        if (event) {
            if (event->type == SDL_EVENT_POLL_SENTINEL) {
                /* Reached the end of a poll cycle, and not willing to wait */
                return SDL_FALSE;
            }
        } else {
            /* Need to peek the next event to check for sentinel */
            SDL_Event dummy;

            if (SDL_PeepEventsInternal(&dummy, 1, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST, SDL_TRUE) &&
                dummy.type == SDL_EVENT_POLL_SENTINEL) {
                SDL_PeepEventsInternal(&dummy, 1, SDL_GETEVENT, SDL_EVENT_POLL_SENTINEL, SDL_EVENT_POLL_SENTINEL, SDL_TRUE);
                /* Reached the end of a poll cycle, and not willing to wait */
                return SDL_FALSE;
            }
        }
    }
    if (result == 0) {
        if (timeoutNS == 0) {
            /* No events available, and not willing to wait */
            return SDL_FALSE;
        }
    } else {
        /* Has existing events */
        return SDL_TRUE;
    }
    /* We should have completely handled timeoutNS == 0 above */
    SDL_assert(timeoutNS != 0);

    if (_this && _this->WaitEventTimeout && _this->SendWakeupEvent && !SDL_events_need_polling()) {
        /* Look if a shown window is available to send the wakeup event. */
        wakeup_window = SDL_find_active_window(_this);
        if (wakeup_window) {
            result = SDL_WaitEventTimeout_Device(_this, wakeup_window, event, start, timeoutNS);
            if (result > 0) {
                return SDL_TRUE;
            } else if (result == 0) {
                return SDL_FALSE;
            } else {
                /* There may be implementation-defined conditions where the backend cannot
                 * reliably wait for the next event. If that happens, fall back to polling.
                 */
            }
        }
    }

    for (;;) {
        SDL_PumpEventsInternal(SDL_TRUE);

        if (SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) {
            return SDL_TRUE;
        }

        Uint64 delay = SDL_MS_TO_NS(1);
        if (timeoutNS > 0) {
            Uint64 now = SDL_GetTicksNS();
            if (now >= expiration) {
                /* Timeout expired and no events */
                return SDL_FALSE;
            }
            delay = SDL_min((expiration - now), delay);
        }
        SDL_DelayNS(delay);
    }
}

int SDL_PushEvent(SDL_Event *event)
{
    if (!event->common.timestamp) {
        event->common.timestamp = SDL_GetTicksNS();
    }

    if (SDL_EventOK.callback || SDL_event_watchers_count > 0) {
        SDL_LockMutex(SDL_event_watchers_lock);
        {
            if (SDL_EventOK.callback && !SDL_EventOK.callback(SDL_EventOK.userdata, event)) {
                SDL_UnlockMutex(SDL_event_watchers_lock);
                return 0;
            }

            if (SDL_event_watchers_count > 0) {
                /* Make sure we only dispatch the current watcher list */
                int i, event_watchers_count = SDL_event_watchers_count;

                SDL_event_watchers_dispatching = SDL_TRUE;
                for (i = 0; i < event_watchers_count; ++i) {
                    if (!SDL_event_watchers[i].removed) {
                        SDL_event_watchers[i].callback(SDL_event_watchers[i].userdata, event);
                    }
                }
                SDL_event_watchers_dispatching = SDL_FALSE;

                if (SDL_event_watchers_removed) {
                    for (i = SDL_event_watchers_count; i--;) {
                        if (SDL_event_watchers[i].removed) {
                            --SDL_event_watchers_count;
                            if (i < SDL_event_watchers_count) {
                                SDL_memmove(&SDL_event_watchers[i], &SDL_event_watchers[i + 1], (SDL_event_watchers_count - i) * sizeof(SDL_event_watchers[i]));
                            }
                        }
                    }
                    SDL_event_watchers_removed = SDL_FALSE;
                }
            }
        }
        SDL_UnlockMutex(SDL_event_watchers_lock);
    }

    if (SDL_PeepEvents(event, 1, SDL_ADDEVENT, 0, 0) <= 0) {
        return -1;
    }

    return 1;
}

void SDL_SetEventFilter(SDL_EventFilter filter, void *userdata)
{
    SDL_LockMutex(SDL_event_watchers_lock);
    {
        /* Set filter and discard pending events */
        SDL_EventOK.callback = filter;
        SDL_EventOK.userdata = userdata;
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    }
    SDL_UnlockMutex(SDL_event_watchers_lock);
}

SDL_bool SDL_GetEventFilter(SDL_EventFilter *filter, void **userdata)
{
    SDL_EventWatcher event_ok;

    SDL_LockMutex(SDL_event_watchers_lock);
    {
        event_ok = SDL_EventOK;
    }
    SDL_UnlockMutex(SDL_event_watchers_lock);

    if (filter) {
        *filter = event_ok.callback;
    }
    if (userdata) {
        *userdata = event_ok.userdata;
    }
    return event_ok.callback ? SDL_TRUE : SDL_FALSE;
}

int SDL_AddEventWatch(SDL_EventFilter filter, void *userdata)
{
    int result = 0;

    SDL_LockMutex(SDL_event_watchers_lock);
    {
        SDL_EventWatcher *event_watchers;

        event_watchers = SDL_realloc(SDL_event_watchers, (SDL_event_watchers_count + 1) * sizeof(*event_watchers));
        if (event_watchers) {
            SDL_EventWatcher *watcher;

            SDL_event_watchers = event_watchers;
            watcher = &SDL_event_watchers[SDL_event_watchers_count];
            watcher->callback = filter;
            watcher->userdata = userdata;
            watcher->removed = SDL_FALSE;
            ++SDL_event_watchers_count;
        } else {
            result = -1;
        }
    }
    SDL_UnlockMutex(SDL_event_watchers_lock);

    return result;
}

void SDL_DelEventWatch(SDL_EventFilter filter, void *userdata)
{
    SDL_LockMutex(SDL_event_watchers_lock);
    {
        int i;

        for (i = 0; i < SDL_event_watchers_count; ++i) {
            if (SDL_event_watchers[i].callback == filter && SDL_event_watchers[i].userdata == userdata) {
                if (SDL_event_watchers_dispatching) {
                    SDL_event_watchers[i].removed = SDL_TRUE;
                    SDL_event_watchers_removed = SDL_TRUE;
                } else {
                    --SDL_event_watchers_count;
                    if (i < SDL_event_watchers_count) {
                        SDL_memmove(&SDL_event_watchers[i], &SDL_event_watchers[i + 1], (SDL_event_watchers_count - i) * sizeof(SDL_event_watchers[i]));
                    }
                }
                break;
            }
        }
    }
    SDL_UnlockMutex(SDL_event_watchers_lock);
}

void SDL_FilterEvents(SDL_EventFilter filter, void *userdata)
{
    SDL_LockMutex(SDL_EventQ.lock);
    {
        SDL_EventEntry *entry, *next;
        for (entry = SDL_EventQ.head; entry; entry = next) {
            next = entry->next;
            if (!filter(userdata, &entry->event)) {
                SDL_CutEvent(entry);
            }
        }
    }
    SDL_UnlockMutex(SDL_EventQ.lock);
}

void SDL_SetEventEnabled(Uint32 type, SDL_bool enabled)
{
    SDL_bool current_state;
    Uint8 hi = ((type >> 8) & 0xff);
    Uint8 lo = (type & 0xff);

    if (SDL_disabled_events[hi] &&
        (SDL_disabled_events[hi]->bits[lo / 32] & (1 << (lo & 31)))) {
        current_state = SDL_FALSE;
    } else {
        current_state = SDL_TRUE;
    }

    if (enabled != current_state) {
        if (enabled) {
#ifdef _MSC_VER /* Visual Studio analyzer can't tell that SDL_disabled_events[hi] isn't NULL if enabled is true */
#pragma warning(push)
#pragma warning(disable : 6011)
#endif
            SDL_disabled_events[hi]->bits[lo / 32] &= ~(1 << (lo & 31));
#ifdef _MSC_VER
#pragma warning(pop)
#endif

            /* Gamepad events depend on joystick events */
            switch (type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_ADDED, SDL_TRUE);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_REMOVED, SDL_TRUE);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_TRUE);
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_HAT_MOTION, SDL_TRUE);
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BUTTON_DOWN, SDL_TRUE);
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BUTTON_UP, SDL_TRUE);
                break;
            case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
                SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE, SDL_TRUE);
                break;
            default:
                break;
            }
        } else {
            /* Disable this event type and discard pending events */
            if (!SDL_disabled_events[hi]) {
                SDL_disabled_events[hi] = (SDL_DisabledEventBlock *)SDL_calloc(1, sizeof(SDL_DisabledEventBlock));
            }
            /* Out of memory, nothing we can do... */
            if (SDL_disabled_events[hi]) {
                SDL_disabled_events[hi]->bits[lo / 32] |= (1 << (lo & 31));
                SDL_FlushEvent(type);
            }
        }

        /* turn off drag'n'drop support if we've disabled the events.
           This might change some UI details at the OS level. */
        if (type == SDL_EVENT_DROP_FILE || type == SDL_EVENT_DROP_TEXT) {
            SDL_ToggleDragAndDropSupport();
        }
    }
}

SDL_bool SDL_EventEnabled(Uint32 type)
{
    Uint8 hi = ((type >> 8) & 0xff);
    Uint8 lo = (type & 0xff);

    if (SDL_disabled_events[hi] &&
        (SDL_disabled_events[hi]->bits[lo / 32] & (1 << (lo & 31)))) {
        return SDL_FALSE;
    } else {
        return SDL_TRUE;
    }
}

Uint32 SDL_RegisterEvents(int numevents)
{
    Uint32 event_base;

    if ((numevents > 0) && (SDL_userevents + numevents <= SDL_EVENT_LAST)) {
        event_base = SDL_userevents;
        SDL_userevents += numevents;
    } else {
        event_base = (Uint32)-1;
    }
    return event_base;
}

int SDL_SendAppEvent(SDL_EventType eventType)
{
    int posted;

    posted = 0;
    if (SDL_EventEnabled(eventType)) {
        SDL_Event event;
        event.type = eventType;
        event.common.timestamp = 0;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

int SDL_SendKeymapChangedEvent(void)
{
    return SDL_SendAppEvent(SDL_EVENT_KEYMAP_CHANGED);
}

int SDL_SendLocaleChangedEvent(void)
{
    return SDL_SendAppEvent(SDL_EVENT_LOCALE_CHANGED);
}

int SDL_SendSystemThemeChangedEvent(void)
{
    return SDL_SendAppEvent(SDL_EVENT_SYSTEM_THEME_CHANGED);
}

int SDL_InitEvents(void)
{
#ifndef SDL_JOYSTICK_DISABLED
    SDL_AddHintCallback(SDL_HINT_AUTO_UPDATE_JOYSTICKS, SDL_AutoUpdateJoysticksChanged, NULL);
#endif
#ifndef SDL_SENSOR_DISABLED
    SDL_AddHintCallback(SDL_HINT_AUTO_UPDATE_SENSORS, SDL_AutoUpdateSensorsChanged, NULL);
#endif
    SDL_AddHintCallback(SDL_HINT_EVENT_LOGGING, SDL_EventLoggingChanged, NULL);
    SDL_AddHintCallback(SDL_HINT_POLL_SENTINEL, SDL_PollSentinelChanged, NULL);
    if (SDL_StartEventLoop() < 0) {
        SDL_DelHintCallback(SDL_HINT_EVENT_LOGGING, SDL_EventLoggingChanged, NULL);
        return -1;
    }

    SDL_InitQuit();

    return 0;
}

void SDL_QuitEvents(void)
{
    SDL_QuitQuit();
    SDL_StopEventLoop();
    SDL_DelHintCallback(SDL_HINT_POLL_SENTINEL, SDL_PollSentinelChanged, NULL);
    SDL_DelHintCallback(SDL_HINT_EVENT_LOGGING, SDL_EventLoggingChanged, NULL);
#ifndef SDL_JOYSTICK_DISABLED
    SDL_DelHintCallback(SDL_HINT_AUTO_UPDATE_JOYSTICKS, SDL_AutoUpdateJoysticksChanged, NULL);
#endif
#ifndef SDL_SENSOR_DISABLED
    SDL_DelHintCallback(SDL_HINT_AUTO_UPDATE_SENSORS, SDL_AutoUpdateSensorsChanged, NULL);
#endif
}
