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

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

#define NUM_SPRITES 100
#define MAX_SPEED   1

static SDLTest_CommonState *state;
static int num_sprites;
static SDL_Texture **sprites;
static SDL_bool cycle_color;
static SDL_bool cycle_alpha;
static int cycle_direction = 1;
static int current_alpha = 0;
static int current_color = 0;
static SDL_FRect *positions;
static SDL_FRect *velocities;
static float sprite_w, sprite_h;
static SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND;
static Uint64 next_fps_check;
static Uint32 frames;
static const int fps_check_delay = 5000;
static int use_rendergeometry = 0;
static SDL_bool suspend_when_occluded;

/* Number of iterations to move sprites - used for visual tests. */
/* -1: infinite random moves (default); >=0: enables N deterministic moves */
static int iterations = -1;

void SDL_AppQuit(void)
{
    SDL_free(sprites);
    SDL_free(positions);
    SDL_free(velocities);
    SDLTest_CommonQuit(state);
}

static int LoadSprite(const char *file)
{
    int i, w, h;

    for (i = 0; i < state->num_windows; ++i) {
        /* This does the SDL_LoadBMP step repeatedly, but that's OK for test code. */
        sprites[i] = LoadTexture(state->renderers[i], file, SDL_TRUE, &w, &h);
        sprite_w = (float)w;
        sprite_h = (float)h;
        if (!sprites[i]) {
            return -1;
        }
        if (SDL_SetTextureBlendMode(sprites[i], blendMode) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode: %s\n", SDL_GetError());
            SDL_DestroyTexture(sprites[i]);
            return -1;
        }
    }

    /* We're ready to roll. :) */
    return 0;
}

static void MoveSprites(SDL_Renderer *renderer, SDL_Texture *sprite)
{
    int i;
    SDL_Rect viewport;
    SDL_FRect temp;
    SDL_FRect *position, *velocity;

    /* Query the sizes */
    SDL_GetRenderViewport(renderer, &viewport);

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
        SDL_SetTextureColorMod(sprite, 255, (Uint8)current_color,
                               (Uint8)current_color);
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
        SDL_SetTextureAlphaMod(sprite, (Uint8)current_alpha);
    }

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0x00 /* used with --transparent */);
    SDL_RenderClear(renderer);

    /* Test points */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
    SDL_RenderPoint(renderer, 0.0f, 0.0f);
    SDL_RenderPoint(renderer, (float)(viewport.w - 1), 0.0f);
    SDL_RenderPoint(renderer, 0.0f, (float)(viewport.h - 1));
    SDL_RenderPoint(renderer, (float)(viewport.w - 1), (float)(viewport.h - 1));

    /* Test horizontal and vertical lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderLine(renderer, 1.0f, 0.0f, (float)(viewport.w - 2), 0.0f);
    SDL_RenderLine(renderer, 1.0f, (float)(viewport.h - 1), (float)(viewport.w - 2), (float)(viewport.h - 1));
    SDL_RenderLine(renderer, 0.0f, 1.0f, 0.0f, (float)(viewport.h - 2));
    SDL_RenderLine(renderer, (float)(viewport.w - 1), 1.0f, (float)(viewport.w - 1), (float)(viewport.h - 2));

    /* Test fill and copy */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    temp.x = 1.0f;
    temp.y = 1.0f;
    temp.w = sprite_w;
    temp.h = sprite_h;
    if (use_rendergeometry == 0) {
        SDL_RenderFillRect(renderer, &temp);
    } else {
        /* Draw two triangles, filled, uniform */
        SDL_Color color;
        SDL_Vertex verts[3];
        SDL_zeroa(verts);
        color.r = 0xFF;
        color.g = 0xFF;
        color.b = 0xFF;
        color.a = 0xFF;

        verts[0].position.x = temp.x;
        verts[0].position.y = temp.y;
        verts[0].color = color;

        verts[1].position.x = temp.x + temp.w;
        verts[1].position.y = temp.y;
        verts[1].color = color;

        verts[2].position.x = temp.x + temp.w;
        verts[2].position.y = temp.y + temp.h;
        verts[2].color = color;

        SDL_RenderGeometry(renderer, NULL, verts, 3, NULL, 0);

        verts[1].position.x = temp.x;
        verts[1].position.y = temp.y + temp.h;
        verts[1].color = color;

        SDL_RenderGeometry(renderer, NULL, verts, 3, NULL, 0);
    }
    SDL_RenderTexture(renderer, sprite, NULL, &temp);
    temp.x = viewport.w - sprite_w - 1;
    temp.y = 1.0f;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderTexture(renderer, sprite, NULL, &temp);
    temp.x = 1.0f;
    temp.y = viewport.h - sprite_h - 1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderTexture(renderer, sprite, NULL, &temp);
    temp.x = viewport.w - sprite_w - 1;
    temp.y = viewport.h - sprite_h - 1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderTexture(renderer, sprite, NULL, &temp);

    /* Test diagonal lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderLine(renderer, sprite_w, sprite_h,
                       viewport.w - sprite_w - 2, viewport.h - sprite_h - 2);
    SDL_RenderLine(renderer, viewport.w - sprite_w - 2, sprite_h,
                       sprite_w, viewport.h - sprite_h - 2);

    /* Conditionally move the sprites, bounce at the wall */
    if (iterations == -1 || iterations > 0) {
        for (i = 0; i < num_sprites; ++i) {
            position = &positions[i];
            velocity = &velocities[i];
            position->x += velocity->x;
            if ((position->x < 0) || (position->x >= (viewport.w - sprite_w))) {
                velocity->x = -velocity->x;
                position->x += velocity->x;
            }
            position->y += velocity->y;
            if ((position->y < 0) || (position->y >= (viewport.h - sprite_h))) {
                velocity->y = -velocity->y;
                position->y += velocity->y;
            }
        }

        /* Countdown sprite-move iterations and disable color changes at iteration end - used for visual tests. */
        if (iterations > 0) {
            iterations--;
            if (iterations == 0) {
                cycle_alpha = SDL_FALSE;
                cycle_color = SDL_FALSE;
            }
        }
    }

    /* Draw sprites */
    if (use_rendergeometry == 0) {
        for (i = 0; i < num_sprites; ++i) {
            position = &positions[i];

            /* Blit the sprite onto the screen */
            SDL_RenderTexture(renderer, sprite, NULL, position);
        }
    } else if (use_rendergeometry == 1) {
        /*
         *   0--1
         *   | /|
         *   |/ |
         *   3--2
         *
         *  Draw sprite2 as triangles that can be recombined as rect by software renderer
         */
        SDL_Vertex *verts = (SDL_Vertex *)SDL_malloc(num_sprites * sizeof(SDL_Vertex) * 6);
        SDL_Vertex *verts2 = verts;
        if (verts) {
            SDL_Color color;
            SDL_GetTextureColorMod(sprite, &color.r, &color.g, &color.b);
            SDL_GetTextureAlphaMod(sprite, &color.a);
            for (i = 0; i < num_sprites; ++i) {
                position = &positions[i];
                /* 0 */
                verts->position.x = position->x;
                verts->position.y = position->y;
                verts->color = color;
                verts->tex_coord.x = 0.0f;
                verts->tex_coord.y = 0.0f;
                verts++;
                /* 1 */
                verts->position.x = position->x + position->w;
                verts->position.y = position->y;
                verts->color = color;
                verts->tex_coord.x = 1.0f;
                verts->tex_coord.y = 0.0f;
                verts++;
                /* 2 */
                verts->position.x = position->x + position->w;
                verts->position.y = position->y + position->h;
                verts->color = color;
                verts->tex_coord.x = 1.0f;
                verts->tex_coord.y = 1.0f;
                verts++;
                /* 0 */
                verts->position.x = position->x;
                verts->position.y = position->y;
                verts->color = color;
                verts->tex_coord.x = 0.0f;
                verts->tex_coord.y = 0.0f;
                verts++;
                /* 2 */
                verts->position.x = position->x + position->w;
                verts->position.y = position->y + position->h;
                verts->color = color;
                verts->tex_coord.x = 1.0f;
                verts->tex_coord.y = 1.0f;
                verts++;
                /* 3 */
                verts->position.x = position->x;
                verts->position.y = position->y + position->h;
                verts->color = color;
                verts->tex_coord.x = 0.0f;
                verts->tex_coord.y = 1.0f;
                verts++;
            }

            /* Blit sprites as triangles onto the screen */
            SDL_RenderGeometry(renderer, sprite, verts2, num_sprites * 6, NULL, 0);
            SDL_free(verts2);
        }
    } else if (use_rendergeometry == 2) {
        /*   0-----1
         *   |\ A /|
         *   | \ / |
         *   |D 2 B|
         *   | / \ |
         *   |/ C \|
         *   3-----4
         *
         * Draw sprite2 as triangles that can *not* be recombined as rect by software renderer
         * Use an 'indices' array
         */
        SDL_Vertex *verts = (SDL_Vertex *)SDL_malloc(num_sprites * sizeof(SDL_Vertex) * 5);
        SDL_Vertex *verts2 = verts;
        int *indices = (int *)SDL_malloc(num_sprites * sizeof(int) * 4 * 3);
        int *indices2 = indices;
        if (verts && indices) {
            int pos = 0;
            SDL_Color color;
            SDL_GetTextureColorMod(sprite, &color.r, &color.g, &color.b);
            SDL_GetTextureAlphaMod(sprite, &color.a);
            for (i = 0; i < num_sprites; ++i) {
                position = &positions[i];
                /* 0 */
                verts->position.x = position->x;
                verts->position.y = position->y;
                verts->color = color;
                verts->tex_coord.x = 0.0f;
                verts->tex_coord.y = 0.0f;
                verts++;
                /* 1 */
                verts->position.x = position->x + position->w;
                verts->position.y = position->y;
                verts->color = color;
                verts->tex_coord.x = 1.0f;
                verts->tex_coord.y = 0.0f;
                verts++;
                /* 2 */
                verts->position.x = position->x + position->w / 2.0f;
                verts->position.y = position->y + position->h / 2.0f;
                verts->color = color;
                verts->tex_coord.x = 0.5f;
                verts->tex_coord.y = 0.5f;
                verts++;
                /* 3 */
                verts->position.x = position->x;
                verts->position.y = position->y + position->h;
                verts->color = color;
                verts->tex_coord.x = 0.0f;
                verts->tex_coord.y = 1.0f;
                verts++;
                /* 4 */
                verts->position.x = position->x + position->w;
                verts->position.y = position->y + position->h;
                verts->color = color;
                verts->tex_coord.x = 1.0f;
                verts->tex_coord.y = 1.0f;
                verts++;
                /* A */
                *indices++ = pos + 0;
                *indices++ = pos + 1;
                *indices++ = pos + 2;
                /* B */
                *indices++ = pos + 1;
                *indices++ = pos + 2;
                *indices++ = pos + 4;
                /* C */
                *indices++ = pos + 3;
                *indices++ = pos + 2;
                *indices++ = pos + 4;
                /* D */
                *indices++ = pos + 3;
                *indices++ = pos + 2;
                *indices++ = pos + 0;
                pos += 5;
            }
        }

        /* Blit sprites as triangles onto the screen */
        SDL_RenderGeometry(renderer, sprite, verts2, num_sprites * 5, indices2, num_sprites * 4 * 3);
        SDL_free(verts2);
        SDL_free(indices2);
    }

    /* Update the screen! */
    SDL_RenderPresent(renderer);
}

int SDL_AppEvent(const SDL_Event *event)
{
    return SDLTest_CommonEventMainCallbacks(state, event);
}

int SDL_AppIterate(void)
{
    Uint64 now;
    int i;
    int active_windows = 0;

    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL ||
            (suspend_when_occluded && (SDL_GetWindowFlags(state->windows[i]) & SDL_WINDOW_OCCLUDED))) {
            continue;
        }
        ++active_windows;
        MoveSprites(state->renderers[i], sprites[i]);
    }

    /* If all windows are occluded, throttle the event polling to 15hz. */
    if (!active_windows) {
        SDL_DelayNS(SDL_NS_PER_SECOND / 15);
    }

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

    return 0;  /* keep going */
}

int SDL_AppInit(int argc, char *argv[])
{
    int i;
    Uint64 seed;
    const char *icon = "icon.bmp";

    /* Initialize parameters */
    num_sprites = NUM_SPRITES;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return -1;
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
                    } else if (SDL_strcasecmp(argv[i + 1], "sub") == 0) {
                        blendMode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_SUBTRACT, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_SUBTRACT);
                        consumed = 2;
                    }
                }
            } else if (SDL_strcasecmp(argv[i], "--iterations") == 0) {
                if (argv[i + 1]) {
                    iterations = SDL_atoi(argv[i + 1]);
                    if (iterations < -1) {
                        iterations = -1;
                    }
                    consumed = 2;
                }
            } else if (SDL_strcasecmp(argv[i], "--cyclecolor") == 0) {
                cycle_color = SDL_TRUE;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--cyclealpha") == 0) {
                cycle_alpha = SDL_TRUE;
                consumed = 1;
            } else if(SDL_strcasecmp(argv[i], "--suspend-when-occluded") == 0) {
                suspend_when_occluded = SDL_TRUE;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--use-rendergeometry") == 0) {
                if (argv[i + 1]) {
                    if (SDL_strcasecmp(argv[i + 1], "mode1") == 0) {
                        /* Draw sprite2 as triangles that can be recombined as rect by software renderer */
                        use_rendergeometry = 1;
                    } else if (SDL_strcasecmp(argv[i + 1], "mode2") == 0) {
                        /* Draw sprite2 as triangles that can *not* be recombined as rect by software renderer
                         * Use an 'indices' array */
                        use_rendergeometry = 2;
                    } else {
                        return -1;
                    }
                }
                consumed = 2;
            } else if (SDL_isdigit(*argv[i])) {
                num_sprites = SDL_atoi(argv[i]);
                consumed = 1;
            } else if (argv[i][0] != '-') {
                icon = argv[i];
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--blend none|blend|add|mod|mul|sub]",
                "[--cyclecolor]",
                "[--cyclealpha]",
                "[--suspend-when-occluded]",
                "[--iterations N]",
                "[--use-rendergeometry mode1|mode2]",
                "[num_sprites]",
                "[icon.bmp]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return -1;
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        return -1;
    }

    /* Create the windows, initialize the renderers, and load the textures */
    sprites =
        (SDL_Texture **)SDL_malloc(state->num_windows * sizeof(*sprites));
    if (!sprites) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
        return -1;
    }
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }
    if (LoadSprite(icon) < 0) {
        return -1;
    }

    /* Allocate memory for the sprite info */
    positions = (SDL_FRect *)SDL_malloc(num_sprites * sizeof(*positions));
    velocities = (SDL_FRect *)SDL_malloc(num_sprites * sizeof(*velocities));
    if (!positions || !velocities) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
        return -1;
    }

    /* Position sprites and set their velocities using the fuzzer */
    if (iterations >= 0) {
        /* Deterministic seed - used for visual tests */
        seed = (Uint64)iterations;
    } else {
        /* Pseudo-random seed generated from the time */
        seed = (Uint64)time(NULL);
    }
    SDLTest_FuzzerInit(seed);
    for (i = 0; i < num_sprites; ++i) {
        positions[i].x = (float)SDLTest_RandomIntegerInRange(0, (int)(state->window_w - sprite_w));
        positions[i].y = (float)SDLTest_RandomIntegerInRange(0, (int)(state->window_h - sprite_h));
        positions[i].w = sprite_w;
        positions[i].h = sprite_h;
        velocities[i].x = 0;
        velocities[i].y = 0;
        while (!velocities[i].x && !velocities[i].y) {
            velocities[i].x = (float)SDLTest_RandomIntegerInRange(-MAX_SPEED, MAX_SPEED);
            velocities[i].y = (float)SDLTest_RandomIntegerInRange(-MAX_SPEED, MAX_SPEED);
        }
    }

    /* Main render loop in SDL_AppIterate will begin when this function returns. */
    frames = 0;
    next_fps_check = SDL_GetTicks() + fps_check_delay;

    return 0;
}

