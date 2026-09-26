/*
 * This example code shows usage of file dialog.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static const char *initial_path = NULL;

static const SDL_FRect OPEN_FILE_RECT = { 50, 50, 220, 140 };
static const SDL_Rect IMAGE_RECT = { 370, 290, 220, 140 };

static const SDL_DialogFileFilter filters[] = {
    { "JPG images", "jpg;jpeg" },
    { "PNG images", "png" }
};

/* This function runs when files are chosen (or dialog is canceled, or error happens). */
static void SDLCALL callback(void *userdata, const char *const *files, int filter)
{
    if (files) {
        if (*files) {
            SDL_Surface *surface = SDL_LoadSurface(*files);
            if (surface) {
                SDL_Surface *scaled = SDL_ScaleSurface(surface, IMAGE_RECT.w, IMAGE_RECT.h, SDL_SCALEMODE_LINEAR);
                SDL_DestroySurface(surface);
                if (scaled) {
                    SDL_Surface *converted = SDL_ConvertSurface(scaled, SDL_PIXELFORMAT_RGBA8888);
                    SDL_DestroySurface(scaled);
                    if (converted) {
                        SDL_UpdateTexture(texture, NULL, converted->pixels, converted->pitch);
                        SDL_DestroySurface(converted);
                    }
                }
            }
        }

        while (*files) {
            SDL_Log("'%s'", *files);
            files++;
        }
    } else {
        SDL_Log("Error: %s", SDL_GetError());
    }
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/asyncio/load-bitmaps", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    initial_path = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (!initial_path) {
        SDL_Log("Will not use an initial path, couldn't get the home directory path: %s", SDL_GetError());
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, IMAGE_RECT.w, IMAGE_RECT.h);
    if (!texture) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create texture!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    } else {
        static const Uint32 blank[220 * 140];
        const SDL_Rect rect = { 0, 0, IMAGE_RECT.w, IMAGE_RECT.h };
        SDL_UpdateTexture(texture, &rect, blank, IMAGE_RECT.w * sizeof(Uint32));
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const SDL_FPoint p = { event->button.x, event->button.y };
        SDL_Window *w = SDL_GetWindowFromID(event->button.windowID);
        if (SDL_PointInRectFloat(&p, &OPEN_FILE_RECT)) {
            SDL_ShowOpenFileDialog(callback, NULL, w, filters, SDL_arraysize(filters), initial_path, 1);
        }
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &OPEN_FILE_RECT);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugText(renderer, OPEN_FILE_RECT.x + 5, OPEN_FILE_RECT.y + OPEN_FILE_RECT.h / 2, "Open File...");

    SDL_FRect rect;
    SDL_RectToFRect(&IMAGE_RECT, &rect);
    SDL_RenderTexture(renderer, texture, NULL, &rect);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyTexture(texture);

    /* SDL will clean up the window/renderer for us. */
}
