/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Test conversion format */

#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

static SDLTest_CommonState *state;

/* Definition format conversions */
static const Uint32 g_AllFormats[] = {
    SDL_PIXELFORMAT_RGBA8888,
    SDL_PIXELFORMAT_YV12,
    SDL_PIXELFORMAT_IYUV,
    SDL_PIXELFORMAT_YUY2,
    SDL_PIXELFORMAT_UYVY,
    SDL_PIXELFORMAT_YVYU,
    SDL_PIXELFORMAT_NV12,
    SDL_PIXELFORMAT_NV21
};
static const int g_numAllFormats = SDL_arraysize(g_AllFormats);

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surf_orig;
    int fmt_src;
    int fmt_dst;
    int fmt_mode;
} DrawState;

static DrawState *drawstates;
static int done;
static int draw_next = 1;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDLTest_CommonQuit(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}


static SDL_Surface *SDL_UnpackYUVSurface(SDL_Surface *surf) {
    /* TODO: make the plane non contiguous in memory / also with various pitches */
    return SDL_DuplicateSurface(surf);
}

static void Draw(DrawState *s)
{
    SDL_Rect viewport;
    SDL_Texture *tex;
    SDL_Surface *src;
    SDL_Surface *dst;
    Uint32 fmt_src = g_AllFormats[s->fmt_src];
    Uint32 fmt_dst = g_AllFormats[s->fmt_dst];
    int src_contiguous = 1;
    int dst_contiguous = 1;
    const char *src_str = "";
    const char *dst_str = "";

    if (s->fmt_mode == 0) {
        src_contiguous = 1;
        dst_contiguous = 1;
    } else if (s->fmt_mode == 1) {
        src_contiguous = 0;
        dst_contiguous = 1;
    } else if (s->fmt_mode == 2) {
        src_contiguous = 1;
        dst_contiguous = 0;
    }
  
    src_str = (src_contiguous ? "" : "(non-contiguous)");
    dst_str = (dst_contiguous ? "" : "(non-contiguous)");

    /* non-contiguous only make sense with YUV */
    if (!SDL_ISPIXELFORMAT_FOURCC(fmt_src)) {
        src_str = "";
        src_contiguous = 0;
    }
    if (!SDL_ISPIXELFORMAT_FOURCC(fmt_dst)) {
        dst_str = "";
        dst_contiguous = 0;
    }

    SDL_Log("------- Convert %s %s -> %s %s", SDL_GetPixelFormatName(fmt_src), src_str, SDL_GetPixelFormatName(fmt_dst), dst_str);

    src = SDL_ConvertSurfaceFormat(s->surf_orig, fmt_src);
    if (!src) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed conversion to create src. %s -> %s", 
                SDL_GetPixelFormatName(s->surf_orig->format->format),
                SDL_GetPixelFormatName(fmt_src));
        quit(2);
    }

    if (!src_contiguous) {
        /* make src surf non-contiguous (to test SDL_ConvertSurfaceFormat()) */
        SDL_Surface *tmp = SDL_UnpackYUVSurface(src);
        if (!tmp) { 
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed SDL_UnpackYUVSurface: %s", SDL_GetError()); 
            quit(2);
        }
        SDL_DestroySurface(src);
        src = tmp;
    }

    dst = SDL_ConvertSurfaceFormat(src, fmt_dst);
    if (!dst) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed conversion %s -> %s", 
                SDL_GetPixelFormatName(src->format->format),
                SDL_GetPixelFormatName(fmt_dst));
        quit(2);
    }

    if (!dst_contiguous) {
        /* make src surf non-contiguous (to test SDL_CreateTextureFromSurface())*/
        SDL_Surface *tmp = SDL_UnpackYUVSurface(dst);
        if (!tmp) { 
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed SDL_UnpackYUVSurface: %s", SDL_GetError()); 
            quit(2);
        }
        SDL_DestroySurface(dst);
        dst = tmp;
    }

    SDL_GetRenderViewport(s->renderer, &viewport);

    tex = SDL_CreateTextureFromSurface(s->renderer, dst);
    if (!dst) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed SDL_CreateTextureFromSurface: %s", SDL_GetError()); 
        quit(2);
    } else {
        /* Draw the texture */
        SDL_RenderTexture(s->renderer, tex, NULL, NULL);
    }

    /* Update the screen! */
    SDL_RenderPresent(s->renderer);
    SDL_DestroyTexture(tex);
    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);

    s->fmt_dst += 1;

    if (s->fmt_dst >= g_numAllFormats) {
        s->fmt_dst = 0;
        s->fmt_src += 1;
    }

    if (s->fmt_src >= g_numAllFormats) {
        s->fmt_src = 0;
        s->fmt_mode += 1;
    }

    if (s->fmt_mode == 3) {
        SDL_Log("done!");
        done = 1;
    }
}


static void loop(void)
{
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.keysym.sym == SDLK_SPACE) {
                draw_next = 1;
            }
        }
        SDLTest_CommonEvent(state, &event, &done);
    }
    if (draw_next) {
        for (i = 0; i < state->num_windows; ++i) {
            if (state->windows[i] == NULL) {
                continue;
            }
            Draw(&drawstates[i]);
        }

        /* comment out here, to run in one shot */
        /* draw_next = 0; */
    }
#ifdef __EMSCRIPTEN__
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif
}

int main(int argc, char *argv[])
{
    int i;
    int frames;
    Uint64 then, now;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        quit(1);
    }

    drawstates = SDL_stack_alloc(DrawState, state->num_windows);
    for (i = 0; i < state->num_windows; ++i) {
        DrawState *drawstate = &drawstates[i];
        SDL_Surface *temp;
        char *path;
        char *file = "sample.bmp";

        drawstate->window = state->windows[i];
        drawstate->renderer = state->renderers[i];
       
        path = GetNearbyFilename(file);

        if (path) {
            file = path;
        }

        temp = SDL_LoadBMP(file);
        if (!temp) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", file, SDL_GetError());
            quit(2);
        }

        drawstate->surf_orig = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_RGB24);

        SDL_DestroySurface(temp);

        if (!drawstate->surf_orig) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't convert: %s", SDL_GetError());
            quit(3);
        }

        drawstate->fmt_src = 0;
        drawstate->fmt_dst = 0;
        drawstate->fmt_mode = 0;
    }

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        ++frames;
        loop();
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
    }

    for (i = 0; i < state->num_windows; ++i) {
        DrawState *drawstate = &drawstates[i];
        SDL_DestroySurface(drawstate->surf_orig);
    }
 
    SDL_stack_free(drawstates);

    quit(0);
    return 0;
}
