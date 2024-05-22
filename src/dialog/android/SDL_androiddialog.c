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
#include "../../core/android/SDL_android.h"

void SDLCALL SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location, SDL_bool allow_many)
{
    if (!Android_JNI_OpenFileDialog(callback, userdata, filters, nfilters, SDL_FALSE, allow_many)) {
        /* SDL_SetError is already called when it fails */
        callback(userdata, NULL, -1);
    }
}

void SDLCALL SDL_ShowSaveFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const SDL_DialogFileFilter *filters, int nfilters, const char *default_location)
{
    if (!Android_JNI_OpenFileDialog(callback, userdata, filters, nfilters, SDL_TRUE, SDL_FALSE)) {
        /* SDL_SetError is already called when it fails */
        callback(userdata, NULL, -1);
    }
}

void SDLCALL SDL_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window, const char *default_location, SDL_bool allow_many)
{
    SDL_Unsupported();
    callback(userdata, NULL, -1);
}
