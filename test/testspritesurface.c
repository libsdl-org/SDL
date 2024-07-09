/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program: Test surface blitting. */

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

static SDL_Surface *sprite;
static SDL_Rect positions[NUM_SPRITES];
static SDL_Rect velocities[NUM_SPRITES];
static int sprite_w, sprite_h;

SDL_Window *window;
SDL_Surface *window_surf;
static int done;

static SDL_Surface *CreateSurface(unsigned char *data, unsigned int len, int *w, int *h) {
    SDL_Surface *surface = NULL;
    SDL_IOStream *src = SDL_IOFromConstMem(data, len);
    if (src) {
        surface = SDL_LoadBMP_IO(src, SDL_TRUE);
        if (surface) {
            /* Treat white as transparent */
            SDL_SetSurfaceColorKey(surface, SDL_TRUE, SDL_MapSurfaceRGB(surface, 255, 255, 255));

            *w = surface->w;
            *h = surface->h;
        }
    }
    return surface;
}

static void MoveSprites(void)
{
    int i;
    int window_w = WINDOW_WIDTH;
    int window_h = WINDOW_HEIGHT;
    SDL_Rect *position, *velocity;
    Uint32 background = SDL_MapSurfaceRGB(window_surf, 0xA0, 0xA0, 0xA0);
    SDL_FillSurfaceRect(window_surf, NULL, background);

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
        SDL_BlitSurface(sprite, NULL, window_surf, position);
    }

    /* Update the screen! */
    SDL_UpdateWindowSurface(window);
}

static void loop(void)
{
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN) {
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
    int return_code = -1;
    int i;

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc > 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "USAGE: %s\n", argv[0]);
        return_code = 1;
        goto quit;
    }

    if ((window = SDL_CreateWindow("testspritesurface", WINDOW_WIDTH, WINDOW_HEIGHT, 0)) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window (%s)", SDL_GetError());
        return_code = 2;
        goto quit;
    }

    if ((window_surf = SDL_GetWindowSurface(window)) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't acquire window surface (%s)", SDL_GetError());
        return_code = 3;
        goto quit;
    }

    sprite = CreateSurface(icon_bmp, icon_bmp_len, &sprite_w, &sprite_h);

    if (!sprite) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create surface (%s)", SDL_GetError());
        return_code = 4;
        goto quit;
    }

    /* Initialize the sprite positions */
    for (i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = SDL_rand(WINDOW_WIDTH - sprite_w);
        positions[i].y = SDL_rand(WINDOW_HEIGHT - sprite_h);
        positions[i].w = sprite_w;
        positions[i].h = sprite_h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (velocities[i].x == 0.f && velocities[i].y == 0.f) {
            velocities[i].x = (SDL_rand(MAX_SPEED * 2 + 1)) - MAX_SPEED;
            velocities[i].y = (SDL_rand(MAX_SPEED * 2 + 1)) - MAX_SPEED;
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
    if (sprite) {
        SDL_DestroySurface(sprite);
        sprite = NULL;
    }
    if (window_surf) {
        SDL_DestroyWindowSurface(window);
        window_surf = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
    return return_code;
}
