/*
Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely.
*/
/* Simple program:  Move N sprites around on the screen as fast as possible */

#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_test_font.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#define MENU_WIDTH  120
#define MENU_HEIGHT 300

#define TOOLTIP_DELAY  500
#define TOOLTIP_WIDTH  175
#define TOOLTIP_HEIGHT 32

static SDLTest_CommonState *state;
static int num_menus;
static Uint64 tooltip_timer;
static int done;
static const SDL_Color colors[] = {
    { 0x7F, 0x00, 0x00, 0xFF },
    { 0x00, 0x7F, 0x00, 0xFF },
    { 0x00, 0x00, 0x7F, 0xFF }
};

struct PopupWindow
{
    SDL_Window *win;
    SDL_Window *parent;
    SDL_Renderer *renderer;
    int idx;
};

static struct PopupWindow *menus;
static struct PopupWindow tooltip;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc)
{
    SDL_free(menus);
    menus = NULL;

    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static int get_menu_index_by_window(SDL_Window *window)
{
    int i;
    for (i = 0; i < num_menus; ++i) {
        if (menus[i].win == window) {
            return i;
        }
    }

    return -1;
}

static SDL_bool window_is_root(SDL_Window *window)
{
    int i;
    for (i = 0; i < state->num_windows; ++i) {
        if (window == state->windows[i]) {
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

static SDL_bool create_popup(struct PopupWindow *new_popup, SDL_bool is_menu)
{
    SDL_Window *focus;
    SDL_Window *new_win;
    SDL_Renderer *new_renderer;
    const int w = is_menu ? MENU_WIDTH : TOOLTIP_WIDTH;
    const int h = is_menu ? MENU_HEIGHT : TOOLTIP_HEIGHT;
    const int v_off = is_menu ? 0 : 32;
    const SDL_WindowFlags flags = is_menu ? SDL_WINDOW_POPUP_MENU : SDL_WINDOW_TOOLTIP;
    float x, y;

    focus = SDL_GetMouseFocus();

    SDL_GetMouseState(&x, &y);
    new_win = SDL_CreatePopupWindow(focus,
                                    (int)x, (int)y + v_off, w, h, flags);

    if (new_win) {
        new_renderer = SDL_CreateRenderer(new_win, state->renderdriver);

        new_popup->win = new_win;
        new_popup->renderer = new_renderer;
        new_popup->parent = focus;

        return SDL_TRUE;
    }

    SDL_zerop(new_popup);
    return SDL_FALSE;
}

static void close_popups(void)
{
    int i;

    for (i = 0; i < num_menus; ++i) {
        /* Destroying the lowest level window recursively destroys the child windows */
        if (window_is_root(menus[i].parent)) {
            SDL_DestroyWindow(menus[i].win);
        }
    }
    SDL_free(menus);
    menus = NULL;
    num_menus = 0;

    /* If the tooltip was a child of a popup, it was recursively destroyed with the popup */
    if (!window_is_root(tooltip.parent)) {
        SDL_zero(tooltip);
    }
}

static void loop(void)
{
    int i;
    char fmt_str[128];
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            /* Hide the tooltip and restart the timer if the mouse is moved */
            if (tooltip.win) {
                SDL_DestroyWindow(tooltip.win);
                SDL_zero(tooltip);
            }
            tooltip_timer = SDL_GetTicks() + TOOLTIP_DELAY;

            if (num_menus > 0 && event.motion.windowID == SDL_GetWindowID(menus[0].parent)) {
                int x = (int)event.motion.x;
                int y = (int)event.motion.y;

                SDL_SetWindowPosition(menus[0].win, x, y);
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            /* Left click closes the popup menus */
            if (event.button.button == SDL_BUTTON_LEFT) {
                close_popups();
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                /* Create a new popup menu */
                menus = SDL_realloc(menus, sizeof(struct PopupWindow) * (num_menus + 1));
                if (create_popup(&menus[num_menus], SDL_TRUE)) {
                    ++num_menus;
                }
            }
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_SPACE) {
                for (i = 0; i < num_menus; ++i) {
                    if (SDL_GetWindowFlags(menus[i].win) & SDL_WINDOW_HIDDEN) {
                        SDL_ShowWindow(menus[i].win);
                    } else {
                        SDL_HideWindow(menus[i].win);
                    }
                }
                /* Don't process this event in SDLTest_CommonEvent() */
                continue;
            }
        }

        SDLTest_CommonEvent(state, &event, &done);
    }

    if (done) {
        return;
    }

    /* Show the tooltip if the delay period has elapsed */
    if (SDL_GetTicks() > tooltip_timer) {
        if (!tooltip.win) {
            create_popup(&tooltip, SDL_FALSE);
        }
    }

    /* Draw the window */
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    /* Draw the menus in alternating colors */
    for (i = 0; i < num_menus; ++i) {
        const SDL_Color *c = &colors[i % SDL_arraysize(colors)];
        SDL_Renderer *renderer = menus[i].renderer;

        SDL_SetRenderDrawColor(renderer, c->r, c->g, c->b, c->a);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_snprintf(fmt_str, sizeof(fmt_str), "Popup Menu %i", i);
        SDLTest_DrawString(renderer, 10.0f, 10.0f, fmt_str);
        SDL_RenderPresent(renderer);
    }

    /* Draw the tooltip */
    if (tooltip.win) {
        int menu_idx;

        SDL_SetRenderDrawColor(tooltip.renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(tooltip.renderer);
        SDL_SetRenderDrawColor(tooltip.renderer, 0xFF, 0xFF, 0xFF, 0xFF);

        menu_idx = get_menu_index_by_window(tooltip.parent);
        if (menu_idx >= 0) {
            SDL_snprintf(fmt_str, sizeof(fmt_str), "Tooltip for popup %i", menu_idx);
        } else {
            SDL_snprintf(fmt_str, sizeof(fmt_str), "Toplevel tooltip");
        }
        SDLTest_DrawString(tooltip.renderer, 10.0f, TOOLTIP_HEIGHT / 2, fmt_str);
        SDL_RenderPresent(tooltip.renderer);
    }
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

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        quit(2);
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
    quit(0);
    /* keep the compiler happy ... */
    return 0;
}
