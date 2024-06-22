/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#define RESIZE_BORDER 20

static const SDL_Rect drag_areas[] = {
    { 20, 20, 100, 100 },
    { 200, 70, 100, 100 },
    { 400, 90, 100, 100 }
};
static const SDL_FRect render_areas[] = {
    { 20.0f, 20.0f, 100.0f, 100.0f },
    { 200.0f, 70.0f, 100.0f, 100.0f },
    { 400.0f, 90.0f, 100.0f, 100.0f }
};

static const SDL_Rect *areas = drag_areas;
static int numareas = SDL_arraysize(drag_areas);

static SDL_HitTestResult SDLCALL
hitTest(SDL_Window *window, const SDL_Point *pt, void *data)
{
    int i;
    int w, h;

    for (i = 0; i < numareas; i++) {
        if (SDL_PointInRect(pt, &areas[i])) {
            SDL_Log("HIT-TEST: DRAGGABLE\n");
            return SDL_HITTEST_DRAGGABLE;
        }
    }

    SDL_GetWindowSize(window, &w, &h);

#define REPORT_RESIZE_HIT(name)                  \
    {                                            \
        SDL_Log("HIT-TEST: RESIZE_" #name "\n"); \
        return SDL_HITTEST_RESIZE_##name;        \
    }

    if (pt->x < RESIZE_BORDER && pt->y < RESIZE_BORDER) {
        REPORT_RESIZE_HIT(TOPLEFT);
    } else if (pt->x > RESIZE_BORDER && pt->x < w - RESIZE_BORDER && pt->y < RESIZE_BORDER) {
        REPORT_RESIZE_HIT(TOP);
    } else if (pt->x > w - RESIZE_BORDER && pt->y < RESIZE_BORDER) {
        REPORT_RESIZE_HIT(TOPRIGHT);
    } else if (pt->x > w - RESIZE_BORDER && pt->y > RESIZE_BORDER && pt->y < h - RESIZE_BORDER) {
        REPORT_RESIZE_HIT(RIGHT);
    } else if (pt->x > w - RESIZE_BORDER && pt->y > h - RESIZE_BORDER) {
        REPORT_RESIZE_HIT(BOTTOMRIGHT);
    } else if (pt->x < w - RESIZE_BORDER && pt->x > RESIZE_BORDER && pt->y > h - RESIZE_BORDER) {
        REPORT_RESIZE_HIT(BOTTOM);
    } else if (pt->x < RESIZE_BORDER && pt->y > h - RESIZE_BORDER) {
        REPORT_RESIZE_HIT(BOTTOMLEFT);
    } else if (pt->x < RESIZE_BORDER && pt->y < h - RESIZE_BORDER && pt->y > RESIZE_BORDER) {
        REPORT_RESIZE_HIT(LEFT);
    }

    SDL_Log("HIT-TEST: NORMAL\n");
    return SDL_HITTEST_NORMAL;
}

int main(int argc, char **argv)
{
    int i;
    int done = 0;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    state->window_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE;

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    for (i = 0; i < state->num_windows; i++) {
        if (SDL_SetWindowHitTest(state->windows[i], hitTest, NULL) == -1) {
            SDL_Log("Enabling hit-testing failed for window %d: %s", i, SDL_GetError());
            SDL_Quit();
            return 1;
        }
    }

    while (!done) {
        SDL_Event e;
        int nothing_to_do = 1;

        for (i = 0; i < state->num_windows; ++i) {
            SDL_SetRenderDrawColor(state->renderers[i], 0, 0, 127, 255);
            SDL_RenderClear(state->renderers[i]);
            SDL_SetRenderDrawColor(state->renderers[i], 255, 0, 0, 255);
            SDLTest_DrawString(state->renderers[i], (float)state->window_w / 2 - 80.0f, 10.0f, "Drag the red boxes");
            SDL_RenderFillRects(state->renderers[i], render_areas, SDL_arraysize(render_areas));
            SDL_RenderPresent(state->renderers[i]);
        }

        while (SDL_PollEvent(&e)) {
            SDLTest_CommonEvent(state, &e, &done);
            nothing_to_do = 0;

            switch (e.type) {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                SDL_Log("button down!\n");
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                SDL_Log("button up!\n");
                break;

            case SDL_EVENT_WINDOW_MOVED:
                SDL_Log("Window event moved to (%d, %d)!\n", (int)e.window.data1, (int)e.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_ESCAPE) {
                    done = 1;
                } else if (e.key.key == SDLK_x) {
                    if (!areas) {
                        areas = drag_areas;
                        numareas = SDL_arraysize(drag_areas);
                    } else {
                        areas = NULL;
                        numareas = 0;
                    }
                }
                break;

            case SDL_EVENT_QUIT:
                done = 1;
                break;
            default:
                break;
            }
        }

        if (nothing_to_do) {
            SDL_Delay(50);
        }
    }

    SDLTest_CommonQuit(state);
    return 0;
}
