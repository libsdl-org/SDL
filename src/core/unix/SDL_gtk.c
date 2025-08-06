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
#include "SDL_gtk.h"

#include <dlfcn.h>

#define SDL_GTK_SYM2_OPTIONAL(ctx, lib, sub, fn, sym)                     \
    ctx.sub.fn = (void *)SDL_LoadFunction(lib, #sym)

#define SDL_GTK_SYM2(ctx, lib, sub, fn, sym)                              \
    if (!(ctx.sub.fn = (void *)SDL_LoadFunction(lib, #sym))) {            \
        return SDL_SetError("Could not load GTK functions");              \
    }

#define SDL_GTK_SYM_OPTIONAL(ctx, lib, sub, fn) \
    SDL_GTK_SYM2_OPTIONAL(ctx, lib, sub, fn, sub##_##fn)

#define SDL_GTK_SYM(ctx, lib, sub, fn) \
    SDL_GTK_SYM2(ctx, lib, sub, fn, sub##_##fn)

#ifdef SDL_PLATFORM_OPENBSD
#define GDK3_LIB "libgdk-3.so"
#else
#define GDK3_LIB "libgdk-3.so.0"
#endif

#ifdef SDL_PLATFORM_OPENBSD
#define GTK3_LIB "libgtk-3.so"
#else
#define GTK3_LIB "libgtk-3.so.0"
#endif

// we never link directly to gtk
static void *libgdk = NULL;
static void *libgtk = NULL;

static SDL_GtkContext gtk;
static GMainContext *sdl_main_context;

gulong signal_connect(gpointer instance, const gchar *detailed_signal, void *c_handler, gpointer data)
{
    return gtk.g.signal_connect_data(instance, detailed_signal, SDL_G_CALLBACK(c_handler), data, NULL, (SDL_GConnectFlags)0);
}

static void QuitGtk(void)
{
    SDL_UnloadObject(libgdk);
    SDL_UnloadObject(libgtk);

    libgdk = NULL;
    libgtk = NULL;
}

static bool IsGtkInit()
{
    return libgdk != NULL && libgtk != NULL;
}

static bool InitGtk(void)
{
    if (!SDL_GetHintBoolean("SDL_ENABLE_GTK", true)) {
        return false;
    }

    if (IsGtkInit()) {
        return true;
    }

    // GTK only allows a single version to be loaded into a process at a time,
    // so if there is one already loaded ensure it is the version we use.
    void *progress_get_type = dlsym(RTLD_DEFAULT, "gtk_progress_get_type");
    void *misc_get_type = dlsym(RTLD_DEFAULT, "gtk_misc_get_type");
    if (progress_get_type || misc_get_type) {
        void *libgtk3 = dlopen(GTK3_LIB, RTLD_NOLOAD | RTLD_LAZY);
        if (!libgtk3) {
            QuitGtk();
            return SDL_SetError("Could not load GTK-3, another GTK version already present");
        }

        dlclose(libgtk3);
    }

    libgdk = SDL_LoadObject(GDK3_LIB);
    libgtk = SDL_LoadObject(GTK3_LIB);

    if (!libgdk || !libgtk) {
        QuitGtk();
        return SDL_SetError("Could not load GTK libraries");
    }

    SDL_GTK_SYM(gtk, libgtk, gtk, init_check);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_new);
    SDL_GTK_SYM(gtk, libgtk, gtk, separator_menu_item_new);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_item_new_with_label);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_item_set_submenu);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_item_get_label);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_item_set_label);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_shell_append);
    SDL_GTK_SYM(gtk, libgtk, gtk, menu_shell_insert);
    SDL_GTK_SYM(gtk, libgtk, gtk, check_menu_item_new_with_label);
    SDL_GTK_SYM(gtk, libgtk, gtk, check_menu_item_get_active);
    SDL_GTK_SYM(gtk, libgtk, gtk, check_menu_item_set_active);
    SDL_GTK_SYM(gtk, libgtk, gtk, widget_show);
    SDL_GTK_SYM(gtk, libgtk, gtk, widget_destroy);
    SDL_GTK_SYM(gtk, libgtk, gtk, widget_get_sensitive);
    SDL_GTK_SYM(gtk, libgtk, gtk, widget_set_sensitive);
    SDL_GTK_SYM(gtk, libgtk, gtk, settings_get_default);

    SDL_GTK_SYM(gtk, libgdk, g, signal_connect_data);
    SDL_GTK_SYM(gtk, libgdk, g, mkdtemp);
    SDL_GTK_SYM(gtk, libgdk, g, get_user_cache_dir);
    SDL_GTK_SYM(gtk, libgdk, g, object_ref);
    SDL_GTK_SYM(gtk, libgdk, g, object_ref_sink);
    SDL_GTK_SYM(gtk, libgdk, g, object_unref);
    SDL_GTK_SYM(gtk, libgdk, g, object_get);
    SDL_GTK_SYM(gtk, libgdk, g, signal_handler_disconnect);
    SDL_GTK_SYM(gtk, libgdk, g, main_context_push_thread_default);
    SDL_GTK_SYM(gtk, libgdk, g, main_context_pop_thread_default);
    SDL_GTK_SYM(gtk, libgdk, g, main_context_new);
    SDL_GTK_SYM(gtk, libgdk, g, main_context_acquire);
    SDL_GTK_SYM(gtk, libgdk, g, main_context_iteration);

    gtk.g.signal_connect = signal_connect;

    if (gtk.gtk.init_check(0, NULL) == GTK_FALSE) {
        QuitGtk();
        return SDL_SetError("Could not init GTK");
    }

    sdl_main_context = gtk.g.main_context_new();
    if (!sdl_main_context) {
        QuitGtk();
        return SDL_SetError("Could not create GTK context");
    }

    if (!gtk.g.main_context_acquire(sdl_main_context)) {
        QuitGtk();
        return SDL_SetError("Could not acquire GTK context");
    }

    return true;
}

static SDL_InitState gtk_init;

bool SDL_Gtk_Init(void)
{
    static bool is_gtk_available = true;

    if (!is_gtk_available) {
        return false; // don't keep trying if this fails.
    }

    if (SDL_ShouldInit(&gtk_init)) {
        if (InitGtk()) {
            SDL_SetInitialized(&gtk_init, true);
        } else {
            is_gtk_available = false;
            SDL_SetInitialized(&gtk_init, true);
            SDL_Gtk_Quit();
        }
    }

    return IsGtkInit();
}

void SDL_Gtk_Quit(void)
{
    if (!SDL_ShouldQuit(&gtk_init)) {
        return;
    }

    QuitGtk();
    SDL_zero(gtk);
    sdl_main_context = NULL;

    SDL_SetInitialized(&gtk_init, false);
}

SDL_GtkContext *SDL_Gtk_GetContext(void)
{
    return IsGtkInit() ? &gtk : NULL;
}

SDL_GtkContext *SDL_Gtk_EnterContext(void)
{
    SDL_Gtk_Init();

    if (IsGtkInit()) {
        gtk.g.main_context_push_thread_default(sdl_main_context);
        return &gtk;
    }

    return NULL;
}

void SDL_Gtk_ExitContext(SDL_GtkContext *ctx)
{
    if (ctx) {
        ctx->g.main_context_pop_thread_default(sdl_main_context);
    }
}

void SDL_UpdateGtk(void)
{
    if (IsGtkInit()) {
        gtk.g.main_context_iteration(sdl_main_context, GTK_FALSE);
        gtk.g.main_context_iteration(NULL, GTK_FALSE);
    }
}
