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

#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"

/* PS3 thread management routines for SDL */

#include <signal.h>
#include <ppu-lv2.h>
#include <sys/thread.h>

static int sig_list[] = {
    SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM, SIGWINCH, 0
};

void SDL_MaskSignals(sigset_t * omask)
{
    sigset_t mask;
    int i;

    sigemptyset(&mask);
    for (i = 0; sig_list[i]; ++i) {
        sigaddset(&mask, sig_list[i]);
    }
    // FIXME as soon as signal are implemented
    // sigprocmask(SIG_BLOCK, &mask, omask);
}

void SDL_UnmaskSignals(sigset_t * omask)
{
    // FIXME as soom as signal are implemented
    //sigprocmask(SIG_SETMASK, omask, NULL);
}

static void RunThread(void *arg)
{
    SDL_Thread *thread = (SDL_Thread *)arg;
    SDL_RunThread(thread);
    sysThreadExit(0);
}

bool SDL_SYS_CreateThread(SDL_Thread * thread, SDL_FunctionPointer pfnBeginThread,
                          SDL_FunctionPointer pfnEndThread)
{
    size_t stack_size = 0x4000;
    u64 priority = 1500;

    /* Create the thread and go! */
    int ret = sysThreadCreate(&thread->handle, RunThread, (void *)thread, priority, stack_size, THREAD_JOINABLE, thread->name);

    if ( ret != 0)
    {
        SDL_SetError("Not enough resources to create thread");
        return false;
    }

    return true;
}

SDL_ThreadID SDL_GetCurrentThreadID(void)
{
    SDL_ThreadID id;
    sysThreadGetId(&id);
    return id;
}

void SDL_SYS_SetupThread(const char *name)
{
    /* Mask asynchronous signals for this thread */
    SDL_MaskSignals(NULL);
    (void)name;
}

void SDL_SYS_WaitThread(SDL_Thread * thread)
{
    u64 exit_code;
    sysThreadJoin(thread->handle, &exit_code);
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    sysThreadDetach(thread->handle);
}

bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    s32 ps3_priority;

    switch (priority) {
        case SDL_THREAD_PRIORITY_LOW:
            ps3_priority = 3000;
            break;
        case SDL_THREAD_PRIORITY_NORMAL:
            ps3_priority = 1000;
            break;
        case SDL_THREAD_PRIORITY_HIGH:
            ps3_priority = 300;
            break;
        case SDL_THREAD_PRIORITY_TIME_CRITICAL:
            ps3_priority = 100;
            break;
        default:
            ps3_priority = 1000;
            break;
    }

    sys_ppu_thread_t tid;
    sysThreadGetId(&tid);
    int ret = sysThreadSetPriority(tid, ps3_priority);
    if (ret != 0) {
        return SDL_SetError("sysThreadSetPrio() failed: %d", ret);
    }
    return true;
}

#endif // SDL_THREAD_PS3

/* vi: set ts=4 sw=4 expandtab: */
