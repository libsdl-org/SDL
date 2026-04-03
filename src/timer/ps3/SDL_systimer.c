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

#ifdef SDL_TIMER_PS3

#include "../SDL_timer_c.h"

#include <time.h>
#include <sys/time.h>
#include <sys/systime.h>
#include <unistd.h>

static struct timeval start;
static bool ticks_started = false;

int gettimeofday(struct timeval* tv, void* unused)
{
    system_time_t sec;
    system_time_t nsec;
    int rv = -1;

    rv = sysGetCurrentTime(&sec, &nsec);
    if (rv < 0) {
        return -1;
    }

    tv->tv_sec = sec;
    tv->tv_usec = nsec / 1000;

    return 0;
}

void SDL_TicksInit(void)
{
    if (ticks_started) {
        return;
    }
    ticks_started = true;

    gettimeofday(&start, NULL);
}

void SDL_TicksQuit(void)
{
    ticks_started = false;
}

Uint64 SDL_GetPerformanceCounter(void)
{
    system_time_t sec;
    system_time_t nsec;
    sysGetCurrentTime(&sec, &nsec);
    return (Uint64)sec * 1000000000ULL + nsec;
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return 1000000000ULL; // nanoseconds
}

void SDL_SYS_DelayNS(Uint64 ns)
{
    sysUsleep(ns);
}

#endif /* SDL_TIMER_PS3 */

/* vi: set ts=4 sw=4 expandtab: */
