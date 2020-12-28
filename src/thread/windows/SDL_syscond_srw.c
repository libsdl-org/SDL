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

#include "SDL_hints.h"
#include "SDL_thread.h"

#include "../generic/SDL_syscond_c.h"

typedef SDL_cond * (*pfnSDL_CreateCond)(void);
typedef void (*pfnSDL_DestroyCond)(SDL_cond *);
typedef int (*pfnSDL_CondSignal)(SDL_cond *);
typedef int (*pfnSDL_CondBroadcast)(SDL_cond *);
typedef int (*pfnSDL_CondWait)(SDL_cond *, SDL_mutex *);
typedef int (*pfnSDL_CondWaitTimeout)(SDL_cond *, SDL_mutex *, Uint32);

typedef struct SDL_cond_impl_t
{
    pfnSDL_CreateCond       Create;
    pfnSDL_DestroyCond      Destroy;
    pfnSDL_CondSignal       Signal;
    pfnSDL_CondBroadcast    Broadcast;
    pfnSDL_CondWait         Wait;
    pfnSDL_CondWaitTimeout  WaitTimeout;
} SDL_cond_impl_t;

/* Implementation will be chosen at runtime based on available Kernel features */
static SDL_cond_impl_t SDL_cond_impl_active = {0};

/**
 * Generic Condition Variable implementation using SDL_mutex and SDL_sem
 */

static const SDL_cond_impl_t SDL_cond_impl_generic =
{
    &SDL_CreateCond_generic,
    &SDL_DestroyCond_generic,
    &SDL_CondSignal_generic,
    &SDL_CondBroadcast_generic,
    &SDL_CondWait_generic,
    &SDL_CondWaitTimeout_generic,
};


SDL_cond *
SDL_CreateCond(void)
{
    if (SDL_cond_impl_active.Create == NULL) {
        SDL_memcpy(&SDL_cond_impl_active, &SDL_cond_impl_generic, sizeof(SDL_cond_impl_active));
    }
    return SDL_cond_impl_active.Create();
}

void
SDL_DestroyCond(SDL_cond * cond)
{
    SDL_cond_impl_active.Destroy(cond);
}

int
SDL_CondSignal(SDL_cond * cond)
{
    return SDL_cond_impl_active.Signal(cond);
}

int
SDL_CondBroadcast(SDL_cond * cond)
{
    return SDL_cond_impl_active.Broadcast(cond);
}

int
SDL_CondWaitTimeout(SDL_cond * cond, SDL_mutex * mutex, Uint32 ms)
{
    return SDL_cond_impl_active.WaitTimeout(cond, mutex, ms);
}

int
SDL_CondWait(SDL_cond * cond, SDL_mutex * mutex)
{
    return SDL_cond_impl_active.Wait(cond, mutex);
}

/* vi: set ts=4 sw=4 expandtab: */
