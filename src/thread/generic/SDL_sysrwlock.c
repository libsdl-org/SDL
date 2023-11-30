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

// An implementation of rwlocks using mutexes, condition variables, and atomics.

#include "SDL_systhread_c.h"

#include "../generic/SDL_sysrwlock_c.h"

/* If two implementations are to be compiled into SDL (the active one
 * will be chosen at runtime), the function names need to be
 * suffixed
 */
// !!! FIXME: this is quite a tapdance with macros and the build system, maybe we can simplify how we do this. --ryan.
#ifndef SDL_THREAD_GENERIC_RWLOCK_SUFFIX
#define SDL_CreateRWLock_generic SDL_CreateRWLock
#define SDL_DestroyRWLock_generic SDL_DestroyRWLock
#define SDL_LockRWLockForReading_generic SDL_LockRWLockForReading
#define SDL_LockRWLockForWriting_generic SDL_LockRWLockForWriting
#define SDL_TryLockRWLockForReading_generic SDL_TryLockRWLockForReading
#define SDL_TryLockRWLockForWriting_generic SDL_TryLockRWLockForWriting
#define SDL_UnlockRWLock_generic SDL_UnlockRWLock
#endif

struct SDL_RWLock
{
#ifdef SDL_THREADS_DISABLED
    int unused;
#else
    SDL_Mutex *lock;
    SDL_Condition *condition;
    SDL_threadID writer_thread;
    SDL_AtomicInt reader_count;
    SDL_AtomicInt writer_count;
#endif
};

SDL_RWLock *SDL_CreateRWLock_generic(void)
{
    SDL_RWLock *rwlock = (SDL_RWLock *) SDL_calloc(1, sizeof (*rwlock));

    if (!rwlock) {
        return NULL;
    }

#ifndef SDL_THREADS_DISABLED
    rwlock->lock = SDL_CreateMutex();
    if (!rwlock->lock) {
        SDL_free(rwlock);
        return NULL;
    }

    rwlock->condition = SDL_CreateCondition();
    if (!rwlock->condition) {
        SDL_DestroyMutex(rwlock->lock);
        SDL_free(rwlock);
        return NULL;
    }

    SDL_AtomicSet(&rwlock->reader_count, 0);
    SDL_AtomicSet(&rwlock->writer_count, 0);
#endif

    return rwlock;
}

void SDL_DestroyRWLock_generic(SDL_RWLock *rwlock)
{
    if (rwlock) {
#ifndef SDL_THREADS_DISABLED
        SDL_DestroyMutex(rwlock->lock);
        SDL_DestroyCondition(rwlock->condition);
#endif
        SDL_free(rwlock);
    }
}

void SDL_LockRWLockForReading_generic(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (rwlock) {
        // !!! FIXME: these don't have to be atomic, we always gate them behind a mutex.
        SDL_LockMutex(rwlock->lock);
        SDL_assert(SDL_AtomicGet(&rwlock->writer_count) == 0);  // shouldn't be able to grab lock if there's a writer!
        SDL_AtomicAdd(&rwlock->reader_count, 1);
        SDL_UnlockMutex(rwlock->lock);   // other readers can attempt to share the lock.
    }
#endif
}

void SDL_LockRWLockForWriting_generic(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (rwlock) {
        SDL_LockMutex(rwlock->lock);
        while (SDL_AtomicGet(&rwlock->reader_count) > 0) {  // while something is holding the shared lock, keep waiting.
            SDL_WaitCondition(rwlock->condition, rwlock->lock);  // release the lock and wait for readers holding the shared lock to release it, regrab the lock.
        }

        // we hold the lock!
        SDL_AtomicAdd(&rwlock->writer_count, 1);  // we let these be recursive, but the API doesn't require this. It _does_ trust you unlock correctly!
    }
#endif
}

int SDL_TryLockRWLockForReading_generic(SDL_RWLock *rwlock)
{
#ifndef SDL_THREADS_DISABLED
    if (rwlock) {
        const int rc = SDL_TryLockMutex(rwlock->lock);
        if (rc != 0) {
            // !!! FIXME: there is a small window where a reader has to lock the mutex, and if we hit that, we will return SDL_RWLOCK_TIMEDOUT even though we could have shared the lock.
            return rc;
        }

        SDL_assert(SDL_AtomicGet(&rwlock->writer_count) == 0);  // shouldn't be able to grab lock if there's a writer!
        SDL_AtomicAdd(&rwlock->reader_count, 1);
        SDL_UnlockMutex(rwlock->lock);   // other readers can attempt to share the lock.
    }
#endif

    return 0;
}

int SDL_TryLockRWLockForWriting_generic(SDL_RWLock *rwlock)
{
#ifndef SDL_THREADS_DISABLED
    if (rwlock) {
        const int rc = SDL_TryLockMutex(rwlock->lock);
        if (rc != 0) {
            return rc;
        }

        if (SDL_AtomicGet(&rwlock->reader_count) > 0) {  // a reader is using the shared lock, treat it as unavailable.
            SDL_UnlockMutex(rwlock->lock);
            return SDL_RWLOCK_TIMEDOUT;
        }

        // we hold the lock!
        SDL_AtomicAdd(&rwlock->writer_count, 1);  // we let these be recursive, but the API doesn't require this. It _does_ trust you unlock correctly!
    }
#endif

    return 0;
}

void SDL_UnlockRWLock_generic(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
#ifndef SDL_THREADS_DISABLED
    if (rwlock) {
        SDL_LockMutex(rwlock->lock);  // recursive lock for writers, readers grab lock to make sure things are sane.

        if (SDL_AtomicGet(&rwlock->reader_count) > 0) {  // we're a reader
            SDL_AtomicAdd(&rwlock->reader_count, -1);
            SDL_BroadcastCondition(rwlock->condition);  // alert any pending writers to attempt to try to grab the lock again.
        } else if (SDL_AtomicGet(&rwlock->writer_count) > 0) {  // we're a writer
            SDL_AtomicAdd(&rwlock->writer_count, -1);
            SDL_UnlockMutex(rwlock->lock);  // recursive unlock.
        }

        SDL_UnlockMutex(rwlock->lock);
    }
#endif
}

