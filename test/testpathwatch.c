/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Simple test of path watch callback. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static SDL_Mutex *lock = NULL;
static SDL_Condition *condvar = NULL;
static SDL_PathWatchEventType last_event = 0;
static const char *last_path = NULL;

static const char *EventTotString(SDL_PathWatchEventType type)
{
    switch (type) {
        case SDL_PATHWATCH_MODIFIED: return "SDL_PATHWATCH_MODIFIED";
        case SDL_PATHWATCH_CREATED: return "SDL_PATHWATCH_CREATED";
        case SDL_PATHWATCH_REMOVED: return "SDL_PATHWATCH_REMOVED";
        case SDL_PATHWATCH_REMOVED_SELF: return "SDL_PATHWATCH_REMOVED_SELF";
        default: return "???";
    }
}

void SDLCALL FileWatchCallback(void *userdata, const char *path, SDL_PathWatchEventType type)
{
    (void)userdata;
    SDL_LockMutex(lock);
    SDL_Log("received event %s for path '%s'", EventTotString(type), path);
    last_event = type;
    SDL_free((void *)last_path);
    last_path = SDL_strdup(path);
    SDL_SignalCondition(condvar);
    SDL_UnlockMutex(lock);
}

static void CheckWatchEvent(const char *expected_path, SDL_PathWatchEventType expected_event_type)
{
    if (SDL_WaitConditionTimeout(condvar, lock, 500)) { // There is a timeout for the full test in CI, so keep this short
        if (!last_path || SDL_strcmp(last_path, expected_path) != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed: wrong path for event %s: got '%s', expected '%s'", EventTotString(expected_event_type), last_path, expected_path);
        }
        if (last_event != expected_event_type) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed: wrong event type for path '%s': got: %s, expected: %s", last_path, EventTotString(last_event), EventTotString(expected_event_type));
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "failed: missing event %s for path '%s'", EventTotString(expected_event_type), expected_path);
    }
    SDL_free((void *)last_path);
    last_path = NULL;
    last_event = 0;
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    if (!SDL_Init(SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() failed: %s", SDL_GetError());
        return 1;
    }

    lock = SDL_CreateMutex();
    if (!lock) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateMutex() failed: %s", SDL_GetError());
        goto error;
    }
    condvar = SDL_CreateCondition();
    if (!condvar) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateCondition() failed: %s", SDL_GetError());
        goto error;
    }

    SDL_IOStream *stream;
    const char *text = "foo\n";

    if (SDL_CreateDirectory("./testpathwatch-test")) {
        if (!SDL_AddPathWatch("./testpathwatch-test", FileWatchCallback, NULL)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AddPathWatch('./testpathwatch-test') failed: %s", SDL_GetError());
            goto error;
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('./testpathwatch-test') failed: %s", SDL_GetError());
        goto error;
    }

    SDL_LockMutex(lock);

    // create sub-directory
    if (!SDL_CreateDirectory("./testpathwatch-test/1/")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateDirectory('./testpathwatch-test/1/') failed: %s", SDL_GetError());
    } else {
        CheckWatchEvent("./testpathwatch-test/1/", SDL_PATHWATCH_CREATED);
    }

    // delete sub-directory
    if (!SDL_RemovePath("./testpathwatch-test/1/")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('./testpathwatch-test/1/') failed: %s", SDL_GetError());
    } else {
        CheckWatchEvent("./testpathwatch-test/1/", SDL_PATHWATCH_REMOVED);
    }

    // create file
    stream = SDL_IOFromFile("./testpathwatch-test/A", "wb");
    if (!stream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_IOFromFile('./testpathwatch/A', 'wb') failed: %s", SDL_GetError());
    } else {
        CheckWatchEvent("./testpathwatch-test/A", SDL_PATHWATCH_CREATED);
    }

    // write to file
    if (SDL_WriteIO(stream, text, SDL_strlen(text)) > SDL_strlen(text)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_WriteIO(stream, text, SDL_strlen(text)) failed: %s", SDL_GetError());
    } else {
        SDL_FlushIO(stream);
        CheckWatchEvent("./testpathwatch-test/A", SDL_PATHWATCH_MODIFIED);
    }
    SDL_CloseIO(stream);

    // delete file
    if (!SDL_RemovePath("./testpathwatch-test/A")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('./testpathwatch-test/A') failed: %s", SDL_GetError());
    } else {
        CheckWatchEvent("./testpathwatch-test/A", SDL_PATHWATCH_REMOVED);
    }

    // delete directory
    if (!SDL_RemovePath("./testpathwatch-test")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RemovePath('./testpathwatch-test') failed: %s", SDL_GetError());
    } else {
        CheckWatchEvent("./testpathwatch-test", SDL_PATHWATCH_REMOVED_SELF);
    }

    SDL_UnlockMutex(lock);

error:
    SDL_RemovePathWatch("./testpathwatch-test", FileWatchCallback, NULL);
    SDL_free((void *)last_path);
    {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}
    }
    SDL_DestroyMutex(lock);
    SDL_DestroyCondition(condvar);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
