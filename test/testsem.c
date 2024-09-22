/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL semaphore code */

#include <signal.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#define NUM_THREADS 10
/* This value should be smaller than the maximum count of the */
/* semaphore implementation: */
#define NUM_OVERHEAD_OPS      10000
#define NUM_OVERHEAD_OPS_MULT 10

static SDL_Semaphore *sem;
static int alive;

typedef struct Thread_State
{
    SDL_Thread *thread;
    int number;
    bool flag;
    int loop_count;
    int content_count;
} Thread_State;

static void log_usage(char *progname, SDLTest_CommonState *state) {
    static const char *options[] = { "[--no-threads]", "init_value", NULL };
    SDLTest_CommonLogUsage(state, progname, options);
}

static void
killed(int sig)
{
    alive = 0;
}

static int SDLCALL
ThreadFuncRealWorld(void *data)
{
    Thread_State *state = (Thread_State *)data;
    while (alive) {
        SDL_WaitSemaphore(sem);
        SDL_Log("Thread number %d has got the semaphore (value = %" SDL_PRIu32 ")!\n",
                state->number, SDL_GetSemaphoreValue(sem));
        SDL_Delay(200);
        SDL_SignalSemaphore(sem);
        SDL_Log("Thread number %d has released the semaphore (value = %" SDL_PRIu32 ")!\n",
                state->number, SDL_GetSemaphoreValue(sem));
        ++state->loop_count;
        SDL_Delay(1); /* For the scheduler */
    }
    SDL_Log("Thread number %d exiting.\n", state->number);
    return 0;
}

static void
TestRealWorld(int init_sem)
{
    Thread_State thread_states[NUM_THREADS] = { { 0 } };
    int i;
    int loop_count;

    sem = SDL_CreateSemaphore(init_sem);

    SDL_Log("Running %d threads, semaphore value = %d\n", NUM_THREADS,
            init_sem);
    alive = 1;
    /* Create all the threads */
    for (i = 0; i < NUM_THREADS; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Thread%u", (unsigned int)i);
        thread_states[i].number = i;
        thread_states[i].thread = SDL_CreateThread(ThreadFuncRealWorld, name, (void *)&thread_states[i]);
    }

    /* Wait 10 seconds */
    SDL_Delay(10 * 1000);

    /* Wait for all threads to finish */
    SDL_Log("Waiting for threads to finish\n");
    alive = 0;
    loop_count = 0;
    for (i = 0; i < NUM_THREADS; ++i) {
        SDL_WaitThread(thread_states[i].thread, NULL);
        loop_count += thread_states[i].loop_count;
    }
    SDL_Log("Finished waiting for threads, ran %d loops in total\n\n", loop_count);

    SDL_DestroySemaphore(sem);
}

static void
TestWaitTimeout(void)
{
    Uint64 start_ticks;
    Uint64 end_ticks;
    Uint64 duration;
    bool result;

    sem = SDL_CreateSemaphore(0);
    SDL_Log("Waiting 2 seconds on semaphore\n");

    start_ticks = SDL_GetTicks();
    result = SDL_WaitSemaphoreTimeout(sem, 2000);
    end_ticks = SDL_GetTicks();

    duration = end_ticks - start_ticks;

    /* Accept a little offset in the effective wait */
    SDL_Log("Wait took %" SDL_PRIu64 " milliseconds\n\n", duration);
    SDL_assert(duration > 1900 && duration < 2050);

    /* Check to make sure the return value indicates timed out */
    if (result) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_WaitSemaphoreTimeout returned: %d; expected: false\n\n", result);
    }

    SDL_DestroySemaphore(sem);
}

static void
TestOverheadUncontended(void)
{
    Uint64 start_ticks;
    Uint64 end_ticks;
    Uint64 duration;
    int i, j;

    sem = SDL_CreateSemaphore(0);
    SDL_Log("Doing %d uncontended Post/Wait operations on semaphore\n", NUM_OVERHEAD_OPS * NUM_OVERHEAD_OPS_MULT);

    start_ticks = SDL_GetTicks();
    for (i = 0; i < NUM_OVERHEAD_OPS_MULT; i++) {
        for (j = 0; j < NUM_OVERHEAD_OPS; j++) {
            SDL_SignalSemaphore(sem);
        }
        for (j = 0; j < NUM_OVERHEAD_OPS; j++) {
            SDL_WaitSemaphore(sem);
        }
    }
    end_ticks = SDL_GetTicks();

    duration = end_ticks - start_ticks;
    SDL_Log("Took %" SDL_PRIu64 " milliseconds\n\n", duration);

    SDL_DestroySemaphore(sem);
}

static int SDLCALL
ThreadFuncOverheadContended(void *data)
{
    Thread_State *state = (Thread_State *)data;

    if (state->flag) {
        while (alive) {
            if (!SDL_TryWaitSemaphore(sem)) {
                ++state->content_count;
            }
            ++state->loop_count;
        }
    } else {
        while (alive) {
            /* Timeout needed to allow check on alive flag */
            if (!SDL_WaitSemaphoreTimeout(sem, 50)) {
                ++state->content_count;
            }
            ++state->loop_count;
        }
    }
    return 0;
}

static void
TestOverheadContended(bool try_wait)
{
    Uint64 start_ticks;
    Uint64 end_ticks;
    Uint64 duration;
    Thread_State thread_states[NUM_THREADS] = { { 0 } };
    char textBuffer[1024];
    int loop_count;
    int content_count;
    int i, j;
    size_t len;

    sem = SDL_CreateSemaphore(0);
    SDL_Log("Doing %d contended %s operations on semaphore using %d threads\n",
            NUM_OVERHEAD_OPS * NUM_OVERHEAD_OPS_MULT, try_wait ? "Post/TryWait" : "Post/WaitTimeout", NUM_THREADS);
    alive = 1;
    /* Create multiple threads to starve the semaphore and cause contention */
    for (i = 0; i < NUM_THREADS; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Thread%u", (unsigned int)i);
        thread_states[i].flag = try_wait;
        thread_states[i].thread = SDL_CreateThread(ThreadFuncOverheadContended, name, (void *)&thread_states[i]);
    }

    start_ticks = SDL_GetTicks();
    for (i = 0; i < NUM_OVERHEAD_OPS_MULT; i++) {
        for (j = 0; j < NUM_OVERHEAD_OPS; j++) {
            SDL_SignalSemaphore(sem);
        }
        /* Make sure threads consumed everything */
        while (SDL_GetSemaphoreValue(sem)) {
            /* Friendlier with cooperative threading models */
            SDL_DelayNS(1);
        }
    }
    end_ticks = SDL_GetTicks();

    alive = 0;
    loop_count = 0;
    content_count = 0;
    for (i = 0; i < NUM_THREADS; ++i) {
        SDL_WaitThread(thread_states[i].thread, NULL);
        loop_count += thread_states[i].loop_count;
        content_count += thread_states[i].content_count;
    }
    SDL_assert_release((loop_count - content_count) == NUM_OVERHEAD_OPS * NUM_OVERHEAD_OPS_MULT);

    duration = end_ticks - start_ticks;
    SDL_Log("Took %" SDL_PRIu64 " milliseconds, threads %s %d out of %d times in total (%.2f%%)\n",
            duration, try_wait ? "where contended" : "timed out", content_count,
            loop_count, ((float)content_count * 100) / loop_count);
    /* Print how many semaphores where consumed per thread */
    (void)SDL_snprintf(textBuffer, sizeof(textBuffer), "{ ");
    for (i = 0; i < NUM_THREADS; ++i) {
        if (i > 0) {
            len = SDL_strlen(textBuffer);
            (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, ", ");
        }
        len = SDL_strlen(textBuffer);
        (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, "%d", thread_states[i].loop_count - thread_states[i].content_count);
    }
    len = SDL_strlen(textBuffer);
    (void)SDL_snprintf(textBuffer + len, sizeof(textBuffer) - len, " }\n");
    SDL_Log("%s\n", textBuffer);

    SDL_DestroySemaphore(sem);
}

int main(int argc, char **argv)
{
    int arg_count = 0;
    int i;
    int init_sem = 0;
    bool enable_threads = true;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--no-threads") == 0) {
                enable_threads = false;
                consumed = 1;
            } else if (arg_count == 0) {
                char *endptr;
                init_sem = SDL_strtol(argv[i], &endptr, 0);
                if (endptr != argv[i] && *endptr == '\0') {
                    arg_count++;
                    consumed = 1;
                }
            }
        }
        if (consumed <= 0) {
            log_usage(argv[0], state);
            return 1;
        }

        i += consumed;
    }

    if (arg_count != 1) {
        log_usage(argv[0], state);
        return 1;
    }

    /* Load the SDL library */
    if (!SDL_Init(0)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    (void)signal(SIGTERM, killed);
    (void)signal(SIGINT, killed);

    if (enable_threads) {
        if (init_sem > 0) {
            TestRealWorld(init_sem);
        }

        TestWaitTimeout();
    }

    TestOverheadUncontended();

    if (enable_threads) {
        TestOverheadContended(false);

        TestOverheadContended(true);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
