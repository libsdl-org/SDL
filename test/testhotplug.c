/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program to test the SDL joystick hotplugging */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    int num_joysticks = 0;
    SDL_Joystick *joystick = NULL;
    SDL_Haptic *haptic = NULL;
    SDL_JoystickID instance = 0;
    SDL_bool keepGoing = SDL_TRUE;
    int i;
    SDL_bool enable_haptic = SDL_TRUE;
    Uint32 init_subsystems = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcasecmp(argv[i], "--nohaptic") == 0) {
                enable_haptic = SDL_FALSE;
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[--nohaptic]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    if (enable_haptic) {
        init_subsystems |= SDL_INIT_HAPTIC;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(init_subsystems) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    /*
    //SDL_CreateWindow("Dummy", 128, 128, 0);
    */

    SDL_free(SDL_GetJoysticks(&num_joysticks));
    SDL_Log("There are %d joysticks at startup\n", num_joysticks);
    if (enable_haptic) {
        SDL_Log("There are %d haptic devices at startup\n", SDL_NumHaptics());
    }

    while (keepGoing) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                keepGoing = SDL_FALSE;
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                if (joystick) {
                    SDL_Log("Only one joystick supported by this test\n");
                } else {
                    joystick = SDL_OpenJoystick(event.jdevice.which);
                    instance = event.jdevice.which;
                    SDL_Log("Joy Added  : %" SDL_PRIu32 " : %s\n", event.jdevice.which, SDL_GetJoystickName(joystick));
                    if (enable_haptic) {
                        if (SDL_JoystickIsHaptic(joystick)) {
                            haptic = SDL_HapticOpenFromJoystick(joystick);
                            if (haptic) {
                                SDL_Log("Joy Haptic Opened\n");
                                if (SDL_HapticRumbleInit(haptic) != 0) {
                                    SDL_Log("Could not init Rumble!: %s\n", SDL_GetError());
                                    SDL_HapticClose(haptic);
                                    haptic = NULL;
                                }
                            } else {
                                SDL_Log("Joy haptic open FAILED!: %s\n", SDL_GetError());
                            }
                        } else {
                            SDL_Log("No haptic found\n");
                        }
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                if (instance == event.jdevice.which) {
                    SDL_Log("Joy Removed: %" SDL_PRIs32 "\n", event.jdevice.which);
                    instance = 0;
                    if (enable_haptic && haptic) {
                        SDL_HapticClose(haptic);
                        haptic = NULL;
                    }
                    SDL_CloseJoystick(joystick);
                    joystick = NULL;
                } else {
                    SDL_Log("Unknown joystick disconnected\n");
                }
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                /*
                //                    SDL_Log("Axis Move: %d\n", event.jaxis.axis);
                */
                if (enable_haptic) {
                    SDL_HapticRumblePlay(haptic, 0.25, 250);
                }
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                SDL_Log("Button Press: %d\n", event.jbutton.button);
                if (enable_haptic && haptic) {
                    SDL_HapticRumblePlay(haptic, 0.25, 250);
                }
                if (event.jbutton.button == 0) {
                    SDL_Log("Exiting due to button press of button 0\n");
                    keepGoing = SDL_FALSE;
                }
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                SDL_Log("Button Release: %d\n", event.jbutton.button);
                break;
            }
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
