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

#ifdef SDL_THREAD_N3DS

// An implementation of condition variables using libctru's CondVar

#include "SDL_sysmutex_c.h"

struct SDL_Condition
{
    CondVar cond_variable;
};

// Create a condition variable
SDL_Condition *SDL_CreateCondition(void)
{
    SDL_Condition *cond = (SDL_Condition *)SDL_malloc(sizeof(SDL_Condition));
    if (cond) {
        CondVar_Init(&cond->cond_variable);
    }
    return cond;
}

// Destroy a condition variable
void SDL_DestroyCondition(SDL_Condition *cond)
{
    if (cond) {
        SDL_free(cond);
    }
}

// Restart one of the threads that are waiting on the condition variable
void SDL_SignalCondition(SDL_Condition *cond)
{
    if (!cond) {
        return;
    }

    CondVar_Signal(&cond->cond_variable);
}

// Restart all threads that are waiting on the condition variable
void SDL_BroadcastCondition(SDL_Condition *cond)
{
    if (!cond) {
        return;
    }

    CondVar_Broadcast(&cond->cond_variable);
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
bool SDL_WaitConditionTimeoutNS(SDL_Condition *cond, SDL_Mutex *mutex, Sint64 timeoutNS)
{
    Result res;

    if (!cond || !mutex) {
        return true;
    }

    res = 0;
    if (timeoutNS < 0) {
        CondVar_Wait(&cond->cond_variable, &mutex->lock.lock);
    } else {
        res = CondVar_WaitTimeout(&cond->cond_variable, &mutex->lock.lock, timeoutNS);
    }

    return R_SUCCEEDED(res);
}

#endif // SDL_THREAD_N3DS
