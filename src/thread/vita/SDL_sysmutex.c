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

#ifdef SDL_THREAD_VITA

#include "SDL_systhread_c.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>

struct SDL_Mutex
{
    SceKernelLwMutexWork lock;
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
            SDL_SetError("Error trying to create mutex: %x", res);
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
        SDL_assert(res == SCE_KERNEL_OK);  // assume we're in a lot of trouble if this assert fails.
    }
#endif // SDL_THREADS_DISABLED
}

int SDL_TryLockMutex(SDL_Mutex *mutex)
{
#ifdef SDL_THREADS_DISABLED
    return 0;
#else
    SceInt32 res = 0;

    if (!mutex) {
        return 0;
    }

    res = sceKernelTryLockLwMutex(&mutex->lock, 1);
    switch (res) {
    case SCE_KERNEL_OK: return 0;
    case SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN: return SDL_MUTEX_TIMEDOUT;
    default: break;
    }

    SDL_assert(!"Error trying to lock mutex");  // assume we're in a lot of trouble if this assert fails.
    return SDL_MUTEX_TIMEDOUT;
#endif // SDL_THREADS_DISABLED
}

void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (mutex != NULL) {
        const SceInt32 res = sceKernelUnlockLwMutex(&mutex->lock, 1);
        SDL_assert(res == SCE_KERNEL_OK);  // assume we're in a lot of trouble if this assert fails.
    }
#endif // SDL_THREADS_DISABLED
}

#endif // SDL_THREAD_VITA
