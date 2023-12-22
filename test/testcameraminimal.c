/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_main.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDLTest_CommonState *state = NULL;
static SDL_Camera *camera = NULL;
static SDL_CameraSpec spec;
static SDL_Texture *texture = NULL;
static SDL_bool texture_updated = SDL_FALSE;
static SDL_Surface *frame_current = NULL;

int SDL_AppInit(int argc, char *argv[])
{
    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return -1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow("Local Video", 1000, 800, 0);
    if (window == NULL) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return -1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

    renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return -1;
    }

    SDL_CameraDeviceID *devices = SDL_GetCameraDevices(NULL);
    if (!devices) {
        SDL_Log("SDL_GetCameraDevices failed: %s", SDL_GetError());
        return -1;
    }

    const SDL_CameraDeviceID devid = devices[0];  /* just take the first one. */
    SDL_free(devices);

    if (!devid) {
        SDL_Log("No cameras available? %s", SDL_GetError());
        return -1;
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
        return -1;
    }

    return 0;  /* start the main app loop. */
}

int SDL_AppEvent(const SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            const SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                SDL_Log("Key : Escape!");
                return 1;
            }
            break;
        }

        case SDL_EVENT_QUIT:
            SDL_Log("Ctlr+C : Quit!");
            return 1;

        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
            if (SDL_GetCameraFormat(camera, &spec) < 0) {
                SDL_Log("Couldn't get camera spec: %s", SDL_GetError());
                return -1;
            }

            /* Create texture with appropriate format */
            texture = SDL_CreateTexture(renderer, spec.format, SDL_TEXTUREACCESS_STATIC, spec.width, spec.height);
            if (texture == NULL) {
                SDL_Log("Couldn't create texture: %s", SDL_GetError());
                return -1;
            }
            break;

        case SDL_EVENT_CAMERA_DEVICE_DENIED:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Camera permission denied!", "User denied access to the camera!", window);
            return -1;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

int SDL_AppIterate(void)
{
    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);

    if (texture != NULL) {   /* if not NULL, camera is ready to go. */
        int win_w, win_h, tw, th;
        SDL_FRect d;
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
             texture_updated = SDL_FALSE;
        }

        /* Update SDL_Texture with last video frame (only once per new frame) */
        if (frame_current && !texture_updated) {
            SDL_UpdateTexture(texture, NULL, frame_current->pixels, frame_current->pitch);
            texture_updated = SDL_TRUE;
        }

        SDL_QueryTexture(texture, NULL, NULL, &tw, &th);
        SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
        d.x = (float) ((win_w - tw) / 2);
        d.y = (float) ((win_h - th) / 2);
        d.w = (float) tw;
        d.h = (float) th;
        SDL_RenderTexture(renderer, texture, NULL, &d);
    }

    SDL_RenderPresent(renderer);

    return 0;  /* keep iterating. */
}

void SDL_AppQuit(void)
{
    SDL_ReleaseCameraFrame(camera, frame_current);
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDLTest_CommonDestroyState(state);
}

