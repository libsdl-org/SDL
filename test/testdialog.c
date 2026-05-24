/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Sample program:  Create open and save dialogs. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static const SDL_DialogFileFilter filters[] = {
    { "All files", "*" },
    { "SVI Session Indexes", "index;svi-index;index.pb" },
    { "JPG images", "jpg;jpeg" },
    { "PNG images", "png" }
};

static void SDLCALL callback(void *userdata, const char * const *files, int filter) {
    char **saved_path = userdata;

    if (files) {
        const char *filter_name = "(filter fetching unsupported)";

        if (filter != -1) {
            if (filter < sizeof(filters) / sizeof(*filters)) {
                filter_name = filters[filter].name;
            } else {
                filter_name = "(No filter was selected)";
            }
        }

        SDL_Log("Filter used: '%s'", filter_name);

        if (*files && saved_path) {
            *saved_path = SDL_strdup(*files);
            /* Create the file */
            SDL_IOStream *stream = SDL_IOFromFile(*saved_path, "w");
            SDL_CloseIO(stream);
        }

        while (*files) {
            SDL_Log("'%s'", *files);
            files++;
        }
    } else {
        SDL_Log("Error: %s", SDL_GetError());
    }
}

static char *concat_strings(const char *a, const char *b)
{
    char *out = NULL;

    if (a != NULL && b != NULL) {
        const size_t out_size = SDL_strlen(a) + SDL_strlen(b) + 1;
        out = (char *)SDL_malloc(out_size);
        if (out) {
            *out = '\0';
            SDL_strlcat(out, a, out_size);
            SDL_strlcat(out, b, out_size);
        }
    }

    return out;
}

int main(int argc, char *argv[])
{
    static const SDL_FRect OPEN_FILE_RECT = { 50, 50, 220, 140 };
    static const SDL_FRect SAVE_FILE_RECT = { 50, 290, 220, 140 };
    static const SDL_FRect OPEN_FOLDER_RECT = { 370, 50, 220, 140 };
    static const char DEFAULT_FILENAME[] = "Untitled.index";
    const char *initial_path = NULL;
    char *last_saved_path = NULL;

    /* Initialize test framework */
    SDLTest_CommonState *state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (state == NULL) {
        return 1;
    }

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        SDL_Quit();
        SDLTest_CommonDestroyState(state);
        return 1;
    }

    if (!SDLTest_CommonInit(state)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }

    initial_path = SDL_GetUserFolder(SDL_FOLDER_HOME);

    if (!initial_path) {
        SDL_Log("Will not use an initial path, couldn't get the home directory path: %s", SDL_GetError());
    }

    while (1) {
        int done = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDLTest_CommonEvent(state, &e, &done);
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                const SDL_FPoint p = { e.button.x, e.button.y };
                SDL_Window *w = SDL_GetWindowFromID(e.button.windowID);
                /*
                 * Arguments, in order:
                 * - A function to call when files are chosen (or dialog is canceled, or error happens)
                 * - A user-managed void pointer to pass to the function when it will be invoked
                 * - The window to bind the dialog to, or NULL if none
                 * - A list of filters for the files, see SDL_DialogFileFilter above (not for SDL_ShowOpenFolderDialog)
                 * - The path where the dialog should start. May be a folder or a file
                 * - Nonzero if the user is allowed to choose multiple entries (not for SDL_ShowSaveFileDialog)
                 */
                if (SDL_PointInRectFloat(&p, &OPEN_FILE_RECT)) {
                    SDL_ShowOpenFileDialog(callback, NULL, w, filters, SDL_arraysize(filters), initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &OPEN_FOLDER_RECT)) {
                    SDL_ShowOpenFolderDialog(callback, NULL, w, initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &SAVE_FILE_RECT)) {
                    char *save_path = NULL;
                    if (last_saved_path) {
                        save_path = SDL_strdup(last_saved_path);
                    } else {
                        save_path = concat_strings(initial_path, DEFAULT_FILENAME);
                    }
                    SDL_ShowSaveFileDialog(callback, &last_saved_path, w, filters, SDL_arraysize(filters), save_path ? save_path : DEFAULT_FILENAME);
                    SDL_free(save_path);
                }
            }
        }
        if (done) {
            break;
        }
        SDL_Delay(100);

        for (int i = 0; i < state->num_windows; i++) {
            SDL_Renderer *r = state->renderers[i];

            SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderClear(r);

            SDL_SetRenderDrawColor(r, 255, 0, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(r, &OPEN_FILE_RECT);

            SDL_SetRenderDrawColor(r, 0, 255, 0, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(r, &SAVE_FILE_RECT);

            SDL_SetRenderDrawColor(r, 0, 0, 255, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(r, &OPEN_FOLDER_RECT);

            SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
            SDLTest_DrawString(r, OPEN_FILE_RECT.x+5, OPEN_FILE_RECT.y+OPEN_FILE_RECT.h/2, "Open File...");
            SDLTest_DrawString(r, SAVE_FILE_RECT.x+5, SAVE_FILE_RECT.y+SAVE_FILE_RECT.h/2, "Save File...");
            SDLTest_DrawString(r, OPEN_FOLDER_RECT.x+5, OPEN_FOLDER_RECT.y+OPEN_FOLDER_RECT.h/2, "Open Folder...");

            SDL_RenderPresent(r);
        }
    }

    SDL_free(last_saved_path);
    SDLTest_CleanupTextDrawing();
    SDLTest_CommonQuit(state);
    return 0;
}
