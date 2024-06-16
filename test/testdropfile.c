/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
    SDL_bool is_hover;
    float x;
    float y;
    unsigned int windowID;
} dropfile_dialog;

int SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    dropfile_dialog *dialog = appstate;
    if (event->type == SDL_EVENT_DROP_BEGIN) {
        SDL_Log("Drop beginning on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
    } else if (event->type == SDL_EVENT_DROP_COMPLETE) {
        dialog->is_hover = SDL_FALSE;
        SDL_Log("Drop complete on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
    } else if ((event->type == SDL_EVENT_DROP_FILE) || (event->type == SDL_EVENT_DROP_TEXT)) {
        const char *typestr = (event->type == SDL_EVENT_DROP_FILE) ? "File" : "Text";
        SDL_Log("%s dropped on window %u: %s at (%f, %f)", typestr, (unsigned int)event->drop.windowID, event->drop.data, event->drop.x, event->drop.y);
    } else if (event->type == SDL_EVENT_DROP_POSITION) {
        dialog->is_hover = SDL_TRUE;
        dialog->x = event->drop.x;
        dialog->y = event->drop.y;
        dialog->windowID = event->drop.windowID;
        SDL_Log("Drop position on window %u at (%f, %f) data = %s", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y, event->drop.data);
    }

    return SDLTest_CommonEventMainCallbacks(dialog->state, event);
}

int SDL_AppIterate(void *appstate)
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

int SDL_AppInit(void **appstate, int argc, char *argv[]) {
    int i;
    dropfile_dialog *dialog;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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
    dialog = SDL_calloc(sizeof(dropfile_dialog), 1);
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

void SDL_AppQuit(void *appstate)
{
    dropfile_dialog *dialog = appstate;
    if (dialog) {
        SDLTest_CommonState *state = dialog->state;
        SDL_free(dialog);
        SDLTest_CommonQuit(state);
    }
}
