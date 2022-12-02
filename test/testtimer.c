/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include <SDL3/SDL.h>

#define DEFAULT_RESOLUTION 1

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
    SDL_Log("Timer %" SDL_PRIu32 " : param = %d\n", interval, (int)(uintptr_t)param);
    return interval;
}

int main(int argc, char *argv[])
{
    int i, desired;
    SDL_TimerID t1, t2, t3;
    Uint64 start, now;
    Uint64 start_perf, now_perf;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_TIMER) < 0) {
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
        start = SDL_GetTicks();
        SDL_Delay(1);
        now = SDL_GetTicks() - start;
        if (now > 100) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "testtimer.c: Delta time erratic at iter %d. Delay 1ms = %d ms in ticks\n", i, (int)now);
            SDL_Quit();
            return 1;
        }
    }

    /* Start the timer */
    desired = 0;
    if (argv[1]) {
        desired = SDL_atoi(argv[1]);
    }
    if (desired == 0) {
        desired = DEFAULT_RESOLUTION;
    }
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
    }
    t2 = SDL_AddTimer(50, callback, (void *)2);
    if (!t2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create timer 2: %s\n", SDL_GetError());
    }
    t3 = SDL_AddTimer(233, callback, (void *)3);
    if (!t3) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create timer 3: %s\n", SDL_GetError());
    }

    /* Wait 10 seconds */
    SDL_Log("Waiting 10 seconds\n");
    SDL_Delay(10 * 1000);

    SDL_Log("Removing timer 1 and waiting 5 more seconds\n");
    SDL_RemoveTimer(t1);

    SDL_Delay(5 * 1000);

    SDL_RemoveTimer(t2);
    SDL_RemoveTimer(t3);

    start_perf = SDL_GetPerformanceCounter();
    for (i = 0; i < 1000000; ++i) {
        ticktock(0, NULL);
    }
    now_perf = SDL_GetPerformanceCounter();
    SDL_Log("1 million iterations of ticktock took %f ms\n", (double)((now_perf - start_perf) * 1000) / SDL_GetPerformanceFrequency());

    SDL_Log("Performance counter frequency: %" SDL_PRIu64 "\n", SDL_GetPerformanceFrequency());
    start = SDL_GetTicks();
    start_perf = SDL_GetPerformanceCounter();
    SDL_Delay(1000);
    now_perf = SDL_GetPerformanceCounter();
    now = SDL_GetTicks();
    SDL_Log("Delay 1 second = %d ms in ticks, %f ms according to performance counter\n", (int)(now - start), (double)((now_perf - start_perf) * 1000) / SDL_GetPerformanceFrequency());

    SDL_Quit();
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
