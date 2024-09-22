/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/*
Copyright (c) 2011, Edgar Simo Serra
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the Simple Directmedia Layer (SDL) nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_Haptic *haptic;

/**
 * The entry point of this force feedback demo.
 * \param[in] argc Number of arguments.
 * \param[in] argv Array of argc arguments.
 */
int main(int argc, char **argv)
{
    int i;
    char *name = NULL;
    int index;
    SDLTest_CommonState *state;
    SDL_HapticID *haptics;
    int num_haptics;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    name = NULL;
    index = -1;

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!name && index < 0) {
                size_t l;

                l = SDL_strlen(argv[i]);
                if ((l < 3) && SDL_isdigit(argv[i][0]) && ((l == 1) || SDL_isdigit(argv[i][1]))) {
                    index = SDL_atoi(argv[i]);
                } else {
                    name = argv[i];
                }
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[device]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            SDL_Log("\n");
            SDL_Log("If device is a two-digit number it'll use it as an index, otherwise\n"
                    "it'll use it as if it were part of the device's name.\n");
            return 1;
        }

        i += consumed;
    }

    /* Initialize the force feedbackness */
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC);
    haptics = SDL_GetHaptics(&num_haptics);
    SDL_Log("%d Haptic devices detected.\n", num_haptics);
    if (num_haptics == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No Haptic devices found!\n");
        SDL_free(haptics);
        return 1;
    }

    /* We'll just use index or the first force feedback device found */
    if (!name) {
        i = (index != -1) ? index : 0;

        if (i >= num_haptics) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Index out of range, aborting.\n");
            SDL_free(haptics);
            return 1;
        }
    }
    /* Try to find matching device */
    else {
        for (i = 0; i < num_haptics; i++) {
            if (SDL_strstr(SDL_GetHapticNameForID(haptics[i]), name) != NULL) {
                break;
            }
        }

        if (i >= num_haptics) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to find device matching '%s', aborting.\n", name);
            SDL_free(haptics);
            return 1;
        }
    }

    haptic = SDL_OpenHaptic(haptics[i]);
    if (!haptic) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create the haptic device: %s\n", SDL_GetError());
        SDL_free(haptics);
        return 1;
    }
    SDL_Log("Device: %s\n", SDL_GetHapticName(haptic));
    SDL_free(haptics);

    /* We only want force feedback errors. */
    SDL_ClearError();

    if (!SDL_HapticRumbleSupported(haptic)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Rumble not supported!\n");
        return 1;
    }
    if (!SDL_InitHapticRumble(haptic)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize rumble: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Log("Playing 2 second rumble at 0.5 magnitude.\n");
    if (!SDL_PlayHapticRumble(haptic, 0.5, 5000)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to play rumble: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Delay(2000);
    SDL_Log("Stopping rumble.\n");
    SDL_StopHapticRumble(haptic);
    SDL_Delay(2000);
    SDL_Log("Playing 2 second rumble at 0.3 magnitude.\n");
    if (!SDL_PlayHapticRumble(haptic, 0.3f, 5000)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to play rumble: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Delay(2000);

    /* Quit */
    if (haptic) {
        SDL_CloseHaptic(haptic);
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
