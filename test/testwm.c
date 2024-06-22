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
#include <SDL3/SDL_test_font.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

static SDLTest_CommonState *state;
static int done;

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
    "window top left",
    "window top",
    "window top right",
    "window right",
    "window bottom right",
    "window bottom",
    "window bottom left",
    "window left"
};
SDL_COMPILE_TIME_ASSERT(cursorNames, SDL_arraysize(cursorNames) == SDL_NUM_SYSTEM_CURSORS);

static int system_cursor = -1;
static SDL_Cursor *cursor = NULL;
static const SDL_DisplayMode *highlighted_mode = NULL;

/* Draws the modes menu, and stores the mode index under the mouse in highlighted_mode */
static void
draw_modes_menu(SDL_Window *window, SDL_Renderer *renderer, SDL_FRect viewport)
{
    const SDL_DisplayMode **modes;
    char text[1024];
    const int lineHeight = 10;
    int i, j;
    int column_chars = 0;
    int text_length;
    float x, y;
    float table_top;
    SDL_FPoint mouse_pos = { -1.0f, -1.0f };
    SDL_DisplayID *display_ids;

    /* Get mouse position */
    if (SDL_GetMouseFocus() == window) {
        float window_x, window_y;
        float logical_x, logical_y;

        SDL_GetMouseState(&window_x, &window_y);
        SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &logical_x, &logical_y);

        mouse_pos.x = logical_x;
        mouse_pos.y = logical_y;
    }

    x = 0.0f;
    y = viewport.y;

    y += lineHeight;

    SDL_strlcpy(text, "Click on a mode to set it with SDL_SetWindowFullscreenMode", sizeof(text));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, x, y, text);
    y += lineHeight;

    SDL_strlcpy(text, "Press Ctrl+Enter to toggle SDL_WINDOW_FULLSCREEN", sizeof(text));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, x, y, text);
    y += lineHeight;

    table_top = y;

    /* Clear the cached mode under the mouse */
    if (window == SDL_GetMouseFocus()) {
        highlighted_mode = NULL;
    }

    display_ids = SDL_GetDisplays(NULL);

    if (display_ids) {
        for (i = 0; display_ids[i]; ++i) {
            const SDL_DisplayID display_id = display_ids[i];
            modes = SDL_GetFullscreenDisplayModes(display_id, NULL);
            for (j = 0; modes[j]; ++j) {
                SDL_FRect cell_rect;
                const SDL_DisplayMode *mode = modes[j];

                (void)SDL_snprintf(text, sizeof(text), "%s mode %d: %dx%d@%gx %gHz",
                                   SDL_GetDisplayName(display_id),
                                   j, mode->w, mode->h, mode->pixel_density, mode->refresh_rate);

                /* Update column width */
                text_length = (int)SDL_strlen(text);
                column_chars = SDL_max(column_chars, text_length);

                /* Check if under mouse */
                cell_rect.x = x;
                cell_rect.y = y;
                cell_rect.w = (float)(text_length * FONT_CHARACTER_SIZE);
                cell_rect.h = (float)lineHeight;

                if (SDL_PointInRectFloat(&mouse_pos, &cell_rect)) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

                    /* Update cached mode under the mouse */
                    if (window == SDL_GetMouseFocus()) {
                        highlighted_mode = mode;
                    }
                } else {
                    SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
                }

                SDLTest_DrawString(renderer, x, y, text);
                y += lineHeight;

                if ((y + lineHeight) > (viewport.y + viewport.h)) {
                    /* Advance to next column */
                    x += (column_chars + 1) * FONT_CHARACTER_SIZE;
                    y = table_top;
                    column_chars = 0;
                }
            }
            SDL_free((void *)modes);
        }
        SDL_free(display_ids);
    }
}

static void loop(void)
{
    int i;
    SDL_Event event;

#ifdef TEST_WAITEVENTTIMEOUT
    /* Wait up to 20 ms for input, as a test */
    Uint64 then = SDL_GetTicks();
    if (SDL_WaitEventTimeout(NULL, 20)) {
        SDL_Log("Got an event!\n");
    }
    Uint64 now = SDL_GetTicks();
    SDL_Log("Waited %d ms for events\n", (int)(now - then));
#endif

    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);

        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
            if (window) {
                SDL_Log("Window %" SDL_PRIu32 " resized to %" SDL_PRIs32 "x%" SDL_PRIs32 "\n",
                        event.window.windowID,
                        event.window.data1,
                        event.window.data2);
            }
        }
        if (event.type == SDL_EVENT_WINDOW_MOVED) {
            SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
            if (window) {
                SDL_Log("Window %" SDL_PRIu32 " moved to %" SDL_PRIs32 ",%" SDL_PRIs32 " (display %s)\n",
                        event.window.windowID,
                        event.window.data1,
                        event.window.data2,
                        SDL_GetDisplayName(SDL_GetDisplayForWindow(window)));
            }
        }
        if (event.type == SDL_EVENT_KEY_UP) {
            SDL_bool updateCursor = SDL_FALSE;

            if (event.key.key == SDLK_a) {
                SDL_assert(!"Keyboard generated assert");
            } else if (event.key.key == SDLK_LEFT) {
                --system_cursor;
                if (system_cursor < 0) {
                    system_cursor = SDL_NUM_SYSTEM_CURSORS - 1;
                }
                updateCursor = SDL_TRUE;
            } else if (event.key.key == SDLK_RIGHT) {
                ++system_cursor;
                if (system_cursor >= SDL_NUM_SYSTEM_CURSORS) {
                    system_cursor = 0;
                }
                updateCursor = SDL_TRUE;
            }
            if (updateCursor) {
                SDL_Log("Changing cursor to \"%s\"", cursorNames[system_cursor]);
                SDL_DestroyCursor(cursor);
                cursor = SDL_CreateSystemCursor((SDL_SystemCursor)system_cursor);
                SDL_SetCursor(cursor);
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            SDL_Window *window = SDL_GetMouseFocus();
            if (highlighted_mode && window) {
                SDL_memcpy(&state->fullscreen_mode, highlighted_mode, sizeof(state->fullscreen_mode));
                SDL_SetWindowFullscreenMode(window, highlighted_mode);
            }
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Window *window = state->windows[i];
        SDL_Renderer *renderer = state->renderers[i];
        if (window && renderer) {
            float y = 0.0f;
            SDL_Rect viewport;
            SDL_FRect menurect;

            SDL_GetRenderViewport(renderer, &viewport);

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDLTest_CommonDrawWindowInfo(renderer, state->windows[i], &y);

            menurect.x = 0.0f;
            menurect.y = y;
            menurect.w = (float)viewport.w;
            menurect.h = (float)viewport.h - y;
            draw_modes_menu(window, renderer, menurect);

            SDL_Delay(16);
            SDL_RenderPresent(renderer);
        }
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

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }

    /* Main render loop */
    done = 0;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    SDL_DestroyCursor(cursor);

    SDLTest_CleanupTextDrawing();
    SDLTest_CommonQuit(state);
    return 0;
}
