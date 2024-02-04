/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#include <stdlib.h>

#define SLIDER_WIDTH_PERC 500.f / 600.f
#define SLIDER_HEIGHT_PERC 70.f / 480.f

static int done;
static SDLTest_CommonState *state;

static SDL_AudioSpec spec;
static SDL_AudioStream *stream;
static Uint8 *audio_buf = NULL;
static Uint32 audio_len = 0;

static SDL_bool auto_loop = SDL_TRUE;
static SDL_bool auto_flush = SDL_FALSE;

static Uint64 last_get_callback = 0;
static int last_get_amount_additional = 0;
static int last_get_amount_total = 0;

typedef struct Slider
{
    SDL_FRect area;
    SDL_bool changed;
    char fmtlabel[64];
    float pos;
    int flags;
    float min;
    float mid;
    float max;
    float value;
} Slider;

#define NUM_SLIDERS 3
Slider sliders[NUM_SLIDERS];
static int active_slider = -1;

static void init_slider(int index, const char* fmtlabel, int flags, float value, float min, float max)
{
    Slider* slider = &sliders[index];

    slider->area.x = state->window_w * (1.f - SLIDER_WIDTH_PERC) / 2;
    slider->area.y = state->window_h * (0.2f + (index * SLIDER_HEIGHT_PERC * 1.4f));
    slider->area.w = SLIDER_WIDTH_PERC * state->window_w;
    slider->area.h = SLIDER_HEIGHT_PERC * state->window_h;
    slider->changed = SDL_TRUE;
    SDL_strlcpy(slider->fmtlabel, fmtlabel, SDL_arraysize(slider->fmtlabel));
    slider->flags = flags;
    slider->min = min;
    slider->max = max;
    slider->value = value;

    if (slider->flags & 1) {
        slider->pos = (value - slider->min + 0.5f) / (slider->max - slider->min + 1.0f);
    } else {
        slider->pos = 0.5f;
        slider->mid = value;
    }
}

static float lerp(float v0, float v1, float t)
{
    return (1 - t) * v0 + t * v1;
}

static void draw_text(SDL_Renderer* renderer, int x, int y, const char* text)
{
    SDL_SetRenderDrawColor(renderer, 0xFD, 0xF6, 0xE3, 0xFF);
    SDLTest_DrawString(renderer, (float) x, (float) y, text);
}

static void draw_textf(SDL_Renderer* renderer, int x, int y, const char* fmt, ...)
{
    char text[256];
    va_list ap;

    va_start(ap, fmt);
    SDL_vsnprintf(text, SDL_arraysize(text), fmt, ap);
    va_end(ap);

    draw_text(renderer, x, y, text);
}

static void queue_audio()
{
    Uint8* new_data = NULL;
    int new_len = 0;
    int retval = 0;
    SDL_AudioSpec new_spec;

    new_spec.format = spec.format;
    new_spec.channels = (int) sliders[2].value;
    new_spec.freq = (int) sliders[1].value;

    SDL_Log("Converting audio from %i to %i", spec.freq, new_spec.freq);

    retval = retval ? retval : SDL_ConvertAudioSamples(&spec, audio_buf, audio_len, &new_spec, &new_data, &new_len);
    retval = retval ? retval : SDL_SetAudioStreamFormat(stream, &new_spec, NULL);
    retval = retval ? retval : SDL_PutAudioStreamData(stream, new_data, new_len);

    if (auto_flush) {
        retval = retval ? retval : SDL_FlushAudioStream(stream);
    }

    SDL_free(new_data);

    if (retval) {
        SDL_Log("Failed to queue audio: %s", SDL_GetError());
    } else {
        SDL_Log("Queued audio");
    }
}

static void skip_audio(float amount)
{
    float speed;
    SDL_AudioSpec dst_spec, new_spec;
    int num_frames;
    int retval = 0;
    void* buf = NULL;

    SDL_LockAudioStream(stream);

    speed = SDL_GetAudioStreamFrequencyRatio(stream);
    SDL_GetAudioStreamFormat(stream, NULL, &dst_spec);

    /* Gimme that crunchy audio */
    new_spec.format = SDL_AUDIO_S8;
    new_spec.channels = 1;
    new_spec.freq = 4000;

    SDL_SetAudioStreamFrequencyRatio(stream, 100.0f);
    SDL_SetAudioStreamFormat(stream, NULL, &new_spec);

    num_frames = (int)(new_spec.freq * ((speed * amount) / 100.0f));
    buf = SDL_malloc(num_frames);

    if (buf) {
        retval = SDL_GetAudioStreamData(stream, buf, num_frames);
        SDL_free(buf);
    }

    SDL_SetAudioStreamFormat(stream, NULL, &dst_spec);
    SDL_SetAudioStreamFrequencyRatio(stream, speed);

    SDL_UnlockAudioStream(stream);

    if (retval >= 0) {
        SDL_Log("Skipped %.2f seconds", amount);
    } else {
        SDL_Log("Failed to skip: %s", SDL_GetError());
    }
}

static const char *AudioFmtToString(const SDL_AudioFormat fmt)
{
    switch (fmt) {
        #define FMTCASE(x) case SDL_AUDIO_##x: return #x
        FMTCASE(U8);
        FMTCASE(S8);
        FMTCASE(S16LE);
        FMTCASE(S16BE);
        FMTCASE(S32LE);
        FMTCASE(S32BE);
        FMTCASE(F32LE);
        FMTCASE(F32BE);
        #undef FMTCASE
    }
    return "?";
}

static const char *AudioChansToStr(const int channels)
{
    switch (channels) {
        case 1: return "Mono";
        case 2: return "Stereo";
        case 3: return "2.1";
        case 4: return "Quad";
        case 5: return "4.1";
        case 6: return "5.1";
        case 7: return "6.1";
        case 8: return "7.1";
        default: break;
    }
    return "?";
}

static void loop(void)
{
    int i, j;
    SDL_Event e;
    SDL_FPoint p;
    SDL_AudioSpec src_spec, dst_spec;
    int available_bytes = 0;
    float available_seconds = 0;

    while (SDL_PollEvent(&e)) {
        SDLTest_CommonEvent(state, &e, &done);
#ifdef SDL_PLATFORM_EMSCRIPTEN
        if (done) {
            emscripten_cancel_main_loop();
        }
#endif
        if (e.type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode sym = e.key.keysym.sym;
            if (sym == SDLK_q) {
                if (SDL_AudioDevicePaused(state->audio_id)) {
                    SDL_ResumeAudioDevice(state->audio_id);
                } else {
                    SDL_PauseAudioDevice(state->audio_id);
                }
            } else if (sym == SDLK_w) {
                auto_loop = !auto_loop;
            } else if (sym == SDLK_e) {
                auto_flush = !auto_flush;
            } else if (sym == SDLK_a) {
                SDL_ClearAudioStream(stream);
                SDL_Log("Cleared audio stream");
            } else if (sym == SDLK_s) {
                queue_audio();
            } else if (sym == SDLK_d) {
                float amount = 1.0f;
                amount *= (e.key.keysym.mod & SDL_KMOD_CTRL) ? 10.0f : 1.0f;
                amount *= (e.key.keysym.mod & SDL_KMOD_SHIFT) ? 10.0f : 1.0f;
                skip_audio(amount);
            }
        }
    }

    if (SDL_GetMouseState(&p.x, &p.y) & SDL_BUTTON_LMASK) {
        if (active_slider == -1) {
            for (i = 0; i < NUM_SLIDERS; ++i) {
                if (SDL_PointInRectFloat(&p, &sliders[i].area)) {
                    active_slider = i;
                    break;
                }
            }
        }
    } else {
        active_slider = -1;
    }

    if (active_slider != -1) {
        Slider* slider = &sliders[active_slider];

        float value = (p.x - slider->area.x) / slider->area.w;
        value = SDL_clamp(value, 0.0f, 1.0f);
        slider->pos = value;

        if (slider->flags & 1) {
            value = slider->min + (value * (slider->max - slider->min + 1.0f));
            value = SDL_clamp(value, slider->min, slider->max);
        } else {
            value = (value * 2.0f) - 1.0f;
            value = (value >= 0)
                ? lerp(slider->mid, slider->max, value)
                : lerp(slider->mid, slider->min, -value);
        }

        if (value != slider->value) {
            slider->value = value;
            slider->changed = SDL_TRUE;
        }
    }

    if (sliders[0].changed) {
        sliders[0].changed = SDL_FALSE;
        SDL_SetAudioStreamFrequencyRatio(stream, sliders[0].value);
    }

    if (SDL_GetAudioStreamFormat(stream, &src_spec, &dst_spec) == 0) {
        available_bytes = SDL_GetAudioStreamAvailable(stream);
        available_seconds = (float)available_bytes / (float)(SDL_AUDIO_FRAMESIZE(dst_spec) * dst_spec.freq);

        /* keep it looping. */
        if (auto_loop && (available_seconds < 10.0f)) {
            queue_audio();
        }
    }

    for (i = 0; i < state->num_windows; i++) {
        int draw_y = 0;
        SDL_Renderer* rend = state->renderers[i];

        SDL_SetRenderDrawColor(rend, 0x00, 0x2B, 0x36, 0xFF);
        SDL_RenderClear(rend);

        for (j = 0; j < NUM_SLIDERS; ++j) {
            Slider* slider = &sliders[j];
            SDL_FRect area;

            SDL_copyp(&area, &slider->area);

            SDL_SetRenderDrawColor(rend, 0x07, 0x36, 0x42, 0xFF);
            SDL_RenderFillRect(rend, &area);

            area.w *= slider->pos;

            SDL_SetRenderDrawColor(rend, 0x58, 0x6E, 0x75, 0xFF);
            SDL_RenderFillRect(rend, &area);

            draw_textf(rend, (int)slider->area.x, (int)slider->area.y, slider->fmtlabel,
                (slider->flags & 2) ? ((float)(int)slider->value) : slider->value);
        }

        draw_textf(rend, 0, draw_y, "%7s, Loop: %3s, Flush: %3s",
            SDL_AudioDevicePaused(state->audio_id) ? "Paused" : "Playing", auto_loop ? "On" : "Off", auto_flush ? "On" : "Off");
        draw_y += FONT_LINE_HEIGHT;

        draw_textf(rend, 0, draw_y, "Available: %4.2f (%i bytes)", available_seconds, available_bytes);
        draw_y += FONT_LINE_HEIGHT;

        SDL_LockAudioStream(stream);

        draw_textf(rend, 0, draw_y, "Get Callback: %i/%i bytes, %2i ms ago",
            last_get_amount_additional, last_get_amount_total, (int)(SDL_GetTicks() - last_get_callback));
        draw_y += FONT_LINE_HEIGHT;

        SDL_UnlockAudioStream(stream);

        draw_y = state->window_h - FONT_LINE_HEIGHT * 3;

        draw_textf(rend, 0, draw_y, "Wav: %6s/%6s/%i",
            AudioFmtToString(spec.format), AudioChansToStr(spec.channels), spec.freq);
        draw_y += FONT_LINE_HEIGHT;

        draw_textf(rend, 0, draw_y, "Src: %6s/%6s/%i",
            AudioFmtToString(src_spec.format), AudioChansToStr(src_spec.channels), src_spec.freq);
        draw_y += FONT_LINE_HEIGHT;

        draw_textf(rend, 0, draw_y, "Dst: %6s/%6s/%i",
            AudioFmtToString(dst_spec.format), AudioChansToStr(dst_spec.channels), dst_spec.freq);
        draw_y += FONT_LINE_HEIGHT;

        SDL_RenderPresent(rend);
    }
}

static void SDLCALL our_get_callback(void *userdata, SDL_AudioStream *strm, int additional_amount, int total_amount)
{
    last_get_callback = SDL_GetTicks();
    last_get_amount_additional = additional_amount;
    last_get_amount_total = total_amount;
}

int main(int argc, char *argv[])
{
    char *filename = NULL;
    int i;
    int rc;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    if (!state) {
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

    /* Load the SDL library */
    if (!SDLTest_CommonInit(state)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    FONT_CHARACTER_SIZE = 16;

    filename = GetResourceFilename(filename, "sample.wav");
    rc = SDL_LoadWAV(filename, &spec, &audio_buf, &audio_len);
    SDL_free(filename);

    if (rc < 0) {
        SDL_Log("Failed to load '%s': %s", filename, SDL_GetError());
        SDL_Quit();
        return 1;
    }

    init_slider(0, "Speed: %3.2fx", 0x0, 1.0f, 0.2f, 5.0f);
    init_slider(1, "Freq: %g", 0x2, (float)spec.freq, 4000.0f, 192000.0f);
    init_slider(2, "Channels: %g", 0x3, (float)spec.channels, 1.0f, 8.0f);

    for (i = 0; i < state->num_windows; i++) {
        SDL_SetWindowTitle(state->windows[i], "Resampler Test");
    }

    stream = SDL_CreateAudioStream(&spec, &spec);
    SDL_SetAudioStreamGetCallback(stream, our_get_callback, NULL);

    SDL_BindAudioStream(state->audio_id, stream);

#ifdef SDL_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    SDLTest_CleanupTextDrawing();
    SDL_DestroyAudioStream(stream);
    SDL_free(audio_buf);
    SDLTest_CommonQuit(state);
    return 0;
}

