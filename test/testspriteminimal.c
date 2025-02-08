/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Move N sprites around on the screen as fast as possible */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include "icon.h"

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define NUM_SPRITES   100
#define MAX_SPEED     1

static SDL_Texture *sprite;
static SDL_FRect positions[NUM_SPRITES];
static SDL_FRect velocities[NUM_SPRITES];
static int sprite_w, sprite_h;

static SDL_Renderer *renderer;
static int done;

static SDL_Texture *CreateTexture(SDL_Renderer *r, unsigned char *data, unsigned int len, int *w, int *h)
{
    SDL_Texture *texture = NULL;
    SDL_Surface *surface;
    SDL_IOStream *src = SDL_IOFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_IO(src, true);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, 255, 255, 255));

            texture = SDL_CreateTextureFromSurface(r, surface);
            *w = surface->w;
            *h = surface->h;
            SDL_DestroySurface(surface);
        }
    }
    return texture;
}

static void MoveSprites(void)
{
    int i;
    int window_w = WINDOW_WIDTH;
    int window_h = WINDOW_HEIGHT;
    SDL_FRect *position, *velocity;

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderClear(renderer);

    /* Move the sprite, bounce at the wall, and draw */
    for (i = 0; i < NUM_SPRITES; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (window_w - sprite_w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (window_h - sprite_h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }

        /* Blit the sprite onto the screen */
        SDL_RenderTexture(renderer, sprite, NULL, position);
    }

    /* Update the screen! */
    SDL_RenderPresent(renderer);
}

static void loop(void)
{
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
            done = 1;
        }
    }
    MoveSprites();
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    int return_code = -1;
    int i;

    if (argc > 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "USAGE: %s", argv[0]);
        return_code = 1;
        goto quit;
    }

    if (!SDL_CreateWindowAndRenderer("testspriteminimal", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        return_code = 2;
        goto quit;
    }

    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    sprite = CreateTexture(renderer, icon_bmp, icon_bmp_len, &sprite_w, &sprite_h);

    if (!sprite) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture (%s)", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    /* Initialize the sprite positions */
    for (i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = (float)SDL_rand(WINDOW_WIDTH - sprite_w);
        positions[i].y = (float)SDL_rand(WINDOW_HEIGHT - sprite_h);
        positions[i].w = (float)sprite_w;
        positions[i].h = (float)sprite_h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (velocities[i].x == 0.f && velocities[i].y == 0.f) {
            velocities[i].x = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
            velocities[i].y = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
        }
    }

    /* Main render loop */
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    return_code = 0;
quit:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return return_code;
}
