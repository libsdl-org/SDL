/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test program to test dynamic loading with the loadso subsystem.
 */

#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

typedef int (*fntype)(const char *);

static void log_usage(char *progname, SDLTest_CommonState *state) {
    static const char *options[] = { "library", "functionname|--hello", NULL };
    SDLTest_CommonLogUsage(state, progname, options);
    SDL_Log("USAGE: %s <library> <functionname>\n", progname);
    SDL_Log("       %s <lib with puts()> --hello\n", progname);
}

int main(int argc, char *argv[])
{
    int i;
    int result = 0;
    int hello = 0;
    const char *libname = NULL;
    const char *symname = NULL;
    SDL_SharedObject *lib = NULL;
    fntype fn = NULL;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--hello") == 0) {
                if (!symname || SDL_strcmp(symname, "puts") == 0) {
                    symname = "puts";
                    consumed = 1;
                    hello = 1;
                }
            } else if (!libname) {
                libname = argv[i];
                consumed = 1;
            } else if (!symname) {
                symname = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            log_usage(argv[0], state);
            return 1;
        }

        i += consumed;
    }

    if (!libname || !symname) {
        log_usage(argv[0], state);
        return 1;
    }

    /* Initialize SDL */
    if (!SDL_Init(0)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 2;
    }

    lib = SDL_LoadObject(libname);
    if (!lib) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_LoadObject('%s') failed: %s\n",
                     libname, SDL_GetError());
        result = 3;
    } else {
        fn = (fntype)SDL_LoadFunction(lib, symname);
        if (!fn) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_LoadFunction('%s') failed: %s\n",
                         symname, SDL_GetError());
            result = 4;
        } else {
            SDL_Log("Found %s in %s at %p\n", symname, libname, fn);
            if (hello) {
                SDL_Log("Calling function...\n");
                fn("     HELLO, WORLD!\n");
                SDL_Log("...apparently, we survived.  :)\n");
                SDL_Log("Unloading library...\n");
            }
        }
        SDL_UnloadObject(lib);
    }
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return result;
}
