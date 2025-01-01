/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test program to check the resolution of the SDL timer on the current
   platform
*/

#include <stdlib.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_test.h"

#define DEFAULT_RESOLUTION 1

static int test_sdl_delay_within_bounds(void) {
    const int testDelay = 100;
    const int marginOfError = 25;
    Uint64 result;
    Uint64 result2;
    Sint64 difference;

    SDLTest_ResetAssertSummary();

    /* Get ticks count - should be non-zero by now */
    result = SDL_GetTicks();
    SDLTest_AssertPass("Call to SDL_GetTicks()");
    SDLTest_AssertCheck(result > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result);

    /* Delay a bit longer and measure ticks and verify difference */
    SDL_Delay(testDelay);
    SDLTest_AssertPass("Call to SDL_Delay(%d)", testDelay);
    result2 = SDL_GetTicks();
    SDLTest_AssertPass("Call to SDL_GetTicks()");
    SDLTest_AssertCheck(result2 > 0, "Check result value, expected: >0, got: %" SDL_PRIu64, result2);
    difference = result2 - result;
    SDLTest_AssertCheck(difference > (testDelay - marginOfError), "Check difference, expected: >%d, got: %" SDL_PRIu64, testDelay - marginOfError, difference);
    /* Disabled because this might fail on non-interactive systems. */
    SDLTest_AssertCheck(difference < (testDelay + marginOfError), "Check difference, expected: <%d, got: %" SDL_PRIu64, testDelay + marginOfError, difference);

    return SDLTest_AssertSummaryToTestResult() == TEST_RESULT_PASSED ? 0 : 1;
}

static int ticks = 0;

static Uint32 SDLCALL
ticktock(Uint32 interval, void *param)
{
    ++ticks;
    return interval;
}

static Uint32 SDLCALL
callback(Uint32 interval, void *param)
{
    int value = (int)(uintptr_t)param;
    SDL_assert( value == 1 || value == 2 || value == 3 );
    SDL_Log("Timer %" SDL_PRIu32 " : param = %d\n", interval, value);
    return interval;
}

int main(int argc, char *argv[])
{
    int i;
    int desired = -1;
    SDL_TimerID t1, t2, t3;
    Uint64 start64, now64;
    Uint32 start32, now32;
    Uint64 start, now;
    SDL_bool run_interactive_tests = SDL_FALSE;
    int return_code = 0;
    SDLTest_CommonState *state;

    state = SDLTest_CommonCreateState(argv, SDL_INIT_TIMER);
    if (!state) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDLTest_CommonCreateState failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp("--interactive", argv[i]) == 0) {
                run_interactive_tests = SDL_TRUE;
                consumed = 1;
            } else if (desired < 0) {
                char *endptr;

                desired = SDL_strtoul(argv[i], &endptr, 0);
                if (desired != 0 && endptr != argv[i] && *endptr == '\0') {
                    consumed = 1;
                }
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--interactive]", "[interval]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_getenv("SDL_TESTS_QUICK") != NULL) {
        SDL_Log("Not running slower tests");
        SDL_Quit();
        return 0;
    }

    /* Verify SDL_GetTicks* acts monotonically increasing, and not erratic. */
    SDL_Log("Sanity-checking GetTicks\n");
    for (i = 0; i < 1000; ++i) {
        start64 = SDL_GetTicks64();
        start32 = SDL_GetTicks();
        SDL_Delay(1);
        now64 = SDL_GetTicks64() - start64;
        now32 = SDL_GetTicks() - start32;
        if (now32 > 100 || now64 > 100) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "testtimer.c: Delta time erratic at iter %d. Delay 1ms = %d ms in ticks, %d ms in ticks64\n", i, (int)now32, (int)now64);
            SDL_Quit();
            return 1;
        }
    }

    if (desired < 0) {
        desired = DEFAULT_RESOLUTION;
    }

    /* Start the timer */
    t1 = SDL_AddTimer(desired, ticktock, NULL);

    /* Wait 10 seconds */
    SDL_Log("Waiting 10 seconds\n");
    SDL_Delay(10 * 1000);

    /* Stop the timer */
    SDL_RemoveTimer(t1);

    /* Print the results */
    if (ticks) {
        SDL_Log("Timer resolution: desired = %d ms, actual = %f ms\n",
                desired, (double)(10 * 1000) / ticks);
    }

    /* Test multiple timers */
    SDL_Log("Testing multiple timers...\n");
    t1 = SDL_AddTimer(100, callback, (void *)1);
    if (!t1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create timer 1: %s\n", SDL_GetError());
        return_code = 1;
    }
    t2 = SDL_AddTimer(50, callback, (void *)2);
    if (!t2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create timer 2: %s\n", SDL_GetError());
        return_code = 1;
    }
    t3 = SDL_AddTimer(233, callback, (void *)3);
    if (!t3) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create timer 3: %s\n", SDL_GetError());
        return_code = 1;
    }

    /* Wait 10 seconds */
    SDL_Log("Waiting 10 seconds\n");
    SDL_Delay(10 * 1000);

    SDL_Log("Removing timer 1 and waiting 5 more seconds\n");
    SDL_RemoveTimer(t1);

    SDL_Delay(5 * 1000);

    SDL_RemoveTimer(t2);
    SDL_RemoveTimer(t3);

    start = SDL_GetPerformanceCounter();
    for (i = 0; i < 1000000; ++i) {
        ticktock(0, NULL);
    }
    now = SDL_GetPerformanceCounter();
    SDL_Log("1 million iterations of ticktock took %f ms\n", (double)((now - start) * 1000) / SDL_GetPerformanceFrequency());

    SDL_Log("Performance counter frequency: %" SDL_PRIu64 "\n", SDL_GetPerformanceFrequency());
    start64 = SDL_GetTicks64();
    start32 = SDL_GetTicks();
    start = SDL_GetPerformanceCounter();
    SDL_Delay(1000);
    now = SDL_GetPerformanceCounter();
    now64 = SDL_GetTicks64();
    now32 = SDL_GetTicks();
    SDL_Log("Delay 1 second = %d ms in ticks, %d ms in ticks64, %f ms according to performance counter\n", (int)(now32 - start32), (int)(now64 - start64), (double)((now - start) * 1000) / SDL_GetPerformanceFrequency());

    if (run_interactive_tests) {
        return_code |= test_sdl_delay_within_bounds();
    }
    SDLTest_CommonQuit(state);
    return return_code;
}

/* vi: set ts=4 sw=4 expandtab: */
