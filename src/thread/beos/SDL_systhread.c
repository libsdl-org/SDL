/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_THREAD_BEOS

/* BeOS thread management routines for SDL */

#include <stdio.h>
#include <signal.h>
#include <be/kernel/OS.h>

#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"


static int sig_list[] = {
    SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM, SIGWINCH, 0
};

void
SDL_MaskSignals(sigset_t * omask)
{
    sigset_t mask;
    int i;

    sigemptyset(&mask);
    for (i = 0; sig_list[i]; ++i) {
        sigaddset(&mask, sig_list[i]);
    }
    sigprocmask(SIG_BLOCK, &mask, omask);
}

void
SDL_UnmaskSignals(sigset_t * omask)
{
    sigprocmask(SIG_SETMASK, omask, NULL);
}

static int32
RunThread(void *data)
{
    SDL_RunThread(data);
    return (0);
}

int
SDL_SYS_CreateThread(SDL_Thread * thread, void *args)
{
    /* The docs say the thread name can't be longer than B_OS_NAME_LENGTH. */
    const char *threadname = thread->name ? thread->name : "SDL Thread";
    char name[B_OS_NAME_LENGTH];
    SDL_snprintf(name, sizeof (name), "%s", threadname);
    name[sizeof (name) - 1] = '\0';

    /* Create the thread and go! */
    thread->handle = spawn_thread(RunThread, name, B_NORMAL_PRIORITY, args);
    if ((thread->handle == B_NO_MORE_THREADS) ||
        (thread->handle == B_NO_MEMORY)) {
        return SDL_SetError("Not enough resources to create thread");
    }
    resume_thread(thread->handle);
    return (0);
}

void
SDL_SYS_SetupThread(const char *name)
{
    /* We set the thread name during SDL_SYS_CreateThread(). */
    /* Mask asynchronous signals for this thread */
    SDL_MaskSignals(NULL);
}

SDL_threadID
SDL_ThreadID(void)
{
    return ((SDL_threadID) find_thread(NULL));
}

int
SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    int32 value;

    if (priority == SDL_THREAD_PRIORITY_LOW) {
        value = B_LOW_PRIORITY;
    } else if (priority == SDL_THREAD_PRIORITY_HIGH) {
        value = B_URGENT_DISPLAY_PRIORITY;
    } else {
        value = B_NORMAL_PRIORITY;
    }
    set_thread_priority(find_thread(NULL), value);
    return 0;
}

void
SDL_SYS_WaitThread(SDL_Thread * thread)
{
    status_t the_status;

    wait_for_thread(thread->handle, &the_status);
}

#endif /* SDL_THREAD_BEOS */

/* vi: set ts=4 sw=4 expandtab: */
