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

#if defined(__WIN32__) || defined(__GDK__)
#include "../core/windows/SDL_windows.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* This file provides a general interface for SDL to read and write
   data sources.  It can easily be extended to files, memory, etc.
*/

#ifdef __APPLE__
#include "cocoa/SDL_rwopsbundlesupport.h"
#endif /* __APPLE__ */

#ifdef __3DS__
#include "n3ds/SDL_rwopsromfs.h"
#endif /* __3DS__ */

#ifdef __ANDROID__
#include "../core/android/SDL_android.h"
#endif

#if defined(__WIN32__) || defined(__GDK__)

/* Functions to read/write Win32 API file pointers */

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define READAHEAD_BUFFER_SIZE 1024

static int SDLCALL windows_file_open(SDL_RWops *context, const char *filename, const char *mode)
{
#if !defined(__XBOXONE__) && !defined(__XBOXSERIES__)
    UINT old_error_mode;
#endif
    HANDLE h;
    DWORD r_right, w_right;
    DWORD must_exist, truncate;
    int a_mode;

    if (context == NULL) {
        return -1; /* failed (invalid call) */
    }

    context->hidden.windowsio.h = INVALID_HANDLE_VALUE; /* mark this as unusable */
    context->hidden.windowsio.buffer.data = NULL;
    context->hidden.windowsio.buffer.size = 0;
    context->hidden.windowsio.buffer.left = 0;

    /* "r" = reading, file must exist */
    /* "w" = writing, truncate existing, file may not exist */
    /* "r+"= reading or writing, file must exist            */
    /* "a" = writing, append file may not exist             */
    /* "a+"= append + read, file may not exist              */
    /* "w+" = read, write, truncate. file may not exist    */

    must_exist = (SDL_strchr(mode, 'r') != NULL) ? OPEN_EXISTING : 0;
    truncate = (SDL_strchr(mode, 'w') != NULL) ? CREATE_ALWAYS : 0;
    r_right = (SDL_strchr(mode, '+') != NULL || must_exist) ? GENERIC_READ : 0;
    a_mode = (SDL_strchr(mode, 'a') != NULL) ? OPEN_ALWAYS : 0;
    w_right = (a_mode || SDL_strchr(mode, '+') || truncate) ? GENERIC_WRITE : 0;

    if (!r_right && !w_right) {
        return -1; /* inconsistent mode */
    }
    /* failed (invalid call) */

    context->hidden.windowsio.buffer.data =
        (char *)SDL_malloc(READAHEAD_BUFFER_SIZE);
    if (!context->hidden.windowsio.buffer.data) {
        return SDL_OutOfMemory();
    }
#if !defined(__XBOXONE__) && !defined(__XBOXSERIES__)
    /* Do not open a dialog box if failure */
    old_error_mode =
        SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
#endif

    {
        LPTSTR tstr = WIN_UTF8ToString(filename);
        h = CreateFile(tstr, (w_right | r_right),
                       (w_right) ? 0 : FILE_SHARE_READ, NULL,
                       (must_exist | truncate | a_mode),
                       FILE_ATTRIBUTE_NORMAL, NULL);
        SDL_free(tstr);
    }

#if !defined(__XBOXONE__) && !defined(__XBOXSERIES__)
    /* restore old behavior */
    SetErrorMode(old_error_mode);
#endif

    if (h == INVALID_HANDLE_VALUE) {
        SDL_free(context->hidden.windowsio.buffer.data);
        context->hidden.windowsio.buffer.data = NULL;
        SDL_SetError("Couldn't open %s", filename);
        return -2; /* failed (CreateFile) */
    }
    context->hidden.windowsio.h = h;
    context->hidden.windowsio.append = a_mode ? SDL_TRUE : SDL_FALSE;

    return 0; /* ok */
}

static Sint64 SDLCALL windows_file_size(SDL_RWops *context)
{
    LARGE_INTEGER size;

    if (context == NULL || context->hidden.windowsio.h == INVALID_HANDLE_VALUE) {
        return SDL_SetError("windows_file_size: invalid context/file not opened");
    }

    if (!GetFileSizeEx(context->hidden.windowsio.h, &size)) {
        return WIN_SetError("windows_file_size");
    }

    return size.QuadPart;
}

static Sint64 SDLCALL windows_file_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    DWORD windowswhence;
    LARGE_INTEGER windowsoffset;

    if (context == NULL || context->hidden.windowsio.h == INVALID_HANDLE_VALUE) {
        return SDL_SetError("windows_file_seek: invalid context/file not opened");
    }

    /* FIXME: We may be able to satisfy the seek within buffered data */
    if (whence == SDL_RW_SEEK_CUR && context->hidden.windowsio.buffer.left) {
        offset -= (long)context->hidden.windowsio.buffer.left;
    }
    context->hidden.windowsio.buffer.left = 0;

    switch (whence) {
    case SDL_RW_SEEK_SET:
        windowswhence = FILE_BEGIN;
        break;
    case SDL_RW_SEEK_CUR:
        windowswhence = FILE_CURRENT;
        break;
    case SDL_RW_SEEK_END:
        windowswhence = FILE_END;
        break;
    default:
        return SDL_SetError("windows_file_seek: Unknown value for 'whence'");
    }

    windowsoffset.QuadPart = offset;
    if (!SetFilePointerEx(context->hidden.windowsio.h, windowsoffset, &windowsoffset, windowswhence)) {
        return WIN_SetError("windows_file_seek");
    }
    return windowsoffset.QuadPart;
}

static Sint64 SDLCALL
windows_file_read(SDL_RWops *context, void *ptr, Sint64 size)
{
    size_t total_need = (size_t) size;
    size_t total_read = 0;
    size_t read_ahead;
    DWORD byte_read;

    if (context == NULL || context->hidden.windowsio.h == INVALID_HANDLE_VALUE) {
        return SDL_SetError("Invalid file handle");
    } else if (!total_need) {
        return 0;
    }


    if (context->hidden.windowsio.buffer.left > 0) {
        void *data = (char *)context->hidden.windowsio.buffer.data +
                     context->hidden.windowsio.buffer.size -
                     context->hidden.windowsio.buffer.left;
        read_ahead =
            SDL_min(total_need, context->hidden.windowsio.buffer.left);
        SDL_memcpy(ptr, data, read_ahead);
        context->hidden.windowsio.buffer.left -= read_ahead;

        if (read_ahead == total_need) {
            return size;
        }
        ptr = (char *)ptr + read_ahead;
        total_need -= read_ahead;
        total_read += read_ahead;
    }

    if (total_need < READAHEAD_BUFFER_SIZE) {
        if (!ReadFile(context->hidden.windowsio.h, context->hidden.windowsio.buffer.data,
                      READAHEAD_BUFFER_SIZE, &byte_read, NULL)) {
            return SDL_Error(SDL_EFREAD);
        }
        read_ahead = SDL_min(total_need, (int)byte_read);
        SDL_memcpy(ptr, context->hidden.windowsio.buffer.data, read_ahead);
        context->hidden.windowsio.buffer.size = byte_read;
        context->hidden.windowsio.buffer.left = byte_read - read_ahead;
        total_read += read_ahead;
    } else {
        if (!ReadFile(context->hidden.windowsio.h, ptr, (DWORD)total_need, &byte_read, NULL)) {
            return SDL_Error(SDL_EFREAD);
        }
        total_read += byte_read;
    }
    return total_read;
}

static Sint64 SDLCALL
windows_file_write(SDL_RWops *context, const void *ptr, Sint64 size)
{
    const size_t total_bytes = (size_t) size;
    DWORD byte_written;

    if (context == NULL || context->hidden.windowsio.h == INVALID_HANDLE_VALUE) {
        return SDL_SetError("Invalid file handle");
    } else if (!total_bytes) {
        return 0;
    }

    if (context->hidden.windowsio.buffer.left) {
        SetFilePointer(context->hidden.windowsio.h,
                       -(LONG)context->hidden.windowsio.buffer.left, NULL,
                       FILE_CURRENT);
        context->hidden.windowsio.buffer.left = 0;
    }

    /* if in append mode, we must go to the EOF before write */
    if (context->hidden.windowsio.append) {
        LARGE_INTEGER windowsoffset;
        windowsoffset.QuadPart = 0;
        if (!SetFilePointerEx(context->hidden.windowsio.h, windowsoffset, &windowsoffset, FILE_END)) {
            return SDL_Error(SDL_EFWRITE);
        }
    }

    if (!WriteFile(context->hidden.windowsio.h, ptr, (DWORD)total_bytes, &byte_written, NULL)) {
        return SDL_Error(SDL_EFWRITE);
    }

    return (Sint64) byte_written;
}

static int SDLCALL windows_file_close(SDL_RWops *context)
{
    if (context) {
        if (context->hidden.windowsio.h != INVALID_HANDLE_VALUE) {
            CloseHandle(context->hidden.windowsio.h);
            context->hidden.windowsio.h = INVALID_HANDLE_VALUE; /* to be sure */
        }
        SDL_free(context->hidden.windowsio.buffer.data);
        context->hidden.windowsio.buffer.data = NULL;
        SDL_DestroyRW(context);
    }
    return 0;
}
#endif /* defined(__WIN32__) || defined(__GDK__) */

#if defined(HAVE_STDIO_H) && !(defined(__WIN32__) || defined(__GDK__))

/* Functions to read/write stdio file pointers. Not used for windows. */

#ifdef HAVE_FOPEN64
#define fopen fopen64
#endif
#ifdef HAVE_FSEEKO64
#define fseek_off_t off64_t
#define fseek       fseeko64
#define ftell       ftello64
#elif defined(HAVE_FSEEKO)
#if defined(OFF_MIN) && defined(OFF_MAX)
#define FSEEK_OFF_MIN OFF_MIN
#define FSEEK_OFF_MAX OFF_MAX
#elif defined(HAVE_LIMITS_H)
/* POSIX doesn't specify the minimum and maximum macros for off_t so
 * we have to improvise and dance around implementation-defined
 * behavior. This may fail if the off_t type has padding bits or
 * is not a two's-complement representation. The compilers will detect
 * and eliminate the dead code if off_t has 64 bits.
 */
#define FSEEK_OFF_MAX (((((off_t)1 << (sizeof(off_t) * CHAR_BIT - 2)) - 1) << 1) + 1)
#define FSEEK_OFF_MIN (-(FSEEK_OFF_MAX)-1)
#endif
#define fseek_off_t off_t
#define fseek       fseeko
#define ftell       ftello
#elif defined(HAVE__FSEEKI64)
#define fseek_off_t __int64
#define fseek       _fseeki64
#define ftell       _ftelli64
#else
#ifdef HAVE_LIMITS_H
#define FSEEK_OFF_MIN LONG_MIN
#define FSEEK_OFF_MAX LONG_MAX
#endif
#define fseek_off_t long
#endif

static Sint64 SDLCALL stdio_size(SDL_RWops *context)
{
    Sint64 pos, size;

    pos = SDL_RWseek(context, 0, SDL_RW_SEEK_CUR);
    if (pos < 0) {
        return -1;
    }
    size = SDL_RWseek(context, 0, SDL_RW_SEEK_END);

    SDL_RWseek(context, pos, SDL_RW_SEEK_SET);
    return size;
}

static Sint64 SDLCALL stdio_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    int stdiowhence;

    switch (whence) {
    case SDL_RW_SEEK_SET:
        stdiowhence = SEEK_SET;
        break;
    case SDL_RW_SEEK_CUR:
        stdiowhence = SEEK_CUR;
        break;
    case SDL_RW_SEEK_END:
        stdiowhence = SEEK_END;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }

#if defined(FSEEK_OFF_MIN) && defined(FSEEK_OFF_MAX)
    if (offset < (Sint64)(FSEEK_OFF_MIN) || offset > (Sint64)(FSEEK_OFF_MAX)) {
        return SDL_SetError("Seek offset out of range");
    }
#endif

    if (fseek((FILE *)context->hidden.stdio.fp, (fseek_off_t)offset, stdiowhence) == 0) {
        Sint64 pos = ftell((FILE *)context->hidden.stdio.fp);
        if (pos < 0) {
            return SDL_SetError("Couldn't get stream offset");
        }
        return pos;
    }
    return SDL_Error(SDL_EFSEEK);
}

static Sint64 SDLCALL
stdio_read(SDL_RWops *context, void *ptr, Sint64 size)
{
    size_t nread;

    nread = fread(ptr, 1, (size_t)size, (FILE *)context->hidden.stdio.fp);
    if (nread == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        return SDL_Error(SDL_EFREAD);
    }
    return (Sint64) nread;
}

static Sint64 SDLCALL
stdio_write(SDL_RWops *context, const void *ptr, Sint64 size)
{
    size_t nwrote;

    nwrote = fwrite(ptr, 1, (size_t)size, (FILE *)context->hidden.stdio.fp);
    if (nwrote == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        return SDL_Error(SDL_EFWRITE);
    }
    return (Sint64) nwrote;
}

static int SDLCALL stdio_close(SDL_RWops *context)
{
    int status = 0;
    if (context) {
        if (context->hidden.stdio.autoclose) {
            if (fclose((FILE *)context->hidden.stdio.fp) != 0) {
                status = SDL_Error(SDL_EFWRITE);
            }
        }
        SDL_DestroyRW(context);
    }
    return status;
}

static SDL_RWops *SDL_RWFromFP(void *fp, SDL_bool autoclose)
{
    SDL_RWops *rwops = NULL;

    rwops = SDL_CreateRW();
    if (rwops != NULL) {
        rwops->size = stdio_size;
        rwops->seek = stdio_seek;
        rwops->read = stdio_read;
        rwops->write = stdio_write;
        rwops->close = stdio_close;
        rwops->hidden.stdio.fp = fp;
        rwops->hidden.stdio.autoclose = autoclose;
        rwops->type = SDL_RWOPS_STDFILE;
    }
    return rwops;
}
#endif /* !HAVE_STDIO_H && !(__WIN32__ || __GDK__) */

/* Functions to read/write memory pointers */

static Sint64 SDLCALL mem_size(SDL_RWops *context)
{
    return (Sint64)(context->hidden.mem.stop - context->hidden.mem.base);
}

static Sint64 SDLCALL mem_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    Uint8 *newpos;

    switch (whence) {
    case SDL_RW_SEEK_SET:
        newpos = context->hidden.mem.base + offset;
        break;
    case SDL_RW_SEEK_CUR:
        newpos = context->hidden.mem.here + offset;
        break;
    case SDL_RW_SEEK_END:
        newpos = context->hidden.mem.stop + offset;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }
    if (newpos < context->hidden.mem.base) {
        newpos = context->hidden.mem.base;
    }
    if (newpos > context->hidden.mem.stop) {
        newpos = context->hidden.mem.stop;
    }
    context->hidden.mem.here = newpos;
    return (Sint64)(context->hidden.mem.here - context->hidden.mem.base);
}

static Sint64 mem_io(SDL_RWops *context, void *dst, const void *src, Sint64 size)
{
    const Sint64 mem_available = (Sint64) (context->hidden.mem.stop - context->hidden.mem.here);
    if (size > mem_available) {
        size = mem_available;
    }
    SDL_memcpy(dst, src, (size_t) size);
    context->hidden.mem.here += size;
    return size;
}

static Sint64 SDLCALL
mem_read(SDL_RWops *context, void *ptr, Sint64 size)
{
    return mem_io(context, ptr, context->hidden.mem.here, size);
}

static Sint64 SDLCALL
mem_write(SDL_RWops *context, const void *ptr, Sint64 size)
{
    return mem_io(context, context->hidden.mem.here, ptr, size);
}

static Sint64 SDLCALL
mem_writeconst(SDL_RWops *context, const void *ptr, Sint64 size)
{
    return SDL_SetError("Can't write to read-only memory");
}

static int SDLCALL mem_close(SDL_RWops *context)
{
    if (context) {
        SDL_DestroyRW(context);
    }
    return 0;
}

/* Functions to create SDL_RWops structures from various data sources */

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
    SDL_RWops *rwops = NULL;
    if (file == NULL || !*file || mode == NULL || !*mode) {
        SDL_SetError("SDL_RWFromFile(): No file or no mode specified");
        return NULL;
    }
#ifdef __ANDROID__
#ifdef HAVE_STDIO_H
    /* Try to open the file on the filesystem first */
    if (*file == '/') {
        FILE *fp = fopen(file, mode);
        if (fp) {
            return SDL_RWFromFP(fp, 1);
        }
    } else {
        /* Try opening it from internal storage if it's a relative path */
        char *path;
        FILE *fp;

        /* !!! FIXME: why not just "char path[PATH_MAX];" ? */
        path = SDL_stack_alloc(char, PATH_MAX);
        if (path) {
            SDL_snprintf(path, PATH_MAX, "%s/%s",
                         SDL_AndroidGetInternalStoragePath(), file);
            fp = fopen(path, mode);
            SDL_stack_free(path);
            if (fp) {
                return SDL_RWFromFP(fp, 1);
            }
        }
    }
#endif /* HAVE_STDIO_H */

    /* Try to open the file from the asset system */
    rwops = SDL_CreateRW();
    if (rwops == NULL) {
        return NULL; /* SDL_SetError already setup by SDL_CreateRW() */
    }

    if (Android_JNI_FileOpen(rwops, file, mode) < 0) {
        SDL_DestroyRW(rwops);
        return NULL;
    }
    rwops->size = Android_JNI_FileSize;
    rwops->seek = Android_JNI_FileSeek;
    rwops->read = Android_JNI_FileRead;
    rwops->write = Android_JNI_FileWrite;
    rwops->close = Android_JNI_FileClose;
    rwops->type = SDL_RWOPS_JNIFILE;

#elif defined(__WIN32__) || defined(__GDK__)
    rwops = SDL_CreateRW();
    if (rwops == NULL) {
        return NULL; /* SDL_SetError already setup by SDL_CreateRW() */
    }

    if (windows_file_open(rwops, file, mode) < 0) {
        SDL_DestroyRW(rwops);
        return NULL;
    }
    rwops->size = windows_file_size;
    rwops->seek = windows_file_seek;
    rwops->read = windows_file_read;
    rwops->write = windows_file_write;
    rwops->close = windows_file_close;
    rwops->type = SDL_RWOPS_WINFILE;
#elif defined(HAVE_STDIO_H)
    {
#if defined(__APPLE__) && !defined(SDL_FILE_DISABLED) // TODO: add dummy?
        FILE *fp = SDL_OpenFPFromBundleOrFallback(file, mode);
#elif defined(__WINRT__)
        FILE *fp = NULL;
        fopen_s(&fp, file, mode);
#elif defined(__3DS__)
        FILE *fp = N3DS_FileOpen(file, mode);
#else
        FILE *fp = fopen(file, mode);
#endif
        if (fp == NULL) {
            SDL_SetError("Couldn't open %s", file);
        } else {
            rwops = SDL_RWFromFP(fp, SDL_TRUE);
        }
    }
#else
    SDL_SetError("SDL not compiled with stdio support");
#endif /* !HAVE_STDIO_H */

    return rwops;
}

SDL_RWops *SDL_RWFromMem(void *mem, size_t size)
{
    SDL_RWops *rwops = NULL;
    if (mem == NULL) {
        SDL_InvalidParamError("mem");
        return rwops;
    }
    if (!size) {
        SDL_InvalidParamError("size");
        return rwops;
    }

    rwops = SDL_CreateRW();
    if (rwops != NULL) {
        rwops->size = mem_size;
        rwops->seek = mem_seek;
        rwops->read = mem_read;
        rwops->write = mem_write;
        rwops->close = mem_close;
        rwops->hidden.mem.base = (Uint8 *)mem;
        rwops->hidden.mem.here = rwops->hidden.mem.base;
        rwops->hidden.mem.stop = rwops->hidden.mem.base + size;
        rwops->type = SDL_RWOPS_MEMORY;
    }
    return rwops;
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, size_t size)
{
    SDL_RWops *rwops = NULL;
    if (mem == NULL) {
        SDL_InvalidParamError("mem");
        return rwops;
    }
    if (!size) {
        SDL_InvalidParamError("size");
        return rwops;
    }

    rwops = SDL_CreateRW();
    if (rwops != NULL) {
        rwops->size = mem_size;
        rwops->seek = mem_seek;
        rwops->read = mem_read;
        rwops->write = mem_writeconst;
        rwops->close = mem_close;
        rwops->hidden.mem.base = (Uint8 *)mem;
        rwops->hidden.mem.here = rwops->hidden.mem.base;
        rwops->hidden.mem.stop = rwops->hidden.mem.base + size;
        rwops->type = SDL_RWOPS_MEMORY_RO;
    }
    return rwops;
}

SDL_RWops *SDL_CreateRW(void)
{
    SDL_RWops *area;

    area = (SDL_RWops *)SDL_malloc(sizeof(*area));
    if (area == NULL) {
        SDL_OutOfMemory();
    } else {
        area->type = SDL_RWOPS_UNKNOWN;
    }
    return area;
}

void SDL_DestroyRW(SDL_RWops *area)
{
    SDL_free(area);
}

/* Load all the data from an SDL data stream */
void *SDL_LoadFile_RW(SDL_RWops *src, size_t *datasize, SDL_bool freesrc)
{
    static const Sint64 FILE_CHUNK_SIZE = 1024;
    Sint64 size;
    Sint64 size_read, size_total;
    void *data = NULL, *newdata;

    if (src == NULL) {
        SDL_InvalidParamError("src");
        return NULL;
    }

    size = SDL_RWsize(src);
    if (size < 0) {
        size = FILE_CHUNK_SIZE;
    }
    data = SDL_malloc((size_t)(size + 1));

    size_total = 0;
    for (;;) {
        if ((size_total + FILE_CHUNK_SIZE) > size) {
            size = (size_total + FILE_CHUNK_SIZE);
            newdata = SDL_realloc(data, (size_t)(size + 1));
            if (newdata == NULL) {
                SDL_free(data);
                data = NULL;
                SDL_OutOfMemory();
                goto done;
            }
            data = newdata;
        }

        size_read = SDL_RWread(src, (char *)data + size_total, size - size_total);
        if (size_read > 0) {
            size_total += size_read;
            continue;
        }
        if (size_read == 0) {
            /* End of file */
            break;
        }
        if (size_read == -2) {
            /* Non-blocking I/O, should we wait here? */
        }

        /* Read error */
        SDL_free(data);
        data = NULL;
        goto done;
    }

    if (datasize) {
        *datasize = (size_t) size_total;
    }
    ((char *)data)[size_total] = '\0';

done:
    if (freesrc && src) {
        SDL_RWclose(src);
    }
    return data;
}

void *SDL_LoadFile(const char *file, size_t *datasize)
{
    return SDL_LoadFile_RW(SDL_RWFromFile(file, "rb"), datasize, 1);
}

Sint64 SDL_RWsize(SDL_RWops *context)
{
    return context->size(context);
}

Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence)
{
    return context->seek(context, offset, whence);
}

Sint64 SDL_RWtell(SDL_RWops *context)
{
    return context->seek(context, 0, SDL_RW_SEEK_CUR);
}

Sint64 SDL_RWread(SDL_RWops *context, void *ptr, Sint64 size)
{
    return context->read(context, ptr, size);
}

Sint64 SDL_RWwrite(SDL_RWops *context, const void *ptr, Sint64 size)
{
    return context->write(context, ptr, size);
}

int SDL_RWclose(SDL_RWops *context)
{
    return context->close(context);
}

/* Functions for dynamically reading and writing endian-specific values */

Uint8 SDL_ReadU8(SDL_RWops *src)
{
    Uint8 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return value;
}

Uint16 SDL_ReadLE16(SDL_RWops *src)
{
    Uint16 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapLE16(value);
}

Uint16 SDL_ReadBE16(SDL_RWops *src)
{
    Uint16 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapBE16(value);
}

Uint32 SDL_ReadLE32(SDL_RWops *src)
{
    Uint32 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapLE32(value);
}

Uint32 SDL_ReadBE32(SDL_RWops *src)
{
    Uint32 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapBE32(value);
}

Uint64 SDL_ReadLE64(SDL_RWops *src)
{
    Uint64 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapLE64(value);
}

Uint64 SDL_ReadBE64(SDL_RWops *src)
{
    Uint64 value = 0;

    SDL_RWread(src, &value, sizeof(value));
    return SDL_SwapBE64(value);
}

size_t SDL_WriteU8(SDL_RWops *dst, Uint8 value)
{
    return (SDL_RWwrite(dst, &value, sizeof(value)) == sizeof(value)) ? 1 : 0;
}

size_t SDL_WriteLE16(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapLE16(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}

size_t SDL_WriteBE16(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapBE16(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}

size_t SDL_WriteLE32(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapLE32(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}

size_t SDL_WriteBE32(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapBE32(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}

size_t SDL_WriteLE64(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapLE64(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}

size_t SDL_WriteBE64(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapBE64(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped)) ? 1 : 0;
}
