/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#ifdef SDL_TIMER_WINDOWS

#include "../../core/windows/SDL_windows.h"
#include <mmsystem.h>

#include "SDL_timer.h"
#include "SDL_hints.h"


/* The first (low-resolution) ticks value of the application */
static DWORD start;
static BOOL ticks_started = FALSE; 

#ifndef USE_GETTICKCOUNT
/* Store if a high-resolution performance counter exists on the system */
static BOOL hires_timer_available;
/* The first high-resolution ticks value of the application */
static LARGE_INTEGER hires_start_ticks;
/* The number of ticks per second of the high-resolution performance counter */
static LARGE_INTEGER hires_ticks_per_second;
#endif

static void
timeSetPeriod(UINT uPeriod)
{
    static UINT timer_period = 0;

    if (uPeriod != timer_period) {
        if (timer_period) {
            timeEndPeriod(timer_period);
        }

        timer_period = uPeriod;

        if (timer_period) {
            timeBeginPeriod(timer_period);
        }
    }
}

static void
SDL_TimerResolutionChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    UINT uPeriod;

    /* Unless the hint says otherwise, let's have good sleep precision */
    if (hint && *hint) {
        uPeriod = SDL_atoi(hint);
    } else {
        uPeriod = 1;
    }
    if (uPeriod || oldValue != hint) {
        timeSetPeriod(uPeriod);
    }
}

void
SDL_InitTicks(void)
{
    if (ticks_started) {
        return;
    }
    ticks_started = TRUE;

    /* Set first ticks value */
#ifdef USE_GETTICKCOUNT
    start = GetTickCount();
#else
    /* QueryPerformanceCounter has had problems in the past, but lots of games
       use it, so we'll rely on it here.
     */
    if (QueryPerformanceFrequency(&hires_ticks_per_second) == TRUE) {
        hires_timer_available = TRUE;
        QueryPerformanceCounter(&hires_start_ticks);
    } else {
        hires_timer_available = FALSE;
        timeSetPeriod(1);     /* use 1 ms timer precision */
        start = timeGetTime();
    }
#endif

    SDL_AddHintCallback(SDL_HINT_TIMER_RESOLUTION,
                        SDL_TimerResolutionChanged, NULL);
}

Uint32
SDL_GetTicks(void)
{
    DWORD now;
#ifndef USE_GETTICKCOUNT
    LARGE_INTEGER hires_now;
#endif

    if (!ticks_started) {
        SDL_InitTicks();
    }

#ifdef USE_GETTICKCOUNT
    now = GetTickCount();
#else
    if (hires_timer_available) {
        QueryPerformanceCounter(&hires_now);

        hires_now.QuadPart -= hires_start_ticks.QuadPart;
        hires_now.QuadPart *= 1000;
        hires_now.QuadPart /= hires_ticks_per_second.QuadPart;

        return (DWORD) hires_now.QuadPart;
    } else {
        now = timeGetTime();
    }
#endif

    return (now - start);
}

Uint64
SDL_GetPerformanceCounter(void)
{
    LARGE_INTEGER counter;

    if (!QueryPerformanceCounter(&counter)) {
        return SDL_GetTicks();
    }
    return counter.QuadPart;
}

Uint64
SDL_GetPerformanceFrequency(void)
{
    LARGE_INTEGER frequency;

    if (!QueryPerformanceFrequency(&frequency)) {
        return 1000;
    }
    return frequency.QuadPart;
}

void
SDL_Delay(Uint32 ms)
{
    Sleep(ms);
}

#endif /* SDL_TIMER_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
