/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of power subsystem. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static void
report_power(void)
{
    int seconds, percent;
    const SDL_PowerState state = SDL_GetPowerInfo(&seconds, &percent);
    const char *statestr = NULL;

    SDL_Log("SDL-reported power info...\n");
    switch (state) {
    case SDL_POWERSTATE_UNKNOWN:
        statestr = "Unknown";
        break;
    case SDL_POWERSTATE_ON_BATTERY:
        statestr = "On battery";
        break;
    case SDL_POWERSTATE_NO_BATTERY:
        statestr = "No battery";
        break;
    case SDL_POWERSTATE_CHARGING:
        statestr = "Charging";
        break;
    case SDL_POWERSTATE_CHARGED:
        statestr = "Charged";
        break;
    default:
        statestr = "!!API ERROR!!";
        break;
    }

    SDL_Log("State: %s\n", statestr);

    if (percent == -1) {
        SDL_Log("Percent left: unknown\n");
    } else {
        SDL_Log("Percent left: %d%%\n", percent);
    }

    if (seconds == -1) {
        SDL_Log("Time left: unknown\n");
    } else {
        SDL_Log("Time left: %d minutes, %d seconds\n", seconds / 60, seconds % 60);
    }
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (SDL_Init(0) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
        return 1;
    }

    report_power();

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}

/* end of testpower.c ... */
