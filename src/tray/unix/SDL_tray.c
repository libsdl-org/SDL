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

#include "../SDL_tray_utils.h"

#include <dlfcn.h>

/* getpid() */
#include <unistd.h>

/* APPINDICATOR_HEADER is not exposed as a build setting, but the code has been
   written nevertheless to make future maintenance easier. */
#ifdef APPINDICATOR_HEADER
#include APPINDICATOR_HEADER
#else
/* ------------------------------------------------------------------------- */
/*                     BEGIN THIRD-PARTY HEADER CONTENT                      */
/* ------------------------------------------------------------------------- */
/* Glib 2.0 */

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
void (*g_object_unref)(gpointer object);

#define g_signal_connect(instance, detailed_signal, c_handler, data) \
    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, (GConnectFlags) 0)

#define _G_TYPE_CIC(ip, gt, ct) ((ct*) ip)

#define G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type) (_G_TYPE_CIC ((instance), (g_type), c_type))

#define G_CALLBACK(f) ((GCallback) (f))

#define FALSE 0
#define TRUE 1

/* GTK 3.0 */

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkMenuShell GtkMenuShell;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;

gboolean (*gtk_init_check)(int *argc, char ***argv);
void (*gtk_main)(void);
void (*gtk_main_quit)(void);
GtkWidget* (*gtk_menu_new)(void);
GtkWidget* (*gtk_separator_menu_item_new)(void);
GtkWidget* (*gtk_menu_item_new_with_label)(const gchar *label);
void (*gtk_menu_item_set_submenu)(GtkMenuItem *menu_item, GtkWidget *submenu);
GtkWidget* (*gtk_check_menu_item_new_with_label)(const gchar *label);
void (*gtk_check_menu_item_set_active)(GtkCheckMenuItem *check_menu_item, gboolean is_active);
void (*gtk_widget_set_sensitive)(GtkWidget *widget, gboolean sensitive);
void (*gtk_widget_show)(GtkWidget *widget);
void (*gtk_menu_shell_append)(GtkMenuShell *menu_shell, GtkWidget *child);
void (*gtk_menu_shell_insert)(GtkMenuShell *menu_shell, GtkWidget *child, gint position);
void (*gtk_widget_destroy)(GtkWidget *widget);
const gchar *(*gtk_menu_item_get_label)(GtkMenuItem *menu_item);
void (*gtk_menu_item_set_label)(GtkMenuItem *menu_item, const gchar *label);
gboolean (*gtk_check_menu_item_get_active)(GtkCheckMenuItem *check_menu_item);
gboolean (*gtk_widget_get_sensitive)(GtkWidget *widget);

#define GTK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_ITEM, GtkMenuItem))
#define GTK_WIDGET(widget) (G_TYPE_CHECK_INSTANCE_CAST ((widget), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_CHECK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItem))
#define GTK_MENU(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))

/* AppIndicator */

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

static int main_gtk_thread(void *data)
{
    gtk_main();
    return 0;
}

static bool gtk_thread_active = false;

#ifdef APPINDICATOR_HEADER

static void quit_gtk(void)
{
}

static bool init_gtk(void)
{

}

#else

static bool gtk_is_init = false;

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

const char *appindicator_names[] = {
    "libayatana-appindicator3.so",
    "libayatana-appindicator3.so.1",
    "libayatana-appindicator.so",
    "libappindicator3.so",
    "libappindicator3.so.1",
    "libappindicator.so",
    "libappindicator.so.1",
    NULL
};

const char *gtk_names[] = {
    "libgtk-3.so",
    "libgtk-3.so.0",
    NULL
};

const char *gdk_names[] = {
    "libgdk-3.so",
    "libgdk-3.so.0",
    NULL
};

static void *find_lib(const char **names)
{
    const char **name_ptr = names;
    void *handle = NULL;

    do {
        handle = dlopen(*name_ptr, RTLD_LAZY);
    } while (*++name_ptr && !handle);

    return handle;
}

static bool init_gtk(void)
{
    if (gtk_is_init) {
        return true;
    }

    libappindicator = find_lib(appindicator_names);
    libgtk = find_lib(gtk_names);
    libgdk = find_lib(gdk_names);

    if (!libappindicator || !libgtk || !libgdk) {
        quit_gtk();
        return SDL_SetError("Could not load GTK/AppIndicator libraries");
    }

    gtk_init_check = dlsym(libgtk, "gtk_init_check");
    gtk_main = dlsym(libgtk, "gtk_main");
    gtk_main_quit = dlsym(libgtk, "gtk_main_quit");
    gtk_menu_new = dlsym(libgtk, "gtk_menu_new");
    gtk_separator_menu_item_new = dlsym(libgtk, "gtk_separator_menu_item_new");
    gtk_menu_item_new_with_label = dlsym(libgtk, "gtk_menu_item_new_with_label");
    gtk_menu_item_set_submenu = dlsym(libgtk, "gtk_menu_item_set_submenu");
    gtk_check_menu_item_new_with_label = dlsym(libgtk, "gtk_check_menu_item_new_with_label");
    gtk_check_menu_item_set_active = dlsym(libgtk, "gtk_check_menu_item_set_active");
    gtk_widget_set_sensitive = dlsym(libgtk, "gtk_widget_set_sensitive");
    gtk_widget_show = dlsym(libgtk, "gtk_widget_show");
    gtk_menu_shell_append = dlsym(libgtk, "gtk_menu_shell_append");
    gtk_menu_shell_insert = dlsym(libgtk, "gtk_menu_shell_insert");
    gtk_widget_destroy = dlsym(libgtk, "gtk_widget_destroy");
    gtk_menu_item_get_label = dlsym(libgtk, "gtk_menu_item_get_label");
    gtk_menu_item_set_label = dlsym(libgtk, "gtk_menu_item_set_label");
    gtk_check_menu_item_get_active = dlsym(libgtk, "gtk_check_menu_item_get_active");
    gtk_widget_get_sensitive = dlsym(libgtk, "gtk_widget_get_sensitive");

    g_signal_connect_data = dlsym(libgdk, "g_signal_connect_data");
    g_object_unref = dlsym(libgdk, "g_object_unref");

    app_indicator_new = dlsym(libappindicator, "app_indicator_new");
    app_indicator_set_status = dlsym(libappindicator, "app_indicator_set_status");
    app_indicator_set_icon = dlsym(libappindicator, "app_indicator_set_icon");
    app_indicator_set_menu = dlsym(libappindicator, "app_indicator_set_menu");

    if (!gtk_init_check ||
        !gtk_main ||
        !gtk_main_quit ||
        !gtk_menu_new ||
        !gtk_separator_menu_item_new ||
        !gtk_menu_item_new_with_label ||
        !gtk_menu_item_set_submenu ||
        !gtk_check_menu_item_new_with_label ||
        !gtk_check_menu_item_set_active ||
        !gtk_widget_set_sensitive ||
        !gtk_widget_show ||
        !gtk_menu_shell_append ||
        !gtk_menu_shell_insert ||
        !gtk_widget_destroy ||
        !g_signal_connect_data ||
        !g_object_unref ||
        !app_indicator_new ||
        !app_indicator_set_status ||
        !app_indicator_set_icon ||
        !app_indicator_set_menu ||
        !gtk_menu_item_get_label ||
        !gtk_menu_item_set_label ||
        !gtk_check_menu_item_get_active ||
        !gtk_widget_get_sensitive) {
        quit_gtk();
        return SDL_SetError("Could not load GTK/AppIndicator functions");
    }

    if (gtk_init_check(0, NULL) == FALSE) {
        quit_gtk();
        return SDL_SetError("Could not init GTK");
    }

    gtk_is_init = true;

    return true;
}
#endif

struct SDL_TrayMenu {
    GtkMenuShell *menu;

    int nEntries;
    SDL_TrayEntry **entries;

    SDL_Tray *parent_tray;
    SDL_TrayEntry *parent_entry;
};

struct SDL_TrayEntry {
    SDL_TrayMenu *parent;
    GtkWidget *item;

    /* Checkboxes are "activated" when programmatically checked/unchecked; this
       is a workaround. */
    bool ignore_signal;

    SDL_TrayEntryFlags flags;
    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu *submenu;
};

struct SDL_Tray {
    AppIndicator *indicator;
    SDL_TrayMenu *menu;
    char icon_path[256];
};

static void call_callback(GtkMenuItem *item, gpointer ptr)
{
    SDL_TrayEntry *entry = ptr;

    /* Not needed with AppIndicator, may be needed with other frameworks */
    /* if (entry->flags & SDL_TRAYENTRY_CHECKBOX) {
        SDL_SetTrayEntryChecked(entry, !SDL_GetTrayEntryChecked(entry));
    } */

    if (entry->ignore_signal) {
        return;
    }

    if (entry->callback) {
        entry->callback(entry->userdata, entry);
    }
}

/* Since AppIndicator deals only in filenames, which are inherently subject to
   timing attacks, don't bother generating a secure filename. */
static bool get_tmp_filename(char *buffer, size_t size)
{
    static int count = 0;

    if (size < 64) {
        return SDL_SetError("Can't create temporary file for icon: size %u < 64", (unsigned int)size);
    }

    int would_have_written = SDL_snprintf(buffer, size, "/tmp/sdl_appindicator_icon_%d_%d.bmp", getpid(), count++);

    return would_have_written > 0 && would_have_written < size - 1;
}

static const char *get_appindicator_id(void)
{
    static int count = 0;
    static char buffer[256];

    int would_have_written = SDL_snprintf(buffer, sizeof(buffer), "sdl-appindicator-%d-%d", getpid(), count++);

    if (would_have_written <= 0 || would_have_written >= sizeof(buffer) - 1) {
        SDL_SetError("Couldn't fit %d bytes in buffer of size %d", would_have_written, (int) sizeof(buffer));
        return NULL;
    }

    return buffer;
}

static void DestroySDLMenu(SDL_TrayMenu *menu)
{
    for (int i = 0; i < menu->nEntries; i++) {
        if (menu->entries[i] && menu->entries[i]->submenu) {
            DestroySDLMenu(menu->entries[i]->submenu);
        }
        SDL_free(menu->entries[i]);
    }
    SDL_free(menu->entries);
    SDL_free(menu);
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    if (init_gtk() != true) {
        return NULL;
    }

    if (!gtk_thread_active) {
        SDL_DetachThread(SDL_CreateThread(main_gtk_thread, "tray gtk", NULL));
        gtk_thread_active = true;
    }

    SDL_Tray *tray = (SDL_Tray *)SDL_malloc(sizeof(*tray));
    if (!tray) {
        return NULL;
    }

    SDL_memset((void *) tray, 0, sizeof(*tray));

    get_tmp_filename(tray->icon_path, sizeof(tray->icon_path));
    SDL_SaveBMP(icon, tray->icon_path);

    tray->indicator = app_indicator_new(get_appindicator_id(), tray->icon_path,
                                        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    app_indicator_set_status(tray->indicator, APP_INDICATOR_STATUS_ACTIVE);

    SDL_IncrementTrayCount();

    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (*tray->icon_path) {
        SDL_RemovePath(tray->icon_path);
    }

    /* AppIndicator caches the icon files; always change filename to avoid caching */

    if (icon) {
        get_tmp_filename(tray->icon_path, sizeof(tray->icon_path));
        SDL_SaveBMP(icon, tray->icon_path);
        app_indicator_set_icon(tray->indicator, tray->icon_path);
    } else {
        *tray->icon_path = '\0';
        app_indicator_set_icon(tray->indicator, NULL);
    }
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    /* AppIndicator provides no tooltip support. */
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    tray->menu = (SDL_TrayMenu *)SDL_malloc(sizeof(*tray->menu));
    if (!tray->menu) {
        return NULL;
    }

    tray->menu->menu = (GtkMenuShell *)gtk_menu_new();
    tray->menu->parent_tray = tray;
    tray->menu->parent_entry = NULL;
    tray->menu->nEntries = 0;
    tray->menu->entries = NULL;

    app_indicator_set_menu(tray->indicator, GTK_MENU(tray->menu->menu));

    return tray->menu;
}

SDL_TrayMenu *SDL_GetTrayMenu(SDL_Tray *tray)
{
    return tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    if (entry->submenu) {
        SDL_SetError("Tray entry submenu already exists");
        return NULL;
    }

    if (!(entry->flags & SDL_TRAYENTRY_SUBMENU)) {
        SDL_SetError("Cannot create submenu for entry not created with SDL_TRAYENTRY_SUBMENU");
        return NULL;
    }

    entry->submenu = (SDL_TrayMenu *)SDL_malloc(sizeof(*entry->submenu));
    if (!entry->submenu) {
        return NULL;
    }

    entry->submenu->menu = (GtkMenuShell *)gtk_menu_new();
    entry->submenu->parent_tray = NULL;
    entry->submenu->parent_entry = entry;
    entry->submenu->nEntries = 0;
    entry->submenu->entries = NULL;

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry->item), GTK_WIDGET(entry->submenu->menu));

    return entry->submenu;
}

SDL_TrayMenu *SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    return entry->submenu;
}

const SDL_TrayEntry **SDL_GetTrayEntries(SDL_TrayMenu *menu, int *size)
{
    if (size) {
        *size = menu->nEntries;
    }
    return (const SDL_TrayEntry **)menu->entries;
}

void SDL_RemoveTrayEntry(SDL_TrayEntry *entry)
{
    if (!entry) {
        return;
    }

    SDL_TrayMenu *menu = entry->parent;

    bool found = false;
    for (int i = 0; i < menu->nEntries - 1; i++) {
        if (menu->entries[i] == entry) {
            found = true;
        }

        if (found) {
            menu->entries[i] = menu->entries[i + 1];
        }
    }

    if (entry->submenu) {
        DestroySDLMenu(entry->submenu);
    }

    menu->nEntries--;
    SDL_TrayEntry **new_entries = (SDL_TrayEntry **)SDL_realloc(menu->entries, (menu->nEntries + 1) * sizeof(*new_entries));

    /* Not sure why shrinking would fail, but even if it does, we can live with a "too big" array */
    if (new_entries) {
        menu->entries = new_entries;
        menu->entries[menu->nEntries] = NULL;
    }

    gtk_widget_destroy(entry->item);
    SDL_free(entry);
}

SDL_TrayEntry *SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    if (pos < -1 || pos > menu->nEntries) {
        SDL_InvalidParamError("pos");
        return NULL;
    }

    if (pos == -1) {
        pos = menu->nEntries;
    }

    SDL_TrayEntry *entry = (SDL_TrayEntry *)SDL_malloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }

    SDL_memset((void *) entry, 0, sizeof(*entry));
    entry->parent = menu;
    entry->item = NULL;
    entry->ignore_signal = false;
    entry->flags = flags;
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu = NULL;

    if (label == NULL) {
        entry->item = gtk_separator_menu_item_new();
    } else if (flags & SDL_TRAYENTRY_CHECKBOX) {
        entry->item = gtk_check_menu_item_new_with_label(label);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry->item), !!(flags & SDL_TRAYENTRY_CHECKED));
    } else {
        entry->item = gtk_menu_item_new_with_label(label);
    }

    gtk_widget_set_sensitive(entry->item, !(flags & SDL_TRAYENTRY_DISABLED));

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **)SDL_realloc(menu->entries, (menu->nEntries + 2) * sizeof(*new_entries));

    if (!new_entries) {
        SDL_free(entry);
        return NULL;
    }

    menu->entries = new_entries;
    menu->nEntries++;

    for (int i = menu->nEntries - 1; i > pos; i--) {
        menu->entries[i] = menu->entries[i - 1];
    }

    new_entries[pos] = entry;
    new_entries[menu->nEntries] = NULL;

    gtk_widget_show(entry->item);
    gtk_menu_shell_insert(menu->menu, entry->item, (pos == menu->nEntries) ? -1 : pos);

    g_signal_connect(entry->item, "activate", G_CALLBACK(call_callback), entry);

    return entry;
}

void SDL_SetTrayEntryLabel(SDL_TrayEntry *entry, const char *label)
{
    gtk_menu_item_set_label(GTK_MENU_ITEM(entry->item), label);
}

const char *SDL_GetTrayEntryLabel(SDL_TrayEntry *entry)
{
    return gtk_menu_item_get_label(GTK_MENU_ITEM(entry->item));
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    if (!(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        SDL_SetError("Cannot update check for entry not created with SDL_TRAYENTRY_CHECKBOX");
        return;
    }

    entry->ignore_signal = true;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry->item), checked);
    entry->ignore_signal = false;
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    if (!(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        SDL_SetError("Cannot fetch check for entry not created with SDL_TRAYENTRY_CHECKBOX");
        return false;
    }

    return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(entry->item));
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    gtk_widget_set_sensitive(entry->item, enabled);
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    return gtk_widget_get_sensitive(entry->item);
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    entry->callback = callback;
    entry->userdata = userdata;
}

SDL_TrayMenu *SDL_GetTrayEntryParent(SDL_TrayEntry *entry)
{
    return entry->parent;
}

SDL_TrayEntry *SDL_GetTrayMenuParentEntry(SDL_TrayMenu *menu)
{
    return menu->parent_entry;
}

SDL_Tray *SDL_GetTrayMenuParentTray(SDL_TrayMenu *menu)
{
    return menu->parent_tray;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    if (!tray) {
        return;
    }

    if (tray->menu) {
        DestroySDLMenu(tray->menu);
    }

    if (*tray->icon_path) {
        SDL_RemovePath(tray->icon_path);
    }

    g_object_unref(tray->indicator);

    SDL_free(tray);

    SDL_DecrementTrayCount();

    if (SDL_HasNoActiveTrays()) {
        gtk_main_quit();
        gtk_thread_active = false;
    }
}
