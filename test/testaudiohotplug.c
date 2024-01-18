/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to test hotplugging of audio devices */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include "testutils.h"

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

static SDL_AudioSpec spec;
static Uint8 *sound = NULL; /* Pointer to wave data */
static Uint32 soundlen = 0; /* Length of wave data */

static SDLTest_CommonState *state;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static int done = 0;

static void poked(int sig)
{
    done = 1;
}

static const char *devtypestr(int iscapture)
{
    return iscapture ? "capture" : "output";
}

static void iteration(void)
{
    SDL_Event e;
    SDL_AudioDeviceID dev;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            done = 1;
        } else if (e.type == SDL_EVENT_KEY_UP) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                done = 1;
            }
        } else if (e.type == SDL_EVENT_AUDIO_DEVICE_ADDED) {
            const SDL_AudioDeviceID which = (SDL_AudioDeviceID) e.adevice.which;
            const SDL_bool iscapture = e.adevice.iscapture ? SDL_TRUE : SDL_FALSE;
            char *name = SDL_GetAudioDeviceName(which);
            if (name) {
                SDL_Log("New %s audio device at id %u: %s", devtypestr(iscapture), (unsigned int)which, name);
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Got new %s device, id %u, but failed to get the name: %s",
                             devtypestr(iscapture), (unsigned int)which, SDL_GetError());
                continue;
            }
            if (!iscapture) {
                SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(which, &spec, NULL, NULL);
                if (!stream) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create/bind an audio stream to %u ('%s'): %s", (unsigned int) which, name, SDL_GetError());
                } else {
                    SDL_Log("Opened '%s' as %u\n", name, (unsigned int) which);
                    /* !!! FIXME: laziness, this used to loop the audio, but we'll just play it once for now on each connect. */
                    SDL_PutAudioStreamData(stream, sound, soundlen);
                    SDL_FlushAudioStream(stream);
                    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
                    /* !!! FIXME: this is leaking the stream for now. We'll wire it up to a dictionary or whatever later. */
                }
            }
            SDL_free(name);
        } else if (e.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {
            dev = (SDL_AudioDeviceID)e.adevice.which;
            SDL_Log("%s device %u removed.\n", devtypestr(e.adevice.iscapture), (unsigned int)dev);
            /* !!! FIXME: we need to keep track of our streams and destroy them here. */
        }
    }
}

#ifdef SDL_PLATFORM_EMSCRIPTEN
static void loop(void)
{
    if (done)
        emscripten_cancel_main_loop();
    else
        iteration();
}
#endif

int main(int argc, char *argv[])
{
    int i;
    char *filename = NULL;
    SDL_Window *window;

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
            exit(1);
        }

        i += consumed;
    }

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Some targets (Mac CoreAudio) need an event queue for audio hotplug, so make and immediately hide a window. */
    window = SDL_CreateWindow("testaudiohotplug", 640, 480, 0);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        quit(1);
    }
    SDL_MinimizeWindow(window);

    filename = GetResourceFilename(filename, "sample.wav");

    if (!filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        quit(1);
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &spec, &sound, &soundlen) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        quit(1);
    }

#ifdef HAVE_SIGNAL_H
    /* Set the signals */
#ifdef SIGHUP
    (void)signal(SIGHUP, poked);
#endif
    (void)signal(SIGINT, poked);
#ifdef SIGQUIT
    (void)signal(SIGQUIT, poked);
#endif
    (void)signal(SIGTERM, poked);
#endif /* HAVE_SIGNAL_H */

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Select a driver with the SDL_AUDIO_DRIVER environment variable.\n");
    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        SDL_Delay(100);
        iteration();
    }
#endif

    /* Clean up on signal */
    /* Quit audio first, then free WAV. This prevents access violations in the audio threads. */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_free(sound);
    SDL_free(filename);
    quit(0);
    return 0;
}
