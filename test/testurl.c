/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

static void tryOpenURL(const char *url)
{
    SDL_Log("Opening '%s' ...", url);
    if (SDL_OpenURL(url)) {
        SDL_Log("  success!");
    } else {
        SDL_Log("  failed! %s", SDL_GetError());
    }
}

int main(int argc, char **argv)
{
    int i;
    SDLTest_CommonState *state;

    state = SDLTest_CommonCreateState(argv, 0);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            if (argv[i][0] != '-') {
                tryOpenURL(argv[i]);
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[URL [...]]",
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return SDL_APP_FAILURE;
        }
        i += consumed;
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
