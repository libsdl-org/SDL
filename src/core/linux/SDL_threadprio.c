/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#ifdef __LINUX__

#include "SDL_stdinc.h"

#if !SDL_THREADS_DISABLED
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include "SDL_system.h"

#if SDL_USE_LIBDBUS
#include "SDL_dbus.h"
/* d-bus queries to org.freedesktop.RealtimeKit1. */
#define RTKIT_DBUS_NODE "org.freedesktop.RealtimeKit1"
#define RTKIT_DBUS_PATH "/org/freedesktop/RealtimeKit1"
#define RTKIT_DBUS_INTERFACE "org.freedesktop.RealtimeKit1"

static pthread_once_t rtkit_initialize_once = PTHREAD_ONCE_INIT;
static Sint32 rtkit_min_nice_level = -20;

static void
rtkit_initialize()
{
    SDL_DBusContext *dbus = SDL_DBus_GetContext();

    /* Try getting minimum nice level: this is often greater than PRIO_MIN (-20). */
    if (!dbus || !SDL_DBus_QueryPropertyOnConnection(dbus->system_conn, RTKIT_DBUS_NODE, RTKIT_DBUS_PATH, RTKIT_DBUS_INTERFACE, "MinNiceLevel",
                                            DBUS_TYPE_INT32, &rtkit_min_nice_level)) {
        rtkit_min_nice_level = -20;
    }
}

static SDL_bool
rtkit_setpriority(pid_t thread, int nice_level)
{
    Uint64 ui64 = (Uint64)thread;
    Sint32 si32 = (Sint32)nice_level;
    SDL_DBusContext *dbus = SDL_DBus_GetContext();

    pthread_once(&rtkit_initialize_once, rtkit_initialize);

    if (si32 < rtkit_min_nice_level)
        si32 = rtkit_min_nice_level;

    if (!dbus || !SDL_DBus_CallMethodOnConnection(dbus->system_conn,
            RTKIT_DBUS_NODE, RTKIT_DBUS_PATH, RTKIT_DBUS_INTERFACE, "MakeThreadHighPriority",
            DBUS_TYPE_UINT64, &ui64, DBUS_TYPE_INT32, &si32, DBUS_TYPE_INVALID,
            DBUS_TYPE_INVALID)) {
        return SDL_FALSE;
    }
    return SDL_TRUE;
}
#endif /* dbus */
#endif /* threads */


/* this is a public symbol, so it has to exist even if threads are disabled. */
int
SDL_LinuxSetThreadPriority(Sint64 threadID, int priority)
{
#if SDL_THREADS_DISABLED
    return SDL_Unsupported();
#else
    if (setpriority(PRIO_PROCESS, (id_t)threadID, priority) == 0) {
        return 0;
    }

#if SDL_USE_LIBDBUS
    /* Note that this fails if you're trying to set high priority
       and you don't have root permission. BUT DON'T RUN AS ROOT!

       You can grant the ability to increase thread priority by
       running the following command on your application binary:
          sudo setcap 'cap_sys_nice=eip' <application>

       Let's try setting priority with RealtimeKit...

       README and sample code at: http://git.0pointer.net/rtkit.git
    */
    if (rtkit_setpriority((pid_t)threadID, priority)) {
        return 0;
    }
#endif

    return SDL_SetError("setpriority() failed");
#endif
}

#endif  /* __LINUX__ */

/* vi: set ts=4 sw=4 expandtab: */
