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

typedef struct GamepadInfo
{
    SDL_JoystickID gamepad_id;
    const char *action;
} GamepadInfo;

static GamepadInfo gamepads_info[16];  /* if you have more than this, we'll ignore it. */

static GamepadInfo *FindGamepadInfo(SDL_JoystickID which)
{
    int i;
    for (i = 0; i < SDL_arraysize(gamepads_info); i++) {
        if (gamepads_info[i].gamepad_id == which) {
            return &gamepads_info[i];
        }
    }
    return NULL;  /* not found */
}

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
    GamepadInfo *info;
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        /* this event is sent for each hotplugged stick, but also each already-connected gamepad during SDL_Init(). */
        SDL_OpenGamepad(event->gdevice.which);
        info = FindGamepadInfo(0);  /* find an empty space */
        if (info) {
            info->gamepad_id = event->gdevice.which;
            info->action = "idle";
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gdevice.which);
        SDL_CloseGamepad(gamepad);
        info = FindGamepadInfo(event->gdevice.which);
        if (info) {
            info->gamepad_id = 0;
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gbutton.which);
        info = FindGamepadInfo(event->gbutton.which);
        switch (event->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            SDL_RumbleGamepad(gamepad, 0xFFFF, 0x0000, 5000);
            if (info) {
                info->action = "rumble high frequency";
            }
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            SDL_RumbleGamepad(gamepad, 0x0000, 0xFFFF, 5000);
            if (info) {
                info->action = "rumble low frequency";
            }
            break;
        default:
            break;
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        SDL_Gamepad *gamepad = SDL_GetGamepadFromID(event->gbutton.which);
        SDL_RumbleGamepad(gamepad, 0x0000, 0x0000, 0);
        info = FindGamepadInfo(event->gbutton.which);
        if (info) {
            info->action = "idle";
        }
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

static void draw_centered_text(const int rw, int *y, const char *str)
{
    const int x = (rw - (((int) SDL_strlen(str)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2;
    if (*str) {
        SDL_RenderDebugText(renderer, (float) x, (float) *y, str);
    }
    *y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2;
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    int rw, rh, y, i;
    SDL_GetCurrentRenderOutputSize(renderer, &rw, &rh);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);  /* clear to black */
    SDL_RenderClear(renderer);

    y = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 8;
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);  /* yellow text */
    draw_centered_text(rw, &y, "Connect gamepads and press buttons to rumble.");
    y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 3;

    /* report all the visible joysticks and what they are doing at the moment. */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);  /* white text */
    for (i = 0; i < SDL_arraysize(gamepads_info); i++) {
        const SDL_JoystickID which = gamepads_info[i].gamepad_id;
        if (which == 0) {
            draw_centered_text(rw, &y, "");  /* just leave a blank line. */
        } else {
            char str[128];
            SDL_snprintf(str, sizeof (str), "%s: %s", SDL_GetGamepadNameForID(which), gamepads_info[i].action);
            draw_centered_text(rw, &y, str);
        }
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_Quit();
    /* SDL will clean up the window/renderer for us. We let the gamepads leak. */
}
