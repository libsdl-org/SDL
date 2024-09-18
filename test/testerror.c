/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL threading code and error handling */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static int alive = 0;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static int SDLCALL
ThreadFunc(void *data)
{
    /* Set the child thread error string */
    SDL_SetError("Thread %s (%" SDL_PRIu64 ") had a problem: %s",
                 (char *)data, SDL_GetCurrentThreadID(), "nevermind");
    while (alive) {
        SDL_Log("Thread '%s' is alive!\n", (char *)data);
        SDL_Delay(1 * 1000);
    }
    SDL_Log("Child thread error string: %s\n", SDL_GetError());
    return 0;
}

int main(int argc, char *argv[])
{
    SDL_Thread *thread;
    SDLTest_CommonState *state;
    int i;
    bool enable_threads = true;

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
            }
        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--no-threads]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }

    /* Load the SDL library */
    if (!SDL_Init(0)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Set the error value for the main thread */
    SDL_SetError("No worries");

    if (SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "SDL_TESTS_QUICK") != NULL) {
        SDL_Log("Not running slower tests");
        SDL_Quit();
        return 0;
    }

    if (enable_threads) {
        alive = 1;
        thread = SDL_CreateThread(ThreadFunc, NULL, "#1");
        if (!thread) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread: %s\n", SDL_GetError());
            quit(1);
        }
        SDL_Delay(5 * 1000);
        SDL_Log("Waiting for thread #1\n");
        alive = 0;
        SDL_WaitThread(thread, NULL);
    }

    SDL_Log("Main thread error string: %s\n", SDL_GetError());

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
