/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#include "testutils.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdio.h> /* for fflush() and stdout */

static SDL_AudioSpec spec;
static Uint8 *sound = NULL; /* Pointer to wave data */
static Uint32 soundlen = 0; /* Length of wave data */

/* these have to be in globals so the Emscripten port can see them in the mainloop.  :/  */
static SDL_AudioStream *stream = NULL;


#ifdef SDL_PLATFORM_EMSCRIPTEN
static void loop(void)
{
    if (SDL_GetAudioStreamAvailable(stream) == 0) {
        SDL_Log("done.");
        SDL_DestroyAudioStream(stream);
        SDL_free(sound);
        SDL_Quit();
        emscripten_cancel_main_loop();
    }
}
#endif

static void
test_multi_audio(SDL_AudioDeviceID *devices, int devcount)
{
    int keep_going = 1;
    SDL_AudioStream **streams = NULL;
    int i;

#ifdef SDL_PLATFORM_ANDROID  /* !!! FIXME: maybe always create a window, in the SDLTest layer, so these #ifdefs don't have to be here? */
    SDL_Event event;

    /* Create a Window to get fully initialized event processing for testing pause on Android. */
    SDL_CreateWindow("testmultiaudio", 320, 240, 0);
#endif

    for (i = 0; i < devcount; i++) {
        char *devname = SDL_GetAudioDeviceName(devices[i]);

        SDL_Log("Playing on device #%d of %d: id=%u, name='%s'...", i, devcount, (unsigned int) devices[i], devname);

        if ((stream = SDL_OpenAudioDeviceStream(devices[i], &spec, NULL, NULL)) == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio stream creation failed: %s", SDL_GetError());
        } else {
            SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
            SDL_PutAudioStreamData(stream, sound, soundlen);
            SDL_FlushAudioStream(stream);
#ifdef SDL_PLATFORM_EMSCRIPTEN
            emscripten_set_main_loop(loop, 0, 1);
#else
            while (SDL_GetAudioStreamAvailable(stream) > 0) {
#ifdef SDL_PLATFORM_ANDROID
                /* Empty queue, some application events would prevent pause. */
                while (SDL_PollEvent(&event)) {
                }
#endif
                SDL_Delay(100);
            }
#endif
            SDL_Log("done.");
            SDL_DestroyAudioStream(stream);
        }
        SDL_free(devname);
        stream = NULL;
    }

    /* note that Emscripten currently doesn't run this part (but maybe only has a single audio device anyhow?) */
    SDL_Log("Playing on all devices...\n");
    streams = (SDL_AudioStream **) SDL_calloc(devcount, sizeof (SDL_AudioStream *));
    if (!streams) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
    } else {
        for (i = 0; i < devcount; i++) {
            streams[i] = SDL_OpenAudioDeviceStream(devices[i], &spec, NULL, NULL);
            if (streams[i] == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio stream creation failed for device %d of %d: %s", i, devcount, SDL_GetError());
            } else {
                SDL_PutAudioStreamData(streams[i], sound, soundlen);
                SDL_FlushAudioStream(streams[i]);
            }
        }

        /* try to start all the devices about the same time. SDL does not guarantee sync across physical devices. */
        for (i = 0; i < devcount; i++) {
            if (streams[i]) {
                SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(streams[i]));
            }
        }

        while (keep_going) {
            keep_going = 0;
            for (i = 0; i < devcount; i++) {
                if (streams[i] && (SDL_GetAudioStreamAvailable(streams[i]) > 0)) {
                    keep_going = 1;
                }
            }
#ifdef SDL_PLATFORM_ANDROID
            /* Empty queue, some application events would prevent pause. */
            while (SDL_PollEvent(&event)) {}
#endif

            SDL_Delay(100);
        }

        for (i = 0; i < devcount; i++) {
            SDL_DestroyAudioStream(streams[i]);
        }

        SDL_free(streams);
    }

    SDL_Log("All done!\n");
}

int main(int argc, char **argv)
{
    SDL_AudioDeviceID *devices = NULL;
    int devcount = 0;
    int i;
    char *filename = NULL;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!filename) {
                filename = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[sample.wav]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    filename = GetResourceFilename(filename, "sample.wav");

    devices = SDL_GetAudioOutputDevices(&devcount);
    if (!devices) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Don't see any specific audio devices!");
    } else {
        /* Load the wave file into memory */
        if (SDL_LoadWAV(filename, &spec, &sound, &soundlen) == -1) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename,
                         SDL_GetError());
        } else {
            test_multi_audio(devices, devcount);
            SDL_free(sound);
        }
    }

    SDL_free(devices);
    SDL_free(filename);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);

    return 0;
}

