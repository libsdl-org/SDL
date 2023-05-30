/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include "testutils.h"

static struct
{
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
    Uint32 soundpos;
} wave;

static SDL_AudioDeviceID device;
static SDL_AudioStream *stream;

static void SDLCALL
fillerup(SDL_AudioStream *stream, int len, void *unused)
{
    Uint8 *waveptr;
    int waveleft;

    /*SDL_Log("CALLBACK WANTS %d MORE BYTES!", len);*/

    /* Set up the pointers */
    waveptr = wave.sound + wave.soundpos;
    waveleft = wave.soundlen - wave.soundpos;

    /* Go! */
    while (waveleft <= len) {
        SDL_PutAudioStreamData(stream, waveptr, waveleft);
        len -= waveleft;
        waveptr = wave.sound;
        waveleft = wave.soundlen;
        wave.soundpos = 0;
    }
    SDL_PutAudioStreamData(stream, waveptr, len);
    wave.soundpos += len;
}

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static void
close_audio(void)
{
    if (device != 0) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

static void
open_audio(void)
{
    SDL_AudioDeviceID *devices = SDL_GetAudioOutputDevices(NULL);
    device = devices ? SDL_OpenAudioDevice(devices[0], &wave.spec) : 0;
    SDL_free(devices);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_free(wave.sound);
        quit(2);
    }
    stream = SDL_CreateAndBindAudioStream(device, &wave.spec);
    if (!stream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create audio stream: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(device);
        SDL_free(wave.sound);
        quit(2);
    }

    SDL_SetAudioStreamGetCallback(stream, fillerup, NULL);
}

#ifndef __EMSCRIPTEN__
static void reopen_audio(void)
{
    close_audio();
    open_audio();
}
#endif


static int done = 0;



#ifdef __EMSCRIPTEN__
static void loop(void)
{
    if (done || (SDL_GetAudioDeviceStatus(device) != SDL_AUDIO_PLAYING)) {
        emscripten_cancel_main_loop();
    } else {
        fillerup();
    }
}
#endif

int main(int argc, char *argv[])
{
    int i;
    char *filename = NULL;
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
        return 1;
    }

    filename = GetResourceFilename(filename, "sample.wav");

    if (filename == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        quit(1);
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &wave.spec, &wave.sound, &wave.soundlen) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        quit(1);
    }

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    open_audio();

    SDL_FlushEvents(SDL_EVENT_AUDIO_DEVICE_ADDED, SDL_EVENT_AUDIO_DEVICE_REMOVED);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event) > 0) {
            if (event.type == SDL_EVENT_QUIT) {
                done = 1;
            }
            if ((event.type == SDL_EVENT_AUDIO_DEVICE_ADDED && !event.adevice.iscapture) ||
                (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED && !event.adevice.iscapture && event.adevice.which == device)) {
                reopen_audio();
            }
        }

        SDL_Delay(100);
    }
#endif

    /* Clean up on signal */
    close_audio();
    SDL_free(wave.sound);
    SDL_free(filename);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
