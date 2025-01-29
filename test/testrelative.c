/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program:  Test relative mouse motion */

#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

static SDLTest_CommonState *state;
static int i, done;
static SDL_FRect rect;
static SDL_Event event;
static bool warp;

static void DrawRects(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    if (SDL_GetWindowRelativeMouseMode(SDL_GetRenderWindow(renderer))) {
        SDLTest_DrawString(renderer, 0.f, 0.f, "Relative Mode: Enabled");
    } else {
        SDLTest_DrawString(renderer, 0.f, 0.f, "Relative Mode: Disabled");
    }
}

static void CenterMouse(void)
{
    /* Warp the mouse back to the center of the window with input focus to use the
     * center point for calculating future motion deltas.
     *
     * NOTE: DO NOT DO THIS IN REAL APPS/GAMES!
     *
     *       This is an outdated method of handling relative pointer motion, and
     *       may not work properly, if at all, on some platforms. It is here *only*
     *       for testing the warp emulation code path internal to SDL.
     *
     *       Relative mouse mode should be used instead!
     */
    SDL_Window *window = SDL_GetKeyboardFocus();
    if (window) {
        int w, h;
        float cx, cy;

        SDL_GetWindowSize(window, &w, &h);
        cx = (float)w / 2.f;
        cy = (float)h / 2.f;

        SDL_WarpMouseInWindow(window, cx, cy);
    }
}

static void loop(void)
{
    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
        switch (event.type) {
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (warp) {
                /* This should activate relative mode for warp emulation, unless disabled via a hint. */
                CenterMouse();
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_C) {
                /* If warp emulation is active, showing the cursor should turn
                 * relative mode off, and it should re-activate after a warp
                 * when hidden again.
                 */
                if (SDL_CursorVisible()) {
                    SDL_HideCursor();
                } else {
                    SDL_ShowCursor();
                }
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
        {
            rect.x += event.motion.xrel;
            rect.y += event.motion.yrel;

            if (warp) {
                CenterMouse();
            }
        } break;
        default:
            break;
        }
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Rect viewport;
        SDL_Renderer *renderer = state->renderers[i];
        if (state->windows[i] == NULL) {
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        /* Wrap the cursor rectangle at the screen edges to keep it visible */
        SDL_GetRenderViewport(renderer, &viewport);
        if (rect.x < viewport.x) {
            rect.x += viewport.w;
        }
        if (rect.y < viewport.y) {
            rect.y += viewport.h;
        }
        if (rect.x > viewport.x + viewport.w) {
            rect.x -= viewport.w;
        }
        if (rect.y > viewport.y + viewport.h) {
            rect.y -= viewport.h;
        }

        DrawRects(renderer);

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
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--warp") == 0) {
                warp = true;
                consumed = 1;
            }
        }

        if (consumed < 0) {
            static const char *options[] = {
                "[--warp]",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }
        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        return 2;
    }

    /* Create the windows and initialize the renderers */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }

    /* If warp mode is activated, the cursor will be repeatedly warped back to
     * the center of the window to simulate the behavior of older games. The cursor
     * is initially hidden in this case to trigger the warp emulation unless it has
     * been explicitly disabled via a hint.
     *
     * Otherwise, try to activate relative mode.
     */
    if (warp) {
        SDL_HideCursor();
    } else {
        for (i = 0; i < state->num_windows; ++i) {
            SDL_SetWindowRelativeMouseMode(state->windows[i], true);
        }
    }

    rect.x = DEFAULT_WINDOW_WIDTH / 2;
    rect.y = DEFAULT_WINDOW_HEIGHT / 2;
    rect.w = 10;
    rect.h = 10;
    /* Main render loop */
    done = 0;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif
    SDLTest_CommonQuit(state);
    return 0;
}
