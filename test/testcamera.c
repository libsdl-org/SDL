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
static SDL_CameraDeviceID front_camera = 0;
static SDL_CameraDeviceID back_camera = 0;

int SDL_AppInit(void **appstate, int argc, char *argv[])
{
    int devcount = 0;
    int i;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO | SDL_INIT_CAMERA);
    if (!state) {
        return -1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return -1;
    }

    state->num_windows = 1;

    /* Load the SDL library */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return -1;
    }

    window = state->windows[0];
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return -1;
    }

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    renderer = state->renderers[0];
    if (!renderer) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return -1;
    }

    SDL_CameraDeviceID *devices = SDL_GetCameraDevices(&devcount);
    if (!devices) {
        SDL_Log("SDL_GetCameraDevices failed: %s", SDL_GetError());
        return -1;
    }

    SDL_Log("Saw %d camera devices.", devcount);
    for (i = 0; i < devcount; i++) {
        const SDL_CameraDeviceID device = devices[i];
        char *name = SDL_GetCameraDeviceName(device);
        const SDL_CameraPosition position = SDL_GetCameraDevicePosition(device);
        const char *posstr = "";
        if (position == SDL_CAMERA_POSITION_FRONT_FACING) {
            front_camera = device;
            posstr = "[front-facing] ";
        } else if (position == SDL_CAMERA_POSITION_BACK_FACING) {
            back_camera = device;
            posstr = "[back-facing] ";
        }
        SDL_Log("  - Camera #%d: %s %s", i, posstr, name);
        SDL_free(name);
    }

    const SDL_CameraDeviceID devid = front_camera ? front_camera : devices[0];  /* no front-facing? just take the first one. */
    SDL_free(devices);

    if (!devid) {
        SDL_Log("No cameras available?");
        return -1;
    }

    SDL_CameraSpec *pspec = &spec;
    spec.interval_numerator = 1000;
    spec.interval_denominator = 1;

    camera = SDL_OpenCameraDevice(devid, pspec);
    if (!camera) {
        SDL_Log("Failed to open camera device: %s", SDL_GetError());
        return -1;
    }

    return 0;  /* start the main app loop. */
}


static int FlipCamera(void)
{
    static Uint64 last_flip = 0;
    if ((SDL_GetTicks() - last_flip) < 3000) {  /* must wait at least 3 seconds between flips. */
        return 0;
    }

    if (camera) {
        const SDL_CameraDeviceID current = SDL_GetCameraInstanceID(camera);
        SDL_CameraDeviceID nextcam = 0;
        if (current == front_camera) {
            nextcam = back_camera;
        } else if (current == back_camera) {
            nextcam = front_camera;
        }

        if (nextcam) {
            SDL_Log("Flip camera!");

            if (frame_current) {
                SDL_ReleaseCameraFrame(camera, frame_current);
                frame_current = NULL;
            }

            SDL_CloseCamera(camera);

            if (texture) {
                SDL_DestroyTexture(texture);
                texture = NULL;  /* will rebuild when new camera is approved. */
            }

            camera = SDL_OpenCameraDevice(nextcam, NULL);
            if (!camera) {
                SDL_Log("Failed to open camera device: %s", SDL_GetError());
                return -1;
            }

            last_flip = SDL_GetTicks();
        }
    }

    return 0;
}

int SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            const SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                SDL_Log("Key : Escape!");
                return 1;
            } else if (sym == SDLK_SPACE) {
                FlipCamera();
                return 0;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            /* !!! FIXME: only flip if clicked in the area of a "flip" icon. */
            return FlipCamera();

        case SDL_EVENT_QUIT:
            SDL_Log("Ctlr+C : Quit!");
            return 1;

        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
            SDL_Log("Camera approved!");
            if (SDL_GetCameraFormat(camera, &spec) < 0) {
                SDL_Log("Couldn't get camera spec: %s", SDL_GetError());
                return -1;
            }

            /* Resize the window to match */
            SDL_SetWindowSize(window, spec.width, spec.height);

            /* Create texture with appropriate format */
            SDL_assert(texture == NULL);
            texture = SDL_CreateTexture(renderer, spec.format, SDL_TEXTUREACCESS_STREAMING, spec.width, spec.height);
            if (!texture) {
                SDL_Log("Couldn't create texture: %s", SDL_GetError());
                return -1;
            }
            break;

        case SDL_EVENT_CAMERA_DEVICE_DENIED:
            SDL_Log("Camera denied!");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Camera permission denied!", "User denied access to the camera!", window);
            return -1;
        default:
            break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

int SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);

    if (texture) {   /* if not NULL, camera is ready to go. */
        int win_w, win_h, tw, th;
        SDL_FRect d;
        Uint64 timestampNS = 0;
        SDL_Surface *frame_next = camera ? SDL_AcquireCameraFrame(camera, &timestampNS) : NULL;

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

    /* !!! FIXME: Render a "flip" icon if front_camera and back_camera are both != 0. */

    SDL_RenderPresent(renderer);

    return 0;  /* keep iterating. */
}

void SDL_AppQuit(void *appstate)
{
    SDL_ReleaseCameraFrame(camera, frame_current);
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDLTest_CommonQuit(state);
}

