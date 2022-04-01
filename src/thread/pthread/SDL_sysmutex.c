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
#include "../../SDL_internal.h"

#include <errno.h>
#include <pthread.h>

#include "SDL_thread.h"

#if !SDL_THREAD_PTHREAD_RECURSIVE_MUTEX && \
    !SDL_THREAD_PTHREAD_RECURSIVE_MUTEX_NP
#define FAKE_RECURSIVE_MUTEX 1
#endif

struct SDL_mutex
{
    pthread_mutex_t id;
#if FAKE_RECURSIVE_MUTEX
    int recursive;
    pthread_t owner;
    pthread_mutex_t rec_id;
    pthread_cond_t rec_cond;
#endif
};

SDL_mutex *
SDL_CreateMutex(void)
{
    SDL_mutex *mutex;
    pthread_mutexattr_t attr;

    /* Allocate the structure */
    mutex = (SDL_mutex *) SDL_calloc(1, sizeof(*mutex));
    if (mutex) {
        pthread_mutexattr_init(&attr);
#if SDL_THREAD_PTHREAD_RECURSIVE_MUTEX
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#elif SDL_THREAD_PTHREAD_RECURSIVE_MUTEX_NP
        pthread_mutexattr_setkind_np(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
        /* No extra attributes necessary */
#endif
        if (pthread_mutex_init(&mutex->id, &attr) != 0) {
            SDL_SetError("pthread_mutex_init() failed");
            SDL_free(mutex);
            mutex = NULL;
        }
#ifdef FAKE_RECURSIVE_MUTEX
        if (mutex && pthread_mutex_init(&mutex->rec_id, &attr) != 0) {
            SDL_SetError("pthread_mutex_init() failed");
            SDL_free(mutex);
            mutex = NULL;
        }
        if (mutex && pthread_cond_init(&mutex->rec_cond, NULL) != 0) {
            SDL_SetError("pthread_cond_init() failed");
            SDL_free(mutex);
            mutex = NULL;
        }
#endif
    } else {
        SDL_OutOfMemory();
    }
    return (mutex);
}

void
SDL_DestroyMutex(SDL_mutex * mutex)
{
    if (mutex) {
        pthread_mutex_destroy(&mutex->id);
#ifdef FAKE_RECURSIVE_MUTEX
        pthread_mutex_destroy(&mutex->rec_id);
        pthread_cond_destroy(&mutex->rec_cond);
#endif
        SDL_free(mutex);
    }
}

/* Lock the mutex */
int
SDL_LockMutex(SDL_mutex * mutex)
{
#if FAKE_RECURSIVE_MUTEX
    pthread_t this_thread;
#endif

    if (mutex == NULL) {
        return SDL_InvalidParamError("mutex");
    }

#if FAKE_RECURSIVE_MUTEX
    this_thread = pthread_self();
    if (pthread_mutex_lock(&mutex->rec_id) == 0) {
        while (mutex->owner && mutex->owner != this_thread) {
            pthread_cond_wait(&mutex->rec_cond, &mutex->rec_id);
        }
        if (mutex->recursive == 0) {
            pthread_mutex_lock(&mutex->id);
        }
        mutex->owner = this_thread;
        ++mutex->recursive;
        pthread_mutex_unlock(&mutex->rec_id);
    } else {
        return SDL_SetError("pthread_mutex_lock() failed");
    }
#else
    if (pthread_mutex_lock(&mutex->id) != 0) {
        return SDL_SetError("pthread_mutex_lock() failed");
    }
#endif
    return 0;
}

int
SDL_TryLockMutex(SDL_mutex * mutex)
{
    int retval;
    int result;
#if FAKE_RECURSIVE_MUTEX
    pthread_t this_thread;
#endif

    if (mutex == NULL) {
        return SDL_InvalidParamError("mutex");
    }

    retval = 0;
#if FAKE_RECURSIVE_MUTEX
    this_thread = pthread_self();
    result = pthread_mutex_trylock(&mutex->rec_id);
    if (result == 0) {
        if (mutex->owner && mutex->owner != this_thread) {
            pthread_mutex_unlock(&mutex->rec_id);
            return SDL_MUTEX_TIMEDOUT;
        }
        if (mutex->recursive == 0) {
            pthread_mutex_lock(&mutex->id);
        }
        mutex->owner = this_thread;
        ++mutex->recursive;
        pthread_mutex_unlock(&mutex->rec_id);
    } else if (result == EBUSY) {
        retval = SDL_MUTEX_TIMEDOUT;
    } else {
        retval = SDL_SetError("pthread_mutex_trylock() failed");
    }
#else
    result = pthread_mutex_trylock(&mutex->id);
    if (result != 0) {
        if (result == EBUSY) {
            retval = SDL_MUTEX_TIMEDOUT;
        } else {
            retval = SDL_SetError("pthread_mutex_trylock() failed");
        }
    }
#endif
    return retval;
}

int
SDL_UnlockMutex(SDL_mutex * mutex)
{
    if (mutex == NULL) {
        return SDL_InvalidParamError("mutex");
    }

#if FAKE_RECURSIVE_MUTEX
    if (pthread_mutex_lock(&mutex->rec_id) == 0) {
        SDL_assert(mutex->owner == pthread_self());
        --mutex->recursive;
        SDL_assert(mutex->recursive >= 0);
        if (mutex->recursive == 0) {
            mutex->owner = 0;
            pthread_mutex_unlock(&mutex->id);
            pthread_cond_signal(&mutex->rec_cond);
        }
        pthread_mutex_unlock(&mutex->rec_id);
    } else {
        return SDL_SetError("pthread_mutex_lock() failed");
    }
#else
    if (pthread_mutex_unlock(&mutex->id) != 0) {
        return SDL_SetError("pthread_mutex_unlock() failed");
    }
#endif /* FAKE_RECURSIVE_MUTEX */

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
