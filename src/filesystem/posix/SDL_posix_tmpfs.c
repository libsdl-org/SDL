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
#include "../SDL_sysfilesystem.h"
#include "../../file/SDL_iostream_c.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

SDL_IOStream *SDL_SYS_CreateSafeTempFile(void)
{
    FILE *file = tmpfile();

    if (!file) {
        SDL_SetError("Could not tmpfile(): %s", strerror(errno));
        return NULL;
    }

    return SDL_IOFromFP(file, true);
}

char *SDL_SYS_CreateUnsafeTempFile(void)
{
    char *file = SDL_strdup("/tmp/tmp.XXXXXX");

    if (!file) {
        return NULL;
    }

    int fd = mkstemp(file);

    if (fd < 0) {
        SDL_free(file);
        SDL_SetError("Could not mkstemp(): %s", strerror(errno));
        return NULL;
    }

    /* Normal usage of mkstemp() would use the file descriptor rather than the
       path, to avoid issues. In this function, security is assumed to be
       unimportant, so no need to worry about it. */
    /* See https://stackoverflow.com/questions/27680807/mkstemp-is-it-safe-to-close-descriptor-and-reopen-it-again */
    close(fd);

    return file;
}

char *SDL_SYS_CreateTempFolder(void)
{
    char *folder = SDL_strdup("/tmp/tmp.XXXXXX");

    if (!folder) {
        return NULL;
    }

    char *res = mkdtemp(folder);

    if (!res) {
        SDL_free(folder);
        SDL_SetError("Could not mkdtemp(): %s", strerror(errno));
        return NULL;
    }

    return folder;
}
