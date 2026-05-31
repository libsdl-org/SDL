/*
 * This example rumbles gamepads on button presses.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Example Input Gamepad Rumble", "1.0", "com.example.input-gamepad-rumble");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/input/gamepad-rumble", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        /* this event is sent for each hotplugged stick, but also each already-connected gamepad during SDL_Init(). */
        SDL_OpenGamepad(event->gdevice.which);
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gdevice.which);
        SDL_CloseGamepad(gamepad);
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gbutton.which);
        switch (event->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            SDL_RumbleGamepad(gamepad, 0xFFFF, 0x0000, 5000);
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            SDL_RumbleGamepad(gamepad, 0x0000, 0xFFFF, 5000);
            break;
        default:
            break;
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gbutton.which);
        SDL_RumbleGamepad(gamepad, 0x0000, 0x0000, 0);
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, 8, 8, "Connect gamepad and press buttons to rumble");

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_Quit();
    /* SDL will clean up the window/renderer for us. We let the gamepads leak. */
}
