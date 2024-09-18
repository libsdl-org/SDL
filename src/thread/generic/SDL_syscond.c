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

// An implementation of condition variables using semaphores and mutexes
/*
   This implementation borrows heavily from the BeOS condition variable
   implementation, written by Christopher Tate and Owen Smith.  Thanks!
 */

#include "../generic/SDL_syscond_c.h"

/* If two implementations are to be compiled into SDL (the active one
 * will be chosen at runtime), the function names need to be
 * suffixed
 */
#ifndef SDL_THREAD_GENERIC_COND_SUFFIX
#define SDL_CreateCondition_generic      SDL_CreateCondition
#define SDL_DestroyCondition_generic     SDL_DestroyCondition
#define SDL_SignalCondition_generic      SDL_SignalCondition
#define SDL_BroadcastCondition_generic   SDL_BroadcastCondition
#endif

typedef struct SDL_cond_generic
{
    SDL_Mutex *lock;
    int waiting;
    int signals;
    SDL_Semaphore *wait_sem;
    SDL_Semaphore *wait_done;
} SDL_cond_generic;

// Create a condition variable
SDL_Condition *SDL_CreateCondition_generic(void)
{
    SDL_cond_generic *cond = (SDL_cond_generic *)SDL_calloc(1, sizeof(*cond));

#ifndef SDL_THREADS_DISABLED
    if (cond) {
        cond->lock = SDL_CreateMutex();
        cond->wait_sem = SDL_CreateSemaphore(0);
        cond->wait_done = SDL_CreateSemaphore(0);
        cond->waiting = cond->signals = 0;
        if (!cond->lock || !cond->wait_sem || !cond->wait_done) {
            SDL_DestroyCondition_generic((SDL_Condition *)cond);
            cond = NULL;
        }
    }
#endif

    return (SDL_Condition *)cond;
}

// Destroy a condition variable
void SDL_DestroyCondition_generic(SDL_Condition *_cond)
{
    SDL_cond_generic *cond = (SDL_cond_generic *)_cond;
    if (cond) {
        if (cond->wait_sem) {
            SDL_DestroySemaphore(cond->wait_sem);
        }
        if (cond->wait_done) {
            SDL_DestroySemaphore(cond->wait_done);
        }
        if (cond->lock) {
            SDL_DestroyMutex(cond->lock);
        }
        SDL_free(cond);
    }
}

// Restart one of the threads that are waiting on the condition variable
void SDL_SignalCondition_generic(SDL_Condition *_cond)
{
    SDL_cond_generic *cond = (SDL_cond_generic *)_cond;
    if (!cond) {
        return;
    }

#ifndef SDL_THREADS_DISABLED
    /* If there are waiting threads not already signalled, then
       signal the condition and wait for the thread to respond.
     */
    SDL_LockMutex(cond->lock);
    if (cond->waiting > cond->signals) {
        ++cond->signals;
        SDL_SignalSemaphore(cond->wait_sem);
        SDL_UnlockMutex(cond->lock);
        SDL_WaitSemaphore(cond->wait_done);
    } else {
        SDL_UnlockMutex(cond->lock);
    }
#endif
}

// Restart all threads that are waiting on the condition variable
void SDL_BroadcastCondition_generic(SDL_Condition *_cond)
{
    SDL_cond_generic *cond = (SDL_cond_generic *)_cond;
    if (!cond) {
        return;
    }

#ifndef SDL_THREADS_DISABLED
    /* If there are waiting threads not already signalled, then
       signal the condition and wait for the thread to respond.
     */
    SDL_LockMutex(cond->lock);
    if (cond->waiting > cond->signals) {
        int i, num_waiting;

        num_waiting = (cond->waiting - cond->signals);
        cond->signals = cond->waiting;
        for (i = 0; i < num_waiting; ++i) {
            SDL_SignalSemaphore(cond->wait_sem);
        }
        /* Now all released threads are blocked here, waiting for us.
           Collect them all (and win fabulous prizes!) :-)
         */
        SDL_UnlockMutex(cond->lock);
        for (i = 0; i < num_waiting; ++i) {
            SDL_WaitSemaphore(cond->wait_done);
        }
    } else {
        SDL_UnlockMutex(cond->lock);
    }
#endif
}

/* Wait on the condition variable for at most 'timeoutNS' nanoseconds.
   The mutex must be locked before entering this function!
   The mutex is unlocked during the wait, and locked again after the wait.

Typical use:

Thread A:
    SDL_LockMutex(lock);
    while ( ! condition ) {
        SDL_WaitCondition(cond, lock);
    }
    SDL_UnlockMutex(lock);

Thread B:
    SDL_LockMutex(lock);
    ...
    condition = true;
    ...
    SDL_SignalCondition(cond);
    SDL_UnlockMutex(lock);
 */
bool SDL_WaitConditionTimeoutNS_generic(SDL_Condition *_cond, SDL_Mutex *mutex, Sint64 timeoutNS)
{
    SDL_cond_generic *cond = (SDL_cond_generic *)_cond;
    bool result = true;

    if (!cond || !mutex) {
        return true;
    }

#ifndef SDL_THREADS_DISABLED
    /* Obtain the protection mutex, and increment the number of waiters.
       This allows the signal mechanism to only perform a signal if there
       are waiting threads.
     */
    SDL_LockMutex(cond->lock);
    ++cond->waiting;
    SDL_UnlockMutex(cond->lock);

    // Unlock the mutex, as is required by condition variable semantics
    SDL_UnlockMutex(mutex);

    // Wait for a signal
    result = SDL_WaitSemaphoreTimeoutNS(cond->wait_sem, timeoutNS);

    /* Let the signaler know we have completed the wait, otherwise
       the signaler can race ahead and get the condition semaphore
       if we are stopped between the mutex unlock and semaphore wait,
       giving a deadlock.  See the following URL for details:
       http://web.archive.org/web/20010914175514/http://www-classic.be.com/aboutbe/benewsletter/volume_III/Issue40.html#Workshop
     */
    SDL_LockMutex(cond->lock);
    if (cond->signals > 0) {
        // If we timed out, we need to eat a condition signal
        if (!result) {
            SDL_WaitSemaphore(cond->wait_sem);
        }
        // We always notify the signal thread that we are done
        SDL_SignalSemaphore(cond->wait_done);

        // Signal handshake complete
        --cond->signals;
    }
    --cond->waiting;
    SDL_UnlockMutex(cond->lock);

    // Lock the mutex, as is required by condition variable semantics
    SDL_LockMutex(mutex);
#endif

    return result;
}

#ifndef SDL_THREAD_GENERIC_COND_SUFFIX
bool SDL_WaitConditionTimeoutNS(SDL_Condition *cond, SDL_Mutex *mutex, Sint64 timeoutNS)
{
    return SDL_WaitConditionTimeoutNS_generic(cond, mutex, timeoutNS);
}
#endif
