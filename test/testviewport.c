/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Check viewports */

#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

static SDLTest_CommonState *state;

static SDL_Rect viewport;
static int done, j;
static bool use_target = false;
#ifdef SDL_PLATFORM_EMSCRIPTEN
static Uint32 wait_start;
#endif
static SDL_Texture *sprite;
static int sprite_w, sprite_h;

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

static void DrawOnViewport(SDL_Renderer *renderer)
{
    SDL_FRect rect;
    SDL_Rect cliprect;

    /* Set the viewport */
    SDL_SetRenderViewport(renderer, &viewport);

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
    SDL_RenderClear(renderer);

    /* Test inside points */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, 0xFF);
    SDL_RenderPoint(renderer, (float)(viewport.h / 2 + 20), (float)(viewport.w / 2));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2 - 20), (float)(viewport.w / 2));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2), (float)(viewport.w / 2 - 20));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2), (float)(viewport.w / 2 + 20));

    /* Test horizontal and vertical lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderLine(renderer, 1.0f, 0.0f, (float)(viewport.w - 2), 0.0f);
    SDL_RenderLine(renderer, 1.0f, (float)(viewport.h - 1), (float)(viewport.w - 2), (float)(viewport.h - 1));
    SDL_RenderLine(renderer, 0.0f, 1.0f, 0.0f, (float)(viewport.h - 2));
    SDL_RenderLine(renderer, (float)(viewport.w - 1), 1.0f, (float)(viewport.w - 1), (float)(viewport.h - 2));

    /* Test diagonal lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xFF, 0xFF);
    SDL_RenderLine(renderer, 0.0f, 0.0f, (float)(viewport.w - 1), (float)(viewport.h - 1));
    SDL_RenderLine(renderer, (float)(viewport.w - 1), 0.0f, 0.0f, (float)(viewport.h - 1));

    /* Test outside points */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, 0xFF);
    SDL_RenderPoint(renderer, (float)(viewport.h / 2 + viewport.h), (float)(viewport.w / 2));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2 - viewport.h), (float)(viewport.w / 2));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2), (float)(viewport.w / 2 - viewport.w));
    SDL_RenderPoint(renderer, (float)(viewport.h / 2), (float)(viewport.w / 2 + viewport.w));

    /* Add a box at the top */
    rect.w = 8.0f;
    rect.h = 8.0f;
    rect.x = (float)((viewport.w - rect.w) / 2);
    rect.y = 0.0f;
    SDL_RenderFillRect(renderer, &rect);

    /* Add a clip rect and fill it with the sprite */
    cliprect.x = (viewport.w - sprite->w) / 2;
    cliprect.y = (viewport.h - sprite->h) / 2;
    cliprect.w = sprite->w;
    cliprect.h = sprite->h;
    SDL_RectToFRect(&cliprect, &rect);
    SDL_SetRenderClipRect(renderer, &cliprect);
    SDL_RenderTexture(renderer, sprite, NULL, &rect);
    SDL_SetRenderClipRect(renderer, NULL);
}

static void loop(void)
{
    SDL_Event event;
    int i;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    /* Avoid using delays */
    if (SDL_GetTicks() - wait_start < 1000) {
        return;
    }
    wait_start = SDL_GetTicks();
#endif
    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
    }

    /* Move a viewport box in steps around the screen */
    viewport.x = j * 100;
    viewport.y = viewport.x;
    viewport.w = 100 + j * 50;
    viewport.h = 100 + j * 50;
    j = (j + 1) % 4;
    SDL_Log("Current Viewport x=%i y=%i w=%i h=%i", viewport.x, viewport.y, viewport.w, viewport.h);

    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL) {
            continue;
        }

        /* Draw using viewport */
        DrawOnViewport(state->renderers[i]);

        /* Update the screen! */
        if (use_target) {
            SDL_SetRenderTarget(state->renderers[i], NULL);
            SDL_RenderTexture(state->renderers[i], state->targets[i], NULL, NULL);
            SDL_RenderPresent(state->renderers[i]);
            SDL_SetRenderTarget(state->renderers[i], state->targets[i]);
        } else {
            SDL_RenderPresent(state->renderers[i]);
        }
    }

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    Uint64 then, now;
    Uint32 frames;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--target") == 0) {
                use_target = true;
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--target]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    sprite = LoadTexture(state->renderers[0], "icon.bmp", true, &sprite_w, &sprite_h);

    if (!sprite) {
        quit(2);
    }

    if (use_target) {
        int w, h;

        for (i = 0; i < state->num_windows; ++i) {
            SDL_GetWindowSize(state->windows[i], &w, &h);
            state->targets[i] = SDL_CreateTexture(state->renderers[i], SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
            SDL_SetRenderTarget(state->renderers[i], state->targets[i]);
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;
    j = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    wait_start = SDL_GetTicks();
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        ++frames;
        loop();
        SDL_Delay(1000);
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second", fps);
    }
    quit(0);
    return 0;
}
