#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static SDL_AudioStream *stream = NULL;
static Uint8 *wav_data = NULL;
static Uint32 wav_data_len = 0;
static SDL_AudioSpec dev_spec;
static Uint64 next_reopen = 0;
static int next_spec = 0;

static const SDL_AudioSpec specs[] = {
    /* mono */
    { SDL_AUDIO_U8, 1,  8000 }, { SDL_AUDIO_S8, 1,  8000 }, { SDL_AUDIO_S16, 1,  8000 }, { SDL_AUDIO_S32, 1,  8000 }, { SDL_AUDIO_F32, 1,  8000 }, { SDL_AUDIO_F32, 1,  8000 },
    { SDL_AUDIO_U8, 1, 11025 }, { SDL_AUDIO_S8, 1, 11025 }, { SDL_AUDIO_S16, 1, 11025 }, { SDL_AUDIO_S32, 1, 11025 }, { SDL_AUDIO_F32, 1, 11025 }, { SDL_AUDIO_F32, 1, 11025 },
    { SDL_AUDIO_U8, 1, 22050 }, { SDL_AUDIO_S8, 1, 22050 }, { SDL_AUDIO_S16, 1, 22050 }, { SDL_AUDIO_S32, 1, 22050 }, { SDL_AUDIO_F32, 1, 22050 }, { SDL_AUDIO_F32, 1, 22050 },
    { SDL_AUDIO_U8, 1, 44100 }, { SDL_AUDIO_S8, 1, 44100 }, { SDL_AUDIO_S16, 1, 44100 }, { SDL_AUDIO_S32, 1, 44100 }, { SDL_AUDIO_F32, 1, 44100 }, { SDL_AUDIO_F32, 1, 44100 },
    { SDL_AUDIO_U8, 1, 48000 }, { SDL_AUDIO_S8, 1, 48000 }, { SDL_AUDIO_S16, 1, 48000 }, { SDL_AUDIO_S32, 1, 48000 }, { SDL_AUDIO_F32, 1, 48000 }, { SDL_AUDIO_F32, 1, 48000 },
    { SDL_AUDIO_U8, 1, 96000 }, { SDL_AUDIO_S8, 1, 96000 }, { SDL_AUDIO_S16, 1, 96000 }, { SDL_AUDIO_S32, 1, 96000 }, { SDL_AUDIO_F32, 1, 96000 }, { SDL_AUDIO_F32, 1, 96000 },
    /* stereo */
    { SDL_AUDIO_U8, 2,  8000 }, { SDL_AUDIO_S8, 2,  8000 }, { SDL_AUDIO_S16, 2,  8000 }, { SDL_AUDIO_S32, 2,  8000 }, { SDL_AUDIO_F32, 2,  8000 }, { SDL_AUDIO_F32, 2,  8000 },
    { SDL_AUDIO_U8, 2, 11025 }, { SDL_AUDIO_S8, 2, 11025 }, { SDL_AUDIO_S16, 2, 11025 }, { SDL_AUDIO_S32, 2, 11025 }, { SDL_AUDIO_F32, 2, 11025 }, { SDL_AUDIO_F32, 2, 11025 },
    { SDL_AUDIO_U8, 2, 22050 }, { SDL_AUDIO_S8, 2, 22050 }, { SDL_AUDIO_S16, 2, 22050 }, { SDL_AUDIO_S32, 2, 22050 }, { SDL_AUDIO_F32, 2, 22050 }, { SDL_AUDIO_F32, 2, 22050 },
    { SDL_AUDIO_U8, 2, 44100 }, { SDL_AUDIO_S8, 2, 44100 }, { SDL_AUDIO_S16, 2, 44100 }, { SDL_AUDIO_S32, 2, 44100 }, { SDL_AUDIO_F32, 2, 44100 }, { SDL_AUDIO_F32, 2, 44100 },
    { SDL_AUDIO_U8, 2, 48000 }, { SDL_AUDIO_S8, 2, 48000 }, { SDL_AUDIO_S16, 2, 48000 }, { SDL_AUDIO_S32, 2, 48000 }, { SDL_AUDIO_F32, 2, 48000 }, { SDL_AUDIO_F32, 2, 48000 },
    { SDL_AUDIO_U8, 2, 96000 }, { SDL_AUDIO_S8, 2, 96000 }, { SDL_AUDIO_S16, 2, 96000 }, { SDL_AUDIO_S32, 2, 96000 }, { SDL_AUDIO_F32, 2, 96000 }, { SDL_AUDIO_F32, 2, 96000 },
};

static const char *SpecToString(const SDL_AudioSpec *spec)
{
    static char buf[256];
    SDL_snprintf(buf, sizeof (buf), "%s, %d channel%s, %dHz", SDL_GetAudioFormatName(spec->format), spec->channels, (spec->channels == 1) ? "" : "s", spec->freq);
    return buf;
}

static SDL_AudioDeviceID open_device(void)
{
    SDL_AudioDeviceID devid = 0;

    while (next_spec < SDL_arraysize(specs)) {
        const SDL_AudioSpec *spec = &specs[next_spec++];

        if ((SDL_AUDIO_BITSIZE(spec->format) <= SDL_AUDIO_BITSIZE(dev_spec.format)) &&
            (spec->channels <= dev_spec.channels) &&
            (spec->freq <= dev_spec.freq)) {
            continue;  /* don't bother, it won't trigger a reopen. */
        }

        SDL_Log("Requesting device open at %s", SpecToString(spec));
        devid = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, spec);
        if (!devid) {
            SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        } else {
            SDL_GetAudioDeviceFormat(devid, &dev_spec, NULL);
            SDL_Log("Device actually opened at %s", SpecToString(&dev_spec));
        }

        next_reopen = SDL_GetTicks() + 2000;
        break;
    }

    return devid;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_AudioSpec wav_spec;
    char *wav_path = NULL;
    SDL_AudioDeviceID devid = 0;

    /* this doesn't have to run very much, so give up tons of CPU time between iterations. */
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "5");

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Using audio driver: %s", SDL_GetCurrentAudioDriver());

    SDL_asprintf(&wav_path, "%ssample.wav", SDL_GetBasePath());  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &wav_spec, &wav_data, &wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_free(wav_path);  /* done with this string. */

    devid = open_device();
    if (!devid) {
        return SDL_APP_FAILURE;
    }

    /* Create our audio stream in the same format as the .wav file. It'll convert to what the audio hardware wants. */
    stream = SDL_CreateAudioStream(&wav_spec, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_BindAudioStream(devid, stream)) {
        SDL_Log("Couldn't bind audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    /* see if we need to feed the audio stream more data yet.
       We're being lazy here, but if there's less than the entire wav file left to play,
       just shove a whole copy of it into the queue, so we always have _tons_ of
       data queued for playback. */
    if (SDL_GetAudioStreamQueued(stream) < (int)wav_data_len) {
        /* feed more data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    }

    /* we just open a new logical device and don't bind a stream to it, or even keep
       a reference to it. We just want to force SDL to reopen the physical device at
       a higher spec. */
    if (SDL_GetTicks() >= next_reopen) {
        if (next_spec < SDL_arraysize(specs)) {
            open_device();  /* if this fails, that's good data, but keep going. */
        } else {
            SDL_Log("No more reopen attempts, we're done!");
            return SDL_APP_SUCCESS;
        }
    }

    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_free(wav_data);  /* strictly speaking, this isn't necessary because the process is ending, but it's good policy. */
    /* SDL will clean up the stream and devices for us. */
}

