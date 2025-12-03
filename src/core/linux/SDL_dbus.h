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

#ifndef SDL_dbus_h_
#define SDL_dbus_h_

#ifdef HAVE_DBUS_DBUS_H
#define SDL_USE_LIBDBUS 1
#include "../../SDL_list.h"
#include <dbus/dbus.h>

#ifndef DBUS_TIMEOUT_USE_DEFAULT
#define DBUS_TIMEOUT_USE_DEFAULT -1
#endif
#ifndef DBUS_TIMEOUT_INFINITE
#define DBUS_TIMEOUT_INFINITE ((int)0x7fffffff)
#endif
#ifndef DBUS_TYPE_UNIX_FD
#define DBUS_TYPE_UNIX_FD ((int)'h')
#endif
#ifndef DBUS_ERROR_UNKNOWN_PROPERTY
#define DBUS_ERROR_UNKNOWN_PROPERTY "org.freedesktop.DBus.Error.UnknownProperty"
#endif

typedef struct SDL_DBusContext
{
    DBusConnection *session_conn;
    DBusConnection *system_conn;

    DBusConnection *(*bus_get_private)(DBusBusType, DBusError *);
    dbus_bool_t (*bus_register)(DBusConnection *, DBusError *);
    void (*bus_add_match)(DBusConnection *, const char *, DBusError *);
    void (*bus_remove_match)(DBusConnection *, const char *, DBusError *);
    int (*bus_request_name)(DBusConnection *, const char *, unsigned int, DBusError *);
    const char *(*bus_get_unique_name)(DBusConnection *);
    DBusConnection *(*connection_open_private)(const char *, DBusError *);
    void (*connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t);
    dbus_bool_t (*connection_get_is_connected)(DBusConnection *);
    dbus_bool_t (*connection_add_filter)(DBusConnection *, DBusHandleMessageFunction, void *, DBusFreeFunction);
    dbus_bool_t (*connection_remove_filter)(DBusConnection *, DBusHandleMessageFunction, void *);
    dbus_bool_t (*connection_try_register_object_path)(DBusConnection *, const char *,
                                                       const DBusObjectPathVTable *, void *, DBusError *);
    dbus_bool_t (*connection_send)(DBusConnection *, DBusMessage *, dbus_uint32_t *);
    DBusMessage *(*connection_send_with_reply_and_block)(DBusConnection *, DBusMessage *, int, DBusError *);
    void (*connection_close)(DBusConnection *);
    void (*connection_ref)(DBusConnection *);
    void (*connection_unref)(DBusConnection *);
    void (*connection_flush)(DBusConnection *);
    dbus_bool_t (*connection_read_write)(DBusConnection *, int);
    dbus_bool_t (*connection_read_write_dispatch)(DBusConnection *, int);
    DBusDispatchStatus (*connection_dispatch)(DBusConnection *);
    dbus_bool_t (*message_is_signal)(DBusMessage *, const char *, const char *);
    dbus_bool_t (*message_is_method_call)(DBusMessage *, const char *, const char *);
    dbus_bool_t (*message_has_path)(DBusMessage *, const char *);
    DBusMessage *(*message_new_error)(DBusMessage *, const char *, const char *);
    DBusMessage *(*message_new_method_call)(const char *, const char *, const char *, const char *);
    DBusMessage *(*message_new_method_return)(DBusMessage *);
    DBusMessage *(*message_new_signal)(const char *, const char *, const char *);
    dbus_bool_t (*message_append_args)(DBusMessage *, int, ...);
    dbus_bool_t (*message_append_args_valist)(DBusMessage *, int, va_list);
    void (*message_iter_init_append)(DBusMessage *, DBusMessageIter *);
    dbus_bool_t (*message_iter_open_container)(DBusMessageIter *, int, const char *, DBusMessageIter *);
    dbus_bool_t (*message_iter_append_basic)(DBusMessageIter *, int, const void *);
    dbus_bool_t (*message_iter_append_fixed_array)(DBusMessageIter *, int, const void *, int);
    dbus_bool_t (*message_iter_close_container)(DBusMessageIter *, DBusMessageIter *);
    dbus_bool_t (*message_get_args)(DBusMessage *, DBusError *, int, ...);
    dbus_bool_t (*message_get_args_valist)(DBusMessage *, DBusError *, int, va_list);
    void (*message_iter_get_fixed_array)(DBusMessageIter *, void *, int *);
    dbus_bool_t (*message_iter_init)(DBusMessage *, DBusMessageIter *);
    dbus_bool_t (*message_iter_next)(DBusMessageIter *);
    void (*message_iter_get_basic)(DBusMessageIter *, void *);
    int (*message_iter_get_arg_type)(DBusMessageIter *);
    void (*message_iter_recurse)(DBusMessageIter *, DBusMessageIter *);
    void (*message_unref)(DBusMessage *);
    dbus_bool_t (*threads_init_default)(void);
    void (*error_init)(DBusError *);
    dbus_bool_t (*error_is_set)(const DBusError *);
    dbus_bool_t (*error_has_name)(const DBusError *, const char *);
    void (*error_free)(DBusError *);
    char *(*get_local_machine_id)(void);
    char *(*try_get_local_machine_id)(DBusError *);
    void (*free)(void *);
    void (*free_string_array)(char **);
    void (*shutdown)(void);

} SDL_DBusContext;

typedef enum SDL_DBusMenuItemType
{
    SDL_DBUS_MENU_ITEM_TYPE_NORMAL,
    SDL_DBUS_MENU_ITEM_TYPE_SEPERATOR,
    SDL_DBUS_MENU_ITEM_TYPE_CHECKBOX
} SDL_DBusMenuItemType;

typedef enum SDL_DBusMenuItemFlags
{
    SDL_DBUS_MENU_ITEM_FLAGS_NONE = 0,
    SDL_DBUS_MENU_ITEM_FLAGS_DISABLED = 1 << 0,
    SDL_DBUS_MENU_ITEM_FLAGS_CHECKED = 1 << 1,
    SDL_DBUS_MENU_ITEM_FLAGS_BAR_ITEM = 1 << 2
} SDL_DBusMenuItemFlags;

/* This struct is meant to be in an SDL_ListNode just like sub_menu */
typedef struct SDL_DBusMenuItem
{
    const char *utf8 /* Text */;
    SDL_DBusMenuItemType type;
    SDL_DBusMenuItemFlags flags;
    void *cb_data;
    void (*cb)(struct SDL_DBusMenuItem *, void *);
    SDL_ListNode *sub_menu; /* NULL if no submenu */

    /* Internal use */
    SDL_DBusContext *dbus;
    long id;
    unsigned long revision;
} SDL_DBusMenuItem;

extern void SDL_DBus_Init(void);
extern void SDL_DBus_Quit(void);
extern SDL_DBusContext *SDL_DBus_GetContext(void);

// These use the built-in Session connection.
extern bool SDL_DBus_CallMethod(const char *node, const char *path, const char *interface, const char *method, ...);
extern bool SDL_DBus_CallVoidMethod(const char *node, const char *path, const char *interface, const char *method, ...);
extern bool SDL_DBus_QueryProperty(const char *node, const char *path, const char *interface, const char *property, int expectedtype, void *result);

// These use whatever connection you like.
extern bool SDL_DBus_CallMethodOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...);
extern bool SDL_DBus_CallVoidMethodOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...);
extern bool SDL_DBus_QueryPropertyOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *property, int expectedtype, void *result);

extern void SDL_DBus_ScreensaverTickle(void);
extern bool SDL_DBus_ScreensaverInhibit(bool inhibit);

extern void SDL_DBus_PumpEvents(void);
extern char *SDL_DBus_GetLocalMachineId(void);

extern char **SDL_DBus_DocumentsPortalRetrieveFiles(const char *key, int *files_count);

extern int SDL_DBus_CameraPortalRequestAccess(void);

/* Menu functions */
extern const char *SDL_DBus_ExportMenu(SDL_DBusContext *ctx, DBusConnection *conn, SDL_ListNode *menu);
extern void SDL_DBus_UpdateMenu(SDL_DBusContext *ctx, DBusConnection *conn, SDL_ListNode *menu);

#endif // HAVE_DBUS_DBUS_H

#endif // SDL_dbus_h_
