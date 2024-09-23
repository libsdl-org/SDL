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

#include <dlfcn.h>

#define TRAY_APPINDICATOR_ID "tray-id"

#ifdef APPINDICATOR_HEADER
#include APPINDICATOR_HEADER
#else
/* ------------------------------------------------------------------------- */
/*                     BEGIN THIRD-PARTY HEADER CONTENT                      */
/* ------------------------------------------------------------------------- */
// Glib 2.0

typedef unsigned long gulong;
typedef void* gpointer;
typedef char gchar;
typedef int gint;
typedef unsigned int guint;
typedef gint gboolean;
typedef void (*GCallback)(void);
typedef struct _GClosure GClosure;
typedef void (*GClosureNotify) (gpointer data, GClosure *closure);
typedef gboolean (*GSourceFunc) (gpointer user_data);
typedef enum
{
    G_CONNECT_AFTER = 1 << 0,
    G_CONNECT_SWAPPED = 1 << 1
} GConnectFlags;
gulong (*g_signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);

#define g_signal_connect(instance, detailed_signal, c_handler, data) \
    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, (GConnectFlags) 0)

#define _G_TYPE_CIC(ip, gt, ct) ((ct*) ip)

#define G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type) (_G_TYPE_CIC ((instance), (g_type), c_type))

#define G_CALLBACK(f) ((GCallback) (f))

#define FALSE 0
#define TRUE 1

// GTK 3.0

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkMenuShell GtkMenuShell;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;

gboolean (*gtk_init_check)(int *argc, char ***argv);
void (*gtk_main)(void);

GtkWidget* (*gtk_menu_new)(void);
GtkWidget* (*gtk_separator_menu_item_new)(void);
GtkWidget* (*gtk_menu_item_new_with_label)(const gchar *label);
void (*gtk_menu_item_set_submenu)(GtkMenuItem *menu_item, GtkWidget *submenu);
GtkWidget* (*gtk_check_menu_item_new_with_label)(const gchar *label);
void (*gtk_check_menu_item_set_active)(GtkCheckMenuItem *check_menu_item, gboolean is_active);
void (*gtk_widget_set_sensitive)(GtkWidget *widget, gboolean sensitive);
void (*gtk_widget_show)(GtkWidget *widget);
void (*gtk_menu_shell_append)(GtkMenuShell *menu_shell, GtkWidget *child);

#define GTK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_ITEM, GtkMenuItem))
#define GTK_WIDGET(widget) (G_TYPE_CHECK_INSTANCE_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_CHECK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItem))
#define GTK_MENU(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))

// AppIndicator

typedef enum {
    APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
    APP_INDICATOR_CATEGORY_COMMUNICATIONS,
    APP_INDICATOR_CATEGORY_SYSTEM_SERVICES,
    APP_INDICATOR_CATEGORY_HARDWARE,
    APP_INDICATOR_CATEGORY_OTHER
} AppIndicatorCategory;

typedef enum {
    APP_INDICATOR_STATUS_PASSIVE,
    APP_INDICATOR_STATUS_ACTIVE,
    APP_INDICATOR_STATUS_ATTENTION
} AppIndicatorStatus;

typedef struct _AppIndicator AppIndicator;
AppIndicator *(*app_indicator_new)(const gchar *id, const gchar *icon_name, AppIndicatorCategory category);
void (*app_indicator_set_status)(AppIndicator *self, AppIndicatorStatus status);
void (*app_indicator_set_icon)(AppIndicator *self, const gchar *icon_name);
void (*app_indicator_set_menu)(AppIndicator *self, GtkMenu *menu);
/* ------------------------------------------------------------------------- */
/*                      END THIRD-PARTY HEADER CONTENT                       */
/* ------------------------------------------------------------------------- */
#endif

static bool gtk_is_init = false;

static int main_gtk_thread(void *data)
{
    gtk_main();
    return 0;
}

static void *libappindicator = NULL;
static void *libgtk = NULL;
static void *libgdk = NULL;

static void quit_gtk(void)
{
    if (libappindicator) {
        dlclose(libappindicator);
        libappindicator = NULL;
    }

    if (libgtk) {
        dlclose(libgtk);
        libgtk = NULL;
    }

    if (libgdk) {
        dlclose(libgdk);
        libgdk = NULL;
    }

    gtk_is_init = false;
}

static bool init_gtk(void)
{
    if (gtk_is_init) {
        return true;
    }

    libappindicator = dlopen("libayatana-appindicator3.so", RTLD_LAZY);
    libgtk = dlopen("libgtk-3.so", RTLD_LAZY);
    libgdk = dlopen("libgdk-3.so", RTLD_LAZY);

    if (!libappindicator || !libgtk || !libgdk) {
        quit_gtk();
        return SDL_SetError("Could not load GTK/AppIndicator libraries");
    }

    gtk_init_check = dlsym(libgtk, "gtk_init_check");
    gtk_main = dlsym(libgtk, "gtk_main");
    gtk_menu_new = dlsym(libgtk, "gtk_menu_new");
    gtk_separator_menu_item_new = dlsym(libgtk, "gtk_separator_menu_item_new");
    gtk_menu_item_new_with_label = dlsym(libgtk, "gtk_menu_item_new_with_label");
    gtk_menu_item_set_submenu = dlsym(libgtk, "gtk_menu_item_set_submenu");
    gtk_check_menu_item_new_with_label = dlsym(libgtk, "gtk_check_menu_item_new_with_label");
    gtk_check_menu_item_set_active = dlsym(libgtk, "gtk_check_menu_item_set_active");
    gtk_widget_set_sensitive = dlsym(libgtk, "gtk_widget_set_sensitive");
    gtk_widget_show = dlsym(libgtk, "gtk_widget_show");
    gtk_menu_shell_append = dlsym(libgtk, "gtk_menu_shell_append");

    g_signal_connect_data = dlsym(libgdk, "g_signal_connect_data");

    app_indicator_new = dlsym(libappindicator, "app_indicator_new");
    app_indicator_set_status = dlsym(libappindicator, "app_indicator_set_status");
    app_indicator_set_icon = dlsym(libappindicator, "app_indicator_set_icon");
    app_indicator_set_menu = dlsym(libappindicator, "app_indicator_set_menu");

    if (!gtk_init_check ||
        !gtk_main ||
        !gtk_menu_new ||
        !gtk_separator_menu_item_new ||
        !gtk_menu_item_new_with_label ||
        !gtk_menu_item_set_submenu ||
        !gtk_check_menu_item_new_with_label ||
        !gtk_check_menu_item_set_active ||
        !gtk_widget_set_sensitive ||
        !gtk_widget_show ||
        !gtk_menu_shell_append ||
        !g_signal_connect_data ||
        !app_indicator_new ||
        !app_indicator_set_status ||
        !app_indicator_set_icon ||
        !app_indicator_set_menu) {
        quit_gtk();
        return SDL_SetError("Could not load GTK/AppIndicator functions");
    }

    if (gtk_init_check(0, NULL) == FALSE) {
        quit_gtk();
        return SDL_SetError("Could not init GTK");
    }

    gtk_is_init = true;

    SDL_DetachThread(SDL_CreateThread(main_gtk_thread, "tray gtk", NULL));

    return true;
}

struct SDL_TrayMenu {
    SDL_Tray *tray;
    GtkMenuShell *menu;
};

struct SDL_TrayEntry {
    SDL_Tray *tray;
    SDL_TrayMenu *menu;
    GtkWidget *item;

    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu submenu;
};

struct SDL_Tray {
    AppIndicator *indicator;
    SDL_TrayMenu menu;

    size_t nEntries;
    SDL_TrayEntry **entries;
};

static void call_callback(GtkMenuItem *item, gpointer ptr)
{
    SDL_TrayEntry *entry = ptr;
    if (entry->callback) {
        entry->callback(entry->userdata, entry);
    }
}

/* FIXME: AppIndicator requires dealing in filenames, which are inherently
 * subject to timing attacks.
 *
 * Can something be done by fetching a file descriptor from mkstemps() and
 * using a path like "/dev/fd/123"?
 */
static bool get_tmp_filename(char *buffer, size_t size)
{
    static int count = 0;

    if (size < 64) {
        return SDL_SetError("Can't create temporary file for icon: size %ld < 64", size);
    }

    int would_have_written = SDL_snprintf(buffer, size, "/tmp/sdl_appindicator_icon_%d.bmp", count++);

    return would_have_written > 0 && would_have_written < size - 1;
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    if (init_gtk() != true) {
        return NULL;
    }

    SDL_Tray *tray = (SDL_Tray *) SDL_malloc(sizeof(SDL_Tray));

    if (!tray) {
        return NULL;
    }

    SDL_memset((void *) tray, 0, sizeof(*tray));

    char icon_path[256];
    get_tmp_filename(icon_path, sizeof(icon_path));
    SDL_SaveBMP(icon, icon_path);

    tray->indicator = app_indicator_new(TRAY_APPINDICATOR_ID, icon_path,
                                        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    SDL_RemovePath(icon_path);

    app_indicator_set_status(tray->indicator, APP_INDICATOR_STATUS_ACTIVE);

    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (icon) {
        char icon_path[256];
        get_tmp_filename(icon_path, sizeof(icon_path));
        SDL_SaveBMP(icon, tray->icon_path);
        app_indicator_set_icon(tray->indicator, tray->icon_path);
        SDL_RemovePath(icon_path);
    } else {
        app_indicator_set_icon(tray->indicator, NULL);
    }
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    /* AppIndicator provides no tooltip support. */
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    tray->menu.menu = (GtkMenuShell *)gtk_menu_new();
    tray->menu.tray = tray;

    app_indicator_set_menu(tray->indicator, GTK_MENU(tray->menu.menu));

    return &tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    entry->submenu.tray = entry->tray;
    entry->submenu.menu = (GtkMenuShell *)gtk_menu_new();

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry->item), GTK_WIDGET(entry->submenu.menu));

    return &entry->submenu;
}

SDL_TrayEntry *SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_TrayEntry *entry = SDL_malloc(sizeof(SDL_TrayEntry));

    if (!entry) {
        return NULL;
    }

    SDL_memset((void *) entry, 0, sizeof(*entry));
    entry->tray = menu->tray;
    entry->menu = menu;

    if (flags & SDL_TRAYENTRY_CHECKBOX) {
        entry->item = gtk_check_menu_item_new_with_label(label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry->item), !!(flags & SDL_TRAYENTRY_CHECKED));
    } else {
        entry->item = gtk_menu_item_new_with_label(label);
    }

    gtk_widget_set_sensitive(entry->item, !(flags & SDL_TRAYENTRY_DISABLED));

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **) SDL_realloc(entry->tray->entries, (entry->tray->nEntries + 1) * sizeof(SDL_TrayEntry **));

    if (!new_entries) {
        SDL_free(entry);
        return NULL;
    }

    new_entries[entry->tray->nEntries] = entry;
    entry->tray->entries = new_entries;
    entry->tray->nEntries++;

    gtk_widget_show(entry->item);
    gtk_menu_shell_append(menu->menu, entry->item);

    return entry;
}

void SDL_AppendTraySeparator(SDL_TrayMenu *menu)
{
    GtkWidget *item = gtk_separator_menu_item_new();
    gtk_widget_show(item);
    gtk_menu_shell_append(menu->menu, item);
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    SDL_Unsupported();
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    SDL_Unsupported();
    return false;
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    SDL_Unsupported();
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    SDL_Unsupported();
    return false;
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    entry->callback = callback;
    entry->userdata = userdata;

    g_signal_connect(entry->item, "activate", G_CALLBACK(call_callback), entry);
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    for (int i = 0; i < tray->nEntries; i++) {
        SDL_free(tray->entries[i]);
    }

    SDL_free(tray->entries);

    SDL_free(tray);
}
