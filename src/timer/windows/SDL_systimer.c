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

#ifdef SDL_TIMER_WINDOWS

#include "../../core/windows/SDL_windows.h"


#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
static void SDL_CleanupWaitableTimer(void *timer)
{
    CloseHandle(timer);
}

HANDLE SDL_GetWaitableTimer()
{
    static SDL_TLSID TLS_timer_handle;
    HANDLE timer;

    if (!TLS_timer_handle) {
        TLS_timer_handle = SDL_CreateTLS();
    }
    timer = SDL_GetTLS(TLS_timer_handle);
    if (!timer) {
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (timer) {
            SDL_SetTLS(TLS_timer_handle, timer, SDL_CleanupWaitableTimer);
        }
    }
    return timer;
}
#endif /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */

Uint64 SDL_GetPerformanceCounter(void)
{
    LARGE_INTEGER counter;
    const BOOL rc = QueryPerformanceCounter(&counter);
    SDL_assert(rc != 0); /* this should _never_ fail if you're on XP or later. */
    return (Uint64)counter.QuadPart;
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    LARGE_INTEGER frequency;
    const BOOL rc = QueryPerformanceFrequency(&frequency);
    SDL_assert(rc != 0); /* this should _never_ fail if you're on XP or later. */
    return (Uint64)frequency.QuadPart;
}

void SDL_DelayNS(Uint64 ns)
{
    /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION flag was added in Windows 10 version 1803.
     *
     * Sleep() is not publicly available to apps in early versions of WinRT.
     *
     * Visual C++ 2013 Update 4 re-introduced Sleep() for Windows 8.1 and
     * Windows Phone 8.1.
     *
     * Use the compiler version to determine availability.
     *
     * NOTE #1: _MSC_FULL_VER == 180030723 for Visual C++ 2013 Update 3.
     * NOTE #2: Visual C++ 2013, when compiling for Windows 8.0 and
     *    Windows Phone 8.0, uses the Visual C++ 2012 compiler to build
     *    apps and libraries.
     */
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    HANDLE timer = SDL_GetWaitableTimer();
    if (timer) {
        LARGE_INTEGER due_time;
        due_time.QuadPart = -((LONGLONG)ns / 100);
        if (SetWaitableTimerEx(timer, &due_time, 0, NULL, NULL, NULL, 0)) {
            WaitForSingleObject(timer, INFINITE);
        }
        return;
    }
#endif

    {
        const Uint64 max_delay = 0xffffffffLL * SDL_NS_PER_MS;
        if (ns > max_delay) {
            ns = max_delay;
        }

#if defined(__WINRT__) && defined(_MSC_FULL_VER) && (_MSC_FULL_VER <= 180030723)
        static HANDLE mutex = 0;
        if (!mutex) {
            mutex = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
        }
        WaitForSingleObjectEx(mutex, (DWORD)SDL_NS_TO_MS(ns), FALSE);
#else
        Sleep((DWORD)SDL_NS_TO_MS(ns));
#endif
    }
}

#endif /* SDL_TIMER_WINDOWS */
