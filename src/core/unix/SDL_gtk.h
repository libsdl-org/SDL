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
#include "SDL_glib.h"

#ifndef SDL_gtk_h_
#define SDL_gtk_h_

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
	SDL_GlibContext g;
	
	SDL_gboolean (*init_check)(int *argc, char ***argv);
	GtkWidget *(*menu_new)(void);
	GtkWidget *(*separator_menu_item_new)(void);
	GtkWidget *(*menu_item_new_with_label)(const SDL_gchar *label);
	void (*menu_item_set_submenu)(GtkMenuItem *menu_item, GtkWidget *submenu);
	GtkWidget *(*check_menu_item_new_with_label)(const SDL_gchar *label);
	void (*check_menu_item_set_active)(GtkCheckMenuItem *check_menu_item, SDL_gboolean is_active);
	void (*widget_set_sensitive)(GtkWidget *widget, SDL_gboolean sensitive);
	void (*widget_show)(GtkWidget *widget);
	void (*menu_shell_append)(GtkMenuShell *menu_shell, GtkWidget *child);
	void (*menu_shell_insert)(GtkMenuShell *menu_shell, GtkWidget *child, SDL_gint position);
	void (*widget_destroy)(GtkWidget *widget);
	const SDL_gchar *(*menu_item_get_label)(GtkMenuItem *menu_item);
	void (*menu_item_set_label)(GtkMenuItem *menu_item, const SDL_gchar *label);
	SDL_gboolean (*check_menu_item_get_active)(GtkCheckMenuItem *check_menu_item);
	SDL_gboolean (*widget_get_sensitive)(GtkWidget *widget);
	GtkSettings *(*settings_get_default)(void);
} SDL_GtkContext;

extern bool SDL_Gtk_Init(void);
extern void SDL_Gtk_Quit(void);
extern SDL_GtkContext *SDL_Gtk_EnterContext(void);
extern void SDL_Gtk_ExitContext(SDL_GtkContext *gtk);
extern void SDL_UpdateGtk(void);

#endif // SDL_gtk_h_
