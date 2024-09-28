/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of filesystem functions. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_EnumerationResult SDLCALL enum_callback(void *userdata, const char *origdir, const char *fname)
{
    SDL_PathInfo info;
    char *fullpath = NULL;

    /* you can use '/' for a path separator on Windows, but to make the log output look correct, we'll #ifdef this... */
    #ifdef SDL_PLATFORM_WINDOWS
    const char *pathsep = "\\";
    #else
    const char *pathsep = "/";
    #endif

    if (SDL_asprintf(&fullpath, "%s%s%s", origdir, *origdir ? pathsep : "", fname) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        return SDL_ENUM_FAILURE;
    }

    if (!SDL_GetPathInfo(fullpath, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't stat '%s': %s", fullpath, SDL_GetError());
    } else {
        const char *type;
        if (info.type == SDL_PATHTYPE_FILE) {
            type = "FILE";
        } else if (info.type == SDL_PATHTYPE_DIRECTORY) {
            type = "DIRECTORY";
        } else {
            type = "OTHER";
        }
        SDL_Log("%s (type=%s, size=%" SDL_PRIu64 ", create=%" SDL_PRIu64 ", mod=%" SDL_PRIu64 ", access=%" SDL_PRIu64 ")",
                fullpath, type, info.size, info.modify_time, info.create_time, info.access_time);

        if (info.type == SDL_PATHTYPE_DIRECTORY) {
            if (!SDL_EnumerateDirectory(fullpath, enum_callback, userdata)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Enumeration failed!");
            }
        }
    }

    SDL_free(fullpath);
    return SDL_ENUM_CONTINUE;  /* keep going */
}


int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    char *pref_path;
    const char *base_path;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDL_Init(0)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s\n", SDL_GetError());
        return 1;
    }

    base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find base path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("base path: '%s'\n", base_path);
    }

    pref_path = SDL_GetPrefPath("libsdl", "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
    }
    SDL_free(pref_path);

    pref_path = SDL_GetPrefPath(NULL, "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path without organization: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
    }
    SDL_free(pref_path);

    if (base_path) {
        char **globlist;
        SDL_IOStream *stream;
        const char *text = "foo\n";

        if (!SDL_EnumerateDirectory(base_path, enum_callback, NULL)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path enumeration failed!");
        }

        globlist = SDL_GlobDirectory(base_path, "*/test*/T?st*", SDL_GLOB_CASEINSENSITIVE, NULL);
        if (!globlist) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path globbing failed!");
        } else {
            int i;
            for (i = 0; globlist[i]; i++) {
                SDL_Log("GLOB[%d]: '%s'", i, globlist[i]);
            }
            SDL_free(globlist);
        }

        /* !!! FIXME: put this in a subroutine and make it test more thoroughly (and put it in testautomation). */
        if (!SDL_CreateDirectory("testfilesystem-test")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('testfilesystem-test') failed: %s", SDL_GetError());
        } else if (!SDL_CreateDirectory("testfilesystem-test/1")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('testfilesystem-test/1') failed: %s", SDL_GetError());
        } else if (!SDL_CreateDirectory("testfilesystem-test/1")) {  /* THIS SHOULD NOT FAIL! Making a directory that already exists should succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('testfilesystem-test/1') failed: %s", SDL_GetError());
        } else if (!SDL_CreateDirectory("testfilesystem-test/3/4/5/6")) {  /* THIS SHOULD NOT FAIL! Making a directory with missing parents succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('testfilesystem-test/3/4/5/6') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test/3/4/5/6")) {  /* THIS SHOULD NOT FAIL! Making a directory with missing parents succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test/3/4/5/6') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test/3/4/5")) {  /* THIS SHOULD NOT FAIL! Making a directory with missing parents succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test/3/4/5') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test/3/4")) {  /* THIS SHOULD NOT FAIL! Making a directory with missing parents succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test/3/4') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test/3")) {  /* THIS SHOULD NOT FAIL! Making a directory with missing parents succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test/3') failed: %s", SDL_GetError());
        } else if (!SDL_RenamePath("testfilesystem-test/1", "testfilesystem-test/2")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenamePath('testfilesystem-test/1', 'testfilesystem-test/2') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test/2")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test/2') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test') failed: %s", SDL_GetError());
        } else if (!SDL_RemovePath("testfilesystem-test")) {  /* THIS SHOULD NOT FAIL! Removing a directory that is already gone should succeed here. */
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-test') failed: %s", SDL_GetError());
        }

        stream = SDL_IOFromFile("testfilesystem-A", "wb");
        if (stream) {
            SDL_WriteIO(stream, text, SDL_strlen(text));
            SDL_CloseIO(stream);

            if (!SDL_RenamePath("testfilesystem-A", "testfilesystem-B")) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenamePath('testfilesystem-A', 'testfilesystem-B') failed: %s", SDL_GetError());
            } else if (!SDL_CopyFile("testfilesystem-B", "testfilesystem-A")) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CopyFile('testfilesystem-B', 'testfilesystem-A') failed: %s", SDL_GetError());
            } else {
                size_t sizeA, sizeB;
                char *textA, *textB;

                textA = (char *)SDL_LoadFile("testfilesystem-A", &sizeA);
                if (!textA || sizeA != SDL_strlen(text) || SDL_strcmp(textA, text) != 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Contents of testfilesystem-A didn't match, expected %s, got %s\n", text, textA);
                }
                SDL_free(textA);

                textB = (char *)SDL_LoadFile("testfilesystem-B", &sizeB);
                if (!textB || sizeB != SDL_strlen(text) || SDL_strcmp(textB, text) != 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Contents of testfilesystem-B didn't match, expected %s, got %s\n", text, textB);
                }
                SDL_free(textB);
            }

            if (!SDL_RemovePath("testfilesystem-A")) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-A') failed: %s", SDL_GetError());
            }
            if (!SDL_RemovePath("testfilesystem-B")) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('testfilesystem-B') failed: %s", SDL_GetError());
            }
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_IOFromFile('testfilesystem-A', 'w') failed: %s", SDL_GetError());
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
