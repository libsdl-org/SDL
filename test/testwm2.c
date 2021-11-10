/*
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <stdlib.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "SDL_test_common.h"
#include "SDL_test_font.h"

static SDLTest_CommonState *state;
int done;

static const char *cursorNames[] = {
        "arrow",
        "ibeam",
        "wait",
        "crosshair",
        "waitarrow",
        "sizeNWSE",
        "sizeNESW",
        "sizeWE",
        "sizeNS",
        "sizeALL",
        "NO",
        "hand",
};
int system_cursor = -1;
SDL_Cursor *cursor = NULL;
SDL_bool relative_mode = SDL_FALSE;
int highlighted_mode = -1;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    exit(rc);
}

/* Draws the modes menu, and stores the mode index under the mouse in highlighted_mode */
static void
draw_modes_menu(SDL_Window *window, SDL_Renderer *renderer, SDL_Rect viewport)
{
    SDL_DisplayMode mode;
    char text[1024];
    const int lineHeight = 10;
    const int display_index = SDL_GetWindowDisplayIndex(window);
    const int num_modes = SDL_GetNumDisplayModes(display_index);
    int i;
    int column_chars = 0;
    int text_length;
    int x, y;
    int table_top;
    SDL_Point mouse_pos = { -1, -1 };

    /* Get mouse position */
    if (SDL_GetMouseFocus() == window) {
        int window_x, window_y;
        float logical_x, logical_y;

        SDL_GetMouseState(&window_x, &window_y);
        SDL_RenderWindowToLogical(renderer, window_x, window_y, &logical_x, &logical_y);

        mouse_pos.x = (int)logical_x;
        mouse_pos.y = (int)logical_y;
    }

    x = 0;
    y = viewport.y;

    y += lineHeight;

    SDL_snprintf(text, sizeof(text), "Click on a mode to set it with SDL_SetWindowDisplayMode");
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, x, y, text);
    y += lineHeight;

    SDL_snprintf(text, sizeof(text), "Press Ctrl+Enter to toggle SDL_WINDOW_FULLSCREEN");
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, x, y, text);
    y += lineHeight;

    table_top = y;

    /* Clear the cached mode under the mouse */
    if (window == SDL_GetMouseFocus()) {
        highlighted_mode = -1;
    }

    for (i = 0; i < num_modes; ++i) {
        SDL_Rect cell_rect;

        if (0 != SDL_GetDisplayMode(display_index, i, &mode)) {
            return;
        }

        SDL_snprintf(text, sizeof(text), "%d: %dx%d@%dHz",
            i, mode.w, mode.h, mode.refresh_rate);

        /* Update column width */
        text_length = (int)SDL_strlen(text);
        column_chars = SDL_max(column_chars, text_length);

        /* Check if under mouse */        
        cell_rect.x = x;
        cell_rect.y = y;
        cell_rect.w = text_length * FONT_CHARACTER_SIZE;
        cell_rect.h = lineHeight;

        if (SDL_PointInRect(&mouse_pos, &cell_rect)) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

            /* Update cached mode under the mouse */
            if (window == SDL_GetMouseFocus()) {
                highlighted_mode = i;
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
        }

        SDLTest_DrawString(renderer, x, y, text);
        y += lineHeight;

        if (y + lineHeight > (viewport.y + viewport.h)) {
            /* Advance to next column */
            x += (column_chars + 1) * FONT_CHARACTER_SIZE;
            y = table_top;
            column_chars = 0;
        }
    }
}

void
loop()
{
    int i;
    SDL_Event event;
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);

            if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
                    if (window) {
                        SDL_Log("Window %d resized to %dx%d\n",
                            event.window.windowID,
                            event.window.data1,
                            event.window.data2);
                    }
                }
                if (event.window.event == SDL_WINDOWEVENT_MOVED) {
                    SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
                    if (window) {
                        SDL_Log("Window %d moved to %d,%d (display %s)\n",
                            event.window.windowID,
                            event.window.data1,
                            event.window.data2,
                            SDL_GetDisplayName(SDL_GetWindowDisplayIndex(window)));
                    }
                }
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    relative_mode = SDL_GetRelativeMouseMode();
                    if (relative_mode) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    }
                }
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    if (relative_mode) {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                SDL_bool updateCursor = SDL_FALSE;

                if (event.key.keysym.sym == SDLK_LEFT) {
                    --system_cursor;
                    if (system_cursor < 0) {
                        system_cursor = SDL_NUM_SYSTEM_CURSORS - 1;
                    }
                    updateCursor = SDL_TRUE;
                } else if (event.key.keysym.sym == SDLK_RIGHT) {
                    ++system_cursor;
                    if (system_cursor >= SDL_NUM_SYSTEM_CURSORS) {
                        system_cursor = 0;
                    }
                    updateCursor = SDL_TRUE;
                }
                if (updateCursor) {
                    SDL_Log("Changing cursor to \"%s\"", cursorNames[system_cursor]);
                    SDL_FreeCursor(cursor);
                    cursor = SDL_CreateSystemCursor((SDL_SystemCursor)system_cursor);
                    SDL_SetCursor(cursor);
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                SDL_Window* window = SDL_GetMouseFocus();
                if (highlighted_mode != -1 && window != NULL) {
                    const int display_index = SDL_GetWindowDisplayIndex(window);
                    SDL_DisplayMode mode;
                    if (0 != SDL_GetDisplayMode(display_index, highlighted_mode, &mode)) {
                        SDL_Log("Couldn't get display mode");
                    } else {
                        SDL_SetWindowDisplayMode(window, &mode);
                    }
                }
            }
        }

        for (i = 0; i < state->num_windows; ++i) {
            SDL_Window* window = state->windows[i];
            SDL_Renderer *renderer = state->renderers[i];
            if (window != NULL && renderer != NULL) {
                int y = 0;
                SDL_Rect viewport, menurect;

                SDL_RenderGetViewport(renderer, &viewport);

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);

                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDLTest_CommonDrawWindowInfo(renderer, state->windows[i], &y);

                menurect.x = 0;
                menurect.y = y;
                menurect.w = viewport.w;
                menurect.h = viewport.h - y;
                draw_modes_menu(window, renderer, menurect);

                SDL_RenderPresent(renderer);
            }
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

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_assert(SDL_arraysize(cursorNames) == SDL_NUM_SYSTEM_CURSORS);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_EventState(SDL_DROPTEXT, SDL_ENABLE);

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }
 
    /* Main render loop */
    done = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    SDL_FreeCursor(cursor);

    quit(0);
    /* keep the compiler happy ... */
    return(0);
}

/* vi: set ts=4 sw=4 expandtab: */
