/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_FILESYSTEM_WINDOWS

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include "../../core/windows/SDL_windows.h"
#include <errhandlingapi.h>
#include <fileapi.h>
#include <shlobj.h>
#include <libloaderapi.h>
/* Lowercase is necessary for Wine */
#include <knownfolders.h>
#include <initguid.h>
#include <windows.h>

char *
SDL_GetBasePath(void)
{
    DWORD buflen = 128;
    WCHAR *path = NULL;
    char *retval = NULL;
    DWORD len = 0;
    int i;

    while (SDL_TRUE) {
        void *ptr = SDL_realloc(path, buflen * sizeof(WCHAR));
        if (ptr == NULL) {
            SDL_free(path);
            SDL_OutOfMemory();
            return NULL;
        }

        path = (WCHAR *)ptr;

        len = GetModuleFileNameW(NULL, path, buflen);
        /* if it truncated, then len >= buflen - 1 */
        /* if there was enough room (or failure), len < buflen - 1 */
        if (len < buflen - 1) {
            break;
        }

        /* buffer too small? Try again. */
        buflen *= 2;
    }

    if (len == 0) {
        SDL_free(path);
        WIN_SetError("Couldn't locate our .exe");
        return NULL;
    }

    for (i = len - 1; i > 0; i--) {
        if (path[i] == '\\') {
            break;
        }
    }

    SDL_assert(i > 0);  /* Should have been an absolute path. */
    path[i + 1] = '\0'; /* chop off filename. */

    retval = WIN_StringToUTF8W(path);
    SDL_free(path);

    return retval;
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    /*
     * Vista and later has a new API for this, but SHGetFolderPath works there,
     *  and apparently just wraps the new API. This is the new way to do it:
     *
     *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
     *                          NULL, &wszPath);
     */

    WCHAR path[MAX_PATH];
    char *retval = NULL;
    WCHAR *worg = NULL;
    WCHAR *wapp = NULL;
    size_t new_wpath_len = 0;
    BOOL api_result = FALSE;

    if (app == NULL) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (org == NULL) {
        org = "";
    }

    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) {
        WIN_SetError("Couldn't locate our prefpath");
        return NULL;
    }

    worg = WIN_UTF8ToStringW(org);
    if (worg == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    wapp = WIN_UTF8ToStringW(app);
    if (wapp == NULL) {
        SDL_free(worg);
        SDL_OutOfMemory();
        return NULL;
    }

    new_wpath_len = SDL_wcslen(worg) + SDL_wcslen(wapp) + SDL_wcslen(path) + 3;

    if ((new_wpath_len + 1) > MAX_PATH) {
        SDL_free(worg);
        SDL_free(wapp);
        WIN_SetError("Path too long.");
        return NULL;
    }

    if (*worg) {
        SDL_wcslcat(path, L"\\", SDL_arraysize(path));
        SDL_wcslcat(path, worg, SDL_arraysize(path));
    }
    SDL_free(worg);

    api_result = CreateDirectoryW(path, NULL);
    if (api_result == FALSE) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            SDL_free(wapp);
            WIN_SetError("Couldn't create a prefpath.");
            return NULL;
        }
    }

    SDL_wcslcat(path, L"\\", SDL_arraysize(path));
    SDL_wcslcat(path, wapp, SDL_arraysize(path));
    SDL_free(wapp);

    api_result = CreateDirectoryW(path, NULL);
    if (api_result == FALSE) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            WIN_SetError("Couldn't create a prefpath.");
            return NULL;
        }
    }

    SDL_wcslcat(path, L"\\", SDL_arraysize(path));

    retval = WIN_StringToUTF8W(path);

    return retval;
}

char *
SDL_GetPath(SDL_Folder folder)
{
    typedef HRESULT (*SHGKFP)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR *);
    char *retval;
    HMODULE lib = LoadLibrary(L"Shell32.dll");
    SHGKFP SHGetKnownFolderPath_ = (SHGKFP) GetProcAddress(lib,
                                                       "SHGetKnownFolderPath");

    if (!SHGetKnownFolderPath_)
    {
        int type;
        HRESULT result;
    	wchar_t path[MAX_PATH];

        switch (folder)
        {
            case SDL_FOLDER_HOME:
                type = CSIDL_PROFILE;
                break;

            case SDL_FOLDER_DESKTOP:
                type = CSIDL_DESKTOP;
                break;

            case SDL_FOLDER_DOCUMENTS:
                type = CSIDL_MYDOCUMENTS;
                break;

            case SDL_FOLDER_DOWNLOADS:
                SDL_SetError("Downloads folder unavailable before Vista");
                return NULL;

            case SDL_FOLDER_MUSIC:
                type = CSIDL_MYMUSIC;
                break;

            case SDL_FOLDER_PICTURES:
                type = CSIDL_MYPICTURES;
                break;

            case SDL_FOLDER_PUBLICSHARE:
                SDL_SetError("Public share unavailable on Windows");
                return NULL;

            case SDL_FOLDER_SAVEDGAMES:
                SDL_SetError("Saved games unavailable before Vista");
                return NULL;

            case SDL_FOLDER_SCREENSHOTS:
                SDL_SetError("Screenshots folder unavailable before Vista");
                return NULL;

            case SDL_FOLDER_TEMPLATES:
                type = CSIDL_TEMPLATES;
                break;

            case SDL_FOLDER_VIDEOS:
                type = CSIDL_MYVIDEO;
                break;

            default:
                SDL_SetError("Unsupported SDL_Folder on Windows before Vista: %d",
                              (int) folder);
                return NULL;
        };

        /* Create the OS-specific folder if it doesn't already exist */
        type |= CSIDL_FLAG_CREATE;

#if 0
        /* Apparently the oldest, but not supported in modern Windows */
        HRESULT result = SHGetSpecialFolderPath(NULL, path, type, TRUE);
#endif

        /* Windows 2000/XP and later, deprecated as of Windows 10 (still
           available), available in Wine (tested 6.0.3) */
        result = SHGetFolderPathW(NULL, type, NULL, SHGFP_TYPE_CURRENT, path);

        /* use `!= TRUE` for SHGetSpecialFolderPath */
        if (result != S_OK)
        {
            SDL_SetError("Couldn't get folder, windows-specific error: %ld",
                         result);
            return NULL;
        }

        retval = (char *) SDL_malloc((SDL_wcslen(path) + 1) * 2);
        if (retval == NULL)
        {
            SDL_OutOfMemory();
            return NULL;
        }
        retval = WIN_StringToUTF8W(path);
        return retval;
    }
    else
    {
        KNOWNFOLDERID type;
        HRESULT result;
    	wchar_t *path;

        switch (folder)
        {
            case SDL_FOLDER_HOME:
                type = FOLDERID_Profile;
                break;

            case SDL_FOLDER_DESKTOP:
                type = FOLDERID_Desktop;
                break;

            case SDL_FOLDER_DOCUMENTS:
                type = FOLDERID_Documents;
                break;

            case SDL_FOLDER_DOWNLOADS:
                type = FOLDERID_Downloads;
                break;

            case SDL_FOLDER_MUSIC:
                type = FOLDERID_Music;
                break;

            case SDL_FOLDER_PICTURES:
                type = FOLDERID_Pictures;
                break;

            case SDL_FOLDER_PUBLICSHARE:
                SDL_SetError("Public share unavailable on Windows");
                return NULL;

            case SDL_FOLDER_SAVEDGAMES:
                type = FOLDERID_SavedGames;
                break;

            case SDL_FOLDER_SCREENSHOTS:
                type = FOLDERID_Screenshots;
                break;

            case SDL_FOLDER_TEMPLATES:
                type = FOLDERID_Templates;
                break;

            case SDL_FOLDER_VIDEOS:
                type = FOLDERID_Videos;
                break;

            default:
                SDL_SetError("Invalid SDL_Folder: %d", (int) folder);
                return NULL;
        };

        result = SHGetKnownFolderPath_(&type, KF_FLAG_CREATE, NULL, &path);

        if (result != S_OK)
        {
            SDL_SetError("Couldn't get folder, windows-specific error: %ld",
                         result);
            return NULL;
        }

        retval = (char *) SDL_malloc((SDL_wcslen(path) + 1) * 2);
        if (retval == NULL)
        {
            SDL_OutOfMemory();
            return NULL;
        }
        retval = WIN_StringToUTF8W(path);
        return retval;
    }
}

#endif /* SDL_FILESYSTEM_WINDOWS */

#ifdef SDL_FILESYSTEM_XBOX
char *
SDL_GetBasePath(void)
{
    SDL_Unsupported();
    return NULL;
}

char *
SDL_GetPrefPath(const char *org, const char *app)
{
    SDL_Unsupported();
    return NULL;
}

char *
SDL_GetPath(SDL_Folder folder)
{
    SDL_Unsupported();
    return NULL;
}
#endif /* SDL_FILESYSTEM_XBOX */
