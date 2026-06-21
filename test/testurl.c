/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
    const char *url = NULL;
    SDLTest_CommonState *state = SDLTest_CommonCreateState(argv, 0);
    bool use_gui = false;

    /* Parse commandline */
    for (int i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            if (argv[i][0] != '-') {
                url = argv[i];
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--gui") == 0) {
                use_gui = true;
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--gui]"
                "[URL [...]]",
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return SDL_APP_FAILURE;
        }
        i += consumed;
    }

    state->flags = SDL_INIT_VIDEO;
    if (!SDLTest_CommonInit(state)) {
        return SDL_APP_FAILURE;
    }

    if (!use_gui) {
        tryOpenURL(url);
    } else {
        SDL_Event event;
        bool quit = false;

        while (!quit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.key == SDLK_SPACE) {
                        tryOpenURL(url);
                    } else if (event.key.key == SDLK_ESCAPE) {
                        quit = true;
                    }
                } else if (event.type == SDL_EVENT_QUIT) {
                    quit = true;
                }
            }

            SDL_SetRenderDrawColor(state->renderers[0], 0, 0, 0, 255);
            SDL_RenderClear(state->renderers[0]);
            SDL_SetRenderDrawColor(state->renderers[0], 255, 255, 255, 255);
            SDL_RenderDebugTextFormat(state->renderers[0], 8.f, 16.f, "Press space to open %s", url);
            SDL_RenderPresent(state->renderers[0]);
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
