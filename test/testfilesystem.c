/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

    if (SDL_asprintf(&fullpath, "%s%s", origdir, fname) < 0) {
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
        SDL_Log("DIRECTORY %s (type=%s, size=%" SDL_PRIu64 ", create=%" SDL_PRIu64 ", mod=%" SDL_PRIu64 ", access=%" SDL_PRIu64 ")",
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


static SDL_EnumerationResult SDLCALL enum_storage_callback(void *userdata, const char *origdir, const char *fname)
{
    SDL_Storage *storage = (SDL_Storage *) userdata;
    SDL_PathInfo info;
    char *fullpath = NULL;

    if (SDL_asprintf(&fullpath, "%s%s", origdir, fname) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        return SDL_ENUM_FAILURE;
    }

    if (!SDL_GetStoragePathInfo(storage, fullpath, &info)) {
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
        SDL_Log("STORAGE %s (type=%s, size=%" SDL_PRIu64 ", create=%" SDL_PRIu64 ", mod=%" SDL_PRIu64 ", access=%" SDL_PRIu64 ")",
                fullpath, type, info.size, info.modify_time, info.create_time, info.access_time);

        if (info.type == SDL_PATHTYPE_DIRECTORY) {
            if (!SDL_EnumerateStorageDirectory(storage, fullpath, enum_storage_callback, userdata)) {
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
    char *curdir;
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s", SDL_GetError());
        return 1;
    }

    base_path = SDL_GetBasePath();
    if (!base_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find base path: %s",
                     SDL_GetError());
    } else {
        SDL_Log("base path: '%s'", base_path);
    }

    pref_path = SDL_GetPrefPath("libsdl", "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path: %s",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'", pref_path);
    }
    SDL_free(pref_path);

    pref_path = SDL_GetPrefPath(NULL, "test_filesystem");
    if (!pref_path) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find pref path without organization: %s",
                     SDL_GetError());
    } else {
        SDL_Log("pref path: '%s'", pref_path);
    }
    SDL_free(pref_path);

    curdir = SDL_GetCurrentDirectory();
    if (!curdir) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find current directory: %s",
                     SDL_GetError());
    } else {
        SDL_Log("current directory: '%s'", curdir);
    }
    SDL_free(curdir);

    if (base_path) {
        char **globlist;
        SDL_Storage *storage = NULL;
        SDL_IOStream *stream;
        const char *text = "foo\n";
        SDL_PathInfo pathinfo;

        if (!SDL_EnumerateDirectory(base_path, enum_callback, NULL)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path enumeration failed!");
        }

        globlist = SDL_GlobDirectory(base_path, "*/test*/T?st*", SDL_GLOB_CASEINSENSITIVE, NULL);
        if (!globlist) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path globbing failed!");
        } else {
            int i;
            for (i = 0; globlist[i]; i++) {
                SDL_Log("DIRECTORY GLOB[%d]: '%s'", i, globlist[i]);
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
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Contents of testfilesystem-A didn't match, expected %s, got %s", text, textA);
                }
                SDL_free(textA);

                textB = (char *)SDL_LoadFile("testfilesystem-B", &sizeB);
                if (!textB || sizeB != SDL_strlen(text) || SDL_strcmp(textB, text) != 0) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Contents of testfilesystem-B didn't match, expected %s, got %s", text, textB);
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

        storage = SDL_OpenFileStorage(base_path);
        if (!storage) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open base path storage object: %s", SDL_GetError());
        } else {
            if (!SDL_EnumerateStorageDirectory(storage, "CMakeFiles", enum_storage_callback, storage)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Storage Base path enumeration failed!");
            }

            globlist = SDL_GlobStorageDirectory(storage, "", "C*/test*/T?st*", SDL_GLOB_CASEINSENSITIVE, NULL);
            if (!globlist) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Base path globbing failed!");
            } else {
                int i;
                for (i = 0; globlist[i]; i++) {
                    SDL_Log("STORAGE GLOB[%d]: '%s'", i, globlist[i]);
                }
                SDL_free(globlist);
            }

            /* these should fail: */
            if (!SDL_GetStoragePathInfo(storage, "CMakeFiles/../testsprite.c", &pathinfo)) {
                SDL_Log("Storage access on path with internal '..' refused correctly.");
            } else {
                SDL_Log("Storage access on path with internal '..' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "CMakeFiles/./TargetDirectories.txt", &pathinfo)) {
                SDL_Log("Storage access on path with internal '.' refused correctly.");
            } else {
                SDL_Log("Storage access on path with internal '.' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "../test", &pathinfo)) {
                SDL_Log("Storage access on path with leading '..' refused correctly.");
            } else {
                SDL_Log("Storage access on path with leading '..' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "./CMakeFiles", &pathinfo)) {
                SDL_Log("Storage access on path with leading '.' refused correctly.");
            } else {
                SDL_Log("Storage access on path with leading '.' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "CMakeFiles/..", &pathinfo)) {
                SDL_Log("Storage access on path with trailing '..' refused correctly.");
            } else {
                SDL_Log("Storage access on path with trailing '..' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "CMakeFiles/.", &pathinfo)) {
                SDL_Log("Storage access on path with trailing '.' refused correctly.");
            } else {
                SDL_Log("Storage access on path with trailing '.' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "..", &pathinfo)) {
                SDL_Log("Storage access on path '..' refused correctly.");
            } else {
                SDL_Log("Storage access on path '..' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, ".", &pathinfo)) {
                SDL_Log("Storage access on path '.' refused correctly.");
            } else {
                SDL_Log("Storage access on path '.' accepted INCORRECTLY.");
            }

            if (!SDL_GetStoragePathInfo(storage, "CMakeFiles\\TargetDirectories.txt", &pathinfo)) {
                SDL_Log("Storage access on path with Windows separator refused correctly.");
            } else {
                SDL_Log("Storage access on path with Windows separator accepted INCORRECTLY.");
            }

            SDL_CloseStorage(storage);
        }

    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
