/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static void tryOpenURL(const char *url)
{
    SDL_Log("Opening '%s' ...", url);
    if (SDL_OpenURL(url)) {
        SDL_Log("  success!");
    } else {
        SDL_Log("  failed! %s", SDL_GetError());
    }
}

int main(int argc, char **argv)
{
    int i;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            tryOpenURL(argv[i]);
        }
    } else {
        tryOpenURL("https://libsdl.org/");
    }

    SDL_Quit();
    return 0;
}
