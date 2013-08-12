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

#ifdef SDL_THREAD_BEOS

/* Semaphores in the BeOS environment */

#include <be/kernel/OS.h>

#include "SDL_thread.h"


struct SDL_semaphore
{
    sem_id id;
};

/* Create a counting semaphore */
SDL_sem *
SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem;

    sem = (SDL_sem *) SDL_malloc(sizeof(*sem));
    if (sem) {
        sem->id = create_sem(initial_value, "SDL semaphore");
        if (sem->id < B_NO_ERROR) {
            SDL_SetError("create_sem() failed");
            SDL_free(sem);
            sem = NULL;
        }
    } else {
        SDL_OutOfMemory();
    }
    return (sem);
}

/* Free the semaphore */
void
SDL_DestroySemaphore(SDL_sem * sem)
{
    if (sem) {
        if (sem->id >= B_NO_ERROR) {
            delete_sem(sem->id);
        }
        SDL_free(sem);
    }
}

int
SDL_SemWaitTimeout(SDL_sem * sem, Uint32 timeout)
{
    int32 val;
    int retval;

    if (!sem) {
        return SDL_SetError("Passed a NULL semaphore");
    }

  tryagain:
    if (timeout == SDL_MUTEX_MAXWAIT) {
        val = acquire_sem(sem->id);
    } else {
        timeout *= 1000;        /* BeOS uses a timeout in microseconds */
        val = acquire_sem_etc(sem->id, 1, B_RELATIVE_TIMEOUT, timeout);
    }
    switch (val) {
    case B_INTERRUPTED:
        goto tryagain;
    case B_NO_ERROR:
        retval = 0;
        break;
    case B_TIMED_OUT:
        retval = SDL_MUTEX_TIMEDOUT;
        break;
    case B_WOULD_BLOCK:
        retval = SDL_MUTEX_TIMEDOUT;
        break;
    default:
        retval = SDL_SetError("acquire_sem() failed");
        break;
    }

    return retval;
}

int
SDL_SemTryWait(SDL_sem * sem)
{
    return SDL_SemWaitTimeout(sem, 0);
}

int
SDL_SemWait(SDL_sem * sem)
{
    return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

/* Returns the current count of the semaphore */
Uint32
SDL_SemValue(SDL_sem * sem)
{
    int32 count;
    Uint32 value;

    value = 0;
    if (sem) {
        get_sem_count(sem->id, &count);
        if (count > 0) {
            value = (Uint32) count;
        }
    }
    return value;
}

/* Atomically increases the semaphore's count (not blocking) */
int
SDL_SemPost(SDL_sem * sem)
{
    if (!sem) {
        return SDL_SetError("Passed a NULL semaphore");
    }

    if (release_sem(sem->id) != B_NO_ERROR) {
        return SDL_SetError("release_sem() failed");
    }
    return 0;
}

#endif /* SDL_THREAD_BEOS */

/* vi: set ts=4 sw=4 expandtab: */
