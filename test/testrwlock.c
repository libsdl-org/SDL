/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
static SDL_ThreadID mainthread;
static SDL_AtomicInt doterminate;
static int nb_threads = 6;
static SDL_Thread **threads;
static int worktime = 1000;
static int writerworktime = 100;
static int timeout = 10000;
static SDLTest_CommonState *state;

static void DoWork(const int workticks)  /* "Work" */
{
    const SDL_ThreadID tid = SDL_GetCurrentThreadID();
    const bool is_reader = tid != mainthread;
    const char *typestr = is_reader ? "Reader" : "Writer";

    SDL_Log("%s Thread %" SDL_PRIu64 ": ready to work", typestr, tid);
    if (is_reader) {
        SDL_LockRWLockForReading(rwlock);
    } else {
        SDL_LockRWLockForWriting(rwlock);
    }

    SDL_Log("%s Thread %" SDL_PRIu64 ": start work!", typestr, tid);
    SDL_Delay(workticks);
    SDL_Log("%s Thread %" SDL_PRIu64 ": work done!", typestr, tid);
    SDL_UnlockRWLock(rwlock);

    /* If this sleep isn't done, then threads may starve */
    SDL_Delay(10);
}

static int SDLCALL
ReaderRun(void *data)
{
    SDL_Log("Reader Thread %" SDL_PRIu64 ": starting up", SDL_GetCurrentThreadID());
    while (!SDL_GetAtomicInt(&doterminate)) {
        DoWork(worktime);
    }
    SDL_Log("Reader Thread %" SDL_PRIu64 ": exiting!", SDL_GetCurrentThreadID());
    return 0;
}

int main(int argc, char *argv[])
{
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

    mainthread = SDL_GetCurrentThreadID();
    SDL_Log("Writer thread: %" SDL_PRIu64, mainthread);
    for (i = 0; i < nb_threads; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Reader%d", i);
        threads[i] = SDL_CreateThread(ReaderRun, name, NULL);
        if (threads[i] == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create reader thread! %s", SDL_GetError());
        }
    }

    while (!SDL_GetAtomicInt(&doterminate) && (SDL_GetTicks() < ((Uint64) timeout))) {
        DoWork(writerworktime);
    }

    SDL_SetAtomicInt(&doterminate, 1);
    SDL_Log("Waiting on reader threads to terminate...");
    for (i = 0; i < nb_threads; ++i) {
        SDL_WaitThread(threads[i], NULL);
    }

    SDL_Log("Reader threads have terminated, quitting!");
    SDL_DestroyRWLock(rwlock);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
