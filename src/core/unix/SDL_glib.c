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

// we never link directly to glib
static const char *glib_names[] = {
#ifdef SDL_PLATFORM_OPENBSD
    "libglib-2.0.so",
#else
    "libglib-2.0.so.0",
#endif
    NULL
};

void *SDL_Glib_FindLib(const char **names)
{
    const char **name_ptr = names;
    void *handle = NULL;

    do {
        handle = SDL_LoadObject(*name_ptr);
    } while (*++name_ptr && !handle);

    return handle;
}

bool SDL_GlibContext_Init(SDL_GlibContext *ctx, void *lib, bool do_unload, bool bypass_hint)
{
	ctx->library = lib;
	ctx->do_unload = do_unload;
	
	if (!bypass_hint) {
		if (!SDL_GetHintBoolean("SDL_ENABLE_GLIB", true)) {
			return false;
		}
	}
	
	if (!ctx->library) {
		ctx->library = SDL_Glib_FindLib(glib_names);
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
    
    return true;
}

bool SDL_GlibContext_Cleanup(SDL_GlibContext *ctx) 
{
	if (ctx->library && ctx->do_unload) {
		SDL_UnloadObject(ctx->library);
	}
    SDL_zero(ctx);
}
