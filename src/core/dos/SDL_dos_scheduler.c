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

#ifdef SDL_PLATFORM_DOS

#include "SDL_dos.h"
#include "SDL_dos_scheduler.h"
#include <setjmp.h>

/* DJGPP's jmp_buf is defined as:
   typedef struct __jmp_buf {
     unsigned long __eax, __ebx, __ecx, __edx, __esi;
     unsigned long __edi, __ebp, __esp, __eip, __eflags;
     unsigned short __cs, __ds, __es, __fs, __gs, __ss;
     unsigned long __sigmask, __signum, __exception_ptr;
     unsigned char __fpu_state[108];
   } jmp_buf[1];
   We patch __esp, __ebp, and __eip to bootstrap new thread contexts. */

// Thread table — static array, no dynamic allocation needed
static DOS_ThreadContext threads[DOS_MAX_THREADS];
static int current_thread = 0; // Index of currently running thread
static bool scheduler_initialized = false;

// Find the next runnable thread using round-robin scheduling.
// Returns thread ID, or -1 if no other thread is runnable.
static int FindNextRunnable(int start)
{
    for (int i = 1; i <= DOS_MAX_THREADS; i++) {
        int idx = (start + i) % DOS_MAX_THREADS;
        if (threads[idx].state == DOS_THREAD_READY) {
            return idx;
        }
    }
    return -1; // No other thread is runnable
}

// Trampoline function that runs on a new thread's stack.
// This is jumped to from DOS_Yield when the new thread runs for the first time.
// We use a global to pass the thread ID since we just switched stacks.
static volatile int trampoline_thread_id;

static void ThreadTrampoline(void)
{
    int tid = trampoline_thread_id;
    DOS_ThreadContext *ctx = &threads[tid];

    // Run the user's thread function
    int result = ctx->entry_fn(ctx->entry_arg);

    // Thread is done
    DOS_ExitThread(result);

    // Should never reach here
    for (;;) {
    }
}

void DOS_SchedulerInit(void)
{
    if (scheduler_initialized) {
        return;
    }

    SDL_memset(threads, 0, sizeof(threads));

    // Thread 0 is the main thread (already running)
    threads[0].state = DOS_THREAD_RUNNING;
    threads[0].id = 0;
    threads[0].join_waiter = -1;
    current_thread = 0;

    scheduler_initialized = true;

    _go32_dpmi_lock_data((void *)threads, sizeof(threads));
    _go32_dpmi_lock_data((void *)&current_thread, sizeof(current_thread));
    _go32_dpmi_lock_data((void *)&scheduler_initialized, sizeof(scheduler_initialized));
    _go32_dpmi_lock_data((void *)&trampoline_thread_id, sizeof(trampoline_thread_id));
}

void DOS_SchedulerQuit(void)
{
    // Clean up any remaining threads
    for (int i = 1; i < DOS_MAX_THREADS; i++) {
        if (threads[i].state != DOS_THREAD_FREE) {
            SDL_assert(threads[i].state == DOS_THREAD_FINISHED);
            DOS_DestroyThread(i);
        }
    }

    scheduler_initialized = false;
}

int DOS_CreateThread(int (*fn)(void *), void *arg, size_t stack_size)
{
    if (!scheduler_initialized) {
        return -1;
    }

    // Find a free slot
    int tid = -1;
    for (int i = 1; i < DOS_MAX_THREADS; i++) {
        if (threads[i].state == DOS_THREAD_FREE) {
            tid = i;
            break;
        }
    }

    if (tid < 0) {
        return -1; // No free slots
    }

    if (stack_size == 0) {
        stack_size = DOS_DEFAULT_STACK_SIZE;
    }

    // Allocate stack
    void *stack = SDL_malloc(stack_size);
    if (!stack) {
        return -1;
    }

    // Lock the stack memory so it won't be paged out.
    // This is important for context switches — we can't take a page fault
    // while switching stacks.
    _go32_dpmi_lock_data(stack, stack_size);

    DOS_ThreadContext *ctx = &threads[tid];
    SDL_memset(ctx, 0, sizeof(*ctx));
    ctx->id = tid;
    ctx->state = DOS_THREAD_READY;
    ctx->stack_base = stack;
    ctx->stack_size = stack_size;
    ctx->entry_fn = fn;
    ctx->entry_arg = arg;
    ctx->finished = false;
    ctx->join_waiter = -1;

    // Set up the initial context. We use setjmp to save a template context,
    // then modify the stack pointer to point to our new stack.
    //
    // The trick: we setjmp here to capture register state, then manually
    // patch the saved __esp and __eip in the jmp_buf struct to point to
    // our new stack and trampoline function.
    if (setjmp(ctx->env) == 0) {
        // Patch the saved context to use our new stack and trampoline.
        // Stack grows downward, so SP starts at the top.
        // Align to 16 bytes for ABI compliance.
        Uint8 *stack_top = (Uint8 *)stack + stack_size;
        stack_top = (Uint8 *)((uintptr_t)stack_top & ~0xFUL); // 16-byte align

        // Leave room for a fake return address (the trampoline never returns,
        // but the ABI expects one on the stack at function entry)
        stack_top -= sizeof(void *);
        *(void **)stack_top = NULL; // Fake return address

        ctx->env[0].__esp = (unsigned long)(uintptr_t)stack_top;
        ctx->env[0].__ebp = (unsigned long)(uintptr_t)stack_top;
        ctx->env[0].__eip = (unsigned long)(uintptr_t)ThreadTrampoline;
    } else {
        SDL_assert(!"Unreachable");
    }

    return tid;
}

void DOS_Yield(void)
{
    if (!scheduler_initialized) {
        return;
    }

    int next = FindNextRunnable(current_thread);
    if (next < 0) {
        return; // No other runnable thread, continue current
    }

    int prev = current_thread;

    // Save current context and switch
    if (setjmp(threads[prev].env) == 0) {
        // Mark previous thread as READY (unless it's BLOCKED or FINISHED)
        if (threads[prev].state == DOS_THREAD_RUNNING) {
            threads[prev].state = DOS_THREAD_READY;
        }

        // Switch to next thread
        current_thread = next;
        threads[next].state = DOS_THREAD_RUNNING;

        // For new threads that haven't run yet, set the trampoline ID
        trampoline_thread_id = next;

        longjmp(threads[next].env, 1);
    }
    // else: we've been switched back to — just return
}

void DOS_ExitThread(int status)
{
    DOS_ThreadContext *ctx = &threads[current_thread];
    ctx->exit_status = status;
    ctx->finished = true;
    ctx->state = DOS_THREAD_FINISHED;

    // Wake up anyone waiting to join this thread
    if (ctx->join_waiter >= 0 && ctx->join_waiter < DOS_MAX_THREADS) {
        DOS_WakeThread(ctx->join_waiter);
    }

    // Find another thread to run. We can't return — our stack frame
    // belongs to the thread that just exited.
    int next = FindNextRunnable(current_thread);
    if (next >= 0) {
        current_thread = next;
        threads[next].state = DOS_THREAD_RUNNING;
        trampoline_thread_id = next;
        longjmp(threads[next].env, 1);
    }

    // If no other thread is runnable and we're not the main thread,
    // this is a problem. Spin-wait for someone to become runnable.
    // (This shouldn't happen in practice — the main thread should
    // always be runnable or waiting.)
    for (;;) {
        __asm__ __volatile__("hlt"); // Wait for interrupt before retrying
        next = FindNextRunnable(current_thread);
        if (next >= 0) {
            current_thread = next;
            threads[next].state = DOS_THREAD_RUNNING;
            trampoline_thread_id = next;
            longjmp(threads[next].env, 1);
        }
    }
}

int DOS_JoinThread(int thread_id)
{
    if (thread_id < 0 || thread_id >= DOS_MAX_THREADS) {
        return -1;
    }

    DOS_ThreadContext *target = &threads[thread_id];

    // If already finished, just return the status
    if (target->finished) {
        return target->exit_status;
    }

    // Register ourselves as the join waiter
    target->join_waiter = current_thread;

    // Block until the target thread finishes
    while (!target->finished) {
        DOS_BlockCurrentThread();
    }

    return target->exit_status;
}

int DOS_GetCurrentThreadID(void)
{
    if (!scheduler_initialized) {
        return 1;
    }
    return current_thread + 1;
}

void DOS_WakeThread(int thread_id)
{
    if (thread_id >= 0 && thread_id < DOS_MAX_THREADS) {
        if (threads[thread_id].state == DOS_THREAD_BLOCKED) {
            threads[thread_id].state = DOS_THREAD_READY;
        }
    }
}

void DOS_BlockCurrentThread(void)
{
    threads[current_thread].state = DOS_THREAD_BLOCKED;
    DOS_Yield();
}

void DOS_DestroyThread(int thread_id)
{
    if (thread_id <= 0 || thread_id >= DOS_MAX_THREADS) {
        return; // Can't destroy main thread (0) or invalid IDs
    }

    DOS_ThreadContext *ctx = &threads[thread_id];
    if (ctx->stack_base) {
        SDL_free(ctx->stack_base);
        ctx->stack_base = NULL;
    }
    ctx->state = DOS_THREAD_FREE;
}

#endif // SDL_PLATFORM_DOS
