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

#if defined(SDL_FSOPS_WINDOWS)

#include "../../core/windows/SDL_windows.h"
#include "../SDL_sysfilesystem.h"

int SDL_SYS_EnumerateDirectory(const char *path, const char *dirname, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    int retval = 1;
    if (*path == '\0') {  // if empty (completely at the root), we need to enumerate drive letters.
        const DWORD drives = GetLogicalDrives();
        char name[3] = { 0, ':', '\0' };
        for (int i = 'A'; (retval == 1) && (i <= 'Z'); i++) {
            if (drives & (1 << (i - 'A'))) {
                name[0] = (char) i;
                retval = cb(userdata, dirname, name);
            }
        }
    } else {
        const size_t patternlen = SDL_strlen(path) + 3;
        char *pattern = (char *) SDL_malloc(patternlen);
        if (!pattern) {
            return -1;
        }

        // you need a wildcard to enumerate through FindFirstFileEx(), but the wildcard is only checked in the
        // filename element at the end of the path string, so always tack on a "\\*" to get everything, and
        // also prevent any wildcards inserted by the app from being respected.
        SDL_snprintf(pattern, patternlen, "%s\\*", path);

        WCHAR *wpattern = WIN_UTF8ToString(pattern);
        SDL_free(pattern);
        if (!wpattern) {
            return -1;
        }

        WIN32_FIND_DATAW entw;
        HANDLE dir = FindFirstFileExW(wpattern, FindExInfoStandard, &entw, FindExSearchNameMatch, NULL, 0);
        SDL_free(wpattern);
        if (dir == INVALID_HANDLE_VALUE) {
            return WIN_SetError("Failed to enumerate directory");
        }

        do {
            const WCHAR *fn = entw.cFileName;

            if (fn[0] == '.') {  // ignore "." and ".."
                if ((fn[1] == '\0') || ((fn[1] == '.') && (fn[2] == '\0'))) {
                    continue;
                }
            }

            char *utf8fn = WIN_StringToUTF8(fn);
            if (!utf8fn) {
                retval = -1;
            } else {
                retval = cb(userdata, dirname, utf8fn);
                SDL_free(utf8fn);
            }
        } while ((retval == 1) && (FindNextFileW(dir, &entw) != 0));

        FindClose(dir);
    }

    return retval;
}

int SDL_SYS_RemovePath(const char *path)
{
    WCHAR *wpath = WIN_UTF8ToString(path);
    if (!wpath) {
        return -1;
    }

    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &info)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            // Note that ERROR_PATH_NOT_FOUND means a parent dir is missing, and we consider that an error.
            return 0;  // thing is already gone, call it a success.
        }
        return WIN_SetError("Couldn't get path's attributes");
    }

    const int isdir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    const BOOL rc = isdir ? RemoveDirectoryW(wpath) : DeleteFileW(wpath);
    SDL_free(wpath);
    return !rc ? WIN_SetError("Couldn't remove path") : 0;
}

int SDL_SYS_RenamePath(const char *oldpath, const char *newpath)
{
    WCHAR *woldpath = WIN_UTF8ToString(oldpath);
    if (!woldpath) {
        return -1;
    }

    WCHAR *wnewpath = WIN_UTF8ToString(newpath);
    if (!wnewpath) {
        SDL_free(woldpath);
        return -1;
    }

    const BOOL rc = MoveFileExW(woldpath, wnewpath, MOVEFILE_REPLACE_EXISTING);
    SDL_free(wnewpath);
    SDL_free(woldpath);
    return !rc ? WIN_SetError("Couldn't rename path") : 0;
}

int SDL_SYS_CreateDirectory(const char *path)
{
    WCHAR *wpath = WIN_UTF8ToString(path);
    if (!wpath) {
        return -1;
    }

    const DWORD rc = CreateDirectoryW(wpath, NULL);
    SDL_free(wpath);
    return !rc ? WIN_SetError("Couldn't create directory") : 0;
}

int SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    WCHAR *wpath = WIN_UTF8ToString(path);
    if (!wpath) {
        return -1;
    }

    WIN32_FILE_ATTRIBUTE_DATA winstat;
    const BOOL rc = GetFileAttributesExW(wpath, GetFileExInfoStandard, &winstat);
    SDL_free(wpath);
    if (!rc) {
        return WIN_SetError("Can't stat");
    }

    if (winstat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = SDL_PATHTYPE_DIRECTORY;
        info->size = 0;
    } else if (winstat.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_DEVICE)) {
        info->type = SDL_PATHTYPE_OTHER;
        info->size = ((((Uint64) winstat.nFileSizeHigh) << 32) | winstat.nFileSizeLow);
    } else {
        info->type = SDL_PATHTYPE_FILE;
        info->size = ((((Uint64) winstat.nFileSizeHigh) << 32) | winstat.nFileSizeLow);
    }

    info->create_time = SDL_TimeFromWindows(winstat.ftCreationTime.dwLowDateTime, winstat.ftCreationTime.dwHighDateTime);
    info->modify_time = SDL_TimeFromWindows(winstat.ftLastWriteTime.dwLowDateTime, winstat.ftLastWriteTime.dwHighDateTime);
    info->access_time = SDL_TimeFromWindows(winstat.ftLastAccessTime.dwLowDateTime, winstat.ftLastAccessTime.dwHighDateTime);

    return 0;
}

#endif // SDL_FSOPS_WINDOWS

