/*
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program to test the SDL game controller routines */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#ifndef SDL_JOYSTICK_DISABLED

#ifdef __IPHONEOS__
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT    480
#else
#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT    480
#endif

#define MAX_NUM_AXES 6
#define MAX_NUM_HATS 2

static SDL_bool s_ForceQuit = SDL_FALSE;

static void
DrawRect(SDL_Renderer *r, const int x, const int y, const int w, const int h)
{
    const SDL_Rect area = { x, y, w, h };
    SDL_RenderFillRect(r, &area);
}

static const char *
ControllerAxisName(const SDL_GameControllerAxis axis)
{
    switch (axis)
    {
        #define AXIS_CASE(ax) case SDL_CONTROLLER_AXIS_##ax: return #ax
        AXIS_CASE(INVALID);
        AXIS_CASE(LEFTX);
        AXIS_CASE(LEFTY);
        AXIS_CASE(RIGHTX);
        AXIS_CASE(RIGHTY);
        AXIS_CASE(TRIGGERLEFT);
        AXIS_CASE(TRIGGERRIGHT);
        #undef AXIS_CASE
        default: return "???";
    }
}

static const char *
ControllerButtonName(const SDL_GameControllerButton button)
{
    switch (button)
    {
        #define BUTTON_CASE(btn) case SDL_CONTROLLER_BUTTON_##btn: return #btn
        BUTTON_CASE(INVALID);
        BUTTON_CASE(A);
        BUTTON_CASE(B);
        BUTTON_CASE(X);
        BUTTON_CASE(Y);
        BUTTON_CASE(BACK);
        BUTTON_CASE(GUIDE);
        BUTTON_CASE(START);
        BUTTON_CASE(LEFTSTICK);
        BUTTON_CASE(RIGHTSTICK);
        BUTTON_CASE(LEFTSHOULDER);
        BUTTON_CASE(RIGHTSHOULDER);
        BUTTON_CASE(DPAD_UP);
        BUTTON_CASE(DPAD_DOWN);
        BUTTON_CASE(DPAD_LEFT);
        BUTTON_CASE(DPAD_RIGHT);
        #undef BUTTON_CASE
        default: return "???";
    }
}

void
WatchGameController(SDL_GameController * gamecontroller)
{
    const char *name = SDL_GameControllerName(gamecontroller);
    const char *basetitle = "Game Controller Test: ";
    const size_t titlelen = SDL_strlen(basetitle) + SDL_strlen(name) + 1;
    char *title = (char *)SDL_malloc(titlelen);
    SDL_Window *window = NULL;
    SDL_Renderer *screen = NULL;
    int done = 0;
    SDL_Event event;
    int i;

    if (title) {
        SDL_snprintf(title, titlelen, "%s%s", basetitle, name);
    }

    /* Create a window to display controller axis position */
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return;
    }

    screen = SDL_CreateRenderer(window, -1, 0);
    if (screen == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return;
    }

    SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_RenderPresent(screen);
    SDL_RaiseWindow(window);

    /* Print info about the controller we are watching */
    SDL_Log("Watching controller %s\n",  name ? name : "Unknown Controller");

    /* Loop, getting controller events! */
    while (!done) {
        /* blank screen, set up for drawing this frame. */
        SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(screen);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_CONTROLLERAXISMOTION:
                SDL_Log("Controller %d axis %d ('%s') value: %d\n",
                       event.caxis.which,
                       event.caxis.axis,
                       ControllerAxisName((SDL_GameControllerAxis)event.caxis.axis),
                       event.caxis.value);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                SDL_Log("Controller %d button %d ('%s') down\n",
                       event.cbutton.which, event.cbutton.button,
                       ControllerButtonName((SDL_GameControllerButton)event.cbutton.button));
                break;
            case SDL_CONTROLLERBUTTONUP:
                SDL_Log("Controller %d button %d ('%s') up\n",
                       event.cbutton.which, event.cbutton.button,
                       ControllerButtonName((SDL_GameControllerButton)event.cbutton.button));
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym != SDLK_ESCAPE) {
                    break;
                }
                /* Fall through to signal quit */
            case SDL_QUIT:
                done = 1;
                s_ForceQuit = SDL_TRUE;
                break;
            default:
                break;
            }
        }
        /* Update visual controller state */
        SDL_SetRenderDrawColor(screen, 0x00, 0xFF, 0x00, SDL_ALPHA_OPAQUE);
        for (i = 0; i <SDL_CONTROLLER_BUTTON_MAX; ++i) {
            if (SDL_GameControllerGetButton(gamecontroller, (SDL_GameControllerButton)i) == SDL_PRESSED) {
                DrawRect(screen, i * 34, SCREEN_HEIGHT - 34, 32, 32);
            }
        }

        SDL_SetRenderDrawColor(screen, 0xFF, 0x00, 0x00, SDL_ALPHA_OPAQUE);
        for (i = 0; i < SDL_CONTROLLER_AXIS_MAX / 2; ++i) {
            /* Draw the X/Y axis */
            int x, y;
            x = (((int) SDL_GameControllerGetAxis(gamecontroller, (SDL_GameControllerAxis)(i * 2 + 0))) + 32768);
            x *= SCREEN_WIDTH;
            x /= 65535;
            if (x < 0) {
                x = 0;
            } else if (x > (SCREEN_WIDTH - 16)) {
                x = SCREEN_WIDTH - 16;
            }
            y = (((int) SDL_GameControllerGetAxis(gamecontroller, (SDL_GameControllerAxis)(i * 2 + 1))) + 32768);
            y *= SCREEN_HEIGHT;
            y /= 65535;
            if (y < 0) {
                y = 0;
            } else if (y > (SCREEN_HEIGHT - 16)) {
                y = SCREEN_HEIGHT - 16;
            }

            DrawRect(screen, x, y, 16, 16);
        }

        SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0xFF, SDL_ALPHA_OPAQUE);

        SDL_RenderPresent(screen);

        if ( !done )
            done = SDL_GameControllerGetAttached( gamecontroller ) == 0;
    }

    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);
}

int
main(int argc, char *argv[])
{
    int i;
    int nController = 0;
    int retcode = 0;
    char guid[64];
    SDL_GameController *gamecontroller;

    /* Enable standard application logging */
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER ) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Print information about the controller */
    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        const char *name;
        const char *description;

        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i),
                                  guid, sizeof (guid));

        if ( SDL_IsGameController(i) )
        {
            nController++;
            name = SDL_GameControllerNameForIndex(i);
            description = "Controller";
        } else {
            name = SDL_JoystickNameForIndex(i);
            description = "Joystick";
        }
        SDL_Log("%s %d: %s (guid %s)\n", description, i, name ? name : "Unknown", guid);
    }
    SDL_Log("There are %d game controller(s) attached (%d joystick(s))\n", nController, SDL_NumJoysticks());

    if (argv[1]) {
        int device = atoi(argv[1]);
        if (device >= SDL_NumJoysticks()) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%i is an invalid joystick index.\n", device);
            retcode = 1;
        } else {
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device),
                                      guid, sizeof (guid));
            SDL_Log("Attempting to open device %i, guid %s\n", device, guid);
            gamecontroller = SDL_GameControllerOpen(device);
            if (gamecontroller == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open joystick %d: %s\n", device, SDL_GetError());
                retcode = 1;
            } else {
                WatchGameController(gamecontroller);
                SDL_GameControllerClose(gamecontroller);
            }
        }
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

    return retcode;
}

#else

int
main(int argc, char *argv[])
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL compiled without Joystick support.\n");
    exit(1);
}

#endif
