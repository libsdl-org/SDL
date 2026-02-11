/*
 * This example code looks for the current gamepad state once per frame,
 * and draws a visual representation of it. See 01-joystick-polling for the
 * equivalent example code for the lower-level joystick API.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

/* Joysticks are low-level interfaces: there's something with a bunch of
   buttons, axes and hats, in no understood order or position. This is
   a flexible interface, but you'll need to build some sort of configuration
   UI to let people tell you what button, etc, does what. On top of this
   interface, SDL offers the "gamepad" API, which works with lots of devices,
   and knows how to map arbitrary buttons and such to look like an
   Xbox/PlayStation/etc gamepad. This is easier, and better, for many games,
   but isn't necessarily a good fit for complex apps and hardware. A flight
   simulator, a realistic racing game, etc, might want the joystick interface
   instead of gamepads. */

/* SDL can handle multiple gamepads, but for simplicity, this program only
   deals with the first gamepad it sees. */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Gamepad *gamepad = NULL;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    char *png_path = NULL;
    SDL_Surface *surface = NULL;

    SDL_SetAppMetadata("Example Input Gamepad Polling", "1.0", "com.example.input-gamepad-polling");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/input/gamepad-polling", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_STRETCH)) {
        return SDL_APP_FAILURE;
    }

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .png into a surface, move it to a texture from there. */
    SDL_asprintf(&png_path, "%sgamepad_front.png", SDL_GetBasePath());  /* allocate a string of the full file path */
    surface = SDL_LoadPNG(png_path);
    if (!surface) {
        SDL_Log("Couldn't load bitmap: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(png_path);  /* done with this, the file is loaded. */

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Couldn't create static texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface);  /* done with this, the texture has a copy of the pixels now. */

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        /* this event is sent for each hotplugged gamepad, but also each already-connected gamepad during SDL_Init(). */
        if (gamepad == NULL) {  /* we don't have a stick yet and one was added, open it! */
            gamepad = SDL_OpenGamepad(event->gdevice.which);
            if (!gamepad) {
                SDL_Log("Failed to open gamepad ID %u: %s", (unsigned int) event->gdevice.which, SDL_GetError());
            }
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (gamepad && (SDL_GetGamepadID(gamepad) == event->gdevice.which)) {
            SDL_CloseGamepad(gamepad);  /* our controller was unplugged. */
            gamepad = NULL;
        }
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const char *text = "Plug in a gamepad, please.";
    static Uint64 leftthumblast = 0xFFFFFFFF;
    static Uint64 rightthumblast = 0xFFFFFFFF;
    const Uint64 now = SDL_GetTicks();
    Sint16 axis_x, axis_y;
    float x, y;
    int i;

    if (gamepad) {  /* we have a stick opened? */
        text = SDL_GetGamepadName(gamepad);
    }

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);  /* white */
    SDL_RenderClear(renderer);

    /* note that you can get input as events, instead of polling, which is
       better since it won't miss button presses if the system is lagging,
       but often times checking the current state per-frame is good enough,
       and maybe better if you'd rather _drop_ inputs due to lag. */

    if (gamepad) {  /* we have a stick opened? */
        /* where to draw the buttons */
        const SDL_FRect buttons[] = {
            { 497, 266, 38,  38 },   /* SDL_GAMEPAD_BUTTON_SOUTH */
            { 550, 217, 38,  38 },   /* SDL_GAMEPAD_BUTTON_EAST */
            { 445, 221, 38,  38 },   /* SDL_GAMEPAD_BUTTON_WEST */
            { 499, 173, 38,  38 },   /* SDL_GAMEPAD_BUTTON_NORTH */
            { 235, 228, 32,  29 },   /* SDL_GAMEPAD_BUTTON_BACK */
            { 287, 195, 69,  69 },   /* SDL_GAMEPAD_BUTTON_GUIDE */
            { 377, 228, 32,  29 },   /* SDL_GAMEPAD_BUTTON_START */
            { 91,  234, 63,  63 },   /* SDL_GAMEPAD_BUTTON_LEFT_STICK */
            { 381, 354, 63,  63 },   /* SDL_GAMEPAD_BUTTON_RIGHT_STICK */
            { 74,  73,  102, 29 },   /* SDL_GAMEPAD_BUTTON_LEFT_SHOULDER */
            { 468, 73,  102, 29 },   /* SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER */
            { 207, 316, 32,  32 },   /* SDL_GAMEPAD_BUTTON_DPAD_UP */
            { 207, 384, 32,  32 },   /* SDL_GAMEPAD_BUTTON_DPAD_DOWN */
            { 173, 351, 32,  32 },   /* SDL_GAMEPAD_BUTTON_DPAD_LEFT */
            { 242, 351, 32,  32 },   /* SDL_GAMEPAD_BUTTON_DPAD_RIGHT */
            { 310, 286, 23,  27 },   /* SDL_GAMEPAD_BUTTON_MISC1 */
            /* there are other buttons: paddles on the back of the gamepad, touchpads, etc, but this is good enough for now. */
        };

        SDL_RenderTexture(renderer, texture, NULL, NULL);  /* draw the gamepad picture to the whole window. */

        /* draw green boxes over buttons that are currently pressed. */
        SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);  /* green */
        for (i = 0; i < SDL_arraysize(buttons); i++) {
            if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton) i)) {
                SDL_RenderFillRect(renderer, &buttons[i]);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, 0xFF);  /* yellow */

        /* left thumb axis. */
        axis_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        axis_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        if ((SDL_abs(axis_x) > 1000) || (SDL_abs(axis_y) > 1000)) {   /* zero means centered, but it might be a little off zero... */
            leftthumblast = now;  /* keep drawing, we're still moving. */
        }
        if ((now - leftthumblast) < 500) {  /* draw if there was movement in the last half-second. */
            const SDL_FRect box = { 107 + ((axis_x / 32767.0f) * 30.0f), 252 + ((axis_y / 32767.0f) * 30.0f), 30, 30 };
            SDL_RenderFillRect(renderer, &box);
        }

        /* right thumb axis. */
        axis_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
        axis_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
        if ((SDL_abs(axis_x) > 1000) || (SDL_abs(axis_y) > 1000)) {   /* zero means centered, but it might be a little off zero... */
            rightthumblast = now;  /* keep drawing, we're still moving. */
        }
        if ((now - rightthumblast) < 500) {  /* draw if there was movement in the last half-second. */
            const SDL_FRect box = { 397 + ((axis_x / 32767.0f) * 30.0f), 370 + ((axis_y / 32767.0f) * 30.0f), 30, 30 };
            SDL_RenderFillRect(renderer, &box);
        }

        /* left trigger. */
        axis_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        if (axis_y > 1000) {   /* zero means unpressed, but it might be a little off zero... */
            const float height = ((axis_y / 32767.0f) * 65.0f);
            const SDL_FRect box = { 127, 1 + (65.0f - height), 37, height };
            SDL_RenderFillRect(renderer, &box);
        }

        /* right trigger. */
        axis_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        if (axis_y > 1000) {   /* zero means unpressed, but it might be a little off zero... */
            const float height = ((axis_y / 32767.0f) * 65.0f);
            const SDL_FRect box = { 481, 1 + (65.0f - height), 37, height };
            SDL_RenderFillRect(renderer, &box);
        }
    }

    x = (((float) WINDOW_WIDTH) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
    if (gamepad) {
        y = (float) (WINDOW_HEIGHT - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2));
    } else {
        y = (((float) WINDOW_HEIGHT) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    }
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);  /* blue */
    SDL_RenderDebugText(renderer, x, y, text);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyTexture(texture);
    SDL_CloseGamepad(gamepad);
    /* SDL will clean up the window/renderer for us. */
}
