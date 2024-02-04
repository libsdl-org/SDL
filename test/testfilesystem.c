/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of filesystem functions. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    char *base_path;
    char *pref_path;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (SDL_Init(0) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
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

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
