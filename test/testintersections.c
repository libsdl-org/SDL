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

#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test_common.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#define SWAP(typ, a, b) \
    do {                \
        typ t = a;      \
        a = b;          \
        b = t;          \
    } while (0)
#define NUM_OBJECTS 100

static SDLTest_CommonState *state;
static int num_objects;
static SDL_bool cycle_color;
static SDL_bool cycle_alpha;
static int cycle_direction = 1;
static int current_alpha = 255;
static int current_color = 255;
static SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;

static float mouse_begin_x = -1.0f, mouse_begin_y = -1.0f;

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

#define MAX_LINES 16
static int num_lines = 0;
static SDL_FRect lines[MAX_LINES];
static int add_line(float x1, float y1, float x2, float y2)
{
    if (num_lines >= MAX_LINES) {
        return 0;
    }
    if ((x1 == x2) && (y1 == y2)) {
        return 0;
    }

    SDL_Log("adding line (%g, %g), (%g, %g)\n", x1, y1, x2, y2);
    lines[num_lines].x = x1;
    lines[num_lines].y = y1;
    lines[num_lines].w = x2;
    lines[num_lines].h = y2;

    return ++num_lines;
}

static void DrawLines(SDL_Renderer *renderer)
{
    int i;
    SDL_Rect viewport;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (i = 0; i < num_lines; ++i) {
        if (i == -1) {
            SDL_RenderLine(renderer, 0.0f, 0.0f, (float)(viewport.w - 1), (float)(viewport.h - 1));
            SDL_RenderLine(renderer, 0.0f, (float)(viewport.h - 1), (float)(viewport.w - 1), 0.0f);
            SDL_RenderLine(renderer, 0.0f, (float)(viewport.h / 2), (float)(viewport.w - 1), (float)(viewport.h / 2));
            SDL_RenderLine(renderer, (float)(viewport.w / 2), 0.0f, (float)(viewport.w / 2), (float)(viewport.h - 1));
        } else {
            SDL_RenderLine(renderer, lines[i].x, lines[i].y, lines[i].w, lines[i].h);
        }
    }
}

#define MAX_RECTS 16
static int num_rects = 0;
static SDL_FRect rects[MAX_RECTS];
static int add_rect(float x1, float y1, float x2, float y2)
{
    if (num_rects >= MAX_RECTS) {
        return 0;
    }
    if ((x1 == x2) || (y1 == y2)) {
        return 0;
    }

    if (x1 > x2) {
        SWAP(float, x1, x2);
    }
    if (y1 > y2) {
        SWAP(float, y1, y2);
    }

    SDL_Log("adding rect (%g, %g), (%g, %g) [%gx%g]\n", x1, y1, x2, y2,
            x2 - x1, y2 - y1);

    rects[num_rects].x = x1;
    rects[num_rects].y = y1;
    rects[num_rects].w = x2 - x1;
    rects[num_rects].h = y2 - y1;

    return ++num_rects;
}

static void
DrawRects(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 255, 127, 0, 255);
    SDL_RenderFillRects(renderer, rects, num_rects);
}

static void
DrawRectLineIntersections(SDL_Renderer *renderer)
{
    int i, j;

    SDL_SetRenderDrawColor(renderer, 0, 255, 55, 255);

    for (i = 0; i < num_rects; i++) {
        for (j = 0; j < num_lines; j++) {
            float x1, y1, x2, y2;
            SDL_FRect r;

            r = rects[i];
            x1 = lines[j].x;
            y1 = lines[j].y;
            x2 = lines[j].w;
            y2 = lines[j].h;

            if (SDL_GetRectAndLineIntersectionFloat(&r, &x1, &y1, &x2, &y2)) {
                SDL_RenderLine(renderer, x1, y1, x2, y2);
            }
        }
    }
}

static void
DrawRectRectIntersections(SDL_Renderer *renderer)
{
    int i, j;

    SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);

    for (i = 0; i < num_rects; i++) {
        for (j = i + 1; j < num_rects; j++) {
            SDL_FRect r;
            if (SDL_GetRectIntersectionFloat(&rects[i], &rects[j], &r)) {
                SDL_RenderFillRect(renderer, &r);
            }
        }
    }
}

static void loop(void *arg)
{
    int i;
    SDL_Event event;
    int *done = (int *)arg;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, done);
        switch (event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouse_begin_x = event.button.x;
            mouse_begin_y = event.button.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == 3) {
                add_line(mouse_begin_x, mouse_begin_y, event.button.x, event.button.y);
            }
            if (event.button.button == 1) {
                add_rect(mouse_begin_x, mouse_begin_y, event.button.x, event.button.y);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
            case SDLK_L:
                if (event.key.mod & SDL_KMOD_SHIFT) {
                    num_lines = 0;
                } else {
                    add_line(
                        (float)SDL_rand(640),
                        (float)SDL_rand(480),
                        (float)SDL_rand(640),
                        (float)SDL_rand(480));
                }
                break;
            case SDLK_R:
                if (event.key.mod & SDL_KMOD_SHIFT) {
                    num_rects = 0;
                } else {
                    add_rect(
                        (float)SDL_rand(640),
                        (float)SDL_rand(480),
                        (float)SDL_rand(640),
                        (float)SDL_rand(480));
                }
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);

        DrawRects(renderer);
        DrawPoints(renderer);
        DrawRectRectIntersections(renderer);
        DrawLines(renderer);
        DrawRectLineIntersections(renderer);

        SDL_RenderPresent(renderer);
    }
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (*done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    Uint64 then, now;
    Uint32 frames;
    int done;

    /* Initialize parameters */
    num_objects = -1; /* -1 means not initialized */

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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
            } else if (num_objects < 0 && SDL_isdigit(*argv[i])) {
                char *endptr = NULL;
                num_objects = (int)SDL_strtol(argv[i], &endptr, 0);
                if (endptr != argv[i] && *endptr == '\0' && num_objects >= 0) {
                    consumed = 1;
                }
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--blend none|blend|add|mod|mul]", "[--cyclecolor]", "[--cyclealpha]", "[count]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    if (num_objects < 0) {
        num_objects = NUM_OBJECTS;
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
    then = SDL_GetTicks();
    done = 0;

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop_arg(loop, &done, 0, 1);
#else
    while (!done) {
        ++frames;
        loop(&done);
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();

    SDLTest_CommonQuit(state);

    if (now > then) {
        double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
    }
    return 0;
}
