/*
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to record audio from the microphone and play it back immediately */

#include "SDL_config.h"

#include <stdio.h>
#include <stdlib.h>

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "SDL.h"

static SDL_AudioDeviceID device;
static SDL_AudioDeviceID capture;

static void SDLCALL
fillerup(void *unused, Uint8 * stream, int len)
{
	SDL_QueueAudio(device, stream, len);
}

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_Quit();
    exit(rc);
}

static void
close_audio()
{
    if (capture != 0) {
        SDL_CloseAudioDevice(capture);
        capture = 0;
    }
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

static void
open_audio()
{
    SDL_AudioSpec spec;

    SDL_zero(spec);
    spec.freq = 44100;
    spec.format = AUDIO_S16;
    spec.channels = 1;
    spec.samples = 1024;
    spec.callback = fillerup;

	capture = SDL_OpenAudioDevice(NULL, SDL_TRUE, &spec, &spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
	if (!capture) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open microphone: %s\n", SDL_GetError());
		quit(2);
	}

	spec.callback = NULL;

	device = SDL_OpenAudioDevice(NULL, SDL_FALSE, &spec, NULL, 0);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        quit(2);
    }

	/* Let the audio run */
	SDL_PauseAudioDevice(capture, SDL_FALSE);
    SDL_PauseAudioDevice(device, SDL_FALSE);
}

static void reopen_audio()
{
    close_audio();
    open_audio();
}


static int done = 0;
void
poked(int sig)
{
    done = 1;
}

#ifdef __EMSCRIPTEN__
void
loop()
{
    if(done || (SDL_GetAudioDeviceStatus(capture) != SDL_AUDIO_PLAYING) || (SDL_GetAudioDeviceStatus(device) != SDL_AUDIO_PLAYING))
        emscripten_cancel_main_loop();
}
#endif

int
main(int argc, char *argv[])
{
    int i;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return (1);
    }

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    open_audio();

    SDL_FlushEvents(SDL_AUDIODEVICEADDED, SDL_AUDIODEVICEREMOVED);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event) > 0) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
            if (event.type == SDL_AUDIODEVICEADDED ||
                (event.type == SDL_AUDIODEVICEREMOVED && event.adevice.iscapture && event.adevice.which == capture) ||
                (event.type == SDL_AUDIODEVICEREMOVED && !event.adevice.iscapture && event.adevice.which == device)) {
                reopen_audio();
            }
        }
        SDL_Delay(100);
    }
#endif

    /* Clean up on signal */
    close_audio();
    SDL_Quit();
    return (0);
}

/* vi: set ts=4 sw=4 expandtab: */
