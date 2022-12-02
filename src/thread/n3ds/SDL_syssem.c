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

#ifdef SDL_THREAD_N3DS

/* An implementation of semaphores using libctru's LightSemaphore */

#include <3ds.h>

struct SDL_semaphore
{
    LightSemaphore semaphore;
};

SDL_sem *
SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem;

    if (initial_value > SDL_MAX_SINT16) {
        SDL_SetError("Initial semaphore value too high for this platform");
        return NULL;
    }

    sem = (SDL_sem *)SDL_malloc(sizeof(*sem));
    if (sem == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    LightSemaphore_Init(&sem->semaphore, initial_value, SDL_MAX_SINT16);

    return sem;
}

/* WARNING:
   You cannot call this function when another thread is using the semaphore.
*/
void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (sem) {
        SDL_free(sem);
    }
}

int SDL_SemWaitTimeoutNS(SDL_sem *sem, Sint64 timeoutNS)
{
    int retval;

    if (sem == NULL) {
        return SDL_InvalidParamError("sem");
    }

    if (timeoutNS == SDL_MUTEX_MAXWAIT) {
        LightSemaphore_Acquire(&sem->semaphore, 1);
        retval = 0;
    } else {
        int return_code = LightSemaphore_TryAcquire(&sem->semaphore, 1);
        if (return_code != 0) {
            /* FIXME: Does this code guarantee a wall clock timeout here?
             *        Can we handle sub-millisecond delays? */
            u32 timeout = (u32)SDL_NS_TO_MS(timeoutNS);
            for (u32 i = 0; i < timeout; i++) {
                svcSleepThread(1000000LL);
                return_code = LightSemaphore_TryAcquire(&sem->semaphore, 1);
                if (return_code == 0) {
                    break;
                }
            }
        }
        retval = return_code != 0 ? SDL_MUTEX_TIMEDOUT : 0;
    }

    return retval;
}

Uint32
SDL_SemValue(SDL_sem *sem)
{
    if (sem == NULL) {
        SDL_InvalidParamError("sem");
        return 0;
    }
    return sem->semaphore.current_count;
}

int SDL_SemPost(SDL_sem *sem)
{
    if (sem == NULL) {
        return SDL_InvalidParamError("sem");
    }
    LightSemaphore_Release(&sem->semaphore, 1);
    return 0;
}

#endif /* SDL_THREAD_N3DS */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
