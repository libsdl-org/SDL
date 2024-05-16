/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_revision.h>

int main(int argc, char *argv[])
{
    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc > 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "USAGE: %s", argv[0]);
        return 1;
    }

#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_Log("Compiled with SDL 3.0 or newer\n");
#else
    SDL_Log("Compiled with SDL older than 3.0\n");
#endif
    SDL_Log("Compiled version: %d.%d.%d (%s)\n",
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
            SDL_REVISION);
    int version = SDL_GetVersion();
    SDL_Log("Runtime version: %d.%d.%d (%s)\n",
            SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version),
            SDL_GetRevision());
    SDL_Quit();
    return 0;
}
