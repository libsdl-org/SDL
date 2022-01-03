/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include "SDL.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h> /* exit() */

#ifdef __IPHONEOS__
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   480
#else
#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480
#endif

static SDL_Window *window;

typedef struct _Line {
    struct _Line *next;

    int x1, y1, x2, y2;
    Uint8 r, g, b;
} Line;

static Line *active = NULL;
static Line *lines = NULL;
static int buttons = 0;

static SDL_bool done = SDL_FALSE;

void
DrawLine(SDL_Renderer * renderer, Line * line)
{
    SDL_SetRenderDrawColor(renderer, line->r, line->g, line->b, 255);
    SDL_RenderDrawLine(renderer, line->x1, line->y1, line->x2, line->y2);
}

void
DrawLines(SDL_Renderer * renderer)
{
    Line *next = lines;
    while (next != NULL) {
        DrawLine(renderer, next);
        next = next->next;
    }
}

void
AppendLine(Line *line)
{
    if (lines) {
        Line *next = lines;
        while (next->next != NULL) {
            next = next->next;
        }
        next->next = line;
    } else {
        lines = line;
    }
}

void
loop(void *arg)
{
    SDL_Renderer *renderer = (SDL_Renderer *)arg;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEMOTION:
            if (!active)
                break;

            active->x2 = event.motion.x;
            active->y2 = event.motion.y;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (!active) {
                active = SDL_calloc(1, sizeof(*active));
                active->x1 = active->x2 = event.button.x;
                active->y1 = active->y2 = event.button.y;
            }

            switch (event.button.button) {
            case SDL_BUTTON_LEFT:   active->r = 255; buttons |= SDL_BUTTON_LMASK; break;
            case SDL_BUTTON_MIDDLE: active->g = 255; buttons |= SDL_BUTTON_MMASK; break;
            case SDL_BUTTON_RIGHT:  active->b = 255; buttons |= SDL_BUTTON_RMASK; break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (!active)
                break;

            switch (event.button.button) {
            case SDL_BUTTON_LEFT:   buttons &= ~SDL_BUTTON_LMASK; break;
            case SDL_BUTTON_MIDDLE: buttons &= ~SDL_BUTTON_MMASK; break;
            case SDL_BUTTON_RIGHT:  buttons &= ~SDL_BUTTON_RMASK; break;
            }

            if (buttons == 0) {
                AppendLine(active);
                active = NULL;
            }
            break;

        case SDL_QUIT:
            done = SDL_TRUE;
            break;

        default:
            break;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    DrawLines(renderer);
    if (active)
        DrawLine(renderer, active);

    SDL_RenderPresent(renderer);

#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int
main(int argc, char *argv[])
{
    SDL_Renderer *renderer;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("Mouse Test", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return SDL_FALSE;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return SDL_FALSE;
    }

    /* Main render loop */
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop, renderer, 0, 1);
#else
    while (!done) {
        loop(renderer);
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
