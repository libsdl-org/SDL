/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test the thread and mutex locking functions
   Also exercises the system's signal/thread interaction
*/

#include <signal.h>
#include <stdlib.h> /* for atexit() */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_Mutex *mutex = NULL;
static SDL_threadID mainthread;
static SDL_AtomicInt doterminate;
static int nb_threads = 6;
static SDL_Thread **threads;
static int worktime = 1000;
static SDLTest_CommonState *state;

/**
 * SDL_Quit() shouldn't be used with atexit() directly because
 * calling conventions may differ...
 */
static void
SDL_Quit_Wrapper(void)
{
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
}

static void printid(void)
{
    SDL_Log("Thread %lu:  exiting\n", SDL_ThreadID());
}

static void terminate(int sig)
{
    (void)signal(SIGINT, terminate);
    SDL_AtomicSet(&doterminate, 1);
}

static void closemutex(int sig)
{
    SDL_threadID id = SDL_ThreadID();
    int i;
    SDL_Log("Thread %lu:  Cleaning up...\n", id == mainthread ? 0 : id);
    SDL_AtomicSet(&doterminate, 1);
    if (threads) {
        for (i = 0; i < nb_threads; ++i) {
            SDL_WaitThread(threads[i], NULL);
        }
        SDL_free(threads);
        threads = NULL;
    }
    SDL_DestroyMutex(mutex);
    /* Let 'main()' return normally */
    if (sig != 0) {
        exit(sig);
    }
}

static int SDLCALL
Run(void *data)
{
    if (SDL_ThreadID() == mainthread) {
        (void)signal(SIGTERM, closemutex);
    }
    SDL_Log("Thread %lu: starting up", SDL_ThreadID());
    while (!SDL_AtomicGet(&doterminate)) {
        SDL_Log("Thread %lu: ready to work\n", SDL_ThreadID());
        if (SDL_LockMutex(mutex) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't lock mutex: %s", SDL_GetError());
            exit(1);
        }
        SDL_Log("Thread %lu: start work!\n", SDL_ThreadID());
        SDL_Delay(1 * worktime);
        SDL_Log("Thread %lu: work done!\n", SDL_ThreadID());
        if (SDL_UnlockMutex(mutex) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't unlock mutex: %s", SDL_GetError());
            exit(1);
        }
        /* If this sleep isn't done, then threads may starve */
        SDL_Delay(10);
    }
    if (SDL_ThreadID() == mainthread && SDL_AtomicGet(&doterminate)) {
        SDL_Log("Thread %lu: raising SIGTERM\n", SDL_ThreadID());
        (void)raise(SIGTERM);
    }
    SDL_Log("Thread %lu: exiting!\n", SDL_ThreadID());
    return 0;
}

#ifndef _WIN32
static Uint32 hit_timeout(Uint32 interval, void *param) {
    SDL_Log("Hit timeout! Sending SIGINT!");
    kill(0, SIGINT);
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    int i;
#ifndef _WIN32
    int timeout = 0;
#endif

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
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
                    nb_threads = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && nb_threads > 0) {
                        consumed = 2;
                    }
                }
#ifndef _WIN32
            } else if (SDL_strcmp(argv[i], "--timeout") == 0) {
                if (argv[i + 1]) {
                    char *endptr;
                    timeout = SDL_strtol(argv[i + 1], &endptr, 0);
                    if (endptr != argv[i + 1] && *endptr == '\0' && timeout > 0) {
                        consumed = 2;
                    }
                }
#endif
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--nbthreads NB]",
                "[--worktime ms]",
#ifndef _WIN32
                "[--timeout ms]",
#endif
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    threads = SDL_malloc(nb_threads * sizeof(SDL_Thread*));

    /* Load the SDL library */
    if (SDL_Init(0) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        exit(1);
    }
    (void)atexit(SDL_Quit_Wrapper);

    SDL_AtomicSet(&doterminate, 0);

    mutex = SDL_CreateMutex();
    if (mutex == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create mutex: %s\n", SDL_GetError());
        exit(1);
    }

    mainthread = SDL_ThreadID();
    SDL_Log("Main thread: %lu\n", mainthread);
    (void)atexit(printid);
    for (i = 0; i < nb_threads; ++i) {
        char name[64];
        (void)SDL_snprintf(name, sizeof(name), "Worker%d", i);
        threads[i] = SDL_CreateThread(Run, name, NULL);
        if (threads[i] == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread!\n");
        }
    }

#ifndef _WIN32
    if (timeout) {
        SDL_AddTimer(timeout, hit_timeout, NULL);
    }
#endif

    (void)signal(SIGINT, terminate);
    Run(NULL);

    return 0; /* Never reached */
}
