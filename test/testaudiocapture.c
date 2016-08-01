/*
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include "SDL.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define CAPTURE_SECONDS 5

static SDL_AudioSpec spec;
static Uint8 *sound = NULL;     /* Pointer to wave data */
static Uint32 soundlen = 0;     /* Length of wave data */
static Uint32 processed = 0;
static SDL_AudioDeviceID devid = 0;

void SDLCALL
capture_callback(void *arg, Uint8 * stream, int len)
{
    const int avail = (int) (soundlen - processed);
    if (len > avail) {
        len = avail;
    }

    /*SDL_Log("CAPTURE CALLBACK: %d more bytes\n", len);*/
    SDL_memcpy(sound + processed, stream, len);
    processed += len;
}

void SDLCALL
play_callback(void *arg, Uint8 * stream, int len)
{
    const Uint8 *waveptr = sound + processed;
    const int avail = soundlen - processed;
    int cpy = len;
    if (cpy > avail) {
        cpy = avail;
    }

    /*SDL_Log("PLAY CALLBACK: %d more bytes\n", cpy);*/
    SDL_memcpy(stream, waveptr, cpy);
    processed += cpy;

    len -= cpy;
    if (len > 0) {
        SDL_memset(stream + cpy, spec.silence, len);
    }
}

static void
loop()
{
    SDL_Event e;
    SDL_bool please_quit = SDL_FALSE;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            please_quit = SDL_TRUE;
        }
    }

    if ((!please_quit) && (processed >= soundlen)) {
        processed = 0;
        if (spec.callback == capture_callback) {
            SDL_Log("Done recording, playing back...\n");
            SDL_PauseAudioDevice(devid, 1);
            SDL_CloseAudioDevice(devid);

            spec.callback = play_callback;
            devid = SDL_OpenAudioDevice(NULL, 0, &spec, &spec, 0);
            if (!devid) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback!\n");
                SDL_Quit();
                exit(1);
            }

            SDL_PauseAudioDevice(devid, 0);
        } else {
            SDL_Log("Done playing back.\n");
            please_quit = SDL_TRUE;
        }
    }

    if (please_quit) {
        /* stop playing back, quit. */
        SDL_Log("Shutting down.\n");
        SDL_PauseAudioDevice(devid, 1);
        SDL_CloseAudioDevice(devid);
        SDL_free(sound);
        sound = NULL;
        SDL_Quit();
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        #endif
        exit(0);
    }
}

int
main(int argc, char **argv)
{
    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Load the SDL library */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return (1);
    }

    /* Android apparently needs a window...? */
    #ifdef __ANDROID__  
    SDL_CreateWindow("testaudiocapture", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 240, 0);
    #endif

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    SDL_zero(spec);
    spec.freq = 44100;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 1024;
    spec.callback = capture_callback;

    soundlen = spec.freq * (SDL_AUDIO_BITSIZE(spec.format) / 8) * spec.channels * CAPTURE_SECONDS;
    sound = (Uint8 *) SDL_malloc(soundlen);
    if (!sound) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory\n");
        SDL_Quit();
        return 1;
    }

    devid = SDL_OpenAudioDevice(NULL, 1, &spec, &spec, 0);
    if (!devid) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for capture: %s!\n", SDL_GetError());
        SDL_free(sound);
        SDL_Quit();
        exit(1);
    }

    SDL_Log("Recording for %d seconds...\n", CAPTURE_SECONDS);
    SDL_PauseAudioDevice(devid, 0);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (1) { loop(); SDL_Delay(16); }
#endif

    return 0;
}

