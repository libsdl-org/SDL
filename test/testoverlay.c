/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/********************************************************************************
 *                                                                              *
 * Test of the overlay used for moved pictures, test more closed to real life.  *
 * Running trojan moose :) Coded by Mike Gorchak.                               *
 *                                                                              *
 ********************************************************************************/

#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#define MOOSEPIC_W 64
#define MOOSEPIC_H 88

#define MOOSEFRAME_SIZE   (MOOSEPIC_W * MOOSEPIC_H)
#define MOOSEFRAMES_COUNT 10

/* *INDENT-OFF* */ /* clang-format off */
static SDL_Color MooseColors[84] = {
    {49, 49, 49, SDL_ALPHA_OPAQUE}
    , {66, 24, 0, SDL_ALPHA_OPAQUE}
    , {66, 33, 0, SDL_ALPHA_OPAQUE}
    , {66, 66, 66, SDL_ALPHA_OPAQUE}
    ,
    {66, 115, 49, SDL_ALPHA_OPAQUE}
    , {74, 33, 0, SDL_ALPHA_OPAQUE}
    , {74, 41, 16, SDL_ALPHA_OPAQUE}
    , {82, 33, 8, SDL_ALPHA_OPAQUE}
    ,
    {82, 41, 8, SDL_ALPHA_OPAQUE}
    , {82, 49, 16, SDL_ALPHA_OPAQUE}
    , {82, 82, 82, SDL_ALPHA_OPAQUE}
    , {90, 41, 8, SDL_ALPHA_OPAQUE}
    ,
    {90, 41, 16, SDL_ALPHA_OPAQUE}
    , {90, 57, 24, SDL_ALPHA_OPAQUE}
    , {99, 49, 16, SDL_ALPHA_OPAQUE}
    , {99, 66, 24, SDL_ALPHA_OPAQUE}
    ,
    {99, 66, 33, SDL_ALPHA_OPAQUE}
    , {99, 74, 33, SDL_ALPHA_OPAQUE}
    , {107, 57, 24, SDL_ALPHA_OPAQUE}
    , {107, 82, 41, SDL_ALPHA_OPAQUE}
    ,
    {115, 57, 33, SDL_ALPHA_OPAQUE}
    , {115, 66, 33, SDL_ALPHA_OPAQUE}
    , {115, 66, 41, SDL_ALPHA_OPAQUE}
    , {115, 74, 0, SDL_ALPHA_OPAQUE}
    ,
    {115, 90, 49, SDL_ALPHA_OPAQUE}
    , {115, 115, 115, SDL_ALPHA_OPAQUE}
    , {123, 82, 0, SDL_ALPHA_OPAQUE}
    , {123, 99, 57, SDL_ALPHA_OPAQUE}
    ,
    {132, 66, 41, SDL_ALPHA_OPAQUE}
    , {132, 74, 41, SDL_ALPHA_OPAQUE}
    , {132, 90, 8, SDL_ALPHA_OPAQUE}
    , {132, 99, 33, SDL_ALPHA_OPAQUE}
    ,
    {132, 99, 66, SDL_ALPHA_OPAQUE}
    , {132, 107, 66, SDL_ALPHA_OPAQUE}
    , {140, 74, 49, SDL_ALPHA_OPAQUE}
    , {140, 99, 16, SDL_ALPHA_OPAQUE}
    ,
    {140, 107, 74, SDL_ALPHA_OPAQUE}
    , {140, 115, 74, SDL_ALPHA_OPAQUE}
    , {148, 107, 24, SDL_ALPHA_OPAQUE}
    , {148, 115, 82, SDL_ALPHA_OPAQUE}
    ,
    {148, 123, 74, SDL_ALPHA_OPAQUE}
    , {148, 123, 90, SDL_ALPHA_OPAQUE}
    , {156, 115, 33, SDL_ALPHA_OPAQUE}
    , {156, 115, 90, SDL_ALPHA_OPAQUE}
    ,
    {156, 123, 82, SDL_ALPHA_OPAQUE}
    , {156, 132, 82, SDL_ALPHA_OPAQUE}
    , {156, 132, 99, SDL_ALPHA_OPAQUE}
    , {156, 156, 156, SDL_ALPHA_OPAQUE}
    ,
    {165, 123, 49, SDL_ALPHA_OPAQUE}
    , {165, 123, 90, SDL_ALPHA_OPAQUE}
    , {165, 132, 82, SDL_ALPHA_OPAQUE}
    , {165, 132, 90, SDL_ALPHA_OPAQUE}
    ,
    {165, 132, 99, SDL_ALPHA_OPAQUE}
    , {165, 140, 90, SDL_ALPHA_OPAQUE}
    , {173, 132, 57, SDL_ALPHA_OPAQUE}
    , {173, 132, 99, SDL_ALPHA_OPAQUE}
    ,
    {173, 140, 107, SDL_ALPHA_OPAQUE}
    , {173, 140, 115, SDL_ALPHA_OPAQUE}
    , {173, 148, 99, SDL_ALPHA_OPAQUE}
    , {173, 173, 173, SDL_ALPHA_OPAQUE}
    ,
    {181, 140, 74, SDL_ALPHA_OPAQUE}
    , {181, 148, 115, SDL_ALPHA_OPAQUE}
    , {181, 148, 123, SDL_ALPHA_OPAQUE}
    , {181, 156, 107, SDL_ALPHA_OPAQUE}
    ,
    {189, 148, 123, SDL_ALPHA_OPAQUE}
    , {189, 156, 82, SDL_ALPHA_OPAQUE}
    , {189, 156, 123, SDL_ALPHA_OPAQUE}
    , {189, 156, 132, SDL_ALPHA_OPAQUE}
    ,
    {189, 189, 189, SDL_ALPHA_OPAQUE}
    , {198, 156, 123, SDL_ALPHA_OPAQUE}
    , {198, 165, 132, SDL_ALPHA_OPAQUE}
    , {206, 165, 99, SDL_ALPHA_OPAQUE}
    ,
    {206, 165, 132, SDL_ALPHA_OPAQUE}
    , {206, 173, 140, SDL_ALPHA_OPAQUE}
    , {206, 206, 206, SDL_ALPHA_OPAQUE}
    , {214, 173, 115, SDL_ALPHA_OPAQUE}
    ,
    {214, 173, 140, SDL_ALPHA_OPAQUE}
    , {222, 181, 148, SDL_ALPHA_OPAQUE}
    , {222, 189, 132, SDL_ALPHA_OPAQUE}
    , {222, 189, 156, SDL_ALPHA_OPAQUE}
    ,
    {222, 222, 222, SDL_ALPHA_OPAQUE}
    , {231, 198, 165, SDL_ALPHA_OPAQUE}
    , {231, 231, 231, SDL_ALPHA_OPAQUE}
    , {239, 206, 173, SDL_ALPHA_OPAQUE}
};
/* *INDENT-ON* */ /* clang-format on */

static SDLTest_CommonState *state;
static Uint64 next_fps_check;
static Uint32 frames;
static const Uint32 fps_check_delay = 5000;

static Uint32 yuv_format = SDL_PIXELFORMAT_YV12;
static SDL_Surface *MooseYUVSurfaces[MOOSEFRAMES_COUNT];
static SDL_Texture *MooseTexture = NULL;
static SDL_FRect displayrect;
static int window_w;
static int window_h;
static int paused = 0;
static int done = 0;
static int fpsdelay;
static bool streaming = true;
static Uint8 *RawMooseData = NULL;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    int i;

    /* If rc is 0, just let main return normally rather than calling exit.
     * This allows testing of platforms where SDL_main is required and does meaningful cleanup.
     */

    SDL_free(RawMooseData);

    for (i = 0; i < MOOSEFRAMES_COUNT; i++) {
         SDL_DestroySurface(MooseYUVSurfaces[i]);
    }

    SDLTest_CommonQuit(state);

    /* Let 'main()' return normally */
    if (rc != 0) {
         exit(rc);
    }
}

static void MoveSprites(SDL_Renderer *renderer)
{
    static int i = 0;

    if (streaming) {
         if (!paused) {
             i = (i + 1) % MOOSEFRAMES_COUNT;
             /* Test both upload paths for NV12/NV21 formats */
             if ((yuv_format == SDL_PIXELFORMAT_NV12 || yuv_format == SDL_PIXELFORMAT_NV21) &&
                 (i % 2) == 0) {
#ifdef TEST_RECT_UPDATE
                 SDL_Rect rect;

                 if (i == 0) {
                     rect.x = 0;
                     rect.y = 0;
                     rect.w = MOOSEPIC_W;
                     rect.h = MOOSEPIC_H;
                 } else {
                     rect.x = MOOSEPIC_W / 4;
                     rect.y = MOOSEPIC_H / 4;
                     rect.w = MOOSEPIC_W / 2;
                     rect.h = MOOSEPIC_H / 2;
                 }
                 SDL_UpdateNVTexture(MooseTexture, &rect,
                                     (Uint8 *)MooseYUVSurfaces[i]->pixels + rect.y * MooseYUVSurfaces[i]->pitch + rect.x, MooseYUVSurfaces[i]->pitch,
                                     (Uint8 *)MooseYUVSurfaces[i]->pixels + MOOSEFRAME_SIZE + (rect.y + 1) / 2 * MooseYUVSurfaces[i]->pitch + (rect.x + 1) / 2, MooseYUVSurfaces[i]->pitch);
#else
                 SDL_UpdateNVTexture(MooseTexture, NULL,
                                     MooseYUVSurfaces[i]->pixels, MooseYUVSurfaces[i]->pitch,
                                     (Uint8 *)MooseYUVSurfaces[i]->pixels + MOOSEFRAME_SIZE, MooseYUVSurfaces[i]->pitch);
#endif
             } else {
                 SDL_UpdateTexture(MooseTexture, NULL, MooseYUVSurfaces[i]->pixels, MooseYUVSurfaces[i]->pitch);
             }
         }
         SDL_RenderClear(renderer);
         SDL_RenderTexture(renderer, MooseTexture, NULL, &displayrect);
         SDL_RenderPresent(renderer);
    } else {
         SDL_Texture *tmp;

         /* Test SDL_CreateTextureFromSurface */
         if (!paused) {
             i = (i + 1) % MOOSEFRAMES_COUNT;
         }

         tmp = SDL_CreateTextureFromSurface(renderer, MooseYUVSurfaces[i]);
         if (!tmp) {
             SDL_Log("Error %s", SDL_GetError());
             quit(7);
         }

         SDL_RenderClear(renderer);
         SDL_RenderTexture(renderer, tmp, NULL, &displayrect);
         SDL_RenderPresent(renderer);
         SDL_DestroyTexture(tmp);
    }
}


static void loop(void)
{
    Uint64 now;
    int i;
    SDL_Event event;

    SDL_Renderer *renderer = state->renderers[0]; /* only 1 window */

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
        SDL_ConvertEventToRenderCoordinates(SDL_GetRenderer(SDL_GetWindowFromEvent(&event)), &event);

        switch (event.type) {
        case SDL_EVENT_WINDOW_RESIZED:
            SDL_SetRenderViewport(renderer, NULL);
            window_w = event.window.data1;
            window_h = event.window.data2;
            displayrect.w = (float)window_w;
            displayrect.h = (float)window_h;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            displayrect.x = event.button.x - window_w / 2;
            displayrect.y = event.button.y - window_h / 2;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.state) {
                displayrect.x = event.motion.x - window_w / 2;
                displayrect.y = event.motion.y - window_h / 2;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_SPACE) {
                paused = !paused;
                break;
            }
            if (event.key.key != SDLK_ESCAPE) {
                break;
            }
            break;
        default:
            break;
        }
    }

#ifndef SDL_PLATFORM_EMSCRIPTEN
    SDL_Delay(fpsdelay);
#endif

    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL) {
            continue;
        }
        MoveSprites(state->renderers[i]);
    }
#ifdef SDL_PLATFORM_EMSCRIPTEN
    if (done) {
        emscripten_cancel_main_loop();
    }
#endif

    frames++;
    now = SDL_GetTicks();
    if (now >= next_fps_check) {
        /* Print out some timing information */
        const Uint64 then = next_fps_check - fps_check_delay;
        const double fps = ((double)frames * 1000) / (now - then);
        SDL_Log("%2.2f frames per second\n", fps);
        next_fps_check = now + fps_check_delay;
        frames = 0;
    }
}


int main(int argc, char **argv)
{
    SDL_IOStream *handle;
    int i;
    int j;
    int fps = 12;
    int nodelay = 0;
    int scale = 5;
    char *filename = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    SDL_zeroa(MooseYUVSurfaces);

    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcmp(argv[i], "--fps") == 0) {
                if (argv[i + 1]) {
                    consumed = 2;
                    fps = SDL_atoi(argv[i + 1]);
                    if (fps == 0) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --fps option requires an argument [from 1 to 1000], default is 12.\n");
                        quit(10);
                    }
                    if ((fps < 0) || (fps > 1000)) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --fps option must be in range from 1 to 1000, default is 12.\n");
                        quit(10);
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --fps option requires an argument [from 1 to 1000], default is 12.\n");
                    quit(10);
                }
            } else if (SDL_strcmp(argv[i], "--nodelay") == 0) {
                consumed = 1;
                nodelay = 1;
            } else if (SDL_strcmp(argv[i], "--nostreaming") == 0) {
                consumed = 1;
                streaming = false;
            } else if (SDL_strcmp(argv[i], "--scale") == 0) {
                consumed = 2;
                if (argv[i + 1]) {
                    scale = SDL_atoi(argv[i + 1]);
                    if (scale == 0) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --scale option requires an argument [from 1 to 50], default is 5.\n");
                        quit(10);
                    }
                    if ((scale < 0) || (scale > 50)) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --scale option must be in range from 1 to 50, default is 5.\n");
                        quit(10);
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --fps option requires an argument [from 1 to 1000], default is 12.\n");
                    quit(10);
                }
            } else if (SDL_strcmp(argv[i], "--yuvformat") == 0) {
                consumed = 2;
                if (argv[i + 1]) {
                    char *fmt = argv[i + 1];

                    if (SDL_strcmp(fmt, "YV12") == 0) {
                        yuv_format = SDL_PIXELFORMAT_YV12;
                    } else if (SDL_strcmp(fmt, "IYUV") == 0) {
                        yuv_format = SDL_PIXELFORMAT_IYUV;
                    } else if (SDL_strcmp(fmt, "YUY2") == 0) {
                        yuv_format = SDL_PIXELFORMAT_YUY2;
                    } else if (SDL_strcmp(fmt, "UYVY") == 0) {
                        yuv_format = SDL_PIXELFORMAT_UYVY;
                    } else if (SDL_strcmp(fmt, "YVYU") == 0) {
                        yuv_format = SDL_PIXELFORMAT_YVYU;
                    } else if (SDL_strcmp(fmt, "NV12") == 0) {
                        yuv_format = SDL_PIXELFORMAT_NV12;
                    } else if (SDL_strcmp(fmt, "NV21") == 0) {
                        yuv_format = SDL_PIXELFORMAT_NV21;
                    } else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --yuvformat option requires one of the: YV12 (default), IYUV, YUY2, UYVY, YVYU, NV12, NV21)\n");
                        quit(10);
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "The --yuvformat option requires one of the: YV12 (default), IYUV, YUY2, UYVY, YVYU, NV12, NV21)\n");
                    quit(10);
                }
            }

        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--fps <frames per second>]",
                "[--nodelay]",
                "[--yuvformat <fmt>] (one of the: YV12 (default), IYUV, YUY2, UYVY, YVYU, NV12, NV21)",
                "[--scale <scale factor>] (initial scale of the overlay)",
                "[--nostreaming] path that use SDL_CreateTextureFromSurface() not STREAMING texture",
                NULL
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }

    /* Force window size */
    state->window_w = MOOSEPIC_W * scale;
    state->window_h = MOOSEPIC_H * scale;

    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    RawMooseData = (Uint8 *)SDL_malloc(MOOSEFRAME_SIZE * MOOSEFRAMES_COUNT);
    if (!RawMooseData) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can't allocate memory for movie !\n");
        quit(1);
    }

    /* load the trojan moose images */
    filename = GetResourceFilename(NULL, "moose.dat");
    if (!filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory\n");
        quit(2);
    }
    handle = SDL_IOFromFile(filename, "rb");
    SDL_free(filename);
    if (!handle) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can't find the file moose.dat !\n");
        quit(2);
    }

    SDL_ReadIO(handle, RawMooseData, MOOSEFRAME_SIZE * MOOSEFRAMES_COUNT);

    SDL_CloseIO(handle);

    /* Create the window and renderer */
    window_w = MOOSEPIC_W * scale;
    window_h = MOOSEPIC_H * scale;

    if (state->num_windows != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Only one window allowed\n");
        quit(1);
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];

        if (streaming) {
            MooseTexture = SDL_CreateTexture(renderer, yuv_format, SDL_TEXTUREACCESS_STREAMING, MOOSEPIC_W, MOOSEPIC_H);
            if (!MooseTexture) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set create texture: %s\n", SDL_GetError());
                quit(5);
            }
        }
    }

    /* Uncomment this to check vertex color with a YUV texture */
    /* SDL_SetTextureColorMod(MooseTexture, 0xff, 0x80, 0x80); */

    for (i = 0; i < MOOSEFRAMES_COUNT; i++) {
        /* Create RGB SDL_Surface */
        SDL_Surface *mooseRGBSurface = SDL_CreateSurface(MOOSEPIC_W, MOOSEPIC_H, SDL_PIXELFORMAT_RGB24);
        if (!mooseRGBSurface) {
            quit(6);
        }

        /* Load Moose into a RGB SDL_Surface */
        {
            Uint8 *rgb = mooseRGBSurface->pixels;
            Uint8 *frame = RawMooseData + i * MOOSEFRAME_SIZE;
            for (j = 0; j < MOOSEFRAME_SIZE; ++j) {
                rgb[0] = MooseColors[frame[j]].r;
                rgb[1] = MooseColors[frame[j]].g;
                rgb[2] = MooseColors[frame[j]].b;
                rgb += 3;
            }
        }

        /* Convert to YUV SDL_Surface */
        MooseYUVSurfaces[i] = SDL_ConvertSurface(mooseRGBSurface, yuv_format);
        if (MooseYUVSurfaces[i] == NULL) {
            quit(7);
        }

        SDL_DestroySurface(mooseRGBSurface);
    }

    SDL_free(RawMooseData);
    RawMooseData = NULL;

    /* set the start frame */
    i = 0;
    if (nodelay) {
        fpsdelay = 0;
    } else {
        fpsdelay = 1000 / fps;
    }

    displayrect.x = 0;
    displayrect.y = 0;
    displayrect.w = (float)window_w;
    displayrect.h = (float)window_h;

    /* Ignore key up events, they don't even get filtered */
    SDL_SetEventEnabled(SDL_EVENT_KEY_UP, false);

    /* Main render loop */
    frames = 0;
    next_fps_check = SDL_GetTicks() + fps_check_delay;
    done = 0;

    /* Loop, waiting for QUIT or RESIZE */
#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, nodelay ? 0 : fps, 1);
#else
    while (!done) {
        loop();
    }
#endif

    quit(0);
    return 0;
}
