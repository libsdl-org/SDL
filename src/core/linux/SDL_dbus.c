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
#include "../../stdlib/SDL_vacopy.h"
#include "SDL_dbus.h"

#ifdef SDL_USE_LIBDBUS
// we never link directly to libdbus.
#define SDL_DRIVER_DBUS_DYNAMIC "libdbus-1.so.3"
static const char *dbus_library = SDL_DRIVER_DBUS_DYNAMIC;
static SDL_SharedObject *dbus_handle = NULL;
static char *inhibit_handle = NULL;
static unsigned int screensaver_cookie = 0;
static SDL_DBusContext dbus;

#define DBUS_MENU_INTERFACE   "com.canonical.dbusmenu"
#define DBUS_MENU_OBJECT_PATH "/Menu"

static const char *menu_introspect= "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<!--\r\nA library to allow applications to provide simple indications of\r\ninformation to be displayed to users of the application through the\r\ninterface shell.\r\n\r\nCopyright 2009 Canonical Ltd.\r\n\r\nAuthors:\r\n    Ted Gould <ted@canonical.com>\r\n    Aurélien Gâteau <aurelien.gateau@canonical.com>\r\n\r\nThis program is free software: you can redistribute it and/or modify it \r\nunder the terms of either or both of the following licenses:\r\n\r\n1) the GNU Lesser General Public License version 3, as published by the \r\nFree Software Foundation; and/or\r\n2) the GNU Lesser General Public License version 2.1, as published by \r\nthe Free Software Foundation.\r\n\r\nThis program is distributed in the hope that it will be useful, but \r\nWITHOUT ANY WARRANTY; without even the implied warranties of \r\nMERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR \r\nPURPOSE.  See the applicable version of the GNU Lesser General Public \r\nLicense for more details.\r\n\r\nYou should have received a copy of both the GNU Lesser General Public \r\nLicense version 3 and version 2.1 along with this program.  If not, see \r\n<http://www.gnu.org/licenses/>\r\n-->\r\n<node name=\"/\" xmlns:dox=\"http://www.canonical.com/dbus/dox.dtd\">\r\n    <dox:d><![CDATA[\r\n    @mainpage\r\n\r\n    The goal of DBusMenu is to expose menus on DBus.\r\n    \r\n    Main interface is documented here: @ref com::canonical::dbusmenu\r\n    ]]></dox:d>\r\n\t<interface name=\"com.canonical.dbusmenu\">\r\n\t\t<dox:d><![CDATA[\r\n\t\tA DBus interface to expose menus on DBus.\r\n\r\n\t\tMenu items are represented with a unique numeric id and a dictionary of\r\n\t\tproperties.\r\n\r\n\t\tTo reduce the amount of DBus traffic, a property should only be returned\r\n\t\tif its value is not the default value.\r\n\r\n\t\tAvailable properties are:\r\n\r\n\t\t<table>\r\n\t\t<tr>\r\n\t\t\t<th>Name</th>\r\n\t\t\t<th>Type</th>\r\n\t\t\t<th>Description</th>\r\n\t\t\t<th>Default Value</th>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>type</td>\r\n\t\t\t<td>String</td>\r\n\t\t\t<td>Can be one of:\r\n\t\t\t- \"standard\": an item which can be clicked to trigger an action or\r\n\t\t\t  show another menu\r\n\t\t\t- \"separator\": a separator\r\n\r\n\t\t\tVendor specific types can be added by prefixing them with\r\n\t\t\t\"x-<vendor>-\".\r\n\t\t\t</td>\r\n\t\t\t<td>\"standard\"</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>label</td>\r\n\t\t\t<td>string</td>\r\n\t\t\t<td>Text of the item, except that:\r\n\t\t\t-# two consecutive underscore characters \"__\" are displayed as a\r\n\t\t\tsingle underscore,\r\n\t\t\t-# any remaining underscore characters are not displayed at all,\r\n\t\t\t-# the first of those remaining underscore characters (unless it is\r\n\t\t\tthe last character in the string) indicates that the following\r\n\t\t\tcharacter is the access key.\r\n\t\t\t</td>\r\n\t\t\t<td>\"\"</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>enabled</td>\r\n\t\t\t<td>boolean</td>\r\n\t\t\t<td>Whether the item can be activated or not.</td>\r\n\t\t\t<td>true</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>visible</td>\r\n\t\t\t<td>boolean</td>\r\n\t\t\t<td>True if the item is visible in the menu.</td>\r\n\t\t\t<td>true</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>icon-name</td>\r\n\t\t\t<td>string</td>\r\n\t\t\t<td>Icon name of the item, following the freedesktop.org icon spec.</td>\r\n\t\t\t<td>\"\"</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>icon-data</td>\r\n\t\t\t<td>binary</td>\r\n\t\t\t<td>PNG data of the icon.</td>\r\n\t\t\t<td>Empty</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>shortcut</td>\r\n\t\t\t<td>array of arrays of strings</td>\r\n\t\t\t<td>The shortcut of the item. Each array represents the key press\r\n\t\t\tin the list of keypresses. Each list of strings contains a list of\r\n\t\t\tmodifiers and then the key that is used. The modifier strings\r\n\t\t\tallowed are: \"Control\", \"Alt\", \"Shift\" and \"Super\".\r\n\r\n\t\t\t- A simple shortcut like Ctrl+S is represented as:\r\n\t\t\t  [[\"Control\", \"S\"]]\r\n\t\t\t- A complex shortcut like Ctrl+Q, Alt+X is represented as:\r\n\t\t\t  [[\"Control\", \"Q\"], [\"Alt\", \"X\"]]</td>\r\n\t\t\t<td>Empty</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>toggle-type</td>\r\n\t\t\t<td>string</td>\r\n\t\t\t<td>\r\n\t\t\tIf the item can be toggled, this property should be set to:\r\n\t\t\t- \"checkmark\": Item is an independent togglable item\r\n\t\t\t- \"radio\": Item is part of a group where only one item can be\r\n\t\t\t  toggled at a time\r\n\t\t\t- \"\": Item cannot be toggled\r\n\t\t\t</td>\r\n\t\t\t<td>\"\"</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>toggle-state</td>\r\n\t\t\t<td>int</td>\r\n\t\t\t<td>\r\n\t\t\tDescribe the current state of a \"togglable\" item. Can be one of:\r\n\t\t\t- 0 = off\r\n\t\t\t- 1 = on\r\n\t\t\t- anything else = indeterminate\r\n\r\n\t\t\tNote:\r\n\t\t\tThe implementation does not itself handle ensuring that only one\r\n\t\t\titem in a radio group is set to \"on\", or that a group does not have\r\n\t\t\t\"on\" and \"indeterminate\" items simultaneously; maintaining this\r\n\t\t\tpolicy is up to the toolkit wrappers.\r\n\t\t\t</td>\r\n\t\t\t<td>-1</td>\r\n\t\t</tr>\r\n\t\t<tr>\r\n\t\t\t<td>children-display</td>\r\n\t\t\t<td>string</td>\r\n\t\t\t<td>\r\n\t\t\tIf the menu item has children this property should be set to\r\n\t\t\t\"submenu\".\r\n\t\t\t</td>\r\n\t\t\t<td>\"\"</td>\r\n\t\t</tr>\r\n\t\t</table>\r\n\r\n\t\tVendor specific properties can be added by prefixing them with\r\n\t\t\"x-<vendor>-\".\r\n\t\t]]></dox:d>\r\n\r\n<!-- Properties -->\r\n\t\t<property name=\"Version\" type=\"u\" access=\"read\">\r\n\t\t\t<dox:d>\r\n\t\t\tProvides the version of the DBusmenu API that this API is\r\n\t\t\timplementing.\r\n\t\t\t</dox:d>\r\n\t\t</property>\r\n\r\n        <property name=\"Status\" type=\"s\" access=\"read\">\r\n            <dox:d>\r\n            Tells if the menus are in a normal state or they believe that they\r\n            could use some attention.  Cases for showing them would be if help\r\n            were referring to them or they accessors were being highlighted.\r\n            This property can have two values: \"normal\" in almost all cases and\r\n            \"notice\" when they should have a higher priority to be shown.\r\n            </dox:d>\r\n        </property>\r\n\r\n<!-- Functions -->\r\n\r\n\t\t<method name=\"GetLayout\">\r\n\t\t\t<annotation name=\"com.trolltech.QtDBus.QtTypeName.Out1\" value=\"DBusMenuLayoutItem\"/>\r\n\t\t\t<dox:d>\r\n\t\t\t  Provides the layout and propertiers that are attached to the entries\r\n\t\t\t  that are in the layout.  It only gives the items that are children\r\n\t\t\t  of the item that is specified in @a parentId.  It will return all of the\r\n\t\t\t  properties or specific ones depending of the value in @a propertyNames.\r\n\r\n\t\t\t  The format is recursive, where the second \'v\' is in the same format\r\n\t\t\t  as the original \'a(ia{sv}av)\'.  Its content depends on the value\r\n\t\t\t  of @a recursionDepth.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"i\" name=\"parentId\" direction=\"in\">\r\n\t\t\t\t<dox:d>The ID of the parent node for the layout.  For\r\n\t\t\t\tgrabbing the layout from the root node use zero.</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"i\" name=\"recursionDepth\" direction=\"in\">\r\n\t\t\t\t<dox:d>\r\n\t\t\t\t  The amount of levels of recursion to use.  This affects the\r\n\t\t\t\t  content of the second variant array.\r\n\t\t\t\t  - -1: deliver all the items under the @a parentId.\r\n\t\t\t\t  - 0: no recursion, the array will be empty.\r\n\t\t\t\t  - n: array will contains items up to \'n\' level depth.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"as\" name=\"propertyNames\" direction=\"in\" >\r\n\t\t\t\t<dox:d>\r\n\t\t\t\t\tThe list of item properties we are\r\n\t\t\t\t\tinterested in.  If there are no entries in the list all of\r\n\t\t\t\t\tthe properties will be sent.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"u\" name=\"revision\" direction=\"out\">\r\n\t\t\t\t<dox:d>The revision number of the layout.  For matching\r\n\t\t\t\twith layoutUpdated signals.</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"(ia{sv}av)\" name=\"layout\" direction=\"out\">\r\n\t\t\t\t<dox:d>The layout, as a recursive structure.</dox:d>\r\n\t\t\t</arg>\r\n\t\t</method>\r\n\r\n\t\t<method name=\"GetGroupProperties\">\r\n\t\t\t<annotation name=\"com.trolltech.QtDBus.QtTypeName.In0\" value=\"QList&lt;int&gt;\"/>\r\n\t\t\t<annotation name=\"com.trolltech.QtDBus.QtTypeName.Out0\" value=\"DBusMenuItemList\"/>\r\n\t\t\t<dox:d>\r\n\t\t\tReturns the list of items which are children of @a parentId.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"ai\" name=\"ids\" direction=\"in\" >\r\n\t\t\t\t<dox:d>\r\n\t\t\t\t\tA list of ids that we should be finding the properties\r\n\t\t\t\t\ton.  If the list is empty, all menu items should be sent.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"as\" name=\"propertyNames\" direction=\"in\" >\r\n\t\t\t\t<dox:d>\r\n\t\t\t\t\tThe list of item properties we are\r\n\t\t\t\t\tinterested in.  If there are no entries in the list all of\r\n\t\t\t\t\tthe properties will be sent.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"a(ia{sv})\" name=\"properties\" direction=\"out\" >\r\n\t\t\t\t<dox:d>\r\n\t\t\t\t\tAn array of property values.\r\n\t\t\t\t\tAn item in this area is represented as a struct following\r\n\t\t\t\t\tthis format:\r\n\t\t\t\t\t@li id unsigned the item id\r\n\t\t\t\t\t@li properties map(string => variant) the requested item properties\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t</method>\r\n\r\n\t\t<method name=\"GetProperty\">\r\n\t\t\t<dox:d>\r\n\t\t\t  Get a signal property on a single item.  This is not useful if you\'re\r\n\t\t\t  going to implement this interface, it should only be used if you\'re\r\n\t\t\t  debugging via a commandline tool.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"i\" name=\"id\" direction=\"in\">\r\n\t\t\t\t<dox:d>the id of the item which received the event</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"s\" name=\"name\" direction=\"in\">\r\n\t\t\t\t<dox:d>the name of the property to get</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"v\" name=\"value\" direction=\"out\">\r\n\t\t\t\t<dox:d>the value of the property</dox:d>\r\n\t\t\t</arg>\r\n\t\t</method>\r\n\r\n\t\t<method name=\"Event\">\r\n\t\t\t<dox:d><![CDATA[\r\n\t\t\tThis is called by the applet to notify the application an event happened on a\r\n\t\t\tmenu item.\r\n\r\n\t\t\t@a type can be one of the following:\r\n\r\n\t\t\t@li \"clicked\"\r\n\t\t\t@li \"hovered\"\r\n\r\n\t\t\tVendor specific events can be added by prefixing them with \"x-<vendor>-\"\r\n\t\t\t]]></dox:d>\r\n\t\t\t<arg type=\"i\" name=\"id\" direction=\"in\" >\r\n\t\t\t\t<dox:d>the id of the item which received the event</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"s\" name=\"eventId\" direction=\"in\" >\r\n\t\t\t\t<dox:d>the type of event</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"v\" name=\"data\" direction=\"in\" >\r\n\t\t\t\t<dox:d>event-specific data</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"u\" name=\"timestamp\" direction=\"in\" >\r\n\t\t\t\t<dox:d>The time that the event occured if available or the time the message was sent if not</dox:d>\r\n\t\t\t</arg>\r\n\t\t</method>\r\n\r\n\t\t<method name=\"AboutToShow\">\r\n\t\t\t<dox:d>\r\n\t\t\tThis is called by the applet to notify the application that it is about\r\n\t\t\tto show the menu under the specified item.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"i\" name=\"id\" direction=\"in\">\r\n\t\t\t\t<dox:d>\r\n\t\t\t\tWhich menu item represents the parent of the item about to be shown.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"b\" name=\"needUpdate\" direction=\"out\">\r\n\t\t\t\t<dox:d>\r\n\t\t\t\tWhether this AboutToShow event should result in the menu being updated.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t</method>\r\n\r\n<!-- Signals -->\r\n\t\t<signal name=\"ItemsPropertiesUpdated\">\r\n\t\t\t<annotation name=\"com.trolltech.QtDBus.QtTypeName.In0\" value=\"DBusMenuItemList\"/>\r\n\t\t\t<annotation name=\"com.trolltech.QtDBus.QtTypeName.In1\" value=\"DBusMenuItemKeysList\"/>\r\n\t\t\t<dox:d>\r\n\t\t\tTriggered when there are lots of property updates across many items\r\n\t\t\tso they all get grouped into a single dbus message.  The format is\r\n\t\t\tthe ID of the item with a hashtable of names and values for those\r\n\t\t\tproperties.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"a(ia{sv})\" name=\"updatedProps\" direction=\"out\" />\r\n\t\t\t<arg type=\"a(ias)\" name=\"removedProps\" direction=\"out\" />\r\n\t\t</signal>\r\n\r\n\t\t<signal name=\"LayoutUpdated\">\r\n\t\t\t<dox:d>\r\n\t\t\tTriggered by the application to notify display of a layout update, up to\r\n\t\t\trevision\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"u\" name=\"revision\" direction=\"out\" >\r\n\t\t\t\t<dox:d>The revision of the layout that we\'re currently on</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"i\" name=\"parent\" direction=\"out\" >\r\n\t\t\t\t<dox:d>\r\n\t\t\t\tIf the layout update is only of a subtree, this is the\r\n\t\t\t\tparent item for the entries that have changed.  It is zero if\r\n\t\t\t\tthe whole layout should be considered invalid.\r\n\t\t\t\t</dox:d>\r\n\t\t\t</arg>\r\n\t\t</signal>\r\n\t\t<signal name=\"ItemActivationRequested\">\r\n\t\t\t<dox:d>\r\n\t\t\t  The server is requesting that all clients displaying this\r\n\t\t\t  menu open it to the user.  This would be for things like\r\n\t\t\t  hotkeys that when the user presses them the menu should\r\n\t\t\t  open and display itself to the user.\r\n\t\t\t</dox:d>\r\n\t\t\t<arg type=\"i\" name=\"id\" direction=\"out\" >\r\n\t\t\t\t<dox:d>ID of the menu that should be activated</dox:d>\r\n\t\t\t</arg>\r\n\t\t\t<arg type=\"u\" name=\"timestamp\" direction=\"out\" >\r\n\t\t\t\t<dox:d>The time that the event occured</dox:d>\r\n\t\t\t</arg>\r\n\t\t</signal>\r\n\r\n<!-- End of interesting stuff -->\r\n\r\n\t</interface>\r\n</node>";

SDL_ELF_NOTE_DLOPEN(
    "core-libdbus",
    "Support for D-Bus IPC",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_RECOMMENDED,
    SDL_DRIVER_DBUS_DYNAMIC);

static bool LoadDBUSSyms(void)
{
#define SDL_DBUS_SYM2_OPTIONAL(TYPE, x, y) \
    dbus.x = (TYPE)SDL_LoadFunction(dbus_handle, #y)

#define SDL_DBUS_SYM2(TYPE, x, y)                            \
    if (!(dbus.x = (TYPE)SDL_LoadFunction(dbus_handle, #y))) \
    return false

#define SDL_DBUS_SYM_OPTIONAL(TYPE, x) \
    SDL_DBUS_SYM2_OPTIONAL(TYPE, x, dbus_##x)

#define SDL_DBUS_SYM(TYPE, x) \
    SDL_DBUS_SYM2(TYPE, x, dbus_##x)

    SDL_DBUS_SYM(DBusConnection * (*)(DBusBusType, DBusError *), bus_get_private);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, DBusError *), bus_register);
    SDL_DBUS_SYM(void (*)(DBusConnection *, const char *, DBusError *), bus_add_match);
    SDL_DBUS_SYM(void (*)(DBusConnection *, const char *, DBusError *), bus_remove_match);
    SDL_DBUS_SYM(const char *(*)(DBusConnection *), bus_get_unique_name);
    SDL_DBUS_SYM(int (*)(DBusConnection *, const char *, unsigned int, DBusError *), bus_request_name);
    SDL_DBUS_SYM(DBusConnection * (*)(const char *, DBusError *), connection_open_private);
    SDL_DBUS_SYM(void (*)(DBusConnection *, dbus_bool_t), connection_set_exit_on_disconnect);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *), connection_get_is_connected);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, DBusHandleMessageFunction, void *, DBusFreeFunction), connection_add_filter);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, DBusHandleMessageFunction, void *), connection_remove_filter);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, const char *, const DBusObjectPathVTable *, void *, DBusError *), connection_try_register_object_path);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, DBusMessage *, dbus_uint32_t *), connection_send);
    SDL_DBUS_SYM(DBusMessage * (*)(DBusConnection *, DBusMessage *, int, DBusError *), connection_send_with_reply_and_block);
    SDL_DBUS_SYM(void (*)(DBusConnection *), connection_close);
    SDL_DBUS_SYM(void (*)(DBusConnection *), connection_ref);
    SDL_DBUS_SYM(void (*)(DBusConnection *), connection_unref);
    SDL_DBUS_SYM(void (*)(DBusConnection *), connection_flush);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, int), connection_read_write);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusConnection *, int), connection_read_write_dispatch);
    SDL_DBUS_SYM(DBusDispatchStatus(*)(DBusConnection *), connection_dispatch);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, const char *, const char *), message_is_signal);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, const char *, const char *), message_is_method_call);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, const char *), message_has_path);
    SDL_DBUS_SYM(DBusMessage * (*)(DBusMessage *, const char *, const char *), message_new_error);
    SDL_DBUS_SYM(DBusMessage * (*)(const char *, const char *, const char *, const char *), message_new_method_call);
    SDL_DBUS_SYM(DBusMessage * (*)(DBusMessage *), message_new_method_return);
    SDL_DBUS_SYM(DBusMessage * (*)(const char *, const char *, const char *), message_new_signal);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, int, ...), message_append_args);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, int, va_list), message_append_args_valist);
    SDL_DBUS_SYM(void (*)(DBusMessage *, DBusMessageIter *), message_iter_init_append);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessageIter *, int, const char *, DBusMessageIter *), message_iter_open_container);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessageIter *, int, const void *), message_iter_append_basic);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessageIter *, int, const void *, int), message_iter_append_fixed_array);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessageIter *, DBusMessageIter *), message_iter_close_container);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, DBusError *, int, ...), message_get_args);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, DBusError *, int, va_list), message_get_args_valist);
    SDL_DBUS_SYM(void (*)(DBusMessageIter *, void *, int *), message_iter_get_fixed_array);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessage *, DBusMessageIter *), message_iter_init);
    SDL_DBUS_SYM(dbus_bool_t(*)(DBusMessageIter *), message_iter_next);
    SDL_DBUS_SYM(void (*)(DBusMessageIter *, void *), message_iter_get_basic);
    SDL_DBUS_SYM(int (*)(DBusMessageIter *), message_iter_get_arg_type);
    SDL_DBUS_SYM(void (*)(DBusMessageIter *, DBusMessageIter *), message_iter_recurse);
    SDL_DBUS_SYM(void (*)(DBusMessage *), message_unref);
    SDL_DBUS_SYM(dbus_bool_t(*)(void), threads_init_default);
    SDL_DBUS_SYM(void (*)(DBusError *), error_init);
    SDL_DBUS_SYM(dbus_bool_t(*)(const DBusError *), error_is_set);
    SDL_DBUS_SYM(dbus_bool_t(*)(const DBusError *, const char *), error_has_name);
    SDL_DBUS_SYM(void (*)(DBusError *), error_free);
    SDL_DBUS_SYM(char *(*)(void), get_local_machine_id);
    SDL_DBUS_SYM_OPTIONAL(char *(*)(DBusError *), try_get_local_machine_id);
    SDL_DBUS_SYM(void (*)(void *), free);
    SDL_DBUS_SYM(void (*)(char **), free_string_array);
    SDL_DBUS_SYM(void (*)(void), shutdown);

#undef SDL_DBUS_SYM
#undef SDL_DBUS_SYM2

    return true;
}

static void UnloadDBUSLibrary(void)
{
    if (dbus_handle) {
        SDL_UnloadObject(dbus_handle);
        dbus_handle = NULL;
    }
}

static bool LoadDBUSLibrary(void)
{
    bool result = true;
    if (!dbus_handle) {
        dbus_handle = SDL_LoadObject(dbus_library);
        if (!dbus_handle) {
            result = false;
            // Don't call SDL_SetError(): SDL_LoadObject already did.
        } else {
            result = LoadDBUSSyms();
            if (!result) {
                UnloadDBUSLibrary();
            }
        }
    }
    return result;
}

static SDL_InitState dbus_init;

void SDL_DBus_Init(void)
{
    static bool is_dbus_available = true;

    if (!is_dbus_available) {
        return; // don't keep trying if this fails.
    }

    if (!SDL_ShouldInit(&dbus_init)) {
        return;
    }

    if (!LoadDBUSLibrary()) {
        goto error;
    }

    if (!dbus.threads_init_default()) {
        goto error;
    }

    DBusError err;
    dbus.error_init(&err);
    // session bus is required

    dbus.session_conn = dbus.bus_get_private(DBUS_BUS_SESSION, &err);
    if (dbus.error_is_set(&err)) {
        dbus.error_free(&err);
        goto error;
    }
    dbus.connection_set_exit_on_disconnect(dbus.session_conn, 0);

    // system bus is optional
    dbus.system_conn = dbus.bus_get_private(DBUS_BUS_SYSTEM, &err);
    if (!dbus.error_is_set(&err)) {
        dbus.connection_set_exit_on_disconnect(dbus.system_conn, 0);
    }

    dbus.error_free(&err);
    SDL_SetInitialized(&dbus_init, true);
    return;

error:
    is_dbus_available = false;
    SDL_SetInitialized(&dbus_init, true);
    SDL_DBus_Quit();
}

void SDL_DBus_Quit(void)
{
    if (!SDL_ShouldQuit(&dbus_init)) {
        return;
    }

    if (dbus.system_conn) {
        dbus.connection_close(dbus.system_conn);
        dbus.connection_unref(dbus.system_conn);
    }
    if (dbus.session_conn) {
        dbus.connection_close(dbus.session_conn);
        dbus.connection_unref(dbus.session_conn);
    }

    if (SDL_GetHintBoolean(SDL_HINT_SHUTDOWN_DBUS_ON_QUIT, false)) {
        if (dbus.shutdown) {
            dbus.shutdown();
        }

        UnloadDBUSLibrary();
    } else {
        /* Leaving libdbus loaded when skipping dbus_shutdown() avoids
         * spurious leak warnings from LeakSanitizer on internal D-Bus
         * allocations that would be freed by dbus_shutdown(). */
        dbus_handle = NULL;
    }

    SDL_zero(dbus);
    if (inhibit_handle) {
        SDL_free(inhibit_handle);
        inhibit_handle = NULL;
    }

    SDL_SetInitialized(&dbus_init, false);
}

SDL_DBusContext *SDL_DBus_GetContext(void)
{
    if (!dbus_handle || !dbus.session_conn) {
        SDL_DBus_Init();
    }

    return (dbus_handle && dbus.session_conn) ? &dbus : NULL;
}

static bool SDL_DBus_CallMethodInternal(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, va_list ap)
{
    bool result = false;

    if (conn) {
        DBusMessage *msg = dbus.message_new_method_call(node, path, interface, method);
        if (msg) {
            int firstarg;
            va_list ap_reply;
            va_copy(ap_reply, ap); // copy the arg list so we don't compete with D-Bus for it
            firstarg = va_arg(ap, int);
            if ((firstarg == DBUS_TYPE_INVALID) || dbus.message_append_args_valist(msg, firstarg, ap)) {
                DBusMessage *reply = dbus.connection_send_with_reply_and_block(conn, msg, 300, NULL);
                if (reply) {
                    // skip any input args, get to output args.
                    while ((firstarg = va_arg(ap_reply, int)) != DBUS_TYPE_INVALID) {
                        // we assume D-Bus already validated all this.
                        {
                            void *dumpptr = va_arg(ap_reply, void *);
                            (void)dumpptr;
                        }
                        if (firstarg == DBUS_TYPE_ARRAY) {
                            {
                                const int dumpint = va_arg(ap_reply, int);
                                (void)dumpint;
                            }
                        }
                    }
                    firstarg = va_arg(ap_reply, int);
                    if ((firstarg == DBUS_TYPE_INVALID) || dbus.message_get_args_valist(reply, NULL, firstarg, ap_reply)) {
                        result = true;
                    }
                    dbus.message_unref(reply);
                }
            }
            va_end(ap_reply);
            dbus.message_unref(msg);
        }
    }

    return result;
}

bool SDL_DBus_CallMethodOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...)
{
    bool result;
    va_list ap;
    va_start(ap, method);
    result = SDL_DBus_CallMethodInternal(conn, node, path, interface, method, ap);
    va_end(ap);
    return result;
}

bool SDL_DBus_CallMethod(const char *node, const char *path, const char *interface, const char *method, ...)
{
    bool result;
    va_list ap;
    va_start(ap, method);
    result = SDL_DBus_CallMethodInternal(dbus.session_conn, node, path, interface, method, ap);
    va_end(ap);
    return result;
}

static bool SDL_DBus_CallVoidMethodInternal(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, va_list ap)
{
    bool result = false;

    if (conn) {
        DBusMessage *msg = dbus.message_new_method_call(node, path, interface, method);
        if (msg) {
            int firstarg = va_arg(ap, int);
            if ((firstarg == DBUS_TYPE_INVALID) || dbus.message_append_args_valist(msg, firstarg, ap)) {
                if (dbus.connection_send(conn, msg, NULL)) {
                    dbus.connection_flush(conn);
                    result = true;
                }
            }

            dbus.message_unref(msg);
        }
    }

    return result;
}

static bool SDL_DBus_CallWithBasicReply(DBusConnection *conn, DBusMessage *msg, const int expectedtype, void *result)
{
    bool retval = false;

    DBusMessage *reply = dbus.connection_send_with_reply_and_block(conn, msg, 300, NULL);
    if (reply) {
        DBusMessageIter iter, actual_iter;
        dbus.message_iter_init(reply, &iter);
        if (dbus.message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
            dbus.message_iter_recurse(&iter, &actual_iter);
        } else {
            actual_iter = iter;
        }

        if (dbus.message_iter_get_arg_type(&actual_iter) == expectedtype) {
            dbus.message_iter_get_basic(&actual_iter, result);
            retval = true;
        }

        dbus.message_unref(reply);
    }

    return retval;
}

bool SDL_DBus_CallVoidMethodOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *method, ...)
{
    bool result;
    va_list ap;
    va_start(ap, method);
    result = SDL_DBus_CallVoidMethodInternal(conn, node, path, interface, method, ap);
    va_end(ap);
    return result;
}

bool SDL_DBus_CallVoidMethod(const char *node, const char *path, const char *interface, const char *method, ...)
{
    bool result;
    va_list ap;
    va_start(ap, method);
    result = SDL_DBus_CallVoidMethodInternal(dbus.session_conn, node, path, interface, method, ap);
    va_end(ap);
    return result;
}

bool SDL_DBus_QueryPropertyOnConnection(DBusConnection *conn, const char *node, const char *path, const char *interface, const char *property, int expectedtype, void *result)
{
    bool retval = false;

    if (conn) {
        DBusMessage *msg = dbus.message_new_method_call(node, path, "org.freedesktop.DBus.Properties", "Get");
        if (msg) {
            if (dbus.message_append_args(msg, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &property, DBUS_TYPE_INVALID)) {
                retval = SDL_DBus_CallWithBasicReply(conn, msg, expectedtype, result);
            }
            dbus.message_unref(msg);
        }
    }

    return retval;
}

bool SDL_DBus_QueryProperty(const char *node, const char *path, const char *interface, const char *property, int expectedtype, void *result)
{
    return SDL_DBus_QueryPropertyOnConnection(dbus.session_conn, node, path, interface, property, expectedtype, result);
}

void SDL_DBus_ScreensaverTickle(void)
{
    if (screensaver_cookie == 0 && !inhibit_handle) { // no need to tickle if we're inhibiting.
        // org.gnome.ScreenSaver is the legacy interface, but it'll either do nothing or just be a second harmless tickle on newer systems, so we leave it for now.
        SDL_DBus_CallVoidMethod("org.gnome.ScreenSaver", "/org/gnome/ScreenSaver", "org.gnome.ScreenSaver", "SimulateUserActivity", DBUS_TYPE_INVALID);
        SDL_DBus_CallVoidMethod("org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", "SimulateUserActivity", DBUS_TYPE_INVALID);
    }
}

static bool SDL_DBus_AppendDictWithKeysAndValues(DBusMessageIter *iterInit, const char **keys, const char **values, int count)
{
    DBusMessageIter iterDict;

    if (!dbus.message_iter_open_container(iterInit, DBUS_TYPE_ARRAY, "{sv}", &iterDict)) {
        goto failed;
    }

    for (int i = 0; i < count; i++) {
        DBusMessageIter iterEntry, iterValue;
        const char *key = keys[i];
        const char *value = values[i];

        if (!dbus.message_iter_open_container(&iterDict, DBUS_TYPE_DICT_ENTRY, NULL, &iterEntry)) {
            goto failed;
        }

        if (!dbus.message_iter_append_basic(&iterEntry, DBUS_TYPE_STRING, &key)) {
            goto failed;
        }

        if (!dbus.message_iter_open_container(&iterEntry, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &iterValue)) {
            goto failed;
        }

        if (!dbus.message_iter_append_basic(&iterValue, DBUS_TYPE_STRING, &value)) {
            goto failed;
        }

        if (!dbus.message_iter_close_container(&iterEntry, &iterValue) || !dbus.message_iter_close_container(&iterDict, &iterEntry)) {
            goto failed;
        }
    }

    if (!dbus.message_iter_close_container(iterInit, &iterDict)) {
        goto failed;
    }

    return true;

failed:
    /* message_iter_abandon_container_if_open() and message_iter_abandon_container() might be
     * missing if libdbus is too old. Instead, we just return without cleaning up any eventual
     * open container */
    return false;
}

static bool SDL_DBus_AppendDictWithKeyValue(DBusMessageIter *iterInit, const char *key, const char *value)
{
    const char *keys[1];
    const char *values[1];

    keys[0] = key;
    values[0] = value;
    return SDL_DBus_AppendDictWithKeysAndValues(iterInit, keys, values, 1);
}

bool SDL_DBus_ScreensaverInhibit(bool inhibit)
{
    const char *default_inhibit_reason = "Playing a game";

    if ((inhibit && (screensaver_cookie != 0 || inhibit_handle)) || (!inhibit && (screensaver_cookie == 0 && !inhibit_handle))) {
        return true;
    }

    if (!dbus.session_conn) {
        /* We either lost connection to the session bus or were not able to
         * load the D-Bus library at all. */
        return false;
    }

    if (SDL_GetSandbox() != SDL_SANDBOX_NONE) {
        const char *bus_name = "org.freedesktop.portal.Desktop";
        const char *path = "/org/freedesktop/portal/desktop";
        const char *interface = "org.freedesktop.portal.Inhibit";
        const char *window = "";                    // As a future improvement we could gather the X11 XID or Wayland surface identifier
        static const unsigned int INHIBIT_IDLE = 8; // Taken from the portal API reference
        DBusMessageIter iterInit;

        if (inhibit) {
            DBusMessage *msg;
            bool result = false;
            const char *key = "reason";
            const char *reply = NULL;
            const char *reason = SDL_GetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME);
            if (!reason || !reason[0]) {
                reason = default_inhibit_reason;
            }

            msg = dbus.message_new_method_call(bus_name, path, interface, "Inhibit");
            if (!msg) {
                return false;
            }

            if (!dbus.message_append_args(msg, DBUS_TYPE_STRING, &window, DBUS_TYPE_UINT32, &INHIBIT_IDLE, DBUS_TYPE_INVALID)) {
                dbus.message_unref(msg);
                return false;
            }

            dbus.message_iter_init_append(msg, &iterInit);

            // a{sv}
            if (!SDL_DBus_AppendDictWithKeyValue(&iterInit, key, reason)) {
                dbus.message_unref(msg);
                return false;
            }

            if (SDL_DBus_CallWithBasicReply(dbus.session_conn, msg, DBUS_TYPE_OBJECT_PATH, &reply)) {
                inhibit_handle = SDL_strdup(reply);
                result = true;
            }

            dbus.message_unref(msg);
            return result;
        } else {
            if (!SDL_DBus_CallVoidMethod(bus_name, inhibit_handle, "org.freedesktop.portal.Request", "Close", DBUS_TYPE_INVALID)) {
                return false;
            }
            SDL_free(inhibit_handle);
            inhibit_handle = NULL;
        }
    } else {
        const char *bus_name = "org.freedesktop.ScreenSaver";
        const char *path = "/org/freedesktop/ScreenSaver";
        const char *interface = "org.freedesktop.ScreenSaver";

        if (inhibit) {
            const char *app = SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING);
            const char *reason = SDL_GetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME);
            if (!reason || !reason[0]) {
                reason = default_inhibit_reason;
            }

            if (!SDL_DBus_CallMethod(bus_name, path, interface, "Inhibit",
                                     DBUS_TYPE_STRING, &app, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID,
                                     DBUS_TYPE_UINT32, &screensaver_cookie, DBUS_TYPE_INVALID)) {
                return false;
            }
            return (screensaver_cookie != 0);
        } else {
            if (!SDL_DBus_CallVoidMethod(bus_name, path, interface, "UnInhibit", DBUS_TYPE_UINT32, &screensaver_cookie, DBUS_TYPE_INVALID)) {
                return false;
            }
            screensaver_cookie = 0;
        }
    }

    return true;
}

void SDL_DBus_PumpEvents(void)
{
    if (dbus.session_conn) {
        dbus.connection_read_write(dbus.session_conn, 0);

        while (dbus.connection_dispatch(dbus.session_conn) == DBUS_DISPATCH_DATA_REMAINS) {
            // Do nothing, actual work happens in DBus_MessageFilter
            SDL_DelayNS(SDL_US_TO_NS(10));
        }
    }
}

/*
 * Get the machine ID if possible. Result must be freed with dbus->free().
 */
char *SDL_DBus_GetLocalMachineId(void)
{
    DBusError err;
    char *result;

    dbus.error_init(&err);

    if (dbus.try_get_local_machine_id) {
        // Available since dbus 1.12.0, has proper error-handling
        result = dbus.try_get_local_machine_id(&err);
    } else {
        /* Available since time immemorial, but has no error-handling:
         * if the machine ID can't be read, many versions of libdbus will
         * treat that as a fatal mis-installation and abort() */
        result = dbus.get_local_machine_id();
    }

    if (result) {
        return result;
    }

    if (dbus.error_is_set(&err)) {
        SDL_SetError("%s: %s", err.name, err.message);
        dbus.error_free(&err);
    } else {
        SDL_SetError("Error getting D-Bus machine ID");
    }

    return NULL;
}

/*
 * Convert file drops with mime type "application/vnd.portal.filetransfer" to file paths
 * Result must be freed with dbus->free_string_array().
 * https://flatpak.github.io/xdg-desktop-portal/#gdbus-method-org-freedesktop-portal-FileTransfer.RetrieveFiles
 */
char **SDL_DBus_DocumentsPortalRetrieveFiles(const char *key, int *path_count)
{
    DBusError err;
    DBusMessageIter iter, iterDict;
    char **paths = NULL;
    DBusMessage *reply = NULL;
    DBusMessage *msg = dbus.message_new_method_call("org.freedesktop.portal.Documents",    // Node
                                                    "/org/freedesktop/portal/documents",   // Path
                                                    "org.freedesktop.portal.FileTransfer", // Interface
                                                    "RetrieveFiles");                      // Method

    // Make sure we have a connection to the dbus session bus
    if (!SDL_DBus_GetContext() || !dbus.session_conn) {
        /* We either cannot connect to the session bus or were unable to
         * load the D-Bus library at all. */
        return NULL;
    }

    dbus.error_init(&err);

    // First argument is a "application/vnd.portal.filetransfer" key from a DnD or clipboard event
    if (!dbus.message_append_args(msg, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID)) {
        SDL_OutOfMemory();
        dbus.message_unref(msg);
        goto failed;
    }

    /* Second argument is a variant dictionary for options.
     * The spec doesn't define any entries yet so it's empty. */
    dbus.message_iter_init_append(msg, &iter);
    if (!dbus.message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &iterDict) ||
        !dbus.message_iter_close_container(&iter, &iterDict)) {
        SDL_OutOfMemory();
        dbus.message_unref(msg);
        goto failed;
    }

    reply = dbus.connection_send_with_reply_and_block(dbus.session_conn, msg, DBUS_TIMEOUT_USE_DEFAULT, &err);
    dbus.message_unref(msg);

    if (reply) {
        dbus.message_get_args(reply, &err, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &paths, path_count, DBUS_TYPE_INVALID);
        dbus.message_unref(reply);
    }

    if (paths) {
        return paths;
    }

failed:
    if (dbus.error_is_set(&err)) {
        SDL_SetError("%s: %s", err.name, err.message);
        dbus.error_free(&err);
    } else {
        SDL_SetError("Error retrieving paths for documents portal \"%s\"", key);
    }

    return NULL;
}

typedef struct SDL_DBus_CameraPortalMessageHandlerData
{
    uint32_t response;
    char *path;
    DBusError *err;
    bool done;
} SDL_DBus_CameraPortalMessageHandlerData;

static DBusHandlerResult SDL_DBus_CameraPortalMessageHandler(DBusConnection *conn, DBusMessage *msg, void *v)
{
    SDL_DBus_CameraPortalMessageHandlerData *data = v;
    const char *name, *old, *new;

    if (dbus.message_is_signal(msg, "org.freedesktop.DBus", "NameOwnerChanged")) {
        if (!dbus.message_get_args(msg, data->err,
                                   DBUS_TYPE_STRING, &name,
                                   DBUS_TYPE_STRING, &old,
                                   DBUS_TYPE_STRING, &new,
                                   DBUS_TYPE_INVALID)) {
            data->done = true;
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        if (SDL_strcmp(name, "org.freedesktop.portal.Desktop") != 0 ||
            SDL_strcmp(new, "") != 0) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        data->done = true;
        data->response = -1;
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (!dbus.message_has_path(msg, data->path) || !dbus.message_is_signal(msg, "org.freedesktop.portal.Request", "Response")) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus.message_get_args(msg, data->err, DBUS_TYPE_UINT32, &data->response, DBUS_TYPE_INVALID);
    data->done = true;
    return DBUS_HANDLER_RESULT_HANDLED;
}

#define SIGNAL_NAMEOWNERCHANGED "type='signal',\
        sender='org.freedesktop.DBus',\
        interface='org.freedesktop.DBus',\
        member='NameOwnerChanged',\
        arg0='org.freedesktop.portal.Desktop',\
        arg2=''"

/*
 * Requests access for the camera. Returns -1 on error, -2 on denied access or
 * missing portal, otherwise returns a file descriptor to be used by the Pipewire driver.
 * https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Camera.html
 */
int SDL_DBus_CameraPortalRequestAccess(void)
{
    SDL_DBus_CameraPortalMessageHandlerData data;
    DBusError err;
    DBusMessageIter iter, iterDict;
    DBusMessage *reply, *msg;
    int fd;

    if (SDL_GetSandbox() == SDL_SANDBOX_NONE) {
        return -2;
    }

    if (!SDL_DBus_GetContext()) {
        return -2;
    }

    dbus.error_init(&err);

    msg = dbus.message_new_method_call("org.freedesktop.portal.Desktop",
                                       "/org/freedesktop/portal/desktop",
                                       "org.freedesktop.portal.Camera",
                                       "AccessCamera");

    dbus.message_iter_init_append(msg, &iter);
    if (!dbus.message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &iterDict) ||
        !dbus.message_iter_close_container(&iter, &iterDict)) {
        SDL_OutOfMemory();
        dbus.message_unref(msg);
        goto failed;
    }

    reply = dbus.connection_send_with_reply_and_block(dbus.session_conn, msg, DBUS_TIMEOUT_USE_DEFAULT, &err);
    dbus.message_unref(msg);

    if (reply) {
        dbus.message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &data.path, DBUS_TYPE_INVALID);
        if (dbus.error_is_set(&err)) {
            dbus.message_unref(reply);
            goto failed;
        }
        if ((data.path = SDL_strdup(data.path)) == NULL) {
            dbus.message_unref(reply);
            SDL_OutOfMemory();
            goto failed;
        }
        dbus.message_unref(reply);
    } else {
        if (dbus.error_has_name(&err, DBUS_ERROR_NAME_HAS_NO_OWNER)) {
            return -2;
        }
        goto failed;
    }

    dbus.bus_add_match(dbus.session_conn, SIGNAL_NAMEOWNERCHANGED, &err);
    if (dbus.error_is_set(&err)) {
        SDL_free(data.path);
        goto failed;
    }
    data.err = &err;
    data.done = false;
    if (!dbus.connection_add_filter(dbus.session_conn, SDL_DBus_CameraPortalMessageHandler, &data, NULL)) {
        SDL_free(data.path);
        SDL_OutOfMemory();
        goto failed;
    }
    while (!data.done && dbus.connection_read_write_dispatch(dbus.session_conn, -1)) {
        ;
    }

    dbus.bus_remove_match(dbus.session_conn, SIGNAL_NAMEOWNERCHANGED, &err);
    if (dbus.error_is_set(&err)) {
        SDL_free(data.path);
        goto failed;
    }
    dbus.connection_remove_filter(dbus.session_conn, SDL_DBus_CameraPortalMessageHandler, &data);
    SDL_free(data.path);
    if (!data.done) {
        goto failed;
    }
    if (dbus.error_is_set(&err)) { // from the message handler
        goto failed;
    }
    if (data.response == 1 || data.response == 2) {
        return -2;
    } else if (data.response != 0) {
        goto failed;
    }

    msg = dbus.message_new_method_call("org.freedesktop.portal.Desktop",
                                       "/org/freedesktop/portal/desktop",
                                       "org.freedesktop.portal.Camera",
                                       "OpenPipeWireRemote");

    dbus.message_iter_init_append(msg, &iter);
    if (!dbus.message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &iterDict) ||
        !dbus.message_iter_close_container(&iter, &iterDict)) {
        SDL_OutOfMemory();
        dbus.message_unref(msg);
        goto failed;
    }

    reply = dbus.connection_send_with_reply_and_block(dbus.session_conn, msg, DBUS_TIMEOUT_USE_DEFAULT, &err);
    dbus.message_unref(msg);

    if (reply) {
        dbus.message_get_args(reply, &err, DBUS_TYPE_UNIX_FD, &fd, DBUS_TYPE_INVALID);
        dbus.message_unref(reply);
        if (dbus.error_is_set(&err)) {
            goto failed;
        }
    } else {
        goto failed;
    }

    return fd;

failed:
    if (dbus.error_is_set(&err)) {
        if (dbus.error_has_name(&err, DBUS_ERROR_NO_MEMORY)) {
            SDL_OutOfMemory();
        }
        SDL_SetError("%s: %s", err.name, err.message);
        dbus.error_free(&err);
    } else {
        SDL_SetError("Error requesting access for the camera");
    }

    return -1;
}

static SDL_DBusMenuItem *GetMenuItemById(SDL_ListNode *menu, long id)
{
    SDL_ListNode *cursor;

    cursor = menu;
    while (cursor) {
        SDL_DBusMenuItem *item;

        item = cursor->entry;
        
        if (item->id == id) {
            return item;
        }

        if (item->sub_menu) {
            SDL_DBusMenuItem *found;

            found = GetMenuItemById(item->sub_menu, id);
            if (found) {
                return found;
            }
        }
        
        cursor = cursor->next;
    }
    return NULL;
}

static void AppendMenuItemProperties(SDL_DBusContext *ctx, SDL_DBusMenuItem *item, DBusMessageIter *dict_iter)
{
    DBusMessageIter entry_iter;
    DBusMessageIter variant_iter;
    const char *key;
    const char *value;
    dbus_bool_t value_bool;

    if (item->utf8) {
        key = "label";
        ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &item->utf8);
        ctx->message_iter_close_container(&entry_iter, &variant_iter);
        ctx->message_iter_close_container(dict_iter, &entry_iter);
    }

    key = "type";
    if (item->type == SDL_DBUS_MENU_ITEM_TYPE_SEPERATOR) {
        value = "separator";
    } else {
        value = "standard";
    }
    ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
    ctx->message_iter_close_container(&entry_iter, &variant_iter);
    ctx->message_iter_close_container(dict_iter, &entry_iter);

    key = "enabled";
    value_bool = !(item->flags & SDL_DBUS_MENU_ITEM_FLAGS_DISABLED);
    ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
    ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &value_bool);
    ctx->message_iter_close_container(&entry_iter, &variant_iter);
    ctx->message_iter_close_container(dict_iter, &entry_iter);

    key = "visible";
    value_bool = TRUE;
    ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
    ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &value_bool);
    ctx->message_iter_close_container(&entry_iter, &variant_iter);
    ctx->message_iter_close_container(dict_iter, &entry_iter);

    if (item->type == SDL_DBUS_MENU_ITEM_TYPE_CHECKBOX) {
        int value_int;

        key = "toggle-type";
        value = "checkmark";
        ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
        ctx->message_iter_close_container(&entry_iter, &variant_iter);
        ctx->message_iter_close_container(dict_iter, &entry_iter);

        key = "toggle-state";
        value_int = (item->flags & SDL_DBUS_MENU_ITEM_FLAGS_CHECKED) ? 1 : 0;
        ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &value_int);
        ctx->message_iter_close_container(&entry_iter, &variant_iter);
        ctx->message_iter_close_container(dict_iter, &entry_iter);
    }

    if (item->sub_menu) {
        key = "children-display";
        value = "submenu";
        ctx->message_iter_open_container(dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &value);
        ctx->message_iter_close_container(&entry_iter, &variant_iter);
        ctx->message_iter_close_container(dict_iter, &entry_iter);
    }
}

static void AppendMenuItem(SDL_DBusContext *ctx, SDL_DBusMenuItem *item, DBusMessageIter *array_iter, int depth)
{
    DBusMessageIter struct_iter, dict_iter, children_iter;

    ctx->message_iter_open_container(array_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
    ctx->message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &item->id);
    ctx->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
    AppendMenuItemProperties(ctx, item, &dict_iter);
    ctx->message_iter_close_container(&struct_iter, &dict_iter);
    ctx->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "v", &children_iter);
    if (item->sub_menu && depth > 0) {
        SDL_ListNode *cursor;

        cursor = item->sub_menu;
        while (cursor) {
            SDL_DBusMenuItem *child;
            DBusMessageIter variant_iter;

            child = cursor->entry;
            ctx->message_iter_open_container(&children_iter, DBUS_TYPE_VARIANT, "(ia{sv}av)", &variant_iter);
            AppendMenuItem(ctx, child, &variant_iter, depth - 1);
            ctx->message_iter_close_container(&children_iter, &variant_iter);
            cursor = cursor->next;
        }
    }
    ctx->message_iter_close_container(&struct_iter, &children_iter);
    ctx->message_iter_close_container(array_iter, &struct_iter);
}

static DBusHandlerResult HandleGetLayout(SDL_DBusContext *ctx, SDL_ListNode *menu, DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply;
    DBusMessageIter reply_iter, struct_iter, dict_iter, children_iter;
    DBusMessageIter entry_iter, variant_iter;
    DBusMessageIter args;
    const char *key;
    const char *val;
    long parent_id;
    long recursion_depth;
    long root_id;
    unsigned long revision;

    ctx->message_iter_init(msg, &args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_INT32) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    ctx->message_iter_get_basic(&args, &parent_id);
    ctx->message_iter_next(&args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_INT32) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    ctx->message_iter_get_basic(&args, &recursion_depth);
    if (recursion_depth == -1) {
        recursion_depth = 100;
    }

    reply = ctx->message_new_method_return(msg);
    ctx->message_iter_init_append(reply, &reply_iter);

    revision = 0;
    if (menu) {
        if (menu->entry) {
            revision = ((SDL_DBusMenuItem *)menu->entry)->revision;
        }
    }
    ctx->message_iter_append_basic(&reply_iter, DBUS_TYPE_UINT32, &revision);

    ctx->message_iter_open_container(&reply_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);

    root_id = 0;
    ctx->message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &root_id);

    key = "children-display";
    val = "submenu";
    ctx->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
    ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
    ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
    ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
    ctx->message_iter_close_container(&entry_iter, &variant_iter);
    ctx->message_iter_close_container(&dict_iter, &entry_iter);
    ctx->message_iter_close_container(&struct_iter, &dict_iter);

    ctx->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "v", &children_iter);
    if (!parent_id && menu) {
        SDL_ListNode *cursor;

        cursor = menu;
        while (cursor) {
            SDL_DBusMenuItem *item;
            DBusMessageIter cvariant_iter, item_struct, item_dict, item_children;

            item = cursor->entry;
            ctx->message_iter_open_container(&children_iter, DBUS_TYPE_VARIANT, "(ia{sv}av)", &cvariant_iter);
            ctx->message_iter_open_container(&cvariant_iter, DBUS_TYPE_STRUCT, NULL, &item_struct);
            ctx->message_iter_append_basic(&item_struct, DBUS_TYPE_INT32, &item->id);
            ctx->message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "{sv}", &item_dict);
            AppendMenuItemProperties(ctx, item, &item_dict);
            ctx->message_iter_close_container(&item_struct, &item_dict);
            ctx->message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "v", &item_children);
            if (item->sub_menu && recursion_depth) {
                SDL_ListNode *child_cursor;

                child_cursor = item->sub_menu;
                while (child_cursor) {
                    SDL_DBusMenuItem *child;
                    DBusMessageIter child_variant;

                    child = child_cursor->entry;
                    ctx->message_iter_open_container(&item_children, DBUS_TYPE_VARIANT, "(ia{sv}av)", &child_variant);
                    AppendMenuItem(ctx, child, &child_variant, recursion_depth - 1);
                    ctx->message_iter_close_container(&item_children, &child_variant);
                    child_cursor = child_cursor->next;
                }
            }
            ctx->message_iter_close_container(&item_struct, &item_children);
            ctx->message_iter_close_container(&cvariant_iter, &item_struct);

            ctx->message_iter_close_container(&children_iter, &cvariant_iter);
            cursor = cursor->next;
        }
    } else if (parent_id) {
        SDL_DBusMenuItem *parent;

        parent = GetMenuItemById(menu, parent_id);
        if (parent && parent->sub_menu) {
            SDL_ListNode *cursor;

            cursor = parent->sub_menu;
            while (cursor) {
                SDL_DBusMenuItem *item;
                DBusMessageIter cvariant_iter, item_struct, item_dict, item_children;

                item = cursor->entry;
                ctx->message_iter_open_container(&children_iter, DBUS_TYPE_VARIANT, "(ia{sv}av)", &cvariant_iter);
                ctx->message_iter_open_container(&cvariant_iter, DBUS_TYPE_STRUCT, NULL, &item_struct);
                ctx->message_iter_append_basic(&item_struct, DBUS_TYPE_INT32, &item->id);
                ctx->message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "{sv}", &item_dict);
                AppendMenuItemProperties(ctx, item, &item_dict);
                ctx->message_iter_close_container(&item_struct, &item_dict);
                ctx->message_iter_open_container(&item_struct, DBUS_TYPE_ARRAY, "v", &item_children);
                if (item->sub_menu && recursion_depth) {
                    SDL_ListNode *child_cursor;

                    child_cursor = item->sub_menu;
                    while (child_cursor) {
                        SDL_DBusMenuItem *child;
                        DBusMessageIter child_variant;

                        child = child_cursor->entry;
                        ctx->message_iter_open_container(&item_children, DBUS_TYPE_VARIANT, "(ia{sv}av)", &child_variant);
                        AppendMenuItem(ctx, child, &child_variant, recursion_depth - 1);
                        ctx->message_iter_close_container(&item_children, &child_variant);
                        child_cursor = child_cursor->next;
                    }
                }
                ctx->message_iter_close_container(&item_struct, &item_children);
                ctx->message_iter_close_container(&cvariant_iter, &item_struct);

                ctx->message_iter_close_container(&children_iter, &cvariant_iter);

                cursor = cursor->next;
            }
        }
    }
    ctx->message_iter_close_container(&struct_iter, &children_iter);
    ctx->message_iter_close_container(&reply_iter, &struct_iter);

    ctx->connection_send(conn, reply, NULL);
    ctx->message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult HandleEvent(SDL_DBusContext *ctx, SDL_ListNode *menu, DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply;
    const char *event_id;
    SDL_DBusMenuItem *item;
    DBusMessageIter args;
    Uint32 id;

    ctx->message_iter_init(msg, &args);
    ctx->message_iter_get_basic(&args, &id);
    ctx->message_iter_next(&args);
    ctx->message_iter_get_basic(&args, &event_id);
    
    item = NULL;
    if (!SDL_strcmp(event_id, "clicked")) {
        item = GetMenuItemById(menu, id);
    }

    reply = ctx->message_new_method_return(msg);
    ctx->connection_send(conn, reply, NULL);
    ctx->message_unref(reply);
        
    if (item) {
        if (item->cb) {
            item->cb(item, item->cb_data);
        }
    }
    
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult HandleGetProperty(SDL_DBusContext *ctx, SDL_ListNode *menu, DBusConnection *conn, DBusMessage *msg)
{
    DBusMessage *reply;
    SDL_DBusMenuItem *item;
    const char *property;
    const char *val;
    DBusMessageIter args;
    DBusMessageIter iter, variant_iter;
    long id;
    int int_val;
    dbus_bool_t bool_val;

    ctx->message_iter_init(msg, &args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_INT32) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    ctx->message_iter_get_basic(&args, &id);
    ctx->message_iter_next(&args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    ctx->message_iter_get_basic(&args, &property);

    item = GetMenuItemById(menu, id);
    if (!item) {
        DBusMessage *error;

        error = ctx->message_new_error(msg, "com.canonical.dbusmenu.Error", "Item not found");
        ctx->connection_send(conn, error, NULL);
        ctx->message_unref(error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    reply = ctx->message_new_method_return(msg);
    ctx->message_iter_init_append(reply, &iter);
    if (!SDL_strcmp(property, "label")) {
        val = item->utf8 ? item->utf8 : "";
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "enabled")) {
        bool_val = !(item->flags & SDL_DBUS_MENU_ITEM_FLAGS_DISABLED);
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "visible")) {
        bool_val = 1;
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "type")) {
        if (item->type == SDL_DBUS_MENU_ITEM_TYPE_SEPERATOR) {
            val = "separator";
        } else {
            val = "standard";
        }
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "toggle-state")) {
        int_val = (item->flags & SDL_DBUS_MENU_ITEM_FLAGS_CHECKED) ? 1 : 0;
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "i", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &int_val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else if (!SDL_strcmp(property, "children-display")) {
        val = item->sub_menu ? "submenu" : "";
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    } else {
        val = "";
        ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
        ctx->message_iter_close_container(&iter, &variant_iter);
    }

    ctx->connection_send(conn, reply, NULL);
    ctx->message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult HandleGetGroupProperties(SDL_DBusContext *ctx, SDL_ListNode *menu, DBusConnection *conn, DBusMessage *msg)
{
    #define FILTER_PROPS_SZ 32
    DBusMessage *reply;
    DBusMessageIter args, array_iter, prop_iter;
    DBusMessageIter iter, reply_array_iter;
    const char *filter_props[FILTER_PROPS_SZ];
    long *ids;
    int ids_sz;
    int filter_sz;
    int i;
    int j;

    ids_sz = 0;
    ctx->message_iter_init(msg, &args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    ctx->message_iter_recurse(&args, &array_iter);
    if (ctx->message_iter_get_arg_type(&array_iter) == DBUS_TYPE_INT32) {
        ctx->message_iter_get_fixed_array(&array_iter, &ids, &ids_sz);
    }

    ctx->message_iter_next(&args);
    if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_ARRAY) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    ctx->message_iter_recurse(&args, &prop_iter);
    filter_sz = 0;
    while (ctx->message_iter_get_arg_type(&prop_iter) == DBUS_TYPE_STRING) {
        if (filter_sz < FILTER_PROPS_SZ) {
            ctx->message_iter_get_basic(&prop_iter, &filter_props[filter_sz]);
            filter_sz++;
        }
        ctx->message_iter_next(&prop_iter);
    }

    reply = ctx->message_new_method_return(msg);
    ctx->message_iter_init_append(reply, &iter);
    ctx->message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(ia{sv})", &reply_array_iter);

    for (i = 0; i < ids_sz; i++) {
        SDL_DBusMenuItem *item;

        item = GetMenuItemById(menu, ids[i]);
        if (item) {
            DBusMessageIter struct_iter, dict_iter;

            ctx->message_iter_open_container(&reply_array_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter);
            ctx->message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &ids[i]);
            ctx->message_iter_open_container(&struct_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

            if (filter_sz == 0) {
                AppendMenuItemProperties(ctx, item, &dict_iter);
            } else {
                for (j = 0; j < filter_sz; j++) {
                    DBusMessageIter entry_iter, variant_iter;
                    const char *prop;
                    const char *val;
                    int int_val;
                    dbus_bool_t bool_val;

                    prop = filter_props[j];
                    if (!SDL_strcmp(prop, "label") && item->utf8) {
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &item->utf8);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "type")) {
                        val = (item->type == SDL_DBUS_MENU_ITEM_TYPE_SEPERATOR) ? "separator" : "standard";
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "enabled")) {
                        bool_val = !(item->flags & SDL_DBUS_MENU_ITEM_FLAGS_DISABLED);
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "visible")) {
                        bool_val = 1;
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "toggle-type") && item->type == SDL_DBUS_MENU_ITEM_TYPE_CHECKBOX) {
                        val = "checkmark";
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "toggle-state")) {
                        int_val = (item->flags & SDL_DBUS_MENU_ITEM_FLAGS_CHECKED) ? 1 : 0;
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "i", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &int_val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    } else if (!SDL_strcmp(prop, "children-display") && item->sub_menu) {
                        val = "submenu";
                        ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
                        ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &prop);
                        ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
                        ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &val);
                        ctx->message_iter_close_container(&entry_iter, &variant_iter);
                        ctx->message_iter_close_container(&dict_iter, &entry_iter);
                    }
                }
            }

            ctx->message_iter_close_container(&struct_iter, &dict_iter);
            ctx->message_iter_close_container(&reply_array_iter, &struct_iter);
        }
    }

    ctx->message_iter_close_container(&iter, &reply_array_iter);
    ctx->connection_send(conn, reply, NULL);
    ctx->message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static long GetMaxMenuItemId(SDL_ListNode *menu)
{
    SDL_ListNode *cursor;
    long max_id;

    max_id = 0;
    cursor = menu;
    while (cursor) {
        SDL_DBusMenuItem *item;

        item = cursor->entry;
        if (item) {
            if (item->id > max_id) {
                max_id = item->id;
            }

            if (item->sub_menu) {
                long sub_max;

                sub_max = GetMaxMenuItemId(item->sub_menu);
                if (sub_max > max_id) {
                    max_id = sub_max;
                }
            }
        }
        cursor = cursor->next;
    }
    return max_id;
}

static void AssignMenuItemIds(SDL_ListNode *menu, long *next_id)
{
    SDL_ListNode *cursor;

    cursor = menu;
    while (cursor) {
        SDL_DBusMenuItem *item;

        item = cursor->entry;
        if (item) {
            if (!item->id) {
                item->id = (*next_id)++;
            }
            if (item->sub_menu) {
                AssignMenuItemIds(item->sub_menu, next_id);
            }
        }
        cursor = cursor->next;
    }
}

void SDL_DBus_InitMenuItemInternals(SDL_DBusMenuItem *item)
{
    item->id = 0;
}

static DBusHandlerResult DBusMenuMessageFilter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    SDL_ListNode *menu;
    SDL_DBusContext *ctx;

    menu = user_data;
    if (!menu || !menu->entry) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    ctx = ((SDL_DBusMenuItem *)menu->entry)->dbus;
    if (ctx->message_is_method_call(msg, DBUS_MENU_INTERFACE, "GetLayout")) {
        return HandleGetLayout(ctx, menu, conn, msg);
    } else if (ctx->message_is_method_call(msg, DBUS_MENU_INTERFACE, "Event")) {
        return HandleEvent(ctx, menu, conn, msg);
    } else if (ctx->message_is_method_call(msg, DBUS_MENU_INTERFACE, "AboutToShow")) {
        DBusMessage *reply;
        dbus_bool_t need_update;

        need_update = TRUE;
        reply = ctx->message_new_method_return(msg);
        ctx->message_append_args(reply, DBUS_TYPE_BOOLEAN, &need_update, DBUS_TYPE_INVALID);
        ctx->connection_send(conn, reply, NULL);
        ctx->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (ctx->message_is_method_call(msg, DBUS_MENU_INTERFACE, "GetGroupProperties")) {
        return HandleGetGroupProperties(ctx, menu, conn, msg);
    } else if (ctx->message_is_method_call(msg, DBUS_MENU_INTERFACE, "GetProperty")) {
        return HandleGetProperty(ctx, menu, conn, msg);
    } else if (ctx->message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get")) {
        DBusMessage *reply;
        const char *interface_name;
        const char *property_name;
        DBusMessageIter args, iter, variant_iter;
        dbus_uint32_t version;

        ctx->message_iter_init(msg, &args);
        if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        ctx->message_iter_get_basic(&args, &interface_name);
        ctx->message_iter_next(&args);
        if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        ctx->message_iter_get_basic(&args, &property_name);

        if (!SDL_strcmp(interface_name, DBUS_MENU_INTERFACE) && !SDL_strcmp(property_name, "Version")) {
            reply = ctx->message_new_method_return(msg);
            ctx->message_iter_init_append(reply, &iter);
            version = 3;
            ctx->message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &variant_iter);
            ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32, &version);
            ctx->message_iter_close_container(&iter, &variant_iter);
            ctx->connection_send(conn, reply, NULL);
            ctx->message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    } else if (ctx->message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll")) {
        DBusMessage *reply;
        const char *interface_name;
        const char *key;
        DBusMessageIter args, iter, dict_iter, entry_iter, variant_iter;
        dbus_uint32_t version;

        ctx->message_iter_init(msg, &args);
        if (ctx->message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        ctx->message_iter_get_basic(&args, &interface_name);

        if (!SDL_strcmp(interface_name, DBUS_MENU_INTERFACE)) {
            reply = ctx->message_new_method_return(msg);
            ctx->message_iter_init_append(reply, &iter);
            ctx->message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

            key = "Version";
            version = 3;
            ctx->message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry_iter);
            ctx->message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key);
            ctx->message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "u", &variant_iter);
            ctx->message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT32, &version);
            ctx->message_iter_close_container(&entry_iter, &variant_iter);
            ctx->message_iter_close_container(&dict_iter, &entry_iter);

            ctx->message_iter_close_container(&iter, &dict_iter);
            ctx->connection_send(conn, reply, NULL);
            ctx->message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
    } else if (ctx->message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        DBusMessage *reply;
        
        reply = ctx->message_new_method_return(msg);
        ctx->message_append_args(reply, DBUS_TYPE_STRING, &menu_introspect, DBUS_TYPE_INVALID);
        ctx->connection_send(conn, reply, NULL);
        ctx->message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

const char *SDL_DBus_ExportMenu(SDL_DBusContext *ctx, DBusConnection *conn, SDL_ListNode *menu)
{
    DBusObjectPathVTable vtable;
    long next_id;

    if (!ctx || !menu) {
        return NULL;
    }

    next_id = 1;
    AssignMenuItemIds(menu, &next_id);

    if (menu->entry) {
        SDL_DBusMenuItem *item;

        item = menu->entry;
        item->dbus = ctx;
        item->revision++;
    }

    vtable.message_function = DBusMenuMessageFilter;
    vtable.unregister_function = NULL;
    if (!ctx->connection_try_register_object_path(conn, DBUS_MENU_OBJECT_PATH, &vtable, menu, NULL)) {
        return NULL;
    }

    return DBUS_MENU_OBJECT_PATH;
}

void SDL_DBus_UpdateMenu(SDL_DBusContext *ctx, DBusConnection *conn, SDL_ListNode *menu)
{
    DBusMessage *signal;
    unsigned long revision;
    long next_id;

    if (!ctx || !menu) {
        return;
    }

    next_id = GetMaxMenuItemId(menu) + 1;
    AssignMenuItemIds(menu, &next_id);

    revision = 0;
    if (menu->entry) {
        SDL_DBusMenuItem *item;

        item = menu->entry;
        item->revision++;
        revision = item->revision;
    }

    signal = ctx->message_new_signal(DBUS_MENU_OBJECT_PATH, DBUS_MENU_INTERFACE, "LayoutUpdated");
    if (signal) {
        long parent;

        parent = 0;
        ctx->message_append_args(signal, DBUS_TYPE_UINT32, &revision, DBUS_TYPE_INT32, &parent, DBUS_TYPE_INVALID);
        ctx->connection_send(conn, signal, NULL);
        ctx->message_unref(signal);
    }
}

#endif
