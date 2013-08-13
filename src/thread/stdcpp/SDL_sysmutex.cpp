/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

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

extern "C" {
#include "SDL_thread.h"
#include "SDL_systhread_c.h"
#include "SDL_log.h"
}

#include <exception>

#include "SDL_sysmutex_c.h"
#include <Windows.h>


/* Create a mutex */
extern "C"
SDL_mutex *
SDL_CreateMutex(void)
{
    /* Allocate and initialize the mutex */
    try {
        SDL_mutex * mutex = new SDL_mutex;
        return mutex;
    } catch (std::exception & ex) {
        SDL_SetError("unable to create C++ mutex: %s", ex.what());
        return NULL;
    } catch (...) {
        SDL_SetError("unable to create C++ mutex due to an unknown exception");
        return NULL;
    }
}

/* Free the mutex */
extern "C"
void
SDL_DestroyMutex(SDL_mutex * mutex)
{
    if (mutex) {
        try {
            delete mutex;
        } catch (...) {
            // catch any and all exceptions, just in case something happens
        }
    }
}

/* Lock the semaphore */
extern "C"
int
SDL_mutexP(SDL_mutex * mutex)
{
    SDL_threadID threadID = SDL_ThreadID();
    DWORD realThreadID = GetCurrentThreadId();
    if (mutex == NULL) {
        SDL_SetError("Passed a NULL mutex");
        return -1;
    }

    try {
        mutex->cpp_mutex.lock();
        return 0;
    } catch (std::exception & ex) {
        SDL_SetError("unable to lock C++ mutex: %s", ex.what());
        return -1;
    } catch (...) {
        SDL_SetError("unable to lock C++ mutex due to an unknown exception");
        return -1;
    }
}

/* Unlock the mutex */
extern "C"
int
SDL_mutexV(SDL_mutex * mutex)
{
    SDL_threadID threadID = SDL_ThreadID();
    DWORD realThreadID = GetCurrentThreadId();
    if (mutex == NULL) {
        SDL_SetError("Passed a NULL mutex");
        return -1;
    }

    try {
        mutex->cpp_mutex.unlock();
        return 0;
    } catch (...) {
        // catch any and all exceptions, just in case something happens.
        SDL_SetError("unable to unlock C++ mutex due to an unknown exception");
        return -1;
    }
}

/* vi: set ts=4 sw=4 expandtab: */
