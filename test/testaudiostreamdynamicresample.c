/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* !!! FIXME: this code is not up to standards for SDL3 test apps. Someone should improve this. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static void SDLCALL audio_callback(void *userdata, Uint8 * stream, int len)
{
    SDL_AudioStream *audiostream = (SDL_AudioStream *) userdata;
    SDL_memset(stream, 0, len);
    SDL_GetAudioStreamData(audiostream, stream, len);
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_bool done = SDL_FALSE;
    const SDL_FRect slider_area = { (640 - 500) / 2, (480 - 100) / 2, 500, 100 };
    SDL_FRect slider_fill_area = slider_area;
    int multiplier = 100;
    SDL_AudioSpec spec;
    Uint8 *audio_buf = NULL;
    Uint32 audio_len = 0;
    SDL_AudioStream *stream;
    SDL_AudioDeviceID device;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window = SDL_CreateWindow("Drag the slider: Normal speed", 640, 480, 0);
    renderer = SDL_CreateRenderer(window, NULL, 0);

    SDL_LoadWAV("sample.wav", &spec, &audio_buf, &audio_len);
    stream = SDL_CreateAudioStream(spec.format, spec.channels, spec.freq, spec.format, spec.channels, spec.freq);
    SDL_PutAudioStreamData(stream, audio_buf, audio_len);
    spec.callback = audio_callback;
    spec.userdata = stream;
    device = SDL_OpenAudioDevice(NULL, SDL_FALSE, &spec, NULL, 0);
    SDL_PlayAudioDevice(device);

    slider_fill_area.w /= 2;

    while (!done) {
        SDL_Event e;
        int newmultiplier = multiplier;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                done = 1;
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    done = 1;
                }
            } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                if (e.motion.state & SDL_BUTTON_LMASK) {
                    const SDL_FPoint p = { e.motion.x, e.motion.y };
                    if (SDL_PointInRectFloat(&p, &slider_area)) {
                        const float w = SDL_roundf(p.x - slider_area.x);
                        slider_fill_area.w = w;
                        newmultiplier = ((int) ((w / slider_area.w) * 800.0f)) - 400;
                    }
                }
            }
        }

        if (multiplier != newmultiplier) {
            char title[64];
            int newfreq = spec.freq;

            multiplier = newmultiplier;
            if (multiplier == 0) {
                SDL_snprintf(title, sizeof (title), "Drag the slider: Normal speed");
            } else if (multiplier < 0) {
                SDL_snprintf(title, sizeof (title), "Drag the slider: %.2fx slow", (-multiplier / 100.0f) + 1.0f);
            } else {
                SDL_snprintf(title, sizeof (title), "Drag the slider: %.2fx fast", (multiplier / 100.0f) + 1.0f);
            }
            SDL_SetWindowTitle(window, title);

            /* this math sucks, but whatever. */
            if (multiplier < 0) {
                newfreq = spec.freq + (int) ((spec.freq * (multiplier / 400.0f)) * 0.75f);
            } else if (multiplier > 0) {
                newfreq = spec.freq + (int) (spec.freq * (multiplier / 100.0f));
            }
            /* SDL_Log("newfreq=%d   multiplier=%d\n", newfreq, multiplier); */
            SDL_LockAudioDevice(device);
            SDL_SetAudioStreamFormat(stream, spec.format, spec.channels, newfreq, spec.format, spec.channels, spec.freq);
            SDL_UnlockAudioDevice(device);
        }

        /* keep it looping. */
        if (SDL_GetAudioStreamAvailable(stream) < (1024 * 100)) {
            SDL_PutAudioStreamData(stream, audio_buf, audio_len);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &slider_area);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &slider_fill_area);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_CloseAudioDevice(device);
    SDL_free(audio_buf);
    SDL_Quit();
    return 0;
}

