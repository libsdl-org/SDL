/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>

static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_AsyncIOQueue *queue = NULL;
static SDLTest_CommonState *state = NULL;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const char *base = NULL;
    SDL_AsyncIO *asyncio = NULL;
    char **bmps = NULL;
    int bmpcount = 0;
    int i;

    SDL_srand(0);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (consumed <= 0) {
            static const char *options[] = {
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            SDL_Quit();
            SDLTest_CommonDestroyState(state);
            return 1;
        }
        i += consumed;
    }

    state->num_windows = 1;

    /* Load the SDL library */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    renderer = state->renderers[0];
    if (!renderer) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return SDL_APP_FAILURE;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 512, 512);
    if (!texture) {
        SDL_Log("Couldn't create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    } else {
        static const Uint32 blank[512 * 512];
        const SDL_Rect rect = { 0, 0, 512, 512 };
        SDL_UpdateTexture(texture, &rect, blank, 512 * sizeof (Uint32));
    }

    queue = SDL_CreateAsyncIOQueue();
    if (!queue) {
        SDL_Log("Couldn't create async i/o queue: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    base = SDL_GetBasePath();
    bmps = SDL_GlobDirectory(base, "*.bmp", SDL_GLOB_CASEINSENSITIVE, &bmpcount);
    if (!bmps || (bmpcount == 0)) {
        SDL_Log("No BMP files found.");
        return SDL_APP_FAILURE;
    }

    for (i = 0; i < bmpcount; i++) {
        char *path = NULL;
        if (SDL_asprintf(&path, "%s%s", base, bmps[i]) < 0) {
            SDL_free(path);
        } else {
            SDL_Log("Loading %s...", path);
            SDL_LoadFileAsync(path, queue, path);
        }
    }

    SDL_free(bmps);

    SDL_Log("Opening asyncio.tmp...");
    asyncio = SDL_AsyncIOFromFile("asyncio.tmp", "w");
    if (!asyncio) {
        SDL_Log("Failed!");
        return SDL_APP_FAILURE;
    }
    SDL_WriteAsyncIO(asyncio, "hello", 0, 5, queue, "asyncio.tmp (write)");
    SDL_CloseAsyncIO(asyncio, true, queue, "asyncio.tmp (flush/close)");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        default:
            break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

static void async_io_task_complete(const SDL_AsyncIOOutcome *outcome)
{
    const char *fname = (const char *) outcome->userdata;
    const char *resultstr = "[unknown result]";

    switch (outcome->result) {
        #define RESCASE(x) case x: resultstr = #x; break
        RESCASE(SDL_ASYNCIO_COMPLETE);
        RESCASE(SDL_ASYNCIO_FAILURE);
        RESCASE(SDL_ASYNCIO_CANCELED);
        #undef RESCASE
    }

    SDL_Log("File '%s' async results: %s", fname, resultstr);

    if (SDL_strncmp(fname, "asyncio.tmp", 11) == 0) {
        return;
    }

    if (outcome->result == SDL_ASYNCIO_COMPLETE) {
        SDL_Surface *surface = SDL_LoadBMP_IO(SDL_IOFromConstMem(outcome->buffer, (size_t) outcome->bytes_transferred), true);
        if (surface) {
            SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
            SDL_DestroySurface(surface);
            if (converted) {
                const SDL_Rect rect = { 50 + SDL_rand(512 - 100), 50 + SDL_rand(512 - 100), converted->w, converted->h };
                SDL_UpdateTexture(texture, &rect, converted->pixels, converted->pitch);
                SDL_DestroySurface(converted);
            }
        }
    }

    SDL_free(outcome->userdata);
    SDL_free(outcome->buffer);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_AsyncIOOutcome outcome;
    if (SDL_GetAsyncIOResult(queue, &outcome)) {
        async_io_task_complete(&outcome);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_DestroyAsyncIOQueue(queue);
    SDL_DestroyTexture(texture);
    SDL_RemovePath("asyncio.tmp");
    SDLTest_CommonQuit(state);
}

