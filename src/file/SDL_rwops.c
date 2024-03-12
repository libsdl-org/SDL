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

struct SDL_RWops
{
    SDL_RWopsInterface iface;
    void *userdata;
    Uint32 status;
    SDL_PropertiesID props;
};


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

typedef struct RWopsWindowsData
{
    SDL_bool append;
    HANDLE h;
    void *data;
    size_t size;
    size_t left;
} RWopsWindowsData;


/* Functions to read/write Win32 API file pointers */
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define READAHEAD_BUFFER_SIZE 1024

static int SDLCALL windows_file_open(RWopsWindowsData *rwopsdata, const char *filename, const char *mode)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES) && !defined(SDL_PLATFORM_WINRT)
    UINT old_error_mode;
#endif
    HANDLE h;
    DWORD r_right, w_right;
    DWORD must_exist, truncate;
    int a_mode;

    SDL_zerop(rwopsdata);
    rwopsdata->h = INVALID_HANDLE_VALUE; /* mark this as unusable */

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

    rwopsdata->data = (char *)SDL_malloc(READAHEAD_BUFFER_SIZE);
    if (!rwopsdata->data) {
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
        SDL_free(rwopsdata->data);
        rwopsdata->data = NULL;
        SDL_SetError("Couldn't open %s", filename);
        return -2; /* failed (CreateFile) */
    }
    rwopsdata->h = h;
    rwopsdata->append = a_mode ? SDL_TRUE : SDL_FALSE;

    return 0; /* ok */
}

static Sint64 SDLCALL windows_file_size(void *userdata)
{
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) userdata;
    LARGE_INTEGER size;

    if (!GetFileSizeEx(rwopsdata->h, &size)) {
        return WIN_SetError("windows_file_size");
    }

    return size.QuadPart;
}

static Sint64 SDLCALL windows_file_seek(void *userdata, Sint64 offset, int whence)
{
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) userdata;
    DWORD windowswhence;
    LARGE_INTEGER windowsoffset;

    // FIXME: We may be able to satisfy the seek within buffered data
    if ((whence == SDL_RW_SEEK_CUR) && (rwopsdata->left)) {
        offset -= rwopsdata->left;
    }
    rwopsdata->left = 0;

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
    if (!SetFilePointerEx(rwopsdata->h, windowsoffset, &windowsoffset, windowswhence)) {
        return WIN_SetError("windows_file_seek");
    }
    return windowsoffset.QuadPart;
}

static size_t SDLCALL windows_file_read(void *userdata, void *ptr, size_t size)
{
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) userdata;
    size_t total_need = size;
    size_t total_read = 0;
    size_t read_ahead;
    DWORD bytes;

    if (rwopsdata->left > 0) {
        void *data = (char *)rwopsdata->data +
                     rwopsdata->size -
                     rwopsdata->left;
        read_ahead = SDL_min(total_need, rwopsdata->left);
        SDL_memcpy(ptr, data, read_ahead);
        rwopsdata->left -= read_ahead;

        if (read_ahead == total_need) {
            return size;
        }
        ptr = (char *)ptr + read_ahead;
        total_need -= read_ahead;
        total_read += read_ahead;
    }

    if (total_need < READAHEAD_BUFFER_SIZE) {
        if (!ReadFile(rwopsdata->h, rwopsdata->data, READAHEAD_BUFFER_SIZE, &bytes, NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        read_ahead = SDL_min(total_need, bytes);
        SDL_memcpy(ptr, rwopsdata->data, read_ahead);
        rwopsdata->size = bytes;
        rwopsdata->left = bytes - read_ahead;
        total_read += read_ahead;
    } else {
        if (!ReadFile(rwopsdata->h, ptr, (DWORD)total_need, &bytes, NULL)) {
            SDL_Error(SDL_EFREAD);
            return 0;
        }
        total_read += bytes;
    }
    return total_read;
}

static size_t SDLCALL windows_file_write(void *userdata, const void *ptr, size_t size)
{
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) userdata;
    const size_t total_bytes = size;
    DWORD bytes;

    if (rwopsdata->left) {
        if (!SetFilePointer(rwopsdata->h, -(LONG)rwopsdata->left, NULL, FILE_CURRENT)) {
            SDL_Error(SDL_EFSEEK);
            return 0;
        }
        rwopsdata->left = 0;
    }

    /* if in append mode, we must go to the EOF before write */
    if (rwopsdata->append) {
        LARGE_INTEGER windowsoffset;
        windowsoffset.QuadPart = 0;
        if (!SetFilePointerEx(rwopsdata->h, windowsoffset, &windowsoffset, FILE_END)) {
            SDL_Error(SDL_EFSEEK);
            return 0;
        }
    }

    if (!WriteFile(rwopsdata->h, ptr, (DWORD)total_bytes, &bytes, NULL)) {
        SDL_Error(SDL_EFWRITE);
        return 0;
    }

    return bytes;
}

static int SDLCALL windows_file_close(void *userdata)
{
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) userdata;
    if (rwopsdata->h != INVALID_HANDLE_VALUE) {
        CloseHandle(rwopsdata->h);
        rwopsdata->h = INVALID_HANDLE_VALUE; /* to be sure */
    }
    SDL_free(rwopsdata->data);
    SDL_free(rwopsdata);
    return 0;
}
#endif /* defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) */

#if defined(HAVE_STDIO_H) && !(defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK))

/* Functions to read/write stdio file pointers. Not used for windows. */

typedef struct RWopsStdioData
{
    FILE *fp;
    SDL_bool autoclose;
} RWopsStdioData;

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

static Sint64 SDLCALL stdio_seek(void *userdata, Sint64 offset, int whence)
{
    RWopsStdioData *rwopsdata = (RWopsStdioData *) userdata;
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

    if (fseek(rwopsdata->fp, (fseek_off_t)offset, stdiowhence) == 0) {
        const Sint64 pos = ftell(rwopsdata->fp);
        if (pos < 0) {
            return SDL_SetError("Couldn't get stream offset");
        }
        return pos;
    }
    return SDL_Error(SDL_EFSEEK);
}

static size_t SDLCALL stdio_read(void *userdata, void *ptr, size_t size)
{
    RWopsStdioData *rwopsdata = (RWopsStdioData *) userdata;
    const size_t bytes = fread(ptr, 1, size, rwopsdata->fp);
    if (bytes == 0 && ferror(rwopsdata->fp)) {
        SDL_Error(SDL_EFREAD);
    }
    return bytes;
}

static size_t SDLCALL stdio_write(void *userdata, const void *ptr, size_t size)
{
    RWopsStdioData *rwopsdata = (RWopsStdioData *) userdata;
    const size_t bytes = fwrite(ptr, 1, size, rwopsdata->fp);
    if (bytes == 0 && ferror(rwopsdata->fp)) {
        SDL_Error(SDL_EFWRITE);
    }
    return bytes;
}

static int SDLCALL stdio_close(void *userdata)
{
    RWopsStdioData *rwopsdata = (RWopsStdioData *) userdata;
    int status = 0;
    if (rwopsdata->autoclose) {
        if (fclose(rwopsdata->fp) != 0) {
            status = SDL_Error(SDL_EFWRITE);
        }
    }
    SDL_free(rwopsdata);
    return status;
}

static SDL_RWops *SDL_RWFromFP(FILE *fp, SDL_bool autoclose)
{
    RWopsStdioData *rwopsdata = (RWopsStdioData *) SDL_malloc(sizeof (*rwopsdata));
    if (!rwopsdata) {
        return NULL;
    }

    SDL_RWopsInterface iface;
    SDL_zero(iface);
    // There's no stdio_size because SDL_SizeRW emulates it the same way we'd do it for stdio anyhow.
    iface.seek = stdio_seek;
    iface.read = stdio_read;
    iface.write = stdio_write;
    iface.close = stdio_close;

    rwopsdata->fp = fp;
    rwopsdata->autoclose = autoclose;

    SDL_RWops *rwops = SDL_OpenRW(&iface, rwopsdata);
    if (!rwops) {
        iface.close(rwopsdata);
    }
    return rwops;
}
#endif /* !HAVE_STDIO_H && !(SDL_PLATFORM_WIN32 || SDL_PLATFORM_GDK) */

/* Functions to read/write memory pointers */

typedef struct RWopsMemData
{
    Uint8 *base;
    Uint8 *here;
    Uint8 *stop;
} RWopsMemData;

static Sint64 SDLCALL mem_size(void *userdata)
{
    const RWopsMemData *rwopsdata = (RWopsMemData *) userdata;
    return (rwopsdata->stop - rwopsdata->base);
}

static Sint64 SDLCALL mem_seek(void *userdata, Sint64 offset, int whence)
{
    RWopsMemData *rwopsdata = (RWopsMemData *) userdata;
    Uint8 *newpos;

    switch (whence) {
    case SDL_RW_SEEK_SET:
        newpos = rwopsdata->base + offset;
        break;
    case SDL_RW_SEEK_CUR:
        newpos = rwopsdata->here + offset;
        break;
    case SDL_RW_SEEK_END:
        newpos = rwopsdata->stop + offset;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }
    if (newpos < rwopsdata->base) {
        newpos = rwopsdata->base;
    }
    if (newpos > rwopsdata->stop) {
        newpos = rwopsdata->stop;
    }
    rwopsdata->here = newpos;
    return (Sint64)(rwopsdata->here - rwopsdata->base);
}

static size_t mem_io(void *userdata, void *dst, const void *src, size_t size)
{
    RWopsMemData *rwopsdata = (RWopsMemData *) userdata;
    const size_t mem_available = (rwopsdata->stop - rwopsdata->here);
    if (size > mem_available) {
        size = mem_available;
    }
    SDL_memcpy(dst, src, size);
    rwopsdata->here += size;
    return size;
}

static size_t SDLCALL mem_read(void *userdata, void *ptr, size_t size)
{
    const RWopsMemData *rwopsdata = (RWopsMemData *) userdata;
    return mem_io(userdata, ptr, rwopsdata->here, size);
}

static size_t SDLCALL mem_write(void *userdata, const void *ptr, size_t size)
{
    const RWopsMemData *rwopsdata = (RWopsMemData *) userdata;
    return mem_io(userdata, rwopsdata->here, ptr, size);
}

/* Functions to create SDL_RWops structures from various data sources */

#if defined(HAVE_STDIO_H) && !defined(SDL_PLATFORM_WINDOWS)
static SDL_bool IsRegularFileOrPipe(FILE *f)
{
    #ifdef SDL_PLATFORM_WINRT
    struct __stat64 st;
    if (_fstat64(_fileno(f), &st) < 0 ||
        !((st.st_mode & _S_IFMT) == _S_IFREG || (st.st_mode & _S_IFMT) == _S_IFIFO)) {
        return SDL_FALSE;
    }
    #else
    struct stat st;
    if (fstat(fileno(f), &st) < 0 || !(S_ISREG(st.st_mode) || S_ISFIFO(st.st_mode))) {
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
            if (!IsRegularFileOrPipe(fp)) {
                fclose(fp);
                SDL_SetError("%s is not a regular file or pipe", file);
                return NULL;
            }
            return SDL_RWFromFP(fp, 1);
        }
    } else {
        /* Try opening it from internal storage if it's a relative path */
        // !!! FIXME: why not just "char path[PATH_MAX];"
        char *path = SDL_stack_alloc(char, PATH_MAX);
        if (path) {
            SDL_snprintf(path, PATH_MAX, "%s/%s",
                         SDL_AndroidGetInternalStoragePath(), file);
            FILE *fp = fopen(path, mode);
            SDL_stack_free(path);
            if (fp) {
                if (!IsRegularFileOrPipe(fp)) {
                    fclose(fp);
                    SDL_SetError("%s is not a regular file or pipe", path);
                    return NULL;
                }
                return SDL_RWFromFP(fp, 1);
            }
        }
    }
#endif /* HAVE_STDIO_H */

    /* Try to open the file from the asset system */

    void *rwopsdata = NULL;
    if (Android_JNI_FileOpen(&rwopsdata, file, mode) < 0) {
        SDL_CloseRW(rwops);
        return NULL;
    }

    SDL_RWopsInterface iface;
    SDL_zero(iface);
    iface.size = Android_JNI_FileSize;
    iface.seek = Android_JNI_FileSeek;
    iface.read = Android_JNI_FileRead;
    iface.write = Android_JNI_FileWrite;
    iface.close = Android_JNI_FileClose;

    rwops = SDL_OpenRW(&iface, rwopsdata);
    if (!rwops) {
        iface.close(rwopsdata);
    }
    return rwops;


#elif defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)
    RWopsWindowsData *rwopsdata = (RWopsWindowsData *) SDL_malloc(sizeof (*rwopsdata));
    if (!rwopsdata) {
        return NULL;
    }

    if (windows_file_open(rwopsdata, file, mode) < 0) {
        SDL_CloseRW(rwops);
        return NULL;
    }

    SDL_RWopsInterface iface;
    SDL_zero(iface);
    iface.size = windows_file_size;
    iface.seek = windows_file_seek;
    iface.read = windows_file_read;
    iface.write = windows_file_write;
    iface.close = windows_file_close;

    rwops = SDL_OpenRW(&iface, rwopsdata);
    if (!rwops) {
        windows_file_close(rwopsdata);
    }
    return rwops;

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
        } else if (!IsRegularFileOrPipe(fp)) {
            fclose(fp);
            fp = NULL;
            SDL_SetError("%s is not a regular file or pipe", file);
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
    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    } else if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    RWopsMemData *rwopsdata = (RWopsMemData *) SDL_malloc(sizeof (*rwopsdata));
    if (!rwopsdata) {
        return NULL;
    }

    SDL_RWopsInterface iface;
    SDL_zero(iface);
    iface.size = mem_size;
    iface.seek = mem_seek;
    iface.read = mem_read;
    iface.write = mem_write;

    rwopsdata->base = (Uint8 *)mem;
    rwopsdata->here = rwopsdata->base;
    rwopsdata->stop = rwopsdata->base + size;

    SDL_RWops *rwops = SDL_OpenRW(&iface, rwopsdata);
    if (!rwops) {
        SDL_free(rwopsdata);
    }
    return rwops;
}

SDL_RWops *SDL_RWFromConstMem(const void *mem, size_t size)
{
    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    } else if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    RWopsMemData *rwopsdata = (RWopsMemData *) SDL_malloc(sizeof (*rwopsdata));
    if (!rwopsdata) {
        return NULL;
    }

    SDL_RWopsInterface iface;
    SDL_zero(iface);
    iface.size = mem_size;
    iface.seek = mem_seek;
    iface.read = mem_read;
    // leave iface.write as NULL.

    rwopsdata->base = (Uint8 *)mem;
    rwopsdata->here = rwopsdata->base;
    rwopsdata->stop = rwopsdata->base + size;

    SDL_RWops *rwops = SDL_OpenRW(&iface, rwopsdata);
    if (!rwops) {
        SDL_free(rwopsdata);
    }
    return rwops;
}

SDL_RWops *SDL_OpenRW(const SDL_RWopsInterface *iface, void *userdata)
{
    if (!iface) {
        SDL_InvalidParamError("iface");
        return NULL;
    }

    SDL_RWops *rwops = (SDL_RWops *)SDL_calloc(1, sizeof(*rwops));
    if (rwops) {
        SDL_copyp(&rwops->iface, iface);
        rwops->userdata = userdata;
    }
    return rwops;
}

int SDL_CloseRW(SDL_RWops *rwops)
{
    int retval = 0;
    if (rwops) {
        if (rwops->iface.close) {
            retval = rwops->iface.close(rwops->userdata);
        }
        SDL_DestroyProperties(rwops->props);
        SDL_free(rwops);
    }
    return retval;
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

    size = SDL_SizeRW(src);
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
                    newdata = (char *)SDL_realloc(data, (size_t)(size + 1));
                }
                if (!newdata) {
                    SDL_free(data);
                    data = NULL;
                    goto done;
                }
                data = newdata;
            }
        }

        size_read = SDL_ReadRW(src, data + size_total, (size_t)(size - size_total));
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
        SDL_CloseRW(src);
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

Sint64 SDL_SizeRW(SDL_RWops *context)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    }
    if (!context->iface.size) {
        Sint64 pos, size;

        pos = SDL_SeekRW(context, 0, SDL_RW_SEEK_CUR);
        if (pos < 0) {
            return -1;
        }
        size = SDL_SeekRW(context, 0, SDL_RW_SEEK_END);

        SDL_SeekRW(context, pos, SDL_RW_SEEK_SET);
        return size;
    }
    return context->iface.size(context->userdata);
}

Sint64 SDL_SeekRW(SDL_RWops *context, Sint64 offset, int whence)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    } else if (!context->iface.seek) {
        return SDL_Unsupported();
    }
    return context->iface.seek(context->userdata, offset, whence);
}

Sint64 SDL_TellRW(SDL_RWops *context)
{
    return SDL_SeekRW(context, 0, SDL_RW_SEEK_CUR);
}

size_t SDL_ReadRW(SDL_RWops *context, void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    } else if (!context->iface.read) {
        context->status = SDL_RWOPS_STATUS_WRITEONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_RWOPS_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->iface.read(context->userdata, ptr, size);
    if (bytes == 0 && context->status == SDL_RWOPS_STATUS_READY) {
        if (*SDL_GetError()) {
            context->status = SDL_RWOPS_STATUS_ERROR;
        } else {
            context->status = SDL_RWOPS_STATUS_EOF;
        }
    }
    return bytes;
}

size_t SDL_WriteRW(SDL_RWops *context, const void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    } else if (!context->iface.write) {
        context->status = SDL_RWOPS_STATUS_READONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_RWOPS_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->iface.write(context->userdata, ptr, size);
    if ((bytes == 0) && (context->status == SDL_RWOPS_STATUS_READY)) {
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

    bytes = SDL_WriteRW(context, string, (size_t)size);
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

    bytes = SDL_WriteRW(context, string, (size_t)size);
    SDL_free(string);
    return bytes;
}

/* Functions for dynamically reading and writing endian-specific values */

SDL_bool SDL_ReadU8(SDL_RWops *src, Uint8 *value)
{
    Uint8 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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

    if (SDL_ReadRW(src, &data, sizeof(data)) == sizeof(data)) {
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
    return (SDL_WriteRW(dst, &value, sizeof(value)) == sizeof(value));
}

SDL_bool SDL_WriteU16LE(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapLE16(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16LE(SDL_RWops *dst, Sint16 value)
{
    return SDL_WriteU16LE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU16BE(SDL_RWops *dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapBE16(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16BE(SDL_RWops *dst, Sint16 value)
{
    return SDL_WriteU16BE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU32LE(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapLE32(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32LE(SDL_RWops *dst, Sint32 value)
{
    return SDL_WriteU32LE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU32BE(SDL_RWops *dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapBE32(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32BE(SDL_RWops *dst, Sint32 value)
{
    return SDL_WriteU32BE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU64LE(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapLE64(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64LE(SDL_RWops *dst, Sint64 value)
{
    return SDL_WriteU64LE(dst, (Uint64)value);
}

SDL_bool SDL_WriteU64BE(SDL_RWops *dst, Uint64 value)
{
    const Uint64 swapped = SDL_SwapBE64(value);
    return (SDL_WriteRW(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64BE(SDL_RWops *dst, Sint64 value)
{
    return SDL_WriteU64BE(dst, (Uint64)value);
}
