/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to load a wave file and loop playing it using SDL sound queueing */

#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include "testutils.h"

static struct
{
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
} wave;

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

static int done = 0;
static void poked(int sig)
{
    done = 1;
}

static SDL_AudioDeviceID g_audio_id = 0;

static void loop(void)
{
#ifdef __EMSCRIPTEN__
    if (done || (SDL_GetAudioDeviceStatus(g_audio_id) != SDL_AUDIO_PLAYING)) {
        emscripten_cancel_main_loop();
    } else
#endif
    {
        /* The device from SDL_OpenAudio() is always device #1. */
        const Uint32 queued = SDL_GetQueuedAudioSize(g_audio_id);
        SDL_Log("Device has %u bytes queued.\n", (unsigned int)queued);
        if (queued <= 8192) { /* time to requeue the whole thing? */
            if (SDL_QueueAudio(g_audio_id, wave.sound, wave.soundlen) == 0) {
                SDL_Log("Device queued %u more bytes.\n", (unsigned int)wave.soundlen);
            } else {
                SDL_Log("Device FAILED to queue %u more bytes: %s\n", (unsigned int)wave.soundlen, SDL_GetError());
            }
        }
    }
}

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
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    filename = GetResourceFilename(filename, "sample.wav");

    if (filename == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        quit(1);
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(filename, &wave.spec, &wave.sound, &wave.soundlen) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", filename, SDL_GetError());
        quit(1);
    }

    wave.spec.callback = NULL; /* we'll push audio. */

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

    /* Initialize fillerup() variables */
    g_audio_id = SDL_OpenAudioDevice(NULL, 0, &wave.spec, NULL, 0);

    if (!g_audio_id) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_free(wave.sound);
        quit(2);
    }

    /*static x[99999]; SDL_QueueAudio(1, x, sizeof (x));*/

    /* Let the audio run */
    SDL_PlayAudioDevice(g_audio_id);

    done = 0;

    /* Note that we stuff the entire audio buffer into the queue in one
       shot. Most apps would want to feed it a little at a time, as it
       plays, but we're going for simplicity here. */

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done && (SDL_GetAudioDeviceStatus(g_audio_id) == SDL_AUDIO_PLAYING)) {
        loop();

        SDL_Delay(100); /* let it play for a while. */
    }
#endif

    /* Clean up on signal */
    SDL_CloseAudioDevice(g_audio_id);
    SDL_free(wave.sound);
    SDL_free(filename);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
