/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

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

static const char *cross[] = {
    /* width height num_colors chars_per_pixel */
    "    32    32        3            1",
    /* colors */
    "o c #000000",
    ". c #ffffff",
    "  c None",
    /* pixels */
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "    oooooooooooooooooooooooo    ",
    "    oooooooooooooooooooooooo    ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "               oo               ",
    "                                ",
    "                                ",
    "                                ",
    "                                ",
    "0,0"
};

static SDL_Surface *load_image_file(const char *file)
{
    SDL_Surface *surface = SDL_LoadBMP(file);
    if (surface) {
        if (SDL_GetSurfacePalette(surface)) {
            const Uint8 bpp = SDL_BITSPERPIXEL(surface->format);
            const Uint8 mask = (1 << bpp) - 1;
            if (SDL_PIXELORDER(surface->format) == SDL_BITMAPORDER_4321)
                SDL_SetSurfaceColorKey(surface, 1, (*(Uint8 *)surface->pixels) & mask);
            else
                SDL_SetSurfaceColorKey(surface, 1, ((*(Uint8 *)surface->pixels) >> (8 - bpp)) & mask);
        } else {
            switch (SDL_BITSPERPIXEL(surface->format)) {
            case 15:
                SDL_SetSurfaceColorKey(surface, 1, (*(Uint16 *)surface->pixels) & 0x00007FFF);
                break;
            case 16:
                SDL_SetSurfaceColorKey(surface, 1, *(Uint16 *)surface->pixels);
                break;
            case 24:
                SDL_SetSurfaceColorKey(surface, 1, (*(Uint32 *)surface->pixels) & 0x00FFFFFF);
                break;
            case 32:
                SDL_SetSurfaceColorKey(surface, 1, *(Uint32 *)surface->pixels);
                break;
            }
        }
    }
    return surface;
}

static SDL_Surface *load_image(const char *file)
{
    SDL_Surface *surface = load_image_file(file);
    if (surface) {
        /* Add a 2x version of this image, if available */
        SDL_Surface *surface2x = NULL;
        const char *ext = SDL_strrchr(file, '.');
        size_t len = SDL_strlen(file) + 2 + 1;
        char *file2x = (char *)SDL_malloc(len);
        if (file2x) {
            SDL_strlcpy(file2x, file, len);
            if (ext) {
                SDL_memcpy(file2x + (ext - file), "2x", 3);
                SDL_strlcat(file2x, ext, len);
            } else {
                SDL_strlcat(file2x, "2x", len);
            }
            surface2x = load_image_file(file2x);
            SDL_free(file2x);
        }
        if (surface2x) {
            SDL_AddSurfaceAlternateImage(surface, surface2x);
            SDL_DestroySurface(surface2x);
        }
    }
    return surface;
}

static SDL_Cursor *init_color_cursor(const char *file)
{
    SDL_Cursor *cursor = NULL;
    SDL_Surface *surface = load_image(file);
    if (surface) {
        cursor = SDL_CreateColorCursor(surface, 0, 0);
        SDL_DestroySurface(surface);
    }
    return cursor;
}

static SDL_Cursor *init_system_cursor(const char *image[])
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
            case 'o':
                data[i] |= 0x01;
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
static int done;
static SDL_Cursor *cursors[3 + SDL_SYSTEM_CURSOR_COUNT];
static SDL_SystemCursor cursor_types[3 + SDL_SYSTEM_CURSOR_COUNT];
static int num_cursors;
static int current_cursor;
static bool show_cursor;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static void loop(void)
{
    int i;
    SDL_Event event;
    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
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
                case SDL_SYSTEM_CURSOR_DEFAULT:
                    SDL_Log("Default");
                    break;
                case SDL_SYSTEM_CURSOR_TEXT:
                    SDL_Log("Text");
                    break;
                case SDL_SYSTEM_CURSOR_WAIT:
                    SDL_Log("Wait");
                    break;
                case SDL_SYSTEM_CURSOR_CROSSHAIR:
                    SDL_Log("Crosshair");
                    break;
                case SDL_SYSTEM_CURSOR_PROGRESS:
                    SDL_Log("Progress: Small wait cursor (or Wait if not available)");
                    break;
                case SDL_SYSTEM_CURSOR_NWSE_RESIZE:
                    SDL_Log("Double arrow pointing northwest and southeast");
                    break;
                case SDL_SYSTEM_CURSOR_NESW_RESIZE:
                    SDL_Log("Double arrow pointing northeast and southwest");
                    break;
                case SDL_SYSTEM_CURSOR_EW_RESIZE:
                    SDL_Log("Double arrow pointing west and east");
                    break;
                case SDL_SYSTEM_CURSOR_NS_RESIZE:
                    SDL_Log("Double arrow pointing north and south");
                    break;
                case SDL_SYSTEM_CURSOR_MOVE:
                    SDL_Log("Move: Four pointed arrow pointing north, south, east, and west");
                    break;
                case SDL_SYSTEM_CURSOR_NOT_ALLOWED:
                    SDL_Log("Not Allowed: Slashed circle or crossbones");
                    break;
                case SDL_SYSTEM_CURSOR_POINTER:
                    SDL_Log("Pointer: Hand");
                    break;
                case SDL_SYSTEM_CURSOR_NW_RESIZE:
                    SDL_Log("Window resize top-left");
                    break;
                case SDL_SYSTEM_CURSOR_N_RESIZE:
                    SDL_Log("Window resize top");
                    break;
                case SDL_SYSTEM_CURSOR_NE_RESIZE:
                    SDL_Log("Window resize top-right");
                    break;
                case SDL_SYSTEM_CURSOR_E_RESIZE:
                    SDL_Log("Window resize right");
                    break;
                case SDL_SYSTEM_CURSOR_SE_RESIZE:
                    SDL_Log("Window resize bottom-right");
                    break;
                case SDL_SYSTEM_CURSOR_S_RESIZE:
                    SDL_Log("Window resize bottom");
                    break;
                case SDL_SYSTEM_CURSOR_SW_RESIZE:
                    SDL_Log("Window resize bottom-left");
                    break;
                case SDL_SYSTEM_CURSOR_W_RESIZE:
                    SDL_Log("Window resize left");
                    break;
                default:
                    SDL_Log("UNKNOWN CURSOR TYPE, FIX THIS PROGRAM.");
                    break;
                }

            } else {
                show_cursor = !show_cursor;
                if (show_cursor) {
                    SDL_ShowCursor();
                } else {
                    SDL_HideCursor();
                }
            }
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_FRect rect;
        int x, y, row;
        int window_w = 0, window_h = 0;

        SDL_GetWindowSize(state->windows[i], &window_w, &window_h);
        rect.w = 128.0f;
        rect.h = 128.0f;
        for (y = 0, row = 0; y < window_h; y += (int)rect.h, ++row) {
            bool black = ((row % 2) == 0) ? true : false;
            for (x = 0; x < window_w; x += (int)rect.w) {
                rect.x = (float)x;
                rect.y = (float)y;

                if (black) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
                }
                SDL_RenderFillRect(renderer, &rect);

                black = !black;
            }
        }
        SDL_RenderPresent(renderer);
    }
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    const char *color_cursor = NULL;
    SDL_Cursor *cursor;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
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

    num_cursors = 0;

    if (color_cursor) {
        SDL_Surface *icon = load_image(color_cursor);
        if (icon) {
            for (i = 0; i < state->num_windows; ++i) {
                SDL_SetWindowIcon(state->windows[i], icon);
            }
            SDL_DestroySurface(icon);
        }

        cursor = init_color_cursor(color_cursor);
        if (cursor) {
            cursors[num_cursors] = cursor;
            cursor_types[num_cursors] = (SDL_SystemCursor)-1;
            num_cursors++;
        }
    }

    cursor = init_system_cursor(arrow);
    if (cursor) {
        cursors[num_cursors] = cursor;
        cursor_types[num_cursors] = (SDL_SystemCursor)-1;
        num_cursors++;
    }

    cursor = init_system_cursor(cross);
    if (cursor) {
        cursors[num_cursors] = cursor;
        cursor_types[num_cursors] = (SDL_SystemCursor)-1;
        num_cursors++;
    }

    for (i = 0; i < SDL_SYSTEM_CURSOR_COUNT; ++i) {
        cursor = SDL_CreateSystemCursor((SDL_SystemCursor)i);
        if (cursor) {
            cursors[num_cursors] = cursor;
            cursor_types[num_cursors] = i;
            num_cursors++;
        }
    }

    if (num_cursors > 0) {
        SDL_SetCursor(cursors[0]);
    }

    show_cursor = SDL_CursorVisible();

    /* Main render loop */
    done = 0;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    for (i = 0; i < num_cursors; ++i) {
        SDL_DestroyCursor(cursors[i]);
    }
    quit(0);

    /* keep the compiler happy ... */
    return 0;
}
