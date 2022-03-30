/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/* We only need to build syscalls for playdate and this seemed a logical place */

#include "../../SDL_internal.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#if defined(SDL_VIDEO_DRIVER_PLAYDATE) && !defined(TARGET_SIMULATOR)

#include "pd_api.h"

#define MAX_DESCRIPTORS         64
#define RESERVED_DESCRIPTORS    3

static FILE *file_descriptors[MAX_DESCRIPTORS];

/* noop - just a stub */
int
main(int argc, char *argv[])
{
    return 0;
}

#ifndef TARGET_SIMULATOR
extern int _start;

void* _sbrk(int incr)
{
    static unsigned char *heap = NULL;
    unsigned char *prev_heap;

    if (heap == NULL) {
        heap = (unsigned char *)&_start;
    }

    prev_heap = heap;
    heap += incr;

    return prev_heap;
}
#endif

extern int _write(SDFile *file, char *ptr, int len)
{
    return pd->file->write(file, ptr, len);
}

extern int _read(SDFile *file, char *ptr, int len)
{
    return pd->file->read(file, ptr, len);
}

extern SDFile* _open(const char *filename, const char *mode)
{
    return pd->file->open(filename, kFileRead|kFileReadData|kFileWrite|kFileAppend);
}

extern int _close(SDFile *file)
{
    return pd->file->close(file);
}

extern int _fstat(const char *file, struct stat *st)
{
    FileStat result;
    struct tm ltm;

    int rc = pd->file->stat(file, &result);

    if (!rc) {
        ltm.tm_sec = result.m_second;
        ltm.tm_min = result.m_minute;
        ltm.tm_hour = result.m_hour;
        ltm.tm_mday = result.m_day;
        ltm.tm_mon = result.m_month;
        ltm.tm_year = result.m_year;

        st->st_dev = 0;
        st->st_ino = 0;
        st->st_mode = 0;
        st->st_nlink = 0;
        st->st_uid = 0;
        st->st_gid = 0;
        st->st_rdev = 0;
        st->st_size = result.size;
        st->st_atime = st->st_mtime = st->st_ctime = mktime(&ltm);
        st->st_blksize = 0;
        st->st_blocks = 0;
    }

    return rc;
}

extern int _isatty(int file)
{
    if (file < RESERVED_DESCRIPTORS) {
        return 1;
    }
    if (file >= MAX_DESCRIPTORS || file_descriptors[file] == NULL) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

extern int _lseek(SDFile *file, int pos, int whence)
{
    return pd->file->seek(file, pos, whence);
}

extern void _exit(int code)
{
    while(1) { /* noop */ }
}

extern void* realloc(void* ptr, size_t size) {
    return pd->system->realloc(ptr, size);
}

extern void* malloc(size_t size) {
    return pd->system->realloc(NULL, size);
}

extern void* calloc(size_t count, size_t size) {
    return memset(malloc(count * size), 0, count * size);
}

extern void free(void* ptr) {
    if (realloc(ptr, 0)) { /* noop */ }
}
#endif