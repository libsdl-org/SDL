/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to load a wave file and loop playing it using SDL audio */

/* loopwaves.c is much more robust in handling WAVE files --
    This is only for simple WAVEs
*/
#include <stdlib.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include "testutils.h"

static struct
{
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
} wave;

static SDL_AudioStream *stream;
static SDLTest_CommonState *state;

static int fillerup(void)
{
    const int minimum = (wave.soundlen / SDL_AUDIO_FRAMESIZE(wave.spec)) / 2;
    if (SDL_GetAudioStreamQueued(stream) < minimum) {
        SDL_PutAudioStreamData(stream, wave.sound, wave.soundlen);
    }
    return 0;
}

int SDL_AppInit(int argc, char *argv[])
{
    int i;
    char *filename = NULL;

    /* this doesn't have to run very much, so give up tons of CPU time between iterations. */
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "5");

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
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    filename = GetResourceFilename(filename, "sample.wav");

    if (!filename) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        return -1;
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &wave.spec, &wave.sound, &wave.soundlen) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        SDL_free(filename);
        return -1;
    }

    SDL_free(filename);

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &wave.spec, NULL, NULL);
    if (!stream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create audio stream: %s\n", SDL_GetError());
        return -1;
    }
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));

    return 0;
}

int SDL_AppEvent(const SDL_Event *event)
{
    return (event->type == SDL_EVENT_QUIT) ? 1 : 0;
}

int SDL_AppIterate(void)
{
    return fillerup();
}

void SDL_AppQuit(void)
{
    SDL_DestroyAudioStream(stream);
    SDL_free(wave.sound);
    SDLTest_CommonDestroyState(state);
}

