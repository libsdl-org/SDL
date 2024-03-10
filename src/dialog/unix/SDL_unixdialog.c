/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#include "./SDL_portaldialog.h"
#include "./SDL_zenitydialog.h"

static void (*detected_open)(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location, SDL_bool allow_many) = NULL;
static void (*detected_save)(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location) = NULL;
static void (*detected_folder)(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const char* default_location, SDL_bool allow_many) = NULL;

/* Returns non-zero on success, 0 on failure */
static int detect_available_methods(void)
{
    if (SDL_Portal_detect()) {
        detected_open = SDL_Portal_ShowOpenFileDialog;
        detected_save = SDL_Portal_ShowSaveFileDialog;
        detected_folder = SDL_Portal_ShowOpenFolderDialog;
        return 1;
    }

    if (SDL_Zenity_detect()) {
        detected_open = SDL_Zenity_ShowOpenFileDialog;
        detected_save = SDL_Zenity_ShowSaveFileDialog;
        detected_folder = SDL_Zenity_ShowOpenFolderDialog;
        return 2;
    }

    SDL_SetError("No supported method for file dialogs");
    return 0;
}

void SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location, SDL_bool allow_many)
{
    /* Call detect_available_methods() again each time in case the situation changed */
    if (!detected_open && !detect_available_methods()) {
        /* SetError() done by detect_available_methods() */
        callback(userdata, NULL, -1);
        return;
    }

    detected_open(callback, userdata, window, filters, default_location, allow_many);
}

void SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location)
{
    /* Call detect_available_methods() again each time in case the situation changed */
    if (!detected_save && !detect_available_methods()) {
        /* SetError() done by detect_available_methods() */
        callback(userdata, NULL, -1);
        return;
    }

    detected_save(callback, userdata, window, filters, default_location);
}

void SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const char* default_location, SDL_bool allow_many)
{
    /* Call detect_available_methods() again each time in case the situation changed */
    if (!detected_folder && !detect_available_methods()) {
        /* SetError() done by detect_available_methods() */
        callback(userdata, NULL, -1);
        return;
    }

    detected_folder(callback, userdata, window, default_location, allow_many);
}
