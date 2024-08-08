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

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDLTest_CommonState *state = NULL;
static float pressure = 0.0f;
static float position_x = 320.0f;
static float position_y = 240.0f;
static Uint32 buttons = 0;
static SDL_bool eraser = SDL_FALSE;

int SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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

    window = state->windows[0];
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    renderer = state->renderers[0];
    if (!renderer) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

int SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_PEN_PROXIMITY_IN:
            SDL_Log("Pen %" SDL_PRIu32 " enters proximity!", event->pproximity.which);
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_PROXIMITY_OUT:
            SDL_Log("Pen %" SDL_PRIu32 " leaves proximity!", event->pproximity.which);
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_DOWN:
            SDL_Log("Pen %" SDL_PRIu32 " down!", event->ptouch.which);
            eraser = (event->ptouch.eraser != 0);
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_UP:
            SDL_Log("Pen %" SDL_PRIu32 " up!", event->ptouch.which);
            pressure = 0.0f;
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_BUTTON_DOWN:
            /*SDL_Log("Pen %" SDL_PRIu32 " button %d down!", event->pbutton.which, (int) event->pbutton.button);*/
            buttons |= (1 << event->pbutton.button);
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_BUTTON_UP:
            /*SDL_Log("Pen %" SDL_PRIu32 " button %d up!", event->pbutton.which, (int) event->pbutton.button);*/
            buttons &= ~(1 << event->pbutton.button);
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_MOTION:
            /*SDL_Log("Pen %" SDL_PRIu32 " moved to (%f,%f)!", event->pmotion.which, event->pmotion.x, event->pmotion.y);*/
            position_x = event->pmotion.x;
            position_y = event->pmotion.y;
            return SDL_APP_CONTINUE;

        case SDL_EVENT_PEN_AXIS:
            /*SDL_Log("Pen %" SDL_PRIu32 " axis %d is now %f!", event->paxis.which, (int) event->paxis.axis, event->paxis.value);*/
            if (event->paxis.axis == SDL_PEN_AXIS_PRESSURE) {
                pressure = event->paxis.value;
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

int SDL_AppIterate(void *appstate)
{
    int i;

    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    for (i = 0; i < 30; i++) {
        if (buttons & (1 << i)) {
            const SDL_FRect rect = { 30.0f * ((float) i), 0.0f, 30.0f, 30.0f };
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    const float size = SDL_max(50.0f * pressure, 10.0f);
    const float halfsize = size / 2.0f;
    const SDL_FRect rect = { position_x - halfsize, position_y - halfsize, size, size };

    if (eraser) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    }
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate)
{
    SDLTest_CommonQuit(state);
}

