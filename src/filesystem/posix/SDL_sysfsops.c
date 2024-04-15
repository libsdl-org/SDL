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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include "../SDL_sysfilesystem.h"

int SDL_SYS_EnumerateDirectory(const char *path, const char *dirname, SDL_EnumerateDirectoryCallback cb, void *userdata)
{
    int retval = 1;

    DIR *dir = opendir(path);
    if (!dir) {
        return SDL_SetError("Can't open directory: %s", strerror(errno));
    }

    struct dirent *ent;
    while ((retval == 1) && ((ent = readdir(dir)) != NULL))
    {
        const char *name = ent->d_name;
        if ((SDL_strcmp(name, ".") == 0) || (SDL_strcmp(name, "..") == 0)) {
            continue;
        }
        retval = cb(userdata, dirname, name);
    }

    closedir(dir);

    return retval;
}

int SDL_SYS_RemovePath(const char *path)
{
    int rc = remove(path);
    if (rc < 0) {
        if (errno == ENOENT) {
            // It's already gone, this is a success
            return 0;
        }
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return 0;
}

int SDL_SYS_RenamePath(const char *oldpath, const char *newpath)
{
    if (rename(oldpath, newpath) < 0) {
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return 0;
}

int SDL_SYS_CreateDirectory(const char *path)
{
    const int rc = mkdir(path, 0770);
    if (rc < 0) {
        const int origerrno = errno;
        if (origerrno == EEXIST) {
            struct stat statbuf;
            if ((stat(path, &statbuf) == 0) && (S_ISDIR(statbuf.st_mode))) {
                return 0;  // it already exists and it's a directory, consider it success.
            }
        }
        return SDL_SetError("Can't create directory: %s", strerror(origerrno));
    }
    return 0;
}

int SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info)
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
    /* POSIX.1-2008 standard */
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
    return 0;
}

#endif // SDL_FSOPS_POSIX

