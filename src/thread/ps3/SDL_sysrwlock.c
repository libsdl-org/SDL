/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_THREAD_PS3

#include <sys/mutex.h>

// Simple mutex-based rwlock for PS3

struct SDL_RWLock {
    sys_mutex_t id;
};

SDL_RWLock *SDL_CreateRWLock(void)
{
    SDL_RWLock *rwlock = (SDL_RWLock *)SDL_malloc(sizeof(*rwlock));
    if (!rwlock) return NULL;

    sys_mutex_attr_t attr;
    sysMutexAttrInitialize(attr);
    attr.attr_recursive = SYS_MUTEX_ATTR_RECURSIVE;

    int ret = sysMutexCreate(&rwlock->id, &attr);
    if (ret != 0) {
        SDL_free(rwlock);
        return NULL;
    }

    return rwlock;
}

void SDL_DestroyRWLock(SDL_RWLock *rwlock)
{
    if (rwlock) {
        sysMutexDestroy(rwlock->id);
        SDL_free(rwlock);
    }
}

void SDL_LockRWLockForReading(SDL_RWLock *rwlock)
{
    if (rwlock) sysMutexLock(rwlock->id, 0);
}

void SDL_LockRWLockForWriting(SDL_RWLock *rwlock)
{
    if (rwlock) sysMutexLock(rwlock->id, 0);
}

bool SDL_TryLockRWLockForReading(SDL_RWLock *rwlock)
{
    if (!rwlock) return true;
    return sysMutexTryLock(rwlock->id) == 0;
}

bool SDL_TryLockRWLockForWriting(SDL_RWLock *rwlock)
{
    if (!rwlock) return true;
    return sysMutexTryLock(rwlock->id) == 0;
}

void SDL_UnlockRWLock(SDL_RWLock *rwlock)
{
    if (rwlock) sysMutexUnlock(rwlock->id);
}

#endif // SDL_THREAD_PS3

/* vi: set ts=4 sw=4 expandtab: */
