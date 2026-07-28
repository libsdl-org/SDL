/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_THREAD_DOS

/* DOS thread management routines for SDL — cooperative threading via
   the DOS mini-scheduler (no OS-level threads on DOS). */

#include "../../core/dos/SDL_dos_scheduler.h"
#include "../SDL_systhread.h"
#include "../SDL_thread_c.h"

static int ThreadEntry(void *arg)
{
    SDL_Thread *thread = (SDL_Thread *)arg;
    SDL_RunThread(thread);
    return 0;
}

bool SDL_SYS_CreateThread(SDL_Thread *thread,
                          SDL_FunctionPointer pfnBeginThread,
                          SDL_FunctionPointer pfnEndThread)
{
    /* Ensure the scheduler is initialized (idempotent) */
    DOS_SchedulerInit();

    size_t stack_size = thread->stacksize;

    int tid = DOS_CreateThread(ThreadEntry, thread, stack_size);
    if (tid < 0) {
        return SDL_SetError("DOS_CreateThread() failed — no free thread slots");
    }

    thread->handle = tid;
    thread->threadid = (SDL_ThreadID) tid;

    return true;
}

void SDL_SYS_SetupThread(const char *name)
{
    /* Nothing to do on DOS */
}

SDL_ThreadID SDL_GetCurrentThreadID(void)
{
    return (SDL_ThreadID)DOS_GetCurrentThreadID();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    DOS_JoinThread(thread->handle);
    DOS_DestroyThread(thread->handle);
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    /* For cooperative threads, detach is a no-op. The thread will clean
       itself up when it finishes. In practice, SDL's thread code handles
       the detach lifecycle via atomics in SDL_RunThread. */
}

bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    /* DOS cooperative scheduler uses round-robin — priority is not
       meaningful. Accept any value without error. */
    return true;
}

#endif /* SDL_THREAD_DOS */
