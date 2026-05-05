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

#ifndef SDL_dos_scheduler_h_
#define SDL_dos_scheduler_h_

#include "SDL_internal.h"
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of cooperative threads. DOS doesn't need many —
// typically just main thread + audio thread + maybe a loading thread.
#define DOS_MAX_THREADS 16

// Default stack size for new threads (64 KB)
#define DOS_DEFAULT_STACK_SIZE (64 * 1024)

// Thread states
typedef enum
{
    DOS_THREAD_FREE = 0, // Slot is available
    DOS_THREAD_READY,    // Runnable
    DOS_THREAD_RUNNING,  // Currently executing
    DOS_THREAD_BLOCKED,  // Waiting on a semaphore/mutex
    DOS_THREAD_FINISHED  // Thread function returned
} DOS_ThreadState;

// Per-thread context
typedef struct DOS_ThreadContext
{
    jmp_buf env; // Saved CPU state for context switch
    DOS_ThreadState state;
    int id;           // Thread ID (index into thread table)
    void *stack_base; // malloc'd stack memory
    size_t stack_size;
    int exit_status; // Return value from thread function

    // Entry point
    int (*entry_fn)(void *);
    void *entry_arg;

    // Join support
    volatile bool finished; // Set when thread function returns
    int join_waiter;        // ID of thread waiting in WaitThread, or -1
} DOS_ThreadContext;

// Initialize the scheduler. Must be called before any thread operations.
// Registers the calling context as thread 0 (the main thread).
void DOS_SchedulerInit(void);

// Shut down the scheduler.
void DOS_SchedulerQuit(void);

// Create a new thread. Returns thread ID (>0) on success, -1 on failure.
// The thread starts in READY state and will run when yielded to.
int DOS_CreateThread(int (*fn)(void *), void *arg, size_t stack_size);

// Yield the current thread's timeslice. Switches to the next runnable thread
// using round-robin scheduling. If no other thread is runnable, returns
// immediately (no-op). This is the core cooperative scheduling primitive.
void DOS_Yield(void);

// Mark a thread as finished and yield. Called when a thread's entry
// function returns.
void DOS_ExitThread(int status);

// Block until the specified thread finishes. Returns the thread's exit status.
int DOS_JoinThread(int thread_id);

// Get the current thread's ID.
int DOS_GetCurrentThreadID(void);

// Mark a thread as READY (used by semaphore signal to wake a blocked thread).
void DOS_WakeThread(int thread_id);

// Mark the current thread as BLOCKED and yield (used by semaphore wait).
void DOS_BlockCurrentThread(void);

// Destroy a thread's resources (stack, etc). Thread must be FINISHED or FREE.
void DOS_DestroyThread(int thread_id);

#ifdef __cplusplus
}
#endif

#endif // SDL_dos_scheduler_h_
