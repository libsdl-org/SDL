/*
 * This example code loads two .wav files, puts them in audio streams and
 * binds them for playback, repeating both sounds on loop. This shows several
 * streams mixing into a single playback device.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioDeviceID audio_device = 0;

/* things that are playing sound (the audiostream itself, plus the original data, so we can refill to loop. */
typedef struct Sound {
    Uint8 *wav_data;
    Uint32 wav_data_len;
    SDL_AudioStream *stream;
} Sound;

static Sound sounds[2];

static bool init_sound(const char *fname, Sound *sound)
{
    bool retval = false;
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    /* Load the .wav files from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%s%s", SDL_GetBasePath(), fname);  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &sound->wav_data, &sound->wav_data_len)) {
        SDL_Log("Couldn't load .wav file: %s", SDL_GetError());
        return false;
    }

    /* Create an audio stream. Set the source format to the wav's format (what
       we'll input), leave the dest format NULL here (it'll change to what the
       device wants once we bind it). */
    sound->stream = SDL_CreateAudioStream(&spec, NULL);
    if (!sound->stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(audio_device, sound->stream)) {  /* once bound, it'll start playing when there is data available! */
        SDL_Log("Failed to bind '%s' stream to device: %s", fname, SDL_GetError());
    } else {
        retval = true;  /* success! */
    }

    SDL_free(wav_path);  /* done with this string. */
    return retval;
}


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{

    SDL_SetAppMetadata("Example Audio Multiple Streams", "1.0", "com.example.audio-multiple-streams");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/audio/multiple-streams", 640, 480, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* open the default audio device in whatever format it prefers; our audio streams will adjust to it. */
    audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (audio_device == 0) {
        SDL_Log("Couldn't open audio device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!init_sound("sample.wav", &sounds[0])) {
        return SDL_APP_FAILURE;
    } else if (!init_sound("sword.wav", &sounds[1])) {
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
    int i;

    for (i = 0; i < SDL_arraysize(sounds); i++) {
        /* If less than a full copy of the audio is queued for playback, put another copy in there.
           This is overkill, but easy when lots of RAM is cheap. One could be more careful and
           queue less at a time, as long as the stream doesn't run dry.  */
        if (SDL_GetAudioStreamQueued(sounds[i].stream) < ((int) sounds[i].wav_data_len)) {
            SDL_PutAudioStreamData(sounds[i].stream, sounds[i].wav_data, (int) sounds[i].wav_data_len);
        }
    }

    /* just blank the screen. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    int i;

    SDL_CloseAudioDevice(audio_device);

    for (i = 0; i < SDL_arraysize(sounds); i++) {
        if (sounds[i].stream) {
            SDL_DestroyAudioStream(sounds[i].stream);
        }
        SDL_free(sounds[i].wav_data);
    }

    /* SDL will clean up the window/renderer for us. */
}
