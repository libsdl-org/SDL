/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Sample program:  Create a parent window and a modal child window. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <stdlib.h>

static SDL_bool CreateModalWindow(SDL_Window *parent, SDL_Window **w, SDL_Renderer **r)
{
    SDL_bool exit_code = SDL_FALSE;
    SDL_PropertiesID props = SDL_CreateProperties();

    if (!props) {
        SDL_Log("Failed to create properties: %s\n", SDL_GetError());
        goto done;
    }

    if (SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Modal Window") < 0) {
        SDL_Log("Failed to set modal property: %s\n", SDL_GetError());
        goto done;
    }

    if (SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MODAL_BOOLEAN, SDL_TRUE) < 0) {
        SDL_Log("Failed to set modal property: %s\n", SDL_GetError());
        goto done;
    }

    if (SDL_SetProperty(props, SDL_PROP_WINDOW_CREATE_PARENT_POINTER, parent) < 0) {
        SDL_Log("Failed to set parent property: %s\n", SDL_GetError());
        goto done;
    }

    if (SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 320) < 0) {
        SDL_Log("Failed to set width property: %s\n", SDL_GetError());
        goto done;
    }

    if (SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 200) < 0) {
        SDL_Log("Failed to set height property: %s\n", SDL_GetError());
        goto done;
    }

    *w = SDL_CreateWindowWithProperties(props);

    if (!*w) {
        SDL_Log("Failed to create child window: %s\n", SDL_GetError());
        goto done;
    }

    *r = SDL_CreateRenderer(*w, NULL, 0);

    if (!*r) {
        SDL_Log("Failed to create child window: %s\n", SDL_GetError());
        SDL_DestroyWindow(*w);
        *w = NULL;
        goto done;
    }

    exit_code = SDL_TRUE;

done:

    SDL_DestroyProperties(props);

    return exit_code;
}

int main(int argc, char *argv[])
{
    SDL_Window *w1 = NULL, *w2 = NULL;
    SDL_Renderer *r1 = NULL, *r2 = NULL;
    SDLTest_CommonState *state = NULL;
    Uint64 show_deadline = 0;
    int i;
    int exit_code = 0;

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

        if (consumed <= 0) {
            static const char *options[] = { NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    if (SDL_CreateWindowAndRenderer(640, 480, 0, &w1, &r1) < 0) {
        SDL_Log("Failed to create parent window and/or renderer: %s\n", SDL_GetError());
        exit_code = 1;
        goto sdl_quit;
    }

    SDL_SetWindowTitle(w1, "Parent Window");

    if (!CreateModalWindow(w1, &w2, &r2)) {
        exit_code = 1;
        goto sdl_quit;
    }

    while (1) {
        int quit = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = 1;
                break;
            } else if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == SDL_GetWindowID(w2)) {
                    SDL_DestroyWindow(w2);
                    r2 = NULL;
                    w2 = NULL;
                } else if (e.window.windowID == SDL_GetWindowID(w1)) {
                    SDL_DestroyWindow(w1);
                    r1 = NULL;
                    w1 = NULL;
                }
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.keysym.sym == SDLK_m && !w2) {
                    if (!CreateModalWindow(w1, &w2, &r2)) {
                        exit_code = 1;
                        goto sdl_quit;
                    }
                } else if (e.key.keysym.sym == SDLK_ESCAPE && w2) {
                    SDL_DestroyWindow(w2);
                    r2 = NULL;
                    w2 = NULL;
                } else if (e.key.keysym.sym == SDLK_h) {
                    show_deadline = SDL_GetTicksNS() + SDL_SECONDS_TO_NS(3);
                    SDL_HideWindow(w1);
                }
            }
        }
        if (quit) {
            break;
        }
        SDL_Delay(100);

        if (show_deadline && show_deadline <= SDL_GetTicksNS()) {
            SDL_ShowWindow(w1);
        }

        /* Parent window is red */
        if (r1) {
            SDL_SetRenderDrawColor(r1, 224, 48, 12, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(r1);
            SDL_RenderPresent(r1);
        }

        /* Child window is blue */
        if (r2) {
            SDL_SetRenderDrawColor(r2, 6, 76, 255, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(r2);
            SDL_RenderPresent(r2);
        }
    }

sdl_quit:
    if (w1) {
        /* The child window and renderer will be cleaned up automatically. */
        SDL_DestroyWindow(w1);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return exit_code;
}
