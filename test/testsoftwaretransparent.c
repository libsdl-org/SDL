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

#define SQUARE_SIZE 100.0f

/* Draw opaque red squares at the four corners of the form, and draw a red square with an alpha value of 180 in the center of the form */
static void draw(SDL_Renderer *renderer)
{
    SDL_FRect rect = { 0.0f, 0.0f, SQUARE_SIZE, SQUARE_SIZE };
    int w, h;

    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    if (w >= 3 * SQUARE_SIZE && h >= 3 * SQUARE_SIZE) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

        rect.x = 0.0f;
        rect.y = 0.0f;
        SDL_RenderFillRect(renderer, &rect);

        rect.y = h - SQUARE_SIZE;
        SDL_RenderFillRect(renderer, &rect);

        rect.x = w - SQUARE_SIZE;
        SDL_RenderFillRect(renderer, &rect);

        rect.y = 0.0f;
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 180);
    rect.x = (w - SQUARE_SIZE) / 2;
    rect.y = (h - SQUARE_SIZE) / 2;
    SDL_RenderFillRect(renderer, &rect);
}

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    bool done = false;
    SDL_Event event;
    SDLTest_CommonState *state;

    int return_code = 1;

    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        goto quit;
    }

    window = SDL_CreateWindow("SDL Software Renderer Transparent Test", 800, 600, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("Couldn't create transparent window: %s", SDL_GetError());
        goto quit;
    }

    /* Create a software renderer */
    renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        goto quit;
    }

    /* Make sure we're setting the alpha channel while drawing */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* We're ready to go! */
    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    done = true;
                }
                break;
            case SDL_EVENT_WINDOW_EXPOSED:
                /* The software renderer is persistent, so only redraw as-needed */
                draw(renderer);
                break;
            case SDL_EVENT_QUIT:
                done = true;
                break;
            default:
                break;
            }
        }

        /* Show everything on the screen and wait a bit */
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }

    /* Success! */
    return_code = 0;

quit:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return return_code;
}
