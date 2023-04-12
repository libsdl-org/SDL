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

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

static SDLTest_CommonState *state;

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

int main(int argc, char *argv[])
{
    int i, done;
    SDL_Event event;
    SDL_bool is_hover = SDL_FALSE;
    float x = 0.0f, y = 0.0f;
    unsigned int windowID = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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
            quit(1);
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }


    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, SDL_TRUE);

    /* Main render loop */
    done = 0;
    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_DROP_BEGIN) {
                SDL_Log("Drop beginning on window %u at (%f, %f)", (unsigned int)event.drop.windowID, event.drop.x, event.drop.y);
            } else if (event.type == SDL_EVENT_DROP_COMPLETE) {
                is_hover = SDL_FALSE;
                SDL_Log("Drop complete on window %u at (%f, %f)", (unsigned int)event.drop.windowID, event.drop.x, event.drop.y);
            } else if ((event.type == SDL_EVENT_DROP_FILE) || (event.type == SDL_EVENT_DROP_TEXT)) {
                const char *typestr = (event.type == SDL_EVENT_DROP_FILE) ? "File" : "Text";
                char *dropped_filedir = event.drop.file;
                SDL_Log("%s dropped on window %u: %s at (%f, %f)", typestr, (unsigned int)event.drop.windowID, dropped_filedir, event.drop.x, event.drop.y);
                /* Normally you'd have to do this, but this is freed in SDLTest_CommonEvent() */
                /*SDL_free(dropped_filedir);*/
            } else if (event.type == SDL_EVENT_DROP_POSITION) {
                is_hover = SDL_TRUE;
                x = event.drop.x;
                y = event.drop.y;
                windowID = event.drop.windowID;
                SDL_Log("Drop position on window %u at (%f, %f) file = %s", (unsigned int)event.drop.windowID, event.drop.x, event.drop.y, event.drop.file);
            }

            SDLTest_CommonEvent(state, &event, &done);
        }

        for (i = 0; i < state->num_windows; ++i) {
            SDL_Renderer *renderer = state->renderers[i];
            SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
            SDL_RenderClear(renderer);
            if (is_hover) {
                if (windowID == SDL_GetWindowID(SDL_GetRenderWindow(renderer))) {
                    int len = 2000;
                    SDL_SetRenderDrawColor(renderer, 0x0A, 0x0A, 0x0A, 0xFF);
                    SDL_RenderLine(renderer, x, y - len, x, y + len);
                    SDL_RenderLine(renderer, x - len, y, x + len, y);
                }
            }
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(16);
    }

    quit(0);
    /* keep the compiler happy ... */
    return 0;
}
