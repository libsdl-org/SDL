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

static int SDLCALL enum_callback(void *userdata, SDL_FSops *fsops, const char *origdir, const char *fname)
{
    SDL_Stat statbuf;
    char *fullpath = NULL;

    if (SDL_asprintf(&fullpath, "%s%s%s", origdir, *origdir ? "/" : "", fname) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        return -1;
    }

    if (SDL_FSstat(fsops, fullpath, &statbuf) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't stat '%s': %s", fullpath, SDL_GetError());
    } else {
        const char *type;
        if (statbuf.filetype == SDL_STATPATHTYPE_FILE) {
            type = "FILE";
        } else if (statbuf.filetype == SDL_STATPATHTYPE_DIRECTORY) {
            type = "DIRECTORY";
        } else {
            type = "OTHER";
        }
        SDL_Log("%s (type=%s, size=%" SDL_PRIu64 ", mod=%" SDL_PRIu64 ", create=%" SDL_PRIu64 ", access=%" SDL_PRIu64 ")",
                fullpath, type, statbuf.filesize, statbuf.modtime, statbuf.createtime, statbuf.accesstime);

        if (statbuf.filetype == SDL_STATPATHTYPE_DIRECTORY) {
            if (SDL_FSenumerate(fsops, fullpath, enum_callback, userdata) < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Enumeration failed!");
            }
        }
    }

    SDL_free(fullpath);
    return 1;  /* keep going */
}


int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    char *pref_path;
    char *base_path;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (SDL_Init(0) == -1) {
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
        SDL_free(pref_path);
    }

    pref_path = SDL_GetPrefPath(NULL, "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path without organization: %s\n",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'\n", pref_path);
        SDL_free(pref_path);
    }

    if (base_path) {
        SDL_FSops *fsops = SDL_CreateFilesystem(base_path);
        if (SDL_FSenumerate(fsops, "", enum_callback, NULL) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path enumeration failed!");
        }
        SDL_DestroyFilesystem(fsops);
    }

    SDL_free(base_path);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
