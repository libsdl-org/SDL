/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test the thread and rwlock locking functions
   Also exercises the system's signal/thread interaction
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_RWLock *rwlock = NULL;
static SDL_AtomicInt doterminate;
static int nb_threads = 6;
static SDL_Thread **threads;
static int TheVariable = 0;
static int worktime = 100;
static int writerworktime = 10;
static int timeout = 10000;
static SDLTest_CommonState *state;

static int SDLCALL ReaderRun(void *data)
{
    const int threadidx = (int) (uintptr_t) data;
    SDL_Log("Reader Thread %d: starting up!", threadidx);
    while (!SDL_GetAtomicInt(&doterminate)) {
        /* if we're past the timeout, just sleep and wait for doterminate. This makes sure we can't drown out the writer with so many read locks that it never gets control. */
        if (SDL_GetTicks() > ((Uint32) timeout)) {
            SDL_Delay(10);
            continue;
        }

        SDL_Log("Reader Thread %d: Ready to grab lock!", threadidx);
        SDL_LockRWLockForReading(rwlock);
        SDL_Log("Reader Thread %d: TheVariable=%d", threadidx, TheVariable);
        if (worktime) {
            SDL_Delay(worktime);  /* hold the lock awhile. */
        }
        SDL_Log("Reader Thread %d: Releasing lock!", threadidx);
        SDL_UnlockRWLock(rwlock);
        SDL_Delay(200 + SDL_rand(200)); /* If this sleep isn't done, then threads may starve */
    }
    SDL_Log("Reader Thread %d: terminating!", threadidx);
    return 0;
}

int main(int argc, char *argv[])
{
    int remaining_ticks;
    int i;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    SDL_SetAtomicInt(&doterminate, 0);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--nbthreads") == 0) {
                if (argv[i + 1]) {
                    char *endptr;
                    nb_threads = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && nb_threads > 0) {
                        consumed = 2;
                    }
                }
            } else if (SDL_strcmp(argv[i], "--worktime") == 0) {
                if (argv[i + 1]) {
                    char *endptr;
                    worktime = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && worktime > 0) {
                        consumed = 2;
                    }
                }
            } else if (SDL_strcmp(argv[i], "--writerworktime") == 0) {
                if (argv[i + 1]) {
                    char *endptr;
                    writerworktime = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && writerworktime > 0) {
                        consumed = 2;
                    }
                }
            } else if (SDL_strcmp(argv[i], "--timeout") == 0) {
                if (argv[i + 1]) {
                    char *endptr;
                    timeout = (Uint64) SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && timeout > 0) {
                        consumed = 2;
                    }
                }
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--nbthreads NB]",
                "[--worktime ms]",
                "[--writerworktime ms]",
                "[--timeout ms]",
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    threads = SDL_malloc(nb_threads * sizeof(SDL_Thread*));

    /* Load the SDL library */
    if (!SDL_Init(0)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
        return 1;
    }

    SDL_SetAtomicInt(&doterminate, 0);

    rwlock = SDL_CreateRWLock();
    if (!rwlock) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create rwlock: %s", SDL_GetError());
        SDL_Quit();
        SDLTest_CommonDestroyState(state);
        return 1;
    }

    for (i = 0; i < nb_threads; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Reader%d", i);
        threads[i] = SDL_CreateThread(ReaderRun, name, (void *)(uintptr_t) (i+1));
        if (threads[i] == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create reader thread! %s", SDL_GetError());
        }
    }

    remaining_ticks = timeout;
    while (!SDL_GetAtomicInt(&doterminate) && (remaining_ticks > 0)) {
        Uint64 now;
        SDL_Log("## New iteration! (%d ticks remaining.)", remaining_ticks);
        SDL_Log("Writer Thread: Ready to grab lock!");
        SDL_LockRWLockForWriting(rwlock);
        SDL_Log("Writer Thread: increment TheVariable to %d", ++TheVariable);
        if (writerworktime) {
            SDL_Delay(writerworktime);
        }
        SDL_Log("Writer Thread: Releasing lock!");
        SDL_UnlockRWLock(rwlock);
        SDL_Delay(50); /* If this sleep isn't done, then threads may starve */
        now = SDL_GetTicks();
        remaining_ticks = ((now < ((Uint64) timeout)) ? ((int) (((Uint64) timeout) - now)) : 0);
    }

    SDL_SetAtomicInt(&doterminate, 1);
    SDL_Log("Waiting on reader threads to terminate...");
    for (i = 0; i < nb_threads; ++i) {
        SDL_WaitThread(threads[i], NULL);
    }
    SDL_free(threads);

    SDL_Log("Reader threads have terminated, quitting!");
    SDL_DestroyRWLock(rwlock);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
