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
#include <errno.h>

/* getpid() */
#include <unistd.h>

/* APPINDICATOR_HEADER is not exposed as a build setting, but the code has been
   written nevertheless to make future maintenance easier. */
#ifdef APPINDICATOR_HEADER
#include APPINDICATOR_HEADER
#else
#include "../../core/unix/SDL_gtk.h"

/* ------------------------------------------------------------------------- */
/*                     BEGIN THIRD-PARTY HEADER CONTENT                      */
/* ------------------------------------------------------------------------- */
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

static AppIndicator *(*app_indicator_new)(const gchar *id, const gchar *icon_name, AppIndicatorCategory category);
static void (*app_indicator_set_status)(AppIndicator *self, AppIndicatorStatus status);
static void (*app_indicator_set_icon)(AppIndicator *self, const gchar *icon_name);
static void (*app_indicator_set_menu)(AppIndicator *self, GtkMenu *menu);

/* ------------------------------------------------------------------------- */
/*                      END THIRD-PARTY HEADER CONTENT                       */
/* ------------------------------------------------------------------------- */
#endif

static void *libappindicator = NULL;

static void quit_appindicator(void)
{
    if (libappindicator) {
        dlclose(libappindicator);
        libappindicator = NULL;
    }
}

const char *appindicator_names[] = {
#ifdef SDL_PLATFORM_OPENBSD
    "libayatana-appindicator3.so",
    "libappindicator3.so",
#else
    "libayatana-appindicator3.so.1",
    "libappindicator3.so.1",
#endif
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

static bool init_appindicator(void)
{
    if (libappindicator) {
        return true;
    }

    libappindicator = find_lib(appindicator_names);

    if (!libappindicator) {
        quit_appindicator();
        return SDL_SetError("Could not load AppIndicator libraries");
    }

    app_indicator_new = dlsym(libappindicator, "app_indicator_new");
    app_indicator_set_status = dlsym(libappindicator, "app_indicator_set_status");
    app_indicator_set_icon = dlsym(libappindicator, "app_indicator_set_icon");
    app_indicator_set_menu = dlsym(libappindicator, "app_indicator_set_menu");

    if (!app_indicator_new ||
        !app_indicator_set_status ||
        !app_indicator_set_icon ||
        !app_indicator_set_menu) {
        quit_appindicator();
        return SDL_SetError("Could not load AppIndicator functions");
    }

    return true;
}

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
    char *icon_dir;
    char *icon_path;

    GtkMenuShell *menu_cached;
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

static bool new_tmp_filename(SDL_Tray *tray)
{
    static int count = 0;

    int would_have_written = SDL_asprintf(&tray->icon_path, "%s/%d.bmp", tray->icon_dir, count++);

    if (would_have_written >= 0) {
        return true;
    }

    tray->icon_path = NULL;
    SDL_SetError("Failed to format new temporary filename");
    return false;
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

    if (menu->menu) {
        SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
        if (gtk) {
            gtk->g.object_unref(menu->menu);
            SDL_Gtk_ExitContext(gtk);
        }
    }

    SDL_free(menu->entries);
    SDL_free(menu);
}

void SDL_UpdateTrays(void)
{
    SDL_UpdateGtk();
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    if (!SDL_IsMainThread()) {
        SDL_SetError("This function should be called on the main thread");
        return NULL;
    }

    if (!init_appindicator()) {
        return NULL;
    }

    SDL_Tray *tray = NULL;
    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (!gtk) {
        goto tray_error;
    }

    tray = (SDL_Tray *)SDL_calloc(1, sizeof(*tray));
    if (!tray) {
        goto tray_error;
    }

    const gchar *cache_dir = gtk->g.get_user_cache_dir();
    if (!cache_dir) {
        SDL_SetError("Cannot get user cache directory: %s", strerror(errno));
        goto tray_error;
    }

    char *sdl_dir;
    SDL_asprintf(&sdl_dir, "%s/SDL", cache_dir);
    if (!SDL_GetPathInfo(sdl_dir, NULL)) {
        if (!SDL_CreateDirectory(sdl_dir)) {
            SDL_SetError("Cannot create directory for tray icon: %s", strerror(errno));
            goto sdl_dir_error;
        }
    }

    /* On success, g_mkdtemp edits its argument in-place to replace the Xs
     * with a random directory name, which it creates safely and atomically.
     * On failure, it sets errno. */
    SDL_asprintf(&tray->icon_dir, "%s/tray-XXXXXX", sdl_dir);
    if (!gtk->g.mkdtemp(tray->icon_dir)) {
        SDL_SetError("Cannot create directory for tray icon: %s", strerror(errno));
        goto icon_dir_error;
    }

    if (icon) {
        if (!new_tmp_filename(tray)) {
            goto icon_dir_error;
        }

        SDL_SaveBMP(icon, tray->icon_path);
    } else {
        // allocate a dummy icon path
        SDL_asprintf(&tray->icon_path, " ");
    }

    tray->indicator = app_indicator_new(get_appindicator_id(), tray->icon_path,
                                        APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    app_indicator_set_status(tray->indicator, APP_INDICATOR_STATUS_ACTIVE);

    // The tray icon isn't shown before a menu is created; create one early.
    tray->menu_cached = (GtkMenuShell *)gtk->g.object_ref_sink(gtk->gtk.menu_new());
    app_indicator_set_menu(tray->indicator, GTK_MENU(tray->menu_cached));

    SDL_RegisterTray(tray);
    SDL_Gtk_ExitContext(gtk);

    return tray;

icon_dir_error:
    SDL_free(tray->icon_dir);

sdl_dir_error:
    SDL_free(sdl_dir);

tray_error:
    if (tray) {
        SDL_free(tray);
    }

    if (gtk) {
        SDL_Gtk_ExitContext(gtk);
    }

    return NULL;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }

    if (tray->icon_path) {
        SDL_RemovePath(tray->icon_path);
        SDL_free(tray->icon_path);
        tray->icon_path = NULL;
    }

    /* AppIndicator caches the icon files; always change filename to avoid caching */

    if (icon && new_tmp_filename(tray)) {
        SDL_SaveBMP(icon, tray->icon_path);
        app_indicator_set_icon(tray->indicator, tray->icon_path);
    } else {
        SDL_free(tray->icon_path);
        tray->icon_path = NULL;
        app_indicator_set_icon(tray->indicator, NULL);
    }
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    /* AppIndicator provides no tooltip support. */
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (!gtk) {
        return NULL;
    }

    tray->menu = (SDL_TrayMenu *)SDL_calloc(1, sizeof(*tray->menu));
    if (!tray->menu) {
        SDL_Gtk_ExitContext(gtk);
        return NULL;
    }

    tray->menu->menu = gtk->g.object_ref(tray->menu_cached);
    tray->menu->parent_tray = tray;
    tray->menu->parent_entry = NULL;
    tray->menu->nEntries = 0;
    tray->menu->entries = NULL;

    SDL_Gtk_ExitContext(gtk);

    return tray->menu;
}

SDL_TrayMenu *SDL_GetTrayMenu(SDL_Tray *tray)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    return tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    if (!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    if (entry->submenu) {
        SDL_SetError("Tray entry submenu already exists");
        return NULL;
    }

    if (!(entry->flags & SDL_TRAYENTRY_SUBMENU)) {
        SDL_SetError("Cannot create submenu for entry not created with SDL_TRAYENTRY_SUBMENU");
        return NULL;
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (!gtk) {
        return NULL;
    }

    entry->submenu = (SDL_TrayMenu *)SDL_calloc(1, sizeof(*entry->submenu));
    if (!entry->submenu) {
        SDL_Gtk_ExitContext(gtk);
        return NULL;
    }

    entry->submenu->menu = gtk->g.object_ref_sink(gtk->gtk.menu_new());
    entry->submenu->parent_tray = NULL;
    entry->submenu->parent_entry = entry;
    entry->submenu->nEntries = 0;
    entry->submenu->entries = NULL;

    gtk->gtk.menu_item_set_submenu(GTK_MENU_ITEM(entry->item), GTK_WIDGET(entry->submenu->menu));

    SDL_Gtk_ExitContext(gtk);

    return entry->submenu;
}

SDL_TrayMenu *SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    if (!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return entry->submenu;
}

const SDL_TrayEntry **SDL_GetTrayEntries(SDL_TrayMenu *menu, int *count)
{
    if (!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    if (count) {
        *count = menu->nEntries;
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

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        gtk->gtk.widget_destroy(entry->item);
        SDL_Gtk_ExitContext(gtk);
    }
    SDL_free(entry);
}

SDL_TrayEntry *SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    if (!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    if (pos < -1 || pos > menu->nEntries) {
        SDL_InvalidParamError("pos");
        return NULL;
    }

    if (pos == -1) {
        pos = menu->nEntries;
    }

    SDL_TrayEntry *entry = NULL;
    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (!gtk) {
        goto error;
    }

    entry = (SDL_TrayEntry *)SDL_calloc(1, sizeof(*entry));
    if (!entry) {
        goto error;
    }

    entry->parent = menu;
    entry->item = NULL;
    entry->ignore_signal = false;
    entry->flags = flags;
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu = NULL;

    if (label == NULL) {
        entry->item = gtk->gtk.separator_menu_item_new();
    } else if (flags & SDL_TRAYENTRY_CHECKBOX) {
        entry->item = gtk->gtk.check_menu_item_new_with_label(label);
        gboolean active = ((flags & SDL_TRAYENTRY_CHECKED) != 0);
        gtk->gtk.check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry->item), active);
    } else {
        entry->item = gtk->gtk.menu_item_new_with_label(label);
    }

    gboolean sensitive = ((flags & SDL_TRAYENTRY_DISABLED) == 0);
    gtk->gtk.widget_set_sensitive(entry->item, sensitive);

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **)SDL_realloc(menu->entries, (menu->nEntries + 2) * sizeof(*new_entries));

    if (!new_entries) {
        goto error;
    }

    menu->entries = new_entries;
    menu->nEntries++;

    for (int i = menu->nEntries - 1; i > pos; i--) {
        menu->entries[i] = menu->entries[i - 1];
    }

    new_entries[pos] = entry;
    new_entries[menu->nEntries] = NULL;

    gtk->gtk.widget_show(entry->item);
    gtk->gtk.menu_shell_insert(menu->menu, entry->item, (pos == menu->nEntries) ? -1 : pos);

    gtk->g.signal_connect(entry->item, "activate", call_callback, entry);

    SDL_Gtk_ExitContext(gtk);

    return entry;

error:
    if (entry) {
        SDL_free(entry);
    }

    if (gtk) {
        SDL_Gtk_ExitContext(gtk);
    }

    return NULL;
}

void SDL_SetTrayEntryLabel(SDL_TrayEntry *entry, const char *label)
{
    if (!entry) {
        return;
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        gtk->gtk.menu_item_set_label(GTK_MENU_ITEM(entry->item), label);
        SDL_Gtk_ExitContext(gtk);
    }
}

const char *SDL_GetTrayEntryLabel(SDL_TrayEntry *entry)
{
    if (!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    const char *label = NULL;

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        label = gtk->gtk.menu_item_get_label(GTK_MENU_ITEM(entry->item));
        SDL_Gtk_ExitContext(gtk);
    }

    return label;
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    if (!entry || !(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        return;
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        entry->ignore_signal = true;
        gtk->gtk.check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry->item), checked);
        entry->ignore_signal = false;
        SDL_Gtk_ExitContext(gtk);
    }
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    if (!entry || !(entry->flags & SDL_TRAYENTRY_CHECKBOX)) {
        return false;
    }

    bool checked = false;

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        checked = gtk->gtk.check_menu_item_get_active(GTK_CHECK_MENU_ITEM(entry->item));
        SDL_Gtk_ExitContext(gtk);
    }

    return checked;
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    if (!entry) {
        return;
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        gtk->gtk.widget_set_sensitive(entry->item, enabled);
        SDL_Gtk_ExitContext(gtk);
    }
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    if (!entry) {
        return false;
    }

    bool enabled = false;

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        enabled = gtk->gtk.widget_get_sensitive(entry->item);
        SDL_Gtk_ExitContext(gtk);
    }

    return enabled;
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    if (!entry) {
        return;
    }

    entry->callback = callback;
    entry->userdata = userdata;
}

void SDL_ClickTrayEntry(SDL_TrayEntry *entry)
{
	if (!entry) {
		return;
	}

	if (entry->flags & SDL_TRAYENTRY_CHECKBOX) {
		SDL_SetTrayEntryChecked(entry, !SDL_GetTrayEntryChecked(entry));
	}

	if (entry->callback) {
		entry->callback(entry->userdata, entry);
	}
}

SDL_TrayMenu *SDL_GetTrayEntryParent(SDL_TrayEntry *entry)
{
    if (!entry) {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return entry->parent;
}

SDL_TrayEntry *SDL_GetTrayMenuParentEntry(SDL_TrayMenu *menu)
{
    return menu->parent_entry;
}

SDL_Tray *SDL_GetTrayMenuParentTray(SDL_TrayMenu *menu)
{
    if (!menu) {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    return menu->parent_tray;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }

    SDL_UnregisterTray(tray);

    if (tray->menu) {
        DestroySDLMenu(tray->menu);
    }

    if (tray->icon_path) {
        SDL_RemovePath(tray->icon_path);
        SDL_free(tray->icon_path);
    }

    if (tray->icon_dir) {
        SDL_RemovePath(tray->icon_dir);
        SDL_free(tray->icon_dir);
    }

    SDL_GtkContext *gtk = SDL_Gtk_EnterContext();
    if (gtk) {
        if (tray->menu_cached) {
            gtk->g.object_unref(tray->menu_cached);
        }

        if (tray->indicator) {
            gtk->g.object_unref(tray->indicator);
        }

        SDL_Gtk_ExitContext(gtk);
    }

    SDL_free(tray);
}
