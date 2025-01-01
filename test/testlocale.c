/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdio.h>
#include "SDL.h"
#include "SDL_test.h"

/* !!! FIXME: move this to the test framework */

static void log_locales(void)
{
    SDL_Locale *locales = SDL_GetPreferredLocales();
    if (!locales) {
        SDL_Log("Couldn't determine locales: %s", SDL_GetError());
    } else {
        SDL_Locale *l;
        unsigned int total = 0;
        SDL_Log("Locales, in order of preference:");
        for (l = locales; l->language; l++) {
            const char *c = l->country;
            SDL_Log(" - %s%s%s", l->language, c ? "_" : "", c ? c : "");
            total++;
        }
        SDL_Log("%u locales seen.", total);
        SDL_free(locales);
    }
}

int main(int argc, char **argv)
{
    SDLTest_CommonState *state;
    SDL_bool listen = SDL_FALSE;
    int i = 1;

    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDLTest_CommonCreateState failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp("--listen", argv[i]) == 0) {
                listen = SDL_TRUE;
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--listen]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        return 1;
    }

    /* Print locales and languages */
    log_locales();

    if (listen) {
        SDL_bool keep_going = SDL_TRUE;
        while (keep_going) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    keep_going = SDL_FALSE;
                } else if (e.type == SDL_LOCALECHANGED) {
                    SDL_Log("Saw SDL_LOCALECHANGED event!");
                    log_locales();
                }
            }
            SDL_Delay(10);
        }
    }

    SDLTest_CommonQuit(state);

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
