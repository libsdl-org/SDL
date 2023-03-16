/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program to test the SDL joystick routines */

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef __IOS__
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480
#else
#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#endif

static SDL_Window *window = NULL;
static SDL_Renderer *screen = NULL;
static SDL_Joystick *joystick = NULL;

static void
PrintJoystick(SDL_Joystick *joy)
{
    const char *type;
    char guid[64];

    SDL_assert(SDL_GetJoystickFromInstanceID(SDL_GetJoystickInstanceID(joy)) == joy);
    SDL_GetJoystickGUIDString(SDL_GetJoystickGUID(joy), guid, sizeof(guid));
    switch (SDL_GetJoystickType(joy)) {
    case SDL_JOYSTICK_TYPE_GAMEPAD:
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
    SDL_Log("          name: %s\n", SDL_GetJoystickName(joy));
    SDL_Log("          type: %s\n", type);
    SDL_Log("           LED: %s\n", SDL_JoystickHasLED(joy) ? "yes" : "no");
    SDL_Log("        rumble: %s\n", SDL_JoystickHasRumble(joy) ? "yes" : "no");
    SDL_Log("trigger rumble: %s\n", SDL_JoystickHasRumbleTriggers(joy) ? "yes" : "no");
    SDL_Log("          axes: %d\n", SDL_GetNumJoystickAxes(joy));
    SDL_Log("          hats: %d\n", SDL_GetNumJoystickHats(joy));
    SDL_Log("       buttons: %d\n", SDL_GetNumJoystickButtons(joy));
    SDL_Log("   instance id: %" SDL_PRIu32 "\n", SDL_GetJoystickInstanceID(joy));
    SDL_Log("          guid: %s\n", guid);
    SDL_Log("       VID/PID: 0x%.4x/0x%.4x\n", SDL_GetJoystickVendor(joy), SDL_GetJoystickProduct(joy));
}

static void
DrawRect(SDL_Renderer *r, const int x, const int y, const int w, const int h)
{
    SDL_FRect area;
    area.x = (float)x;
    area.y = (float)y;
    area.w = (float)w;
    area.h = (float)h;
    SDL_RenderFillRect(r, &area);
}

static void loop(void *arg)
{
    SDL_Event event;
    int i;
    SDL_bool *done = (SDL_bool*)arg;

    /* blank screen, set up for drawing this frame. */
    SDL_SetRenderDrawColor(screen, 0x0, 0x0, 0x0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);

    while (SDL_PollEvent(&event)) {
        switch (event.type) {

        case SDL_EVENT_JOYSTICK_ADDED:
            SDL_Log("Joystick device %" SDL_PRIu32 " added.\n", event.jdevice.which);
            if (joystick == NULL) {
                joystick = SDL_OpenJoystick(event.jdevice.which);
                if (joystick) {
                    PrintJoystick(joystick);
                } else {
                    SDL_Log("Couldn't open joystick: %s\n", SDL_GetError());
                }
            }
            break;

        case SDL_EVENT_JOYSTICK_REMOVED:
            SDL_Log("Joystick device %" SDL_PRIu32 " removed.\n", event.jdevice.which);
            if (event.jdevice.which == SDL_GetJoystickInstanceID(joystick)) {
                SDL_JoystickID *joysticks;

                SDL_CloseJoystick(joystick);
                joystick = NULL;

                joysticks = SDL_GetJoysticks(NULL);
                if (joysticks) {
                    if (joysticks[0]) {
                        joystick = SDL_OpenJoystick(joysticks[0]);
                        if (joystick) {
                            PrintJoystick(joystick);
                        } else {
                            SDL_Log("Couldn't open joystick: %s\n", SDL_GetError());
                        }
                    }
                    SDL_free(joysticks);
                }
            }
            break;

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
            SDL_Log("Joystick %" SDL_PRIu32 " axis %d value: %d\n",
                    event.jaxis.which,
                    event.jaxis.axis, event.jaxis.value);
            break;
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
            SDL_Log("Joystick %" SDL_PRIu32 " hat %d value:",
                    event.jhat.which, event.jhat.hat);
            if (event.jhat.value == SDL_HAT_CENTERED) {
                SDL_Log(" centered");
            }
            if (event.jhat.value & SDL_HAT_UP) {
                SDL_Log(" up");
            }
            if (event.jhat.value & SDL_HAT_RIGHT) {
                SDL_Log(" right");
            }
            if (event.jhat.value & SDL_HAT_DOWN) {
                SDL_Log(" down");
            }
            if (event.jhat.value & SDL_HAT_LEFT) {
                SDL_Log(" left");
            }
            SDL_Log("\n");
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            SDL_Log("Joystick %" SDL_PRIu32 " button %d down\n",
                    event.jbutton.which, event.jbutton.button);
            /* First button triggers a 0.5 second full strength rumble */
            if (event.jbutton.button == 0) {
                SDL_RumbleJoystick(joystick, 0xFFFF, 0xFFFF, 500);
            }
            break;
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
            SDL_Log("Joystick %" SDL_PRIu32 " button %d up\n",
                    event.jbutton.which, event.jbutton.button);
            break;
        case SDL_EVENT_KEY_DOWN:
            /* Press the L key to lag for 3 seconds, to see what happens
                when SDL doesn't service the event loop quickly. */
            if (event.key.keysym.sym == SDLK_l) {
                SDL_Log("Lagging for 3 seconds...\n");
                SDL_Delay(3000);
                break;
            }

            if ((event.key.keysym.sym != SDLK_ESCAPE) &&
                (event.key.keysym.sym != SDLK_AC_BACK)) {
                break;
            }
            SDL_FALLTHROUGH;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_QUIT:
            *done = SDL_TRUE;
            break;
        default:
            break;
        }
    }

    if (joystick) {
        const int BUTTONS_PER_LINE = ((SCREEN_WIDTH - 4) / 34);
        int x, y;

        /* Update visual joystick state */
        SDL_SetRenderDrawColor(screen, 0x00, 0xFF, 0x00, SDL_ALPHA_OPAQUE);
        y = SCREEN_HEIGHT - ((((SDL_GetNumJoystickButtons(joystick) + (BUTTONS_PER_LINE - 1)) / BUTTONS_PER_LINE) + 1) * 34);
        for (i = 0; i < SDL_GetNumJoystickButtons(joystick); ++i) {
            if ((i % BUTTONS_PER_LINE) == 0) {
                y += 34;
            }
            if (SDL_GetJoystickButton(joystick, i) == SDL_PRESSED) {
                x = 2 + (i % BUTTONS_PER_LINE) * 34;
                DrawRect(screen, x, y, 32, 32);
            }
        }

        SDL_SetRenderDrawColor(screen, 0xFF, 0x00, 0x00, SDL_ALPHA_OPAQUE);
        for (i = 0; i < SDL_GetNumJoystickAxes(joystick); ++i) {
            /* Draw the X/Y axis */
            x = (((int)SDL_GetJoystickAxis(joystick, i)) + 32768);
            x *= SCREEN_WIDTH;
            x /= 65535;
            if (x < 0) {
                x = 0;
            } else if (x > (SCREEN_WIDTH - 16)) {
                x = SCREEN_WIDTH - 16;
            }
            ++i;
            if (i < SDL_GetNumJoystickAxes(joystick)) {
                y = (((int)SDL_GetJoystickAxis(joystick, i)) + 32768);
            } else {
                y = 32768;
            }
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
        for (i = 0; i < SDL_GetNumJoystickHats(joystick); ++i) {
            /* Derive the new position */
            const Uint8 hat_pos = SDL_GetJoystickHat(joystick, i);
            x = SCREEN_WIDTH / 2;
            y = SCREEN_HEIGHT / 2;

            if (hat_pos & SDL_HAT_UP) {
                y = 0;
            } else if (hat_pos & SDL_HAT_DOWN) {
                y = SCREEN_HEIGHT - 8;
            }

            if (hat_pos & SDL_HAT_LEFT) {
                x = 0;
            } else if (hat_pos & SDL_HAT_RIGHT) {
                x = SCREEN_WIDTH - 8;
            }

            DrawRect(screen, x, y, 8, 8);
        }
    }

    SDL_Delay(16);
    SDL_RenderPresent(screen);

#ifdef __EMSCRIPTEN__
    if (*done == SDL_TRUE) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    SDL_bool done;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("Joystick Test", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return SDL_FALSE;
    }

    screen = SDL_CreateRenderer(window, NULL, 0);
    if (screen == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return SDL_FALSE;
    }

    SDL_SetRenderDrawColor(screen, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(screen);
    SDL_RenderPresent(screen);

    done = SDL_FALSE;

    /* Loop, getting joystick events! */
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop, &done, 0, 1);
#else
    while (!done) {
        loop(&done);
    }
#endif

    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    SDLTest_CommonDestroyState(state);

    return 0;
}
