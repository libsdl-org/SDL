/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test of window content scale/DPI handling. */

#include "SDL.h"
#include <math.h>

#include "SDL_test.h"
#include "SDL_test_common.h"

static SDLTest_CommonState *state;
static int quitting = 0;

static void 
dump_info(SDL_Window* window)
{
    int w, h, pix_w, pix_h, window_id;
    float scale_h, scale_v;

    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pix_w, &pix_h);
    SDL_GetWindowContentScale(window, &scale_h, &scale_v);
    window_id = SDL_GetWindowID(window);

    SDL_Log("Window: %d\n", window_id);
    SDL_Log("Size: %dx%d \n", w, h);
    SDL_Log("Pixel Size: %dx%d \n", pix_w, pix_h);
    SDL_Log("Content Scale: %fx%f \n", scale_h, scale_v);
}

static void
process_event(SDL_Event event)
{
    SDLTest_CommonEvent(state, &event, &quitting);

    switch (event.type) {
    case SDL_WINDOWEVENT: {
        SDL_Window* window = SDL_GetWindowFromID(event.window.windowID);

        switch (event.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            SDL_Log("Size changed, dumping info:\n");
            dump_info(window);
            break;
        }
        break;
    }
    }
}

static void
loop(void)
{
    SDL_Event event;
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer;
        SDL_Window *window;
        float scale_h, scale_v;
        int mouse_x, mouse_y;
        int size_h, size_v;
        SDL_Rect mouse_rect;

        if (!state->renderers[i]) {
            continue;
        }

        /* Render a red square under the cursor, properly scaled of course. */
        renderer = state->renderers[i];
        window = state->windows[i];

        SDL_GetWindowContentScale(window, &scale_h, &scale_v);

        SDL_GetMouseState(&mouse_x, &mouse_y);

        mouse_x = (int) (mouse_x * scale_h);
        mouse_y = (int) (mouse_y * scale_v);

        size_h = (int) (30 * scale_h);
        size_v = (int) (30 * scale_v);

        mouse_rect.x = mouse_x - size_h / 2;
        mouse_rect.y = mouse_y - size_v / 2;
        mouse_rect.w = size_h;
        mouse_rect.h = size_v;

        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
        SDL_RenderFillRect(renderer, &mouse_rect);

        SDL_RenderPresent(renderer);
    }

    if (SDL_WaitEventTimeout(&event, 10)) {
        process_event(event);
    }

    while (SDL_PollEvent(&event)) {
        process_event(event);
    }
}

int
main(int argc, char *argv[])
{
    int i;
   
    SDL_SetHint("SDL_WINDOWS_DPI_SCALING", "1");

    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    state->window_title = "DPI test";
    state->window_w = 1280;
    state->window_h = 720;
    state->window_flags |= SDL_WINDOW_RESIZABLE;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    for (i = 0; i < state->num_windows; ++i) {
        dump_info(state->windows[i]);
    }

    while (!quitting) {
        loop();
    }

    SDLTest_CommonQuit(state);
    return 0;
}

/* end of testdpi.c ... */
