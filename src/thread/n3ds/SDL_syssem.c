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

#ifdef SDL_THREAD_N3DS

/* An implementation of semaphores using libctru's LightSemaphore */

#include <3ds.h>

int WaitOnSemaphoreFor(SDL_Semaphore *sem, Sint64 timeout);

struct SDL_Semaphore
{
    LightSemaphore semaphore;
};

SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_Semaphore *sem;

    if (initial_value > SDL_MAX_SINT16) {
        SDL_SetError("Initial semaphore value too high for this platform");
        return NULL;
    }

    sem = (SDL_Semaphore *)SDL_malloc(sizeof(*sem));
    if (!sem) {
        return NULL;
    }

    LightSemaphore_Init(&sem->semaphore, initial_value, SDL_MAX_SINT16);

    return sem;
}

/* WARNING:
   You cannot call this function when another thread is using the semaphore.
*/
void SDL_DestroySemaphore(SDL_Semaphore *sem)
{
    SDL_free(sem);
}

int SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }

    if (timeoutNS == -1) {  // -1 == wait indefinitely.
        LightSemaphore_Acquire(&sem->semaphore, 1);
        return 0;
    }

    if (LightSemaphore_TryAcquire(&sem->semaphore, 1) != 0) {
        return WaitOnSemaphoreFor(sem, timeoutNS);
    }

    return 0;
}

int WaitOnSemaphoreFor(SDL_Semaphore *sem, Sint64 timeout)
{
    Uint64 stop_time = SDL_GetTicksNS() + timeout;
    Uint64 current_time = SDL_GetTicksNS();
    while (current_time < stop_time) {
        if (LightSemaphore_TryAcquire(&sem->semaphore, 1) == 0) {
            return 0;
        }
        /* 100 microseconds seems to be the sweet spot */
        SDL_DelayNS(SDL_US_TO_NS(100));
        current_time = SDL_GetTicksNS();
    }

    /* If we failed, yield to avoid starvation on busy waits */
    SDL_DelayNS(1);
    return SDL_MUTEX_TIMEDOUT;
}

Uint32 SDL_GetSemaphoreValue(SDL_Semaphore *sem)
{
    if (!sem) {
        SDL_InvalidParamError("sem");
        return 0;
    }
    return sem->semaphore.current_count;
}

int SDL_PostSemaphore(SDL_Semaphore *sem)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    LightSemaphore_Release(&sem->semaphore, 1);
    return 0;
}

#endif /* SDL_THREAD_N3DS */
