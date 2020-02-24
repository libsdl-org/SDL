/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if defined(SDL_TIMER_DOS)

#include <time.h>
#include <dos.h>

#include "SDL_timer.h"

static clock_t start;
static SDL_bool ticks_started = SDL_FALSE;

void
SDL_TicksInit(void)
{
    if (ticks_started) {
        return;
    }
    start = clock();
    ticks_started = SDL_TRUE;
}

void
SDL_TicksQuit(void)
{
    ticks_started = SDL_FALSE;
}

Uint32
SDL_GetTicks(void)
{
    if (!ticks_started) {
        SDL_TicksInit();
    }

    return (clock() - start) * 1000 / CLOCKS_PER_SEC;
}

Uint64
SDL_GetPerformanceCounter(void)
{
#ifdef HAVE_UCLOCK
    return uclock();
#else
    return SDL_GetTicks();
#endif
}

Uint64
SDL_GetPerformanceFrequency(void)
{
#ifdef HAVE_UCLOCK
    return UCLOCKS_PER_SEC;
#else
    return return 1000;
#endif
}

void
SDL_Delay(Uint32 ms)
{
    delay(ms);
}

#endif /* SDL_TIMER_DOS */

/* vi: set ts=4 sw=4 expandtab: */
