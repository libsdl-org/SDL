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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

static SDLTest_CommonState *state;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    SDL_Texture *sprite;
    SDL_FRect sprite_rect;
    int scale_direction;
} DrawState;

static DrawState *drawstates;
static int done;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static void Draw(DrawState *s)
{
    SDL_Rect viewport;

    SDL_GetRenderViewport(s->renderer, &viewport);

    /* Draw the background */
    SDL_RenderTexture(s->renderer, s->background, NULL, NULL);

    /* Scale and draw the sprite */
    s->sprite_rect.w += s->scale_direction;
    s->sprite_rect.h += s->scale_direction;
    if (s->scale_direction > 0) {
        if (s->sprite_rect.w >= viewport.w || s->sprite_rect.h >= viewport.h) {
            s->scale_direction = -1;
        }
    } else {
        if (s->sprite_rect.w <= 1 || s->sprite_rect.h <= 1) {
            s->scale_direction = 1;
        }
    }
    s->sprite_rect.x = (float)((viewport.w - s->sprite_rect.w) / 2);
    s->sprite_rect.y = (float)((viewport.h - s->sprite_rect.h) / 2);

    SDL_RenderTexture(s->renderer, s->sprite, NULL, &s->sprite_rect);

    /* Update the screen! */
    SDL_RenderPresent(s->renderer);
}

static void loop(void)
{
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
    }
    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL) {
            continue;
        }
        Draw(&drawstates[i]);
    }
#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    int frames;
    Uint64 then, now;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        quit(1);
    }

    drawstates = SDL_stack_alloc(DrawState, state->num_windows);
    for (i = 0; i < state->num_windows; ++i) {
        DrawState *drawstate = &drawstates[i];
        int w, h;

        drawstate->window = state->windows[i];
        drawstate->renderer = state->renderers[i];
        drawstate->sprite = LoadTexture(drawstate->renderer, "icon.bmp", SDL_TRUE, NULL, NULL);
        drawstate->background = LoadTexture(drawstate->renderer, "sample.bmp", SDL_FALSE, NULL, NULL);
        if (!drawstate->sprite || !drawstate->background) {
            quit(2);
        }
        SDL_QueryTexture(drawstate->sprite, NULL, NULL, &w, &h);
        drawstate->sprite_rect.w = (float)w;
        drawstate->sprite_rect.h = (float)h;
        drawstate->scale_direction = 1;
    }

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        ++frames;
        loop();
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
    }

    SDL_stack_free(drawstates);

    quit(0);
    return 0;
}
