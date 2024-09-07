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

#if defined(SDL_FSOPS_POSIX)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

int SDL_SYS_EnumerateDirectory(const char *path, const char *dirname, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    int result = 1;

    DIR *dir = opendir(path);
    if (!dir) {
        SDL_SetError("Can't open directory: %s", strerror(errno));
        return -1;
    }

    struct dirent *ent;
    while ((result == 1) && ((ent = readdir(dir)) != NULL))
    {
        const char *name = ent->d_name;
        if ((SDL_strcmp(name, ".") == 0) || (SDL_strcmp(name, "..") == 0)) {
            continue;
        }
        result = cb(userdata, dirname, name);
    }

    closedir(dir);

    return result;
}

bool SDL_SYS_RemovePath(const char *path)
{
    int rc = remove(path);
    if (rc < 0) {
        if (errno == ENOENT) {
            // It's already gone, this is a success
            return true;
        }
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return true;
}

bool SDL_SYS_RenamePath(const char *oldpath, const char *newpath)
{
    if (rename(oldpath, newpath) < 0) {
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return true;
}

bool SDL_SYS_CopyFile(const char *oldpath, const char *newpath)
{
    char *buffer = NULL;
    char *tmppath = NULL;
    SDL_IOStream *input = NULL;
    SDL_IOStream *output = NULL;
    const size_t maxlen = 4096;
    size_t len;
    bool result = false;

    if (SDL_asprintf(&tmppath, "%s.tmp", newpath) < 0) {
        goto done;
    }

    input = SDL_IOFromFile(oldpath, "rb");
    if (!input) {
        goto done;
    }

    output = SDL_IOFromFile(tmppath, "wb");
    if (!output) {
        goto done;
    }

    buffer = (char *)SDL_malloc(maxlen);
    if (!buffer) {
        goto done;
    }

    while ((len = SDL_ReadIO(input, buffer, maxlen)) > 0) {
        if (SDL_WriteIO(output, buffer, len) < len) {
            goto done;
        }
    }
    if (SDL_GetIOStatus(input) != SDL_IO_STATUS_EOF) {
        goto done;
    }

    SDL_CloseIO(input);
    input = NULL;

    if (!SDL_CloseIO(output)) {
        goto done;
    }
    output = NULL;

    if (!SDL_RenamePath(tmppath, newpath)) {
        SDL_RemovePath(tmppath);
        goto done;
    }

    result = true;

done:
    if (output) {
        SDL_CloseIO(output);
        SDL_RemovePath(tmppath);
    }
    if (input) {
        SDL_CloseIO(input);
    }
    SDL_free(tmppath);
    SDL_free(buffer);

    return result;
}

bool SDL_SYS_CreateDirectory(const char *path)
{
    const int rc = mkdir(path, 0770);
    if (rc < 0) {
        const int origerrno = errno;
        if (origerrno == EEXIST) {
            struct stat statbuf;
            if ((stat(path, &statbuf) == 0) && (S_ISDIR(statbuf.st_mode))) {
                return true;  // it already exists and it's a directory, consider it success.
            }
        }
        return SDL_SetError("Can't create directory: %s", strerror(origerrno));
    }
    return true;
}

bool SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info)
{
    struct stat statbuf;
    const int rc = stat(path, &statbuf);
    if (rc < 0) {
        return SDL_SetError("Can't stat: %s", strerror(errno));
    } else if (S_ISREG(statbuf.st_mode)) {
        info->type = SDL_PATHTYPE_FILE;
        info->size = (Uint64) statbuf.st_size;
    } else if (S_ISDIR(statbuf.st_mode)) {
        info->type = SDL_PATHTYPE_DIRECTORY;
        info->size = 0;
    } else {
        info->type = SDL_PATHTYPE_OTHER;
        info->size = (Uint64) statbuf.st_size;
    }

#if defined(HAVE_ST_MTIM)
    // POSIX.1-2008 standard
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctim.tv_sec) + statbuf.st_ctim.tv_nsec;
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtim.tv_sec) + statbuf.st_mtim.tv_nsec;
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atim.tv_sec) + statbuf.st_atim.tv_nsec;
#elif defined(SDL_PLATFORM_APPLE)
    /* Apple platform stat structs use 'st_*timespec' naming. */
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctimespec.tv_sec) + statbuf.st_ctimespec.tv_nsec;
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtimespec.tv_sec) + statbuf.st_mtimespec.tv_nsec;
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atimespec.tv_sec) + statbuf.st_atimespec.tv_nsec;
#else
    info->create_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_ctime);
    info->modify_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_mtime);
    info->access_time = (SDL_Time)SDL_SECONDS_TO_NS(statbuf.st_atime);
#endif
    return true;
}

#endif // SDL_FSOPS_POSIX

