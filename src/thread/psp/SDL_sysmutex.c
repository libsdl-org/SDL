/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_THREAD_PSP

// An implementation of mutexes using semaphores

#include "SDL_systhread_c.h"

#include <pspthreadman.h>
#include <pspkerror.h>

#define SCE_KERNEL_MUTEX_ATTR_RECURSIVE 0x0200U

struct SDL_Mutex
{
    SceLwMutexWorkarea lock;
};

SDL_Mutex *SDL_CreateMutex(void)
{
    SDL_Mutex *mutex = (SDL_Mutex *)SDL_malloc(sizeof(*mutex));
    if (mutex) {
        const SceInt32 res = sceKernelCreateLwMutex(
            &mutex->lock,
            "SDL mutex",
            SCE_KERNEL_MUTEX_ATTR_RECURSIVE,
            0,
            NULL);

        if (res < 0) {
            SDL_free(mutex);
            mutex = NULL;
            SDL_SetError("Error trying to create mutex: %lx", res);
        }
    }
    return mutex;
}

void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        sceKernelDeleteLwMutex(&mutex->lock);
        SDL_free(mutex);
    }
}

void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (mutex != NULL) {
        const SceInt32 res = sceKernelLockLwMutex(&mutex->lock, 1, NULL);
        SDL_assert(res == SCE_KERNEL_ERROR_OK);  // assume we're in a lot of trouble if this assert fails.
    }
#endif // SDL_THREADS_DISABLED
}

int SDL_TryLockMutex(SDL_Mutex *mutex)
{
    int retval = 0;
#ifndef SDL_THREADS_DISABLED
    if (mutex) {
        const SceInt32 res = sceKernelTryLockLwMutex(&mutex->lock, 1);
        if (res == SCE_KERNEL_ERROR_OK) {
            retval = 0;
        } else if (res == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
            retval = SDL_MUTEX_TIMEDOUT;
        } else {
            SDL_assert(!"Error trying to lock mutex");  // assume we're in a lot of trouble if this assert fails.
            retval = SDL_MUTEX_TIMEDOUT;
        }
    }
#endif // SDL_THREADS_DISABLED
    return retval;
}

void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (mutex != NULL) {
        const SceInt32 res = sceKernelUnlockLwMutex(&mutex->lock, 1);
        SDL_assert(res == 0);  // assume we're in a lot of trouble if this assert fails.
    }
#endif // SDL_THREADS_DISABLED
}

#endif // SDL_THREAD_PSP
