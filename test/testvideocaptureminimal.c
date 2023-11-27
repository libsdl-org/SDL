/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL3/SDL_video_capture.h"
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event evt;
    int quit = 0;
    SDLTest_CommonState  *state = NULL;

    SDL_VideoCaptureDevice *device = NULL;
    SDL_VideoCaptureSpec obtained;

    SDL_VideoCaptureFrame frame_current;
    SDL_Texture *texture = NULL;
    int texture_updated = 0;

    SDL_zero(evt);
    SDL_zero(obtained);
    SDL_zero(frame_current);

    /* Set 0 to disable TouchEvent to be duplicated as MouseEvent with SDL_TOUCH_MOUSEID */
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    /* Set 0 to disable MouseEvent to be duplicated as TouchEvent with SDL_MOUSE_TOUCHID */
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) { /* FIXME: SDL_INIT_JOYSTICK needed for add/removing devices at runtime */
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

    device = SDL_OpenVideoCaptureWithSpec(0, NULL, &obtained, SDL_VIDEO_CAPTURE_ALLOW_ANY_CHANGE);
    if (!device) {
        SDL_Log("No video capture? %s", SDL_GetError());
        return 1;
    }

    if (SDL_StartVideoCapture(device) < 0) {
        SDL_Log("error SDL_StartVideoCapture(): %s", SDL_GetError());
        return 1;
    }

    /* Create texture with appropriate format */
    if (texture == NULL) {
        texture = SDL_CreateTexture(renderer, obtained.format, SDL_TEXTUREACCESS_STATIC, obtained.width, obtained.height);
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
            SDL_VideoCaptureFrame frame_next;
            SDL_zero(frame_next);

            if (SDL_AcquireVideoCaptureFrame(device, &frame_next) < 0) {
                SDL_Log("err SDL_AcquireVideoCaptureFrame: %s", SDL_GetError());
            }
#if 0
            if (frame_next.num_planes) {
                SDL_Log("frame: %p  at %" SDL_PRIu64, (void*)frame_next.data[0], frame_next.timestampNS);
            }
#endif

            if (frame_next.num_planes) {
                if (frame_current.num_planes) {
                    if (SDL_ReleaseVideoCaptureFrame(device, &frame_current) < 0) {
                        SDL_Log("err SDL_ReleaseVideoCaptureFrame: %s", SDL_GetError());
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
        if (frame_current.num_planes && texture_updated == 0) {
            /* Use software data */
            if (frame_current.num_planes == 1) {
                SDL_UpdateTexture(texture, NULL,
                        frame_current.data[0], frame_current.pitch[0]);
            } else if (frame_current.num_planes == 2) {
                SDL_UpdateNVTexture(texture, NULL,
                        frame_current.data[0], frame_current.pitch[0],
                        frame_current.data[1], frame_current.pitch[1]);
            } else if (frame_current.num_planes == 3) {
                SDL_UpdateYUVTexture(texture, NULL, frame_current.data[0], frame_current.pitch[0],
                        frame_current.data[1], frame_current.pitch[1],
                        frame_current.data[2], frame_current.pitch[2]);
            }
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
            d.y = (float)(win_h - th);
            d.w = (float)tw;
            d.h = (float)(th - 10);
            SDL_RenderTexture(renderer, texture, NULL, &d);
        }
        SDL_Delay(10);
        SDL_RenderPresent(renderer);
    }

    if (SDL_StopVideoCapture(device) < 0) {
        SDL_Log("error SDL_StopVideoCapture(): %s", SDL_GetError());
    }
    if (frame_current.num_planes) {
        SDL_ReleaseVideoCaptureFrame(device, &frame_current);
    }
    SDL_CloseVideoCapture(device);

    if (texture) {
        SDL_DestroyTexture(texture);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}
