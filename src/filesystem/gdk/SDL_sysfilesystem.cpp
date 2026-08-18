/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

extern "C" {
#include "../SDL_sysfilesystem.h"
}

#include "../../core/windows/SDL_windows.h"
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_system.h>
#include <SDL3/SDL_filesystem.h>
#include <XGameSaveFiles.h>

char *SDL_SYS_GetBasePath(void)
{
    char *path = WIN_GetModulePath(NULL);  // look up full path of the current process's EXE file.
    if (!path) {
        return NULL;  // error message was already set.
    }

    char *ptr = SDL_strrchr(path, '\\');
    SDL_assert(ptr != NULL);  // Should have been an absolute path.

    ptr[1] = '\0'; // chop off filename, leave '\\'.

    ptr = (char *) SDL_realloc(path, ((size_t) (ptr - path)) + 2);  // try to shrink this allocation down a little.
    return ptr ? ptr : path;  // return shrunk buffer if shrink worked out, unchanged original buffer if not.
}

char *SDL_SYS_GetExeName(void)
{
    char *path = WIN_GetModulePath(NULL);  // look up full path of the current process's EXE file.
    if (!path) {
        return NULL;  // error message was already set.
    }

    char *ptr = SDL_strrchr(path, '\\');
    const size_t slen = SDL_strlen(ptr);  // counts null terminator because we're still sitting on path separator.
    SDL_memmove(path, ptr + 1, slen);  // move filename string to start of SDL_realloc'd region.
    ptr = (char *) SDL_realloc(path, slen);  // try to shrink this allocation down a little.
    return ptr ? ptr : path;  // return shrunk buffer if shrink worked out, unchanged original buffer if not.
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    XUserHandle user = NULL;
    XAsyncBlock block = { 0 };
    char *folderPath;
    HRESULT result;
    const char *csid = SDL_GetHint("SDL_GDK_SERVICE_CONFIGURATION_ID");

    // This should be set before calling SDL_GetPrefPath!
    if (!csid) {
        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "Set SDL_GDK_SERVICE_CONFIGURATION_ID before calling SDL_GetPrefPath!");
        return SDL_strdup("T:\\");
    }

    if (!SDL_GetGDKDefaultUser(&user)) {
        // Error already set, just return
        return NULL;
    }

    if (FAILED(result = XGameSaveFilesGetFolderWithUiAsync(user, csid, &block))) {
        WIN_SetErrorFromHRESULT("XGameSaveFilesGetFolderWithUiAsync", result);
        return NULL;
    }

    folderPath = (char *)SDL_malloc(MAX_PATH);
    do {
        result = XGameSaveFilesGetFolderWithUiResult(&block, MAX_PATH, folderPath);
    } while (result == E_PENDING);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT("XGameSaveFilesGetFolderWithUiResult", result);
        SDL_free(folderPath);
        return NULL;
    }

    /* We aren't using 'app' here because the container rules are a lot more
     * strict than the NTFS rules, so it will most likely be invalid :(
     */
    SDL_strlcat(folderPath, "\\SDLPrefPath\\", MAX_PATH);
    if (CreateDirectoryA(folderPath, NULL) == FALSE) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            WIN_SetError("CreateDirectoryA");
            SDL_free(folderPath);
            return NULL;
        }
    }
    return folderPath;
}

// TODO
char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    SDL_Unsupported();
    return NULL;
}

char *SDL_SYS_GetCurrentDirectory(void)
{
    const char *base = SDL_GetBasePath();
    if (!base) {
        return NULL;
    }

    return SDL_strdup(base);
}
