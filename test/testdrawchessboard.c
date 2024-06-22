/*
   Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely.

   This file is created by : Nitin Jain (nitin.j4\samsung.com)
*/

/* Sample program:  Draw a Chess Board  by using SDL_CreateSoftwareRenderer API */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Surface *surface;
static int done;


static void DrawChessBoard(void)
{
    int row = 0, column = 0, x = 0;
    SDL_FRect rect;
    SDL_Rect darea;

    /* Get the Size of drawing surface */
    SDL_GetRenderViewport(renderer, &darea);

    for (; row < 8; row++) {
        column = row % 2;
        x = column;
        for (; column < 4 + (row % 2); column++) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);

            rect.w = (float)(darea.w / 8);
            rect.h = (float)(darea.h / 8);
            rect.x = (float)(x * rect.w);
            rect.y = (float)(row * rect.h);
            x = x + 2;
            SDL_RenderFillRect(renderer, &rect);

            /* Draw a red diagonal line through the upper left rectangle */
            if (column == 0 && row == 0) {
                SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 0xFF);
                SDL_RenderLine(renderer, 0, 0, rect.w, rect.h);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

static void loop(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {

        /* Re-create when window surface has been resized */
        if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

            SDL_DestroyRenderer(renderer);

            surface = SDL_GetWindowSurface(window);
            renderer = SDL_CreateSoftwareRenderer(surface);
            /* Clear the rendering surface with the specified color */
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderClear(renderer);
        }

        if (e.type == SDL_EVENT_QUIT) {
            done = 1;
#ifdef SDL_PLATFORM_EMSCRIPTEN
            emscripten_cancel_main_loop();
#endif
            return;
        }

        if ((e.type == SDL_EVENT_KEY_DOWN) && (e.key.key == SDLK_ESCAPE)) {
            done = 1;
#ifdef SDL_PLATFORM_EMSCRIPTEN
            emscripten_cancel_main_loop();
#endif
            return;
        }
    }

    DrawChessBoard();

    /* Got everything on rendering surface,
       now Update the drawing image on window screen */
    SDL_UpdateWindowSurface(window);
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init fail : %s\n", SDL_GetError());
        return 1;
    }

    /* Create window and renderer for given surface */
    window = SDL_CreateWindow("Chess Board", 640, 480, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation fail : %s\n", SDL_GetError());
        return 1;
    }
    surface = SDL_GetWindowSurface(window);
    renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Render creation for surface fail : %s\n", SDL_GetError());
        return 1;
    }

    /* Clear the rendering surface with the specified color */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    /* Draw the Image on rendering surface */
    done = 0;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
