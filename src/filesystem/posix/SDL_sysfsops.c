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

int SDL_SYS_FSenumerate(SDL_FSops *fs, const char *fullpath, const char *dirname, SDL_EnumerateCallback cb, void *userdata)
{
    int retval = 1;

    DIR *dir = opendir(fullpath);
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
        retval = cb(userdata, fs, dirname, name);
    }

    closedir(dir);

    return retval;
}

int SDL_SYS_FSremove(SDL_FSops *fs, const char *fullpath)
{
    int rc = remove(fullpath);
    if (rc < 0) {
        const int origerrno = errno;
        if (origerrno == ENOENT) {
            char *parent = SDL_strdup(fullpath);
            if (!parent) {
                return -1;
            }
            char *ptr = SDL_strrchr(parent, '/');
            if (ptr) {
                *ptr = '\0';  // chop off thing we were removing, see if parent is there.
                struct stat statbuf;
                rc = stat(parent, &statbuf);
                if (rc == 0) {
                    SDL_free(parent);
                    return 0;  // it's already gone, and parent exists, consider it success.
                }
            }
            SDL_free(parent);
        }
        return SDL_SetError("Can't remove path: %s", strerror(origerrno));
    }
    return 0;
}

int SDL_SYS_FSrename(SDL_FSops *fs, const char *oldfullpath, const char *newfullpath)
{
    if (rename(oldfullpath, newfullpath) < 0) {
        return SDL_SetError("Can't remove path: %s", strerror(errno));
    }
    return 0;
}

int SDL_SYS_FSmkdir(SDL_FSops *fs, const char *fullpath)
{
    const int rc = mkdir(fullpath, 0770);
    if (rc < 0) {
        const int origerrno = errno;
        if (origerrno == EEXIST) {
            struct stat statbuf;
            if ((stat(fullpath, &statbuf) == 0) && (S_ISDIR(statbuf.st_mode))) {
                return 0;  // it already exists and it's a directory, consider it success.
            }
        }
        return SDL_SetError("Can't create directory: %s", strerror(origerrno));
    }
    return 0;
}

int SDL_SYS_FSstat(SDL_FSops *fs, const char *fullpath, SDL_Stat *st)
{
    struct stat statbuf;
    const int rc = stat(fullpath, &statbuf);
    if (rc < 0) {
        return SDL_SetError("Can't stat: %s", strerror(errno));
    } else if (S_ISREG(statbuf.st_mode)) {
        st->filetype = SDL_STATPATHTYPE_FILE;
        st->filesize = (Uint64) statbuf.st_size;
    } else if (S_ISDIR(statbuf.st_mode)) {
        st->filetype = SDL_STATPATHTYPE_DIRECTORY;
        st->filesize = 0;
    } else {
        st->filetype = SDL_STATPATHTYPE_OTHER;
        st->filesize = (Uint64) statbuf.st_size;
    }

    // SDL file time is seconds since the Unix epoch, so we're already good here.
    st->modtime = (Uint64) statbuf.st_mtime;
    st->createtime = (Uint64) statbuf.st_ctime;
    st->accesstime = (Uint64) statbuf.st_atime;

    return 0;
}

#endif

