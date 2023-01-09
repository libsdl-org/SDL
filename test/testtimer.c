/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
    Uint64 start64, now64;
    Uint32 start32, now32;
    Uint64 start, now;

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

    SDL_Quit();
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
