/*
 * This example code creates an simple audio stream for playing sound, and
 * loads a .wav file that is pushed through the stream in a loop.
 *
 * This code is public domain. Feel free to use it for any purpose!
 *
 * The .wav file is a sample from Will Provost's song, The Living Proof,
 * used with permission.
 *
 *    From the album The Living Proof
 *    Publisher: 5 Guys Named Will
 *    Copyright 1996 Will Provost
 *    https://itunes.apple.com/us/album/the-living-proof/id4153978
 *    http://www.amazon.com/The-Living-Proof-Will-Provost/dp/B00004R8RH
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioStream *stream = NULL;
static Uint8 *wav_data = NULL;
static Uint32 wav_data_len = 0;

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_AudioSpec spec;
    char *wav_path = NULL;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* we don't _need_ a window for audio-only things but it's good policy to have one. */
    if (!SDL_CreateWindowAndRenderer("examples/audio/load-wav", 640, 480, 0, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    /* Load the .wav file from wherever the app is being run from. */
    SDL_asprintf(&wav_path, "%ssample.wav", SDL_GetBasePath());  /* allocate a string of the full file path */
    if (!SDL_LoadWAV(wav_path, &spec, &wav_data, &wav_data_len)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't load .wav file!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    SDL_free(wav_path);  /* done with this string. */

    /* Create our audio stream in the same format as the .wav file. It'll convert to what the audio hardware wants. */
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create audio stream!", SDL_GetError(), window);
        return SDL_APP_FAILURE;
    }

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(stream);

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
    if (SDL_GetAudioStreamAvailable(stream) < (int)wav_data_len) {
        /* feed more data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(stream, wav_data, wav_data_len);
    }

    /* we're not doing anything with the renderer, so just blank it out. */
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate)
{
    SDL_free(wav_data);  /* strictly speaking, this isn't necessary because the process is ending, but it's good policy. */
    /* SDL will clean up the window/renderer for us. */
}

