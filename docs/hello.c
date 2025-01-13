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

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    const char *message = "Hello World!";
    int w = 0, h = 0;
    float x, y;
    bool done = false;

    /* Create the window */
    if (!SDL_CreateWindowAndRenderer("Hello World", 0, 0, SDL_WINDOW_FULLSCREEN, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s\n", SDL_GetError());
        return 1;
    }

    while (!done) {
        SDL_Event event;

        /* Handle events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_QUIT) {
                done = true;
            }
        }

        /* Center the message */
        SDL_GetWindowSize(window, &w, &h);
        x = (float)(w - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(message)) / 2;
        y = (float)(h - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2;

        /* Draw the message */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, x, y, message);
        SDL_RenderPresent(renderer);
    }
    SDL_Quit();

    return 0;
}

