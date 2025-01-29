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

#include "glass.h"


static SDL_HitTestResult SDLCALL ShapeHitTest(SDL_Window *window, const SDL_Point *area, void *userdata)
{
    SDL_Surface *shape = (SDL_Surface *)userdata;
    Uint8 r, g, b, a;

    if (SDL_ReadSurfacePixel(shape, area->x, area->y, &r, &g, &b, &a)) {
        if (a != SDL_ALPHA_TRANSPARENT) {
            /* We'll just make everything draggable */
            return SDL_HITTEST_DRAGGABLE;
        }
    }
    return SDL_HITTEST_NORMAL;
}

int main(int argc, char *argv[])
{
    const char *image_file = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *shape = NULL;
    bool resizable = false;
    SDL_WindowFlags flags;
    bool done = false;
    SDL_Event event;
    int i;
    int return_code = 1;

    for (i = 1; i < argc; ++i) {
        if (SDL_strcmp(argv[i], "--resizable") == 0) {
            resizable = true;
        } else if (!image_file) {
            image_file = argv[i];
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s [--resizable] [shape.bmp]", argv[0]);
            goto quit;
        }
    }

    if (image_file) {
        shape = SDL_LoadBMP(image_file);
        if (!shape) {
            SDL_Log("Couldn't load %s: %s", image_file, SDL_GetError());
            goto quit;
        }
    } else {
        SDL_IOStream *stream = SDL_IOFromConstMem(glass_bmp, sizeof(glass_bmp));
        if (!stream) {
            SDL_Log("Couldn't create iostream for glass.bmp: %s", SDL_GetError());
            goto quit;
        }
        shape = SDL_LoadBMP_IO(stream, true);
        if (!shape) {
            SDL_Log("Couldn't load glass.bmp: %s", SDL_GetError());
            goto quit;
        }
    }

    /* Create the window hidden so we can set the shape before it's visible */
    flags = (SDL_WINDOW_HIDDEN | SDL_WINDOW_TRANSPARENT);
    if (resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    } else {
        flags |= SDL_WINDOW_BORDERLESS;
    }
    window = SDL_CreateWindow("SDL Shape Test", shape->w, shape->h, flags);
    if (!window) {
        SDL_Log("Couldn't create transparent window: %s", SDL_GetError());
        goto quit;
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        goto quit;
    }

    if (!SDL_ISPIXELFORMAT_ALPHA(shape->format)) {
        /* Set the colorkey to the top-left pixel */
        Uint8 r, g, b, a;

        SDL_ReadSurfacePixel(shape, 0, 0, &r, &g, &b, &a);
        SDL_SetSurfaceColorKey(shape, 1, SDL_MapSurfaceRGBA(shape, r, g, b, a));
    }

    if (!resizable) {
        /* Set the hit test callback so we can drag the window */
        if (!SDL_SetWindowHitTest(window, ShapeHitTest, shape)) {
            SDL_Log("Couldn't set hit test callback: %s", SDL_GetError());
            goto quit;
        }
    }

    /* Set the window size to the size of our shape and show it */
    SDL_SetWindowShape(window, shape);
    SDL_ShowWindow(window);

    /* We're ready to go! */
    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    done = true;
                }
                break;
            case SDL_EVENT_QUIT:
                done = true;
                break;
            default:
                break;
            }
        }

        /* We'll clear to white, but you could do other drawing here */
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

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
