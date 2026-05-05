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

#ifdef SDL_THREAD_DOS

/* Semaphore implementation for DOS cooperative threading.
   Uses cli/sti for atomicity and cooperative yielding for waits. */

#include "../../core/dos/SDL_dos.h"
#include "../../core/dos/SDL_dos_scheduler.h"

struct SDL_Semaphore
{
    volatile Uint32 count;
};

SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_Semaphore *sem = (SDL_Semaphore *)SDL_malloc(sizeof(*sem));
    if (sem) {
        sem->count = initial_value;
    }
    return sem;
}

void SDL_DestroySemaphore(SDL_Semaphore *sem)
{
    if (sem) {
        SDL_free(sem);
    }
}

bool SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS)
{
    if (!sem) {
        return true;
    }

    /* Try-wait (poll): check and decrement if possible */
    if (timeoutNS == 0) {
        bool acquired = false;
        DOS_DisableInterrupts();
        if (sem->count > 0) {
            sem->count--;
            acquired = true;
        }
        DOS_EnableInterrupts();
        return acquired;
    }

    /* Indefinite wait (-1) or timed wait */
    Uint64 deadline = 0;
    if (timeoutNS > 0) {
        deadline = SDL_GetPerformanceCounter() +
                   (Uint64)((double)timeoutNS * (double)SDL_GetPerformanceFrequency() / 1e9);
    }

    for (;;) {
        DOS_DisableInterrupts();
        if (sem->count > 0) {
            sem->count--;
            DOS_EnableInterrupts();
            return true; /* Acquired */
        }
        DOS_EnableInterrupts();

        /* Check timeout */
        if (timeoutNS > 0 && SDL_GetPerformanceCounter() >= deadline) {
            return false; /* Timed out */
        }

        /* Yield to other threads instead of busy-spinning */
        DOS_Yield();
    }
}

Uint32 SDL_GetSemaphoreValue(SDL_Semaphore *sem)
{
    if (!sem) {
        return 0;
    }
    return sem->count;
}

void SDL_SignalSemaphore(SDL_Semaphore *sem)
{
    if (!sem) {
        return;
    }

    DOS_DisableInterrupts();
    sem->count++;
    DOS_EnableInterrupts();
}

#endif /* SDL_THREAD_DOS */
