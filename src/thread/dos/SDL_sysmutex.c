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

/* Mutex implementation for DOS cooperative threading.
   Uses cli/sti for atomicity and cooperative yielding for contention. */

#include "../../core/dos/SDL_dos.h"
#include "../../core/dos/SDL_dos_scheduler.h"

#define MUTEX_NO_OWNER -1

struct SDL_Mutex
{
    volatile int owner;     /* Thread ID of owner, or MUTEX_NO_OWNER if unlocked */
    volatile int recursive; /* Recursion count */
};

SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *mutex = (SDL_Mutex *)SDL_malloc(sizeof(*mutex));
    if (mutex) {
        mutex->owner = MUTEX_NO_OWNER;
        mutex->recursive = 0;
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        SDL_free(mutex);
    }
}

void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (!mutex) {
        return;
    }

    int tid = DOS_GetCurrentThreadID();

    for (;;) {
        DOS_DisableInterrupts();
        if (mutex->owner == MUTEX_NO_OWNER) {
            mutex->owner = tid;
            mutex->recursive = 1;
            DOS_EnableInterrupts();
            return;
        }
        if (mutex->owner == tid) {
            mutex->recursive++;
            DOS_EnableInterrupts();
            return;
        }
        DOS_EnableInterrupts();
        DOS_Yield();
    }
}

bool SDL_TryLockMutex(SDL_Mutex *mutex)
{
    if (!mutex) {
        return true;
    }

    int tid = DOS_GetCurrentThreadID();

    DOS_DisableInterrupts();
    if (mutex->owner == MUTEX_NO_OWNER) {
        mutex->owner = tid;
        mutex->recursive = 1;
        DOS_EnableInterrupts();
        return true;
    }
    if (mutex->owner == tid) {
        mutex->recursive++;
        DOS_EnableInterrupts();
        return true;
    }
    DOS_EnableInterrupts();
    return false;
}

void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS
{
    if (!mutex) {
        return;
    }

    DOS_DisableInterrupts();
    if (mutex->recursive > 1) {
        mutex->recursive--;
    } else {
        mutex->owner = MUTEX_NO_OWNER;
        mutex->recursive = 0;
    }
    DOS_EnableInterrupts();
}

#endif /* SDL_THREAD_DOS */
