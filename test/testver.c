/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

int main(int argc, char *argv[])
{
    SDL_version compiled;
    SDL_version linked;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_Log("Compiled with SDL 3.0 or newer\n");
#else
    SDL_Log("Compiled with SDL older than 3.0\n");
#endif
    SDL_VERSION(&compiled);
    SDL_Log("Compiled version: %d.%d.%d (%s)\n",
            compiled.major, compiled.minor, compiled.patch,
            SDL_REVISION);
    SDL_GetVersion(&linked);
    SDL_Log("Linked version: %d.%d.%d (%s)\n",
            linked.major, linked.minor, linked.patch,
            SDL_GetRevision());
    SDL_Quit();
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
