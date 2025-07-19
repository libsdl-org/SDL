/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple program: Display a texture with changeable texture address modes */

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
    SDL_Texture *texture;
} DrawState;

static DrawState *drawstates;
static SDL_TextureAddressMode u_mode = SDL_TEXTURE_ADDRESS_CLAMP;
static SDL_TextureAddressMode v_mode = SDL_TEXTURE_ADDRESS_WRAP;
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

static bool
Draw(DrawState *s)
{
    const SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
    const int indices[6] = { 0, 1, 2, 1, 2, 3 };
    SDL_Rect viewport;
    SDL_Vertex vertices[4];

    SDL_GetRenderViewport(s->renderer, &viewport);

    SDL_SetRenderDrawColorFloat(s->renderer, 0.5f, 1.0f, 0.5f, 1.0f);
    SDL_RenderClear(s->renderer);

    SDL_SetRenderTextureAddressMode(s->renderer, u_mode, v_mode);

    vertices[0].position.x = viewport.w * 0.25f;
    vertices[0].position.y = viewport.h * 0.25f;
    vertices[1].position.x = viewport.w * 0.75f;
    vertices[1].position.y = viewport.h * 0.25f;
    vertices[2].position.x = viewport.w * 0.25f;
    vertices[2].position.y = viewport.h * 0.75f;
    vertices[3].position.x = viewport.w * 0.75f;
    vertices[3].position.y = viewport.h * 0.75f;
    vertices[0].color = color;
    vertices[1].color = color;
    vertices[2].color = color;
    vertices[3].color = color;
    vertices[0].tex_coord.x = -0.1f;
    vertices[0].tex_coord.y = -0.1f;
    vertices[1].tex_coord.x =  1.1f;
    vertices[1].tex_coord.y = -0.1f;
    vertices[2].tex_coord.x = -0.1f;
    vertices[2].tex_coord.y =  1.1f;
    vertices[3].tex_coord.x =  1.1f;
    vertices[3].tex_coord.y =  1.1f;
    SDL_RenderGeometry(s->renderer, s->texture, vertices, 4, indices, 6);

    SDL_RenderPresent(s->renderer);
    return true;
}

static void loop(void)
{
    const int texture_address_count = SDL_TEXTURE_ADDRESS_WRAP + 1;
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_U) {
                u_mode = (u_mode + 1) % texture_address_count;
                SDL_Log("Texture address mode for U: %d", u_mode);
            } else if (event.key.key == SDLK_V) {
                v_mode = (v_mode + 1) % texture_address_count;
                SDL_Log("Texture address mode for V: %d", v_mode);
            } else {
                SDLTest_CommonEvent(state, &event, &done);
            }
        } else {
            SDLTest_CommonEvent(state, &event, &done);
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL) {
            continue;
        }
        if (!Draw(&drawstates[i])) {
            done = 1;
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

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    drawstates = SDL_stack_alloc(DrawState, state->num_windows);
    for (i = 0; i < state->num_windows; ++i) {
        DrawState *drawstate = &drawstates[i];

        drawstate->window = state->windows[i];
        drawstate->renderer = state->renderers[i];
        drawstate->texture = LoadTexture(drawstate->renderer, "sample.bmp", false);
        if (!drawstate->texture) {
            quit(2);
        }
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
        SDL_Log("%2.2f frames per second", fps);
    }

    SDL_stack_free(drawstates);

    quit(0);
    return 0;
}
