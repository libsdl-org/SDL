/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL threading code */

#include <stdlib.h>
#include <signal.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_TLSID tls;
static int alive = 0;
static int testprio = 0;
static SDLTest_CommonState *state;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonDestroyState(state);
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static const char *
getprioritystr(SDL_ThreadPriority priority)
{
    switch (priority) {
    case SDL_THREAD_PRIORITY_LOW:
        return "SDL_THREAD_PRIORITY_LOW";
    case SDL_THREAD_PRIORITY_NORMAL:
        return "SDL_THREAD_PRIORITY_NORMAL";
    case SDL_THREAD_PRIORITY_HIGH:
        return "SDL_THREAD_PRIORITY_HIGH";
    case SDL_THREAD_PRIORITY_TIME_CRITICAL:
        return "SDL_THREAD_PRIORITY_TIME_CRITICAL";
    }

    return "???";
}

static int SDLCALL
ThreadFunc(void *data)
{
    SDL_ThreadPriority prio = SDL_THREAD_PRIORITY_NORMAL;

    SDL_SetTLS(tls, "baby thread", NULL);
    SDL_Log("Started thread %s: My thread id is %" SDL_PRIu64 ", thread data = %s\n",
            (char *)data, SDL_GetCurrentThreadID(), (const char *)SDL_GetTLS(tls));
    while (alive) {
        SDL_Log("Thread '%s' is alive!\n", (char *)data);

        if (testprio) {
            SDL_Log("SDL_SetThreadPriority(%s):%d\n", getprioritystr(prio), SDL_SetThreadPriority(prio));
            if (++prio > SDL_THREAD_PRIORITY_TIME_CRITICAL) {
                prio = SDL_THREAD_PRIORITY_LOW;
            }
        }

        SDL_Delay(1 * 1000);
    }
    SDL_Log("Thread '%s' exiting!\n", (char *)data);
    return 0;
}

static void
killed(int sig)
{
    SDL_Log("Killed with SIGTERM, waiting 5 seconds to exit\n");
    SDL_Delay(5 * 1000);
    alive = 0;
    quit(0);
}

int main(int argc, char *argv[])
{
    int i;
    SDL_Thread *thread;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp("--prio", argv[i]) == 0) {
                testprio = 1;
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--prio]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    /* Load the SDL library */
    if (SDL_Init(0) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_getenv("SDL_TESTS_QUICK") != NULL) {
        SDL_Log("Not running slower tests");
        SDL_Quit();
        return 0;
    }

    tls = SDL_CreateTLS();
    SDL_assert(tls);
    SDL_SetTLS(tls, "main thread", NULL);
    SDL_Log("Main thread data initially: %s\n", (const char *)SDL_GetTLS(tls));

    alive = 1;
    thread = SDL_CreateThread(ThreadFunc, "One", "#1");
    if (!thread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread: %s\n", SDL_GetError());
        quit(1);
    }
    SDL_Delay(5 * 1000);
    SDL_Log("Waiting for thread #1\n");
    alive = 0;
    SDL_WaitThread(thread, NULL);

    SDL_Log("Main thread data finally: %s\n", (const char *)SDL_GetTLS(tls));

    alive = 1;
    (void)signal(SIGTERM, killed);
    thread = SDL_CreateThread(ThreadFunc, "Two", "#2");
    if (!thread) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread: %s\n", SDL_GetError());
        quit(1);
    }
    (void)raise(SIGTERM);

    SDL_Quit(); /* Never reached */
    return 0;   /* Never reached */
}
