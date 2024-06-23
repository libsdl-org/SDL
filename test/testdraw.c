/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  draw as many random objects on the screen as possible */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test_common.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#define NUM_OBJECTS 100

static SDLTest_CommonState *state;
static int num_objects;
static SDL_bool cycle_color;
static SDL_bool cycle_alpha;
static int cycle_direction = 1;
static int current_alpha = 255;
static int current_color = 255;
static SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
static Uint64 next_fps_check;
static Uint32 frames;
static const int fps_check_delay = 5000;

static int done;

static void DrawPoints(SDL_Renderer *renderer)
{
    int i;
    float x, y;
    SDL_Rect viewport;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

    for (i = 0; i < num_objects * 4; ++i) {
        /* Cycle the color and alpha, if desired */
        if (cycle_color) {
            current_color += cycle_direction;
            if (current_color < 0) {
                current_color = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_color > 255) {
                current_color = 255;
                cycle_direction = -cycle_direction;
            }
        }
        if (cycle_alpha) {
            current_alpha += cycle_direction;
            if (current_alpha < 0) {
                current_alpha = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_alpha > 255) {
                current_alpha = 255;
                cycle_direction = -cycle_direction;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, (Uint8)current_color,
                               (Uint8)current_color, (Uint8)current_alpha);

        x = (float)SDL_rand(viewport.w);
        y = (float)SDL_rand(viewport.h);
        SDL_RenderPoint(renderer, x, y);
    }
}

static void DrawLines(SDL_Renderer *renderer)
{
    int i;
    float x1, y1, x2, y2;
    SDL_Rect viewport;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

    for (i = 0; i < num_objects; ++i) {
        /* Cycle the color and alpha, if desired */
        if (cycle_color) {
            current_color += cycle_direction;
            if (current_color < 0) {
                current_color = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_color > 255) {
                current_color = 255;
                cycle_direction = -cycle_direction;
            }
        }
        if (cycle_alpha) {
            current_alpha += cycle_direction;
            if (current_alpha < 0) {
                current_alpha = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_alpha > 255) {
                current_alpha = 255;
                cycle_direction = -cycle_direction;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, (Uint8)current_color,
                               (Uint8)current_color, (Uint8)current_alpha);

        if (i == 0) {
            SDL_RenderLine(renderer, 0.0f, 0.0f, (float)(viewport.w - 1), (float)(viewport.h - 1));
            SDL_RenderLine(renderer, 0.0f, (float)(viewport.h - 1), (float)(viewport.w - 1), 0.0f);
            SDL_RenderLine(renderer, 0.0f, (float)(viewport.h / 2), (float)(viewport.w - 1), (float)(viewport.h / 2));
            SDL_RenderLine(renderer, (float)(viewport.w / 2), 0.0f, (float)(viewport.w / 2), (float)(viewport.h - 1));
        } else {
            x1 = (float)(SDL_rand(viewport.w * 2) - viewport.w);
            x2 = (float)(SDL_rand(viewport.w * 2) - viewport.w);
            y1 = (float)(SDL_rand(viewport.h * 2) - viewport.h);
            y2 = (float)(SDL_rand(viewport.h * 2) - viewport.h);
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }
    }
}

static void DrawRects(SDL_Renderer *renderer)
{
    int i;
    SDL_FRect rect;
    SDL_Rect viewport;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

    for (i = 0; i < num_objects / 4; ++i) {
        /* Cycle the color and alpha, if desired */
        if (cycle_color) {
            current_color += cycle_direction;
            if (current_color < 0) {
                current_color = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_color > 255) {
                current_color = 255;
                cycle_direction = -cycle_direction;
            }
        }
        if (cycle_alpha) {
            current_alpha += cycle_direction;
            if (current_alpha < 0) {
                current_alpha = 0;
                cycle_direction = -cycle_direction;
            }
            if (current_alpha > 255) {
                current_alpha = 255;
                cycle_direction = -cycle_direction;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, (Uint8)current_color,
                               (Uint8)current_color, (Uint8)current_alpha);

        rect.w = (float)SDL_rand(viewport.h / 2);
        rect.h = (float)SDL_rand(viewport.h / 2);
        rect.x = (float)((SDL_rand(viewport.w * 2) - viewport.w) - (rect.w / 2));
        rect.y = (float)((SDL_rand(viewport.h * 2) - viewport.h) - (rect.h / 2));
        SDL_RenderFillRect(renderer, &rect);
    }
}

static void loop(void)
{
    Uint64 now;
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
    }
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);

        DrawRects(renderer);
        DrawLines(renderer);
        DrawPoints(renderer);

        SDL_RenderPresent(renderer);
    }
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
    frames++;
    now = SDL_GetTicks();
    if (now >= next_fps_check) {
        /* Print out some timing information */
        const Uint64 then = next_fps_check - fps_check_delay;
        const double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
        next_fps_check = now + fps_check_delay;
        frames = 0;
    }
}

int main(int argc, char *argv[])
{
    int i;

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize parameters */
    num_objects = NUM_OBJECTS;

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
            } else if (SDL_strcasecmp(argv[i], "--cyclecolor") == 0) {
                cycle_color = SDL_TRUE;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--cyclealpha") == 0) {
                cycle_alpha = SDL_TRUE;
                consumed = 1;
            } else if (SDL_isdigit(*argv[i])) {
                num_objects = SDL_atoi(argv[i]);
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--blend none|blend|add|mod|mul]",
                "[--cyclecolor]",
                "[--cyclealpha]",
                "[num_objects]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    /* Create the windows and initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawBlendMode(renderer, blendMode);
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }

    /* Main render loop */
    frames = 0;
    next_fps_check = SDL_GetTicks() + fps_check_delay;
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDLTest_CommonQuit(state);

    return 0;
}
