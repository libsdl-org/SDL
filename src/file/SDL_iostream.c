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

struct SDL_IOStream
{
    SDL_IOStreamInterface iface;
    void *userdata;
    SDL_IOStatus status;
    SDL_PropertiesID props;
};


#ifdef SDL_PLATFORM_APPLE
#include "cocoa/SDL_iostreambundlesupport.h"
#endif /* SDL_PLATFORM_APPLE */

#ifdef SDL_PLATFORM_3DS
#include "n3ds/SDL_iostreamromfs.h"
#endif /* SDL_PLATFORM_3DS */

#ifdef SDL_PLATFORM_ANDROID
#include <unistd.h>
#include "../core/android/SDL_android.h"
#endif

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)

typedef struct IOStreamWindowsData
{
    SDL_bool append;
    HANDLE h;
    void *data;
    size_t size;
    size_t left;
} IOStreamWindowsData;


/* Functions to read/write Win32 API file pointers */
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif

#define READAHEAD_BUFFER_SIZE 1024

static int SDLCALL windows_file_open(IOStreamWindowsData *iodata, const char *filename, const char *mode)
{
#if !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES) && !defined(SDL_PLATFORM_WINRT)
    UINT old_error_mode;
#endif
    HANDLE h;
    DWORD r_right, w_right;
    DWORD must_exist, truncate;
    int a_mode;

    SDL_zerop(iodata);
    iodata->h = INVALID_HANDLE_VALUE; /* mark this as unusable */

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

    iodata->data = (char *)SDL_malloc(READAHEAD_BUFFER_SIZE);
    if (!iodata->data) {
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
        SDL_free(iodata->data);
        iodata->data = NULL;
        SDL_SetError("Couldn't open %s", filename);
        return -2; /* failed (CreateFile) */
    }
    iodata->h = h;
    iodata->append = a_mode ? SDL_TRUE : SDL_FALSE;

    return 0; /* ok */
}

static Sint64 SDLCALL windows_file_size(void *userdata)
{
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) userdata;
    LARGE_INTEGER size;

    if (!GetFileSizeEx(iodata->h, &size)) {
        return WIN_SetError("windows_file_size");
    }

    return size.QuadPart;
}

static Sint64 SDLCALL windows_file_seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) userdata;
    DWORD windowswhence;
    LARGE_INTEGER windowsoffset;

    // FIXME: We may be able to satisfy the seek within buffered data
    if ((whence == SDL_IO_SEEK_CUR) && (iodata->left)) {
        offset -= iodata->left;
    }
    iodata->left = 0;

    switch (whence) {
    case SDL_IO_SEEK_SET:
        windowswhence = FILE_BEGIN;
        break;
    case SDL_IO_SEEK_CUR:
        windowswhence = FILE_CURRENT;
        break;
    case SDL_IO_SEEK_END:
        windowswhence = FILE_END;
        break;
    default:
        return SDL_SetError("windows_file_seek: Unknown value for 'whence'");
    }

    windowsoffset.QuadPart = offset;
    if (!SetFilePointerEx(iodata->h, windowsoffset, &windowsoffset, windowswhence)) {
        return WIN_SetError("windows_file_seek");
    }
    return windowsoffset.QuadPart;
}

static size_t SDLCALL windows_file_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) userdata;
    size_t total_need = size;
    size_t total_read = 0;
    size_t read_ahead;
    DWORD bytes;

    if (iodata->left > 0) {
        void *data = (char *)iodata->data +
                     iodata->size -
                     iodata->left;
        read_ahead = SDL_min(total_need, iodata->left);
        SDL_memcpy(ptr, data, read_ahead);
        iodata->left -= read_ahead;

        if (read_ahead == total_need) {
            return size;
        }
        ptr = (char *)ptr + read_ahead;
        total_need -= read_ahead;
        total_read += read_ahead;
    }

    if (total_need < READAHEAD_BUFFER_SIZE) {
        if (!ReadFile(iodata->h, iodata->data, READAHEAD_BUFFER_SIZE, &bytes, NULL)) {
            SDL_SetError("Error reading from datastream");
            return 0;
        }
        read_ahead = SDL_min(total_need, bytes);
        SDL_memcpy(ptr, iodata->data, read_ahead);
        iodata->size = bytes;
        iodata->left = bytes - read_ahead;
        total_read += read_ahead;
    } else {
        if (!ReadFile(iodata->h, ptr, (DWORD)total_need, &bytes, NULL)) {
            SDL_SetError("Error reading from datastream");
            return 0;
        }
        total_read += bytes;
    }
    return total_read;
}

static size_t SDLCALL windows_file_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) userdata;
    const size_t total_bytes = size;
    DWORD bytes;

    if (iodata->left) {
        if (!SetFilePointer(iodata->h, -(LONG)iodata->left, NULL, FILE_CURRENT)) {
            SDL_SetError("Error seeking in datastream");
            return 0;
        }
        iodata->left = 0;
    }

    /* if in append mode, we must go to the EOF before write */
    if (iodata->append) {
        LARGE_INTEGER windowsoffset;
        windowsoffset.QuadPart = 0;
        if (!SetFilePointerEx(iodata->h, windowsoffset, &windowsoffset, FILE_END)) {
            SDL_SetError("Error seeking in datastream");
            return 0;
        }
    }

    if (!WriteFile(iodata->h, ptr, (DWORD)total_bytes, &bytes, NULL)) {
        SDL_SetError("Error writing to datastream");
        return 0;
    }

    return bytes;
}

static int SDLCALL windows_file_close(void *userdata)
{
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) userdata;
    if (iodata->h != INVALID_HANDLE_VALUE) {
        CloseHandle(iodata->h);
        iodata->h = INVALID_HANDLE_VALUE; /* to be sure */
    }
    SDL_free(iodata->data);
    SDL_free(iodata);
    return 0;
}
#endif /* defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) */

#if defined(HAVE_STDIO_H) && !(defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK))

/* Functions to read/write stdio file pointers. Not used for windows. */

typedef struct IOStreamStdioData
{
    FILE *fp;
    SDL_bool autoclose;
} IOStreamStdioData;

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

static Sint64 SDLCALL stdio_seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    IOStreamStdioData *iodata = (IOStreamStdioData *) userdata;
    int stdiowhence;

    switch (whence) {
    case SDL_IO_SEEK_SET:
        stdiowhence = SEEK_SET;
        break;
    case SDL_IO_SEEK_CUR:
        stdiowhence = SEEK_CUR;
        break;
    case SDL_IO_SEEK_END:
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

    if (fseek(iodata->fp, (fseek_off_t)offset, stdiowhence) == 0) {
        const Sint64 pos = ftell(iodata->fp);
        if (pos < 0) {
            return SDL_SetError("Couldn't get stream offset");
        }
        return pos;
    }
    return SDL_SetError("Error seeking in datastream");
}

static size_t SDLCALL stdio_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamStdioData *iodata = (IOStreamStdioData *) userdata;
    const size_t bytes = fread(ptr, 1, size, iodata->fp);
    if (bytes == 0 && ferror(iodata->fp)) {
        SDL_SetError("Error reading from datastream");
    }
    return bytes;
}

static size_t SDLCALL stdio_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamStdioData *iodata = (IOStreamStdioData *) userdata;
    const size_t bytes = fwrite(ptr, 1, size, iodata->fp);
    if (bytes == 0 && ferror(iodata->fp)) {
        SDL_SetError("Error writing to datastream");
    }
    return bytes;
}

static int SDLCALL stdio_close(void *userdata)
{
    IOStreamStdioData *iodata = (IOStreamStdioData *) userdata;
    int status = 0;
    if (iodata->autoclose) {
        if (fclose(iodata->fp) != 0) {
            status = SDL_SetError("Error writing to datastream");
        }
    }
    SDL_free(iodata);
    return status;
}

static SDL_IOStream *SDL_IOFromFP(FILE *fp, SDL_bool autoclose)
{
    IOStreamStdioData *iodata = (IOStreamStdioData *) SDL_malloc(sizeof (*iodata));
    if (!iodata) {
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    // There's no stdio_size because SDL_GetIOSize emulates it the same way we'd do it for stdio anyhow.
    iface.seek = stdio_seek;
    iface.read = stdio_read;
    iface.write = stdio_write;
    iface.close = stdio_close;

    iodata->fp = fp;
    iodata->autoclose = autoclose;

    SDL_IOStream *iostr = SDL_OpenIO(&iface, iodata);
    if (!iostr) {
        iface.close(iodata);
    } else {
        const SDL_PropertiesID props = SDL_GetIOProperties(iostr);
        if (props) {
            SDL_SetProperty(props, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, fp);
        }
    }

    return iostr;
}
#endif /* !HAVE_STDIO_H && !(SDL_PLATFORM_WIN32 || SDL_PLATFORM_GDK) */

/* Functions to read/write memory pointers */

typedef struct IOStreamMemData
{
    Uint8 *base;
    Uint8 *here;
    Uint8 *stop;
} IOStreamMemData;

static Sint64 SDLCALL mem_size(void *userdata)
{
    const IOStreamMemData *iodata = (IOStreamMemData *) userdata;
    return (iodata->stop - iodata->base);
}

static Sint64 SDLCALL mem_seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    IOStreamMemData *iodata = (IOStreamMemData *) userdata;
    Uint8 *newpos;

    switch (whence) {
    case SDL_IO_SEEK_SET:
        newpos = iodata->base + offset;
        break;
    case SDL_IO_SEEK_CUR:
        newpos = iodata->here + offset;
        break;
    case SDL_IO_SEEK_END:
        newpos = iodata->stop + offset;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }
    if (newpos < iodata->base) {
        newpos = iodata->base;
    }
    if (newpos > iodata->stop) {
        newpos = iodata->stop;
    }
    iodata->here = newpos;
    return (Sint64)(iodata->here - iodata->base);
}

static size_t mem_io(void *userdata, void *dst, const void *src, size_t size)
{
    IOStreamMemData *iodata = (IOStreamMemData *) userdata;
    const size_t mem_available = (iodata->stop - iodata->here);
    if (size > mem_available) {
        size = mem_available;
    }
    SDL_memcpy(dst, src, size);
    iodata->here += size;
    return size;
}

static size_t SDLCALL mem_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamMemData *iodata = (IOStreamMemData *) userdata;
    return mem_io(userdata, ptr, iodata->here, size);
}

static size_t SDLCALL mem_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamMemData *iodata = (IOStreamMemData *) userdata;
    return mem_io(userdata, iodata->here, ptr, size);
}

static int SDLCALL mem_close(void *userdata)
{
    SDL_free(userdata);
    return 0;
}

/* Functions to create SDL_IOStream structures from various data sources */

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

SDL_IOStream *SDL_IOFromFile(const char *file, const char *mode)
{
    SDL_IOStream *iostr = NULL;

    if (!file || !*file) {
        SDL_InvalidParamError("file");
        return NULL;
    }
    if (!mode || !*mode) {
        SDL_InvalidParamError("mode");
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
            return SDL_IOFromFP(fp, 1);
        }
    } else if (SDL_strncmp(file, "content://", 10) == 0) {
        /* Try opening content:// URI */
        int fd = Android_JNI_OpenFileDescriptor(file, mode);
        if (fd == -1) {
            /* SDL error is already set. */
            return NULL;
        }

        FILE *fp = fdopen(fd, mode);
        if (!fp) {
            close(fd);
            SDL_SetError("Unable to open file descriptor (%d) from URI %s", fd, file);
            return NULL;
        }

        return SDL_IOFromFP(fp, SDL_TRUE);
    } else {
        /* Try opening it from internal storage if it's a relative path */
        char *path = NULL;
        SDL_asprintf(&path, "%s/%s", SDL_AndroidGetInternalStoragePath(), file);
        if (path) {
            FILE *fp = fopen(path, mode);
            SDL_free(path);
            if (fp) {
                if (!IsRegularFileOrPipe(fp)) {
                    fclose(fp);
                    SDL_SetError("%s is not a regular file or pipe", path);
                    return NULL;
                }
                return SDL_IOFromFP(fp, 1);
            }
        }
    }
#endif /* HAVE_STDIO_H */

    /* Try to open the file from the asset system */

    void *iodata = NULL;
    if (Android_JNI_FileOpen(&iodata, file, mode) < 0) {
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    iface.size = Android_JNI_FileSize;
    iface.seek = Android_JNI_FileSeek;
    iface.read = Android_JNI_FileRead;
    iface.write = Android_JNI_FileWrite;
    iface.close = Android_JNI_FileClose;

    iostr = SDL_OpenIO(&iface, iodata);
    if (!iostr) {
        iface.close(iodata);
    } else {
        const SDL_PropertiesID props = SDL_GetIOProperties(iostr);
        if (props) {
            SDL_SetProperty(props, SDL_PROP_IOSTREAM_ANDROID_AASSET_POINTER, iodata);
        }
    }

#elif defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_GDK) || defined(SDL_PLATFORM_WINRT)
    IOStreamWindowsData *iodata = (IOStreamWindowsData *) SDL_malloc(sizeof (*iodata));
    if (!iodata) {
        return NULL;
    }

    if (windows_file_open(iodata, file, mode) < 0) {
        windows_file_close(iodata);
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    iface.size = windows_file_size;
    iface.seek = windows_file_seek;
    iface.read = windows_file_read;
    iface.write = windows_file_write;
    iface.close = windows_file_close;

    iostr = SDL_OpenIO(&iface, iodata);
    if (!iostr) {
        windows_file_close(iodata);
    } else {
        const SDL_PropertiesID props = SDL_GetIOProperties(iostr);
        if (props) {
            SDL_SetProperty(props, SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, iodata->h);
        }
    }

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
            iostr = SDL_IOFromFP(fp, SDL_TRUE);
        }
    }

#else
    SDL_SetError("SDL not compiled with stdio support");
#endif /* !HAVE_STDIO_H */

    return iostr;
}

SDL_IOStream *SDL_IOFromMem(void *mem, size_t size)
{
    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    } else if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    IOStreamMemData *iodata = (IOStreamMemData *) SDL_malloc(sizeof (*iodata));
    if (!iodata) {
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    iface.size = mem_size;
    iface.seek = mem_seek;
    iface.read = mem_read;
    iface.write = mem_write;
    iface.close = mem_close;

    iodata->base = (Uint8 *)mem;
    iodata->here = iodata->base;
    iodata->stop = iodata->base + size;

    SDL_IOStream *iostr = SDL_OpenIO(&iface, iodata);
    if (!iostr) {
        SDL_free(iodata);
    }
    return iostr;
}

SDL_IOStream *SDL_IOFromConstMem(const void *mem, size_t size)
{
    if (!mem) {
        SDL_InvalidParamError("mem");
        return NULL;
    } else if (!size) {
        SDL_InvalidParamError("size");
        return NULL;
    }

    IOStreamMemData *iodata = (IOStreamMemData *) SDL_malloc(sizeof (*iodata));
    if (!iodata) {
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    iface.size = mem_size;
    iface.seek = mem_seek;
    iface.read = mem_read;
    // leave iface.write as NULL.
    iface.close = mem_close;

    iodata->base = (Uint8 *)mem;
    iodata->here = iodata->base;
    iodata->stop = iodata->base + size;

    SDL_IOStream *iostr = SDL_OpenIO(&iface, iodata);
    if (!iostr) {
        SDL_free(iodata);
    }
    return iostr;
}

typedef struct IOStreamDynamicMemData
{
    SDL_IOStream *stream;
    IOStreamMemData data;
    Uint8 *end;
} IOStreamDynamicMemData;

static Sint64 SDLCALL dynamic_mem_size(void *userdata)
{
    IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) userdata;
    return mem_size(&iodata->data);
}

static Sint64 SDLCALL dynamic_mem_seek(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) userdata;
    return mem_seek(&iodata->data, offset, whence);
}

static size_t SDLCALL dynamic_mem_read(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) userdata;
    return mem_io(&iodata->data, ptr, iodata->data.here, size);
}

static int dynamic_mem_realloc(IOStreamDynamicMemData *iodata, size_t size)
{
    size_t chunksize = (size_t)SDL_GetNumberProperty(SDL_GetIOProperties(iodata->stream), SDL_PROP_IOSTREAM_DYNAMIC_CHUNKSIZE_NUMBER, 0);
    if (!chunksize) {
        chunksize = 1024;
    }

    // We're intentionally allocating more memory than needed so it can be null terminated
    size_t chunks = (((iodata->end - iodata->data.base) + size) / chunksize) + 1;
    size_t length = (chunks * chunksize);
    Uint8 *base = (Uint8 *)SDL_realloc(iodata->data.base, length);
    if (!base) {
        return -1;
    }

    size_t here_offset = (iodata->data.here - iodata->data.base);
    size_t stop_offset = (iodata->data.stop - iodata->data.base);
    iodata->data.base = base;
    iodata->data.here = base + here_offset;
    iodata->data.stop = base + stop_offset;
    iodata->end = base + length;
    return SDL_SetProperty(SDL_GetIOProperties(iodata->stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, base);
}

static size_t SDLCALL dynamic_mem_write(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) userdata;
    if (size > (size_t)(iodata->data.stop - iodata->data.here)) {
        if (size > (size_t)(iodata->end - iodata->data.here)) {
            if (dynamic_mem_realloc(iodata, size) < 0) {
                return 0;
            }
        }
        iodata->data.stop = iodata->data.here + size;
    }
    return mem_io(&iodata->data, iodata->data.here, ptr, size);
}

static int SDLCALL dynamic_mem_close(void *userdata)
{
    const IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) userdata;
    void *mem = SDL_GetProperty(SDL_GetIOProperties(iodata->stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
    if (mem) {
        SDL_free(mem);
    }
    SDL_free(userdata);
    return 0;
}

SDL_IOStream *SDL_IOFromDynamicMem(void)
{
    IOStreamDynamicMemData *iodata = (IOStreamDynamicMemData *) SDL_malloc(sizeof (*iodata));
    if (!iodata) {
        return NULL;
    }

    SDL_IOStreamInterface iface;
    SDL_zero(iface);
    iface.size = dynamic_mem_size;
    iface.seek = dynamic_mem_seek;
    iface.read = dynamic_mem_read;
    iface.write = dynamic_mem_write;
    iface.close = dynamic_mem_close;

    iodata->data.base = NULL;
    iodata->data.here = NULL;
    iodata->data.stop = NULL;
    iodata->end = NULL;

    SDL_IOStream *iostr = SDL_OpenIO(&iface, iodata);
    if (iostr) {
        iodata->stream = iostr;
    } else {
        SDL_free(iodata);
    }
    return iostr;
}

SDL_IOStatus SDL_GetIOStatus(SDL_IOStream *context)
{
    if (!context) {
        SDL_InvalidParamError("context");
        return SDL_IO_STATUS_ERROR;
    }
    return context->status;
}

SDL_IOStream *SDL_OpenIO(const SDL_IOStreamInterface *iface, void *userdata)
{
    if (!iface) {
        SDL_InvalidParamError("iface");
        return NULL;
    }

    SDL_IOStream *iostr = (SDL_IOStream *)SDL_calloc(1, sizeof(*iostr));
    if (iostr) {
        SDL_copyp(&iostr->iface, iface);
        iostr->userdata = userdata;
    }
    return iostr;
}

int SDL_CloseIO(SDL_IOStream *iostr)
{
    int retval = 0;
    if (iostr) {
        if (iostr->iface.close) {
            retval = iostr->iface.close(iostr->userdata);
        }
        SDL_DestroyProperties(iostr->props);
        SDL_free(iostr);
    }
    return retval;
}

/* Load all the data from an SDL data stream */
void *SDL_LoadFile_IO(SDL_IOStream *src, size_t *datasize, SDL_bool closeio)
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

    size = SDL_GetIOSize(src);
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

        size_read = SDL_ReadIO(src, data + size_total, (size_t)(size - size_total));
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
    if (closeio && src) {
        SDL_CloseIO(src);
    }
    return data;
}

void *SDL_LoadFile(const char *file, size_t *datasize)
{
    return SDL_LoadFile_IO(SDL_IOFromFile(file, "rb"), datasize, SDL_TRUE);
}

SDL_PropertiesID SDL_GetIOProperties(SDL_IOStream *context)
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

Sint64 SDL_GetIOSize(SDL_IOStream *context)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    }
    if (!context->iface.size) {
        Sint64 pos, size;

        pos = SDL_SeekIO(context, 0, SDL_IO_SEEK_CUR);
        if (pos < 0) {
            return -1;
        }
        size = SDL_SeekIO(context, 0, SDL_IO_SEEK_END);

        SDL_SeekIO(context, pos, SDL_IO_SEEK_SET);
        return size;
    }
    return context->iface.size(context->userdata);
}

Sint64 SDL_SeekIO(SDL_IOStream *context, Sint64 offset, SDL_IOWhence whence)
{
    if (!context) {
        return SDL_InvalidParamError("context");
    } else if (!context->iface.seek) {
        return SDL_Unsupported();
    }
    return context->iface.seek(context->userdata, offset, whence);
}

Sint64 SDL_TellIO(SDL_IOStream *context)
{
    return SDL_SeekIO(context, 0, SDL_IO_SEEK_CUR);
}

size_t SDL_ReadIO(SDL_IOStream *context, void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    } else if (!context->iface.read) {
        context->status = SDL_IO_STATUS_WRITEONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_IO_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->iface.read(context->userdata, ptr, size, &context->status);
    if (bytes == 0 && context->status == SDL_IO_STATUS_READY) {
        if (*SDL_GetError()) {
            context->status = SDL_IO_STATUS_ERROR;
        } else {
            context->status = SDL_IO_STATUS_EOF;
        }
    }
    return bytes;
}

size_t SDL_WriteIO(SDL_IOStream *context, const void *ptr, size_t size)
{
    size_t bytes;

    if (!context) {
        SDL_InvalidParamError("context");
        return 0;
    } else if (!context->iface.write) {
        context->status = SDL_IO_STATUS_READONLY;
        SDL_Unsupported();
        return 0;
    }

    context->status = SDL_IO_STATUS_READY;
    SDL_ClearError();

    if (size == 0) {
        return 0;
    }

    bytes = context->iface.write(context->userdata, ptr, size, &context->status);
    if ((bytes == 0) && (context->status == SDL_IO_STATUS_READY)) {
        context->status = SDL_IO_STATUS_ERROR;
    }
    return bytes;
}

size_t SDL_IOprintf(SDL_IOStream *context, SDL_PRINTF_FORMAT_STRING const char *fmt, ...)
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

    bytes = SDL_WriteIO(context, string, (size_t)size);
    SDL_free(string);
    return bytes;
}

size_t SDL_IOvprintf(SDL_IOStream *context, SDL_PRINTF_FORMAT_STRING const char *fmt, va_list ap)
{
    int size;
    char *string;
    size_t bytes;

    size = SDL_vasprintf(&string, fmt, ap);
    if (size < 0) {
        return 0;
    }

    bytes = SDL_WriteIO(context, string, (size_t)size);
    SDL_free(string);
    return bytes;
}

/* Functions for dynamically reading and writing endian-specific values */

SDL_bool SDL_ReadU8(SDL_IOStream *src, Uint8 *value)
{
    Uint8 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = data;
    }
    return result;
}

SDL_bool SDL_ReadS8(SDL_IOStream *src, Sint8 *value)
{
    Sint8 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = data;
    }
    return result;
}

SDL_bool SDL_ReadU16LE(SDL_IOStream *src, Uint16 *value)
{
    Uint16 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap16LE(data);
    }
    return result;
}

SDL_bool SDL_ReadS16LE(SDL_IOStream *src, Sint16 *value)
{
    return SDL_ReadU16LE(src, (Uint16 *)value);
}

SDL_bool SDL_ReadU16BE(SDL_IOStream *src, Uint16 *value)
{
    Uint16 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap16BE(data);
    }
    return result;
}

SDL_bool SDL_ReadS16BE(SDL_IOStream *src, Sint16 *value)
{
    return SDL_ReadU16BE(src, (Uint16 *)value);
}

SDL_bool SDL_ReadU32LE(SDL_IOStream *src, Uint32 *value)
{
    Uint32 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap32LE(data);
    }
    return result;
}

SDL_bool SDL_ReadS32LE(SDL_IOStream *src, Sint32 *value)
{
    return SDL_ReadU32LE(src, (Uint32 *)value);
}

SDL_bool SDL_ReadU32BE(SDL_IOStream *src, Uint32 *value)
{
    Uint32 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap32BE(data);
    }
    return result;
}

SDL_bool SDL_ReadS32BE(SDL_IOStream *src, Sint32 *value)
{
    return SDL_ReadU32BE(src, (Uint32 *)value);
}

SDL_bool SDL_ReadU64LE(SDL_IOStream *src, Uint64 *value)
{
    Uint64 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap64LE(data);
    }
    return result;
}

SDL_bool SDL_ReadS64LE(SDL_IOStream *src, Sint64 *value)
{
    return SDL_ReadU64LE(src, (Uint64 *)value);
}

SDL_bool SDL_ReadU64BE(SDL_IOStream *src, Uint64 *value)
{
    Uint64 data = 0;
    SDL_bool result = SDL_FALSE;

    if (SDL_ReadIO(src, &data, sizeof(data)) == sizeof(data)) {
        result = SDL_TRUE;
    }
    if (value) {
        *value = SDL_Swap64BE(data);
    }
    return result;
}

SDL_bool SDL_ReadS64BE(SDL_IOStream *src, Sint64 *value)
{
    return SDL_ReadU64BE(src, (Uint64 *)value);
}

SDL_bool SDL_WriteU8(SDL_IOStream *dst, Uint8 value)
{
    return (SDL_WriteIO(dst, &value, sizeof(value)) == sizeof(value));
}

SDL_bool SDL_WriteS8(SDL_IOStream *dst, Sint8 value)
{
    return (SDL_WriteIO(dst, &value, sizeof(value)) == sizeof(value));
}

SDL_bool SDL_WriteU16LE(SDL_IOStream *dst, Uint16 value)
{
    const Uint16 swapped = SDL_Swap16LE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16LE(SDL_IOStream *dst, Sint16 value)
{
    return SDL_WriteU16LE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU16BE(SDL_IOStream *dst, Uint16 value)
{
    const Uint16 swapped = SDL_Swap16BE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS16BE(SDL_IOStream *dst, Sint16 value)
{
    return SDL_WriteU16BE(dst, (Uint16)value);
}

SDL_bool SDL_WriteU32LE(SDL_IOStream *dst, Uint32 value)
{
    const Uint32 swapped = SDL_Swap32LE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32LE(SDL_IOStream *dst, Sint32 value)
{
    return SDL_WriteU32LE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU32BE(SDL_IOStream *dst, Uint32 value)
{
    const Uint32 swapped = SDL_Swap32BE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS32BE(SDL_IOStream *dst, Sint32 value)
{
    return SDL_WriteU32BE(dst, (Uint32)value);
}

SDL_bool SDL_WriteU64LE(SDL_IOStream *dst, Uint64 value)
{
    const Uint64 swapped = SDL_Swap64LE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64LE(SDL_IOStream *dst, Sint64 value)
{
    return SDL_WriteU64LE(dst, (Uint64)value);
}

SDL_bool SDL_WriteU64BE(SDL_IOStream *dst, Uint64 value)
{
    const Uint64 swapped = SDL_Swap64BE(value);
    return (SDL_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped));
}

SDL_bool SDL_WriteS64BE(SDL_IOStream *dst, Sint64 value)
{
    return SDL_WriteU64BE(dst, (Uint64)value);
}
