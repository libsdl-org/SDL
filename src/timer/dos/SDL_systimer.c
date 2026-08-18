/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_TIMER_DOS

#include <dos.h>  /* delay */
#include <time.h> /* uclock, uclock_t, UCLOCKS_PER_SEC */

#include "../../core/dos/SDL_dos_scheduler.h"

/* DJGPP's uclock() reprograms PIT channel 0 for a higher tick rate on first
   call, giving ~1.19 MHz resolution (UCLOCKS_PER_SEC == 1193180).  This is
   the same approach SDL2-dos used and gives sub-microsecond precision without
   any extra setup. */

Uint64 SDL_GetPerformanceCounter(void)
{
    return (Uint64)uclock();
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return (Uint64)UCLOCKS_PER_SEC;
}

void SDL_SYS_DelayNS(Uint64 ns)
{
    if (ns == 0) {
        DOS_Yield();
        return;
    }

    const uclock_t delay_start = uclock();
    const uclock_t target_ticks = (uclock_t)((ns * UCLOCKS_PER_SEC) / SDL_NS_PER_SECOND);

    while ((uclock() - delay_start) < target_ticks) {
        /* Always yield first so cooperative threads can run. */
        DOS_Yield();

        /* If more than 1 ms remains, do a short sleep to avoid burning
           100% CPU when no other threads need to run.  DJGPP's delay()
           is a busy-wait but it does halt-loop on the PIT, which is
           lighter than a tight uclock() poll. */
        uclock_t remaining = target_ticks - (uclock() - delay_start);
        if (remaining > (UCLOCKS_PER_SEC / 1000)) {
            delay(1);
        }
    }
}

#endif /* SDL_TIMER_DOS */
