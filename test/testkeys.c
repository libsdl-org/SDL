/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Print out all the scancodes we have, just to verify them */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDL_Scancode scancode;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    for (scancode = 0; scancode < SDL_NUM_SCANCODES; ++scancode) {
        SDL_Log("Scancode #%d, \"%s\"\n", scancode,
                SDL_GetScancodeName(scancode));
    }
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
