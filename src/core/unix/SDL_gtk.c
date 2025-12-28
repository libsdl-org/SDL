/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_gtk.h"

#include <errno.h>
#include <unistd.h>

#ifndef HAVE_GETRESUID
// Non-POSIX, but Linux and some BSDs have it.
// To reduce the number of code paths, if getresuid() isn't available at
// compile-time, we behave as though it existed but failed at runtime.
static inline int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    errno = ENOSYS;
    return -1;
}
#endif

#ifndef HAVE_GETRESGID
// Same as getresuid() but for the primary group
static inline int getresgid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    errno = ENOSYS;
    return -1;
}
#endif

bool SDL_CanUseGtk(void)
{
    // "Real", "effective" and "saved" IDs: see e.g. Linux credentials(7)
    uid_t ruid = -1, euid = -1, suid = -1;
    gid_t rgid = -1, egid = -1, sgid = -1;

    if (!SDL_GetHintBoolean("SDL_ENABLE_GTK", true)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Not using GTK due to hint");
        return false;
    }

    // This is intended to match the check in gtkmain.c, rather than being
    // an exhaustive check for having elevated privileges: as a result
    // we don't use Linux getauxval() or prctl PR_GET_DUMPABLE,
    // BSD issetugid(), or similar OS-specific detection

    if (getresuid(&ruid, &euid, &suid) != 0) {
        ruid = suid = getuid();
        euid = geteuid();
    }

    if (getresgid(&rgid, &egid, &sgid) != 0) {
        rgid = sgid = getgid();
        egid = getegid();
    }

    // Real ID != effective ID means we are setuid or setgid:
    // GTK will refuse to initialize, and instead will call exit().
    if (ruid != euid || rgid != egid) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Not using GTK due to setuid/setgid");
        return false;
    }

    // Real ID != saved ID means we are setuid or setgid, we previously
    // dropped privileges, but we can regain them; this protects against
    // accidents but does not protect against arbitrary code execution.
    // Again, GTK will refuse to initialize if this is the case.
    if (ruid != suid || rgid != sgid) {
        SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Not using GTK due to saved uid/gid");
        return false;
    }

    return true;
}
