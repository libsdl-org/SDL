/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL threading code and error handling */

#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"
#include "SDL_test.h"

static int alive = 0;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    exit(rc);
}

int SDLCALL
ThreadFunc(void *data)
{
    /* Set the child thread error string */
    SDL_SetError("Thread %s (%lu) had a problem: %s",
                 (char *)data, SDL_ThreadID(), "nevermind");
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

    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDLTest_CommonCreateState failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Set the error value for the main thread */
    SDL_SetError("No worries");

    if (SDL_getenv("SDL_TESTS_QUICK") != NULL) {
        SDL_Log("Not running slower tests");
        SDL_Quit();
        return 0;
    }

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

    SDL_Log("Main thread error string: %s\n", SDL_GetError());

    SDLTest_CommonQuit(state);
    return 0;
}
