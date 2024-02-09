/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
    SDL_Surface *shape = NULL;
    SDL_Texture *shape_texture = NULL;
    SDL_Event event;
    SDL_bool done = SDL_FALSE;
    int return_code = 1;

    if (argc != 2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s shape.bmp\n", argv[0]);
        goto quit;
    }

    /* Create the window hidden so we can set the window size to match our shape */
    window = SDL_CreateWindow("Shape test", 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT);
    if (!window) {
        SDL_Log("Couldn't create transparent window: %s\n", SDL_GetError());
        goto quit;
    }

    renderer = SDL_CreateRenderer(window, NULL, 0);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s\n", SDL_GetError());
        goto quit;
    }

    shape = SDL_LoadBMP(argv[1]);
    if (!shape) {
        SDL_Log("Couldn't load %s: %s\n", argv[1], SDL_GetError());
        goto quit;
    }

    if (!SDL_ISPIXELFORMAT_ALPHA(shape->format->format)) {
        /* Set the colorkey to the top-left pixel */
        Uint8 r, g, b, a;

        SDL_ReadSurfacePixel(shape, 0, 0, &r, &g, &b, &a);
        SDL_SetSurfaceColorKey(shape, 1, SDL_MapRGBA(shape->format, r, g, b, a));
    }

    shape_texture = SDL_CreateTextureFromSurface(renderer, shape);
    if (!shape_texture) {
        SDL_Log("Couldn't create shape texture: %s\n", SDL_GetError());
        goto quit;
    }

    /* Set the blend mode so the alpha channel of the shape is the window transparency */
    if (SDL_SetTextureBlendMode(shape_texture,
            SDL_ComposeCustomBlendMode(
                SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD)) < 0) {
        SDL_Log("Couldn't set shape blend mode: %s\n", SDL_GetError());
        goto quit;
    }

    /* Set the window size to the size of our shape and show it */
    SDL_SetWindowSize(window, shape->w, shape->h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    /* We're ready to go! */
    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_QUIT:
                /* Quit on keypress and quit event */
                done = SDL_TRUE;
                break;
            default:
                break;
            }
        }

        /* We'll clear to white, but you could do other drawing here */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        /* Apply the shape */
        SDL_RenderTexture(renderer, shape_texture, NULL, NULL);

        /* Show everything on the screen and wait a bit */
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }

    /* Success! */
    return_code = 0;

quit:
    SDL_DestroySurface(shape);
    SDL_Quit();
    return return_code;
}
