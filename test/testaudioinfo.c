/*
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdio.h>
#include "SDL.h"

static void
print_devices(int iscapture)
{
    const char *typestr = ((iscapture) ? "capture" : "output");
    int n = SDL_GetNumAudioDevices(iscapture);

    printf("%s devices:\n", typestr);

    if (n == -1)
        printf("  Driver can't detect specific %s devices.\n\n", typestr);
    else if (n == 0)
        printf("  No %s devices found.\n\n", typestr);
    else {
        int i;
        for (i = 0; i < n; i++) {
            printf("  %s\n", SDL_GetAudioDeviceName(i, iscapture));
        }
        printf("\n");
    }
}

int
main(int argc, char **argv)
{
    int n;

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return (1);
    }

    /* Print available audio drivers */
    n = SDL_GetNumAudioDrivers();
    if (n == 0) {
        printf("No built-in audio drivers\n\n");
    } else {
        int i;
        printf("Built-in audio drivers:\n");
        for (i = 0; i < n; ++i) {
            printf("  %s\n", SDL_GetAudioDriver(i));
        }
        printf("\n");
    }

    printf("Using audio driver: %s\n\n", SDL_GetCurrentAudioDriver());

    print_devices(0);
    print_devices(1);

    SDL_Quit();
    return 0;
}
