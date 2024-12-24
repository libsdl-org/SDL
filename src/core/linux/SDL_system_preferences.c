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

#include "SDL_dbus.h"
#include "../../events/SDL_events_c.h"

#include <unistd.h>

#define PORTAL_DESTINATION "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INTERFACE "org.freedesktop.portal.Settings"
#define PORTAL_METHOD "ReadOne"

#define SIGNAL_INTERFACE "org.freedesktop.portal.Settings"
#define SIGNAL_NAME "SettingChanged"
// Signal namespace and key will vary

typedef struct SystemPrefData
{
    Uint32 contrast;
    Uint32 animations;
    Uint32 shapes;
    Uint32 hide_scrollbars;
    Uint32 cursor_size;
    Uint32 screen_reader;
    double text_scale;
} SystemPrefData;

static SystemPrefData system_pref_data;

typedef struct
{
    const char *destination;
    const char *path;
    const char *interface;
    const char *method;
    const char *namespace;
    const char *key;

    const char *signal_interface;
    const char *signal_name;
    const char *signal_args[3];
    const bool check_key;

    const SDL_EventType event_type;
    const SDL_SystemPreference event_pref;

    void *setting;
} Property;

// To be added:
// org.gtk.Settings /org/gtk/Settings/ org.gtk.Settings.EnableAnimations
// org.gnome.SettingsDaemon.XSettings /org/gtk/Settings/ org.gtk.Settings.EnableAnimations
static const Property props[] = {
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.freedesktop.appearance",
        "contrast",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.freedesktop.appearance", "contrast", NULL },
        true,
        SDL_EVENT_SYSTEM_PREFERENCE_CHANGED,
        SDL_SYSTEM_PREFERENCE_HIGH_CONTRAST,
        &system_pref_data.contrast,
    },
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.gnome.desktop.interface",
        "enable-animations",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.gnome.desktop.interface", "enable-animations", NULL },
        true,
        SDL_EVENT_SYSTEM_PREFERENCE_CHANGED,
        SDL_SYSTEM_PREFERENCE_REDUCED_MOTION,
        &system_pref_data.animations,
    },
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.gnome.desktop.a11y.interface",
        "show-status-shapes",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.gnome.desktop.a11y.interface", "show-status-shapes", NULL },
        true,
        SDL_EVENT_SYSTEM_PREFERENCE_CHANGED,
        SDL_SYSTEM_PREFERENCE_COLORBLIND,
        &system_pref_data.shapes,
    },
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.gnome.desktop.interface",
        "overlay-scrolling",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.gnome.desktop.interface", "overlay-scrolling", NULL },
        true,
        SDL_EVENT_SYSTEM_PREFERENCE_CHANGED,
        SDL_SYSTEM_PREFERENCE_PERSIST_SCROLLBARS,
        &system_pref_data.hide_scrollbars,
    },
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.gnome.desktop.interface",
        "cursor-size",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.gnome.desktop.interface", "cursor-size", NULL },
        true,
        SDL_EVENT_SYSTEM_CURSOR_SCALE_CHANGED,
        0,
        &system_pref_data.cursor_size,
    },
    {
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings",
        "ReadOne",
        "org.gnome.desktop.interface",
        "text-scaling-factor",
        "org.freedesktop.portal.Settings",
        "SettingChanged",
        { "org.gnome.desktop.interface", "text-scaling-factor", NULL },
        true,
        SDL_EVENT_SYSTEM_TEXT_SCALE_CHANGED,
        0,
        &system_pref_data.text_scale,
    },
    {
        "org.a11y.Bus",
        "/org/a11y/bus",
        "org.freedesktop.DBus.Properties",
        "Get",
        "org.a11y.Status",
        "ScreenReaderEnabled",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        { NULL, NULL, NULL },
        false,
        SDL_EVENT_SYSTEM_PREFERENCE_CHANGED,
        SDL_SYSTEM_PREFERENCE_SCREEN_READER,
        &system_pref_data.screen_reader,
    },
};

// FIXME: Type checking for setting Uint32 vs double
static bool DBus_ExtractPref(DBusMessageIter *iter, void *setting) {
    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    DBusMessageIter variant_iter;
    DBusMessageIter *data_iter;

    // Direct fetch returns UINT32 directly, event sends it wrapped in a variant
    if (dbus->message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
        dbus->message_iter_recurse(iter, &variant_iter);
        data_iter = &variant_iter;
    } else {
        data_iter = iter;
    }

    switch (dbus->message_iter_get_arg_type(data_iter))
    {
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_BOOLEAN:
        dbus->message_iter_get_basic(data_iter, (Uint32 *)setting);
        break;

    case DBUS_TYPE_DOUBLE:
        dbus->message_iter_get_basic(data_iter, (double *)setting);
        break;

    default:
        return false;
    };

    return true;
}

static DBusHandlerResult DBus_MessageFilter(DBusConnection *conn, DBusMessage *msg, void *data) {
    SDL_DBusContext *dbus = (SDL_DBusContext *)data;

    for (int i = 0; i < sizeof(props) / sizeof(*props); i++) {
        const Property *prop = &props[i];

        if (dbus->message_is_signal(msg, prop->signal_interface, prop->signal_name)) {
            DBusMessageIter signal_iter;
            const char *namespace, *key;

            dbus->message_iter_init(msg, &signal_iter);

            // Check if the parameters are what we expect
            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING)
                continue;

            dbus->message_iter_get_basic(&signal_iter, &namespace);
            if (SDL_strcmp(prop->namespace, namespace) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    continue;

                if (dbus->message_iter_get_arg_type(&signal_iter) == DBUS_TYPE_STRING) {
                    dbus->message_iter_get_basic(&signal_iter, &key);

                    if (SDL_strcmp(prop->key, key) != 0)
                        continue;

                    if (!dbus->message_iter_next(&signal_iter))
                        continue;

                    if (!DBus_ExtractPref(&signal_iter, prop->setting))
                        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

                    if (prop->event_type == SDL_EVENT_SYSTEM_PREFERENCE_CHANGED) {
                        SDL_SendSystemPreferenceChangedEvent(prop->event_pref);
                    } else {
                        SDL_SendAppEvent(prop->event_type);
                    }

                    return DBUS_HANDLER_RESULT_HANDLED;
                } else if (dbus->message_iter_get_arg_type(&signal_iter) == DBUS_TYPE_ARRAY) {
                    // This is for PropertiesChanged-based settings, like org.a11y.bus
                    DBusMessageIter array_iter;
                    dbus->message_iter_recurse(&signal_iter, &array_iter);

                    while (dbus->message_iter_get_arg_type(&array_iter) == DBUS_TYPE_DICT_ENTRY) {
                        DBusMessageIter dict_entry_iter;
                        DBusMessageIter value_iter;

                        dbus->message_iter_recurse(&array_iter, &dict_entry_iter);

                        // Extract the key and value from the dict entry
                        dbus->message_iter_get_basic(&dict_entry_iter, &key);

                        if (SDL_strcmp(key, prop->key) != 0)
                            goto next_and_continue;

                        dbus->message_iter_next(&dict_entry_iter);
                        dbus->message_iter_recurse(&dict_entry_iter, &value_iter);

                        if (!DBus_ExtractPref(&value_iter, prop->setting))
                            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

                        if (prop->event_type == SDL_EVENT_SYSTEM_PREFERENCE_CHANGED) {
                            SDL_SendSystemPreferenceChangedEvent(prop->event_pref);
                        } else {
                            SDL_SendAppEvent(prop->event_type);
                        }

                        return DBUS_HANDLER_RESULT_HANDLED;

next_and_continue:
                        dbus->message_iter_next(&array_iter);
                    }
                } else {
                    continue;
                }
            }
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


#if 0
    if (dbus->message_is_signal(msg, SIGNAL_INTERFACE, SIGNAL_NAME)) {
        DBusMessageIter signal_iter;
        const char *namespace, *key;

        dbus->message_iter_init(msg, &signal_iter);
        // Check if the parameters are what we expect
        if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING)
            goto not_our_signal;

        dbus->message_iter_get_basic(&signal_iter, &namespace);

        // FIXME: For every setting outside org.freedesktop.appearance, DBus
        // sends two events rather than one.
        if (SDL_strcmp("org.freedesktop.appearance", namespace) == 0) {
            if (!dbus->message_iter_next(&signal_iter))
                goto not_our_signal;

            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING)
                goto not_our_signal;
            dbus->message_iter_get_basic(&signal_iter, &key);

            if (SDL_strcmp("contrast", key) != 0)
                goto not_our_signal;

            if (!dbus->message_iter_next(&signal_iter))
                goto not_our_signal;

            if (!DBus_ExtractPref(&signal_iter, &system_pref_data.contrast))
                goto not_our_signal;

            SDL_SendSystemPreferenceChangedEvent(SDL_SYSTEM_PREFERENCE_HIGH_CONTRAST);

            return DBUS_HANDLER_RESULT_HANDLED;
        } else if (SDL_strcmp("org.gnome.desktop.interface", namespace) == 0) {
            if (!dbus->message_iter_next(&signal_iter))
                goto not_our_signal;

            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING)
                goto not_our_signal;

            dbus->message_iter_get_basic(&signal_iter, &key);

            if (SDL_strcmp("enable-animations", key) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    goto not_our_signal;

                if (!DBus_ExtractPref(&signal_iter, &system_pref_data.animations))
                    goto not_our_signal;

                SDL_SendSystemPreferenceChangedEvent(SDL_SYSTEM_PREFERENCE_REDUCED_MOTION);

                return DBUS_HANDLER_RESULT_HANDLED;
            } else if (SDL_strcmp("overlay-scrolling", key) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    goto not_our_signal;

                if (!DBus_ExtractPref(&signal_iter, &system_pref_data.hide_scrollbars))
                    goto not_our_signal;

                SDL_SendSystemPreferenceChangedEvent(SDL_SYSTEM_PREFERENCE_PERSIST_SCROLLBARS);

                return DBUS_HANDLER_RESULT_HANDLED;
            } else if (SDL_strcmp("cursor-size", key) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    goto not_our_signal;

                if (!DBus_ExtractPref(&signal_iter, &system_pref_data.cursor_size))
                    goto not_our_signal;

                SDL_SendAppEvent(SDL_EVENT_SYSTEM_CURSOR_SCALE_CHANGED);

                return DBUS_HANDLER_RESULT_HANDLED;
            } else if (SDL_strcmp("text-scaling-factor", key) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    goto not_our_signal;

                if (!DBus_ExtractPref(&signal_iter, &system_pref_data.text_scale))
                    goto not_our_signal;

                SDL_SendAppEvent(SDL_EVENT_SYSTEM_TEXT_SCALE_CHANGED);

                return DBUS_HANDLER_RESULT_HANDLED;
            } else {
                goto not_our_signal;
            }
        }  else if (SDL_strcmp("org.gnome.desktop.a11y.interface", namespace) == 0) {
            if (!dbus->message_iter_next(&signal_iter))
                goto not_our_signal;

            if (dbus->message_iter_get_arg_type(&signal_iter) != DBUS_TYPE_STRING)
                goto not_our_signal;

            dbus->message_iter_get_basic(&signal_iter, &key);

            if (SDL_strcmp("show-status-shapes", key) == 0) {
                if (!dbus->message_iter_next(&signal_iter))
                    goto not_our_signal;

                if (!DBus_ExtractPref(&signal_iter, &system_pref_data.shapes))
                    goto not_our_signal;

                SDL_SendSystemPreferenceChangedEvent(SDL_SYSTEM_PREFERENCE_COLORBLIND);

                return DBUS_HANDLER_RESULT_HANDLED;
            } else {
                goto not_our_signal;
            }
        } else {
            goto not_our_signal;
        }
    }
not_our_signal:
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
#endif
}

bool Unix_SystemPref_Init(void)
{
    static bool is_init = false;

    if (is_init) {
        return true;
    }

    SDL_DBusContext *dbus = SDL_DBus_GetContext();
    DBusMessage *msg;

    system_pref_data.contrast = false;
    system_pref_data.animations = true;
    system_pref_data.shapes = false;
    system_pref_data.hide_scrollbars = true;
    system_pref_data.screen_reader = false;
    system_pref_data.cursor_size = 24;
    system_pref_data.text_scale = 1.0f;

    if (!dbus) {
        return false;
    }

    for (int i = 0; i < sizeof(props) / sizeof(*props); i++) {
        const Property *prop = &props[i];

        msg = dbus->message_new_method_call(prop->destination, prop->path, prop->interface, prop->method);
        if (msg) {
            if (dbus->message_append_args(msg, DBUS_TYPE_STRING, &prop->namespace, DBUS_TYPE_STRING, &prop->key, DBUS_TYPE_INVALID)) {
                DBusMessage *reply = dbus->connection_send_with_reply_and_block(dbus->session_conn, msg, 300, NULL);
                if (reply) {
                    DBusMessageIter reply_iter, variant_outer_iter;

                    dbus->message_iter_init(reply, &reply_iter);
                    // The response has signature <<u>>
                    if (dbus->message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_VARIANT)
                        goto incorrect_type;

                    dbus->message_iter_recurse(&reply_iter, &variant_outer_iter);
                    if (!DBus_ExtractPref(&variant_outer_iter, prop->setting))
                        goto incorrect_type;

incorrect_type:
                    dbus->message_unref(reply);
                }
            }
            dbus->message_unref(msg);
        }

        char buffer[2048];
        int written = 0;
        written += SDL_snprintf(buffer, sizeof(buffer), "type='signal', interface='%s',"
                                "member='%s'", prop->signal_interface,
                                prop->signal_name);

        for (int j = 0; j < sizeof(prop->signal_args) / sizeof(*prop->signal_args) && prop->signal_args[j]; j++) {
            if (written >= sizeof(buffer)) {
                SDL_SetError("Binding system prefereces DBus key: buffer too small, this is a bug");
                return false;
            }

            written += SDL_snprintf(buffer + written, sizeof(buffer) - written, ", arg%d='%s'", j, prop->signal_args[j]);
        }

        if (written >= sizeof(buffer)) {
            SDL_SetError("Binding system prefereces DBus key: buffer too small, this is a bug");
            return false;
        }

        dbus->bus_add_match(dbus->session_conn, buffer, NULL);
    }

    dbus->connection_add_filter(dbus->session_conn,
                                &DBus_MessageFilter, dbus, NULL);
    dbus->connection_flush(dbus->session_conn);

    is_init = true;

    return true;
}

bool Unix_GetSystemPreference(SDL_SystemPreference preference)
{
    switch (preference)
    {
    case SDL_SYSTEM_PREFERENCE_REDUCED_MOTION:
        return !system_pref_data.animations;

    case SDL_SYSTEM_PREFERENCE_HIGH_CONTRAST:
        return !!system_pref_data.contrast;

    case SDL_SYSTEM_PREFERENCE_COLORBLIND:
        return !!system_pref_data.shapes;

    case SDL_SYSTEM_PREFERENCE_PERSIST_SCROLLBARS:
        return !system_pref_data.hide_scrollbars;

    case SDL_SYSTEM_PREFERENCE_SCREEN_READER:
        return !!system_pref_data.screen_reader;

    default:
        return SDL_Unsupported();
    }
}

bool Unix_GetSystemAccentColor(SDL_Color *color)
{
    return SDL_Unsupported();
}

float Unix_GetSystemTextScale(void)
{
    return (float) system_pref_data.text_scale;
}

float Unix_GetSystemCursorScale(void)
{
    return (float) system_pref_data.cursor_size / 24.0f;
}
