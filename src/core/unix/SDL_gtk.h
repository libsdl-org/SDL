/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_gtk_h_
#define SDL_gtk_h_

/* Glib 2.0 */

typedef unsigned long gulong;
typedef void *gpointer;
typedef char gchar;
typedef int gint;
typedef unsigned int guint;
typedef double gdouble;
typedef gint gboolean;
typedef void (*GCallback)(void);
typedef struct _GClosure GClosure;
typedef void (*GClosureNotify) (gpointer data, GClosure *closure);
typedef gboolean (*GSourceFunc) (gpointer user_data);

typedef struct _GParamSpec GParamSpec;
typedef struct _GMainContext GMainContext;

typedef enum SDL_GConnectFlags
{
    SDL_G_CONNECT_DEFAULT = 0,
    SDL_G_CONNECT_AFTER = 1 << 0,
    SDL_G_CONNECT_SWAPPED = 1 << 1
} SDL_GConnectFlags;

#define SDL_G_CALLBACK(f) ((GCallback) (f))
#define SDL_G_TYPE_CIC(ip, gt, ct) ((ct*) ip)
#define SDL_G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type) (SDL_G_TYPE_CIC ((instance), (g_type), c_type))

#define GTK_FALSE 0
#define GTK_TRUE 1


/* GTK 3.0 */

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkMenuShell GtkMenuShell;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;
typedef struct _GtkSettings GtkSettings;

#define GTK_MENU_ITEM(obj) (SDL_G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_ITEM, GtkMenuItem))
#define GTK_WIDGET(widget) (SDL_G_TYPE_CHECK_INSTANCE_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_CHECK_MENU_ITEM(obj) (SDL_G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItem))
#define GTK_MENU(obj) (SDL_G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))


typedef struct SDL_GtkContext
{
	/* Glib 2.0 */
	struct
	{
		gulong (*signal_connect)(gpointer instance, const gchar *detailed_signal, void *c_handler, gpointer data);
		gulong (*signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, SDL_GConnectFlags connect_flags);
		void (*object_unref)(gpointer object);
		gchar *(*mkdtemp)(gchar *template);
		gchar *(*get_user_cache_dir)(void);
		gpointer (*object_ref_sink)(gpointer object);
		gpointer (*object_ref)(gpointer object);
		void (*object_get)(gpointer object, const gchar *first_property_name, ...);
		void (*signal_handler_disconnect)(gpointer instance, gulong handler_id);
		void (*main_context_push_thread_default)(GMainContext *context);
		void (*main_context_pop_thread_default)(GMainContext *context);
		GMainContext *(*main_context_new)(void);
		void (*main_context_unref)(GMainContext *context);
		gboolean (*main_context_acquire)(GMainContext *context);
		gboolean (*main_context_iteration)(GMainContext *context, gboolean may_block);
	} g;

	/* GTK 3.0 */
	struct
	{
		gboolean (*init_check)(int *argc, char ***argv);
		GtkWidget *(*menu_new)(void);
		GtkWidget *(*separator_menu_item_new)(void);
		GtkWidget *(*menu_item_new_with_label)(const gchar *label);
		void (*menu_item_set_submenu)(GtkMenuItem *menu_item, GtkWidget *submenu);
		GtkWidget *(*check_menu_item_new_with_label)(const gchar *label);
		void (*check_menu_item_set_active)(GtkCheckMenuItem *check_menu_item, gboolean is_active);
		void (*widget_set_sensitive)(GtkWidget *widget, gboolean sensitive);
		void (*widget_show)(GtkWidget *widget);
		void (*menu_shell_append)(GtkMenuShell *menu_shell, GtkWidget *child);
		void (*menu_shell_insert)(GtkMenuShell *menu_shell, GtkWidget *child, gint position);
		void (*widget_destroy)(GtkWidget *widget);
		const gchar *(*menu_item_get_label)(GtkMenuItem *menu_item);
		void (*menu_item_set_label)(GtkMenuItem *menu_item, const gchar *label);
		gboolean (*check_menu_item_get_active)(GtkCheckMenuItem *check_menu_item);
		gboolean (*widget_get_sensitive)(GtkWidget *widget);
		GtkSettings *(*settings_get_default)(void);
	} gtk;
} SDL_GtkContext;

extern bool SDL_CanUseGtk(void);
extern bool SDL_Gtk_Init(void);
extern void SDL_Gtk_Quit(void);
extern SDL_GtkContext *SDL_Gtk_EnterContext(void);
extern void SDL_Gtk_ExitContext(SDL_GtkContext *ctx);
extern void SDL_UpdateGtk(void);

#endif // SDL_gtk_h_
