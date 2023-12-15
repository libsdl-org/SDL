/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_test.h"
#include "SDL3/SDL_camera.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdio.h>

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event evt;
    int quit = 0;
    SDLTest_CommonState  *state = NULL;

    SDL_Camera *camera = NULL;
    SDL_CameraSpec spec;
    SDL_Texture *texture = NULL;
    int texture_updated = 0;
    SDL_Surface *frame_current = NULL;

    SDL_zero(evt);
    SDL_zero(spec);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Local Video", 1000, 800, 0);
    if (window == NULL) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return 1;
    }

    SDL_CameraDeviceID *devices = SDL_GetCameraDevices(NULL);
    if (!devices) {
        SDL_Log("SDL_GetCameraDevices failed: %s", SDL_GetError());
        return 1;
    }

    const SDL_CameraDeviceID devid = devices[0];  /* just take the first one. */
    SDL_free(devices);

    if (!devid) {
        SDL_Log("No cameras available? %s", SDL_GetError());
        return 1;
    }
    
    SDL_CameraSpec *pspec = NULL;
    #if 0  /* just for edge-case testing purposes, ignore. */
    pspec = &spec;
    spec.width = 100 /*1280 * 2*/;
    spec.height = 100 /*720 * 2*/;
    spec.format = SDL_PIXELFORMAT_YUY2 /*SDL_PIXELFORMAT_RGBA8888*/;
    #endif

    camera = SDL_OpenCameraDevice(devid, pspec);
    if (!camera) {
        SDL_Log("Failed to open camera device: %s", SDL_GetError());
        return 1;
    }

   if (SDL_GetCameraSpec(camera, &spec) < 0) {
        SDL_Log("Couldn't get camera spec: %s", SDL_GetError());
        return 1;
    }

    /* Create texture with appropriate format */
    if (texture == NULL) {
        texture = SDL_CreateTexture(renderer, spec.format, SDL_TEXTUREACCESS_STATIC, spec.width, spec.height);
        if (texture == NULL) {
            SDL_Log("Couldn't create texture: %s", SDL_GetError());
            return 1;
        }
    }

    while (!quit) {
        while (SDL_PollEvent(&evt)) {
            int sym = 0;
            switch (evt.type)
            {
                case SDL_EVENT_KEY_DOWN:
                    {
                        sym = evt.key.keysym.sym;
                        break;
                    }

                case SDL_EVENT_QUIT:
                    {
                        quit = 1;
                        SDL_Log("Ctlr+C : Quit!");
                    }
            }

            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                quit = 1;
                SDL_Log("Key : Escape!");
            }
        }

        {
            Uint64 timestampNS = 0;
            SDL_Surface *frame_next = SDL_AcquireCameraFrame(camera, &timestampNS);

#if 0
            if (frame_next) {
                SDL_Log("frame: %p  at %" SDL_PRIu64, (void*)frame_next->pixels, timestampNS);
            }
#endif

            if (frame_next) {
                if (frame_current) {
                    if (SDL_ReleaseCameraFrame(camera, frame_current) < 0) {
                        SDL_Log("err SDL_ReleaseCameraFrame: %s", SDL_GetError());
                    }
                }

                /* It's not needed to keep the frame once updated the texture is updated.
                 * But in case of 0-copy, it's needed to have the frame while using the texture.
                 */
                frame_current = frame_next;
                texture_updated = 0;
            }
        }

        /* Update SDL_Texture with last video frame (only once per new frame) */
        if (frame_current && texture_updated == 0) {
            SDL_UpdateTexture(texture, NULL, frame_current->pixels, frame_current->pitch);
            texture_updated = 1;
        }

        SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
        SDL_RenderClear(renderer);
        {
            int win_w, win_h, tw, th, w;
            SDL_FRect d;
            SDL_QueryTexture(texture, NULL, NULL, &tw, &th);
            SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
            w = win_w;
            if (tw > w - 20) {
                float scale = (float) (w - 20) / (float) tw;
                tw = w - 20;
                th = (int)((float) th * scale);
            }
            d.x = (float)(10 );
            d.y = ((float)(win_h - th)) / 2.0f;
            d.w = (float)tw;
            d.h = (float)(th - 10);
            SDL_RenderTexture(renderer, texture, NULL, &d);
        }
        SDL_Delay(10);
        SDL_RenderPresent(renderer);
    }

    if (frame_current) {
        SDL_ReleaseCameraFrame(camera, frame_current);
    }
    SDL_CloseCamera(camera);

    if (texture) {
        SDL_DestroyTexture(texture);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}

