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
static SDL_AudioStream *stream_in = NULL;
static SDL_AudioStream *stream_out = NULL;
static int done = 0;

static void loop(void)
{
    const SDL_AudioDeviceID devid_in = SDL_GetAudioStreamBinding(stream_in);
    const SDL_AudioDeviceID devid_out = SDL_GetAudioStreamBinding(stream_out);
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
                SDL_ResumeAudioDevice(devid_in);
            }
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (e.button.button == 1) {
                SDL_PauseAudioDevice(devid_in);
                SDL_FlushAudioStream(stream_in);  /* so no samples are held back for resampling purposes. */
                SDL_ResumeAudioDevice(devid_out);
            }
        }
    }

    if (!SDL_IsAudioDevicePaused(devid_in)) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    /* Feed any new data we captured to the output stream. It'll play when we unpause the device. */
    while (!please_quit && (SDL_GetAudioStreamAvailable(stream_in) > 0)) {
        Uint8 buf[1024];
        const int br = SDL_GetAudioStreamData(stream_in, buf, sizeof(buf));
        if (br < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read from input audio stream: %s\n", SDL_GetError());
            please_quit = 1;
        } else if (SDL_PutAudioStreamData(stream_out, buf, br) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to write to output audio stream: %s\n", SDL_GetError());
            please_quit = 1;
        }
    }

    if (please_quit) {
        /* stop playing back, quit. */
        SDL_Log("Shutting down.\n");
        SDL_CloseAudioDevice(devid_in);
        SDL_CloseAudioDevice(devid_out);
        SDL_DestroyAudioStream(stream_in);
        SDL_DestroyAudioStream(stream_out);
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
}

int main(int argc, char **argv)
{
    SDL_AudioDeviceID *devices;
    SDLTest_CommonState *state;
    SDL_AudioSpec outspec;
    SDL_AudioSpec inspec;
    SDL_AudioDeviceID device;
    SDL_AudioDeviceID want_device = SDL_AUDIO_DEVICE_DEFAULT_CAPTURE;
    const char *devname = NULL;
    int i;

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
            static const char *options[] = { "[device_name]", NULL };
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

    devices = SDL_GetAudioCaptureDevices(NULL);
    for (i = 0; devices[i] != 0; i++) {
        char *name = SDL_GetAudioDeviceName(devices[i]);
        SDL_Log(" Capture device #%d: '%s'\n", i, name);
        if (devname && (SDL_strcmp(devname, name) == 0)) {
            want_device = devices[i];
        }
        SDL_free(name);
    }

    if (devname && (want_device == SDL_AUDIO_DEVICE_DEFAULT_CAPTURE)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Didn't see a capture device named '%s', using the system default instead.\n", devname);
        devname = NULL;
    }

    /* DirectSound can fail in some instances if you open the same hardware
       for both capture and output and didn't open the output end first,
       according to the docs, so if you're doing something like this, always
       open your capture devices second in case you land in those bizarre
       circumstances. */

    SDL_Log("Opening default playback device...\n");
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, NULL);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
    SDL_PauseAudioDevice(device);
    SDL_GetAudioDeviceFormat(device, &outspec);
    stream_out = SDL_CreateAndBindAudioStream(device, &outspec);
    if (!stream_out) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create an audio stream for playback: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_Log("Opening capture device %s%s%s...\n",
            devname ? "'" : "",
            devname ? devname : "[[default]]",
            devname ? "'" : "");

    device = SDL_OpenAudioDevice(want_device, NULL);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for capture: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
    SDL_PauseAudioDevice(device);
    SDL_GetAudioDeviceFormat(device, &inspec);
    stream_in = SDL_CreateAndBindAudioStream(device, &inspec);
    if (!stream_in) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create an audio stream for capture: %s!\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    SDL_SetAudioStreamFormat(stream_in, NULL, &outspec);  /* make sure we output at the playback format. */

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

    SDLTest_CommonDestroyState(state);

    return 0;
}
