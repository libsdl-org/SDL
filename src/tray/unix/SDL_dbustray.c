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
#include "../../core/linux/SDL_dbus.h"

#ifdef SDL_USE_LIBDBUS

#include <unistd.h>
#include "../SDL_tray_utils.h"
#include "SDL_unixtray.h"
#include "../../video/SDL_surface_c.h"

#define SNI_INTERFACE         "org.kde.StatusNotifierItem"
#define SNI_WATCHER_SERVICE   "org.kde.StatusNotifierWatcher"
#define SNI_WATCHER_PATH      "/StatusNotifierWatcher"
#define SNI_WATCHER_INTERFACE "org.kde.StatusNotifierWatcher"
#define SNI_OBJECT_PATH       "/StatusNotifierItem"

/* TODO: Destroy all menus and entires on destroy tray */

typedef struct ItemToFree
{
    void *item;
    void (*func)(void *);
} ItemToFree;

typedef struct SDL_TrayDriverDBus
{
    SDL_TrayDriver _parent;

    SDL_DBusContext *dbus;
} SDL_TrayDriverDBus;

typedef struct SDL_TrayDBus
{
    SDL_Tray _parent;

    DBusConnection *connection;
    char *object_name;

    char *tooltip;
    SDL_Surface *surface;
    
    SDL_ListNode *free_list;
    
    bool break_update;
} SDL_TrayDBus;

typedef struct SDL_TrayMenuDBus
{
    SDL_TrayMenu _parent;

    SDL_ListNode *menu;
    const char *menu_path;
} SDL_TrayMenuDBus;

typedef struct SDL_TrayEntryDBus
{
    SDL_TrayEntry _parent;

    SDL_DBusMenuItem item;
    SDL_TrayMenuDBus *sub_menu;
} SDL_TrayEntryDBus;

static DBusHandlerResult HandleGetAllProps(SDL_Tray *tray, SDL_TrayDBus *tray_dbus, SDL_TrayDriverDBus *driver, DBusMessage *msg)
{
    SDL_TrayMenuDBus *menu_dbus;
    DBusMessageIter iter, dict_iter, entry_iter, variant_iter;
    DBusMessageIter struct_iter, array_iter;
    DBusMessage *reply;
    const char *interface;
    const char *key;
    const char *value;
    const char *empty;
    dbus_uint32_t uint32_val;
    dbus_bool_t bool_value;

    empty = "";
    driver->dbus->message_iter_init(msg, &iter);
    driver->dbus->message_iter_get_basic(&iter, &interface);
    reply = driver->dbus->message_new_method_return(msg);
    driver->dbus->message_iter_init_append(reply, &iter);
    driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

    key = "Category";
    value = "ApplicationStatus";
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "Id";
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &tray_dbus->object_name);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "Title";
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &empty);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "Status";
    value = "Active";
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "IconName";
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &empty);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "WindowId";
    uint32_val = 0;
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "u", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32, &uint32_val);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    key = "ItemIsMenu";
    menu_dbus = (SDL_TrayMenuDBus *)tray->menu;
    if (menu_dbus) {
        bool_value = TRUE;
    } else {
        bool_value = FALSE;
    }
    driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
    driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_value);
    driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
    driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);

    if (menu_dbus) {
        if (menu_dbus->menu_path) {
            key = "Menu";
            value = menu_dbus->menu_path;
            driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
            driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
            driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "o", &variant_iter);
            driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_OBJECT_PATH, &value);
            driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
            driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);
        }
    }

    if (tray_dbus->surface) {
        DBusMessageIter pixmap_array_iter, pixmap_struct_iter, pixmap_byte_array_iter;

        key = "IconPixmap";
        driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "a(iiay)", &variant_iter);
        driver->dbus->message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "(iiay)", &pixmap_array_iter);
        driver->dbus->message_iter_open_container(&pixmap_array_iter, DBUS_TYPE_STRUCT, NULL, &pixmap_struct_iter);
        driver->dbus->message_iter_append_basic(&pixmap_struct_iter, DBUS_TYPE_INT32, &tray_dbus->surface->w);
        driver->dbus->message_iter_append_basic(&pixmap_struct_iter, DBUS_TYPE_INT32, &tray_dbus->surface->h);
        driver->dbus->message_iter_open_container(&pixmap_struct_iter, DBUS_TYPE_ARRAY, "y", &pixmap_byte_array_iter);
        driver->dbus->message_iter_append_fixed_array(&pixmap_byte_array_iter, DBUS_TYPE_BYTE, &tray_dbus->surface->pixels, tray_dbus->surface->pitch * tray_dbus->surface->h);
        driver->dbus->message_iter_close_container(&pixmap_struct_iter, &pixmap_byte_array_iter);
        driver->dbus->message_iter_close_container(&pixmap_array_iter, &pixmap_struct_iter);
        driver->dbus->message_iter_close_container(&variant_iter, &pixmap_array_iter);
        driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
        driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);
    }

    if (tray_dbus->tooltip) {
        key = "ToolTip";
        driver->dbus->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        driver->dbus->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        driver->dbus->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant_iter);
        driver->dbus->message_iter_open_container(&variant_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "(iiay)", &array_iter);
        driver->dbus->message_iter_close_container(&struct_iter, &array_iter);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &tray_dbus->tooltip);
        driver->dbus->message_iter_close_container(&variant_iter, &struct_iter);
        driver->dbus->message_iter_close_container(&entry_iter, &variant_iter);
        driver->dbus->message_iter_close_container(&dict_iter, &entry_iter);
        driver->dbus->message_iter_close_container(&iter, &dict_iter);
    }

    driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
    driver->dbus->message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult HandleGetProp(SDL_Tray *tray, SDL_TrayDBus *tray_dbus, SDL_TrayDriverDBus *driver, DBusMessage *msg)
{
    SDL_TrayMenuDBus *menu_dbus;
    DBusMessageIter iter, variant_iter;
    DBusMessage *reply;
    const char *interface, *property;
    const char *value;
    const char *empty;
    dbus_bool_t bool_value;

    empty = "";
    driver->dbus->message_iter_init(msg, &iter);
    driver->dbus->message_iter_get_basic(&iter, &interface);
    driver->dbus->message_iter_next(&iter);
    driver->dbus->message_iter_get_basic(&iter, &property);

    reply = driver->dbus->message_new_method_return(msg);
    driver->dbus->message_iter_init_append(reply, &iter);

    menu_dbus = (SDL_TrayMenuDBus *)tray->menu;
    if (!SDL_strcmp(property, "Category")) {
        value = "ApplicationStatus";
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "Id")) {
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &tray_dbus->object_name);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "Title")) {
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "Status")) {
        value = "Active";
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "IconName")) {
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "ItemIsMenu")) {
        if (menu_dbus) {
            bool_value = TRUE;
        } else {
            bool_value = FALSE;
        }
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_value);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "Menu") && menu_dbus) {
        if (menu_dbus->menu_path) {
            value = menu_dbus->menu_path;
            driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "o", &variant_iter);
            driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_OBJECT_PATH, &value);
            driver->dbus->message_iter_close_container(&iter, &variant_iter);
        }
    } else if (!SDL_strcmp(property, "IconPixmap") && tray_dbus->surface) {
        DBusMessageIter array_iter, struct_iter;
        DBusMessageIter byte_array_iter;

        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a(iiay)", &variant_iter);
        driver->dbus->message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "(iiay)", &array_iter);
        driver->dbus->message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &tray_dbus->surface->w);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &tray_dbus->surface->h);
        driver->dbus->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "y", &byte_array_iter);
        driver->dbus->message_iter_append_fixed_array(&byte_array_iter, DBUS_TYPE_BYTE, &tray_dbus->surface->pixels, tray_dbus->surface->pitch * tray_dbus->surface->h);
        driver->dbus->message_iter_close_container(&struct_iter, &byte_array_iter);
        driver->dbus->message_iter_close_container(&array_iter, &struct_iter);
        driver->dbus->message_iter_close_container(&variant_iter, &array_iter);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "ToolTip") && tray_dbus->tooltip) {
        DBusMessageIter struct_iter, array_iter;

        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &variant_iter);
        driver->dbus->message_iter_open_container(&variant_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "(iiay)", &array_iter);
        driver->dbus->message_iter_close_container(&struct_iter, &array_iter);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &empty);
        driver->dbus->message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &tray_dbus->tooltip);
        driver->dbus->message_iter_close_container(&variant_iter, &struct_iter);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "WindowId")) {
        dbus_uint32_t uint32_val;
        
        uint32_val = 0;
        driver->dbus->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &variant_iter);
        driver->dbus->message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32, &uint32_val);
        driver->dbus->message_iter_close_container(&iter, &variant_iter);
    } else {
        driver->dbus->message_unref(reply);
        reply = driver->dbus->message_new_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property");
    }

    driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
    driver->dbus->message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult MessageHandler(DBusConnection *connection, DBusMessage *msg, void *user_data)
{
    SDL_Tray *tray;
    SDL_TrayDriverDBus *driver;
    DBusMessage *reply;
    
    tray = user_data;
    driver = (SDL_TrayDriverDBus *)tray->driver;

    if (driver->dbus->message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
        return HandleGetProp(tray, (SDL_TrayDBus *)tray, driver, msg);
    } else if (driver->dbus->message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll")) {
        return HandleGetAllProps(tray, (SDL_TrayDBus *)tray, driver, msg);
    } else if (driver->dbus->message_is_method_call(msg, SNI_INTERFACE, "ContextMenu")) {
        reply = driver->dbus->message_new_method_return(msg);
        driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
        driver->dbus->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (driver->dbus->message_is_method_call(msg, SNI_INTERFACE, "Activate")) {
        reply = driver->dbus->message_new_method_return(msg);
        driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
        driver->dbus->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (driver->dbus->message_is_method_call(msg, SNI_INTERFACE, "SecondaryActivate")) {
        reply = driver->dbus->message_new_method_return(msg);
        driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
        driver->dbus->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (driver->dbus->message_is_method_call(msg, SNI_INTERFACE, "Scroll")) {
        reply = driver->dbus->message_new_method_return(msg);
        driver->dbus->connection_send(driver->dbus->session_conn, reply, NULL);
        driver->dbus->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

SDL_Tray *CreateTray(SDL_TrayDriver *driver, SDL_Surface *icon, const char *tooltip)
{
    SDL_TrayDriverDBus *dbus_driver;
    SDL_TrayDBus *tray_dbus;
    SDL_Tray *tray;
    const char *object_path;
    DBusObjectPathVTable vtable;
    DBusError err;
    int status;
    dbus_bool_t bool_status;
#define CLEANUP()                           \
    SDL_free(tray_dbus->tooltip);           \
    SDL_DestroySurface(tray_dbus->surface); \
    SDL_free(tray_dbus)
#define CLEANUP2()                                              \
    dbus_driver->dbus->connection_close(tray_dbus->connection); \
    CLEANUP()

    /* alloc */
    tray_dbus = SDL_malloc(sizeof(SDL_TrayDBus));
    tray = (SDL_Tray *)tray_dbus;
    if (!tray_dbus) {
        return NULL;
    }

    /* fill */
    tray->menu = NULL;
    tray->driver = driver;
    if (tooltip) {
        tray_dbus->tooltip = SDL_strdup(tooltip);
    } else {
        tray_dbus->tooltip = NULL;
    }
    tray_dbus->surface = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_ARGB32);
    tray_dbus->free_list = NULL;
    tray_dbus->break_update = false;

    /* connect */
    dbus_driver = (SDL_TrayDriverDBus *)driver;
    dbus_driver->dbus->error_init(&err);
    tray_dbus->connection = dbus_driver->dbus->bus_get_private(DBUS_BUS_SESSION, &err);
    if (dbus_driver->dbus->error_is_set(&err)) {
        SDL_SetError("Unable to create tray: %s", err.message);
        dbus_driver->dbus->error_free(&err);
        CLEANUP();
        return NULL;
    }
    if (!tray_dbus->connection) {
        SDL_SetError("Unable to create tray: unable to get connection!");
        CLEANUP();
        return NULL;
    }

    /* request name */
    driver->count++;
    SDL_asprintf(&tray_dbus->object_name, "org.kde.StatusNotifierItem-%d-%d", getpid(), driver->count);
    status = dbus_driver->dbus->bus_request_name(tray_dbus->connection, tray_dbus->object_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_driver->dbus->error_is_set(&err)) {
        SDL_SetError("Unable to create tray: %s", err.message);
        dbus_driver->dbus->error_free(&err);
        CLEANUP2();
        return NULL;
    }
    if (status != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        SDL_SetError("Unable to create tray: unable to request a unique name!");
        CLEANUP2();
        return NULL;
    }

    /* create object */
    object_path = SNI_OBJECT_PATH;
    vtable.message_function = MessageHandler;
    bool_status = dbus_driver->dbus->connection_try_register_object_path(tray_dbus->connection, object_path, &vtable, tray_dbus, &err);
    if (dbus_driver->dbus->error_is_set(&err)) {
        SDL_SetError("Unable to create tray: %s", err.message);
        dbus_driver->dbus->error_free(&err);
        CLEANUP2();
        return NULL;
    }
    if (!bool_status) {
        SDL_SetError("Unable to create tray: unable to register object path!");
        CLEANUP2();
        return NULL;
    }
    object_path = tray_dbus->object_name;
    if (!SDL_DBus_CallVoidMethodOnConnection(tray_dbus->connection, SNI_WATCHER_SERVICE, SNI_WATCHER_PATH, SNI_WATCHER_INTERFACE, "RegisterStatusNotifierItem", DBUS_TYPE_STRING, &object_path, DBUS_TYPE_INVALID)) {
        SDL_SetError("Unable to create tray: unable to register status notifier item!");
        CLEANUP2();
        return NULL;
    }

    return tray;
}

void DestroyTray(SDL_Tray *tray)
{
    SDL_TrayDBus *tray_dbus;
    SDL_TrayDriverDBus *driver;
    SDL_ListNode *cursor;
    
    driver = (SDL_TrayDriverDBus *)tray->driver;
    tray_dbus = (SDL_TrayDBus *)tray;

    /* destroy connection */
    driver->dbus->connection_flush(tray_dbus->connection);
    tray_dbus->break_update = true;
    driver->dbus->connection_close(tray_dbus->connection);
    tray_dbus->connection = NULL;

    /* destroy icon and tooltip */
    SDL_free(tray_dbus->tooltip);
    SDL_DestroySurface(tray_dbus->surface);
    
    /* free items to be freed */
    cursor = tray_dbus->free_list;
    while (cursor) {    
        ItemToFree *free_item;
            
        free_item = cursor->entry;
        free_item->func(free_item->item);
        SDL_free(free_item);
        
        cursor = cursor->next;
    }
    SDL_ListClear(&tray_dbus->free_list);
    
    /* free the tray */
    SDL_free(tray);
}

void UpdateTray(SDL_Tray *tray)
{
    SDL_TrayDBus *tray_dbus;
    SDL_TrayDriverDBus *driver;

    driver = (SDL_TrayDriverDBus *)tray->driver;
    tray_dbus = (SDL_TrayDBus *)tray;

    if (tray_dbus->break_update) {
        return;
    }
    
    driver->dbus->connection_read_write(tray_dbus->connection, 0);
    while (driver->dbus->connection_dispatch(tray_dbus->connection) == DBUS_DISPATCH_DATA_REMAINS) {
        if (tray_dbus->break_update) {
            break;
        }    
        SDL_DelayNS(SDL_US_TO_NS(10));
    }
}

void SetTrayIcon(SDL_Tray *tray, SDL_Surface *surface)
{
    SDL_TrayDBus *tray_dbus;
    SDL_TrayDriverDBus *driver;
    DBusMessage *signal;

    driver = (SDL_TrayDriverDBus *)tray->driver;
    tray_dbus = (SDL_TrayDBus *)tray;

    if (tray_dbus->surface) {
        SDL_DestroySurface(tray_dbus->surface);
    }
    tray_dbus->surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ARGB32);

    signal = driver->dbus->message_new_signal(SNI_OBJECT_PATH, SNI_INTERFACE, "NewIcon");
    driver->dbus->connection_send(tray_dbus->connection, signal, NULL);
    driver->dbus->connection_flush(tray_dbus->connection);
    driver->dbus->message_unref(signal);
}

void SetTrayTooltip(SDL_Tray *tray, const char *text)
{
    SDL_TrayDBus *tray_dbus;
    SDL_TrayDriverDBus *driver;
    DBusMessage *signal;

    driver = (SDL_TrayDriverDBus *)tray->driver;
    tray_dbus = (SDL_TrayDBus *)tray;

    if (tray_dbus->tooltip) {
        SDL_free(tray_dbus->tooltip);
    }
    
    if (text) {
        tray_dbus->tooltip = SDL_strdup(text);
    } else {
        tray_dbus->tooltip = NULL;
    }

    signal = driver->dbus->message_new_signal(SNI_OBJECT_PATH, SNI_INTERFACE, "NewToolTip");
    driver->dbus->connection_send(tray_dbus->connection, signal, NULL);
    driver->dbus->connection_flush(tray_dbus->connection);
    driver->dbus->message_unref(signal);
}

SDL_TrayMenu *CreateTrayMenu(SDL_Tray *tray)
{
    SDL_TrayMenuDBus *menu_dbus;

    menu_dbus = SDL_malloc(sizeof(SDL_TrayMenuDBus));
    tray->menu = (SDL_TrayMenu *)menu_dbus;
    if (!menu_dbus) {
        SDL_SetError("Unable to create tray menu: allocation failure!");
        return NULL;
    }

    menu_dbus->menu = NULL;
    menu_dbus->menu_path = NULL;
    tray->menu->parent_tray = tray;
    tray->menu->parent_entry = NULL;

    return tray->menu;
}

SDL_TrayMenu *CreateTraySubmenu(SDL_TrayEntry *entry)
{
    SDL_TrayMenuDBus *menu_dbus;
    SDL_TrayMenu *menu;
    SDL_TrayEntryDBus *entry_dbus;

    entry_dbus = (SDL_TrayEntryDBus *)entry;
    menu_dbus = SDL_malloc(sizeof(SDL_TrayMenuDBus));
    menu = (SDL_TrayMenu *)menu_dbus;
    if (!menu_dbus) {
        SDL_SetError("Unable to create tray submenu: allocation failure!");
        return NULL;
    }

    menu_dbus->menu = NULL;
    menu_dbus->menu_path = NULL;
    menu->parent_tray = entry->parent->parent_tray;
    menu->parent_entry = entry;
    entry_dbus->sub_menu = menu_dbus;
    
    return menu;
}

SDL_TrayMenu *GetTraySubmenu(SDL_TrayEntry *entry)
{
    SDL_TrayEntryDBus *entry_dbus;

    entry_dbus = (SDL_TrayEntryDBus *)entry;

    return (SDL_TrayMenu *)entry_dbus->sub_menu;
}

SDL_TrayEntry *InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_Tray *tray;
    SDL_TrayDriverDBus *driver;
    SDL_TrayMenuDBus *menu_dbus;
    SDL_TrayDBus *tray_dbus;
    SDL_TrayEntry *entry;
    SDL_TrayEntryDBus *entry_dbus;
    bool update;

    tray = menu->parent_tray;
    menu_dbus = (SDL_TrayMenuDBus *)menu;
    tray_dbus = (SDL_TrayDBus *)tray;
    driver = (SDL_TrayDriverDBus *)tray->driver;

    /* TODD: pos */
    entry_dbus = SDL_malloc(sizeof(SDL_TrayEntryDBus));
    entry = (SDL_TrayEntry *)entry_dbus;
    if (!entry_dbus) {
        SDL_SetError("Unable to create tray entry: allocation failure!");
        return NULL;
    }
    
    entry->parent = menu;
    entry_dbus->item.utf8 = label;
    if (!label) {
        entry_dbus->item.type = SDL_DBUS_MENU_ITEM_TYPE_SEPERATOR;
    } else if (flags & SDL_TRAYENTRY_CHECKBOX) {
        entry_dbus->item.type = SDL_DBUS_MENU_ITEM_TYPE_CHECKBOX;
    } else {
        entry_dbus->item.type = SDL_DBUS_MENU_ITEM_TYPE_NORMAL;
    }
    entry_dbus->item.flags = SDL_DBUS_MENU_ITEM_FLAGS_NONE;
    entry_dbus->item.cb_data = NULL;
    entry_dbus->item.cb = NULL;
    entry_dbus->item.sub_menu = NULL;
    entry_dbus->item.udata = entry_dbus;
    entry_dbus->sub_menu = NULL;
    SDL_DBus_InitMenuItemInternals(&entry_dbus->item);
    
    if (menu_dbus->menu) {
        update = true;
    } else {
        update = false;
    }
    
    if (menu->parent_entry) {
        update = true;
    }
    
    SDL_ListAppend(&menu_dbus->menu, &entry_dbus->item);

    if (update) {
        SDL_TrayMenuDBus *main_menu_dbus;
        
        main_menu_dbus = (SDL_TrayMenuDBus *)tray->menu;
        SDL_DBus_UpdateMenu(driver->dbus, tray_dbus->connection, main_menu_dbus->menu);
    } else {
        menu_dbus->menu_path = SDL_DBus_ExportMenu(driver->dbus, tray_dbus->connection, menu_dbus->menu);
    }

    return entry;
}

SDL_TrayEntry **GetTrayEntries(SDL_TrayMenu *menu, int *count) {
    SDL_TrayEntry **ret;
    SDL_Tray *tray;
    SDL_TrayDBus *tray_dbus;
    SDL_TrayMenuDBus *menu_dbus;
    SDL_ListNode *cursor;
    ItemToFree *free_item;
    int sz;
    int i;
    
    tray = menu->parent_tray;
    tray_dbus = (SDL_TrayDBus *)tray;
    menu_dbus = (SDL_TrayMenuDBus *)menu;
    
    cursor = tray_dbus->free_list;
    while (cursor) {        
        free_item = cursor->entry;
        free_item->func(free_item->item);
        SDL_free(free_item);
        
        cursor = cursor->next;
    }
    SDL_ListClear(&tray_dbus->free_list);

    sz = SDL_ListCountEntries(&menu_dbus->menu);
    ret = SDL_calloc(sz + 1, sizeof(SDL_TrayEntry *));
    if (!ret) {
        SDL_SetError("Memory allocation failure!");
        return NULL;
    }
    free_item = SDL_malloc(sizeof(ItemToFree));
    free_item->item = ret;
    free_item->func = SDL_free;
    SDL_ListAdd(&tray_dbus->free_list, free_item);
    
    i = 0;
    cursor = menu_dbus->menu;
    while (cursor) {
        SDL_DBusMenuItem *item;
        
        item = cursor->entry;
        ret[i] = item->udata;
        cursor = cursor->next;
        i++;
    }    
    ret[sz] = NULL;
    
    *count = sz;
    return ret;
}

void RemoveTrayEntry(SDL_TrayEntry *entry)
{
    SDL_TrayEntryDBus *entry_dbus;
    SDL_TrayMenuDBus *menu_dbus;
    SDL_Tray *tray;
    SDL_TrayDriverDBus *driver;
    SDL_TrayDBus *tray_dbus;

    tray = entry->parent->parent_tray;
    tray_dbus = (SDL_TrayDBus *)tray;
    driver = (SDL_TrayDriverDBus *)tray->driver;
    entry_dbus = (SDL_TrayEntryDBus *)entry;
    menu_dbus = (SDL_TrayMenuDBus *)entry->parent;
    
    SDL_ListRemove(&menu_dbus->menu, &entry_dbus->item);
    /* TODO: destroy submenu */
    SDL_free(entry);
    
    SDL_DBus_UpdateMenu(driver->dbus, tray_dbus->connection, menu_dbus->menu);
}

void EntryCallback(SDL_DBusMenuItem *item, void *udata) {
    SDL_TrayCallback entry_cb;
    
    entry_cb = item->udata2;
    entry_cb(udata, item->udata);
}

void SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    SDL_TrayEntryDBus *entry_dbus;
    SDL_TrayMenuDBus *menu_dbus;
    SDL_Tray *tray;
    SDL_TrayDriverDBus *driver;
    SDL_TrayDBus *tray_dbus;

    tray = entry->parent->parent_tray;
    tray_dbus = (SDL_TrayDBus *)tray;
    driver = (SDL_TrayDriverDBus *)tray->driver;
    entry_dbus = (SDL_TrayEntryDBus *)entry;
    menu_dbus = (SDL_TrayMenuDBus *)entry->parent;

    entry_dbus->item.cb = EntryCallback;
    entry_dbus->item.cb_data = userdata;
    entry_dbus->item.udata2 = callback;

    SDL_DBus_UpdateMenu(driver->dbus, tray_dbus->connection, menu_dbus->menu);
}

void DestroyDriver(SDL_TrayDriver *driver)
{
    SDL_DBus_Quit();
    SDL_free(driver);
}

SDL_TrayDriver *SDL_Tray_CreateDBusDriver(void)
{
    SDL_TrayDriverDBus *dbus_driver;
    SDL_TrayDriver *driver;
    SDL_DBusContext *ctx;
    char **paths;
    int count;
    int i;
    bool sni_supported;

    /* init dbus, get ctx */
    SDL_DBus_Init();
    ctx = SDL_DBus_GetContext();
    if (!ctx) {
        return NULL;
    }

    /* detect if we have a watcher and a host */
    sni_supported = false;
    paths = NULL;
    count = 0;
    if (SDL_DBus_CallMethod("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames", DBUS_TYPE_INVALID, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &paths, &count, DBUS_TYPE_INVALID)) {
        if (paths) {
            bool watcher_found;

            watcher_found = false;
            for (i = 0; i < count; i++) {
                if (!SDL_strcmp(paths[i], SNI_WATCHER_SERVICE)) {
                    watcher_found = true;
                }
            }
            ctx->free_string_array(paths);

            if (watcher_found) {
                dbus_bool_t host_registered;

                host_registered = FALSE;
                SDL_DBus_QueryProperty(SNI_WATCHER_SERVICE, SNI_WATCHER_PATH, SNI_WATCHER_INTERFACE, "IsStatusNotifierHostRegistered", DBUS_TYPE_BOOLEAN, &host_registered);
                if (watcher_found && host_registered) {
                    sni_supported = true;
                }
            }
        }
    }
    if (!sni_supported) {
        SDL_SetError("Unable to create tray: no SNI support!");
        SDL_DBus_Quit();
        return NULL;
    }

    /* alloc */
    dbus_driver = SDL_malloc(sizeof(SDL_TrayDriverDBus));
    driver = (SDL_TrayDriver *)dbus_driver;
    if (!dbus_driver) {
        return NULL;
    }

    /* fill */
    dbus_driver->dbus = ctx;
    driver->name = "dbus";
    driver->count = 0;
    driver->CreateTray = CreateTray;
    driver->DestroyTray = DestroyTray;
    driver->UpdateTray = UpdateTray;
    driver->SetTrayIcon = SetTrayIcon;
    driver->SetTrayTooltip = SetTrayTooltip;
    driver->CreateTrayMenu = CreateTrayMenu;
    driver->InsertTrayEntryAt = InsertTrayEntryAt;
    driver->CreateTraySubmenu = CreateTraySubmenu;
    driver->GetTraySubmenu = GetTraySubmenu;
    driver->GetTrayEntries = GetTrayEntries;
    driver->RemoveTrayEntry = RemoveTrayEntry;
    driver->SetTrayEntryCallback = SetTrayEntryCallback;
    driver->DestroyDriver = DestroyDriver;
    
    return driver;
}

#endif
