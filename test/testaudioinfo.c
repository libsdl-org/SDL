/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

static void
print_devices(bool recording)
{
    SDL_AudioSpec spec;
    const char *typestr = (recording ? "recording" : "playback");
    int n = 0;
    int frames;
    SDL_AudioDeviceID *devices = recording ? SDL_GetAudioRecordingDevices(&n) : SDL_GetAudioPlaybackDevices(&n);

    if (!devices) {
        SDL_Log("  Driver failed to report %s devices: %s", typestr, SDL_GetError());
        SDL_Log("%s", "");
    } else if (n == 0) {
        SDL_Log("  No %s devices found.", typestr);
        SDL_Log("%s", "");
    } else {
        int i;
        SDL_Log("Found %d %s device%s:", n, typestr, n != 1 ? "s" : "");
        for (i = 0; i < n; i++) {
            const char *name = SDL_GetAudioDeviceName(devices[i]);
            if (name) {
                SDL_Log("  %d: %s", i, name);
            } else {
                SDL_Log("  %d Error: %s", i, SDL_GetError());
            }

            if (SDL_GetAudioDeviceFormat(devices[i], &spec, &frames)) {
                SDL_Log("     Sample Rate: %d", spec.freq);
                SDL_Log("     Channels: %d", spec.channels);
                SDL_Log("     SDL_AudioFormat: %X", spec.format);
                SDL_Log("     Buffer Size: %d frames", frames);
            }
        }
        SDL_Log("%s", "");
    }
    SDL_free(devices);
}

int main(int argc, char **argv)
{
    SDL_AudioSpec spec;
    int i;
    int n;
    int frames;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    /* Load the SDL library */
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    /* Print available audio drivers */
    n = SDL_GetNumAudioDrivers();
    if (n == 0) {
        SDL_Log("No built-in audio drivers");
        SDL_Log("%s", "");
    } else {
        SDL_Log("Built-in audio drivers:");
        for (i = 0; i < n; ++i) {
            SDL_Log("  %d: %s", i, SDL_GetAudioDriver(i));
        }
        SDL_Log("Select a driver with the SDL_AUDIO_DRIVER environment variable.");
    }

    SDL_Log("Using audio driver: %s", SDL_GetCurrentAudioDriver());
    SDL_Log("%s", "");

    print_devices(false);
    print_devices(true);

    if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &frames)) {
        SDL_Log("Default Playback Device:");
        SDL_Log("Sample Rate: %d", spec.freq);
        SDL_Log("Channels: %d", spec.channels);
        SDL_Log("SDL_AudioFormat: %X", spec.format);
        SDL_Log("Buffer Size: %d frames", frames);
    } else {
        SDL_Log("Error when calling SDL_GetAudioDeviceFormat(default playback): %s", SDL_GetError());
    }

    if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, &frames)) {
        SDL_Log("Default Recording Device:");
        SDL_Log("Sample Rate: %d", spec.freq);
        SDL_Log("Channels: %d", spec.channels);
        SDL_Log("SDL_AudioFormat: %X", spec.format);
        SDL_Log("Buffer Size: %d frames", frames);
    } else {
        SDL_Log("Error when calling SDL_GetAudioDeviceFormat(default recording): %s", SDL_GetError());
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}

