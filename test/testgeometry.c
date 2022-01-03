/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  draw a RGB triangle, with texture  */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "SDL_test_common.h"

static SDLTest_CommonState *state;
static SDL_bool use_texture = SDL_FALSE;
static SDL_Texture **sprites;
static SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
static double angle = 0.0;
static int sprite_w, sprite_h;

int done;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_free(sprites);
    SDLTest_CommonQuit(state);
    exit(rc);
}

int
LoadSprite(const char *file)
{
    int i;
    SDL_Surface *temp;

    /* Load the sprite image */
    temp = SDL_LoadBMP(file);
    if (temp == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", file, SDL_GetError());
        return (-1);
    }
    sprite_w = temp->w;
    sprite_h = temp->h;

    /* Set transparent pixel as the pixel at (0,0) */
    if (temp->format->palette) {
        SDL_SetColorKey(temp, 1, *(Uint8 *) temp->pixels);
    } else {
        switch (temp->format->BitsPerPixel) {
        case 15:
            SDL_SetColorKey(temp, 1, (*(Uint16 *) temp->pixels) & 0x00007FFF);
            break;
        case 16:
            SDL_SetColorKey(temp, 1, *(Uint16 *) temp->pixels);
            break;
        case 24:
            SDL_SetColorKey(temp, 1, (*(Uint32 *) temp->pixels) & 0x00FFFFFF);
            break;
        case 32:
            SDL_SetColorKey(temp, 1, *(Uint32 *) temp->pixels);
            break;
        }
    }

    /* Create textures from the image */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        sprites[i] = SDL_CreateTextureFromSurface(renderer, temp);
        if (!sprites[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s\n", SDL_GetError());
            SDL_FreeSurface(temp);
            return (-1);
        }
        if (SDL_SetTextureBlendMode(sprites[i], blendMode) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode: %s\n", SDL_GetError());
            SDL_FreeSurface(temp);
            SDL_DestroyTexture(sprites[i]);
            return (-1);
        }
    }
    SDL_FreeSurface(temp);

    /* We're ready to roll. :) */
    return (0);
}


void
loop()
{
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {

        if (event.type == SDL_MOUSEMOTION) {
            if (event.motion.state) {
                int xrel, yrel;
                int window_w, window_h;
                SDL_Window *window = SDL_GetWindowFromID(event.motion.windowID);
                SDL_GetWindowSize(window, &window_w, &window_h);
                xrel = event.motion.xrel;
                yrel = event.motion.yrel;
                if (event.motion.y < window_h / 2) {
                    angle += xrel;
                } else {
                    angle -= xrel;
                }
                if (event.motion.x < window_w / 2) {
                    angle -= yrel;
                } else {
                    angle += yrel;
                }
            }
        } else {
            SDLTest_CommonEvent(state, &event, &done);
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL)
            continue;
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);

        {
            SDL_Rect viewport;
            SDL_Vertex verts[3];
            double a;
            double d;
            int cx, cy;

            /* Query the sizes */
            SDL_RenderGetViewport(renderer, &viewport);
            SDL_zeroa(verts);
            cx = viewport.x + viewport.w / 2;
            cy = viewport.y + viewport.h / 2;
            d = (viewport.w + viewport.h) / 5;

            a = (angle * 3.1415) / 180.0; 
            verts[0].position.x = cx + d * SDL_cos(a);
            verts[0].position.y = cy + d * SDL_sin(a);
            verts[0].color.r = 0xFF;
            verts[0].color.g = 0;
            verts[0].color.b = 0;
            verts[0].color.a = 0xFF;

            a = ((angle + 120) * 3.1415) / 180.0; 
            verts[1].position.x = cx + d * SDL_cos(a);
            verts[1].position.y = cy + d * SDL_sin(a);
            verts[1].color.r = 0;
            verts[1].color.g = 0xFF;
            verts[1].color.b = 0;
            verts[1].color.a = 0xFF;

            a = ((angle + 240) * 3.1415) / 180.0; 
            verts[2].position.x = cx + d * SDL_cos(a);
            verts[2].position.y = cy + d * SDL_sin(a);
            verts[2].color.r = 0;
            verts[2].color.g = 0;
            verts[2].color.b = 0xFF;
            verts[2].color.a = 0xFF;

            if (use_texture) {
                verts[0].tex_coord.x = 0.5;
                verts[0].tex_coord.y = 0.0;
                verts[1].tex_coord.x = 1.0;
                verts[1].tex_coord.y = 1.0;
                verts[2].tex_coord.x = 0.0;
                verts[2].tex_coord.y = 1.0;
            }

            SDL_RenderGeometry(renderer, sprites[i], verts, 3, NULL, 0);
        }

        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int
main(int argc, char *argv[])
{
    int i;
    const char *icon = "icon.bmp";
    Uint32 then, now, frames;

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
                    }
                }
            } else if (SDL_strcasecmp(argv[i], "--use-texture") == 0) {
                use_texture = SDL_TRUE;
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--blend none|blend|add|mod]", "[--use-texture]", NULL };
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
        (SDL_Texture **) SDL_malloc(state->num_windows * sizeof(*sprites));
    if (!sprites) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
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


    srand((unsigned int)time(NULL));

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
        double fps = ((double) frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
    }
    
    quit(0);

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
