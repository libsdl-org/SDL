/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

static char *get_next_line(Uint8 **fdataptr, size_t *fdatalen)
{
    char *retval = (char *) *fdataptr;
    Uint8 *ptr = *fdataptr;
    size_t len = *fdatalen;

    if (len == 0) {
        return NULL;
    }

    while (len > 0) {
        if (*ptr == '\r') {
            *ptr = '\0';
        } else if (*ptr == '\n') {
            *ptr = '\0';
            ptr++;
            len--;
            break;
        }
        ptr++;
        len--;
    }

    *fdataptr = ptr;
    *fdatalen = len;
    return retval;
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
    char *ucs4;
    char *test[2];
    int i;
    int errors = 0;
    SDLTest_CommonState *state;
    Uint8 *fdata = NULL;
    Uint8 *fdataptr = NULL;
    char *line = NULL;
    size_t fdatalen = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

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
    fdata = (Uint8 *) (fname ? SDL_LoadFile(fname, &fdatalen) : NULL);
    if (!fdata) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to load %s\n", fname);
        return 1;
    }

    fdataptr = fdata;
    while ((line = get_next_line(&fdataptr, &fdatalen)) != NULL) {
        /* Convert to UCS-4 */
        size_t len;
        ucs4 = SDL_iconv_string("UCS-4", "UTF-8", line, SDL_strlen(line) + 1);
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
        SDL_Log("%s", test[0]);
        SDL_free(test[0]);
    }
    SDL_free(fdata);
    SDL_free(fname);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Total errors: %d\n", errors);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return errors ? errors + 1 : 0;
}
