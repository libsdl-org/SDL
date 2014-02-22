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
#include "../SDL_internal.h"

/* General event handling code for SDL */

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_syswm.h"
#include "SDL_thread.h"
#include "SDL_events_c.h"
#include "../timer/SDL_timer_c.h"
#if !SDL_JOYSTICK_DISABLED
#include "../joystick/SDL_joystick_c.h"
#endif
#include "../video/SDL_sysvideo.h"

/* An arbitrary limit so we don't have unbounded growth */
#define SDL_MAX_QUEUED_EVENTS   65535

/* Public data -- the event filter */
SDL_EventFilter SDL_EventOK = NULL;
void *SDL_EventOKParam;

typedef struct SDL_EventWatcher {
    SDL_EventFilter callback;
    void *userdata;
    struct SDL_EventWatcher *next;
} SDL_EventWatcher;

static SDL_EventWatcher *SDL_event_watchers = NULL;

typedef struct {
    Uint32 bits[8];
} SDL_DisabledEventBlock;

static SDL_DisabledEventBlock *SDL_disabled_events[256];
static Uint32 SDL_userevents = SDL_USEREVENT;

/* Private data -- event queue */
typedef struct _SDL_EventEntry
{
    SDL_Event event;
    SDL_SysWMmsg msg;
    struct _SDL_EventEntry *prev;
    struct _SDL_EventEntry *next;
} SDL_EventEntry;

typedef struct _SDL_SysWMEntry
{
    SDL_SysWMmsg msg;
    struct _SDL_SysWMEntry *next;
} SDL_SysWMEntry;

static struct
{
    SDL_mutex *lock;
    volatile SDL_bool active;
    volatile int count;
    SDL_EventEntry *head;
    SDL_EventEntry *tail;
    SDL_EventEntry *free;
    SDL_SysWMEntry *wmmsg_used;
    SDL_SysWMEntry *wmmsg_free;
} SDL_EventQ = { NULL, SDL_TRUE };


static SDL_INLINE SDL_bool
SDL_ShouldPollJoystick()
{
#if !SDL_JOYSTICK_DISABLED
    if ((!SDL_disabled_events[SDL_JOYAXISMOTION >> 8] ||
         SDL_JoystickEventState(SDL_QUERY)) &&
        SDL_PrivateJoystickNeedsPolling()) {
        return SDL_TRUE;
    }
#endif
    return SDL_FALSE;
}

/* Public functions */

void
SDL_StopEventLoop(void)
{
    int i;
    SDL_EventEntry *entry;
    SDL_SysWMEntry *wmmsg;

    if (SDL_EventQ.lock) {
        SDL_LockMutex(SDL_EventQ.lock);
    }

    SDL_EventQ.active = SDL_FALSE;

    /* Clean out EventQ */
    for (entry = SDL_EventQ.head; entry; ) {
        SDL_EventEntry *next = entry->next;
        SDL_free(entry);
        entry = next;
    }
    for (entry = SDL_EventQ.free; entry; ) {
        SDL_EventEntry *next = entry->next;
        SDL_free(entry);
        entry = next;
    }
    for (wmmsg = SDL_EventQ.wmmsg_used; wmmsg; ) {
        SDL_SysWMEntry *next = wmmsg->next;
        SDL_free(wmmsg);
        wmmsg = next;
    }
    for (wmmsg = SDL_EventQ.wmmsg_free; wmmsg; ) {
        SDL_SysWMEntry *next = wmmsg->next;
        SDL_free(wmmsg);
        wmmsg = next;
    }
    SDL_EventQ.count = 0;
    SDL_EventQ.head = NULL;
    SDL_EventQ.tail = NULL;
    SDL_EventQ.free = NULL;
    SDL_EventQ.wmmsg_used = NULL;
    SDL_EventQ.wmmsg_free = NULL;

    /* Clear disabled event state */
    for (i = 0; i < SDL_arraysize(SDL_disabled_events); ++i) {
        SDL_free(SDL_disabled_events[i]);
        SDL_disabled_events[i] = NULL;
    }

    while (SDL_event_watchers) {
        SDL_EventWatcher *tmp = SDL_event_watchers;
        SDL_event_watchers = tmp->next;
        SDL_free(tmp);
    }
    SDL_EventOK = NULL;

    if (SDL_EventQ.lock) {
        SDL_UnlockMutex(SDL_EventQ.lock);
        SDL_DestroyMutex(SDL_EventQ.lock);
        SDL_EventQ.lock = NULL;
    }
}

/* This function (and associated calls) may be called more than once */
int
SDL_StartEventLoop(void)
{
    /* We'll leave the event queue alone, since we might have gotten
       some important events at launch (like SDL_DROPFILE)

       FIXME: Does this introduce any other bugs with events at startup?
     */

    /* Create the lock and set ourselves active */
#if !SDL_THREADS_DISABLED
    if (!SDL_EventQ.lock) {
        SDL_EventQ.lock = SDL_CreateMutex();
    }
    if (SDL_EventQ.lock == NULL) {
        return (-1);
    }
#endif /* !SDL_THREADS_DISABLED */

    /* Process most event types */
    SDL_EventState(SDL_TEXTINPUT, SDL_DISABLE);
    SDL_EventState(SDL_TEXTEDITING, SDL_DISABLE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);

    SDL_EventQ.active = SDL_TRUE;

    return (0);
}


/* Add an event to the event queue -- called with the queue locked */
static int
SDL_AddEvent(SDL_Event * event)
{
    SDL_EventEntry *entry;

    if (SDL_EventQ.count >= SDL_MAX_QUEUED_EVENTS) {
        SDL_SetError("Event queue is full (%d events)", SDL_EventQ.count);
        return 0;
    }

    if (SDL_EventQ.free == NULL) {
        entry = (SDL_EventEntry *)SDL_malloc(sizeof(*entry));
        if (!entry) {
            return 0;
        }
    } else {
        entry = SDL_EventQ.free;
        SDL_EventQ.free = entry->next;
    }

    entry->event = *event;
    if (event->type == SDL_SYSWMEVENT) {
        entry->msg = *event->syswm.msg;
        entry->event.syswm.msg = &entry->msg;
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
    ++SDL_EventQ.count;

    return 1;
}

/* Remove an event from the queue -- called with the queue locked */
static void
SDL_CutEvent(SDL_EventEntry *entry)
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

    entry->next = SDL_EventQ.free;
    SDL_EventQ.free = entry;
    SDL_assert(SDL_EventQ.count > 0);
    --SDL_EventQ.count;
}

/* Lock the event queue, take a peep at it, and unlock it */
int
SDL_PeepEvents(SDL_Event * events, int numevents, SDL_eventaction action,
               Uint32 minType, Uint32 maxType)
{
    int i, used;

    /* Don't look after we've quit */
    if (!SDL_EventQ.active) {
        /* We get a few spurious events at shutdown, so don't warn then */
        if (action != SDL_ADDEVENT) {
            SDL_SetError("The event system has been shut down");
        }
        return (-1);
    }
    /* Lock the event queue */
    used = 0;
    if (!SDL_EventQ.lock || SDL_LockMutex(SDL_EventQ.lock) == 0) {
        if (action == SDL_ADDEVENT) {
            for (i = 0; i < numevents; ++i) {
                used += SDL_AddEvent(&events[i]);
            }
        } else {
            SDL_EventEntry *entry, *next;
            SDL_SysWMEntry *wmmsg, *wmmsg_next;
            SDL_Event tmpevent;
            Uint32 type;

            /* If 'events' is NULL, just see if they exist */
            if (events == NULL) {
                action = SDL_PEEKEVENT;
                numevents = 1;
                events = &tmpevent;
            }

            /* Clean out any used wmmsg data
               FIXME: Do we want to retain the data for some period of time?
             */
            for (wmmsg = SDL_EventQ.wmmsg_used; wmmsg; wmmsg = wmmsg_next) {
                wmmsg_next = wmmsg->next;
                wmmsg->next = SDL_EventQ.wmmsg_free;
                SDL_EventQ.wmmsg_free = wmmsg;
            }
            SDL_EventQ.wmmsg_used = NULL;

            for (entry = SDL_EventQ.head; entry && used < numevents; entry = next) {
                next = entry->next;
                type = entry->event.type;
                if (minType <= type && type <= maxType) {
                    events[used] = entry->event;
                    if (entry->event.type == SDL_SYSWMEVENT) {
                        /* We need to copy the wmmsg somewhere safe.
                           For now we'll guarantee it's valid at least until
                           the next call to SDL_PeepEvents()
                         */
                        SDL_SysWMEntry *wmmsg;
                        if (SDL_EventQ.wmmsg_free) {
                            wmmsg = SDL_EventQ.wmmsg_free;
                            SDL_EventQ.wmmsg_free = wmmsg->next;
                        } else {
                            wmmsg = (SDL_SysWMEntry *)SDL_malloc(sizeof(*wmmsg));
                        }
                        wmmsg->msg = *entry->event.syswm.msg;
                        wmmsg->next = SDL_EventQ.wmmsg_used;
                        SDL_EventQ.wmmsg_used = wmmsg;
                        events[used].syswm.msg = &wmmsg->msg;
                    }
                    ++used;

                    if (action == SDL_GETEVENT) {
                        SDL_CutEvent(entry);
                    }
                }
            }
        }
        SDL_UnlockMutex(SDL_EventQ.lock);
    } else {
        return SDL_SetError("Couldn't lock event queue");
    }
    return (used);
}

SDL_bool
SDL_HasEvent(Uint32 type)
{
    return (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, type, type) > 0);
}

SDL_bool
SDL_HasEvents(Uint32 minType, Uint32 maxType)
{
    return (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, minType, maxType) > 0);
}

void
SDL_FlushEvent(Uint32 type)
{
    SDL_FlushEvents(type, type);
}

void
SDL_FlushEvents(Uint32 minType, Uint32 maxType)
{
    /* Don't look after we've quit */
    if (!SDL_EventQ.active) {
        return;
    }

    /* Make sure the events are current */
#if 0
    /* Actually, we can't do this since we might be flushing while processing
       a resize event, and calling this might trigger further resize events.
    */
    SDL_PumpEvents();
#endif

    /* Lock the event queue */
    if (SDL_LockMutex(SDL_EventQ.lock) == 0) {
        SDL_EventEntry *entry, *next;
        Uint32 type;
        for (entry = SDL_EventQ.head; entry; entry = next) {
            next = entry->next;
            type = entry->event.type;
            if (minType <= type && type <= maxType) {
                SDL_CutEvent(entry);
            }
        }
        SDL_UnlockMutex(SDL_EventQ.lock);
    }
}

/* Run the system dependent event loops */
void
SDL_PumpEvents(void)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    /* Get events from the video subsystem */
    if (_this) {
        _this->PumpEvents(_this);
    }
#if !SDL_JOYSTICK_DISABLED
    /* Check for joystick state change */
    if (SDL_ShouldPollJoystick()) {
        SDL_JoystickUpdate();
    }
#endif
}

/* Public functions */

int
SDL_PollEvent(SDL_Event * event)
{
    return SDL_WaitEventTimeout(event, 0);
}

int
SDL_WaitEvent(SDL_Event * event)
{
    return SDL_WaitEventTimeout(event, -1);
}

int
SDL_WaitEventTimeout(SDL_Event * event, int timeout)
{
    Uint32 expiration = 0;

    if (timeout > 0)
        expiration = SDL_GetTicks() + timeout;

    for (;;) {
        SDL_PumpEvents();
        switch (SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        case -1:
            return 0;
        case 1:
            return 1;
        case 0:
            if (timeout == 0) {
                /* Polling and no events, just return */
                return 0;
            }
            if (timeout > 0 && SDL_TICKS_PASSED(SDL_GetTicks(), expiration)) {
                /* Timeout expired and no events */
                return 0;
            }
            SDL_Delay(10);
            break;
        }
    }
}

int
SDL_PushEvent(SDL_Event * event)
{
    SDL_EventWatcher *curr;

    event->common.timestamp = SDL_GetTicks();

    if (SDL_EventOK && !SDL_EventOK(SDL_EventOKParam, event)) {
        return 0;
    }

    for (curr = SDL_event_watchers; curr; curr = curr->next) {
        curr->callback(curr->userdata, event);
    }

    if (SDL_PeepEvents(event, 1, SDL_ADDEVENT, 0, 0) <= 0) {
        return -1;
    }

    SDL_GestureProcessEvent(event);

    return 1;
}

void
SDL_SetEventFilter(SDL_EventFilter filter, void *userdata)
{
    /* Set filter and discard pending events */
    SDL_EventOK = NULL;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_EventOKParam = userdata;
    SDL_EventOK = filter;
}

SDL_bool
SDL_GetEventFilter(SDL_EventFilter * filter, void **userdata)
{
    if (filter) {
        *filter = SDL_EventOK;
    }
    if (userdata) {
        *userdata = SDL_EventOKParam;
    }
    return SDL_EventOK ? SDL_TRUE : SDL_FALSE;
}

/* FIXME: This is not thread-safe yet */
void
SDL_AddEventWatch(SDL_EventFilter filter, void *userdata)
{
    SDL_EventWatcher *watcher, *tail;

    watcher = (SDL_EventWatcher *)SDL_malloc(sizeof(*watcher));
    if (!watcher) {
        /* Uh oh... */
        return;
    }

    /* create the watcher */
    watcher->callback = filter;
    watcher->userdata = userdata;
    watcher->next = NULL;

    /* add the watcher to the end of the list */
    if (SDL_event_watchers) {
        for (tail = SDL_event_watchers; tail->next; tail = tail->next) {
            continue;
        }
        tail->next = watcher;
    } else {
        SDL_event_watchers = watcher;
    }
}

/* FIXME: This is not thread-safe yet */
void
SDL_DelEventWatch(SDL_EventFilter filter, void *userdata)
{
    SDL_EventWatcher *prev = NULL;
    SDL_EventWatcher *curr;

    for (curr = SDL_event_watchers; curr; prev = curr, curr = curr->next) {
        if (curr->callback == filter && curr->userdata == userdata) {
            if (prev) {
                prev->next = curr->next;
            } else {
                SDL_event_watchers = curr->next;
            }
            SDL_free(curr);
            break;
        }
    }
}

void
SDL_FilterEvents(SDL_EventFilter filter, void *userdata)
{
    if (SDL_LockMutex(SDL_EventQ.lock) == 0) {
        SDL_EventEntry *entry, *next;
        for (entry = SDL_EventQ.head; entry; entry = next) {
            next = entry->next;
            if (!filter(userdata, &entry->event)) {
                SDL_CutEvent(entry);
            }
        }
        SDL_UnlockMutex(SDL_EventQ.lock);
    }
}

Uint8
SDL_EventState(Uint32 type, int state)
{
    Uint8 current_state;
    Uint8 hi = ((type >> 8) & 0xff);
    Uint8 lo = (type & 0xff);

    if (SDL_disabled_events[hi] &&
        (SDL_disabled_events[hi]->bits[lo/32] & (1 << (lo&31)))) {
        current_state = SDL_DISABLE;
    } else {
        current_state = SDL_ENABLE;
    }

    if (state != current_state)
    {
        switch (state) {
        case SDL_DISABLE:
            /* Disable this event type and discard pending events */
            if (!SDL_disabled_events[hi]) {
                SDL_disabled_events[hi] = (SDL_DisabledEventBlock*) SDL_calloc(1, sizeof(SDL_DisabledEventBlock));
                if (!SDL_disabled_events[hi]) {
                    /* Out of memory, nothing we can do... */
                    break;
                }
            }
            SDL_disabled_events[hi]->bits[lo/32] |= (1 << (lo&31));
            SDL_FlushEvent(type);
            break;
        case SDL_ENABLE:
            SDL_disabled_events[hi]->bits[lo/32] &= ~(1 << (lo&31));
            break;
        default:
            /* Querying state... */
            break;
        }
    }

    return current_state;
}

Uint32
SDL_RegisterEvents(int numevents)
{
    Uint32 event_base;

    if ((numevents > 0) && (SDL_userevents+numevents <= SDL_LASTEVENT)) {
        event_base = SDL_userevents;
        SDL_userevents += numevents;
    } else {
        event_base = (Uint32)-1;
    }
    return event_base;
}

int
SDL_SendAppEvent(SDL_EventType eventType)
{
    int posted;

    posted = 0;
    if (SDL_GetEventState(eventType) == SDL_ENABLE) {
        SDL_Event event;
        event.type = eventType;
        posted = (SDL_PushEvent(&event) > 0);
    }
    return (posted);
}

int
SDL_SendSysWMEvent(SDL_SysWMmsg * message)
{
    int posted;

    posted = 0;
    if (SDL_GetEventState(SDL_SYSWMEVENT) == SDL_ENABLE) {
        SDL_Event event;
        SDL_memset(&event, 0, sizeof(event));
        event.type = SDL_SYSWMEVENT;
        event.syswm.msg = message;
        posted = (SDL_PushEvent(&event) > 0);
    }
    /* Update internal event state */
    return (posted);
}

/* vi: set ts=4 sw=4 expandtab: */
