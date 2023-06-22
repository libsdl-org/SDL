/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Move N sprites around on the screen as fast as possible */

#include <stdlib.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

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

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
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
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_KEY_DOWN) {
            done = 1;
        }
    }
    MoveSprites();
#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    int i;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc > 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "USAGE: %s\n", argv[0]);
        quit(1);
    }

    if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) < 0) {
        quit(2);
    }

    sprite = LoadTexture(renderer, "icon.bmp", SDL_TRUE, &sprite_w, &sprite_h);

    if (sprite == NULL) {
        quit(2);
    }

    /* Initialize the sprite positions */
    srand((unsigned int)time(NULL));
    for (i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = (float)(rand() % (WINDOW_WIDTH - sprite_w));
        positions[i].y = (float)(rand() % (WINDOW_HEIGHT - sprite_h));
        positions[i].w = (float)sprite_w;
        positions[i].h = (float)sprite_h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (!velocities[i].x && !velocities[i].y) {
            velocities[i].x = (float)((rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED);
            velocities[i].y = (float)((rand() % (MAX_SPEED * 2 + 1)) - MAX_SPEED);
        }
    }

    /* Main render loop */
    done = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    quit(0);

    return 0; /* to prevent compiler warning */
}
