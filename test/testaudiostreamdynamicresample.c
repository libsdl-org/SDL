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
#include "testutils.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#define SLIDER_WIDTH_PERC 500.f / 600.f
#define SLIDER_HEIGHT_PERC 100.f / 480.f

static int done;
static SDLTest_CommonState *state;

static int multiplier = 100;
static SDL_FRect slider_area;
static SDL_FRect slider_fill_area;
static SDL_AudioSpec spec;
static SDL_AudioStream *stream;
static Uint8 *audio_buf = NULL;
static Uint32 audio_len = 0;

static void loop(void)
{
    int i;
    SDL_Event e;
    int newmultiplier = multiplier;

    while (SDL_PollEvent(&e)) {
        SDLTest_CommonEvent(state, &e, &done);
#ifdef __EMSCRIPTEN__
        if (done) {
            emscripten_cancel_main_loop();
        }
#endif
        if (e.type == SDL_EVENT_MOUSE_MOTION) {
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
        SDL_AudioSpec newspec;
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
        for (i = 0; i < state->num_windows; i++) {
            SDL_SetWindowTitle(state->windows[i], title);
        }

        /* this math sucks, but whatever. */
        if (multiplier < 0) {
            newfreq = spec.freq + (int) ((spec.freq * (multiplier / 400.0f)) * 0.75f);
        } else if (multiplier > 0) {
            newfreq = spec.freq + (int) (spec.freq * (multiplier / 100.0f));
        }
        /* SDL_Log("newfreq=%d   multiplier=%d\n", newfreq, multiplier); */
        SDL_memcpy(&newspec, &spec, sizeof (spec));
        newspec.freq = newfreq;
        SDL_SetAudioStreamFormat(stream, &newspec, NULL);
    }

    /* keep it looping. */
    if (SDL_GetAudioStreamAvailable(stream) < ((int) (audio_len / 2))) {
        SDL_PutAudioStreamData(stream, audio_buf, audio_len);
    }

    for (i = 0; i < state->num_windows; i++) {
        SDL_SetRenderDrawColor(state->renderers[i], 0, 0, 255, 255);
        SDL_RenderClear(state->renderers[i]);
        SDL_SetRenderDrawColor(state->renderers[i], 0, 0, 0, 255);
        SDL_RenderFillRect(state->renderers[i], &slider_area);
        SDL_SetRenderDrawColor(state->renderers[i], 255, 0, 0, 255);
        SDL_RenderFillRect(state->renderers[i], &slider_fill_area);
        SDL_RenderPresent(state->renderers[i]);
    }
}

int main(int argc, char *argv[])
{
    char *filename = NULL;
    int i;
    int rc;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    if (state == NULL) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!filename) {
                filename = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[sample.wav]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            exit(1);
        }

        i += consumed;
    }

    slider_area.x = state->window_w * (1.f - SLIDER_WIDTH_PERC) / 2;
    slider_area.y = state->window_h * (1.f - SLIDER_HEIGHT_PERC) / 2;
    slider_area.w = SLIDER_WIDTH_PERC * state->window_w;
    slider_area.h = SLIDER_HEIGHT_PERC * state->window_h;

    slider_fill_area = slider_area;
    slider_fill_area.w /= 2;

    /* Load the SDL library */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    filename = GetResourceFilename(filename, "sample.wav");
    rc = SDL_LoadWAV(filename, &spec, &audio_buf, &audio_len);
    SDL_free(filename);

    if (rc < 0) {
        SDL_Log("Failed to load '%s': %s", filename, SDL_GetError());
        SDL_Quit();
        return 1;
    }

    for (i = 0; i < state->num_windows; i++) {
        SDL_SetWindowTitle(state->windows[i], "Drag the slider: Normal speed");
    }

    stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_PutAudioStreamData(stream, audio_buf, audio_len);
    SDL_BindAudioStream(state->audio_id, stream);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDL_DestroyAudioStream(stream);
    SDL_free(audio_buf);
    SDLTest_CommonQuit(state);
    SDL_Quit();
    return 0;
}

