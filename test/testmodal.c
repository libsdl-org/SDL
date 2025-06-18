/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    if (!SDL_CreateWindowAndRenderer("Parent Window", 640, 480, 0, &w1, &r1)) {
        SDL_Log("Failed to create parent window and/or renderer: %s", SDL_GetError());
        exit_code = 1;
        goto sdl_quit;
    }

    if (!SDL_CreateWindowAndRenderer("Non-Modal Window", 320, 200, 0, &w2, &r2)) {
        SDL_Log("Failed to create parent window and/or renderer: %s", SDL_GetError());
        exit_code = 1;
        goto sdl_quit;
    }

    if (SDL_SetWindowParent(w2, w1)) {
        if (SDL_SetWindowModal(w2, true)) {
            SDL_SetWindowTitle(w2, "Modal Window");
        }
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
                    SDL_DestroyRenderer(r2);
                    SDL_DestroyWindow(w2);
                    r2 = NULL;
                    w2 = NULL;
                } else if (e.window.windowID == SDL_GetWindowID(w1)) {
                    SDL_DestroyRenderer(r1);
                    SDL_DestroyWindow(w1);
                    r1 = NULL;
                    w1 = NULL;
                }
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if ((e.key.key == SDLK_M || e.key.key == SDLK_N) && !w2) {
                    if (!SDL_CreateWindowAndRenderer("Non-Modal Window", 320, 200, SDL_WINDOW_HIDDEN, &w2, &r2)) {
                        SDL_Log("Failed to create modal window and/or renderer: %s", SDL_GetError());
                        exit_code = 1;
                        goto sdl_quit;
                    }

                    if (e.key.key == SDLK_M) {
                        if (SDL_SetWindowParent(w2, w1)) {
                            if (SDL_SetWindowModal(w2, true)) {
                                SDL_SetWindowTitle(w2, "Modal Window");
                            }
                        }
                    }
                    SDL_ShowWindow(w2);
                } else if (e.key.key == SDLK_ESCAPE && w2) {
                    SDL_DestroyWindow(w2);
                    r2 = NULL;
                    w2 = NULL;
                } else if (e.key.key == SDLK_H) {
                    if (e.key.mod & SDL_KMOD_CTRL) {
                        /* Hide the parent, which should hide the modal too. */
                        show_deadline = SDL_GetTicksNS() + SDL_SECONDS_TO_NS(3);
                        SDL_HideWindow(w1);
                    } else if (w2) {
                        /* Show/hide the modal window */
                        if (SDL_GetWindowFlags(w2) & SDL_WINDOW_HIDDEN) {
                            SDL_ShowWindow(w2);
                        } else {
                            SDL_HideWindow(w2);
                        }
                    }
                } else if (e.key.key == SDLK_P && w2) {
                    if (SDL_GetWindowFlags(w2) & SDL_WINDOW_MODAL) {
                        /* Unparent the window */
                        if (SDL_SetWindowModal(w2, false)) {
                            if (SDL_SetWindowParent(w2, NULL)) {
                                SDL_SetWindowTitle(w2, "Non-Modal Window");
                            }
                        }
                    } else {
                        /* Reparent the window */
                        if (SDL_SetWindowParent(w2, w1)) {
                            if (SDL_SetWindowModal(w2, true)) {
                                SDL_SetWindowTitle(w2, "Modal Window");
                            }
                        }
                    }
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
