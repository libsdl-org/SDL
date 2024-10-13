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

// #define DEBUG_TIMERS

typedef enum SDL_TimerCallbackType
{
    SDL_TIMER_CALLBACK_TYPE_MS,
    SDL_TIMER_CALLBACK_TYPE_NS,
    SDL_TIMER_CALLBACK_TYPE_PRECISE
} SDL_TimerCallbackType;

typedef struct SDL_TimerCallbackData
{
    SDL_TimerCallbackType type;
    union
    {
        SDL_TimerCallback callback_ms;
        SDL_NSTimerCallback callback_ns;
        struct
        {
            SDL_NSTimerCallback callback;
            Sint64 accumulated;
        } precise;
    };
} SDL_TimerCallbackData;

#if !defined(SDL_PLATFORM_EMSCRIPTEN) || !defined(SDL_THREADS_DISABLED)

typedef struct SDL_Timer
{
    SDL_TimerID timerID;
    SDL_TimerCallbackData callback_data;
    void *userdata;
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

// The timers are kept in a sorted list
typedef struct
{
    // Data used by the main thread
    SDL_InitState init;
    SDL_Thread *thread;
    SDL_TimerMap *timermap;
    SDL_Mutex *timermap_lock;

    // Padding to separate cache lines between threads
    char cache_pad[SDL_CACHELINE_SIZE];

    // Data used to communicate with the timer thread
    SDL_SpinLock lock;
    SDL_Semaphore *sem;
    SDL_Timer *pending;
    SDL_Timer *freelist;
    SDL_AtomicInt active;

    // List of timers - this is only touched by the timer thread
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

    // Insert the timer here!
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
    Uint64 tick, start, now, interval, delay;

    /* Threaded timer loop:
     *  1. Queue timers added by other threads
     *  2. Handle any timers that should dispatch this cycle
     *  3. Wait until next dispatch time or new timer arrives
     */
    for (;;) {
        // Pending and freelist maintenance
        SDL_LockSpinlock(&data->lock);
        {
            // Get any timers ready to be queued
            pending = data->pending;
            data->pending = NULL;

            // Make any unused timer structures available
            if (freelist_head) {
                freelist_tail->next = data->freelist;
                data->freelist = freelist_head;
            }
        }
        SDL_UnlockSpinlock(&data->lock);

        // Sort the pending timers into our list
        while (pending) {
            current = pending;
            pending = pending->next;
            SDL_AddTimerInternal(data, current);
        }
        freelist_head = NULL;
        freelist_tail = NULL;

        // Check to see if we're still running, after maintenance
        if (!SDL_GetAtomicInt(&data->active)) {
            break;
        }

        // Initial delay if there are no timers
        delay = (Uint64)-1;

        tick = SDL_GetTicksNS();

        // Process all the pending timers for this tick
        while (data->timers) {
            current = data->timers;

            if (tick < current->scheduled) {
                // Scheduled for the future, wait a bit
                delay = (current->scheduled - tick);
                break;
            }

            // We're going to do something with this timer
            data->timers = current->next;

            if (SDL_GetAtomicInt(&current->canceled)) {
                interval = 0;
            } else {
                switch (current->callback_data.type) {
                case SDL_TIMER_CALLBACK_TYPE_MS:
                    interval = SDL_MS_TO_NS(current->callback_data.callback_ms(current->userdata, current->timerID, (Uint32)SDL_NS_TO_MS(current->interval)));
                    break;
                case SDL_TIMER_CALLBACK_TYPE_NS:
                    interval = current->callback_data.callback_ns(current->userdata, current->timerID, current->interval);
                    break;
		case SDL_TIMER_CALLBACK_TYPE_PRECISE:
                    interval = current->callback_data.precise.callback(current->userdata, current->timerID, current->interval);
		    break;
                }
            }

            if (interval > 0) {
                // Reschedule this timer
                current->interval = interval;
                if (current->callback_data.type == SDL_TIMER_CALLBACK_TYPE_PRECISE) {
		    /* We want good pacing accuracy in the common case, but we
		       don't want to skip iterations, so only use the
		       accumulated time if we don't have to skip.
		     */
                    if (
                        (current->callback_data.precise.accumulated >= 0 && current->callback_data.precise.accumulated < interval) ||
                        current->callback_data.precise.accumulated < 0) {
                        current->scheduled = tick + interval - current->callback_data.precise.accumulated;
                    } else {
                        current->scheduled = tick + interval;
                        current->callback_data.precise.accumulated = 0;
                    }
                } else {
                    current->scheduled = tick + interval;
                }
                SDL_AddTimerInternal(data, current);
            } else {
                if (!freelist_head) {
                    freelist_head = current;
                }
                if (freelist_tail) {
                    freelist_tail->next = current;
                }
                freelist_tail = current;

                SDL_SetAtomicInt(&current->canceled, 1);
            }
        }

        // Adjust the delay based on processing time
        start = now = SDL_GetTicksNS();
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
        switch (current->callback_data.type) {
        case SDL_TIMER_CALLBACK_TYPE_MS:
        case SDL_TIMER_CALLBACK_TYPE_NS:
            SDL_WaitSemaphoreTimeoutNS(data->sem, delay);
            break;
        case SDL_TIMER_CALLBACK_TYPE_PRECISE:
        {
            /* This is an adaptation of the algorithm in SDL_DelayPrecise() to
               produce proper timer behavior, ending the wait if signaled by
               the semaphore. Notes about how the algorithm works are in
               SDL_DelayPrecise()'s code.
             */

            bool signaled = false;
            const Uint64 SHORT_SLEEP_NS = 1 * SDL_NS_PER_MS;
            const Uint64 target_value = tick + delay;
            Uint64 max_sleep_ns = SHORT_SLEEP_NS;
            while (now + max_sleep_ns < target_value) {
                if (SDL_WaitSemaphoreTimeoutNS(data->sem, SHORT_SLEEP_NS)) {
                    signaled = true;
                    goto precise_timer_done;
                }

                const Uint64 next_now = SDL_GetTicksNS();
                const Uint64 next_sleep_ns = (next_now - now);
                if (next_sleep_ns > max_sleep_ns) {
                    max_sleep_ns = next_sleep_ns;
                }
                now = next_now;
            }

            if (now < target_value) {
                if ((target_value - now) > (max_sleep_ns - SHORT_SLEEP_NS)) {
                    const Uint64 delay_ns = (target_value - now) - (max_sleep_ns - SHORT_SLEEP_NS);
                    if (SDL_WaitSemaphoreTimeoutNS(data->sem, delay_ns)) {
                        signaled = true;
                        goto precise_timer_done;
                    } else {
                        now = SDL_GetTicksNS();
                    }
                }
            } else {
                signaled = SDL_WaitSemaphoreTimeoutNS(data->sem, 0);
                goto precise_timer_done;
            }

            while (now + SHORT_SLEEP_NS < target_value) {
                if (SDL_WaitSemaphoreTimeoutNS(data->sem, SHORT_SLEEP_NS)) {
                    signaled = true;
                    goto precise_timer_done;
                } else {
                    now = SDL_GetTicksNS();
                }
            }

            while (now < target_value) {
                if (SDL_WaitSemaphoreTimeoutNS(data->sem, 0)) {
                    signaled = true;
                    goto precise_timer_done;
                }
                now = SDL_GetTicksNS();
            }

        precise_timer_done:
            if (signaled) {
                current->callback_data.precise.accumulated = 0;
            } else {
                current->callback_data.precise.accumulated = (now - start) - delay;
            }
            break;
        }
        }
    }
    return 0;
}

bool SDL_InitTimers(void)
{
    SDL_TimerData *data = &SDL_timer_data;

    if (!SDL_ShouldInit(&data->init)) {
        return true;
    }

    data->timermap_lock = SDL_CreateMutex();
    if (!data->timermap_lock) {
        goto error;
    }

    data->sem = SDL_CreateSemaphore(0);
    if (!data->sem) {
        goto error;
    }

    SDL_SetAtomicInt(&data->active, true);

    // Timer threads use a callback into the app, so we can't set a limited stack size here.
    data->thread = SDL_CreateThread(SDL_TimerThread, "SDLTimer", data);
    if (!data->thread) {
        goto error;
    }

    SDL_SetInitialized(&data->init, true);
    return true;

error:
    SDL_SetInitialized(&data->init, true);
    SDL_QuitTimers();
    return false;
}

void SDL_QuitTimers(void)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_Timer *timer;
    SDL_TimerMap *entry;

    if (!SDL_ShouldQuit(&data->init)) {
        return;
    }

    SDL_SetAtomicInt(&data->active, false);

    // Shutdown the timer thread
    if (data->thread) {
        SDL_SignalSemaphore(data->sem);
        SDL_WaitThread(data->thread, NULL);
        data->thread = NULL;
    }

    if (data->sem) {
        SDL_DestroySemaphore(data->sem);
        data->sem = NULL;
    }

    // Clean up the timer entries
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

    if (data->timermap_lock) {
        SDL_DestroyMutex(data->timermap_lock);
        data->timermap_lock = NULL;
    }

    SDL_SetInitialized(&data->init, false);
}

static bool SDL_CheckInitTimers(void)
{
    return SDL_InitTimers();
}

static SDL_TimerID SDL_CreateTimer(Uint64 interval, const SDL_TimerCallbackData *callback_data, void *userdata)
{
    const Uint64 start = SDL_GetTicksNS();
    SDL_TimerData *data = &SDL_timer_data;
    SDL_Timer *timer;
    SDL_TimerMap *entry;

    if (!callback_data) {
        SDL_InvalidParamError("callback data");
        return 0;
    }

    switch (callback_data->type) {
    case SDL_TIMER_CALLBACK_TYPE_MS:
        if (!callback_data->callback_ms) {
            SDL_InvalidParamError("callback ms");
            return 0;
        }
        break;
    case SDL_TIMER_CALLBACK_TYPE_NS:
        if (!callback_data->callback_ns) {
            SDL_InvalidParamError("callback ns");
            return 0;
        }
        break;
    case SDL_TIMER_CALLBACK_TYPE_PRECISE:
        if (!callback_data->precise.callback) {
            SDL_InvalidParamError("callback precise");
            return 0;
        }
        break;
    default:
        SDL_InvalidParamError("callback type");
        return 0;
    }

    if (!SDL_CheckInitTimers()) {
        return 0;
    }

    SDL_LockSpinlock(&data->lock);
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
    timer->callback_data = *callback_data;
    timer->userdata = userdata;
    timer->interval = interval;
    if (timer->callback_data.type == SDL_TIMER_CALLBACK_TYPE_PRECISE) {
        timer->callback_data.precise.accumulated = 0;
    }
    timer->scheduled = start + timer->interval;
    SDL_SetAtomicInt(&timer->canceled, 0);

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

    // Add the timer to the pending list for the timer thread
    SDL_LockSpinlock(&data->lock);
    timer->next = data->pending;
    data->pending = timer;
    SDL_UnlockSpinlock(&data->lock);

    // Wake up the timer thread if necessary
    SDL_SignalSemaphore(data->sem);

    return entry->timerID;
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_MS;
    callback_data.callback_ms = callback;
    return SDL_CreateTimer(SDL_MS_TO_NS(interval), &callback_data, userdata);
}

SDL_TimerID SDL_AddTimerNS(Uint64 interval, SDL_NSTimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_NS;
    callback_data.callback_ns = callback;
    return SDL_CreateTimer(interval, &callback_data, userdata);
}

SDL_TimerID SDL_AddTimerPrecise(Uint64 interval, SDL_NSTimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_PRECISE;
    callback_data.precise.callback = callback;
    return SDL_CreateTimer(interval, &callback_data, userdata);
}

bool SDL_RemoveTimer(SDL_TimerID id)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *prev, *entry;
    bool canceled = false;

    if (!id) {
        return SDL_InvalidParamError("id");
    }

    // Find the timer
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
        if (!SDL_GetAtomicInt(&entry->timer->canceled)) {
            SDL_SetAtomicInt(&entry->timer->canceled, 1);
            canceled = true;
        }
        SDL_free(entry);
    }
    if (canceled) {
        return true;
    } else {
        return SDL_SetError("Timer not found");
    }
}

#else

/* TODO: No precise timer logic is here. Not sure how best to implement it
   in Emscripten. -nightmareci
*/

#include <emscripten/emscripten.h>
#include <emscripten/eventloop.h>

typedef struct SDL_TimerMap
{
    SDL_TimerID timerID;
    int timeoutID;
    Uint64 interval;
    SDL_TimerCallbackData callback_data;
    void *userdata;
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
    switch (entry->callback_data.type) {
    case SDL_TIMER_CALLBACK_TYPE_MS:
        entry->interval = SDL_MS_TO_NS(entry->callback_data.callback_ms(entry->userdata, entry->timerID, (Uint32)SDL_NS_TO_MS(entry->interval)));
        break;
    case SDL_TIMER_CALLBACK_TYPE_NS:
        entry->interval = entry->callback_data.callback_ns(entry->userdata, entry->timerID, entry->interval);
        break;
    case SDL_TIMER_CALLBACK_TYPE_PRECISE:
        entry->interval = entry->callback_data.precise.callback(entry->userdata, entry->timerID, entry->interval);
        break;
    }
    if (entry->interval > 0) {
        entry->timeoutID = emscripten_set_timeout(&SDL_Emscripten_TimerHelper,
                                                  SDL_NS_TO_MS(entry->interval),
                                                  entry);
    }
}

bool SDL_InitTimers(void)
{
    return true;
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

static SDL_TimerID SDL_CreateTimer(Uint64 interval, SDL_TimerCallbackData *callback_data, void *userdata)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *entry;

    if (!callback_data) {
        SDL_InvalidParamError("callback data");
        return 0;
    }

    switch (callback_data->type) {
    case SDL_TIMER_CALLBACK_TYPE_MS:
        if (!callback_data->callback_ms) {
            SDL_InvalidParamError("callback ms");
            return 0;
        }
        break;
    case SDL_TIMER_CALLBACK_TYPE_NS:
        if (!callback_data->callback_ns) {
            SDL_InvalidParamError("callback ns");
            return 0;
        }
        break;
    case SDL_TIMER_CALLBACK_TYPE_PRECISE:
        if (!callback_data->precise.callback) {
            SDL_InvalidParamError("callback precise");
            return 0;
        }
        break;
    default:
        SDL_InvalidParamError("callback type");
        return 0;
    }

    entry = (SDL_TimerMap *)SDL_malloc(sizeof(*entry));
    if (!entry) {
        return 0;
    }
    entry->timerID = SDL_GetNextObjectID();
    entry->callback_data = *callback_data;
    entry->userdata = userdata;
    entry->interval = interval;

    entry->timeoutID = emscripten_set_timeout(&SDL_Emscripten_TimerHelper,
                                              SDL_NS_TO_MS(entry->interval),
                                              entry);

    entry->next = data->timermap;
    data->timermap = entry;

    return entry->timerID;
}

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_MS;
    callback_data.callback_ms = callback;
    return SDL_CreateTimer(SDL_MS_TO_NS(interval), &callback_data, userdata);
}

SDL_TimerID SDL_AddTimerNS(Uint64 interval, SDL_NSTimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_NS;
    callback_data.callback_ns = callback;
    return SDL_CreateTimer(interval, &callback_data, userdata);
}

SDL_TimerID SDL_AddTimerPrecise(Uint64 interval, SDL_NSTimerCallback callback, void *userdata)
{
    SDL_TimerCallbackData callback_data;
    callback_data.type = SDL_TIMER_CALLBACK_TYPE_PRECISE;
    callback_data.precise.callback = callback;
    return SDL_CreateTimer(interval, &callback_data, userdata);
}

bool SDL_RemoveTimer(SDL_TimerID id)
{
    SDL_TimerData *data = &SDL_timer_data;
    SDL_TimerMap *prev, *entry;

    if (!id) {
        return SDL_InvalidParamError("id");
    }

    // Find the timer
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
        return true;
    } else {
        return SDL_SetError("Timer not found");
    }
}

#endif // !defined(SDL_PLATFORM_EMSCRIPTEN) || !SDL_THREADS_DISABLED

static Uint64 tick_start;
static Uint32 tick_numerator_ns;
static Uint32 tick_denominator_ns;
static Uint32 tick_numerator_ms;
static Uint32 tick_denominator_ms;

#if defined(SDL_TIMER_WINDOWS) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
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
#endif // HAVE_TIME_BEGIN_PERIOD
}

static void SDLCALL SDL_TimerResolutionChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    int period;

    // Unless the hint says otherwise, let's have good sleep precision
    if (hint && *hint) {
        period = SDL_atoi(hint);
    } else {
        period = 1;
    }
    if (period || oldValue != hint) {
        SDL_SetSystemTimerResolutionMS(period);
    }
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

    gcd = SDL_CalculateGCD(SDL_NS_PER_SECOND, (Uint32)tick_freq);
    tick_numerator_ns = (SDL_NS_PER_SECOND / gcd);
    tick_denominator_ns = (Uint32)(tick_freq / gcd);

    gcd = SDL_CalculateGCD(SDL_MS_PER_SECOND, (Uint32)tick_freq);
    tick_numerator_ms = (SDL_MS_PER_SECOND / gcd);
    tick_denominator_ms = (Uint32)(tick_freq / gcd);

    tick_start = SDL_GetPerformanceCounter();
    if (!tick_start) {
        --tick_start;
    }
}

void SDL_QuitTicks(void)
{
    SDL_RemoveHintCallback(SDL_HINT_TIMER_RESOLUTION,
                        SDL_TimerResolutionChanged, NULL);

    SDL_SetSystemTimerResolutionMS(0); // always release our timer resolution request.

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
    SDL_SYS_DelayNS(SDL_MS_TO_NS(ms));
}

void SDL_DelayNS(Uint64 ns)
{
    SDL_SYS_DelayNS(ns);
}

void SDL_DelayPrecise(Uint64 ns)
{
    Uint64 current_value = SDL_GetTicksNS();
    const Uint64 target_value = current_value + ns;

    // Sleep for a short number of cycles when real sleeps are desired.
    // We'll use 1 ms, it's the minimum guaranteed to produce real sleeps across
    // all platforms.
    const Uint64 SHORT_SLEEP_NS = 1 * SDL_NS_PER_MS;

    // Try to sleep short of target_value. If for some crazy reason
    // a particular platform sleeps for less than 1 ms when 1 ms was requested,
    // that's fine, the code below can cope with that, but in practice no
    // platforms behave that way.
    Uint64 max_sleep_ns = SHORT_SLEEP_NS;
    while (current_value + max_sleep_ns < target_value) {
        // Sleep for a short time
        SDL_SYS_DelayNS(SHORT_SLEEP_NS);

        const Uint64 now = SDL_GetTicksNS();
        const Uint64 next_sleep_ns = (now - current_value);
        if (next_sleep_ns > max_sleep_ns) {
            max_sleep_ns = next_sleep_ns;
        }
        current_value = now;
    }

    // Do a shorter sleep of the remaining time here, less the max overshoot in
    // the first loop. Due to maintaining max_sleep_ns as
    // greater-than-or-equal-to-1 ms, we can always subtract off 1 ms to get
    // the duration overshot beyond a 1 ms sleep request; if the system never
    // overshot, great, it's zero duration. By choosing the max overshoot
    // amount, we're likely to not overshoot here. If the sleep here ends up
    // functioning like SDL_DelayNS(0) internally, that's fine, we just don't
    // get to do a more-precise-than-1 ms-resolution sleep to undershoot by a
    // small amount on the current system, but SDL_DelayNS(0) does at least
    // introduce a small, yielding delay on many platforms, better than an
    // unyielding busyloop.
    //
    // Note that we'll always do at least one sleep in this function, so the
    // minimum resolution will be that of SDL_SYS_DelayNS()
    if (current_value < target_value && (target_value - current_value) > (max_sleep_ns - SHORT_SLEEP_NS)) {
        const Uint64 delay_ns = (target_value - current_value) - (max_sleep_ns - SHORT_SLEEP_NS);
        SDL_SYS_DelayNS(delay_ns);
        current_value = SDL_GetTicksNS();
    }

    // We've likely undershot target_value at this point by a pretty small
    // amount, but maybe not. The footgun case if not handled here is where
    // we've undershot by a large amount, like several ms, but still smaller
    // than the amount max_sleep_ns overshot by; in such a situation, the above
    // shorter-sleep block didn't do any delay, the if-block wasn't entered.
    // Also, maybe the shorter-sleep undershot by several ms, so we still don't
    // want to spin a lot then. In such a case, we accept the possibility of
    // overshooting to not spin much, or if overshot here, not at all, keeping
    // CPU/power usage down in any case. Due to scheduler sloppiness, it's
    // entirely possible to end up undershooting/overshooting here by much less
    // than 1 ms even if the current system's sleep function is only 1
    // ms-resolution, as SDL_GetTicksNS() generally is better resolution than 1
    // ms on the systems SDL supports.
    while (current_value + SHORT_SLEEP_NS < target_value) {
        SDL_SYS_DelayNS(SHORT_SLEEP_NS);
        current_value = SDL_GetTicksNS();
    }

    // Spin for any remaining time
    while (current_value < target_value) {
        SDL_CPUPauseInstruction();
        current_value = SDL_GetTicksNS();
    }
}
