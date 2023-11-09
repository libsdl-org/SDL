/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* quiet windows compiler warnings */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include "testutils.h"

static size_t
widelen(char *data)
{
    size_t len = 0;
    Uint32 *p = (Uint32 *)data;
    while (*p++) {
        ++len;
    }
    return len;
}

int main(int argc, char *argv[])
{
    const char *formats[] = {
        "UTF8",
        "UTF-8",
        "UTF16BE",
        "UTF-16BE",
        "UTF16LE",
        "UTF-16LE",
        "UTF32BE",
        "UTF-32BE",
        "UTF32LE",
        "UTF-32LE",
        "UCS4",
        "UCS-4",
    };

    char *fname = NULL;
    char buffer[BUFSIZ];
    char *ucs4;
    char *test[2];
    int i;
    FILE *file;
    int errors = 0;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
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
            if (!fname) {
                fname = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            static const char *options[] = { "[utf8.txt]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    fname = GetResourceFilename(fname, "utf8.txt");
    file = fopen(fname, "rb");
    if (!file) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to open %s\n", fname);
        return 1;
    }
    SDL_free(fname);

    while (fgets(buffer, sizeof(buffer), file)) {
        /* Convert to UCS-4 */
        size_t len;
        ucs4 =
            SDL_iconv_string("UCS-4", "UTF-8", buffer,
                             SDL_strlen(buffer) + 1);
        len = (widelen(ucs4) + 1) * 4;
        for (i = 0; i < SDL_arraysize(formats); ++i) {
            test[0] = SDL_iconv_string(formats[i], "UCS-4", ucs4, len);
            test[1] = SDL_iconv_string("UCS-4", formats[i], test[0], len);
            if (!test[1] || SDL_memcmp(test[1], ucs4, len) != 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FAIL: %s\n", formats[i]);
                ++errors;
            }
            SDL_free(test[0]);
            SDL_free(test[1]);
        }
        test[0] = SDL_iconv_string("UTF-8", "UCS-4", ucs4, len);
        SDL_free(ucs4);
        (void)fputs(test[0], stdout);
        SDL_free(test[0]);
    }
    (void)fclose(file);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Total errors: %d\n", errors);
    SDLTest_CommonDestroyState(state);
    return errors ? errors + 1 : 0;
}
