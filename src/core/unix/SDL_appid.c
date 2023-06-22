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

#include "SDL_appid.h"
#include <unistd.h>

const char *SDL_GetExeName()
{
    static const char *proc_name = NULL;

    /* TODO: Use a fallback if BSD has no mounted procfs (OpenBSD has no procfs at all) */
    if (!proc_name) {
#if defined(__LINUX__) || defined(__FREEBSD__) || defined (__NETBSD__)
        static char linkfile[1024];
        int linksize;

#if defined(__LINUX__)
        const char *proc_path = "/proc/self/exe";
#elif defined(__FREEBSD__)
        const char *proc_path = "/proc/curproc/file";
#elif defined(__NETBSD__)
        const char *proc_path = "/proc/curproc/exe";
#endif
        linksize = readlink(proc_path, linkfile, sizeof(linkfile) - 1);
        if (linksize > 0) {
            linkfile[linksize] = '\0';
            proc_name = SDL_strrchr(linkfile, '/');
            if (proc_name) {
                ++proc_name;
            } else {
                proc_name = linkfile;
            }
        }
#endif
    }

    return proc_name;
}

const char *SDL_GetAppID()
{
    /* Always check the hint, as it may have changed */
    const char *id_str = SDL_GetHint(SDL_HINT_APP_ID);

    if (!id_str) {
        /* If the hint isn't set, try to use the application's executable name */
        id_str = SDL_GetExeName();
    }

    if (!id_str) {
        /* Finally, use the default we've used forever */
        id_str = "SDL_App";
    }

    return id_str;
}
