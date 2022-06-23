/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Game controller mapping generator */
/* Gabriel Jacobo <gabomdq@gmail.com> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "testutils.h"

#ifndef SDL_JOYSTICK_DISABLED


int main(int argc, char *argv[]) {
    SDL_Event event;
    SDL_Joystick *joy;
    const char *type;
    char guid[64];
    SDL_bool running = SDL_TRUE;

    SDL_Init(SDL_INIT_JOYSTICK);
    joy = SDL_JoystickOpen(0);
    if (joy == NULL) {
        SDL_Log("Couldn't open joystick 0: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    SDL_assert(SDL_JoystickFromInstanceID(SDL_JoystickInstanceID(joy)) == joy);
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, sizeof (guid));
    switch (SDL_JoystickGetType(joy)) {
    case SDL_JOYSTICK_TYPE_GAMECONTROLLER:
        type = "Game Controller";
        break;
    case SDL_JOYSTICK_TYPE_WHEEL:
        type = "Wheel";
        break;
    case SDL_JOYSTICK_TYPE_ARCADE_STICK:
        type = "Arcade Stick";
        break;
    case SDL_JOYSTICK_TYPE_FLIGHT_STICK:
        type = "Flight Stick";
        break;
    case SDL_JOYSTICK_TYPE_DANCE_PAD:
        type = "Dance Pad";
        break;
    case SDL_JOYSTICK_TYPE_GUITAR:
        type = "Guitar";
        break;
    case SDL_JOYSTICK_TYPE_DRUM_KIT:
        type = "Drum Kit";
        break;
    case SDL_JOYSTICK_TYPE_ARCADE_PAD:
        type = "Arcade Pad";
        break;
    case SDL_JOYSTICK_TYPE_THROTTLE:
        type = "Throttle";
        break;
    default:
        type = "Unknown";
        break;
    }

    SDL_Log("Joystick\n");
    SDL_Log("          name: %s\n", SDL_JoystickName(joy));
    SDL_Log("          type: %s\n", type);
    SDL_Log("           LED: %s\n", SDL_JoystickHasLED(joy) ? "yes" : "no");
    SDL_Log("        rumble: %s\n", SDL_JoystickHasRumble(joy) ? "yes" : "no");
    SDL_Log("trigger rumble: %s\n", SDL_JoystickHasRumbleTriggers(joy) ? "yes" : "no");
    SDL_Log("          axes: %d\n", SDL_JoystickNumAxes(joy));
    SDL_Log("         balls: %d\n", SDL_JoystickNumBalls(joy));
    SDL_Log("          hats: %d\n", SDL_JoystickNumHats(joy));
    SDL_Log("       buttons: %d\n", SDL_JoystickNumButtons(joy));
    SDL_Log("   instance id: %d\n", SDL_JoystickInstanceID(joy));
    SDL_Log("          guid: %s\n", guid);
    SDL_Log("       VID/PID: 0x%.4x/0x%.4x\n", SDL_JoystickGetVendor(joy), SDL_JoystickGetProduct(joy));
    
    
    SDL_Delay(1000);
    SDL_Log("Checking Rumble functionality for 2 seconds\n");
    SDL_JoystickRumble(joy, 0xAAFF, 0xAAFF, 2000);

    while (running) {
        SDL_WaitEvent(&event);
        switch(event.type) {
            case SDL_QUIT:
                running = SDL_FALSE;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = SDL_FALSE;
                break;
            case SDL_JOYAXISMOTION:
                SDL_Log("Axis: %i %i\n", event.jaxis.axis, event.jaxis.value);
                break;
            case SDL_JOYBUTTONDOWN:
                SDL_Log("Button: %i\n", event.jbutton.button);
                break;
            case SDL_JOYDEVICEREMOVED:
                SDL_Log("Joy removed: %i\n", event.jdevice.which);
                if (event.jdevice.which == SDL_JoystickInstanceID(joy))
                    running = SDL_FALSE;
                break;
            default:
                break;
        }
    }

    SDL_JoystickClose(joy);
    SDL_Quit();
    return 0;
}

#else

int
main(int argc, char *argv[])
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL compiled without Joystick support.\n");
    return 1;
}

#endif

/* vi: set ts=4 sw=4 expandtab: */
