/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_TIMER_PSP

#include "../SDL_timer_c.h"
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pspthreadman.h>


Uint64
SDL_GetPerformanceCounter(void)
{
    Uint64 ticks;
	struct timeval now;

	gettimeofday(&now, NULL);
	ticks = now.tv_sec;
	ticks *= SDL_US_PER_SECOND;
	ticks += now.tv_usec;
    return ticks;
}

Uint64
SDL_GetPerformanceFrequency(void)
{
    return SDL_US_PER_SECOND;
}

void SDL_DelayNS(Uint64 ns)
{
    const Uint64 max_delay = 0xffffffff * SDL_NS_PER_US;
    if (ns > max_delay) {
        ns = max_delay;
    }
    sceKernelDelayThreadCB((SceUInt)SDL_NS_TO_US(ns));
}

#endif /* SDL_TIMER_PSP */

/* vim: ts=4 sw=4
 */
