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

/* Pressure-sensitive pen handling code for SDL */

#include "../SDL_hints_c.h"
#include "SDL_events_c.h"
#include "SDL_pen_c.h"

#define PEN_MOUSE_EMULATE   0 /* pen behaves like mouse */
#define PEN_MOUSE_STATELESS 1 /* pen does not update mouse state */
#define PEN_MOUSE_DISABLED  2 /* pen does not send mouse events */

/* flags that are not SDL_PEN_FLAG_ */
#define PEN_FLAGS_CAPABILITIES (~(SDL_PEN_FLAG_NEW | SDL_PEN_FLAG_DETACHED | SDL_PEN_FLAG_STALE))

#define PEN_GET_PUBLIC_STATUS_MASK(pen) (((pen)->header.flags & (SDL_PEN_ERASER_MASK | SDL_PEN_DOWN_MASK)))

static int pen_mouse_emulation_mode = PEN_MOUSE_EMULATE; /* SDL_HINT_PEN_NOT_MOUSE */

static int pen_delay_mouse_button_mode = 1; /* SDL_HINT_PEN_DELAY_MOUSE_BUTTON */

#ifndef SDL_THREADS_DISABLED
static SDL_Mutex *SDL_pen_access_lock;
#  define SDL_LOCK_PENS()   SDL_LockMutex(SDL_pen_access_lock)
#  define SDL_UNLOCK_PENS() SDL_UnlockMutex(SDL_pen_access_lock)
#else
#  define SDL_LOCK_PENS()
#  define SDL_UNLOCK_PENS()
#endif

static struct
{
    SDL_Pen *pens;         /* if "sorted" is SDL_TRUE:
                              sorted by: (is-attached, id):
                              - first all attached pens, in ascending ID order
                              - then all detached pens, in ascending ID order */
    size_t pens_allocated; /* # entries allocated to "pens" */
    size_t pens_known;     /* <= pens_allocated; this includes detached pens */
    size_t pens_attached;  /* <= pens_known; all attached pens are at the beginning of "pens" */
    SDL_bool sorted;       /* This is SDL_FALSE in the period between SDL_PenGCMark() and SDL_PenGCSWeep() */
} pen_handler;

static SDL_PenID pen_invalid = { SDL_PEN_INVALID };

static SDL_GUID pen_guid_zero = { { 0 } };

#define SDL_LOAD_LOCK_PEN(penvar, instance_id, err_return) \
    SDL_Pen *penvar;                              \
    if (instance_id == SDL_PEN_INVALID) {         \
        SDL_SetError("Invalid SDL_PenID");        \
        return (err_return);                      \
    }                                             \
    SDL_LOCK_PENS();\
    penvar = SDL_GetPenPtr(instance_id);          \
    if (!(penvar)) {                              \
        SDL_SetError("Stale SDL_PenID");          \
        SDL_UNLOCK_PENS();                        \
        return (err_return);                      \
    }

static int SDL_GUIDCompare(SDL_GUID lhs, SDL_GUID rhs)
{
    return SDL_memcmp(lhs.data, rhs.data, sizeof(lhs.data));
}

static int SDLCALL pen_compare(const void *lhs, const void *rhs)
{
    int left_inactive = (((const SDL_Pen *)lhs)->header.flags & SDL_PEN_FLAG_DETACHED);
    int right_inactive = (((const SDL_Pen *)rhs)->header.flags & SDL_PEN_FLAG_DETACHED);
    if (left_inactive && !right_inactive) {
        return 1;
    }
    if (!left_inactive && right_inactive) {
        return -1;
    }
    return ((const SDL_Pen *)lhs)->header.id - ((const SDL_Pen *)rhs)->header.id;
}

static int SDLCALL pen_header_compare(const void *lhs, const void *rhs)
{
    const SDL_PenHeader *l = (const SDL_PenHeader *)lhs;
    const SDL_PenHeader *r = (const SDL_PenHeader *)rhs;
    int l_detached = l->flags & SDL_PEN_FLAG_DETACHED;
    int r_detached = r->flags & SDL_PEN_FLAG_DETACHED;

    if (l_detached != r_detached) {
        if (l_detached) {
            return -1;
        }
        return 1;
    }

    return l->id - r->id;
}

SDL_Pen *SDL_GetPenPtr(Uint32 instance_id)
{
    unsigned int i;

    if (!pen_handler.pens) {
        return NULL;
    }

    if (pen_handler.sorted) {
        SDL_PenHeader key;
        SDL_Pen *pen;

        SDL_zero(key);
        key.id = instance_id;

        pen = (SDL_Pen *)SDL_bsearch(&key, pen_handler.pens, pen_handler.pens_known, sizeof(SDL_Pen), pen_header_compare);
        if (pen) {
            return pen;
        }
        /* If the pen is not active, fall through */
    }

    /* fall back to linear search */
    for (i = 0; i < pen_handler.pens_known; ++i) {
        if (pen_handler.pens[i].header.id == instance_id) {
            return &pen_handler.pens[i];
        }
    }
    return NULL;
}

SDL_PenID *SDL_GetPens(int *count)
{
    int i;
    int pens_nr = (int)pen_handler.pens_attached;
    SDL_PenID *pens = (SDL_PenID *)SDL_calloc(pens_nr + 1, sizeof(SDL_PenID));
    if (!pens) { /* OOM */
        return pens;
    }

    for (i = 0; i < pens_nr; ++i) {
        pens[i] = pen_handler.pens[i].header.id;
    }

    if (count) {
        *count = pens_nr;
    }
    return pens;
}

SDL_PenID SDL_GetPenFromGUID(SDL_GUID guid)
{
    unsigned int i;
    /* Must do linear search */
    SDL_LOCK_PENS();
    for (i = 0; i < pen_handler.pens_known; ++i) {
        SDL_Pen *pen = &pen_handler.pens[i];

        if (0 == SDL_GUIDCompare(guid, pen->guid)) {
            SDL_UNLOCK_PENS();
            return pen->header.id;
        }
    }
    SDL_UNLOCK_PENS();
    return pen_invalid;
}

SDL_bool SDL_PenConnected(SDL_PenID instance_id)
{
    SDL_Pen *pen;
    SDL_bool result;

    if (instance_id == SDL_PEN_INVALID) {
        return SDL_FALSE;
    }

    SDL_LOCK_PENS();
    pen = SDL_GetPenPtr(instance_id);
    if (!pen) {
        SDL_UNLOCK_PENS();
        return SDL_FALSE;
    }
    result = (pen->header.flags & SDL_PEN_FLAG_DETACHED) ? SDL_FALSE : SDL_TRUE;
    SDL_UNLOCK_PENS();
    return result;
}

SDL_GUID SDL_GetPenGUID(SDL_PenID instance_id)
{
    SDL_GUID result;
    SDL_LOAD_LOCK_PEN(pen, instance_id, pen_guid_zero);
    result = pen->guid;
    SDL_UNLOCK_PENS();
    return result;
}

const char *SDL_GetPenName(SDL_PenID instance_id)
{
    const char *result;
    SDL_LOAD_LOCK_PEN(pen, instance_id, NULL);
    result = pen->name; /* Allocated separately from the pen table, so it is safe to hand to client code  */
    SDL_UNLOCK_PENS();
    return result;
}

SDL_PenSubtype SDL_GetPenType(SDL_PenID instance_id)
{
    SDL_PenSubtype result;
    SDL_LOAD_LOCK_PEN(pen, instance_id, SDL_PEN_TYPE_UNKNOWN);
    result = pen->type;
    SDL_UNLOCK_PENS();
    return result;
}

SDL_PenCapabilityFlags SDL_GetPenCapabilities(SDL_PenID instance_id, SDL_PenCapabilityInfo *info)
{
    Uint32 result;
    SDL_LOAD_LOCK_PEN(pen, instance_id, 0u);
    if (info) {
        *info = pen->info;
    }
    result = pen->header.flags & PEN_FLAGS_CAPABILITIES;
    SDL_UNLOCK_PENS();
    return result;
}

Uint32 SDL_GetPenStatus(SDL_PenID instance_id, float *x, float *y, float *axes, size_t num_axes)
{
    Uint32 result;
    SDL_LOAD_LOCK_PEN(pen, instance_id, 0u);

    if (x) {
        *x = pen->last.x;
    }
    if (y) {
        *y = pen->last.y;
    }
    if (axes && num_axes) {
        size_t axes_to_copy = SDL_min(num_axes, SDL_PEN_NUM_AXES);
        SDL_memcpy(axes, pen->last.axes, sizeof(float) * axes_to_copy);
    }
    result = pen->last.buttons | (pen->header.flags & (SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK | SDL_PEN_DOWN_MASK));
    SDL_UNLOCK_PENS();
    return result;
}

/* Backend functionality */

/* Sort all pens.  Only safe during SDL_LOCK_PENS. */
static void pen_sort(void)
{
    SDL_qsort(pen_handler.pens,
              pen_handler.pens_known,
              sizeof(SDL_Pen),
              pen_compare);
    pen_handler.sorted = SDL_TRUE;
}

SDL_Pen *SDL_PenModifyBegin(Uint32 instance_id)
{
    SDL_PenID id = { 0 };
    const size_t alloc_growth_constant = 1; /* Expect few pens */
    SDL_Pen *pen;

    id = instance_id;

    if (id == SDL_PEN_INVALID) {
        SDL_SetError("Invalid SDL_PenID");
        return NULL;
    }

    SDL_LOCK_PENS();
    pen = SDL_GetPenPtr(id);

    if (!pen) {
        if (!pen_handler.pens || pen_handler.pens_known == pen_handler.pens_allocated) {
            size_t pens_to_allocate = pen_handler.pens_allocated + alloc_growth_constant;
            SDL_Pen *pens;
            if (pen_handler.pens) {
                pens = (SDL_Pen *)SDL_realloc(pen_handler.pens, sizeof(SDL_Pen) * pens_to_allocate);
                SDL_memset(pens + pen_handler.pens_known, 0,
                           sizeof(SDL_Pen) * (pens_to_allocate - pen_handler.pens_allocated));
            } else {
                pens = (SDL_Pen *)SDL_calloc(sizeof(SDL_Pen), pens_to_allocate);
            }
            pen_handler.pens = pens;
            pen_handler.pens_allocated = pens_to_allocate;
        }
        pen = &pen_handler.pens[pen_handler.pens_known];
        pen_handler.pens_known += 1;

        /* Default pen initialization */
        pen->header.id = id;
        pen->header.flags = SDL_PEN_FLAG_NEW;
        pen->info.num_buttons = SDL_PEN_INFO_UNKNOWN;
        pen->info.max_tilt = SDL_PEN_INFO_UNKNOWN;
        pen->type = SDL_PEN_TYPE_PEN;
        pen->name = (char *)SDL_calloc(1, SDL_PEN_MAX_NAME); /* Never deallocated */
    }
    return pen;
}

void SDL_PenModifyAddCapabilities(SDL_Pen *pen, Uint32 capabilities)
{
    if (capabilities & SDL_PEN_ERASER_MASK) {
        pen->header.flags &= ~SDL_PEN_INK_MASK;
    } else if (capabilities & SDL_PEN_INK_MASK) {
        pen->header.flags &= ~SDL_PEN_ERASER_MASK;
    }
    pen->header.flags |= (capabilities & PEN_FLAGS_CAPABILITIES);
}

static void pen_hotplug_attach(SDL_Pen *pen)
{
    if (!pen->header.window) {
        /* Attach to default window */
        const SDL_Mouse *mouse = SDL_GetMouse();
        SDL_SendPenWindowEvent(0, pen->header.id, mouse->focus);
    }
}

static void pen_hotplug_detach(SDL_Pen *pen)
{
    SDL_SendPenWindowEvent(0, pen->header.id, NULL);
}

void SDL_PenModifyEnd(SDL_Pen *pen, SDL_bool attach)
{
    SDL_bool is_new = pen->header.flags & SDL_PEN_FLAG_NEW;
    SDL_bool was_attached = !(pen->header.flags & (SDL_PEN_FLAG_DETACHED | SDL_PEN_FLAG_NEW));
    SDL_bool broke_sort_order = SDL_FALSE;

    if (pen->type == SDL_PEN_TYPE_NONE) {
        /* remove pen */
        if (!is_new) {
            if (!(pen->header.flags & SDL_PEN_FLAG_ERROR)) {
                SDL_Log("Error: attempt to remove known pen %lu", (unsigned long) pen->header.id);
                pen->header.flags |= SDL_PEN_FLAG_ERROR;
            }

            /* Treat as detached pen of unknown type instead */
            pen->type = SDL_PEN_TYPE_PEN;
            attach = SDL_FALSE;
        } else {
            pen_handler.pens_known -= 1;
            SDL_free(pen->name);
            SDL_memset(pen, 0, sizeof(SDL_Pen));
            SDL_UNLOCK_PENS();
            return;
        }
    }

    pen->header.flags &= ~(SDL_PEN_FLAG_NEW | SDL_PEN_FLAG_STALE | SDL_PEN_FLAG_DETACHED);
    if (attach == SDL_FALSE) {
        pen->header.flags |= SDL_PEN_FLAG_DETACHED;
        if (was_attached) {
            broke_sort_order = SDL_TRUE;
            if (!is_new) {
                pen_handler.pens_attached -= 1;
            }
            pen_hotplug_detach(pen);
        }
    } else if (!was_attached || is_new) {
        broke_sort_order = SDL_TRUE;
        pen_handler.pens_attached += 1;
        pen_hotplug_attach(pen);
    }

    if (is_new) {
        /* default: name */
        if (!pen->name[0]) {
            SDL_snprintf(pen->name, SDL_PEN_MAX_NAME,
                         "%s %lu",
                         pen->type == SDL_PEN_TYPE_ERASER ? "Eraser" : "Pen",
                         (unsigned long) pen->header.id);
        }

        /* default: enabled axes */
        if (!(pen->header.flags & (SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK))) {
            pen->info.max_tilt = 0; /* no tilt => no max_tilt */
        }

        /* sanity-check GUID */
        if (0 == SDL_GUIDCompare(pen->guid, pen_guid_zero)) {
            SDL_Log("Error: pen %lu: has GUID 0", (unsigned long) pen->header.id);
        }

        /* pen or eraser? */
        if (pen->type == SDL_PEN_TYPE_ERASER || pen->header.flags & SDL_PEN_ERASER_MASK) {
            pen->header.flags = (pen->header.flags & ~SDL_PEN_INK_MASK) | SDL_PEN_ERASER_MASK;
            pen->type = SDL_PEN_TYPE_ERASER;
        } else {
            pen->header.flags = (pen->header.flags & ~SDL_PEN_ERASER_MASK) | SDL_PEN_INK_MASK;
        }

        broke_sort_order = SDL_TRUE;
    }
    if (broke_sort_order && pen_handler.sorted) {
        /* Maintain sortedness invariant */
        pen_sort();
    }
    SDL_UNLOCK_PENS();
}

void SDL_PenGCMark(void)
{
    unsigned int i;
    SDL_LOCK_PENS();
    for (i = 0; i < pen_handler.pens_known; ++i) {
        SDL_Pen *pen = &pen_handler.pens[i];
        pen->header.flags |= SDL_PEN_FLAG_STALE;
    }
    pen_handler.sorted = SDL_FALSE;
    SDL_UNLOCK_PENS();
}

void SDL_PenGCSweep(void *context, void (*free_deviceinfo)(Uint32, void *, void *))
{
    unsigned int i;
    pen_handler.pens_attached = 0;

    SDL_LOCK_PENS();
    /* We don't actually free the SDL_Pen entries, so that we can still answer queries about
       formerly active SDL_PenIDs later.  */
    for (i = 0; i < pen_handler.pens_known; ++i) {
        SDL_Pen *pen = &pen_handler.pens[i];

        if (pen->header.flags & SDL_PEN_FLAG_STALE) {
            pen->header.flags |= SDL_PEN_FLAG_DETACHED;
            pen_hotplug_detach(pen);
            if (pen->deviceinfo) {
                free_deviceinfo(pen->header.id, pen->deviceinfo, context);
                pen->deviceinfo = NULL;
            }
        } else {
            pen_handler.pens_attached += 1;
        }

        pen->header.flags &= ~SDL_PEN_FLAG_STALE;
    }
    pen_sort();
    /* We could test for changes in the above and send a hotplugging event here */
    SDL_UNLOCK_PENS();
}

static void pen_relative_coordinates(SDL_Window *window, SDL_bool window_relative, float *x, float *y)
{
    int win_x, win_y;

    if (window_relative) {
        return;
    }
    SDL_GetWindowPosition(window, &win_x, &win_y);
    *x -= win_x;
    *y -= win_y;
}

/* Initialises timestamp, windowID, which, pen_state, x, y, and axes */
static void event_setup(const SDL_Pen *pen, const SDL_Window *window, Uint64 timestamp, const SDL_PenStatusInfo *status, SDL_Event *event)
{
    Uint16 last_buttons = pen->last.buttons;

    if (timestamp == 0) {
        /* Generate timestamp ourselves, if needed, so that we report the same timestamp
           for the pen event and for any emulated mouse event */
        timestamp = SDL_GetTicksNS();
    }

    /* This code assumes that all of the SDL_Pen*Event structures have the same layout for these fields.
     * This is checked by testautomation_pen.c, pen_memoryLayout(). */
    event->pmotion.timestamp = timestamp;
    event->pmotion.windowID = window ? window->id : 0;
    event->pmotion.which = pen->header.id;
    event->pmotion.pen_state = last_buttons | PEN_GET_PUBLIC_STATUS_MASK(pen);
    event->pmotion.x = status->x;
    event->pmotion.y = status->y;
    SDL_memcpy(event->pmotion.axes, status->axes, SDL_PEN_NUM_AXES * sizeof(float));
}


int SDL_SendPenMotion(Uint64 timestamp,
                      SDL_PenID instance_id,
                      SDL_bool window_relative,
                      const SDL_PenStatusInfo *status)
{
    int i;
    SDL_Pen *pen = SDL_GetPenPtr(instance_id);
    SDL_Event event;
    SDL_bool posted = SDL_FALSE;
    float x = status->x;
    float y = status->y;
    float last_x = pen->last.x;
    float last_y = pen->last.y;
    /* Suppress mouse updates for axis changes or sub-pixel movement: */
    SDL_bool send_mouse_update;
    SDL_bool axes_changed = SDL_FALSE;
    SDL_Window *window;

    if (!pen) {
        return SDL_FALSE;
    }
    window = pen->header.window;
    if (!window) {
        return SDL_FALSE;
    }

    pen_relative_coordinates(window, window_relative, &x, &y);

    /* Check if the event actually modifies any cached axis or location, update as neeed */
    if (x != last_x || y != last_y) {
        axes_changed = SDL_TRUE;
        pen->last.x = status->x;
        pen->last.y = status->y;
    }
    for (i = 0; i < SDL_PEN_NUM_AXES; ++i) {
        if ((pen->header.flags & SDL_PEN_AXIS_CAPABILITY(i)) && (status->axes[i] != pen->last.axes[i])) {
            axes_changed = SDL_TRUE;
            pen->last.axes[i] = status->axes[i];
        }
    }
    if (!axes_changed) {
        /* No-op event */
        return SDL_FALSE;
    }

    send_mouse_update = (x != last_x) || (y != last_y);

    if (!(SDL_MousePositionInWindow(window, x, y))) {
        return SDL_FALSE;
    }

    if (SDL_EventEnabled(SDL_EVENT_PEN_MOTION)) {
        event_setup(pen, window, timestamp, status, &event);
        event.pmotion.type = SDL_EVENT_PEN_MOTION;

        posted = SDL_PushEvent(&event) > 0;

        if (!posted) {
            return SDL_FALSE;
        }
    }

    if (send_mouse_update) {
        switch (pen_mouse_emulation_mode) {
        case PEN_MOUSE_EMULATE:
            return (SDL_SendMouseMotion(0, window, SDL_PEN_MOUSEID, SDL_FALSE, x, y)) || posted;

        case PEN_MOUSE_STATELESS:
            /* Report mouse event but don't update mouse state */
            if (SDL_EventEnabled(SDL_EVENT_MOUSE_MOTION)) {
                event.motion.windowID = window->id;
                event.motion.timestamp = timestamp;
                event.motion.which = SDL_PEN_MOUSEID;
                event.motion.type = SDL_EVENT_MOUSE_MOTION;
                event.motion.state = pen->last.buttons | PEN_GET_PUBLIC_STATUS_MASK(pen);
                event.motion.x = x;
                event.motion.y = y;
                event.motion.xrel = last_x - x;
                event.motion.yrel = last_y - y;
                return (SDL_PushEvent(&event) > 0) || posted;
            }
            break;

        default:
            break;
        }
    }
    return posted;
}

int SDL_SendPenTipEvent(Uint64 timestamp, SDL_PenID instance_id, Uint8 state)
{
    SDL_Pen *pen = SDL_GetPenPtr(instance_id);
    SDL_Event event;
    SDL_bool posted = SDL_FALSE;
    SDL_PenStatusInfo *last = &pen->last;
    Uint8 mouse_button = SDL_BUTTON_LEFT;
    SDL_Window *window;

    if (!pen) {
        return SDL_FALSE;
    }
    window = pen->header.window;

    if ((state == SDL_PRESSED) && !(window && SDL_MousePositionInWindow(window, last->x, last->y))) {
        return SDL_FALSE;
    }

    if (state == SDL_PRESSED) {
        event.pbutton.type = SDL_EVENT_PEN_DOWN;
        pen->header.flags |= SDL_PEN_DOWN_MASK;
    } else {
        event.pbutton.type = SDL_EVENT_PEN_UP;
        pen->header.flags &= ~SDL_PEN_DOWN_MASK;
    }

    if (SDL_EventEnabled(event.ptip.type)) {
        event_setup(pen, window, timestamp, &pen->last, &event);

        /* Used as eraser?  Report eraser event, otherwise ink event */
        event.ptip.tip = (pen->header.flags & SDL_PEN_ERASER_MASK) ? SDL_PEN_TIP_ERASER : SDL_PEN_TIP_INK;
        event.ptip.state = state == SDL_PRESSED ? SDL_PRESSED : SDL_RELEASED;

        posted = SDL_PushEvent(&event) > 0;

        if (!posted) {
            return SDL_FALSE;
        }
    }

    /* Mouse emulation */
    if (pen_delay_mouse_button_mode) {
        /* Send button events when pen touches / leaves surface */
        mouse_button = pen->last_mouse_button;
        if (mouse_button == 0) {
            mouse_button = SDL_BUTTON_LEFT; /* No current button? Instead report left mouse button */
        }
        /* A button may get released while drawing, but SDL_SendPenButton doesn't reset last_mouse_button
           while touching the surface, so release and reset last_mouse_button on pen tip release */
        if (pen->last.buttons == 0 && state == SDL_RELEASED) {
            pen->last_mouse_button = 0;
        }
    }

    switch (pen_mouse_emulation_mode) {
    case PEN_MOUSE_EMULATE:
        return (SDL_SendMouseButton(timestamp, window, SDL_PEN_MOUSEID, state, mouse_button)) || posted;

    case PEN_MOUSE_STATELESS:
        /* Report mouse event without updating mouse state */
        event.button.type = state == SDL_PRESSED ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        if (SDL_EventEnabled(event.button.type)) {
            event.button.windowID = window ? window->id : 0;
            event.button.timestamp = timestamp;
            event.button.which = SDL_PEN_MOUSEID;

            event.button.state = state;
            event.button.button = mouse_button;
            event.button.clicks = 1;
            event.button.x = last->x;
            event.button.y = last->y;
            return (SDL_PushEvent(&event) > 0) || posted;
        }
        break;

    default:
        break;
    }
    return posted;
}

int SDL_SendPenButton(Uint64 timestamp,
                      SDL_PenID instance_id,
                      Uint8 state, Uint8 button)
{
    SDL_Pen *pen = SDL_GetPenPtr(instance_id);
    SDL_Event event;
    SDL_bool posted = SDL_FALSE;
    SDL_PenStatusInfo *last = &pen->last;
    Uint8 mouse_button = button + 1; /* For mouse emulation, PEN_DOWN counts as button 1, so the first actual button is mouse button 2 */
    SDL_Window *window;

    if (!pen) {
        return SDL_FALSE;
    }
    window = pen->header.window;

    if ((state == SDL_PRESSED) && !(window && SDL_MousePositionInWindow(window, last->x, last->y))) {
        return SDL_FALSE;
    }

    if (state == SDL_PRESSED) {
        event.pbutton.type = SDL_EVENT_PEN_BUTTON_DOWN;
        pen->last.buttons |= (1 << (button - 1));
    } else {
        event.pbutton.type = SDL_EVENT_PEN_BUTTON_UP;
        pen->last.buttons &= ~(1 << (button - 1));
    }

    if (SDL_EventEnabled(event.pbutton.type)) {
        event_setup(pen, window, timestamp, &pen->last, &event);

        event.pbutton.button = button;
        event.pbutton.state = state == SDL_PRESSED ? SDL_PRESSED : SDL_RELEASED;

        posted = SDL_PushEvent(&event) > 0;

        if (!posted) {
            return SDL_FALSE;
        }
    }

    /* Mouse emulation */
    if (pen_delay_mouse_button_mode) {
        /* Can only change active mouse button while not touching the surface */
        if (!(pen->header.flags & SDL_PEN_DOWN_MASK)) {
            if (state == SDL_RELEASED) {
                pen->last_mouse_button = 0;
            } else {
                pen->last_mouse_button = mouse_button;
            }
        }
        /* Defer emulation event */
        return SDL_TRUE;
    }

    switch (pen_mouse_emulation_mode) {
    case PEN_MOUSE_EMULATE:
        return (SDL_SendMouseButton(timestamp, window, SDL_PEN_MOUSEID, state, mouse_button)) || posted;

    case PEN_MOUSE_STATELESS:
        /* Report mouse event without updating mouse state */
        event.button.type = state == SDL_PRESSED ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        if (SDL_EventEnabled(event.button.type)) {
            event.button.windowID = window ? window->id : 0;
            event.button.timestamp = timestamp;
            event.button.which = SDL_PEN_MOUSEID;

            event.button.state = state;
            event.button.button = mouse_button;
            event.button.clicks = 1;
            event.button.x = last->x;
            event.button.y = last->y;
            return (SDL_PushEvent(&event) > 0) || posted;
        }
        break;

    default:
        break;
    }
    return posted;
}

int SDL_SendPenWindowEvent(Uint64 timestamp, SDL_PenID instance_id, SDL_Window *window)
{
    SDL_EventType event_id = window ? SDL_EVENT_WINDOW_PEN_ENTER : SDL_EVENT_WINDOW_PEN_LEAVE;
    SDL_Event event = { 0 };
    SDL_Pen *pen = SDL_GetPenPtr(instance_id);
    SDL_bool posted;

    if (!pen) {
        return SDL_FALSE;
    }

    if (pen->header.window == window) { /* (TRIVIAL-EVENT) Nothing new to report */
        return SDL_FALSE;
    }

    if (timestamp == 0) {
        /* Generate timestamp ourselves, if needed, so that we report the same timestamp
           for the pen event and for any emulated mouse event */
        timestamp = SDL_GetTicksNS();
    }

    event.window.type = event_id;
    /* If window == NULL and not (TRIVIAL-EVENT), then pen->header.window != NULL */
    event.window.timestamp = timestamp;
    event.window.windowID = window ? window->id : pen->header.window->id;
    event.window.data1 = instance_id;
    posted = (SDL_PushEvent(&event) > 0);

    /* Update after assembling event */
    pen->header.window = window;

    switch (pen_mouse_emulation_mode) {
    case PEN_MOUSE_EMULATE:
        SDL_SetMouseFocus(event_id == SDL_EVENT_WINDOW_PEN_ENTER ? window : NULL);
        break;

    case PEN_MOUSE_STATELESS:
        /* Send event without going through mouse API */
        if (event_id == SDL_EVENT_WINDOW_PEN_ENTER) {
            event.window.type = SDL_EVENT_WINDOW_MOUSE_ENTER;
        } else {
            event.window.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
        }
        posted = posted || (SDL_PushEvent(&event) > 0);
        break;

    default:
        break;
    }

    return posted;
}

static void SDLCALL SDL_PenUpdateHint(void *userdata, const char *name, const char *oldvalue, const char *newvalue)
{
    int *var = (int *)userdata;
    if (newvalue == NULL) {
        return;
    }

    if (0 == SDL_strcmp("2", newvalue)) {
        *var = 2;
    } else if (0 == SDL_strcmp("1", newvalue)) {
        *var = 1;
    } else if (0 == SDL_strcmp("0", newvalue)) {
        *var = 0;
    } else {
        SDL_Log("Unexpected value for pen hint: '%s'", newvalue);
    }
}

void SDL_PenInit(void)
{
#if (SDL_PEN_DEBUG_NOID | SDL_PEN_DEBUG_NONWACOM | SDL_PEN_DEBUG_UNKNOWN_WACOM)
    printf("[pen] Debugging enabled: noid=%d  nonwacom=%d  unknown-wacom=%d\n",
           SDL_PEN_DEBUG_NOID, SDL_PEN_DEBUG_NONWACOM, SDL_PEN_DEBUG_UNKNOWN_WACOM);
#endif
    SDL_AddHintCallback(SDL_HINT_PEN_NOT_MOUSE,
                        SDL_PenUpdateHint, &pen_mouse_emulation_mode);

    SDL_AddHintCallback(SDL_HINT_PEN_DELAY_MOUSE_BUTTON,
                        SDL_PenUpdateHint, &pen_delay_mouse_button_mode);
#ifndef SDL_THREADS_DISABLED
    SDL_pen_access_lock = SDL_CreateMutex();
#endif
}

void SDL_PenQuit(void)
{
    unsigned int i;

    SDL_DelHintCallback(SDL_HINT_PEN_NOT_MOUSE,
                        SDL_PenUpdateHint, &pen_mouse_emulation_mode);

    SDL_DelHintCallback(SDL_HINT_PEN_DELAY_MOUSE_BUTTON,
                        SDL_PenUpdateHint, &pen_delay_mouse_button_mode);
#ifndef SDL_THREADS_DISABLED
    SDL_DestroyMutex(SDL_pen_access_lock);
    SDL_pen_access_lock = NULL;
#endif

    if (pen_handler.pens) {
        for (i = 0; i < pen_handler.pens_known; ++i) {
            SDL_free(pen_handler.pens[i].name);
        }
        SDL_free(pen_handler.pens);
        /* Reset static pen information */
        SDL_memset(&pen_handler, 0, sizeof(pen_handler));
    }
}

SDL_bool SDL_PenPerformHitTest(void)
{
    return pen_mouse_emulation_mode == PEN_MOUSE_EMULATE;
}

/* Vendor-specific bits */

/* Default pen names */
#define PEN_NAME_AES      0
#define PEN_NAME_ART      1
#define PEN_NAME_AIRBRUSH 2
#define PEN_NAME_GENERAL  3
#define PEN_NAME_GRIP     4
#define PEN_NAME_INKING   5
#define PEN_NAME_PRO      6
#define PEN_NAME_PRO2     7
#define PEN_NAME_PRO3     8
#define PEN_NAME_PRO3D    9
#define PEN_NAME_PRO_SLIM 10
#define PEN_NAME_STROKE   11

#define PEN_NAME_LAST PEN_NAME_STROKE
#define PEN_NUM_NAMES (PEN_NAME_LAST + 1)

static const char *default_pen_names[] = {
    /* PEN_NAME_AES */
    "AES Pen",
    /* PEN_NAME_ART */
    "Art Pen",
    /* PEN_NAME_AIRBRUSH */
    "Airbrush Pen",
    /* PEN_NAME_GENERAL */
    "Pen",
    /* PEN_NAME_GRIP */
    "Grip Pen",
    /* PEN_NAME_INKING */
    "Inking Pen",
    /* PEN_NAME_PRO */
    "Pro Pen",
    /* PEN_NAME_PRO2 */
    "Pro Pen 2",
    /* PEN_NAME_PRO3 */
    "Pro Pen 3",
    /* PEN_NAME_PRO3D */
    "Pro Pen 3D",
    /* PEN_NAME_PRO_SLIM */
    "Pro Pen Slim",
    /* PEN_NAME_STROKE */
    "Stroke Pen"
};
SDL_COMPILE_TIME_ASSERT(default_pen_names, SDL_arraysize(default_pen_names) == PEN_NUM_NAMES);

#define PEN_SPEC_TYPE_SHIFT    0
#define PEN_SPEC_TYPE_MASK     0x0000000fu
#define PEN_SPEC_BUTTONS_SHIFT 4
#define PEN_SPEC_BUTTONS_MASK  0x000000f0u
#define PEN_SPEC_NAME_SHIFT    8
#define PEN_SPEC_NAME_MASK     0x00000f00u
#define PEN_SPEC_AXES_SHIFT    0
#define PEN_SPEC_AXES_MASK     0xffff0000u

#define PEN_WACOM_ID_INVALID 0xffffffffu /* Generic "invalid ID" marker */

#define PEN_SPEC(name, buttons, type, axes) (0 | (PEN_SPEC_NAME_MASK & ((name) << PEN_SPEC_NAME_SHIFT)) | (PEN_SPEC_BUTTONS_MASK & ((buttons) << PEN_SPEC_BUTTONS_SHIFT)) | (PEN_SPEC_TYPE_MASK & ((type) << PEN_SPEC_TYPE_SHIFT)) | (PEN_SPEC_AXES_MASK & ((axes) << PEN_SPEC_AXES_SHIFT)))

/* Returns a suitable pen name string from default_pen_names on success, otherwise NULL. */
static const char *pen_wacom_identify_tool(Uint32 requested_wacom_id, int *num_buttons, int *tool_type, int *axes)
{
    int i;

    /* List of known Wacom pens, extracted from libwacom.stylus and wacom_wac.c in the Linux kernel.
       Could be complemented by dlopen()ing libwacom, in the future (if new pens get added).  */
    struct
    {
        /* Compress properties to 8 bytes per device in order to keep memory cost well below 1k.
           Could be compressed further with more complex code.  */
        Uint32 wacom_id; /* Must be != PEN_WACOM_ID_INVALID */
        Uint32 properties;
    } tools[] = {
        {  0x0001, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0011, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0019, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0021, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0031, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0039, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0049, PEN_SPEC(PEN_NAME_GENERAL,  1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0071, PEN_SPEC(PEN_NAME_GENERAL,  1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0200, PEN_SPEC(PEN_NAME_PRO3,     3, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0221, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0231, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0271, PEN_SPEC(PEN_NAME_GENERAL,  1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0421, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0431, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0621, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0631, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK) },
        {  0x0801, PEN_SPEC(PEN_NAME_INKING,   0, SDL_PEN_TYPE_PENCIL,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0802, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0804, PEN_SPEC(PEN_NAME_ART,      2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_ROTATION_MASK) },
        {  0x080a, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x080c, PEN_SPEC(PEN_NAME_ART,      2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0812, PEN_SPEC(PEN_NAME_INKING,   0, SDL_PEN_TYPE_PENCIL,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0813, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x081b, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0822, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0823, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x082a, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x082b, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0832, PEN_SPEC(PEN_NAME_STROKE,   0, SDL_PEN_TYPE_BRUSH,    SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0842, PEN_SPEC(PEN_NAME_PRO2,     2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x084a, PEN_SPEC(PEN_NAME_PRO2,     2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0852, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x085a, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0862, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0885, PEN_SPEC(PEN_NAME_ART,      0, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_ROTATION_MASK) },
        {  0x08e2, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0902, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_AIRBRUSH, SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_SLIDER_MASK) },
        {  0x090a, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0912, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_AIRBRUSH, SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_SLIDER_MASK) },
        {  0x0913, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_AIRBRUSH, SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x091a, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x091b, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x0d12, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_AIRBRUSH, SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_SLIDER_MASK) },
        {  0x0d1a, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x8051, PEN_SPEC(PEN_NAME_AES,      0, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x805b, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x806b, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x807b, PEN_SPEC(PEN_NAME_GENERAL,  1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x826b, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x846b, PEN_SPEC(PEN_NAME_AES,      1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK) },
        {  0x2802, PEN_SPEC(PEN_NAME_INKING,   0, SDL_PEN_TYPE_PENCIL,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x4200, PEN_SPEC(PEN_NAME_PRO3,     3, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x4802, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x480a, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        {  0x8842, PEN_SPEC(PEN_NAME_PRO3D,    3, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x10802, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x10804, PEN_SPEC(PEN_NAME_ART,      2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_ROTATION_MASK) },
        { 0x1080a, PEN_SPEC(PEN_NAME_GRIP,     2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x1080c, PEN_SPEC(PEN_NAME_ART,      2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x10842, PEN_SPEC(PEN_NAME_PRO_SLIM, 2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x1084a, PEN_SPEC(PEN_NAME_PRO_SLIM, 2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x10902, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_AIRBRUSH, SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK | SDL_PEN_AXIS_SLIDER_MASK) },
        { 0x1090a, PEN_SPEC(PEN_NAME_AIRBRUSH, 1, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x12802, PEN_SPEC(PEN_NAME_INKING,   0, SDL_PEN_TYPE_PENCIL,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x14802, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x1480a, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x16802, PEN_SPEC(PEN_NAME_PRO,      2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x1680a, PEN_SPEC(PEN_NAME_PRO,      2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x18802, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_PEN,      SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0x1880a, PEN_SPEC(PEN_NAME_GENERAL,  2, SDL_PEN_TYPE_ERASER,   SDL_PEN_AXIS_PRESSURE_MASK | SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK | SDL_PEN_AXIS_DISTANCE_MASK) },
        { 0, 0 }
    };

    /* The list of pens is sorted, so we could do binary search, but this call should be pretty rare. */
    for (i = 0; tools[i].wacom_id; ++i) {
        if (tools[i].wacom_id == requested_wacom_id) {
            Uint32 properties = tools[i].properties;
            int name_index = (properties & PEN_SPEC_NAME_MASK) >> PEN_SPEC_NAME_SHIFT;

            *num_buttons = (properties & PEN_SPEC_BUTTONS_MASK) >> PEN_SPEC_BUTTONS_SHIFT;
            *tool_type = (properties & PEN_SPEC_TYPE_MASK) >> PEN_SPEC_TYPE_SHIFT;
            *axes = (properties & PEN_SPEC_AXES_MASK) >> PEN_SPEC_AXES_SHIFT;

            return default_pen_names[name_index];
        }
    }
    return NULL;
}

void SDL_PenUpdateGUIDForGeneric(SDL_GUID *guid, Uint32 upper, Uint32 lower)
{
    int i;

    for (i = 0; i < 4; ++i) {
        guid->data[8 + i] = (lower >> (i * 8)) & 0xff;
    }

    for (i = 0; i < 4; ++i) {
        guid->data[12 + i] = (upper >> (i * 8)) & 0xff;
    }
}

void SDL_PenUpdateGUIDForType(SDL_GUID *guid, SDL_PenSubtype pentype)
{
    guid->data[7] = pentype;
}

void SDL_PenUpdateGUIDForWacom(SDL_GUID *guid, Uint32 wacom_devicetype_id, Uint32 wacom_serial_id)
{
    int i;

    for (i = 0; i < 4; ++i) {
        guid->data[0 + i] = (wacom_serial_id >> (i * 8)) & 0xff;
    }

    for (i = 0; i < 3; ++i) { /* 24 bit values */
        guid->data[4 + i] = (wacom_devicetype_id >> (i * 8)) & 0xff;
    }
}

int SDL_PenModifyForWacomID(SDL_Pen *pen, Uint32 wacom_devicetype_id, Uint32 *axis_flags)
{
    const char *name = NULL;
    int num_buttons = 0;
    int tool_type = 0;
    int axes = 0;

#if SDL_PEN_DEBUG_UNKNOWN_WACOM
    wacom_devicetype_id = PEN_WACOM_ID_INVALID; /* force detection to fail */
#endif

#if defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_FREEBSD) || defined(SDL_PLATFORM_NETBSD) || defined(SDL_PLATFORM_OPENBSD)
    /* According to Ping Cheng, the curent Wacom for Linux maintainer, device IDs on Linux
       squeeze a "0" nibble after the 3rd (least significant) nibble.
       This may also affect the *BSDs, so they are heuristically included here.
       On those platforms, we first try the "patched" ID: */
    if (0 == (wacom_devicetype_id & 0x0000f000u)) {
        const Uint32 lower_mask = 0xfffu;
        int wacom_devicetype_alt_id = ((wacom_devicetype_id & ~lower_mask) >> 4) | (wacom_devicetype_id & lower_mask);

        name = pen_wacom_identify_tool(wacom_devicetype_alt_id, &num_buttons, &tool_type, &axes);
        if (name) {
            wacom_devicetype_id = wacom_devicetype_alt_id;
        }
    }
#endif
    if (name == NULL) {
        name = pen_wacom_identify_tool(wacom_devicetype_id, &num_buttons, &tool_type, &axes);
    }

    if (!name) {
        *axis_flags = 0;
        return SDL_FALSE;
    }

    *axis_flags = axes;

    /* Override defaults */
    if (pen->info.num_buttons == SDL_PEN_INFO_UNKNOWN) {
        pen->info.num_buttons = (Sint8)SDL_min(num_buttons, SDL_MAX_SINT8);
    }
    if (pen->type == SDL_PEN_TYPE_PEN) {
        pen->type = (SDL_PenSubtype)tool_type;
    }
    if (pen->info.max_tilt == SDL_PEN_INFO_UNKNOWN) {
        /* supposedly: 64 degrees left, 63 right, as reported by the Wacom X11 driver */
        pen->info.max_tilt = 64.0f;
    }
    pen->info.wacom_id = wacom_devicetype_id;
    if (0 == pen->name[0]) {
        SDL_snprintf(pen->name, SDL_PEN_MAX_NAME,
                     "Wacom %s%s", name, (tool_type == SDL_PEN_TYPE_ERASER) ? " Eraser" : "");
    }
    return SDL_TRUE;
}
