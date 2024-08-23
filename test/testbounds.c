/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char **argv)
{
    SDL_DisplayID *displays;
    int i;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
        return 1;
    }

    displays = SDL_GetDisplays(NULL);
    if (displays) {
        for (i = 0; displays[i]; i++) {
            SDL_Rect bounds = { -1, -1, -1, -1 }, usable = { -1, -1, -1, -1 };
            SDL_GetDisplayBounds(displays[i], &bounds);
            SDL_GetDisplayUsableBounds(displays[i], &usable);
            SDL_Log("Display #%d ('%s'): bounds={(%d,%d),%dx%d}, usable={(%d,%d),%dx%d}",
                    i, SDL_GetDisplayName(displays[i]),
                    bounds.x, bounds.y, bounds.w, bounds.h,
                    usable.x, usable.y, usable.w, usable.h);
        }
        SDL_free(displays);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
