/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

typedef struct {
    SDLTest_CommonState *state;
    bool is_hover;
    float x;
    float y;
    unsigned int windowID;
} dropfile_dialog;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;
    dropfile_dialog *dialog;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        /* needed voodoo to allow app to launch via macOS Finder */
        if (SDL_strncmp(argv[i], "-psn", 4) == 0) {
            consumed = 1;
        }
        if (consumed == 0) {
            consumed = -1;
        }
        if (consumed < 0) {
            SDLTest_CommonLogUsage(state, argv[0], NULL);
            goto onerror;
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        goto onerror;
    }
    dialog = SDL_calloc(1, sizeof(dropfile_dialog));
    if (!dialog) {
        goto onerror;
    }
    *appstate = dialog;

    dialog->state = state;
    return SDL_APP_CONTINUE;
onerror:
    SDLTest_CommonQuit(state);
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    dropfile_dialog *dialog = appstate;
    if (event->type == SDL_EVENT_DROP_BEGIN) {
        SDL_Log("Drop beginning on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
    } else if (event->type == SDL_EVENT_DROP_COMPLETE) {
        dialog->is_hover = false;
        SDL_Log("Drop complete on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
    } else if ((event->type == SDL_EVENT_DROP_FILE) || (event->type == SDL_EVENT_DROP_TEXT)) {
        const char *typestr = (event->type == SDL_EVENT_DROP_FILE) ? "File" : "Text";
        SDL_Log("%s dropped on window %u: %s at (%f, %f)", typestr, (unsigned int)event->drop.windowID, event->drop.data, event->drop.x, event->drop.y);
    } else if (event->type == SDL_EVENT_DROP_POSITION) {
        const float w_x = event->drop.x;
        const float w_y = event->drop.y;
        SDL_ConvertEventToRenderCoordinates(SDL_GetRenderer(SDL_GetWindowFromEvent(event)), event);
        dialog->is_hover = true;
        dialog->x = event->drop.x;
        dialog->y = event->drop.y;
        dialog->windowID = event->drop.windowID;
        SDL_Log("Drop position on window %u at (%f, %f) data = %s", (unsigned int)event->drop.windowID, w_x, w_y, event->drop.data);
    }

    return SDLTest_CommonEventMainCallbacks(dialog->state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    dropfile_dialog *dialog = appstate;
    int i;

    for (i = 0; i < dialog->state->num_windows; ++i) {
        SDL_Renderer *renderer = dialog->state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
        if (dialog->is_hover) {
            if (dialog->windowID == SDL_GetWindowID(SDL_GetRenderWindow(renderer))) {
                int len = 2000;
                SDL_SetRenderDrawColor(renderer, 0x0A, 0x0A, 0x0A, 0xFF);
                SDL_RenderLine(renderer, dialog->x, dialog->y - len, dialog->x, dialog->y + len);
                SDL_RenderLine(renderer, dialog->x - len, dialog->y, dialog->x + len, dialog->y);
            }
        }
        SDL_RenderPresent(renderer);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    dropfile_dialog *dialog = appstate;
    if (dialog) {
        SDLTest_CommonState *state = dialog->state;
        SDL_free(dialog);
        SDLTest_CommonQuit(state);
    }
}
