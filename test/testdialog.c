/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Sample program:  Create open and save dialogs. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

const SDL_DialogFileFilter filters[] = {
    { "All files", "*" },
    { "SVI Session Indexes", "index;svi-index;index.pb" },
    { "JPG images", "jpg;jpeg" },
    { "PNG images", "png" }
};

static void SDLCALL callback(void *userdata, const char * const *files, int filter) {
    if (files) {
        const char* filter_name = "(filter fetching unsupported)";

        if (filter != -1) {
            if (filter < sizeof(filters) / sizeof(*filters)) {
                filter_name = filters[filter].name;
            } else {
                filter_name = "(No filter was selected)";
            }
        }

        SDL_Log("Filter used: '%s'", filter_name);

        while (*files) {
            SDL_Log("'%s'", *files);
            files++;
        }
    } else {
        SDL_Log("Error: %s", SDL_GetError());
    }
}

int main(int argc, char *argv[])
{
    SDL_Window *w;
    SDL_Renderer *r;
    SDLTest_CommonState *state;
    const SDL_FRect open_file_rect = { 50, 50, 220, 140 };
    const SDL_FRect save_file_rect = { 50, 290, 220, 140 };
    const SDL_FRect open_folder_rect = { 370, 50, 220, 140 };
    int i;
    const char *initial_path = NULL;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);

        if (consumed <= 0) {
            static const char *options[] = { NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed (%s)", SDL_GetError());
        return 1;
    }
    if (!SDL_CreateWindowAndRenderer("testdialog", 640, 480, 0, &w, &r)) {
        SDL_Log("Failed to create window and/or renderer: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    initial_path = SDL_GetUserFolder(SDL_FOLDER_HOME);

    if (!initial_path) {
        SDL_Log("Will not use an initial path, couldn't get the home directory path: %s", SDL_GetError());
    }

    while (1) {
        int quit = 0;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = 1;
                break;
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                const SDL_FPoint p = { e.button.x, e.button.y };
                /*
                 * Arguments, in order:
                 * - A function to call when files are chosen (or dialog is canceled, or error happens)
                 * - A user-managed void pointer to pass to the function when it will be invoked
                 * - The window to bind the dialog to, or NULL if none
                 * - A list of filters for the files, see SDL_DialogFileFilter above (not for SDL_ShowOpenFolderDialog)
                 * - The path where the dialog should start. May be a folder or a file
                 * - Nonzero if the user is allowed to choose multiple entries (not for SDL_ShowSaveFileDialog)
                 */
                if (SDL_PointInRectFloat(&p, &open_file_rect)) {
                    SDL_ShowOpenFileDialog(callback, NULL, w, filters, SDL_arraysize(filters), initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &open_folder_rect)) {
                    SDL_ShowOpenFolderDialog(callback, NULL, w, initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &save_file_rect)) {
                    SDL_ShowSaveFileDialog(callback, NULL, w, filters, SDL_arraysize(filters), initial_path);
                }
            }
        }
        if (quit) {
            break;
        }
        SDL_Delay(100);

        SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(r);

        SDL_SetRenderDrawColor(r, 255, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &open_file_rect);

        SDL_SetRenderDrawColor(r, 0, 255, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &save_file_rect);

        SDL_SetRenderDrawColor(r, 0, 0, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &open_folder_rect);

        SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDLTest_DrawString(r, open_file_rect.x+5, open_file_rect.y+open_file_rect.h/2, "Open File...");
        SDLTest_DrawString(r, save_file_rect.x+5, save_file_rect.y+save_file_rect.h/2, "Save File...");
        SDLTest_DrawString(r, open_folder_rect.x+5, open_folder_rect.y+open_folder_rect.h/2, "Open Folder...");

        SDL_RenderPresent(r);
    }

    SDLTest_CleanupTextDrawing();
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
