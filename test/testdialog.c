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

static void SDLCALL file_callback(void *userdata, const char * const *files, int filter) {
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

static void SDLCALL input_callback(void *userdata, const char *input, SDL_DialogResult result) {
    switch (result)
    {
    case SDL_DIALOGRESULT_ERROR:
        SDL_Log("Error: %s\n", SDL_GetError());
        break;

    case SDL_DIALOGRESULT_CANCEL:
        SDL_Log("Cancel\n");
        break;

    case SDL_DIALOGRESULT_SUCCESS:
        SDL_Log("'%s'\n", input);
        break;

    default:
        SDL_Log("Unknown result: %d\n", result);
    }
}

static bool progress_done = false;

static void SDLCALL progress_callback(void* dialog, SDL_DialogResult result)
{
    switch (result)
    {
    case SDL_DIALOGRESULT_ERROR:
        SDL_Log("Error: %s\n", SDL_GetError());
        break;

    case SDL_DIALOGRESULT_CANCEL:
        SDL_Log("Cancel\n");
        break;

    case SDL_DIALOGRESULT_SUCCESS:
        SDL_Log("Success\n");
        break;

    default:
        SDL_Log("Unknown result: %d\n", result);
    }

    progress_done = true;
}

static void SDLCALL color_callback(void* userdata, SDL_Color c, SDL_DialogResult result)
{
    switch (result)
    {
    case SDL_DIALOGRESULT_ERROR:
        SDL_Log("Error: %s\n", SDL_GetError());
        break;

    case SDL_DIALOGRESULT_CANCEL:
        SDL_Log("Cancel\n");
        break;

    case SDL_DIALOGRESULT_SUCCESS:
        SDL_Log("%d, %d, %d, %d\n", c.r, c.g, c.b, c.a);
        break;

    default:
        SDL_Log("Unknown result: %d\n", result);
    }
}

static void SDLCALL date_callback(void* userdata, SDL_Date d, SDL_DialogResult result)
{
    switch (result)
    {
    case SDL_DIALOGRESULT_ERROR:
        SDL_Log("Error: %s\n", SDL_GetError());
        break;

    case SDL_DIALOGRESULT_CANCEL:
        SDL_Log("Cancel\n");
        break;

    case SDL_DIALOGRESULT_SUCCESS:
        SDL_Log("%04d-%02d-%02d\n", d.y, d.m, d.d);
        break;

    default:
        SDL_Log("Unknown result: %d\n", result);
    }
}

int main(int argc, char *argv[])
{
    SDL_Window *w;
    SDL_Renderer *r;
    SDLTest_CommonState *state;
    const SDL_FRect open_file_rect = { 50, 50, 220, 70 };
    const SDL_FRect save_file_rect = { 370, 50, 220, 70 };
    const SDL_FRect open_folder_rect = { 50, 140, 220, 70 };
    const SDL_FRect input_rect = { 370, 140, 220, 70 };
    const SDL_FRect progress_rect = { 50, 230, 220, 70 };
    const SDL_FRect color_rect = { 370, 230, 220, 70 };
    const SDL_FRect date_rect = { 50, 320, 220, 70 };
    int i;
    const char *initial_path = NULL;
    SDL_ProgressDialog* progress_dialog = NULL;
    int progress;

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
                    SDL_ShowOpenFileDialog(file_callback, NULL, w, filters, SDL_arraysize(filters), initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &open_folder_rect)) {
                    SDL_ShowOpenFolderDialog(file_callback, NULL, w, initial_path, 1);
                } else if (SDL_PointInRectFloat(&p, &save_file_rect)) {
                    SDL_ShowSaveFileDialog(file_callback, NULL, w, filters, SDL_arraysize(filters), initial_path);
                } else if (SDL_PointInRectFloat(&p, &input_rect)) {
                    SDL_PropertiesID props = SDL_CreateProperties();
                    SDL_SetPointerProperty(props, SDL_PROP_INPUT_DIALOG_WINDOW_POINTER, w);
                    SDL_ShowInputDialogWithProperties(input_callback, NULL, props);
                    SDL_DestroyProperties(props);
                } else if (SDL_PointInRectFloat(&p, &progress_rect)) {
                    if (progress_dialog) {
                        continue;
                    }
                    SDL_PropertiesID props = SDL_CreateProperties();
                    SDL_SetPointerProperty(props, SDL_PROP_PROGRESS_DIALOG_WINDOW_POINTER, w);
                    progress_dialog = SDL_ShowProgressDialogWithProperties(progress_callback, NULL, props);
                    SDL_DestroyProperties(props);
                    progress = 0;
                    progress_done = false;
                    /* Check creating and immediately destroying a progress dialog */
                } else if (SDL_PointInRectFloat(&p, &color_rect)) {
                    SDL_PropertiesID props = SDL_CreateProperties();
                    SDL_SetPointerProperty(props, SDL_PROP_COLOR_DIALOG_WINDOW_POINTER, w);
                    SDL_ShowColorPickerDialogWithProperties(color_callback, NULL, props);
                    SDL_DestroyProperties(props);
                } else if (SDL_PointInRectFloat(&p, &date_rect)) {
                    SDL_PropertiesID props = SDL_CreateProperties();
                    SDL_SetPointerProperty(props, SDL_PROP_DATE_DIALOG_WINDOW_POINTER, w);
                    SDL_ShowDatePickerDialogWithProperties(date_callback, NULL, props);
                    SDL_DestroyProperties(props);
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

        SDL_SetRenderDrawColor(r, 255, 255, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &input_rect);

        SDL_SetRenderDrawColor(r, 255, 0, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &progress_rect);

        SDL_SetRenderDrawColor(r, 0, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &color_rect);

        SDL_SetRenderDrawColor(r, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(r, &date_rect);

        SDL_SetRenderDrawColor(r, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDLTest_DrawString(r, open_file_rect.x+5, open_file_rect.y+open_file_rect.h/2, "Open File...");
        SDLTest_DrawString(r, save_file_rect.x+5, save_file_rect.y+save_file_rect.h/2, "Save File...");
        SDLTest_DrawString(r, open_folder_rect.x+5, open_folder_rect.y+open_folder_rect.h/2, "Open Folder...");
        SDLTest_DrawString(r, input_rect.x+5, input_rect.y+input_rect.h/2, "Input test...");
        SDLTest_DrawString(r, progress_rect.x+5, progress_rect.y+progress_rect.h/2, "Progress...");
        SDLTest_DrawString(r, color_rect.x+5, color_rect.y+color_rect.h/2, "Choose color...");
        SDLTest_DrawString(r, date_rect.x+5, date_rect.y+date_rect.h/2, "Choose date...");

        SDL_RenderPresent(r);

        if (progress_dialog) {
            if (progress_done) {
                SDL_DestroyProgressDialog(progress_dialog);
                progress_dialog = NULL;
            } else if (progress < 30) {
                SDL_UpdateProgressDialog(progress_dialog, ((float) ++progress) / 30.0f, NULL);
            }
        }
    }

    SDLTest_CleanupTextDrawing();
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
