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
#include "./SDL_dialog.h"

#include "../../core/linux/SDL_dbus.h"

#ifdef SDL_USE_LIBDBUS

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORTAL_DESTINATION "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INTERFACE "org.freedesktop.portal.FileChooser"

#define SIGNAL_SENDER "org.freedesktop.portal.Desktop"
#define SIGNAL_INTERFACE "org.freedesktop.portal.Request"
#define SIGNAL_NAME "Response"
#define SIGNAL_FILTER "type='signal', sender='"SIGNAL_SENDER"', interface='"SIGNAL_INTERFACE"', member='"SIGNAL_NAME"', path='"

#define HANDLE_LEN 10

typedef struct {
    SDL_DialogFileCallback callback;
    void *userdata;
    const char *path;
} SignalCallback;

static void DBus_AppendStringOption(SDL_DBusContext *dbus, DBusMessageIter *options, const char *key, const char *value)
{
    DBusMessageIter options_pair, options_value;

    dbus->message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &options_pair);
    dbus->message_iter_append_basic(&options_pair, DBUS_TYPE_STRING, &key);
    dbus->message_iter_open_container(&options_pair, DBUS_TYPE_VARIANT, "s", &options_value);
    dbus->message_iter_append_basic(&options_value, DBUS_TYPE_STRING, &value);
    dbus->message_iter_close_container(&options_pair, &options_value);
    dbus->message_iter_close_container(options, &options_pair);
}

static void DBus_AppendBoolOption(SDL_DBusContext *dbus, DBusMessageIter *options, const char *key, int value)
{
    DBusMessageIter options_pair, options_value;

    dbus->message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &options_pair);
    dbus->message_iter_append_basic(&options_pair, DBUS_TYPE_STRING, &key);
    dbus->message_iter_open_container(&options_pair, DBUS_TYPE_VARIANT, "b", &options_value);
    dbus->message_iter_append_basic(&options_value, DBUS_TYPE_BOOLEAN, &value);
    dbus->message_iter_close_container(&options_pair, &options_value);
    dbus->message_iter_close_container(options, &options_pair);
}

static void DBus_AppendFilter(SDL_DBusContext *dbus, DBusMessageIter *parent, const SDL_DialogFileFilter *filter)
{
    DBusMessageIter filter_entry, filter_array, filter_array_entry;
    char *state = NULL, *patterns, *pattern, *glob_pattern;
    int zero = 0;

    dbus->message_iter_open_container(parent, DBUS_TYPE_STRUCT, NULL, &filter_entry);
    dbus->message_iter_append_basic(&filter_entry, DBUS_TYPE_STRING, &filter->name);
    dbus->message_iter_open_container(&filter_entry, DBUS_TYPE_ARRAY, "(us)", &filter_array);

    patterns = SDL_strdup(filter->pattern);
    if (!patterns) {
        SDL_OutOfMemory();
        goto cleanup;
    }

    pattern = SDL_strtok_r(patterns, ";", &state);
    while (pattern) {
        size_t max_len = SDL_strlen(pattern) + 3;

        dbus->message_iter_open_container(&filter_array, DBUS_TYPE_STRUCT, NULL, &filter_array_entry);
        dbus->message_iter_append_basic(&filter_array_entry, DBUS_TYPE_UINT32, &zero);

        glob_pattern = SDL_calloc(sizeof(char), max_len);
        if (!glob_pattern) {
            SDL_OutOfMemory();
            goto cleanup;
        }
        glob_pattern[0] = '*';
        /* Special case: The '*' filter doesn't need to be rewritten */
        if (pattern[0] != '*' || pattern[1]) {
            glob_pattern[1] = '.';
            SDL_strlcat(glob_pattern + 2, pattern, max_len);
        }
        dbus->message_iter_append_basic(&filter_array_entry, DBUS_TYPE_STRING, &glob_pattern);
        SDL_free(glob_pattern);

        dbus->message_iter_close_container(&filter_array, &filter_array_entry);
        pattern = SDL_strtok_r(NULL, ";", &state);
    }

cleanup:
    SDL_free(patterns);

    dbus->message_iter_close_container(&filter_entry, &filter_array);
    dbus->message_iter_close_container(parent, &filter_entry);
}

static void DBus_AppendFilters(SDL_DBusContext *dbus, DBusMessageIter *options, const SDL_DialogFileFilter *filters)
{
    DBusMessageIter options_pair, options_value, options_value_array;
    static const char *filters_name = "filters";

    dbus->message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &options_pair);
    dbus->message_iter_append_basic(&options_pair, DBUS_TYPE_STRING, &filters_name);
    dbus->message_iter_open_container(&options_pair, DBUS_TYPE_VARIANT, "a(sa(us))", &options_value);
    dbus->message_iter_open_container(&options_value, DBUS_TYPE_ARRAY, "(sa(us))", &options_value_array);
    for (const SDL_DialogFileFilter *filter = filters; filter && filter->name && filter->pattern; ++filter) {
        DBus_AppendFilter(dbus, &options_value_array, filter);
    }
    dbus->message_iter_close_container(&options_value, &options_value_array);
    dbus->message_iter_close_container(&options_pair, &options_value);
    dbus->message_iter_close_container(options, &options_pair);
}

static void DBus_AppendByteArray(SDL_DBusContext *dbus, DBusMessageIter *options, const char *key, const char *value)
{
    DBusMessageIter options_pair, options_value, options_array;

    dbus->message_iter_open_container(options, DBUS_TYPE_DICT_ENTRY, NULL, &options_pair);
    dbus->message_iter_append_basic(&options_pair, DBUS_TYPE_STRING, &key);
    dbus->message_iter_open_container(&options_pair, DBUS_TYPE_VARIANT, "ay", &options_value);
    dbus->message_iter_open_container(&options_value, DBUS_TYPE_ARRAY, "y", &options_array);
    do {
        dbus->message_iter_append_basic(&options_array, DBUS_TYPE_BYTE, value);
    } while (*value++);
    dbus->message_iter_close_container(&options_value, &options_array);
    dbus->message_iter_close_container(&options_pair, &options_value);
    dbus->message_iter_close_container(options, &options_pair);
}

static DBusHandlerResult DBus_MessageFilter(DBusConnection *conn, DBusMessage *msg, void *data) {
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    SignalCallback *signal_data = (SignalCallback *)data;

    if (dbus->message_is_signal(msg, SIGNAL_INTERFACE, SIGNAL_NAME)
            && dbus->message_has_path(msg, signal_data->path)) {
        DBusMessageIter signal_iter, result_array, array_entry, value_entry, uri_entry;
        uint32_t result;
        size_t length = 2, current = 0;
        const char **path;

        dbus->message_iter_init(msg, &signal_iter);
        /* Check if the parameters are what we expect */
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_UINT32)
            goto not_our_signal;
        dbus->message_iter_get_basic(&signal_iter, &result);

        if (result == 1) {
            /* cancelled */
            const char *result_data[] = { NULL };
            signal_data->callback(signal_data->userdata, result_data, -1); /* TODO: Set this to the last selected filter */
            goto handled;
        }
        else if (result) {
            /* some error occurred */
            signal_data->callback(signal_data->userdata, NULL, -1);
            goto handled;
        }

        if (!dbus->message_iter_next(&signal_iter))
            goto not_our_signal;

        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_ARRAY)
            goto not_our_signal;
        dbus->message_iter_recurse(&signal_iter, &result_array);

        while (dbus->message_iter_get_arg_type(&result_array) == DBUS_TYPE_DICT_ENTRY)
        {
            const char *method;

            dbus->message_iter_recurse(&result_array, &array_entry);
            if (dbus->message_iter_get_arg_type(&array_entry) != DBUS_TYPE_STRING)
                goto not_our_signal;

            dbus->message_iter_get_basic(&array_entry, &method);
            if (!SDL_strcmp(method, "uris")) {
                /* we only care about the selected file paths */
                break;
            }

            if (!dbus->message_iter_next(&result_array))
                goto not_our_signal;
        }

        if (!dbus->message_iter_next(&array_entry))
            goto not_our_signal;

        if (dbus->message_iter_get_arg_type(&array_entry) != DBUS_TYPE_VARIANT)
            goto not_our_signal;
        dbus->message_iter_recurse(&array_entry, &value_entry);

        if (dbus->message_iter_get_arg_type(&value_entry) != DBUS_TYPE_ARRAY)
            goto not_our_signal;
        dbus->message_iter_recurse(&value_entry, &uri_entry);

        path = SDL_malloc(sizeof(const char *) * length);
        if (!path) {
            SDL_OutOfMemory();
            signal_data->callback(signal_data->userdata, NULL, -1);
            goto cleanup;
        }

        while (dbus->message_iter_get_arg_type(&uri_entry) == DBUS_TYPE_STRING)
        {
            if (current >= length - 1) {
                ++length;
                path = SDL_realloc(path, sizeof(const char *) * length);
                if (!path) {
                    SDL_OutOfMemory();
                    signal_data->callback(signal_data->userdata, NULL, -1);
                    goto cleanup;
                }
            }
            dbus->message_iter_get_basic(&uri_entry, path + current);

            dbus->message_iter_next(&uri_entry);
            ++current;
        }
        path[length - 1] = NULL;
        signal_data->callback(signal_data->userdata, path, -1); /* TODO: Fetch the index of the filter that was used */
cleanup:
        dbus->connection_remove_filter(conn, &DBus_MessageFilter, signal_data);

        SDL_free(path);
        SDL_free((void *)signal_data->path);
        SDL_free(signal_data);
handled:
        return DBUS_HANDLER_RESULT_HANDLED;
    }
not_our_signal:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void DBus_OpenDialog(const char *method, const char *method_title, SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location, SDL_bool allow_many, int open_folders)
{
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    DBusMessage *msg;
    DBusMessageIter params, options;
    const char *signal_id;
    char *handle_str, *filter;
    int filter_len;
    static const char *parent_window = ""; /* TODO: Consider using X11's PID or the Wayland handle */
    static uint32_t handle_id = 0;

    if (dbus == NULL) {
        SDL_SetError("%s", "Failed to connect to DBus!");
        return;
    }

    msg = dbus->message_new_method_call(PORTAL_DESTINATION, PORTAL_PATH, PORTAL_INTERFACE, method);
    if (msg == NULL) {
        SDL_SetError("%s", "Failed to send message to portal!");
        return;
    }

    dbus->message_iter_init_append(msg, &params);
    dbus->message_iter_append_basic(&params, DBUS_TYPE_STRING, &parent_window);
    dbus->message_iter_append_basic(&params, DBUS_TYPE_STRING, &method_title);
    dbus->message_iter_open_container(&params, DBUS_TYPE_ARRAY, "{sv}", &options);

    handle_str = SDL_malloc(sizeof(char) * (HANDLE_LEN + 1));
    if (!handle_str) {
        SDL_OutOfMemory();
        return;
    }
    SDL_snprintf(handle_str, HANDLE_LEN, "%u", ++handle_id);
    DBus_AppendStringOption(dbus, &options, "handle_token", handle_str);
    SDL_free(handle_str);

    DBus_AppendBoolOption(dbus, &options, "modal", !!window);
    if (allow_many == SDL_TRUE) {
        DBus_AppendBoolOption(dbus, &options, "multiple", 1);
    }
    if (open_folders) {
        DBus_AppendBoolOption(dbus, &options, "directory", 1);
    }
    if (filters) {
        DBus_AppendFilters(dbus, &options, filters);
    }
    if (default_location) {
        DBus_AppendByteArray(dbus, &options, "current_folder", default_location);
    }
    dbus->message_iter_close_container(&params, &options);

    DBusMessage *reply = dbus->connection_send_with_reply_and_block(dbus->session_conn, msg, DBUS_TIMEOUT_INFINITE, NULL);
    if (reply) {
        DBusMessageIter reply_iter;
        dbus->message_iter_init(reply, &reply_iter);

        if (dbus->message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_OBJECT_PATH)
        {
            SDL_SetError("%s", "Invalid response received by DBus!");
            goto incorrect_type;
        }
        dbus->message_iter_get_basic(&reply_iter, &signal_id);
    }

    dbus->message_unref(msg);

    filter_len = SDL_strlen(SIGNAL_FILTER) + SDL_strlen(signal_id) + 2;
    filter = SDL_malloc(sizeof(char) * filter_len);
    if (!filter) {
        SDL_OutOfMemory();
        goto incorrect_type;
    }

    SDL_snprintf(filter, filter_len, SIGNAL_FILTER"%s'", signal_id);
    dbus->bus_add_match(dbus->session_conn, filter, NULL);
    SDL_free(filter);

    SignalCallback *data = SDL_malloc(sizeof(SignalCallback));
    if (!data) {
        SDL_OutOfMemory();
        goto incorrect_type;
    }
    data->callback = callback;
    data->userdata = userdata;
    data->path = SDL_strdup(signal_id);
    if (!data->path) {
        SDL_OutOfMemory();
        SDL_free(data);
        goto incorrect_type;
    }

    /* TODO: This should be registered before opening the portal, or the filter will not catch
             the message if it is sent before we register the filter.
    */
    dbus->connection_add_filter(dbus->session_conn,
                                &DBus_MessageFilter, data, NULL);
    dbus->connection_flush(dbus->session_conn);

incorrect_type:
    dbus->message_unref(reply);
}

void SDL_Portal_ShowOpenFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location, SDL_bool allow_many)
{
    DBus_OpenDialog("OpenFile", "Open File", callback, userdata, window, filters, default_location, allow_many, 0);
}

void SDL_Portal_ShowSaveFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location)
{
    DBus_OpenDialog("SaveFile", "Save File", callback, userdata, window, filters, default_location, 0, 0);
}

void SDL_Portal_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const char* default_location, SDL_bool allow_many)
{
    DBus_OpenDialog("OpenFile", "Open Folder", callback, userdata, window, NULL, default_location, allow_many, 1);
}

int SDL_Portal_detect(void)
{
    /* TODO */
    return 0;
}

#else

/* Dummy implementation to avoid compilation problems */

void SDL_Portal_ShowOpenFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location, SDL_bool allow_many)
{
    SDL_Unsupported();
    callback(userdata, NULL, -1);
}

void SDL_Portal_ShowSaveFileDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const SDL_DialogFileFilter *filters, const char* default_location)
{
    SDL_Unsupported();
    callback(userdata, NULL, -1);
}

void SDL_Portal_ShowOpenFolderDialog(SDL_DialogFileCallback callback, void* userdata, SDL_Window* window, const char* default_location, SDL_bool allow_many)
{
    SDL_Unsupported();
    callback(userdata, NULL, -1);
}

int SDL_Portal_detect(void)
{
    return 0;
}

#endif /* SDL_USE_LIBDBUS */
