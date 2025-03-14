/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  draw a RGB triangle, with texture  */

#include <stdlib.h>

#include "testutils.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test_common.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

static SDLTest_CommonState *state;
static bool use_texture = false;
static SDL_Texture **sprites;
static SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
static float angle = 0.0f;
static int translate_cx = 0;
static int translate_cy = 0;

static int done;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_free(sprites);
    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static int LoadSprite(const char *file)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        /* This does the SDL_LoadBMP step repeatedly, but that's OK for test code. */
        sprites[i] = LoadTexture(state->renderers[i], file, true);
        if (!sprites[i]) {
            return -1;
        }
        if (!SDL_SetTextureBlendMode(sprites[i], blendMode)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode: %s", SDL_GetError());
            SDL_DestroyTexture(sprites[i]);
            return -1;
        }
    }

    /* We're ready to roll. :) */
    return 0;
}

static void loop(void)
{
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {

        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            if (event.motion.state) {
                float xrel, yrel;
                int window_w, window_h;
                SDL_Window *window = SDL_GetWindowFromID(event.motion.windowID);
                SDL_GetWindowSize(window, &window_w, &window_h);
                xrel = event.motion.xrel;
                yrel = event.motion.yrel;
                if (event.motion.y < (float)window_h / 2.0f) {
                    angle += xrel;
                } else {
                    angle -= xrel;
                }
                if (event.motion.x < (float)window_w / 2.0f) {
                    angle -= yrel;
                } else {
                    angle += yrel;
                }
            }
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_LEFT) {
                translate_cx -= 1;
            } else if (event.key.key == SDLK_RIGHT) {
                translate_cx += 1;
            } else if (event.key.key == SDLK_UP) {
                translate_cy -= 1;
            } else if (event.key.key == SDLK_DOWN) {
                translate_cy += 1;
            } else {
                SDLTest_CommonEvent(state, &event, &done);
            }
        } else {
            SDLTest_CommonEvent(state, &event, &done);
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);

        {
            SDL_Rect viewport;
            SDL_Vertex verts[3];
            float a;
            float d;
            int cx, cy;

            /* Query the sizes */
            SDL_GetRenderViewport(renderer, &viewport);
            SDL_zeroa(verts);
            cx = viewport.x + viewport.w / 2;
            cy = viewport.y + viewport.h / 2;
            d = (viewport.w + viewport.h) / 5.f;

            cx += translate_cx;
            cy += translate_cy;

            a = (angle * SDL_PI_F) / 180.0f;
            verts[0].position.x = cx + d * SDL_cosf(a);
            verts[0].position.y = cy + d * SDL_sinf(a);
            verts[0].color.r = 1.0f;
            verts[0].color.g = 0;
            verts[0].color.b = 0;
            verts[0].color.a = 1.0f;

            a = ((angle + 120) * SDL_PI_F) / 180.0f;
            verts[1].position.x = cx + d * SDL_cosf(a);
            verts[1].position.y = cy + d * SDL_sinf(a);
            verts[1].color.r = 0;
            verts[1].color.g = 1.0f;
            verts[1].color.b = 0;
            verts[1].color.a = 1.0f;

            a = ((angle + 240) * SDL_PI_F) / 180.0f;
            verts[2].position.x = cx + d * SDL_cosf(a);
            verts[2].position.y = cy + d * SDL_sinf(a);
            verts[2].color.r = 0;
            verts[2].color.g = 0;
            verts[2].color.b = 1.0f;
            verts[2].color.a = 1.0f;

            if (use_texture) {
                verts[0].tex_coord.x = 0.5f;
                verts[0].tex_coord.y = 0.0f;
                verts[1].tex_coord.x = 1.0f;
                verts[1].tex_coord.y = 1.0f;
                verts[2].tex_coord.x = 0.0f;
                verts[2].tex_coord.y = 1.0f;
            }

            SDL_RenderGeometry(renderer, sprites[i], verts, 3, NULL, 0);
        }

        SDL_RenderPresent(renderer);
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
    const char *icon = "icon.bmp";
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
            if (SDL_strcasecmp(argv[i], "--blend") == 0) {
                if (argv[i + 1]) {
                    if (SDL_strcasecmp(argv[i + 1], "none") == 0) {
                        blendMode = SDL_BLENDMODE_NONE;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "blend") == 0) {
                        blendMode = SDL_BLENDMODE_BLEND;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "add") == 0) {
                        blendMode = SDL_BLENDMODE_ADD;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "mod") == 0) {
                        blendMode = SDL_BLENDMODE_MOD;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "mul") == 0) {
                        blendMode = SDL_BLENDMODE_MUL;
                        consumed = 2;
                    }
                }
            } else if (SDL_strcasecmp(argv[i], "--use-texture") == 0) {
                use_texture = true;
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--blend none|blend|add|mod|mul]", "[--use-texture]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    /* Create the windows, initialize the renderers, and load the textures */
    sprites =
        (SDL_Texture **)SDL_malloc(state->num_windows * sizeof(*sprites));
    if (!sprites) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        quit(2);
    }
    /* Create the windows and initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawBlendMode(renderer, blendMode);
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
        sprites[i] = NULL;
    }
    if (use_texture) {
        if (LoadSprite(icon) < 0) {
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

    quit(0);

    return 0;
}
