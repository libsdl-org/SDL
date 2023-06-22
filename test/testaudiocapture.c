/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioSpec spec;
static SDL_AudioDeviceID devid_in = 0;
static SDL_AudioDeviceID devid_out = 0;
static int done = 0;

static void loop(void)
{
    SDL_bool please_quit = SDL_FALSE;
    SDL_Event e;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            please_quit = SDL_TRUE;
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                please_quit = SDL_TRUE;
            }
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (e.button.button == 1) {
                SDL_PauseAudioDevice(devid_out);
                SDL_PlayAudioDevice(devid_in);
            }
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (e.button.button == 1) {
                SDL_PauseAudioDevice(devid_in);
                SDL_PlayAudioDevice(devid_out);
            }
        }
    }

    if (SDL_GetAudioDeviceStatus(devid_in) == SDL_AUDIO_PLAYING) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    if (please_quit) {
        /* stop playing back, quit. */
        SDL_Log("Shutting down.\n");
        SDL_PauseAudioDevice(devid_in);
        SDL_CloseAudioDevice(devid_in);
        SDL_PauseAudioDevice(devid_out);
        SDL_CloseAudioDevice(devid_out);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        /* Let 'main()' return normally */
        done = 1;
        return;
    }

    /* Note that it would be easier to just have a one-line function that
        calls SDL_QueueAudio() as a capture device callback, but we're
        trying to test the API, so we use SDL_DequeueAudio() here. */
    while (SDL_TRUE) {
        Uint8 buf[1024];
        const Uint32 br = SDL_DequeueAudio(devid_in, buf, sizeof(buf));
        SDL_QueueAudio(devid_out, buf, br);
        if (br < sizeof(buf)) {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    /* (NULL means "open default device.") */
    const char *devname = NULL;
    SDL_AudioSpec wanted;
    int devcount;
    int i;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!devname) {
                devname = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[driver_name]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_CreateWindowAndRenderer(320, 240, 0, &window, &renderer) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create SDL window and renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    devcount = SDL_GetNumAudioDevices(SDL_TRUE);
    for (i = 0; i < devcount; i++) {
        SDL_Log(" Capture device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
    }

    SDL_zero(wanted);
    wanted.freq = 44100;
    wanted.format = SDL_AUDIO_F32SYS;
    wanted.channels = 1;
    wanted.samples = 4096;
    wanted.callback = NULL;

    SDL_zero(spec);

    /* DirectSound can fail in some instances if you open the same hardware
       for both capture and output and didn't open the output end first,
       according to the docs, so if you're doing something like this, always
       open your capture devices second in case you land in those bizarre
       circumstances. */

    SDL_Log("Opening default playback device...\n");
    devid_out = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wanted, &spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!devid_out) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_Log("Opening capture device %s%s%s...\n",
            devname ? "'" : "",
            devname ? devname : "[[default]]",
            devname ? "'" : "");

    devid_in = SDL_OpenAudioDevice(devname, SDL_TRUE, &spec, &spec, 0);
    if (!devid_in) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for capture: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_Log("Ready! Hold down mouse or finger to record!\n");

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
        if (!done) {
            SDL_Delay(16);
        }
    }
#endif

    /* SDL_DestroyRenderer(renderer); */
    /* SDL_DestroyWindow(window); */

    /* SDL_Quit(); */
    /* SDLTest_CommonDestroyState(state); */

    return 0;
}
