/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/* Stolen from the mailing list */
/* Creates a new mouse cursor from an XPM */

/* XPM */
static const char *arrow[] = {
    /* width height num_colors chars_per_pixel */
    "    32    32        3            1",
    /* colors */
    "X c #000000",
    ". c #ffffff",
    "  c None",
    /* pixels */
    "X                               ",
    "XX                              ",
    "X.X                             ",
    "X..X                            ",
    "X...X                           ",
    "X....X                          ",
    "X.....X                         ",
    "X......X                        ",
    "X.......X                       ",
    "X........X                      ",
    "X.....XXXXX                     ",
    "X..X..X                         ",
    "X.X X..X                        ",
    "XX  X..X                        ",
    "X    X..X                       ",
    "     X..X                       ",
    "      X..X                      ",
    "      X..X                      ",
    "       XX                       ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "0,0"
};

static SDL_Cursor *
init_color_cursor(const char *file)
{
    SDL_Cursor *cursor = NULL;
    SDL_Surface *surface = SDL_LoadBMP(file);
    if (surface) {
        if (surface->format->palette) {
            SDL_SetColorKey(surface, 1, *(Uint8 *)surface->pixels);
        } else {
            switch (surface->format->BitsPerPixel) {
            case 15:
                SDL_SetColorKey(surface, 1, (*(Uint16 *)surface->pixels) & 0x00007FFF);
                break;
            case 16:
                SDL_SetColorKey(surface, 1, *(Uint16 *)surface->pixels);
                break;
            case 24:
                SDL_SetColorKey(surface, 1, (*(Uint32 *)surface->pixels) & 0x00FFFFFF);
                break;
            case 32:
                SDL_SetColorKey(surface, 1, *(Uint32 *)surface->pixels);
                break;
            }
        }
        cursor = SDL_CreateColorCursor(surface, 0, 0);
        SDL_FreeSurface(surface);
    }
    return cursor;
}

static SDL_Cursor *
init_system_cursor(const char *image[])
{
    int i, row, col;
    Uint8 data[4 * 32];
    Uint8 mask[4 * 32];
    int hot_x = 0;
    int hot_y = 0;

    i = -1;
    for (row = 0; row < 32; ++row) {
        for (col = 0; col < 32; ++col) {
            if (col % 8) {
                data[i] <<= 1;
                mask[i] <<= 1;
            } else {
                ++i;
                data[i] = mask[i] = 0;
            }
            switch (image[4 + row][col]) {
            case 'X':
                data[i] |= 0x01;
                mask[i] |= 0x01;
                break;
            case '.':
                mask[i] |= 0x01;
                break;
            case ' ':
                break;
            }
        }
    }
    (void)SDL_sscanf(image[4 + row], "%d,%d", &hot_x, &hot_y);
    return SDL_CreateCursor(data, mask, 32, 32, hot_x, hot_y);
}

static SDLTest_CommonState *state;
int done;
static SDL_Cursor *cursors[1 + SDL_NUM_SYSTEM_CURSORS];
static SDL_SystemCursor cursor_types[1 + SDL_NUM_SYSTEM_CURSORS];
static int num_cursors;
static int current_cursor;
static int show_cursor;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    exit(rc);
}

void loop()
{
    int i;
    SDL_Event event;
    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (num_cursors == 0) {
                    continue;
                }

                ++current_cursor;
                if (current_cursor == num_cursors) {
                    current_cursor = 0;
                }

                SDL_SetCursor(cursors[current_cursor]);

                switch ((int)cursor_types[current_cursor]) {
                case (SDL_SystemCursor)-1:
                    SDL_Log("Custom cursor");
                    break;
                case SDL_SYSTEM_CURSOR_ARROW:
                    SDL_Log("Arrow");
                    break;
                case SDL_SYSTEM_CURSOR_IBEAM:
                    SDL_Log("I-beam");
                    break;
                case SDL_SYSTEM_CURSOR_WAIT:
                    SDL_Log("Wait");
                    break;
                case SDL_SYSTEM_CURSOR_CROSSHAIR:
                    SDL_Log("Crosshair");
                    break;
                case SDL_SYSTEM_CURSOR_WAITARROW:
                    SDL_Log("Small wait cursor (or Wait if not available)");
                    break;
                case SDL_SYSTEM_CURSOR_SIZENWSE:
                    SDL_Log("Double arrow pointing northwest and southeast");
                    break;
                case SDL_SYSTEM_CURSOR_SIZENESW:
                    SDL_Log("Double arrow pointing northeast and southwest");
                    break;
                case SDL_SYSTEM_CURSOR_SIZEWE:
                    SDL_Log("Double arrow pointing west and east");
                    break;
                case SDL_SYSTEM_CURSOR_SIZENS:
                    SDL_Log("Double arrow pointing north and south");
                    break;
                case SDL_SYSTEM_CURSOR_SIZEALL:
                    SDL_Log("Four pointed arrow pointing north, south, east, and west");
                    break;
                case SDL_SYSTEM_CURSOR_NO:
                    SDL_Log("Slashed circle or crossbones");
                    break;
                case SDL_SYSTEM_CURSOR_HAND:
                    SDL_Log("Hand");
                    break;
                default:
                    SDL_Log("UNKNOWN CURSOR TYPE, FIX THIS PROGRAM.");
                    break;
                }

            } else {
                show_cursor = !show_cursor;
                SDL_ShowCursor(show_cursor);
            }
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    const char *color_cursor = NULL;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (state == NULL) {
        return 1;
    }
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            color_cursor = argv[i];
            break;
        }
        if (consumed < 0) {
            SDLTest_CommonLogUsage(state, argv[0], NULL);
            quit(1);
        }
        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }

    num_cursors = 0;

    if (color_cursor) {
        SDL_Cursor *cursor = init_color_cursor(color_cursor);
        if (cursor) {
            cursors[num_cursors] = cursor;
            cursor_types[num_cursors] = (SDL_SystemCursor)-1;
            num_cursors++;
        }
    } else {
        SDL_Cursor *cursor = init_system_cursor(arrow);
        if (cursor) {
            cursors[num_cursors] = cursor;
            cursor_types[num_cursors] = (SDL_SystemCursor)-1;
            num_cursors++;
        }
    }

    for (i = 0; i < SDL_NUM_SYSTEM_CURSORS; ++i) {
        SDL_Cursor *cursor = SDL_CreateSystemCursor((SDL_SystemCursor)i);
        if (cursor) {
            cursors[num_cursors] = cursor;
            cursor_types[num_cursors] = i;
            num_cursors++;
        }
    }

    if (num_cursors > 0) {
        SDL_SetCursor(cursors[0]);
    }

    show_cursor = SDL_ShowCursor(SDL_QUERY);

    /* Main render loop */
    done = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    for (i = 0; i < num_cursors; ++i) {
        SDL_FreeCursor(cursors[i]);
    }
    quit(0);

    /* keep the compiler happy ... */
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
