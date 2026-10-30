/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Sample program: Create and test dockable windows */

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <stdlib.h>

#define MAIN_WIN_WIDTH  640
#define MAIN_WIN_HEIGHT 480
#define DOCK_WIN_WIDTH  320
#define DOCK_WIN_HEIGHT 240

static SDL_Window *mainWindow = NULL, *dragWindow = NULL;
static SDL_Renderer *mainRenderer = NULL, *dragRenderer = NULL;
static SDLTest_CommonState *state = NULL;
static int dw_x, dw_y;
static int i;

static void DrawDockable(SDL_Renderer *renderer, float x, float y)
{
    SDL_SetRenderDrawColor(renderer, 128, 200, 128, SDL_ALPHA_OPAQUE);
    const SDL_FRect rect = { x, y, DOCK_WIN_WIDTH, DOCK_WIN_HEIGHT };
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDLTest_DrawString(renderer, rect.x + rect.w / 2, rect.y + rect.h / 2, "Drag Me");
}

static SDL_HitTestResult DragHitCallback(SDL_Window *win, const SDL_Point *area, void *data)
{
    return SDL_HITTEST_DRAGGABLE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        const int consumed = SDLTest_CommonArg(state, i);

        if (consumed <= 0) {
            static const char *options[] = { NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Main Window", 640, 480, 0, &mainWindow, &mainRenderer)) {
        SDL_Log("Failed to create main window and/or renderer: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* Main window is gray */
    if (mainRenderer) {
        SDL_SetRenderDrawColor(mainRenderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(mainRenderer);
        if (!dragRenderer) {
            DrawDockable(mainRenderer, (float)dw_x, (float)dw_y);
        }
        SDL_RenderPresent(mainRenderer);
    }
    if (dragRenderer) {
        DrawDockable(dragRenderer, 0.f, 0.f);
        SDL_RenderPresent(dragRenderer);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (event->window.windowID == SDL_GetWindowID(mainWindow)) {
            return SDL_APP_SUCCESS;
        }
        break;

    case SDL_EVENT_DROP_BEGIN:
        if (dragWindow) {
            SDL_SetWindowOpacity(dragWindow, 0.5f);
        }
        break;

    case SDL_EVENT_DROP_POSITION:
        if (event->drop.dropWindowID) {
            SDL_Window *w = SDL_GetWindowFromID(event->drop.dropWindowID);
            SDL_SetWindowOpacity(w, 0.5f);
        }
        break;

    case SDL_EVENT_DROP_WINDOW:
    {
        /* Using the window position here is unreliable, as the window isn't necessarily changing position
         * while being dragged. The origin of the window relative to the pointer is calculated with the
         * drag offset property.
         */
        SDL_PropertiesID props = SDL_GetWindowProperties(dragWindow);
        const int x = SDL_lroundf(event->drop.x) - (int)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_DRAG_OFFSET_X_NUMBER, 0);
        const int y = SDL_lroundf(event->drop.y) - (int)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_DRAG_OFFSET_Y_NUMBER, 0);

        // If the dockable window is entirely within the parent, destroy the window and render it locally.
        if (x >= 0 && x + DOCK_WIN_WIDTH < MAIN_WIN_WIDTH && y >= 0 && y + DOCK_WIN_HEIGHT < MAIN_WIN_HEIGHT) {
            dw_x = x;
            dw_y = y;
            SDL_DestroyWindow(dragWindow);
            dragWindow = NULL;
            dragRenderer = NULL;

            SDL_Log("Window docked at (%i, %i)", dw_x, dw_y);
        }

    } break;

    case SDL_EVENT_DROP_COMPLETE:
        if (dragWindow) {
            SDL_SetWindowOpacity(dragWindow, 1.0f);
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
        if (event->button.button == SDL_BUTTON_LEFT &&
            event->button.x >= dw_x && event->button.x < dw_x + DOCK_WIN_WIDTH &&
            event->button.y >= dw_y && event->button.y < dw_y + DOCK_WIN_HEIGHT) {
            if (!dragWindow) {
                SDL_PropertiesID props = SDL_CreateProperties();
                SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
                SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
                SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, true);
                SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_DOCKABLE_BOOLEAN, true);
                SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_PARENT_POINTER, mainWindow);
                SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 320);
                SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 240);

                int wx, wy;
                SDL_GetWindowPosition(mainWindow, &wx, &wy);

                wx += dw_x;
                wy += dw_y;
                SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, wx);
                SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, wy);
                dragWindow = SDL_CreateWindowWithProperties(props);
                SDL_DestroyProperties(props);
                dragRenderer = SDL_CreateRenderer(dragWindow, NULL);
                SDL_SetWindowHitTest(dragWindow, DragHitCallback, NULL);
                SDL_ShowWindow(dragWindow);
            }
        }
    } break;
    default:
        break;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (mainWindow) {
        /* The child window and renderer will be cleaned up automatically. */
        SDL_DestroyWindow(mainWindow);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
}