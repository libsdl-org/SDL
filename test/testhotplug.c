/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
    int num_keyboards = 0;
    int num_mice = 0;
    int num_joysticks = 0;
    SDL_Joystick *joystick = NULL;
    SDL_Haptic *haptic = NULL;
    SDL_JoystickID instance = 0;
    bool keepGoing = true;
    int i;
    bool enable_haptic = true;
    Uint32 init_subsystems = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcasecmp(argv[i], "--nohaptic") == 0) {
                enable_haptic = false;
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

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    /* Initialize SDL (Note: video is required to start event loop) */
    if (!SDL_Init(init_subsystems)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        exit(1);
    }

    /*
    //SDL_CreateWindow("Dummy", 128, 128, 0);
    */

    SDL_free(SDL_GetKeyboards(&num_keyboards));
    SDL_Log("There are %d keyboards at startup", num_keyboards);

    SDL_free(SDL_GetMice(&num_mice));
    SDL_Log("There are %d mice at startup", num_mice);

    SDL_free(SDL_GetJoysticks(&num_joysticks));
    SDL_Log("There are %d joysticks at startup", num_joysticks);

    if (enable_haptic) {
        int num_haptics;
        SDL_free(SDL_GetHaptics(&num_haptics));
        SDL_Log("There are %d haptic devices at startup", num_haptics);
    }

    while (keepGoing) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                keepGoing = false;
                break;
            case SDL_EVENT_KEYBOARD_ADDED:
                SDL_Log("Keyboard '%s' added  : %" SDL_PRIu32, SDL_GetKeyboardNameForID(event.kdevice.which), event.kdevice.which);
                break;
            case SDL_EVENT_KEYBOARD_REMOVED:
                SDL_Log("Keyboard removed: %" SDL_PRIu32, event.kdevice.which);
                break;
            case SDL_EVENT_MOUSE_ADDED:
                SDL_Log("Mouse '%s' added  : %" SDL_PRIu32, SDL_GetMouseNameForID(event.mdevice.which), event.mdevice.which);
                break;
            case SDL_EVENT_MOUSE_REMOVED:
                SDL_Log("Mouse removed: %" SDL_PRIu32, event.mdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                if (joystick) {
                    SDL_Log("Only one joystick supported by this test");
                } else {
                    joystick = SDL_OpenJoystick(event.jdevice.which);
                    instance = event.jdevice.which;
                    SDL_Log("Joy Added  : %" SDL_PRIu32 " : %s", event.jdevice.which, SDL_GetJoystickName(joystick));
                    if (enable_haptic) {
                        if (SDL_IsJoystickHaptic(joystick)) {
                            haptic = SDL_OpenHapticFromJoystick(joystick);
                            if (haptic) {
                                SDL_Log("Joy Haptic Opened");
                                if (!SDL_InitHapticRumble(haptic)) {
                                    SDL_Log("Could not init Rumble!: %s", SDL_GetError());
                                    SDL_CloseHaptic(haptic);
                                    haptic = NULL;
                                }
                            } else {
                                SDL_Log("Joy haptic open FAILED!: %s", SDL_GetError());
                            }
                        } else {
                            SDL_Log("No haptic found");
                        }
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                if (instance == event.jdevice.which) {
                    SDL_Log("Joy Removed: %" SDL_PRIs32, event.jdevice.which);
                    instance = 0;
                    if (enable_haptic && haptic) {
                        SDL_CloseHaptic(haptic);
                        haptic = NULL;
                    }
                    SDL_CloseJoystick(joystick);
                    joystick = NULL;
                } else {
                    SDL_Log("Unknown joystick disconnected");
                }
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                /*
                //                    SDL_Log("Axis Move: %d", event.jaxis.axis);
                */
                if (enable_haptic) {
                    SDL_PlayHapticRumble(haptic, 0.25, 250);
                }
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                SDL_Log("Button Press: %d", event.jbutton.button);
                if (enable_haptic && haptic) {
                    SDL_PlayHapticRumble(haptic, 0.25, 250);
                }
                if (event.jbutton.button == 0) {
                    SDL_Log("Exiting due to button press of button 0");
                    keepGoing = false;
                }
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                SDL_Log("Button Release: %d", event.jbutton.button);
                break;
            default:
                break;
            }
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
