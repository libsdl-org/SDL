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

#include "SDL_timer_c.h"
#include "../thread/SDL_systhread.h"

/* #define DEBUG_TIMERS */

#if !defined(SDL_PLATFORM_EMSCRIPTEN) || !defined(SDL_THREADS_DISABLED)

typedef struct SDL_Timer
{
    SDL_TimerID timerID;
    SDL_TimerCallback callback;
    void *param;
    Uint64 interval;
    Uint64 scheduled;
    SDL_AtomicInt canceled;
    struct SDL_Timer *next;
} SDL_Timer;

typedef struct SDL_TimerMap
{
    SDL_TimerID timerID;
    SDL_Timer *timer;
    struct SDL_TimerMap *next;
} SDL_TimerMap;

/* The timers are kept in a sorted list */
typedef struct
{
    /* Data used by the main thread */
    SDL_Thread *thread;
    SDL_TimerMap *timermap;
    SDL_Mutex *timermap_lock;

    /* Padding to separate cache lines between threads */
    char cache_pad[SDL_CACHELINE_SIZE];

    /* Data used to communicate with the timer thread */
    SDL_SpinLock lock;
    SDL_Semaphore *sem;
    SDL_Timer *pending;
    SDL_Timer *freelist;
    SDL_AtomicInt active;

    /* List of timers - this is only touched by the timer thread */
    SDL_Timer *timers;
} SDL_TimerData;

static SDL_TimerData SDL_timer_data;

/* The idea here is that any thread might add a timer, but a single
 * thread manages the active timer queue, sorted by scheduling time.
 *
 * Timers are removed by simply setting a canceled flag
 */

static void SDL_AddTimerInternal(SDL_TimerData *data, SDL_Timer *timer)
{
    SDL_Timer *prev, *curr;

    prev = NULL;
    for (curr = data->timers; curr; prev = curr, curr = curr->next) {
        if (curr->scheduled > timer->scheduled) {
            break;
        }
    }

    /* Insert the timer here! */
    if (prev) {
        prev->next = timer;
    } else {
        data->timers = timer;
    }
    timer->next = curr;
}

static int SDLCALL SDL_TimerThread(void *_data)
{
    SDL_TimerData *data = (SDL_TimerData *)_data;
    SDL_Timer *pending;
    SDL_Timer *current;
    SDL_Timer *freelist_head = NULL;
    SDL_Timer *freelist_tail = NULL;
    Uint64 tick, now, interval, delay;

    /* Threaded timer loop:
     *  1. Queue timers added by other threads
     *  2. Handle any timers that should dispatch this cycle
     *  3. Wait until next dispatch time or new timer arrives
     */
    for (;;) {
        /* Pending and freelist maintenance */
        SDL_LockSpinlock(&data->lock);
        {
            /* Get any timers ready to be queued */
            pending = data->pending;
            data->pending = NULL;

            /* Make any unused timer structures available */
            if (freelist_head) {
                freelist_tail->next = data->freelist;
                data->freelist = freelist_head;
            }
        }
        SDL_UnlockSpinlock(&data->lock);

        /* Sort the pending timers into our list */
        while (pending) {
            current = pending;
            pending = pending->next;
            SDL_AddTimerInternal(data, current);
        }
        freelist_head = NULL;
        freelist_tail = NULL;

        /* Check to see if we're still running, after maintenance */
        if (!SDL_AtomicGet(&data->active)) {
            break;
        }

        /* Initial delay if there are no timers */
        delay = (Uint64)-1;

        tick = SDL_GetTicksNS();

        /* Process all the pending timers for this tick */
        while (data->timers) {
            current = data->timers;

            if (tick < current->scheduled) {
                /* Scheduled for the future, wait a bit */
                delay = (current->scheduled - tick);
                break;
            }

            /* We're going to do something with this timer */
            data->timers = current->next;

            if (SDL_AtomicGet(&current->canceled)) {
                interval = 0;
            } else {
                /* FIXME: We could potentially support sub-millisecond timers now */
                interval = SDL_MS_TO_NS(current->callback((Uint32)SDL_NS_TO_MS(current->interval), current->param));
            }

            if (interval > 0) {
                /* Reschedule this timer */
                current->interval = interval;
                current->scheduled = tick + interval;
                SDL_AddTimerInternal(data, current);
            } else {
                if (!freelist_head) {
                    freelist_head = current;
                }
                if (freelist_tail) {
                    freelist_tail->next = current;
                }
                freelist_tail = current;

                SDL_AtomicSet(&current->canceled, 1);
            }
        }

        /* Adjust the delay based on processing time */
        now = SDL_GetTicksNS();
        interval = (now - tick);
        if (interval > delay) {
            delay = 0;
        } else {
            delay -= interval;
        }

        /* Note that each time a timer is added, this will return
           immediately, but we process the timers added all at once.
           That's okay, it just means we run through the loop a few
           extra times.
         */
        SDL_WaitSemaphoreTimeoutNS(data->sem, delay);
    }
    return 0;
}

int SDL_InitTimers(void)
{
    SDL_TimerData *data = &SDL_timer_data;

    if (!SDL_AtomicGet(&data->active)) {
        const char *name = "SDLTimer";
        data->timermap_lock = SDL_CreateMutex();
        if (!data->timermap_lock) {
            return -1;
        }

        data->sem = SDL_CreateSemaphore(0);
        if (!data->sem) {
            SDL_DestroyMutex(data->timermap_lock);
            return -1;
        }

        SDL_AtomicSet(&data->active, 1);

        /* Timer threads use a callback into the app, so we can't set a limited stack size here. */
        data->thread = SDL_CreateThreadInternal(SDL_TimerThread, name, 0, data);
        if (!data->thread) {
            SDL_QuitTimers();
            return -1;
        }
    }
    return 0;
}

void SDL_QuitTimers(void)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_Timer *timer;
    SDL_TimerMap *entry;

    if (SDL_AtomicCompareAndSwap(&data->active, 1, 0)) { /* active? Move to inactive. */
        /* Shutdown the timer thread */
        if (data->thread) {
            SDL_PostSemaphore(data->sem);
            SDL_WaitThread(data->thread, NULL);
            data->thread = NULL;
        }

        SDL_DestroySemaphore(data->sem);
        data->sem = NULL;

        /* Clean up the timer entries */
        while (data->timers) {
            timer = data->timers;
            data->timers = timer->next;
            SDL_free(timer);
        }
        while (data->freelist) {
            timer = data->freelist;
            data->freelist = timer->next;
            SDL_free(timer);
        }
        while (data->timermap) {
            entry = data->timermap;
            data->timermap = entry->next;
            SDL_free(entry);
        }

        SDL_DestroyMutex(data->timermap_lock);
        data->timermap_lock = NULL;
    }
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void *param)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_Timer *timer;
    SDL_TimerMap *entry;

    SDL_LockSpinlock(&data->lock);
    if (!SDL_AtomicGet(&data->active)) {
        if (SDL_InitTimers() < 0) {
            SDL_UnlockSpinlock(&data->lock);
            return 0;
        }
    }

    timer = data->freelist;
    if (timer) {
        data->freelist = timer->next;
    }
    SDL_UnlockSpinlock(&data->lock);

    if (timer) {
        SDL_RemoveTimer(timer->timerID);
    } else {
        timer = (SDL_Timer *)SDL_malloc(sizeof(*timer));
        if (!timer) {
            return 0;
        }
    }
    timer->timerID = SDL_GetNextObjectID();
    timer->callback = callback;
    timer->param = param;
    timer->interval = SDL_MS_TO_NS(interval);
    timer->scheduled = SDL_GetTicksNS() + timer->interval;
    SDL_AtomicSet(&timer->canceled, 0);

    entry = (SDL_TimerMap *)SDL_malloc(sizeof(*entry));
    if (!entry) {
        SDL_free(timer);
        return 0;
    }
    entry->timer = timer;
    entry->timerID = timer->timerID;

    SDL_LockMutex(data->timermap_lock);
    entry->next = data->timermap;
    data->timermap = entry;
    SDL_UnlockMutex(data->timermap_lock);

    /* Add the timer to the pending list for the timer thread */
    SDL_LockSpinlock(&data->lock);
    timer->next = data->pending;
    data->pending = timer;
    SDL_UnlockSpinlock(&data->lock);

    /* Wake up the timer thread if necessary */
    SDL_PostSemaphore(data->sem);

    return entry->timerID;
}

SDL_bool SDL_RemoveTimer(SDL_TimerID id)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *prev, *entry;
    SDL_bool canceled = SDL_FALSE;

    /* Find the timer */
    SDL_LockMutex(data->timermap_lock);
    prev = NULL;
    for (entry = data->timermap; entry; prev = entry, entry = entry->next) {
        if (entry->timerID == id) {
            if (prev) {
                prev->next = entry->next;
            } else {
                data->timermap = entry->next;
            }
            break;
        }
    }
    SDL_UnlockMutex(data->timermap_lock);

    if (entry) {
        if (!SDL_AtomicGet(&entry->timer->canceled)) {
            SDL_AtomicSet(&entry->timer->canceled, 1);
            canceled = SDL_TRUE;
        }
        SDL_free(entry);
    }
    return canceled;
}

#else

#include <emscripten/emscripten.h>
#include <emscripten/eventloop.h>

typedef struct SDL_TimerMap
{
    SDL_TimerID timerID;
    int timeoutID;
    Uint32 interval;
    SDL_TimerCallback callback;
    void *param;
    struct SDL_TimerMap *next;
} SDL_TimerMap;

typedef struct
{
    SDL_TimerMap *timermap;
} SDL_TimerData;

static SDL_TimerData SDL_timer_data;

static void SDL_Emscripten_TimerHelper(void *userdata)
{
    SDL_TimerMap *entry = (SDL_TimerMap *)userdata;
    entry->interval = entry->callback(entry->interval, entry->param);
    if (entry->interval > 0) {
        entry->timeoutID = emscripten_set_timeout(&SDL_Emscripten_TimerHelper,
                                                  entry->interval,
                                                  entry);
    }
}

int SDL_InitTimers(void)
{
    return 0;
}

void SDL_QuitTimers(void)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *entry;

    while (data->timermap) {
        entry = data->timermap;
        data->timermap = entry->next;
        SDL_free(entry);
    }
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void *param)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *entry;

    entry = (SDL_TimerMap *)SDL_malloc(sizeof(*entry));
    if (!entry) {
        return 0;
    }
    entry->timerID = SDL_GetNextObjectID();
    entry->callback = callback;
    entry->param = param;
    entry->interval = interval;

    entry->timeoutID = emscripten_set_timeout(&SDL_Emscripten_TimerHelper,
                                              entry->interval,
                                              entry);

    entry->next = data->timermap;
    data->timermap = entry;

    return entry->timerID;
}

SDL_bool SDL_RemoveTimer(SDL_TimerID id)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *prev, *entry;

    /* Find the timer */
    prev = NULL;
    for (entry = data->timermap; entry; prev = entry, entry = entry->next) {
        if (entry->timerID == id) {
            if (prev) {
                prev->next = entry->next;
            } else {
                data->timermap = entry->next;
            }
            break;
        }
    }

    if (entry) {
        emscripten_clear_timeout(entry->timeoutID);
        SDL_free(entry);

        return SDL_TRUE;
    }
    return SDL_FALSE;
}

#endif /* !defined(SDL_PLATFORM_EMSCRIPTEN) || !SDL_THREADS_DISABLED */

static Uint64 tick_start;
static Uint32 tick_numerator_ns;
static Uint32 tick_denominator_ns;
static Uint32 tick_numerator_ms;
static Uint32 tick_denominator_ms;

#if defined(SDL_TIMER_WINDOWS) && \
    !defined(SDL_PLATFORM_WINRT) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
#include <mmsystem.h>
#define HAVE_TIME_BEGIN_PERIOD
#endif

static void SDL_SetSystemTimerResolutionMS(int period)
{
#ifdef HAVE_TIME_BEGIN_PERIOD
    static int timer_period = 0;

    if (period != timer_period) {
        if (timer_period) {
            timeEndPeriod((UINT)timer_period);
        }

        timer_period = period;

        if (timer_period) {
            timeBeginPeriod((UINT)timer_period);
        }
    }
#endif /* HAVE_TIME_BEGIN_PERIOD */
}

static void SDLCALL SDL_TimerResolutionChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    int period;

    /* Unless the hint says otherwise, let's have good sleep precision */
    if (hint && *hint) {
        period = SDL_atoi(hint);
    } else {
        period = 1;
    }
    if (period || oldValue != hint) {
        SDL_SetSystemTimerResolutionMS(period);
    }
}

static Uint32 CalculateGCD(Uint32 a, Uint32 b)
{
    if (b == 0) {
        return a;
    }
    return CalculateGCD(b, (a % b));
}

void SDL_InitTicks(void)
{
    Uint64 tick_freq;
    Uint32 gcd;

    if (tick_start) {
        return;
    }

    /* If we didn't set a precision, set it high. This affects lots of things
       on Windows besides the SDL timers, like audio callbacks, etc. */
    SDL_AddHintCallback(SDL_HINT_TIMER_RESOLUTION,
                        SDL_TimerResolutionChanged, NULL);

    tick_freq = SDL_GetPerformanceFrequency();
    SDL_assert(tick_freq > 0 && tick_freq <= (Uint64)SDL_MAX_UINT32);

    gcd = CalculateGCD(SDL_NS_PER_SECOND, (Uint32)tick_freq);
    tick_numerator_ns = (SDL_NS_PER_SECOND / gcd);
    tick_denominator_ns = (Uint32)(tick_freq / gcd);

    gcd = CalculateGCD(SDL_MS_PER_SECOND, (Uint32)tick_freq);
    tick_numerator_ms = (SDL_MS_PER_SECOND / gcd);
    tick_denominator_ms = (Uint32)(tick_freq / gcd);

    tick_start = SDL_GetPerformanceCounter();
    if (!tick_start) {
        --tick_start;
    }
}

void SDL_QuitTicks(void)
{
    SDL_DelHintCallback(SDL_HINT_TIMER_RESOLUTION,
                        SDL_TimerResolutionChanged, NULL);

    SDL_SetSystemTimerResolutionMS(0); /* always release our timer resolution request. */

    tick_start = 0;
}

Uint64 SDL_GetTicksNS(void)
{
    Uint64 starting_value, value;

    if (!tick_start) {
        SDL_InitTicks();
    }

    starting_value = (SDL_GetPerformanceCounter() - tick_start);
    value = (starting_value * tick_numerator_ns);
    SDL_assert(value >= starting_value);
    value /= tick_denominator_ns;
    return value;
}

Uint64 SDL_GetTicks(void)
{
    Uint64 starting_value, value;

    if (!tick_start) {
        SDL_InitTicks();
    }

    starting_value = (SDL_GetPerformanceCounter() - tick_start);
    value = (starting_value * tick_numerator_ms);
    SDL_assert(value >= starting_value);
    value /= tick_denominator_ms;
    return value;
}

void SDL_Delay(Uint32 ms)
{
    SDL_DelayNS(SDL_MS_TO_NS(ms));
}
