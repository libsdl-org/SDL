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

static void syntax(void) {
    SDL_Log("----------------------------------");
    SDL_Log("Usage:");
    SDL_Log("  s: toggle start / stop");
    SDL_Log("----------------------------------");
}

int main(int argc, char *argv[])
{
    int i, done;
    SDL_Event event;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    SDL_SetEventEnabled(SDL_EVENT_LOCATION, true);

    syntax();

    if (SDL_StartLocation() < 0) {
        SDL_Log("SDL_StartLocation() error: %s", SDL_GetError());
    } else {
        SDL_Log("SDL_StartLocation() ... waiting (~10 seconds) ...");
    }

    /* Main render loop */
    done = 0;
    while (!done) {
        /* Check for events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_LOCATION) {
                double latitude = event.location.latitude;
                double longitude = event.location.longitude;
                double altitude = event.location.altitude;
                SDL_Log("SDL_EVENT_LOCATION: latitude=%g longitude=%g altitude=%g", latitude, longitude, altitude);
            }

            if (event.type == SDL_EVENT_KEY_DOWN) {
                int sym = event.key.key;
                if (sym == SDLK_S) {
                    static int started = 1;
                    if (started == 1) {
                        const char *err;
                        SDL_Log("SDL_StopLocation()");
                        SDL_StopLocation();
                        err = SDL_GetError();
                        if (err && err[0]) {
                            SDL_Log("SDL_StopLocation() error: %s", SDL_GetError());
                        } else {
                            started = 0;
                        }
                    } else {
                        SDL_Log("SDL_StartLocation()");
                        if (SDL_StartLocation() < 0) {
                            SDL_Log("SDL_StartLocation() error: %s", SDL_GetError());
                        } else {
                            started = 1;
                        }

                    }
                }

            }

            SDLTest_CommonEvent(state, &event, &done);
        }

        for (i = 0; i < state->num_windows; ++i) {
            SDL_Renderer *renderer = state->renderers[i];
            SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(16);
    }

    quit(0);
    /* keep the compiler happy ... */
    return 0;
}
