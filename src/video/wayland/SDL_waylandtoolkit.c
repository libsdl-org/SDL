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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_internal.h"
#include "SDL_waylandtoolkit.h"
#include "SDL_waylandtoolkitbitmap.h"
#ifdef HAVE_FCFT_H	
#include "SDL_waylandtoolkitfcft.h"
#endif

/* Nasty struct definition needed because of macro hell :( */
struct SDL_Surface
{
    SDL_SurfaceFlags flags;   
    SDL_PixelFormat format;    
    int w;                     
    int h;                    
    int pitch;                 
    void *pixels;              
    int refcount;             
    void *reserved;           
};

extern SDL_WaylandTextRenderer *WaylandToolkit_CreateTextRenderer() {
	SDL_WaylandTextRenderer *ret;
	
#ifdef HAVE_FCFT_H	
	if (SDL_GetHintBoolean(SDL_HINT_VIDEO_WAYLAND_ALLOW_FCFT, true)) {
		ret = WaylandToolkit_CreateTextRendererFcft();
    } else {
		ret = NULL;
	}
#else
	ret = NULL;
#endif

	if (!ret) {
		return WaylandToolkit_CreateTextRendererBitmap();
	} else {
		return ret;
	}
}

SDL_Surface *WaylandToolkit_RenderText(SDL_WaylandTextRenderer *renderer, char *utf8, SDL_Color *bg_fill) {
	if (renderer) {
		SDL_Surface **surfaces;
		SDL_Surface *ret;
		char *utf8t;
		char *start;
		char *end;
		size_t utf8t_sz;
		int surfaces_count;
		int max_surface_width;
		int total_surface_height;
		int i;
		SDL_Rect rct;
		
		surfaces = NULL;
		max_surface_width = 0;
		surfaces_count = 0;
		total_surface_height = 0;
		rct.x = 0;
		rct.y = 0;
		ret = NULL;
		utf8t_sz = SDL_strlen(utf8) + 2;
		utf8t = SDL_malloc(utf8t_sz);
		if (!utf8t) {
			return NULL;
		}
		SDL_strlcpy(utf8t, utf8, utf8t_sz);
		SDL_strlcat(utf8t, "\n", utf8t_sz);
		start = end = utf8t;

		while ((end = SDL_strpbrk(start, "\n\r\f\v"))) {
			Uint32 *utf32;
			char *utf8ta;
			size_t sz;
			size_t ci;
			
			i = surfaces_count;
			surfaces_count++;
			if (surfaces_count == 1) {
				surfaces = SDL_calloc(surfaces_count, sizeof(SDL_Surface *));
			} else {
				surfaces = SDL_realloc(surfaces, surfaces_count * sizeof(SDL_Surface *));				
			}
			utf8ta = SDL_strndup(start, (size_t)(end - start));
			sz = SDL_utf8strlen(utf8ta);
			utf32 = SDL_iconv_utf8_ucs4(utf8ta);
			for (ci = 0; ci < sz; ci++) {
				utf32[ci] = SDL_Swap32BE(utf32[ci]);
			}
			surfaces[i] = renderer->render(renderer, utf32, sz, bg_fill);
			max_surface_width = SDL_max(max_surface_width, surfaces[i]->w);
			total_surface_height += surfaces[i]->h;
			SDL_free(utf32);
			SDL_free(utf8ta);
			start = end + 1;
		}
		
		ret = SDL_CreateSurface(max_surface_width, total_surface_height, SDL_PIXELFORMAT_ARGB8888);
		puts(SDL_GetError()); /* debug */
		for (i = 0; i < surfaces_count; i++) {
			SDL_BlitSurface(surfaces[i], NULL, ret, &rct);
			rct.y += surfaces[i]->h;
			SDL_DestroySurface(surfaces[i]);
		}
		SDL_free(utf8t);
		
		return ret;
	} else {
		return NULL;
	}
}

void WaylandToolkit_FreeTextRenderer(SDL_WaylandTextRenderer *renderer) {
	if (renderer) {
		renderer->free(renderer);
	}
}

void WaylandToolkit_SetTextRendererSize(SDL_WaylandTextRenderer *renderer, int pt_sz) {
	if (renderer) {
		renderer->set_pt_sz(renderer, pt_sz);
	}
}

void WaylandToolkit_SetTextRendererColor(SDL_WaylandTextRenderer *renderer, SDL_Color *color) {
	if (renderer) {
		renderer->set_color(renderer, color);
	}
}

#endif // SDL_VIDEO_DRIVER_WAYLAND

