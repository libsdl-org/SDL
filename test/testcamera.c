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
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <SDL3/SDL_test_common.h>

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
    char window_title[128];
    int devcount = 0;
    int i;
    const char *camera_name = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO | SDL_INIT_CAMERA);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (SDL_strcmp(argv[i], "--camera") == 0 && argv[i+1]) {
                camera_name = argv[i+1];
                consumed = 2;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = {
                "[--camera name]",
                NULL,
            };
            SDLTest_CommonLogUsage(state, argv[0], options);
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

    window = state->windows[0];
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    renderer = state->renderers[0];
    if (!renderer) {
        /* SDL_Log("Couldn't create renderer: %s", SDL_GetError()); */
        return SDL_APP_FAILURE;
    }

    SDL_CameraDeviceID *devices = SDL_GetCameraDevices(&devcount);
    if (!devices) {
        SDL_Log("SDL_GetCameraDevices failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_CameraDeviceID camera_id = 0;

    SDL_Log("Saw %d camera devices.", devcount);
    for (i = 0; i < devcount; i++) {
        const SDL_CameraDeviceID device = devices[i];
        const char *name = SDL_GetCameraDeviceName(device);
        const SDL_CameraPosition position = SDL_GetCameraDevicePosition(device);
        const char *posstr = "";
        if (position == SDL_CAMERA_POSITION_FRONT_FACING) {
            front_camera = device;
            posstr = "[front-facing] ";
        } else if (position == SDL_CAMERA_POSITION_BACK_FACING) {
            back_camera = device;
            posstr = "[back-facing] ";
        }
        if (camera_name && SDL_strcasecmp(name, camera_name) == 0) {
            camera_id = device;
        }
        SDL_Log("  - Camera #%d: %s %s", i, posstr, name);
    }

    if (!camera_id) {
        if (camera_name) {
            SDL_Log("Could not find camera \"%s\"", camera_name);
            return SDL_APP_FAILURE;
        }
        if (front_camera) {
            camera_id = front_camera;
        } else if (devcount > 0) {
            camera_id = devices[0];
        }
    }

    SDL_free(devices);

    if (!camera_id) {
        SDL_Log("No cameras available?");
        return SDL_APP_FAILURE;
    }

    SDL_CameraSpec *pspec = &spec;
    spec.framerate_numerator = 1000;
    spec.framerate_denominator = 1;

    camera = SDL_OpenCameraDevice(camera_id, pspec);
    if (!camera) {
        SDL_Log("Failed to open camera device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_snprintf(window_title, sizeof (window_title), "testcamera: %s (%s)", SDL_GetCameraDeviceName(camera_id), SDL_GetCurrentCameraDriver());
    SDL_SetWindowTitle(window, window_title);

    return SDL_APP_CONTINUE;
}


static int FlipCamera(void)
{
    static Uint64 last_flip = 0;
    if ((SDL_GetTicks() - last_flip) < 3000) {  /* must wait at least 3 seconds between flips. */
        return SDL_APP_CONTINUE;
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
                return SDL_APP_FAILURE;
            }

            last_flip = SDL_GetTicks();
        }
    }

    return SDL_APP_CONTINUE;
}

int SDL_AppEvent(void *appstate, const SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            const SDL_Keycode sym = event->key.key;
            if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) {
                SDL_Log("Key : Escape!");
                return SDL_APP_SUCCESS;
            } else if (sym == SDLK_SPACE) {
                FlipCamera();
                return SDL_APP_CONTINUE;
            }
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            /* !!! FIXME: only flip if clicked in the area of a "flip" icon. */
            return FlipCamera();

        case SDL_EVENT_QUIT:
            SDL_Log("Quit!");
            return 1;

        case SDL_EVENT_CAMERA_DEVICE_APPROVED:
            SDL_Log("Camera approved!");
            break;

        case SDL_EVENT_CAMERA_DEVICE_DENIED:
            SDL_Log("Camera denied!");
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Camera permission denied!", "User denied access to the camera!", window);
            return SDL_APP_FAILURE;
        default:
            break;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

int SDL_AppIterate(void *appstate)
{
    SDL_SetRenderDrawColor(renderer, 0x99, 0x99, 0x99, 255);
    SDL_RenderClear(renderer);

    int win_w, win_h;
    float tw, th;
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

    if (frame_current) {
        if (!texture ||
            SDL_GetTextureSize(texture, &tw, &th) < 0 ||
            (int)tw != frame_current->w || (int)th != frame_current->h) {
            /* Resize the window to match */
            SDL_SetWindowSize(window, frame_current->w, frame_current->h);

            if (texture) {
                SDL_DestroyTexture(texture);
            }

            SDL_Colorspace colorspace = SDL_GetSurfaceColorspace(frame_current);

            /* Create texture with appropriate format */
            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, frame_current->format);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, colorspace);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STREAMING);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame_current->w);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame_current->h);
            texture = SDL_CreateTextureWithProperties(renderer, props);
            SDL_DestroyProperties(props);
            if (!texture) {
                SDL_Log("Couldn't create texture: %s", SDL_GetError());
                return SDL_APP_FAILURE;
            }
        }

        /* Update SDL_Texture with last video frame (only once per new frame) */
        if (frame_current && !texture_updated) {
            SDL_UpdateTexture(texture, NULL, frame_current->pixels, frame_current->pitch);
            texture_updated = SDL_TRUE;
        }

        SDL_GetTextureSize(texture, &tw, &th);
        SDL_GetRenderOutputSize(renderer, &win_w, &win_h);
        d.x = ((win_w - tw) / 2);
        d.y = ((win_h - th) / 2);
        d.w = tw;
        d.h = th;
        SDL_RenderTexture(renderer, texture, NULL, &d);
    }

    /* !!! FIXME: Render a "flip" icon if front_camera and back_camera are both != 0. */

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate)
{
    SDL_ReleaseCameraFrame(camera, frame_current);
    SDL_CloseCamera(camera);
    SDL_DestroyTexture(texture);
    SDLTest_CommonQuit(state);
}
