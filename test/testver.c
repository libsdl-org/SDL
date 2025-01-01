/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Test program to compare the compile-time version of SDL with the linked
   version of SDL
*/

#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"
#include "SDL_revision.h"
#include "SDL_test.h"

int main(int argc, char *argv[])
{
    SDL_version compiled;
    SDL_version linked;
    SDLTest_CommonState *state;

    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDLTest_CommonCreateState failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

#if SDL_VERSION_ATLEAST(2, 0, 0)
    SDL_Log("Compiled with SDL 2.0 or newer\n");
#else
    SDL_Log("Compiled with SDL older than 2.0\n");
#endif
    SDL_VERSION(&compiled);
    SDL_Log("Compiled version: %d.%d.%d (%s)\n",
            compiled.major, compiled.minor, compiled.patch,
            SDL_REVISION);
    SDL_GetVersion(&linked);
    SDL_Log("Linked version: %d.%d.%d (%s)\n",
            linked.major, linked.minor, linked.patch,
            SDL_GetRevision());

    SDLTest_CommonQuit(state);
    return 0;
}
