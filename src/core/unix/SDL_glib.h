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

#ifndef SDL_glib_h_
#define SDL_glib_h_

/* Dynamic loading functions & macros */
#define SDL_GLIB_SYM2_OPTIONAL(ctx, lib, fn, sym)                     \
    ctx.fn = (void *)SDL_LoadFunction(lib, #sym)

#define SDL_GLIB_SYM2(ctx, lib, fn, sym)                              \
    if (!(ctx.fn = (void *)SDL_LoadFunction(lib, #sym))) {            \
        return SDL_SetError("Could not load #lib functions");              \
    }

#define SDL_GLIB_SYM_OPTIONAL(ctx, lib, fn) \
    SDL_GLIB_SYM2_OPTIONAL(ctx, lib, fn, sub##_##fn)

#define SDL_GLIB_SYM(ctx, lib, sub, fn) \
    SDL_GLIB_SYM2(ctx, lib, fn, sub##_##fn)
    
#define SDL_GLIB_PTR_SYM2(ctx, lib, fn, sym)                              \
    if (!(ctx->fn = (void *)SDL_LoadFunction(lib, #sym))) {            \
        return SDL_SetError("Could not load #lib functions");              \
    }

#define SDL_GLIB_PTR_SYM(ctx, lib, sub, fn) \
    SDL_GLIB_PTR_SYM2(ctx, lib, fn, sub##_##fn)
    
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

#define G_FALSE 0
#define G_TRUE 1

/* GIO */
typedef struct _GSettings GSettings;

typedef struct SDL_GlibContext
{
	void *library;
	bool do_unload;
	
	/* GLib */
	gulong (*signal_connect)(gpointer instance, const gchar *detailed_signal, void *c_handler, gpointer data);
	gulong (*signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, SDL_GConnectFlags connect_flags);
	void (*object_unref)(gpointer object);
	gchar *(*mkdtemp)(gchar *template);
	gpointer (*object_ref_sink)(gpointer object);
	gpointer (*object_ref)(gpointer object);
	void (*object_get)(gpointer object, const gchar *first_property_name, ...);
	void (*signal_handler_disconnect)(gpointer instance, gulong handler_id);
	void (*main_context_push_thread_default)(GMainContext *context);
	void (*main_context_pop_thread_default)(GMainContext *context);
	GMainContext *(*main_context_new)(void);
	gboolean (*main_context_acquire)(GMainContext *context);
	gboolean (*main_context_iteration)(GMainContext *context, gboolean may_block);
	void (*strfreev)(gchar **str_array);
	
	/* GIO */
	GSettings *(*settings_new)(const gchar *schema_id);
	const gchar *const *(*settings_list_schemas)(void);
	gchar **(*settings_get_strv)(GSettings *settings, const gchar *key);

} SDL_GlibContext;

extern bool SDL_GlibContext_Init(SDL_GlibContext *ctx, void *lib, bool do_unload, bool bypass_hint, bool gio);
extern void SDL_GlibContext_Cleanup(SDL_GlibContext *ctx);

#endif // SDL_glib_h_
