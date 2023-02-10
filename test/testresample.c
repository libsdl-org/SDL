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

int main(int argc, char **argv)
{
    SDL_AudioSpec spec;
    SDL_AudioStream *stream = NULL;
    Uint8 *dst_buf = NULL;
    Uint32 len = 0;
    Uint8 *data = NULL;
    int cvtfreq = 0;
    int cvtchans = 0;
    int bitsize = 0;
    int blockalign = 0;
    int avgbytes = 0;
    SDL_RWops *io = NULL;
    int dst_len;
    int ret = 0;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc != 5) {
        SDL_Log("USAGE: %s in.wav out.wav newfreq newchans\n", argv[0]);
        ret = 1;
        goto end;
    }

    cvtfreq = SDL_atoi(argv[3]);
    cvtchans = SDL_atoi(argv[4]);

    if (SDL_Init(SDL_INIT_AUDIO) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
        ret = 2;
        goto end;
    }

    if (SDL_LoadWAV(argv[1], &spec, &data, &len) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to load %s: %s\n", argv[1], SDL_GetError());
        ret = 3;
        goto end;
    }

    if (SDL_ConvertAudioSamples(spec.format, spec.channels, spec.freq, data, len,
                           spec.format, cvtchans, cvtfreq, &dst_buf, &dst_len) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed to convert samples: %s\n", SDL_GetError());
        ret = 4;
        goto end;
    }

    /* write out a WAV header... */
    io = SDL_RWFromFile(argv[2], "wb");
    if (io == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fopen('%s') failed: %s\n", argv[2], SDL_GetError());
        ret = 5;
        goto end;
    }

    bitsize = SDL_AUDIO_BITSIZE(spec.format);
    blockalign = (bitsize / 8) * cvtchans;
    avgbytes = cvtfreq * blockalign;

    SDL_WriteLE32(io, 0x46464952); /* RIFF */
    SDL_WriteLE32(io, dst_len + 36);
    SDL_WriteLE32(io, 0x45564157);                             /* WAVE */
    SDL_WriteLE32(io, 0x20746D66);                             /* fmt */
    SDL_WriteLE32(io, 16);                                     /* chunk size */
    SDL_WriteLE16(io, SDL_AUDIO_ISFLOAT(spec.format) ? 3 : 1); /* uncompressed */
    SDL_WriteLE16(io, cvtchans);                               /* channels */
    SDL_WriteLE32(io, cvtfreq);                                /* sample rate */
    SDL_WriteLE32(io, avgbytes);                               /* average bytes per second */
    SDL_WriteLE16(io, blockalign);                             /* block align */
    SDL_WriteLE16(io, bitsize);                                /* significant bits per sample */
    SDL_WriteLE32(io, 0x61746164);                             /* data */
    SDL_WriteLE32(io, dst_len);                                /* size */
    SDL_RWwrite(io, dst_buf, dst_len);

    if (SDL_RWclose(io) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "fclose('%s') failed: %s\n", argv[2], SDL_GetError());
        ret = 6;
        goto end;
    }

end:
    SDL_free(dst_buf);
    SDL_free(data);
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return ret;
}
