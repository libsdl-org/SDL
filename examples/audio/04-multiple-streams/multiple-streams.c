/*
 * This example code loads .wav files dropped onto the app window, puts
 * them in an audio stream and binds them for playback. This shows several
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
static SDL_AudioStream **streams = NULL;
static int num_streams = 0;

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

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

static void load_wav_file(const char *fname)
{
    int idx;
    SDL_AudioSpec spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_data_len = 0;

    /* Find an unused element in the streams array... */
    for (idx = 0; idx < num_streams; idx++) {
        if (streams[idx] == NULL) {
            break;
        }
    }

    /* No space? Grow the array. */
    if (idx == num_streams) {
        void *ptr = SDL_realloc(streams, (num_streams + 1) * sizeof (*streams));
        if (!ptr) {
            SDL_Log("Out of memory!");
            return;  // oh well.
        }
        streams = (SDL_AudioStream **) ptr;
        streams[idx] = NULL;
        num_streams++;
    }

    /* Load the new .wav file */
    if (!SDL_LoadWAV(fname, &spec, &wav_data, &wav_data_len)) {
        SDL_Log("Failed to load '%s': %s", fname, SDL_GetError());
        return;  // oh well.
    }

    /* Create an audio stream. Set the source format to the wav's format (what
       we'll input), leave the dest format NULL here (it'll change to what the
       device wants once we bind it). */
    streams[idx] = SDL_CreateAudioStream(&spec, NULL);
    if (!streams[idx]) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
    } else if (!SDL_BindAudioStream(audio_device, streams[idx])) {  /* once bound, it'll start playing when there is data available! */
        SDL_Log("Failed to bind '%s' stream to device: %s", fname, SDL_GetError());
    } else if (!SDL_PutAudioStreamData(streams[idx], wav_data, (int) wav_data_len)) {
        SDL_Log("Failed to put '%s' data into stream: %s", fname, SDL_GetError());
    } else {
        /* tell SDL we won't be sending more data to this stream, so don't hold back for resampling. */
        SDL_FlushAudioStream(streams[idx]);
    }

    SDL_free(wav_data);
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_DROP_FILE) {
        load_wav_file(event->drop.data);
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    int winw = 640, winh = 480;
    const char *text = "--> Drag and drop .wav files here <--";
    float x, y;
    int i;

    /* see if any streams have finished; destroy them if so. */
    for (i = 0; i < num_streams; i++) {
        if (streams[i] && (SDL_GetAudioStreamAvailable(streams[i]) == 0)) {
            SDL_DestroyAudioStream(streams[i]);
            streams[i] = NULL;
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_GetWindowSize(window, &winw, &winh);
    x = (((float) winw) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
    y = (((float) winh) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, x, y, text);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    int i;

    SDL_CloseAudioDevice(audio_device);

    /* see if any streams have finished; destroy them if so. */
    for (i = 0; i < num_streams; i++) {
        if (streams[i]) {
            SDL_DestroyAudioStream(streams[i]);
        }
    }

    SDL_free(streams);

    /* SDL will clean up the window/renderer for us. */
}
