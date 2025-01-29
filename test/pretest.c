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

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[])
{
    Uint64 start;
    Uint64 prequit;
    (void)argc;
    (void)argv;
    SDL_Init(0);
    start = SDL_GetTicks();
    SDL_free(SDL_GetPrefPath("libsdl", "test_filesystem"));
    prequit = SDL_GetTicks();
    SDL_Log("SDL_GetPrefPath took %" SDL_PRIu64 "ms", prequit - start);
    SDL_Quit();
    SDL_Log("SDL_Quit took %" SDL_PRIu64 "ms", SDL_GetTicks() - prequit);
    return 0;
}
