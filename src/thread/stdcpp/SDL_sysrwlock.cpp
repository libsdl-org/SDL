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

#include <shared_mutex>
#include <system_error>
#include <Windows.h>

struct SDL_RWLock
{
    std::shared_mutex cpp_mutex;
    SDL_threadID write_owner;
};

extern "C" SDL_RWLock *SDL_CreateRWLock(void)
{
    try {
        SDL_RWLock *rwlock = new SDL_RWLock;
        return rwlock;
    } catch (std::system_error &ex) {
        SDL_SetError("unable to create a C++ rwlock: code=%d; %s", ex.code(), ex.what());
        return NULL;
    } catch (std::bad_alloc &) {
        SDL_OutOfMemory();
        return NULL;
    }
}

extern "C" void SDL_DestroyRWLock(SDL_RWLock *rwlock)
{
    if (rwlock) {
        delete rwlock;
    }
}

extern "C" void SDL_LockRWLockForReading(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    if (rwlock) {
        try {
            rwlock->cpp_mutex.lock_shared();
        } catch (std::system_error &ex) {
            SDL_assert(!"Error trying to lock rwlock for reading");  // assume we're in a lot of trouble if this assert fails.
            //return SDL_SetError("unable to lock a C++ rwlock: code=%d; %s", ex.code(), ex.what());
        }
    }
}

extern "C" void SDL_LockRWLockForWriting(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS // clang doesn't know about NULL mutexes
{
    if (rwlock) {
        try {
            rwlock->cpp_mutex.lock();
            rwlock->write_owner = SDL_ThreadID();
        } catch (std::system_error &ex) {
            SDL_assert(!"Error trying to lock rwlock for writing");  // assume we're in a lot of trouble if this assert fails.
            //return SDL_SetError("unable to lock a C++ rwlock: code=%d; %s", ex.code(), ex.what());
        }
    }
}

extern "C" int SDL_TryLockRWLockForReading(SDL_RWLock *rwlock)
{
    int retval = 0;
    if (rwlock) {
        if (rwlock->cpp_mutex.try_lock_shared() == false) {
            retval = SDL_RWLOCK_TIMEDOUT;
        }
    }
    return retval;
}

extern "C" int SDL_TryLockRWLockForWriting(SDL_RWLock *rwlock)
{
    int retval = 0;
    if (rwlock) {
        if (rwlock->cpp_mutex.try_lock() == false) {
            retval = SDL_RWLOCK_TIMEDOUT;
        } else {
            rwlock->write_owner = SDL_ThreadID();
        }
    }
    return retval;
}

extern "C" void SDL_UnlockRWLock(SDL_RWLock *rwlock) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    if (rwlock) {
        if (rwlock->write_owner == SDL_ThreadID()) {
            rwlock->write_owner = 0;
            rwlock->cpp_mutex.unlock();
        } else {
            rwlock->cpp_mutex.unlock_shared();
        }
    }
}

