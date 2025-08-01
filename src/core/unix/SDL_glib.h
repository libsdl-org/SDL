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
    
extern void *SDL_Glib_FindLib(const char **names);

/* Glib 2.0 */
typedef unsigned long SDL_gulong;
typedef void *SDL_gpointer;
typedef char SDL_gchar;
typedef int SDL_gint;
typedef unsigned int SDL_guint;
typedef double SDL_gdouble;
typedef SDL_gint SDL_gboolean;

typedef void (*SDL_GCallback)(void);
typedef struct _SDL_GClosure SDL_GClosure;
typedef void (*SDL_GClosureNotify) (SDL_gpointer data, SDL_GClosure *closure);
typedef SDL_gboolean (*SDL_GSourceFunc) (SDL_gpointer user_data);

typedef struct _SDL_GParamSpec SDL_GParamSpec;
typedef struct _SDL_GMainContext SDL_GMainContext;

typedef enum SDL_GConnectFlags
{
    SDL_G_CONNECT_DEFAULT = 0,
    SDL_G_CONNECT_AFTER = 1 << 0,
    SDL_G_CONNECT_SWAPPED = 1 << 1
} SDL_GConnectFlags;

#define SDL_G_CALLBACK(f) ((SDL_GCallback) (f))
#define SDL_G_TYPE_CIC(ip, gt, ct) ((ct*) ip)
#define SDL_G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type) (SDL_G_TYPE_CIC ((instance), (g_type), c_type))

#define SDL_G_FALSE 0
#define SDL_G_TRUE 1

typedef struct SDL_GlibContext
{
	void *library;
	bool do_unload;
	
	SDL_gulong (*signal_connect)(SDL_gpointer instance, const SDL_gchar *detailed_signal, void *c_handler, SDL_gpointer data);
	SDL_gulong (*signal_connect_data)(SDL_gpointer instance, const SDL_gchar *detailed_signal, SDL_GCallback c_handler, SDL_gpointer data, SDL_GClosureNotify destroy_data, SDL_GConnectFlags connect_flags);
	void (*object_unref)(SDL_gpointer object);
	SDL_gchar *(*mkdtemp)(SDL_gchar *template);
	SDL_gpointer (*object_ref_sink)(SDL_gpointer object);
	SDL_gpointer (*object_ref)(SDL_gpointer object);
	void (*object_get)(SDL_gpointer object, const SDL_gchar *first_property_name, ...);
	void (*signal_handler_disconnect)(SDL_gpointer instance, SDL_gulong handler_id);
	void (*main_context_push_thread_default)(SDL_GMainContext *context);
	void (*main_context_pop_thread_default)(SDL_GMainContext *context);
	SDL_GMainContext *(*main_context_new)(void);
	SDL_gboolean (*main_context_acquire)(SDL_GMainContext *context);
	SDL_gboolean (*main_context_iteration)(SDL_GMainContext *context, SDL_gboolean may_block);
} SDL_GlibContext;

extern bool SDL_GlibContext_Init(SDL_GlibContext *ctx, void *lib, bool do_unload, bool bypass_hint);
extern void SDL_GlibContext_Cleanup(SDL_GlibContext *ctx);

#endif // SDL_glib_h_
