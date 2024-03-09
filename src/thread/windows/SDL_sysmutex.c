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

#ifdef SDL_THREAD_WINDOWS

/**
 * Mutex functions using the Win32 API
 * There are two implementations available based on:
 * - Critical Sections. Available on all OS versions since Windows XP.
 * - Slim Reader/Writer Locks. Requires Windows 7 or newer.
 * which are chosen at runtime.
 */

#include "SDL_sysmutex_c.h"

/* Implementation will be chosen at runtime based on available Kernel features */
SDL_mutex_impl_t SDL_mutex_impl_active = { 0 };

/**
 * Implementation based on Slim Reader/Writer (SRW) Locks for Win 7 and newer.
 */

#ifdef SDL_PLATFORM_WINRT
/* Functions are guaranteed to be available */
#define pInitializeSRWLock InitializeSRWLock
#define pReleaseSRWLockExclusive    ReleaseSRWLockExclusive
#define pAcquireSRWLockExclusive    AcquireSRWLockExclusive
#define pTryAcquireSRWLockExclusive TryAcquireSRWLockExclusive
#else
typedef VOID(WINAPI *pfnInitializeSRWLock)(PSRWLOCK);
typedef VOID(WINAPI *pfnReleaseSRWLockExclusive)(PSRWLOCK);
typedef VOID(WINAPI *pfnAcquireSRWLockExclusive)(PSRWLOCK);
typedef BOOLEAN(WINAPI *pfnTryAcquireSRWLockExclusive)(PSRWLOCK);
static pfnInitializeSRWLock pInitializeSRWLock = NULL;
static pfnReleaseSRWLockExclusive pReleaseSRWLockExclusive = NULL;
static pfnAcquireSRWLockExclusive pAcquireSRWLockExclusive = NULL;
static pfnTryAcquireSRWLockExclusive pTryAcquireSRWLockExclusive = NULL;
#endif

static SDL_Mutex *SDL_CreateMutex_srw(void)
{
    SDL_mutex_srw *mutex = (SDL_mutex_srw *)SDL_calloc(1, sizeof(*mutex));
    if (mutex) {
        pInitializeSRWLock(&mutex->srw);
    }
    return (SDL_Mutex *)mutex;
}

static void SDL_DestroyMutex_srw(SDL_Mutex *mutex)
{
    /* There are no kernel allocated resources */
    SDL_free(mutex);
}

static void SDL_LockMutex_srw(SDL_Mutex *_mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    SDL_mutex_srw *mutex = (SDL_mutex_srw *)_mutex;
    const DWORD this_thread = GetCurrentThreadId();

    if (mutex->owner == this_thread) {
        ++mutex->count;
    } else {
        /* The order of operations is important.
           We set the locking thread id after we obtain the lock
           so unlocks from other threads will fail.
         */
        pAcquireSRWLockExclusive(&mutex->srw);
        SDL_assert(mutex->count == 0 && mutex->owner == 0);
        mutex->owner = this_thread;
        mutex->count = 1;
    }
}

static int SDL_TryLockMutex_srw(SDL_Mutex *_mutex)
{
    SDL_mutex_srw *mutex = (SDL_mutex_srw *)_mutex;
    const DWORD this_thread = GetCurrentThreadId();
    int retval = 0;

    if (mutex->owner == this_thread) {
        ++mutex->count;
    } else {
        if (pTryAcquireSRWLockExclusive(&mutex->srw) != 0) {
            SDL_assert(mutex->count == 0 && mutex->owner == 0);
            mutex->owner = this_thread;
            mutex->count = 1;
        } else {
            retval = SDL_MUTEX_TIMEDOUT;
        }
    }
    return retval;
}

static void SDL_UnlockMutex_srw(SDL_Mutex *_mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    SDL_mutex_srw *mutex = (SDL_mutex_srw *)_mutex;

    if (mutex->owner == GetCurrentThreadId()) {
        if (--mutex->count == 0) {
            mutex->owner = 0;
            pReleaseSRWLockExclusive(&mutex->srw);
        }
    } else {
        SDL_assert(!"mutex not owned by this thread");  // undefined behavior...!
    }
}

static const SDL_mutex_impl_t SDL_mutex_impl_srw = {
    &SDL_CreateMutex_srw,
    &SDL_DestroyMutex_srw,
    &SDL_LockMutex_srw,
    &SDL_TryLockMutex_srw,
    &SDL_UnlockMutex_srw,
    SDL_MUTEX_SRW,
};

/**
 * Fallback Mutex implementation using Critical Sections (before Win 7)
 */

static SDL_Mutex *SDL_CreateMutex_cs(void)
{
    SDL_mutex_cs *mutex = (SDL_mutex_cs *)SDL_malloc(sizeof(*mutex));
    if (mutex) {
        // Initialize
        // On SMP systems, a non-zero spin count generally helps performance
#ifdef SDL_PLATFORM_WINRT
        InitializeCriticalSectionEx(&mutex->cs, 2000, 0);
#else
        InitializeCriticalSectionAndSpinCount(&mutex->cs, 2000);
#endif
    }
    return (SDL_Mutex *)mutex;
}

static void SDL_DestroyMutex_cs(SDL_Mutex *mutex_)
{
    SDL_mutex_cs *mutex = (SDL_mutex_cs *)mutex_;
    DeleteCriticalSection(&mutex->cs);
    SDL_free(mutex);
}

static void SDL_LockMutex_cs(SDL_Mutex *mutex_) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    SDL_mutex_cs *mutex = (SDL_mutex_cs *)mutex_;
    EnterCriticalSection(&mutex->cs);
}

static int SDL_TryLockMutex_cs(SDL_Mutex *mutex_)
{
    SDL_mutex_cs *mutex = (SDL_mutex_cs *)mutex_;
    return (TryEnterCriticalSection(&mutex->cs) == 0) ? SDL_MUTEX_TIMEDOUT : 0;
}

static void SDL_UnlockMutex_cs(SDL_Mutex *mutex_) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    SDL_mutex_cs *mutex = (SDL_mutex_cs *)mutex_;
    LeaveCriticalSection(&mutex->cs);
}

static const SDL_mutex_impl_t SDL_mutex_impl_cs = {
    &SDL_CreateMutex_cs,
    &SDL_DestroyMutex_cs,
    &SDL_LockMutex_cs,
    &SDL_TryLockMutex_cs,
    &SDL_UnlockMutex_cs,
    SDL_MUTEX_CS,
};

/**
 * Runtime selection and redirection
 */

SDL_Mutex *SDL_CreateMutex(void)
{
    if (!SDL_mutex_impl_active.Create) {
        // Default to fallback implementation
        const SDL_mutex_impl_t *impl = &SDL_mutex_impl_cs;

        if (!SDL_GetHintBoolean(SDL_HINT_WINDOWS_FORCE_MUTEX_CRITICAL_SECTIONS, SDL_FALSE)) {
#ifdef SDL_PLATFORM_WINRT
            // Link statically on this platform
            impl = &SDL_mutex_impl_srw;
#else
            // Try faster implementation for Windows 7 and newer
            HMODULE kernel32 = GetModuleHandle(TEXT("kernel32.dll"));
            if (kernel32) {
                // Requires Vista:
                pInitializeSRWLock = (pfnInitializeSRWLock)GetProcAddress(kernel32, "InitializeSRWLock");
                pReleaseSRWLockExclusive = (pfnReleaseSRWLockExclusive)GetProcAddress(kernel32, "ReleaseSRWLockExclusive");
                pAcquireSRWLockExclusive = (pfnAcquireSRWLockExclusive)GetProcAddress(kernel32, "AcquireSRWLockExclusive");
                // Requires 7:
                pTryAcquireSRWLockExclusive = (pfnTryAcquireSRWLockExclusive)GetProcAddress(kernel32, "TryAcquireSRWLockExclusive");
                if (pInitializeSRWLock && pReleaseSRWLockExclusive && pAcquireSRWLockExclusive && pTryAcquireSRWLockExclusive) {
                    impl = &SDL_mutex_impl_srw;
                }
            }
#endif
        }

        // Copy instead of using pointer to save one level of indirection
        SDL_copyp(&SDL_mutex_impl_active, impl);
    }
    return SDL_mutex_impl_active.Create();
}

void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        SDL_mutex_impl_active.Destroy(mutex);
    }
}

void SDL_LockMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        SDL_mutex_impl_active.Lock(mutex);
    }
}

int SDL_TryLockMutex(SDL_Mutex *mutex)
{
    return mutex ? SDL_mutex_impl_active.TryLock(mutex) : 0;
}

void SDL_UnlockMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        SDL_mutex_impl_active.Unlock(mutex);
    }
}

#endif // SDL_THREAD_WINDOWS
