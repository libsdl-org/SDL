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

#ifdef SDL_FILESYSTEM_DOS

#include <sys/stat.h>

#include "../../core/dos/SDL_dos.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

static char *GetExePath(void)
{
    /* As of MS-DOS 3.0, you can get the full path to the EXE from the very end of the
        environment table, which is discovered through the PSP:
        https://en.wikipedia.org/wiki/Program_Segment_Prefix

       An EXE is launched with the PSP's segment in DS, but we'll use a DOS API to obtain it
        later, in case we don't control startup. https://stanislavs.org/helppc/int_21-62.html

       The table pointed to by the word at offset 0x2C in the PSP is just a collection
        of ASCIZ strings, with a blank string signifying the end. The EXE path is
        after that.

       https://stackoverflow.com/questions/60928997/how-to-get-environment-variables-in-a-dos-assembly-program
     */
    __dpmi_regs regs;
    regs.x.ax = 0x6200;  /* get PSP address */
    regs.x.bx = 0x0000;
    __dpmi_int(0x21, &regs);

    const Uint32 pspseg = (Uint32) regs.x.bx;
    const Uint32 envsel = (Uint32) DOS_PeekUint16((pspseg << 16) | 0x2C);

    int zero_count = 0;
    int offset;
    for (offset = 0; (offset < 0xFFFF) && (zero_count < 2); offset++) {
        const char ch = (char) _farpeekb(envsel, offset);
        if (ch == 0) {
            zero_count++;
        } else {
            zero_count = 0;
        }
    }

    if (zero_count != 2) {
        return NULL;  // uhoh
    } else if (_farpeekw(envsel, offset) < 1) {  // there's a Uint16 here that represents number of extension strings. In practice it's always 1 and that one string is the path we need.
        return NULL;  // uhoh
    }

    offset += 2;

    int slen = 0;
    for (unsigned long i = (unsigned long) offset; _farpeekb(envsel, i) != 0; i++) {
        slen++;
    }

    slen++;  /* count the null terminator. */

    char *retval = (char *) SDL_malloc(slen);
    if (retval) {
        for (int i = 0; i < slen; i++) {
            const char ch = (char) _farpeekb(envsel, offset + i);
            retval[i] = (ch == '\\') ? '/' : ch;     // I don't know if this is a good idea. Drop DOS path separators, use Unix style instead.
        }
    }

    return retval;
}

char *SDL_SYS_GetBasePath(void)
{
    char *path = GetExePath();  // look up full path of the current process's EXE file.
    if (!path) {
        return NULL;  // error message was already set.
    }

    char *ptr = SDL_strrchr(path, '/');
    SDL_assert(ptr != NULL);  // Should have been an absolute path.

    ptr[1] = '\0'; // chop off filename, leave '/'.

    ptr = (char *) SDL_realloc(path, ((size_t) (ptr - path)) + 2);  // try to shrink this allocation down a little.
    return ptr ? ptr : path;  // return shrunk buffer if shrink worked out, unchanged original buffer if not.
}

char *SDL_SYS_GetExeName(void)
{
    char *path = GetExePath();  // look up full path of the current process's EXE file.
    if (!path) {
        return NULL;  // error message was already set.
    }

    char *ptr = SDL_strrchr(path, '/');
    const size_t slen = SDL_strlen(ptr);  // counts null terminator because we're still sitting on path separator.
    SDL_memmove(path, ptr + 1, slen);  // move filename string to start of SDL_realloc'd region.
    ptr = (char *) SDL_realloc(path, slen);  // try to shrink this allocation down a little.
    return ptr ? ptr : path;  // return shrunk buffer if shrink worked out, unchanged original buffer if not.
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    char *result = NULL;
    size_t len;
    const char *base = SDL_GetBasePath();
    if (!base) {
        return NULL;
    }

    len = SDL_strlen(base) + SDL_strlen(org) + SDL_strlen(app) + 4;
    result = (char *)SDL_malloc(len);
    if (result) {
        if (*org) {
            SDL_snprintf(result, len, "%s%s", base, org);
            mkdir(result, 0755);
            SDL_snprintf(result, len, "%s%s/%s/", base, org, app);
        } else {
            SDL_snprintf(result, len, "%s%s/", base, app);
        }

        mkdir(result, 0755);
    }

    return result;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    SDL_Unsupported();
    return NULL;
}

#endif // SDL_FILESYSTEM_DOS
