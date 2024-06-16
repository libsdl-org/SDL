/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple program: picks the offscreen backend and renders each frame to a bmp */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_opengl.h>

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;
static int done = SDL_FALSE;
static int frame_number = 0;
static int width = 640;
static int height = 480;
static unsigned int max_frames = 200;

static void draw(void)
{
    SDL_FRect rect;

    SDL_SetRenderDrawColor(renderer, 0x10, 0x9A, 0xCE, 0xFF);
    SDL_RenderClear(renderer);

    /* Grow based on the frame just to show a difference per frame of the region */
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = (float)((frame_number * 2) % width);
    rect.h = (float)((frame_number * 2) % height);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x10, 0x21, 0xFF);
    SDL_RenderFillRect(renderer, &rect);

    SDL_RenderPresent(renderer);
}

static void save_surface_to_bmp(void)
{
    SDL_Surface *surface;
    char file[128];

    surface = SDL_RenderReadPixels(renderer, NULL);

    (void)SDL_snprintf(file, sizeof(file), "SDL_window%" SDL_PRIs32 "-%8.8d.bmp",
                       SDL_GetWindowID(window), ++frame_number);

    SDL_SaveBMP(surface, file);
    SDL_DestroySurface(surface);
}

static void loop(void)
{
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            done = SDL_TRUE;
            break;
        default:
            break;
        }
    }

    draw();
    save_surface_to_bmp();

#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
#ifndef SDL_PLATFORM_EMSCRIPTEN
    Uint64 then, now;
    Uint32 frames;
#endif
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Force the offscreen renderer, if it cannot be created then fail out */
    SDL_SetHint("SDL_VIDEO_DRIVER", "offscreen");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Couldn't initialize the offscreen video driver: %s\n",
                SDL_GetError());
        return SDL_FALSE;
    }

    /* If OPENGL fails to init it will fallback to using a framebuffer for rendering */
    window = SDL_CreateWindow("Offscreen Test", width, height, 0);

    if (!window) {
        SDL_Log("Couldn't create window: %s\n", SDL_GetError());
        return SDL_FALSE;
    }

    renderer = SDL_CreateRenderer(window, NULL);

    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s\n",
                SDL_GetError());
        return SDL_FALSE;
    }

    SDL_RenderClear(renderer);

#ifndef SDL_PLATFORM_EMSCRIPTEN
    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;
#endif

    SDL_Log("Rendering %u frames offscreen\n", max_frames);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done && frames < max_frames) {
        ++frames;
        loop();

        /* Print out some timing information, along with remaining frames */
        if (frames % (max_frames / 10) == 0) {
            now = SDL_GetTicks();
            if (now > then) {
                double fps = ((double)frames * 1000) / (now - then);
                SDL_Log("Frames remaining: %" SDL_PRIu32 " rendering at %2.2f frames per second\n", max_frames - frames, fps);
            }
        }
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
