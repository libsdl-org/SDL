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
#include "SDL_progressbar.h"
#include "SDL_internal.h"

#include "SDL_dbus.h"

#ifdef SDL_USE_LIBDBUS

#include <unistd.h>

#include "../unix/SDL_appid.h"

#define UnityLauncherAPI_DBUS_INTERFACE "com.canonical.Unity.LauncherEntry"
#define UnityLauncherAPI_DBUS_SIGNAL    "Update"

static char *GetDBUSObjectPath()
{
    char *app_id = SDL_strdup(SDL_GetAppID());

    if (!app_id) {
        return NULL;
    }

    // Sanitize exe_name to make it a legal D-Bus path element
    for (char *p = app_id; *p; ++p) {
        if (!SDL_isalnum(*p)) {
            *p = '_';
        }
    }

    // Ensure it starts with a letter or underscore
    if (!SDL_isalpha(app_id[0]) && app_id[0] != '_') {
        SDL_memmove(app_id + 1, app_id, SDL_strlen(app_id) + 1);
        app_id[0] = '_';
    }

    // Create full path
    char path[1024];
    SDL_snprintf(path, sizeof(path), "/org/libsdl/%s_%d", app_id, getpid());

    SDL_free(app_id);

    return SDL_strdup(path);
}

static char *GetAppDesktopPath()
{
    const char *desktop_suffix = ".desktop";
    const char *app_id = SDL_GetAppID();
    const size_t desktop_path_total_length = SDL_strlen(app_id) + SDL_strlen(desktop_suffix) + 1;
    char *desktop_path = (char *)SDL_malloc(desktop_path_total_length);
    if (!desktop_path) {
        return NULL;
    }
    *desktop_path = '\0';
    SDL_strlcat(desktop_path, app_id, desktop_path_total_length);
    SDL_strlcat(desktop_path, desktop_suffix, desktop_path_total_length);

    return desktop_path;
}

static int ShouldShowProgress(SDL_ProgressState progressState)
{
    if (progressState == SDL_PROGRESS_STATE_INVALID ||
        progressState == SDL_PROGRESS_STATE_NONE) {
        return 0;
    }

    // Unity LauncherAPI only supports "normal" display of progress
    return 1;
}

bool DBUS_ApplyWindowProgress(SDL_VideoDevice *_this, SDL_Window *window)
{
    // Signal signature:
    // signal com.canonical.Unity.LauncherEntry.Update (in s app_uri, in a{sv} properties)

    SDL_DBusContext *dbus = SDL_DBus_GetContext();

    if (!dbus || !dbus->session_conn) {
        return false;
    }

    char *objectPath = GetDBUSObjectPath();
    if (!objectPath) {
        return false;
    }

    DBusMessage *msg = dbus->message_new_signal(objectPath, UnityLauncherAPI_DBUS_INTERFACE, UnityLauncherAPI_DBUS_SIGNAL);
    if (!msg) {
        SDL_free(objectPath);
        return false;
    }

    char *desktop_path = GetAppDesktopPath();
    if (!desktop_path) {
        dbus->message_unref(msg);
        SDL_free(objectPath);
        return false;
    }

    const char *progress_visible_str = "progress-visible";
    const char *progress_str = "progress";
    int dbus_type_boolean_str = DBUS_TYPE_BOOLEAN;
    int dbus_type_double_str = DBUS_TYPE_DOUBLE;

    const int progress_visible = ShouldShowProgress(window->progress_state);
    double progress = (double)window->progress_value;

    DBusMessageIter args, props;
    dbus->message_iter_init_append(msg, &args);
    dbus->message_iter_append_basic(&args, DBUS_TYPE_STRING, &desktop_path);   // Setup app_uri paramter
    dbus->message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &props); // Setup properties parameter
    DBusMessageIter key_it, value_it;
    // Set progress visible property
    dbus->message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, NULL, &key_it);
    dbus->message_iter_append_basic(&key_it, DBUS_TYPE_STRING, &progress_visible_str); // Append progress-visible key data
    dbus->message_iter_open_container(&key_it, DBUS_TYPE_VARIANT, (const char *)&dbus_type_boolean_str, &value_it);
    dbus->message_iter_append_basic(&value_it, DBUS_TYPE_BOOLEAN, &progress_visible); // Append progress-visible value data
    dbus->message_iter_close_container(&key_it, &value_it);
    dbus->message_iter_close_container(&props, &key_it);
    // Set progress value property
    dbus->message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, NULL, &key_it);
    dbus->message_iter_append_basic(&key_it, DBUS_TYPE_STRING, &progress_str); // Append progress key data
    dbus->message_iter_open_container(&key_it, DBUS_TYPE_VARIANT, (const char *)&dbus_type_double_str, &value_it);
    dbus->message_iter_append_basic(&value_it, DBUS_TYPE_DOUBLE, &progress); // Append progress value data
    dbus->message_iter_close_container(&key_it, &value_it);
    dbus->message_iter_close_container(&props, &key_it);
    dbus->message_iter_close_container(&args, &props);

    dbus->connection_send(dbus->session_conn, msg, NULL);

    SDL_free(desktop_path);
    dbus->message_unref(msg);
    SDL_free(objectPath);

    return true;
}

#endif // SDL_USE_LIBDBUS
