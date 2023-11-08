/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

static void log_usage(char *progname, SDLTest_CommonState *state) {
    static const char *options[] = { "in.wav", "out.wav", "newfreq", "newchan", NULL };
    SDLTest_CommonLogUsage(state, progname, options);
}

int main(int argc, char **argv)
{
    SDL_AudioSpec spec;
    SDL_AudioSpec cvtspec;
    SDL_AudioStream *stream = NULL;
    Uint8 *dst_buf = NULL;
    Uint32 len = 0;
    Uint8 *data = NULL;
    int bitsize = 0;
    int blockalign = 0;
    int avgbytes = 0;
    SDL_RWops *io = NULL;
    int dst_len;
    int ret = 0;
    int argpos = 0;
    int i;
    SDLTest_CommonState *state;
    char *file_in = NULL;
    char *file_out = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_zero(cvtspec);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (argpos == 0) {
                file_in = argv[i];
                argpos++;
                consumed = 1;
            } else if (argpos == 1) {
                file_out = argv[i];
                argpos++;
                consumed = 1;
            } else if (argpos == 2) {
                char *endp;
                cvtspec.freq  = (int)SDL_strtoul(argv[i], &endp, 0);
                if (endp != argv[i] && *endp == '\0') {
                    argpos++;
                    consumed = 1;
                }
            } else if (argpos == 3) {
                char *endp;
                cvtspec.channels = (int)SDL_strtoul(argv[i], &endp, 0);
                if (endp != argv[i] && *endp == '\0') {
                    argpos++;
                    consumed = 1;
                }
            }
        }
        if (consumed <= 0) {
            log_usage(argv[0], state);
            ret = 1;
            goto end;
        }

        i += consumed;
    }

    if (argpos != 4) {
        log_usage(argv[0], state);
        ret = 1;
        goto end;
    }

    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
        ret = 2;
        goto end;
    }

    if (SDL_LoadWAV(file_in, &spec, &data, &len) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to load %s: %s\n", file_in, SDL_GetError());
        ret = 3;
        goto end;
    }

    cvtspec.format = spec.format;
    if (SDL_ConvertAudioSamples(&spec, data, len, &cvtspec, &dst_buf, &dst_len) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to convert samples: %s\n", SDL_GetError());
        ret = 4;
        goto end;
    }

    /* write out a WAV header... */
    io = SDL_RWFromFile(file_out, "wb");
    if (!io) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fopen('%s') failed: %s\n", file_out, SDL_GetError());
        ret = 5;
        goto end;
    }

    bitsize = SDL_AUDIO_BITSIZE(spec.format);
    blockalign = (bitsize / 8) * cvtspec.channels;
    avgbytes = cvtspec.freq * blockalign;

    SDL_WriteU32LE(io, 0x46464952); /* RIFF */
    SDL_WriteU32LE(io, dst_len + 36);
    SDL_WriteU32LE(io, 0x45564157);                             /* WAVE */
    SDL_WriteU32LE(io, 0x20746D66);                             /* fmt */
    SDL_WriteU32LE(io, 16);                                     /* chunk size */
    SDL_WriteU16LE(io, SDL_AUDIO_ISFLOAT(spec.format) ? 3 : 1); /* uncompressed */
    SDL_WriteU16LE(io, cvtspec.channels);                       /* channels */
    SDL_WriteU32LE(io, cvtspec.freq);                           /* sample rate */
    SDL_WriteU32LE(io, avgbytes);                               /* average bytes per second */
    SDL_WriteU16LE(io, blockalign);                             /* block align */
    SDL_WriteU16LE(io, bitsize);                                /* significant bits per sample */
    SDL_WriteU32LE(io, 0x61746164);                             /* data */
    SDL_WriteU32LE(io, dst_len);                                /* size */
    SDL_RWwrite(io, dst_buf, dst_len);

    if (SDL_RWclose(io) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fclose('%s') failed: %s\n", file_out, SDL_GetError());
        ret = 6;
        goto end;
    }

end:
    SDL_free(dst_buf);
    SDL_free(data);
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return ret;
}
