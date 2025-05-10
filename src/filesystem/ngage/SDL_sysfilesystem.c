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

extern void NGAGE_GetAppPath(char* path);

char *SDL_SYS_GetBasePath(void)
{
    char app_path[512];
    NGAGE_GetAppPath(app_path);
    char *base_path = SDL_strdup(app_path);
    return base_path;
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    char *pref_path;
    if (SDL_asprintf(&pref_path, "C:/System/Apps/%s/%s/", org, app) < 0)
        return NULL;
    else
        return pref_path;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    const char *folder_path = NULL;
    switch (folder)
    {
        case SDL_FOLDER_HOME:
            folder_path = "C:/";
            break;
        case SDL_FOLDER_PICTURES:
            folder_path = "C:/Nokia/Pictures/";
            break;
        case SDL_FOLDER_SAVEDGAMES:
            folder_path = "C:/";
            break;
        case SDL_FOLDER_SCREENSHOTS:
            folder_path = "C:/Nokia/Pictures/";
            break;
        case SDL_FOLDER_VIDEOS:
            folder_path = "C:/Nokia/Videos/";
            break;
        default:
            folder_path = "C:/Nokia/Others/";
            break;
    }
    return SDL_strdup(folder_path);
}
