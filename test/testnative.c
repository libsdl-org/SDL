/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Create a native window and attach an SDL renderer */

#include "testnative.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include "testutils.h"

#include <stdlib.h>

#define WINDOW_W    640
#define WINDOW_H    480
#define NUM_SPRITES 100
#define MAX_SPEED   1

static NativeWindowFactory *factories[] = {
#ifdef TEST_NATIVE_WINDOWS
    &WindowsWindowFactory,
#endif
#ifdef TEST_NATIVE_WAYLAND
    &WaylandWindowFactory,
#endif
#ifdef TEST_NATIVE_X11
    &X11WindowFactory,
#endif
#ifdef TEST_NATIVE_COCOA
    &CocoaWindowFactory,
#endif
    NULL
};
static NativeWindowFactory *factory = NULL;
static void *native_window;
static SDL_FRect *positions, *velocities;
static SDLTest_CommonState *state;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    if (native_window && factory) {
        factory->DestroyNativeWindow(native_window);
    }
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static void MoveSprites(SDL_Renderer *renderer, SDL_Texture *sprite)
{
    int i;
    SDL_Rect viewport;
    SDL_FRect *position, *velocity;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderClear(renderer);

    /* Move the sprite, bounce at the wall, and draw */
    for (i = 0; i < NUM_SPRITES; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (viewport.w - sprite->w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (viewport.h - sprite->h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }

        /* Blit the sprite onto the screen */
        SDL_RenderTexture(renderer, sprite, NULL, position);
    }

    /* Update the screen! */
    SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{
    int i, done;
    const char *driver;
    SDL_PropertiesID props;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *sprite;
    int window_w, window_h;
    SDL_Event event;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL video: %s\n",
                     SDL_GetError());
        exit(1);
    }
    driver = SDL_GetCurrentVideoDriver();

    /* Find a native window driver and create a native window */
    for (i = 0; factories[i]; ++i) {
        if (SDL_strcmp(driver, factories[i]->tag) == 0) {
            factory = factories[i];
            break;
        }
    }
    if (!factory) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find native window code for %s driver\n",
                     driver);
        quit(2);
    }
    SDL_Log("Creating native window for %s driver\n", driver);
    native_window = factory->CreateNativeWindow(WINDOW_W, WINDOW_H);
    if (!native_window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create native window\n");
        quit(3);
    }
    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, "sdl2-compat.external_window", native_window);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, WINDOW_W);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, WINDOW_H);
    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create SDL window: %s\n", SDL_GetError());
        quit(4);
    }
    SDL_SetWindowTitle(window, "SDL Native Window Test");

    /* Create the renderer */
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        quit(5);
    }

    /* Clear the window, load the sprite and go! */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderClear(renderer);

    sprite = LoadTexture(renderer, "icon.bmp", true, NULL, NULL);
    if (!sprite) {
        quit(6);
    }

    /* Allocate memory for the sprite info */
    SDL_GetWindowSize(window, &window_w, &window_h);
    positions = (SDL_FRect *)SDL_malloc(NUM_SPRITES * sizeof(*positions));
    velocities = (SDL_FRect *)SDL_malloc(NUM_SPRITES * sizeof(*velocities));
    if (!positions || !velocities) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
        quit(2);
    }
    for (i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = (float)SDL_rand(window_w - sprite->w);
        positions[i].y = (float)SDL_rand(window_h - sprite->h);
        positions[i].w = (float)sprite->w;
        positions[i].h = (float)sprite->h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (velocities[i].x == 0.f && velocities[i].y == 0.f) {
            velocities[i].x = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
            velocities[i].y = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
        }
    }

    /* Main render loop */
    done = 0;
    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            if (state->verbose & VERBOSE_EVENT) {
                if (((event.type != SDL_EVENT_MOUSE_MOTION) &&
                    (event.type != SDL_EVENT_FINGER_MOTION)) ||
                    (state->verbose & VERBOSE_MOTION)) {
                    SDLTest_PrintEvent(&event);
                }
            }

            switch (event.type) {
            case SDL_EVENT_WINDOW_EXPOSED:
                SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
                SDL_RenderClear(renderer);
                break;
            case SDL_EVENT_QUIT:
                done = 1;
                break;
            default:
                break;
            }
        }
        MoveSprites(renderer, sprite);
    }

    SDL_DestroyTexture(sprite);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_free(positions);
    SDL_free(velocities);

    quit(0);

    return 0; /* to prevent compiler warning */
}
