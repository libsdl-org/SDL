/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to test surround sound audio channels */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static int total_channels;
static int active_channel;

#define SAMPLE_RATE_HZ        48000
#define QUICK_TEST_TIME_MSEC  100
#define CHANNEL_TEST_TIME_SEC 5
#define MAX_AMPLITUDE         SDL_MAX_SINT16

#define SINE_FREQ_HZ     500
#define LFE_SINE_FREQ_HZ 50

/* The channel layout is defined in SDL_audio.h */
static const char *
get_channel_name(int channel_index, int channel_count)
{
    switch (channel_index) {
    case 0:
        return "Front Left";
    case 1:
        return "Front Right";
    case 2:
        switch (channel_count) {
        case 3:
        case 5:
            return "Low Frequency Effects";
        case 4:
            return "Back Left";
        default:
            return "Front Center";
        }
    case 3:
        switch (channel_count) {
        case 4:
            return "Back Right";
        case 5:
            return "Back Left";
        default:
            return "Low Frequency Effects";
        }
    case 4:
        switch (channel_count) {
        case 5:
            return "Back Right";
        case 6:
            return "Side Left";
        case 7:
            return "Back Center";
        case 8:
            return "Back Left";
        }
        SDL_assert(0);
    case 5:
        switch (channel_count) {
        case 6:
            return "Side Right";
        case 7:
            return "Side Left";
        case 8:
            return "Back Right";
        }
        SDL_assert(0);
    case 6:
        switch (channel_count) {
        case 7:
            return "Side Right";
        case 8:
            return "Side Left";
        }
        SDL_assert(0);
    case 7:
        return "Side Right";
    }

    return NULL;
}

static SDL_bool is_lfe_channel(int channel_index, int channel_count)
{
    return (channel_count == 3 && channel_index == 2) || (channel_count >= 6 && channel_index == 3);
}

static void SDLCALL fill_buffer(void *userdata, SDL_AudioStream *stream, int len, int totallen)
{
    const int samples = len / sizeof(Sint16);
    Sint16 *buffer = NULL;
    static int total_samples = 0;
    int i;

    /* This can happen for a short time when switching devices */
    if (active_channel == total_channels) {
        return;
    }

    buffer = (Sint16 *) SDL_calloc(samples, sizeof(Sint16));
    if (!buffer) {
        return;  /* oh well. */
    }

    /* Play a sine wave on the active channel only */
    for (i = active_channel; i < samples; i += total_channels) {
        float time = (float)total_samples++ / SAMPLE_RATE_HZ;
        int sine_freq = is_lfe_channel(active_channel, total_channels) ? LFE_SINE_FREQ_HZ : SINE_FREQ_HZ;
        int amplitude;

        /* Gradually ramp up and down to avoid audible pops when switching between channels */
        if (total_samples < SAMPLE_RATE_HZ) {
            amplitude = total_samples * MAX_AMPLITUDE / SAMPLE_RATE_HZ;
        } else if (total_samples > (CHANNEL_TEST_TIME_SEC - 1) * SAMPLE_RATE_HZ) {
            amplitude = (CHANNEL_TEST_TIME_SEC * SAMPLE_RATE_HZ - total_samples) * MAX_AMPLITUDE / SAMPLE_RATE_HZ;
        } else {
            amplitude = MAX_AMPLITUDE;
        }

        buffer[i] = (Sint16)(SDL_sin(6.283185f * sine_freq * time) * amplitude);

        /* Reset our state for next callback if this channel test is finished */
        if (total_samples == CHANNEL_TEST_TIME_SEC * SAMPLE_RATE_HZ) {
            total_samples = 0;
            active_channel++;
            break;
        }
    }

    SDL_PutAudioStreamData(stream, buffer, samples * sizeof (Sint16));

    SDL_free(buffer);
}

int main(int argc, char *argv[])
{
    SDL_AudioDeviceID *devices = NULL;
    SDLTest_CommonState *state;
    int devcount = 0;
    int i;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    devices = SDL_GetAudioOutputDevices(&devcount);
    if (!devices) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetAudioOutputDevices() failed: %s\n", SDL_GetError());
        devcount = 0;
    }

    SDL_Log("Available audio devices:");
    for (i = 0; i < devcount; i++) {
        SDL_Log("%s", SDL_GetAudioDeviceName(devices[i]));
    }

    for (i = 0; i < devcount; i++) {
        SDL_AudioStream *stream = NULL;
        char *devname = SDL_GetAudioDeviceName(devices[i]);
        int j;
        SDL_AudioSpec spec;

        SDL_Log("Testing audio device: %s\n", devname);
        SDL_free(devname);

        if (SDL_GetAudioDeviceFormat(devices[i], &spec, NULL) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetAudioDeviceFormat() failed: %s\n", SDL_GetError());
            continue;
        }

        SDL_Log("  (%d channels)\n", spec.channels);

        spec.freq = SAMPLE_RATE_HZ;
        spec.format = SDL_AUDIO_S16;

        /* These are used by the fill_buffer callback */
        total_channels = spec.channels;
        active_channel = 0;

        stream = SDL_OpenAudioDeviceStream(devices[i], &spec, fill_buffer, NULL);
        if (!stream) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError());
            continue;
        }
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));

        for (j = 0; j < total_channels; j++) {
            const int sine_freq = is_lfe_channel(j, total_channels) ? LFE_SINE_FREQ_HZ : SINE_FREQ_HZ;

            SDL_Log("Playing %d Hz test tone on channel: %s\n", sine_freq, get_channel_name(j, total_channels));

            /* fill_buffer() will increment the active channel */
            if (SDL_getenv("SDL_TESTS_QUICK") != NULL) {
                SDL_Delay(QUICK_TEST_TIME_MSEC);
            } else {
                SDL_Delay(CHANNEL_TEST_TIME_SEC * 1000);
            }
        }

        SDL_DestroyAudioStream(stream);
    }

    SDL_free(devices);

    SDL_Quit();
    return 0;
}
