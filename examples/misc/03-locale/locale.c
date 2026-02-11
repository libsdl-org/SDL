/*
 * This example code reports the currently selected locales.
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
    SDL_SetAppMetadata("Example Misc Locale", "1.0", "com.example.misc-locale");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/misc/locale", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const SDL_FRect frame = { 0, 0, 640, 480 };
    SDL_Locale **locales;
    char msg[128];
    int count, i;
    float x, y;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    locales = SDL_GetPreferredLocales(&count);
    if (!locales) {
        x = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msg))) / 2.0f);
        y = frame.y;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, x, y, msg);
    } else {
        SDL_snprintf(msg, sizeof (msg), "Locales, in order of preference (%d total):", count);

        x = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msg))) / 2.0f);
        y = frame.y;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, x, y, msg);

        for (i = 0; locales[i]; ++i) {
            const SDL_Locale *l = locales[i];
            const char *c = l->country;

            SDL_snprintf(msg, sizeof (msg), " - %s%s%s", l->language, c ? "_" : "", c ? c : "");

            x = frame.x + ((frame.w - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(msg))) / 2.0f);
            y = frame.y + ((SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 2) * (i + 1));
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, x, y, msg);
        }
        SDL_free(locales);
    }

    /* put the new rendering on the screen. */
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
}

