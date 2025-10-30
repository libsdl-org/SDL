/*
 * This example code loads a bitmap with asynchronous i/o and renders it.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AsyncIOQueue *queue = NULL;

#define TOTAL_TEXTURES 4
static const char * const pngs[TOTAL_TEXTURES] = { "sample.png", "gamepad_front.png", "speaker.png", "icon2x.png" };
static SDL_Texture *textures[TOTAL_TEXTURES];
static const SDL_FRect texture_rects[TOTAL_TEXTURES] = {
    { 116, 156, 408, 167 },
    { 20, 200, 96, 60 },
    { 525, 180, 96, 96 },
    { 288, 375, 64, 64 }
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int i;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/asyncio/load-bitmaps", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    queue = SDL_CreateAsyncIOQueue();
    if (!queue) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create async i/o queue!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* Load some .png files asynchronously from wherever the app is being run from, put them in the same queue. */
    for (i = 0; i < SDL_arraysize(pngs); i++) {
        char *path = NULL;
        SDL_asprintf(&path, "%s%s", SDL_GetBasePath(), pngs[i]);  /* allocate a string of the full file path */
        /* you _should) check for failure, but we'll just go on without files here. */
        SDL_LoadFileAsync(path, queue, (void *) pngs[i]);  /* attach the filename as app-specific data, so we can see it later. */
        SDL_free(path);
    }

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
    SDL_AsyncIOOutcome outcome;
    int i;

    if (SDL_GetAsyncIOResult(queue, &outcome)) {  /* a .png file load has finished? */
        if (outcome.result == SDL_ASYNCIO_COMPLETE) {
            /* this might be _any_ of the pngs; they might finish loading in any order. */
            for (i = 0; i < SDL_arraysize(pngs); i++) {
                /* this doesn't need a strcmp because we gave the pointer from this array to SDL_LoadFileAsync */
                if (outcome.userdata == pngs[i]) {
                    break;
                }
            }

            if (i < SDL_arraysize(pngs)) {  /* (just in case.) */
                SDL_Surface *surface = SDL_LoadPNG_IO(SDL_IOFromConstMem(outcome.buffer, (size_t) outcome.bytes_transferred), true);
                if (surface) {  /* the renderer is not multithreaded, so create the texture here once the data loads. */
                    textures[i] = SDL_CreateTextureFromSurface(renderer, surface);
                    if (!textures[i]) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create texture!", SDL_GetError(), NULL);
                        return SDL_APP_FAILURE;
                    }
                    SDL_DestroySurface(surface);
                }
            }
        }
        SDL_free(outcome.buffer);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (i = 0; i < SDL_arraysize(textures); i++) {
        SDL_RenderTexture(renderer, textures[i], NULL, &texture_rects[i]);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    int i;

    SDL_DestroyAsyncIOQueue(queue);

    for (i = 0; i < SDL_arraysize(textures); i++) {
        SDL_DestroyTexture(textures[i]);
    }

    /* SDL will clean up the window/renderer for us. */
}

