/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to test querying of display info */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static void
print_mode(const char *prefix, const SDL_DisplayMode *mode)
{
    if (mode == NULL) {
        return;
    }

    SDL_Log("%s: fmt=%s w=%d h=%d scale=%d%% refresh=%gHz\n",
            prefix, SDL_GetPixelFormatName(mode->format),
            mode->pixel_w, mode->pixel_h, (int)(mode->display_scale * 100.0f), mode->refresh_rate);
}

int main(int argc, char *argv[])
{
    SDL_DisplayID *displays;
    const SDL_DisplayMode **modes;
    const SDL_DisplayMode *mode;
    int num_displays, i;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Log("Using video target '%s'.\n", SDL_GetCurrentVideoDriver());
    displays = SDL_GetDisplays(&num_displays);

    SDL_Log("See %d displays.\n", num_displays);

    for (i = 0; i < num_displays; i++) {
        SDL_DisplayID dpy = displays[i];
        SDL_Rect rect = { 0, 0, 0, 0 };
        int m, num_modes = 0;

        SDL_GetDisplayBounds(dpy, &rect);
        modes = SDL_GetFullscreenDisplayModes(dpy, &num_modes);
        SDL_Log("%" SDL_PRIu32 ": \"%s\" (%dx%d, (%d, %d)), %d fullscreen modes.\n", dpy, SDL_GetDisplayName(dpy), rect.w, rect.h, rect.x, rect.y, num_modes);

        mode = SDL_GetCurrentDisplayMode(dpy);
        if (mode) {
            print_mode("CURRENT", mode);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "    CURRENT: failed to query (%s)\n", SDL_GetError());
        }

        mode = SDL_GetDesktopDisplayMode(dpy);
        if (mode) {
            print_mode("DESKTOP", mode);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "    DESKTOP: failed to query (%s)\n", SDL_GetError());
        }

        for (m = 0; m < num_modes; m++) {
            char prefix[64];
            (void)SDL_snprintf(prefix, sizeof prefix, "    MODE %d", m);
            print_mode(prefix, modes[i]);
        }
        SDL_free(modes);

        SDL_Log("\n");
    }
    SDL_free(displays);

    SDL_Quit();
    return 0;
}
