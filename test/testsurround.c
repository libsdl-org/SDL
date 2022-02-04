/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Program to test surround sound audio channels */
#include "SDL_config.h"

#include "SDL.h"

static int total_channels;
static int active_channel;

#define SAMPLE_RATE_HZ 48000
#define CHANNEL_TEST_TIME_SEC 5
#define MAX_AMPLITUDE SDL_MAX_SINT16

#define SINE_FREQ_HZ 500
#define LFE_SINE_FREQ_HZ 50

/* The channel layout is defined in SDL_audio.h */
const char*
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
        case 7:
            return "Back Center";
        case 6:
        case 8:
            return "Back Left";
        }
    case 5:
        switch (channel_count) {
        case 7:
            return "Back Left";
        case 6:
        case 8:
            return "Back Right";
        }
    case 6:
        switch (channel_count) {
        case 7:
            return "Back Right";
        case 8:
            return "Side Left";
        }
    case 7:
        return "Side Right";
    }

    return NULL;
}

SDL_bool
is_lfe_channel(int channel_index, int channel_count)
{
    return (channel_count == 3 && channel_index == 2) || (channel_count >= 6 && channel_index == 3);
}

void SDLCALL
fill_buffer(void* unused, Uint8* stream, int len)
{
    Sint16* buffer = (Sint16*)stream;
    int samples = len / sizeof(Sint16);
    static int total_samples = 0;
    int i;

    SDL_memset(stream, 0, len);

    /* This can happen for a short time when switching devices */
    if (active_channel == total_channels) {
        return;
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
}

int
main(int argc, char *argv[])
{
    int i;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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

    for (i = 0; i < SDL_GetNumAudioDevices(0); i++) {
        const char *devname = SDL_GetAudioDeviceName(i, 0);
        int j;
        SDL_AudioSpec spec;
        SDL_AudioDeviceID dev;

        if (SDL_GetAudioDeviceSpec(i, 0, &spec) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetAudioSpec() failed: %s\n", SDL_GetError());
            continue;
        }

        spec.freq = SAMPLE_RATE_HZ;
        spec.format = AUDIO_S16SYS;
        spec.samples = 4096;
        spec.callback = fill_buffer;

        dev = SDL_OpenAudioDevice(devname, 0, &spec, NULL, 0);
        if (dev == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenAudioDevice() failed: %s\n", SDL_GetError());
            continue;
        }

        SDL_Log("Testing audio device: %s (%d channels)\n", devname, spec.channels);

        /* These are used by the fill_buffer callback */
        total_channels = spec.channels;
        active_channel = 0;

        SDL_PauseAudioDevice(dev, 0);

        for (j = 0; j < total_channels; j++) {
            int sine_freq = is_lfe_channel(j, total_channels) ? LFE_SINE_FREQ_HZ : SINE_FREQ_HZ;

            SDL_Log("Playing %d Hz test tone on channel: %s\n", sine_freq, get_channel_name(j, total_channels));

            /* fill_buffer() will increment the active channel */
            SDL_Delay(CHANNEL_TEST_TIME_SEC * 1000);
        }

        SDL_CloseAudioDevice(dev);
    }

    SDL_Quit();
    return 0;
}

