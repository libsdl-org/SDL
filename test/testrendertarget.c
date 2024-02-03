/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program:  Move N sprites around on the screen as fast as possible */

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

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
static SDL_bool test_composite = SDL_FALSE;

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

static SDL_bool
DrawComposite(DrawState *s)
{
    SDL_Rect viewport;
    SDL_FRect R;
    SDL_Texture *target;
    SDL_Surface *surface;

    static SDL_bool blend_tested = SDL_FALSE;
    if (!blend_tested) {
        SDL_Texture *A, *B;

        A = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 1, 1);
        SDL_SetTextureBlendMode(A, SDL_BLENDMODE_BLEND);

        B = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 1, 1);
        SDL_SetTextureBlendMode(B, SDL_BLENDMODE_BLEND);

        SDL_SetRenderTarget(s->renderer, A);
        SDL_SetRenderDrawColor(s->renderer, 0x00, 0x00, 0x00, 0x80);
        SDL_RenderFillRect(s->renderer, NULL);

        SDL_SetRenderTarget(s->renderer, B);
        SDL_SetRenderDrawColor(s->renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderFillRect(s->renderer, NULL);
        SDL_RenderTexture(s->renderer, A, NULL, NULL);

        surface = SDL_RenderReadPixels(s->renderer, NULL);
        if (surface) {
            Uint8 r, g, b, a;
            if (SDL_ReadSurfacePixel(surface, 0, 0, &r, &g, &b, &a) == 0) {
                SDL_Log("Blended pixel: 0x%.2x%.2x%.2x%.2x\n", r, g, b, a);
            }
            SDL_DestroySurface(surface);
        }

        SDL_DestroyTexture(A);
        SDL_DestroyTexture(B);
        blend_tested = SDL_TRUE;
    }

    SDL_GetRenderViewport(s->renderer, &viewport);

    target = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, viewport.w, viewport.h);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(s->renderer, target);

    /* Draw the background.
       This is solid black so when the sprite is copied to it, any per-pixel alpha will be blended through.
     */
    SDL_SetRenderDrawColor(s->renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderFillRect(s->renderer, NULL);

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

    SDL_SetRenderTarget(s->renderer, NULL);
    SDL_RenderTexture(s->renderer, s->background, NULL, NULL);

    SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(s->renderer, 0xff, 0x00, 0x00, 0x80);
    R.x = 0.0f;
    R.y = 0.0f;
    R.w = 100.0f;
    R.h = 100.0f;
    SDL_RenderFillRect(s->renderer, &R);
    SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_NONE);

    SDL_RenderTexture(s->renderer, target, NULL, NULL);
    SDL_DestroyTexture(target);

    /* Update the screen! */
    SDL_RenderPresent(s->renderer);
    return SDL_TRUE;
}

static SDL_bool
Draw(DrawState *s)
{
    SDL_Rect viewport;
    SDL_Texture *target;

    SDL_GetRenderViewport(s->renderer, &viewport);

    target = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, viewport.w, viewport.h);
    if (!target) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create render target texture: %s\n", SDL_GetError());
        return SDL_FALSE;
    }
    SDL_SetRenderTarget(s->renderer, target);

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

    SDL_SetRenderTarget(s->renderer, NULL);
    SDL_RenderTexture(s->renderer, target, NULL, NULL);
    SDL_DestroyTexture(target);

    /* Update the screen! */
    SDL_RenderPresent(s->renderer);
    return SDL_TRUE;
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
        if (test_composite) {
            if (!DrawComposite(&drawstates[i])) {
                done = 1;
            }
        } else {
            if (!Draw(&drawstates[i])) {
                done = 1;
            }
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
    int frames;
    Uint64 then, now;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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
            if (SDL_strcasecmp(argv[i], "--composite") == 0) {
                test_composite = SDL_TRUE;
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--composite]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    drawstates = SDL_stack_alloc(DrawState, state->num_windows);
    for (i = 0; i < state->num_windows; ++i) {
        DrawState *drawstate = &drawstates[i];
        int w, h;

        drawstate->window = state->windows[i];
        drawstate->renderer = state->renderers[i];
        if (test_composite) {
            drawstate->sprite = LoadTexture(drawstate->renderer, "icon-alpha.bmp", SDL_TRUE, NULL, NULL);
        } else {
            drawstate->sprite = LoadTexture(drawstate->renderer, "icon.bmp", SDL_TRUE, NULL, NULL);
        }
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

#ifdef SDL_PLATFORM_EMSCRIPTEN
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
