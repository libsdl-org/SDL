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

// we never link directly to glib
#ifdef SDL_PLATFORM_OPENBSD
#define GLIB_LIB "libglib-2.0.so"
#else
#define GLIB_LIB "libglib-2.0.so.0"
#endif

#ifdef SDL_PLATFORM_OPENBSD
#define GIO_LIB "libgio-2.0.so"
#else
#define GIO_LIB "libgio-2.0.so.0"
#endif


bool SDL_GlibContext_Init(SDL_GlibContext *ctx, void *lib, bool do_unload, bool bypass_hint, bool gio)
{
	ctx->library = lib;
	ctx->do_unload = do_unload;
	
	if (!bypass_hint) {
		if (!SDL_GetHintBoolean(SDL_HINT_ENABLE_GLIB, true)) {
			return false;
		}
	}
	
	if (!ctx->library) {
		if (!gio) {
			ctx->library = SDL_LoadObject(GLIB_LIB);
		} else {
			ctx->library = SDL_LoadObject(GIO_LIB);
		}
		if (!ctx->library) {
			return SDL_SetError("Could not load Glib");
		}
	}

    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, signal_connect_data);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, mkdtemp);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, object_ref);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, object_ref_sink);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, object_unref);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, object_get);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, signal_handler_disconnect);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, main_context_push_thread_default);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, main_context_pop_thread_default);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, main_context_new);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, main_context_acquire);
    SDL_GLIB_PTR_SYM(ctx, ctx->library, g, main_context_iteration);
	SDL_GLIB_PTR_SYM(ctx, ctx->library, g, strfreev);
  
	if (gio) {
		SDL_GLIB_PTR_SYM(ctx, ctx->library, g, settings_new);
		SDL_GLIB_PTR_SYM(ctx, ctx->library, g, settings_list_schemas);	
		SDL_GLIB_PTR_SYM(ctx, ctx->library, g, settings_get_strv);	
	}
    return true;
}

void SDL_GlibContext_Cleanup(SDL_GlibContext *ctx) 
{
	if (ctx->library && ctx->do_unload) {
		SDL_UnloadObject(ctx->library);
	}
    SDL_zero(ctx);
}
