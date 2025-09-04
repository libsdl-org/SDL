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


int main(int argc, char *argv[])
{
    const char *image_file = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_WindowFlags flags;
    bool done = false;
    SDL_Event event;
    int i;
    int return_code = 1;


    int windowWidth = 800;
    int windowHeight = 600;
    SDL_FRect destRect;
    destRect.x = 0;
    destRect.y = 0;
    destRect.w = 100;
    destRect.h = 100;

    SDL_FRect destRect2;
    destRect2.x = 700;
    destRect2.y = 0;
    destRect2.w = 100;
    destRect2.h = 100;

    SDL_FRect destRect3;
    destRect3.x = 0;
    destRect3.y = 500;
    destRect3.w = 100;
    destRect3.h = 100;

    SDL_FRect destRect4;
    destRect4.x = 700;
    destRect4.y = 500;
    destRect4.w = 100;
    destRect4.h = 100;

    SDL_FRect destRect5;
    destRect5.x = 350;
    destRect5.y = 250;
    destRect5.w = 100;
    destRect5.h = 100;


    /* Create the window hidden */
    flags = (SDL_WINDOW_HIDDEN |  SDL_WINDOW_TRANSPARENT);
    //flags |= SDL_WINDOW_BORDERLESS;

    window = SDL_CreateWindow("SDL Software Renderer Transparent Test", windowWidth, windowHeight, flags);
    if (!window) {
        SDL_Log("Couldn't create transparent window: %s", SDL_GetError());
        goto quit;
    }

    /* Create a software renderer and set the blend mode */
    renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        goto quit;
    }

    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
        SDL_Log("Couldn't set renderer blend mode: %s\n", SDL_GetError());
        return false;
    }

    /* Create texture and set the blend mode */
    // texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    // texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);
    if (texture == NULL) {
        SDL_Log("Couldn't create texture: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED)) {
        SDL_Log("Couldn't set texture blend mode: %s\n", SDL_GetError());
        return false;
    }

    /* Show */
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

        
        /* Draw opaque red squares at the four corners of the form, and draw a red square with an alpha value of 180 in the center of the form */
        
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &destRect);
        SDL_RenderFillRect(renderer, &destRect2);
        SDL_RenderFillRect(renderer, &destRect3);
        SDL_RenderFillRect(renderer, &destRect4);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 180);
        SDL_RenderFillRect(renderer, &destRect5);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);

        /* Show everything on the screen and wait a bit */
        SDL_RenderPresent(renderer);
        SDL_Delay(100);
    }

    /* Success! */
    return_code = 0;

quit:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return return_code;
}
