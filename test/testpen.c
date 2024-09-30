/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>

typedef struct Pen
{
    SDL_PenID pen;
    Uint8 r, g, b;
    float axes[SDL_PEN_AXIS_COUNT];
    float x;
    float y;
    Uint32 buttons;
    bool eraser;
    bool touching;
    struct Pen *next;
} Pen;

static SDL_Renderer *renderer = NULL;
static SDLTest_CommonState *state = NULL;
static SDL_Texture *white_pixel = NULL;
static Pen pens;


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;

    SDL_srand(0);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (consumed <= 0) {
            static const char *options[] = {
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            SDL_Quit();
            SDLTest_CommonDestroyState(state);
            return 1;
        }
        i += consumed;
    }

    state->num_windows = 1;

    /* Load the SDL library */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    renderer = state->renderers[0];
    if (!renderer) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return SDL_APP_FAILURE;
    }

    white_pixel = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 16, 16);
    if (!white_pixel) {
        SDL_Log("Couldn't create white_pixel texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    } else {
        const SDL_Rect rect = { 0, 0, 16, 16 };
        Uint32 pixels[16 * 16];
        SDL_memset(pixels, 0xFF, sizeof (pixels));
        SDL_UpdateTexture(white_pixel, &rect, pixels, 16 * sizeof (Uint32));
    }

    SDL_HideCursor();

    return SDL_APP_CONTINUE;
}

static Pen *FindPen(SDL_PenID which)
{
    Pen *i;
    for (i = pens.next; i != NULL; i = i->next) {
        if (i->pen == which) {
            return i;
        }
    }
    return NULL;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    Pen *pen = NULL;

    switch (event->type) {
        case SDL_EVENT_PEN_PROXIMITY_IN: {
            pen = (Pen *) SDL_calloc(1, sizeof (*pen));
            if (!pen) {
                SDL_Log("Out of memory!");
                return SDL_APP_FAILURE;
            }

            SDL_Log("Pen %" SDL_PRIu32 " enters proximity!", event->pproximity.which);
            pen->pen = event->pproximity.which;
            pen->r = (Uint8) SDL_rand(256);
            pen->g = (Uint8) SDL_rand(256);
            pen->b = (Uint8) SDL_rand(256);
            pen->x = 320.0f;
            pen->y = 240.0f;
            pen->next = pens.next;
            pens.next = pen;

            return SDL_APP_CONTINUE;
        }

        case SDL_EVENT_PEN_PROXIMITY_OUT: {
            Pen *prev = &pens;
            Pen *i;

            SDL_Log("Pen %" SDL_PRIu32 " leaves proximity!", event->pproximity.which);
            for (i = pens.next; i != NULL; i = i->next) {
                if (i->pen == event->pproximity.which) {
                    prev->next = i->next;
                    SDL_free(i);
                    break;
                }
                prev = i;
            }

            return SDL_APP_CONTINUE;
        }

        case SDL_EVENT_PEN_DOWN:
            /*SDL_Log("Pen %" SDL_PRIu32 " down!", event->ptouch.which);*/
            pen = FindPen(event->ptouch.which);
            if (pen) {
                pen->touching = true;
                pen->eraser = (event->ptouch.eraser != 0);
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_UP:
            /*SDL_Log("Pen %" SDL_PRIu32 " up!", event->ptouch.which);*/
            pen = FindPen(event->ptouch.which);
            if (pen) {
                pen->touching = false;
                pen->axes[SDL_PEN_AXIS_PRESSURE] = 0.0f;
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_BUTTON_DOWN:
            /*SDL_Log("Pen %" SDL_PRIu32 " button %d down!", event->pbutton.which, (int) event->pbutton.button);*/
            pen = FindPen(event->ptouch.which);
            if (pen) {
                pen->buttons |= (1 << (event->pbutton.button-1));
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_BUTTON_UP:
            /*SDL_Log("Pen %" SDL_PRIu32 " button %d up!", event->pbutton.which, (int) event->pbutton.button);*/
            pen = FindPen(event->ptouch.which);
            if (pen) {
                pen->buttons &= ~(1 << (event->pbutton.button-1));
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_MOTION:
            /*SDL_Log("Pen %" SDL_PRIu32 " moved to (%f,%f)!", event->pmotion.which, event->pmotion.x, event->pmotion.y);*/
            pen = FindPen(event->ptouch.which);
            if (pen) {
                pen->x = event->pmotion.x;
                pen->y = event->pmotion.y;
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_AXIS:
            /*SDL_Log("Pen %" SDL_PRIu32 " axis %d is now %f!", event->paxis.which, (int) event->paxis.axis, event->paxis.value);*/
            pen = FindPen(event->ptouch.which);
            if (pen && (event->paxis.axis < SDL_arraysize(pen->axes))) {
                pen->axes[event->paxis.axis] = event->paxis.value;
            }
            return SDL_APP_CONTINUE;

        case SDL_EVENT_KEY_DOWN: {
            const SDL_Keycode sym = event->key.key;
            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                SDL_Log("Key : Escape!");
                return SDL_APP_SUCCESS;
            }
            break;
        }

        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        default:
            break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

static void DrawOnePen(Pen *pen, int num)
{
    int i;

    /* draw button presses for this pen. A square for each in the pen's color, offset down the screen so they don't overlap. */
    SDL_SetRenderDrawColor(renderer, pen->r, pen->g, pen->b, 255);
    for (i = 0; i < 8; i++) {   /* we assume you don't have more than 8 buttons atm... */
        if (pen->buttons & (1 << i)) {
            const SDL_FRect rect = { 30.0f * ((float) i), ((float) num) * 30.0f, 30.0f, 30.0f };
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    /* draw a square to represent pressure. Always green for eraser and blue for pen */
    /* we do this with a texture, so we can trivially rotate it, which SDL_RenderFillRect doesn't offer. */
    if (pen->axes[SDL_PEN_AXIS_PRESSURE] > 0.0f) {
        const float size = (150.0f * pen->axes[SDL_PEN_AXIS_PRESSURE]) + 20.0f;
        const float halfsize = size / 2.0f;
        const SDL_FRect rect = { pen->x - halfsize, pen->y - halfsize, size, size };
        const SDL_FPoint center = { halfsize, halfsize };
        if (pen->eraser) {
            SDL_SetTextureColorMod(white_pixel, 0, 255, 0);
        } else {
            SDL_SetTextureColorMod(white_pixel, 0, 0, 255);
        }
        SDL_RenderTextureRotated(renderer, white_pixel, NULL, &rect, pen->axes[SDL_PEN_AXIS_ROTATION], &center, SDL_FLIP_NONE);
    }

    /* draw a little square for position in the center of the pressure, with the pen-specific color. */
    {
        const float distance = pen->touching ? 0.0f : SDL_clamp(pen->axes[SDL_PEN_AXIS_DISTANCE], 0.0f, 1.0f);
        const float size = 10 + (30.0f * (1.0f - distance));
        const float halfsize = size / 2.0f;
        const SDL_FRect rect = { pen->x - halfsize, pen->y - halfsize, size, size };
        const SDL_FPoint center = { halfsize, halfsize };
        SDL_SetTextureColorMod(white_pixel, pen->r, pen->g, pen->b);
        SDL_RenderTextureRotated(renderer, white_pixel, NULL, &rect, pen->axes[SDL_PEN_AXIS_ROTATION], &center, SDL_FLIP_NONE);
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    int num = 0;
    Pen *pen;

    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);

    for (pen = pens.next; pen != NULL; pen = pen->next, num++) {
        DrawOnePen(pen, num);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    Pen *i, *next;
    for (i = pens.next; i != NULL; i = next) {
        next = i->next;
        SDL_free(i);
    }
    pens.next = NULL;
    SDL_DestroyTexture(white_pixel);
    SDLTest_CommonQuit(state);
}

