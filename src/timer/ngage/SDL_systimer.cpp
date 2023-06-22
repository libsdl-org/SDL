/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_TIMER_NGAGE

#include <e32std.h>
#include <e32hal.h>

static TUint start_tick = 0;

#ifdef __cplusplus
extern "C" {
#endif


Uint64 SDL_GetPerformanceCounter(void)
{
    /* FIXME: Need to account for 32-bit wrapping */
    return (Uint64)User::TickCount();
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    return SDL_US_PER_SECOND;
}

void SDL_DelayNS(Uint64 ns)
{
    const Uint64 max_delay = 0x7fffffffLL * SDL_NS_PER_US;
    if (ns > max_delay) {
        ns = max_delay;
    }
    User::After(TTimeIntervalMicroSeconds32((TInt)SDL_NS_TO_US(ns)));
}

#ifdef __cplusplus
}
#endif

#endif /* SDL_TIMER_NGAGE */
