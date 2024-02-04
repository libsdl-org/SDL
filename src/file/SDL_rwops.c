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

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)
#include "../core/windows/SDL_windows.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#include <sys/stat.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* This file provides a general interface for SDL to read and write
   data sources.  It can easily be extended to files, memory, etc.
*/

#ifdef SDL_PLATFORM_APPLE
#include "cocoa/SDL_rwopsbundlesupport.h"
#endif /* SDL_PLATFORM_APPLE */

#ifdef SDL_PLATFORM_3DS
#include "n3ds/SDL_rwopsromfs.h"
#endif /* SDL_PLATFORM_3DS */

#ifdef SDL_PLATFORM_ANDROID
#include "../core/android/SDL_android.h"
#endif

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)

/* Functions to read/write Win32 API file pointers */
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define READAHEAD_BUFFER_SIZE 1024

static int SDLCALL windows_file_open(SDL_RWops *context, const char *filename, const char *mode)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES) && !defined(SDL_PLATFORM_WINRT)
    UINT old_error_mode;
#endif
    HANDLE h;
    DWORD r_right, w_right;
    DWORD must_exist, truncate;
    int a_mode;

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
        return -1;
    }
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES) && !defined(SDL_PLATFORM_WINRT)
    /* Do not open a dialog box if failure */
    old_error_mode =
        SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
#endif

    {
        LPTSTR tstr = WIN_UTF8ToString(filename);
#if defined(SDL_PLATFORM_WINRT)
        CREATEFILE2_EXTENDED_PARAMETERS extparams;
        SDL_zero(extparams);
        extparams.dwSize = sizeof(extparams);
        extparams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        h = CreateFile2(tstr,
                        (w_right | r_right),
                        (w_right) ? 0 : FILE_SHARE_READ,
                        (must_exist | truncate | a_mode),
                        &extparams);
#else
        h = CreateFile(tstr,
                       (w_right | r_right),
                       (w_right) ? 0 : FILE_SHARE_READ,
                       NULL,
                       (must_exist | truncate | a_mode),
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
#endif
        SDL_free(tstr);
    }

#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES) && !defined(SDL_PLATFORM_WINRT)
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

    if (!GetFileSizeEx(context->hidden.windowsio.h, &size)) {
        return WIN_SetError("windows_file_size");
    }

    return size.QuadPart;
}

static Sint64 SDLCALL windows_file_seek(SDL_RWops *context, Sint64 offset, int whence)
{
    DWORD windowswhence;
    LARGE_INTEGER windowsoffset;

    /* FIXME: We may be able to satisfy the seek within buffered data */
    if (whence == SDL_RW_SEEK_CUR && context->hidden.windowsio.buffer.left) {
        offset -= context->hidden.windowsio.buffer.left;
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

static size_t SDLCALL windows_file_read(SDL_RWops *context, void *ptr, size_t size)
{
    size_t total_need = size;
    size_t total_read = 0;
    size_t read_ahead;
    DWORD bytes;

    if (context->hidden.windowsio.buffer.left > 0) {
        void *data = (char *)context->hidden.windowsio.buffer.data +
                     context->hidden.windowsio.buffer.size -
                     context->hidden.windowsio.buffer.left;
        read_ahead = SDL_min(total_need, context->hidden.windowsio.buffer.left);
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
                      READAHEAD_BUFFER_SIZE, &bytes, NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        read_ahead = SDL_min(total_need, bytes);
        SDL_memcpy(ptr, context->hidden.windowsio.buffer.data, read_ahead);
        context->hidden.windowsio.buffer.size = bytes;
        context->hidden.windowsio.buffer.left = bytes - read_ahead;
        total_read += read_ahead;
    } else {
        if (!ReadFile(context->hidden.windowsio.h, ptr, (DWORD)total_need, &bytes, NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        total_read += bytes;
    }
    return total_read;
}

static size_t SDLCALL windows_file_write(SDL_RWops *context, const void *ptr, size_t size)
{
    const size_t total_bytes = size;
    DWORD bytes;

    if (context->hidden.windowsio.buffer.left) {
        if (!SetFilePointer(context->hidden.windowsio.h,
                       -(LONG)context->hidden.windowsio.buffer.left, NULL,
                       FILE_CURRENT)) {
            SDL_Error(SDL_EFSEEK);
            return 0;
        }
        context->hidden.windowsio.buffer.left = 0;
    }

    /* if in append mode, we must go to the EOF before write */
    if (context->hidden.windowsio.append) {
        LARGE_INTEGER windowsoffset;
        windowsoffset.QuadPart = 0;
        if (!SetFilePointerEx(context->hidden.windowsio.h, windowsoffset, &windowsoffset, FILE_END)) {
            SDL_Error(SDL_EFSEEK);
            return 0;
        }
    }

    if (!WriteFile(context->hidden.windowsio.h, ptr, (DWORD)total_bytes, &bytes, NULL)) {
        SDL_Error(SDL_EFWRITE);
        return 0;
    }

    return bytes;
}

static int SDLCALL windows_file_close(SDL_RWops *context)
{
    if (context->hidden.windowsio.h != INVALID_HANDLE_VALUE) {
        CloseHandle(context->hidden.windowsio.h);
        context->hidden.windowsio.h = INVALID_HANDLE_VALUE; /* to be sure */
    }
    if (context->hidden.windowsio.buffer.data) {
        SDL_free(context->hidden.windowsio.buffer.data);
        context->hidden.windowsio.buffer.data = NULL;
    }
    SDL_DestroyRW(context);
    return 0;
}
#endif /* defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) */

#if defined(HAVE_STDIO_H) && !(defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK))

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

static size_t SDLCALL stdio_read(SDL_RWops *context, void *ptr, size_t size)
{
    size_t bytes;

    bytes = fread(ptr, 1, size, (FILE *)context->hidden.stdio.fp);
    if (bytes == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFREAD);
    }
    return bytes;
}

static size_t SDLCALL stdio_write(SDL_RWops *context, const void *ptr, size_t size)
{
    size_t bytes;

    bytes = fwrite(ptr, 1, size, (FILE *)context->hidden.stdio.fp);
    if (bytes == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFWRITE);
    }
    return bytes;
}

static int SDLCALL stdio_close(SDL_RWops *context)
{
    int status = 0;
    if (context->hidden.stdio.autoclose) {
        if (fclose((FILE *)context->hidden.stdio.fp) != 0) {
            status = SDL_Error(SDL_EFWRITE);
        }
    }
    SDL_DestroyRW(context);
    return status;
}

static SDL_RWops *SDL_RWFromFP(void *fp, SDL_bool autoclose)
{
    SDL_RWops *rwops = NULL;

    rwops = SDL_CreateRW();
    if (rwops) {
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
#endif /* !HAVE_STDIO_H && !(SDL_PLATFORM_WIN32 || SDL_PLATFORM_GDK) */

/* Functions to read/write memory pointers */

static Sint64 SDLCALL mem_size(SDL_RWops *context)
{
    return (context->hidden.mem.stop - context->hidden.mem.base);
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

static size_t mem_io(SDL_RWops *context, void *dst, const void *src, size_t size)
{
    const size_t mem_available = (context->hidden.mem.stop - context->hidden.mem.here);
    if (size > mem_available) {
        size = mem_available;
    }
    SDL_memcpy(dst, src, size);
    context->hidden.mem.here += size;
    return size;
}

static size_t SDLCALL mem_read(SDL_RWops *context, void *ptr, size_t size)
{
    return mem_io(context, ptr, context->hidden.mem.here, size);
}

static size_t SDLCALL mem_write(SDL_RWops *context, const void *ptr, size_t size)
{
    return mem_io(context, context->hidden.mem.here, ptr, size);
}

/* Functions to create SDL_RWops structures from various data sources */

#if defined(HAVE_STDIO_H) && !defined(SDL_PLATFORM_WINDOWS)
static SDL_bool SDL_IsRegularFile(FILE *f)
{
    #ifdef SDL_PLATFORM_WINRT
    struct __stat64 st;
    if (_fstat64(_fileno(f), &st) < 0 || (st.st_mode & _S_IFMT) != _S_IFREG) {
        return SDL_FALSE;
    }
    #else
    struct stat st;
    if (fstat(fileno(f), &st) < 0 || !S_ISREG(st.st_mode)) {
        return SDL_FALSE;
    }
    #endif
    return SDL_TRUE;
}
#endif

SDL_RWops *SDL_RWFromFile(const char *file, const char *mode)
{
    SDL_RWops *rwops = NULL;
    if (!file || !*file || !mode || !*mode) {
        SDL_SetError("SDL_RWFromFile(): No file or no mode specified");
        return NULL;
    }
#ifdef SDL_PLATFORM_ANDROID
#ifdef HAVE_STDIO_H
    /* Try to open the file on the filesystem first */
    if (*file == '/') {
        FILE *fp = fopen(file, mode);
        if (fp) {
            if (!SDL_IsRegularFile(fp)) {
                fclose(fp);
                SDL_SetError("%s is not a regular file", file);
                return NULL;
            }
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
                if (!SDL_IsRegularFile(fp)) {
                    fclose(fp);
                    SDL_SetError("%s is not a regular file", path);
                    return NULL;
                }
                return SDL_RWFromFP(fp, 1);
            }
        }
    }
#endif /* HAVE_STDIO_H */

    /* Try to open the file from the asset system */
    rwops = SDL_CreateRW();
    if (!rwops) {
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

#elif defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)
    rwops = SDL_CreateRW();
    if (!rwops) {
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
#if defined(SDL_PLATFORM_APPLE)
        FILE *fp = SDL_OpenFPFromBundleOrFallback(file, mode);
#elif defined(SDL_PLATFORM_WINRT)
        FILE *fp = NULL;
        fopen_s(&fp, file, mode);
#elif defined(SDL_PLATFORM_3DS)
        FILE *fp = N3DS_FileOpen(file, mode);
#else
        FILE *fp = fopen(file, mode);
#endif
        if (!fp) {
            SDL_SetError("Couldn't open %s", file);
        } else if (!SDL_IsRegularFile(fp)) {
            fclose(fp);
            fp = NULL;
            SDL_SetError("%s is not a regular file", file);
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

    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    }
    if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    rwops = SDL_CreateRW();
    if (rwops) {
        rwops->size = mem_size;
        rwops->seek = mem_seek;
        rwops->read = mem_read;
        rwops->write = mem_write;
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

    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    }
    if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    rwops = SDL_CreateRW();
    if (rwops) {
        rwops->size = mem_size;
        rwops->seek = mem_seek;
        rwops->read = mem_read;
        rwops->hidden.mem.base = (Uint8 *)mem;
        rwops->hidden.mem.here = rwops->hidden.mem.base;
        rwops->hidden.mem.stop = rwops->hidden.mem.base + size;
        rwops->type = SDL_RWOPS_MEMORY_RO;
    }
    return rwops;
}

SDL_RWops *SDL_CreateRW(void)
{
    SDL_RWops *context;

    context = (SDL_RWops *)SDL_calloc(1, sizeof(*context));
    if (context) {
        context->type = SDL_RWOPS_UNKNOWN;
    }
    return context;
}

void SDL_DestroyRW(SDL_RWops *context)
{
    SDL_DestroyProperties(context->props);
    SDL_free(context);
}

/* Load all the data from an SDL data stream */
void *SDL_LoadFile_RW(SDL_RWops *src, size_t *datasize, SDL_bool freesrc)
{
    const int FILE_CHUNK_SIZE = 1024;
    Sint64 size, size_total = 0;
    size_t size_read;
    char *data = NULL, *newdata;
    SDL_bool loading_chunks = SDL_FALSE;

    if (!src) {
        SDL_InvalidParamError("src");
        goto done;
    }

    size = SDL_RWsize(src);
    if (size < 0) {
        size = FILE_CHUNK_SIZE;
        loading_chunks = SDL_TRUE;
    }
    if (size >= SDL_SIZE_MAX) {
        goto done;
    }
    data = (char *)SDL_malloc((size_t)(size + 1));
    if (!data) {
        goto done;
    }

    size_total = 0;
    for (;;) {
        if (loading_chunks) {
            if ((size_total + FILE_CHUNK_SIZE) > size) {
                size = (size_total + FILE_CHUNK_SIZE);
                if (size >= SDL_SIZE_MAX) {
                    newdata = NULL;
                } else {
                    newdata = SDL_realloc(data, (size_t)(size + 1));
                }
                if (!newdata) {
                    SDL_free(data);
                    data = NULL;
                    goto done;
                }
                data = newdata;
            }
        }

        size_read = SDL_RWread(src, data + size_total, (size_t)(size - size_total));
        if (size_read > 0) {
            size_total += size_read;
            continue;
        }

        /* The stream status will remain set for the caller to check */
        break;
    }

    data[size_total] = '\0';

done:
    if (datasize) {
        *datasize = (size_t)size_total;
    }
    if (freesrc && src) {
        SDL_RWclose(src);
    }
    return data;
}

void *SDL_LoadFile(const char *file, size_t *datasize)
{
    return SDL_LoadFile_RW(SDL_RWFromFile(file, "rb"), datasize, SDL_TRUE);
}

SDL_PropertiesID SDL_GetRWProperties(SDL_RWops *context)
{
    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    }

    if (context->props == 0) {
        context->props = SDL_CreateProperties();
    }
    return context->props;
}

Sint64 SDL_RWsize(SDL_RWops *context)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    }
    if (!context->size) {
        Sint64 pos, size;

        pos = SDL_RWseek(context, 0, SDL_RW_SEEK_CUR);
        if (pos < 0) {
            return -1;
        }
        size = SDL_RWseek(context, 0, SDL_RW_SEEK_END);

        SDL_RWseek(context, pos, SDL_RW_SEEK_SET);
        return size;
    }
    return context->size(context);
}

Sint64 SDL_RWseek(SDL_RWops *context, Sint64 offset, int whence)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    }
    if (!context->seek) {
        return SDL_Unsupported();
    }
    return context->seek(context, offset, whence);
}

Sint64 SDL_RWtell(SDL_RWops *context)
{
    return SDL_RWseek(context, 0, SDL_RW_SEEK_CUR);
}

size_t SDL_RWread(SDL_RWops *context, void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    }
    if (!context->read) {
        context->status = SDL_RWOPS_STATUS_WRITEONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_RWOPS_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->read(context, ptr, size);
    if (bytes == 0 && context->status == SDL_RWOPS_STATUS_READY) {
        if (*SDL_GetError()) {
            context->status = SDL_RWOPS_STATUS_ERROR;
        } else {
            context->status = SDL_RWOPS_STATUS_EOF;
        }
    }
    return bytes;
}

size_t SDL_RWwrite(SDL_RWops *context, const void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    }
    if (!context->write) {
        context->status = SDL_RWOPS_STATUS_READONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_RWOPS_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->write(context, ptr, size);
    if (bytes == 0 && context->status == SDL_RWOPS_STATUS_READY) {
        context->status = SDL_RWOPS_STATUS_ERROR;
    }
    return bytes;
}

size_t SDL_RWprintf(SDL_RWops *context, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
{
    va_list ap;
    int size;
    char *string;
    size_t bytes;

    va_start(ap, fmt);
    size = SDL_vasprintf(&string, fmt, ap);
    va_end(ap);
    if (size < 0) {
        return 0;
    }

    bytes = SDL_RWwrite(context, string, (size_t)size);
    SDL_free(string);
    return bytes;
}

size_t SDL_RWvprintf(SDL_RWops *context, SDL_PRINTF_FORMAT_STRING const char *fmt, va_list ap)
{
    int size;
    char *string;
    size_t bytes;

    size = SDL_vasprintf(&string, fmt, ap);
    if (size < 0) {
        return 0;
    }

    bytes = SDL_RWwrite(context, string, (size_t)size);
    SDL_free(string);
    return bytes;
}

int SDL_RWclose(SDL_RWops *context)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    }
    if (!context->close) {
        SDL_DestroyRW(context);
        return 0;
    }
    return context->close(context);
}

/* Functions for dynamically reading and writing endian-specific values */

SDL_bool SDL_ReadU8(SDL_RWops *src, Uint8 *value)
{
    Uint8 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = data;
    }
    return result;
}

SDL_bool SDL_ReadU16LE(SDL_RWops *src, Uint16 *value)
{
    Uint16 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapLE16(data);
    }
    return result;
}

SDL_bool SDL_ReadS16LE(SDL_RWops *src, Sint16 *value)
{
    return SDL_ReadU16LE(src, (Uint16 *)value);
}

SDL_bool SDL_ReadU16BE(SDL_RWops *src, Uint16 *value)
{
    Uint16 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapBE16(data);
    }
    return result;
}

SDL_bool SDL_ReadS16BE(SDL_RWops *src, Sint16 *value)
{
    return SDL_ReadU16BE(src, (Uint16 *)value);
}

SDL_bool SDL_ReadU32LE(SDL_RWops *src, Uint32 *value)
{
    Uint32 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapLE32(data);
    }
    return result;
}

SDL_bool SDL_ReadS32LE(SDL_RWops *src, Sint32 *value)
{
    return SDL_ReadU32LE(src, (Uint32 *)value);
}

SDL_bool SDL_ReadU32BE(SDL_RWops *src, Uint32 *value)
{
    Uint32 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapBE32(data);
    }
    return result;
}

SDL_bool SDL_ReadS32BE(SDL_RWops *src, Sint32 *value)
{
    return SDL_ReadU32BE(src, (Uint32 *)value);
}

SDL_bool SDL_ReadU64LE(SDL_RWops *src, Uint64 *value)
{
    Uint64 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapLE64(data);
    }
    return result;
}

SDL_bool SDL_ReadS64LE(SDL_RWops *src, Sint64 *value)
{
    return SDL_ReadU64LE(src, (Uint64 *)value);
}

SDL_bool SDL_ReadU64BE(SDL_RWops *src, Uint64 *value)
{
    Uint64 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_RWread(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_SwapBE64(data);
    }
    return result;
}

SDL_bool SDL_ReadS64BE(SDL_RWops *src, Sint64 *value)
{
    return SDL_ReadU64BE(src, (Uint64 *)value);
}

SDL_bool SDL_WriteU8(SDL_RWops *dst, Uint8 value)
{
    return (SDL_RWwrite(dst, &value, sizeof(value)) == sizeof(value));
}

SDL_bool SDL_WriteU16LE(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapLE16(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16LE(SDL_RWops *dst, Sint16 value)
{
    return SDL_WriteU16LE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU16BE(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapBE16(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16BE(SDL_RWops *dst, Sint16 value)
{
    return SDL_WriteU16BE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU32LE(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapLE32(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32LE(SDL_RWops *dst, Sint32 value)
{
    return SDL_WriteU32LE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU32BE(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapBE32(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32BE(SDL_RWops *dst, Sint32 value)
{
    return SDL_WriteU32BE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU64LE(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapLE64(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64LE(SDL_RWops *dst, Sint64 value)
{
    return SDL_WriteU64LE(dst, (Uint64)value);
}

SDL_bool SDL_WriteU64BE(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapBE64(value);
    return (SDL_RWwrite(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64BE(SDL_RWops *dst, Sint64 value)
{
    return SDL_WriteU64BE(dst, (Uint64)value);
}
