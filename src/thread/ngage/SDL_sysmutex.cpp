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

// An implementation of mutexes using the Symbian API.

#include <e32std.h>

#include "SDL_systhread_c.h"

struct SDL_Mutex
{
    TInt handle;
};

extern TInt CreateUnique(TInt (*aFunc)(const TDesC &aName, TAny *, TAny *), TAny *, TAny *);

static TInt NewMutex(const TDesC &aName, TAny *aPtr1, TAny *)
{
    return ((RMutex *)aPtr1)->CreateGlobal(aName);
}

// Create a mutex
SDL_Mutex *SDL_CreateMutex(void)
{
    RMutex rmutex;

    TInt status = CreateUnique(NewMutex, &rmutex, NULL);
    if (status != KErrNone) {
        SDL_SetError("Couldn't create mutex.");
        return NULL;
    }
    SDL_Mutex *mutex = new /*(ELeave)*/ SDL_Mutex;
    mutex->handle = rmutex.Handle();
    return mutex;
}

// Free the mutex
void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        RMutex rmutex;
        rmutex.SetHandle(mutex->handle);
        rmutex.Signal();
        rmutex.Close();
        delete (mutex);
        mutex = NULL;
    }
}

// Lock the mutex
void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS // clang doesn't know about NULL mutexes
{
    if (mutex) {
        RMutex rmutex;
        rmutex.SetHandle(mutex->handle);
        rmutex.Wait();
    }
}

// Try to lock the mutex
#if 0
bool SDL_TryLockMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        // Not yet implemented.
        return true;
    }
    return true;
}
#endif

// Unlock the mutex
void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS // clang doesn't know about NULL mutexes
{
    if (mutex) {
        RMutex rmutex;
        rmutex.SetHandle(mutex->handle);
        rmutex.Signal();
    }
}

