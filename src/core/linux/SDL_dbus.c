/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_dbus.h"

#if SDL_USE_LIBDBUS
/* we never link directly to libdbus. */
#include "SDL_loadso.h"
static const char *dbus_library = "libdbus-1.so.3";
static void *dbus_handle = NULL;
static unsigned int screensaver_cookie = 0;
static SDL_DBusContext dbus = {0};

static int
LoadDBUSSyms(void)
{
    #define SDL_DBUS_SYM2(x, y) \
        if (!(dbus.x = SDL_LoadFunction(dbus_handle, #y))) return -1
        
    #define SDL_DBUS_SYM(x) \
        SDL_DBUS_SYM2(x, dbus_##x)

    SDL_DBUS_SYM(bus_get_private);
    SDL_DBUS_SYM(bus_register);
    SDL_DBUS_SYM(bus_add_match);
    SDL_DBUS_SYM(connection_open_private);
    SDL_DBUS_SYM(connection_set_exit_on_disconnect);
    SDL_DBUS_SYM(connection_get_is_connected);
    SDL_DBUS_SYM(connection_add_filter);
    SDL_DBUS_SYM(connection_try_register_object_path);
    SDL_DBUS_SYM(connection_send);
    SDL_DBUS_SYM(connection_send_with_reply_and_block);
    SDL_DBUS_SYM(connection_close);
    SDL_DBUS_SYM(connection_unref);
    SDL_DBUS_SYM(connection_flush);
    SDL_DBUS_SYM(connection_read_write);
    SDL_DBUS_SYM(connection_dispatch);
    SDL_DBUS_SYM(message_is_signal);
    SDL_DBUS_SYM(message_new_method_call);
    SDL_DBUS_SYM(message_append_args);
    SDL_DBUS_SYM(message_get_args);
    SDL_DBUS_SYM(message_iter_init);
    SDL_DBUS_SYM(message_iter_next);
    SDL_DBUS_SYM(message_iter_get_basic);
    SDL_DBUS_SYM(message_iter_get_arg_type);
    SDL_DBUS_SYM(message_iter_recurse);
    SDL_DBUS_SYM(message_unref);
    SDL_DBUS_SYM(error_init);
    SDL_DBUS_SYM(error_is_set);
    SDL_DBUS_SYM(error_free);
    SDL_DBUS_SYM(get_local_machine_id);
    SDL_DBUS_SYM(free);
    SDL_DBUS_SYM(shutdown);

    #undef SDL_DBUS_SYM
    #undef SDL_DBUS_SYM2

    return 0;
}

static void
UnloadDBUSLibrary(void)
{
    if (dbus_handle != NULL) {
        SDL_UnloadObject(dbus_handle);
        dbus_handle = NULL;
    }
}

static int
LoadDBUSLibrary(void)
{
    int retval = 0;
    if (dbus_handle == NULL) {
        dbus_handle = SDL_LoadObject(dbus_library);
        if (dbus_handle == NULL) {
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
        } else {
            retval = LoadDBUSSyms();
            if (retval < 0) {
                UnloadDBUSLibrary();
            }
        }
    }

    return retval;
}

void
SDL_DBus_Init(void)
{
    if (!dbus.session_conn && LoadDBUSLibrary() != -1) {
        DBusError err;
        dbus.error_init(&err);
        dbus.session_conn = dbus.bus_get_private(DBUS_BUS_SESSION, &err);
        if (dbus.error_is_set(&err)) {
            dbus.error_free(&err);
            if (dbus.session_conn) {
                dbus.connection_unref(dbus.session_conn);
                dbus.session_conn = NULL;
            }
            return;  /* oh well */
        }
        dbus.connection_set_exit_on_disconnect(dbus.session_conn, 0);
    }
}

void
SDL_DBus_Quit(void)
{
    if (dbus.session_conn) {
        dbus.connection_close(dbus.session_conn);
        dbus.connection_unref(dbus.session_conn);
        dbus.shutdown();
        SDL_memset(&dbus, 0, sizeof(dbus));
    }
    UnloadDBUSLibrary();
}

SDL_DBusContext *
SDL_DBus_GetContext(void)
{
    if(!dbus_handle || !dbus.session_conn){
        SDL_DBus_Init();
    }
    
    if(dbus_handle && dbus.session_conn){
        return &dbus;
    } else {
        return NULL;
    }
}

void
SDL_DBus_ScreensaverTickle(void)
{
    DBusConnection *conn = dbus.session_conn;
    if (conn != NULL) {
        DBusMessage *msg = dbus.message_new_method_call("org.gnome.ScreenSaver",
                                                        "/org/gnome/ScreenSaver",
                                                        "org.gnome.ScreenSaver",
                                                        "SimulateUserActivity");
        if (msg != NULL) {
            if (dbus.connection_send(conn, msg, NULL)) {
                dbus.connection_flush(conn);
            }
            dbus.message_unref(msg);
        }
    }
}

SDL_bool
SDL_DBus_ScreensaverInhibit(SDL_bool inhibit)
{
    DBusConnection *conn = dbus.session_conn;

    if (conn == NULL)
        return SDL_FALSE;

    if (inhibit &&
        screensaver_cookie != 0)
        return SDL_TRUE;
    if (!inhibit &&
        screensaver_cookie == 0)
        return SDL_TRUE;

    if (inhibit) {
        const char *app = "My SDL application";
        const char *reason = "Playing a game";

        DBusMessage *msg = dbus.message_new_method_call("org.freedesktop.ScreenSaver",
                                                         "/org/freedesktop/ScreenSaver",
                                                         "org.freedesktop.ScreenSaver",
                                                         "Inhibit");
        if (msg != NULL) {
            dbus.message_append_args (msg,
                                      DBUS_TYPE_STRING, &app,
                                      DBUS_TYPE_STRING, &reason,
                                      DBUS_TYPE_INVALID);
        }

        if (msg != NULL) {
            DBusMessage *reply;

            reply = dbus.connection_send_with_reply_and_block(conn, msg, 300, NULL);
            if (reply) {
                if (!dbus.message_get_args(reply, NULL,
                                           DBUS_TYPE_UINT32, &screensaver_cookie,
                                           DBUS_TYPE_INVALID))
                    screensaver_cookie = 0;
                dbus.message_unref(reply);
            }

            dbus.message_unref(msg);
        }

        if (screensaver_cookie == 0) {
            return SDL_FALSE;
        }
        return SDL_TRUE;
    } else {
        DBusMessage *msg = dbus.message_new_method_call("org.freedesktop.ScreenSaver",
                                                        "/org/freedesktop/ScreenSaver",
                                                        "org.freedesktop.ScreenSaver",
                                                        "UnInhibit");
        dbus.message_append_args (msg,
                                  DBUS_TYPE_UINT32, &screensaver_cookie,
                                  DBUS_TYPE_INVALID);
        if (msg != NULL) {
            if (dbus.connection_send(conn, msg, NULL)) {
                dbus.connection_flush(conn);
            }
            dbus.message_unref(msg);
        }

        screensaver_cookie = 0;
        return SDL_TRUE;
    }
}
#endif
