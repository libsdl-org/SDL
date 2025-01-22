/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#define NUMTHREADS 10

static SDL_AtomicInt time_for_threads_to_die[NUMTHREADS];

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
SubThreadFunc(void *data)
{
    SDL_AtomicInt *flag = (SDL_AtomicInt *)data;
    while (!SDL_GetAtomicInt(flag)) {
        SDL_Delay(10);
    }
    return 0;
}

static int SDLCALL
ThreadFunc(void *data)
{
    SDL_Thread *sub_threads[NUMTHREADS];
    SDL_AtomicInt flags[NUMTHREADS];
    int i;
    int tid = (int)(uintptr_t)data;

    SDL_Log("Creating Thread %d", tid);

    for (i = 0; i < NUMTHREADS; i++) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Child%d_%d", tid, i);
        SDL_SetAtomicInt(&flags[i], 0);
        sub_threads[i] = SDL_CreateThread(SubThreadFunc, name, &flags[i]);
    }

    SDL_Log("Thread '%d' waiting for signal", tid);
    while (SDL_GetAtomicInt(&time_for_threads_to_die[tid]) != 1) {
        ; /* do nothing */
    }

    SDL_Log("Thread '%d' sending signals to subthreads", tid);
    for (i = 0; i < NUMTHREADS; i++) {
        SDL_SetAtomicInt(&flags[i], 1);
        SDL_WaitThread(sub_threads[i], NULL);
    }

    SDL_Log("Thread '%d' exiting!", tid);

    return 0;
}

int main(int argc, char *argv[])
{
    SDL_Thread *threads[NUMTHREADS];
    int i;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        SDL_Quit();
        SDLTest_CommonDestroyState(state);
        return 1;
    }

    (void)signal(SIGSEGV, SIG_DFL);
    for (i = 0; i < NUMTHREADS; i++) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Parent%d", i);
        SDL_SetAtomicInt(&time_for_threads_to_die[i], 0);
        threads[i] = SDL_CreateThread(ThreadFunc, name, (void *)(uintptr_t)i);

        if (threads[i] == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread: %s", SDL_GetError());
            quit(1);
        }
    }

    for (i = 0; i < NUMTHREADS; i++) {
        SDL_SetAtomicInt(&time_for_threads_to_die[i], 1);
    }

    for (i = 0; i < NUMTHREADS; i++) {
        SDL_WaitThread(threads[i], NULL);
    }
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
