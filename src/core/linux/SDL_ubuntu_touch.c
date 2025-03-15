/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#include "SDL_ubuntu_touch.h"

#include <stdio.h>
#include <unistd.h>

bool SDL_IsUbuntuTouch(void)
{
    return access("/etc/ubuntu-touch-session.d/", F_OK) == 0;
}

// https://docs.ubports.com/en/latest/porting/configure_test_fix/Display.html#form-factor
SDL_UTFormFactor SDL_GetUbuntuTouchFormFactor(void)
{
    SDL_UTFormFactor form_factor = SDL_UTFORMFACTOR_UNKNOWN;
    FILE *config_file = NULL;
    char *line = NULL;
    size_t line_alloc = 0;
    ssize_t line_size = 0;

    config_file = fopen("/etc/ubuntu-touch-session.d/android.conf", "r");
    if (!config_file) {
        return form_factor;
    }

    while ((line_size = getline(&line, &line_alloc, config_file)) != -1) {
        if (SDL_strncmp(line, "FORM_FACTOR=", 12) == 0) {
            if (SDL_strcmp(line, "FORM_FACTOR=handset\n")) {
                form_factor = SDL_UTFORMFACTOR_PHONE;
            } else if (SDL_strcmp(line, "FORM_FACTOR=tablet\n")) {
                form_factor = SDL_UTFORMFACTOR_TABLET;
            } else if (SDL_strcmp(line, "FORM_FACTOR=laptop\n")) {
                form_factor = SDL_UTFORMFACTOR_LAPTOP;
            } else if (SDL_strcmp(line, "FORM_FACTOR=desktop\n")) {
                form_factor = SDL_UTFORMFACTOR_DESKTOP;
            } else {
                form_factor = SDL_UTFORMFACTOR_UNKNOWN;
            }
        }

        free(line);
    }

    fclose(config_file);

    return form_factor;
}
