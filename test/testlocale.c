/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static void log_locales(void)
{
    SDL_Locale *locales = SDL_GetPreferredLocales();
    if (locales == NULL) {
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
    int i;
    int listen = 0;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
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
            if (SDL_strcmp(argv[1], "--listen") == 0) {
                listen = 1;
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

    /* Print locales and languages */
    if (SDLTest_CommonInit(state) == SDL_FALSE) {
        return 1;
    }

    log_locales();

    if (listen) {
        int done = 0;
        while (!done) {
            SDL_Event e;
            SDLTest_CommonEvent(state, &e, &done);
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    done = 1;
                } else if (e.type == SDL_EVENT_LOCALE_CHANGED) {
                    SDL_Log("Saw SDL_EVENT_LOCALE_CHANGED event!");
                    log_locales();
                }
            }

            for (i = 0; i < state->num_windows; i++) {
                SDL_RenderPresent(state->renderers[i]);
            }

            SDL_Delay(10);
        }
    }

    SDLTest_CommonQuit(state);

    return 0;
}
