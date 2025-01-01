/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Call SDL_GetPrefPath to warm the SHGetFolderPathW cache */

/**
 * We noticed frequent ci timeouts running testfilesystem on 32-bit Windows.
 * Internally, this functions calls Shell32.SHGetFolderPathW.
 */

#include "SDL.h"
#include "SDL_test.h"

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    Uint64 start;
    char *path;

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

    start = SDL_GetTicks();
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    path = SDL_GetPrefPath("libsdl", "test_filesystem");
    SDL_free(path);
    SDL_Log("SDL_GetPrefPath took %" SDL_PRIu64 "ms", SDL_GetTicks() - start);
    SDLTest_CommonQuit(state);
    return 0;
}
