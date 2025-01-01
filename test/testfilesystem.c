/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of filesystem functions. */

#include <stdio.h>
#include "SDL.h"
#include "SDL_test.h"

int main(int argc, char *argv[])
{
    char *base_path;
    char *pref_path;
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

    base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find base path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("base path: '%s'\n", base_path);
        SDL_free(base_path);
    }

    pref_path = SDL_GetPrefPath("libsdl", "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
        SDL_free(pref_path);
    }

    pref_path = SDL_GetPrefPath(NULL, "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path without organization: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
        SDL_free(pref_path);
    }

    SDLTest_CommonQuit(state);
    return 0;
}
