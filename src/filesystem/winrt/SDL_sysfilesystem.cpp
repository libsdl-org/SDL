/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

/* TODO, WinRT: include copyright info in SDL_winrtpaths.cpp
   TODO, WinRT: remove the need to compile this with C++/CX (/ZW) extensions, and if possible, without C++ at all
*/

#ifdef __WINRT__

extern "C" {
#include "SDL_filesystem.h"
#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "SDL_system.h"
#include "../../core/windows/SDL_windows.h"
}

#include <string>
#include <unordered_map>

using namespace std;
using namespace Windows::Storage;

extern "C" const wchar_t *
SDL_WinRTGetFSPathUNICODE(SDL_WinRT_Path pathType)
{
    switch (pathType) {
        case SDL_WINRT_PATH_INSTALLED_LOCATION:
        {
            static wstring path;
            if (path.empty()) {
                path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
            }
            return path.c_str();
        }

        case SDL_WINRT_PATH_LOCAL_FOLDER:
        {
            static wstring path;
            if (path.empty()) {
                path = ApplicationData::Current->LocalFolder->Path->Data();
            }
            return path.c_str();
        }

#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
        case SDL_WINRT_PATH_ROAMING_FOLDER:
        {
            static wstring path;
            if (path.empty()) {
                path = ApplicationData::Current->RoamingFolder->Path->Data();
            }
            return path.c_str();
        }

        case SDL_WINRT_PATH_TEMP_FOLDER:
        {
            static wstring path;
            if (path.empty()) {
                path = ApplicationData::Current->TemporaryFolder->Path->Data();
            }
            return path.c_str();
        }
#endif

        default:
            break;
    }

    SDL_Unsupported();
    return NULL;
}

extern "C" const char *
SDL_WinRTGetFSPathUTF8(SDL_WinRT_Path pathType)
{
    typedef unordered_map<SDL_WinRT_Path, string> UTF8PathMap;
    static UTF8PathMap utf8Paths;

    UTF8PathMap::iterator searchResult = utf8Paths.find(pathType);
    if (searchResult != utf8Paths.end()) {
        return searchResult->second.c_str();
    }

    const wchar_t * ucs2Path = SDL_WinRTGetFSPathUNICODE(pathType);
    if (!ucs2Path) {
        return NULL;
    }

    char * utf8Path = WIN_StringToUTF8(ucs2Path);
    utf8Paths[pathType] = utf8Path;
    SDL_free(utf8Path);
    return utf8Paths[pathType].c_str();
}

extern "C" char *
SDL_GetBasePath(void)
{
    const char * srcPath = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_INSTALLED_LOCATION);
    size_t destPathLen;
    char * destPath = NULL;

    if (!srcPath) {
        SDL_SetError("Couldn't locate our basepath: %s", SDL_GetError());
        return NULL;
    }

    destPathLen = SDL_strlen(srcPath) + 2;
    destPath = (char *) SDL_malloc(destPathLen);
    if (!destPath) {
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_snprintf(destPath, destPathLen, "%s\\", srcPath);
    return destPath;
}

extern "C" char *
SDL_GetPrefPath(const char *org, const char *app)
{
    /* WinRT note: The 'SHGetFolderPath' API that is used in Windows 7 and
     * earlier is not available on WinRT or Windows Phone.  WinRT provides
     * a similar API, but SHGetFolderPath can't be called, at least not
     * without violating Microsoft's app-store requirements.
     */

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    /* A 'Roaming' folder is not available in Windows Phone 8, however a 'Local' folder is. */
    const char * srcPath = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_LOCAL_FOLDER);
#else
    /* A 'Roaming' folder is available on Windows 8 and 8.1.  Use that. */
    const char * srcPath = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_ROAMING_FOLDER);
#endif

    size_t destPathLen;
    char * destPath = NULL;

    if (!srcPath) {
        SDL_SetError("Couldn't locate our basepath: %s", SDL_GetError());
        return NULL;
    }

    destPathLen = SDL_strlen(srcPath) + SDL_strlen(org) + SDL_strlen(app) + 4;
    destPath = (char *) SDL_malloc(destPathLen);
    if (!destPath) {
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_snprintf(destPath, destPathLen, "%s\\%s\\%s\\", srcPath, org, app);
    return destPath;
}

#endif /* __WINRT__ */
