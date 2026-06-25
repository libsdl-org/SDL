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

#if defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(HAVE_FCFT_H) 
#include "SDL_waylandtoolkit.h"
#include "SDL_waylandtoolkitfcft.h"
#include <fcft/fcft.h>

/* #define SDL_WAYLAND_TOOLKIT_FCFT_FINI */
#define SDL_WAYLAND_TOOLKIT_FCFT_DEBUG

/* fcft symbols */
typedef bool (*SDL_WaylandFcftInit)(enum fcft_log_colorize, bool, enum fcft_log_class);
typedef void (*SDL_WaylandFcftFini)(void);
typedef struct fcft_font *(*SDL_WaylandFcftFromName)(size_t count, const char *names[static count], const char *);
typedef void (*SDL_WaylandFcftDestroy)(struct fcft_font *);
typedef enum fcft_capabilities (*SDL_WaylandFcftCaps)(void);
typedef bool (*SDL_WaylandFcftKern)(struct fcft_font *, uint32_t, uint32_t, long *restrict, long *restrict);
typedef struct fcft_glyph *(*SDL_WaylandFcftRastChr)(struct fcft_font *, uint32_t, enum fcft_subpixel);
typedef struct fcft_text_run  *(*SDL_WaylandFcftRastRun)(struct fcft_font *, size_t len, const uint32_t text[static len], enum fcft_subpixel);
typedef void (*SDL_WaylandFcftDestroyRun)(struct fcft_text_run *);

/* pixman symbols */
typedef pixman_bool_t (*SDL_WaylandFcftPixmanImgUnref)(pixman_image_t *);
typedef pixman_image_t *(*SDL_WaylandFcftPixmanImgColFill)(const pixman_color_t *);
typedef pixman_image_t *(*SDL_WaylandFcftPixmanImgCreate)(pixman_format_code_t, int, int, uint32_t *, int);
typedef pixman_image_t *(*SDL_WaylandFcftPixmanImgComposite)(pixman_op_t, pixman_image_t *, pixman_image_t *, pixman_image_t *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t , int32_t, int32_t);
typedef pixman_format_code_t (*SDL_WaylandFcftPixmanImgGetFmt)(pixman_image_t *);

struct SDL_WaylandTextRendererFcft {
	SDL_WaylandTextRenderer base;

	/* font and color */
	struct fcft_font *cfont;
	pixman_image_t *color_fill;

	/* fcft library and functions */
	SDL_SharedObject *fcft_lib;
    SDL_WaylandFcftInit fcft_init;
    SDL_WaylandFcftFini fcft_fini;
    SDL_WaylandFcftFromName fcft_from_name;
    SDL_WaylandFcftDestroy fcft_destroy;
    SDL_WaylandFcftCaps fcft_capabilities;
    SDL_WaylandFcftKern fcft_kerning;
    SDL_WaylandFcftRastChr fcft_rasterize_char_utf32;
    SDL_WaylandFcftRastRun fcft_rasterize_text_run_utf32;
	SDL_WaylandFcftDestroyRun fcft_text_run_destroy;
	
	/* pixman functions */
    SDL_WaylandFcftPixmanImgUnref pixman_image_unref;
    SDL_WaylandFcftPixmanImgColFill pixman_image_create_solid_fill;
    SDL_WaylandFcftPixmanImgCreate pixman_image_create_bits_no_clear;
    SDL_WaylandFcftPixmanImgComposite pixman_image_composite32;
    SDL_WaylandFcftPixmanImgGetFmt pixman_image_get_format;
};

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

void WaylandToolkit_SetTextRendererSizeFcft(SDL_WaylandTextRenderer *renderer, int pt_sz) {
	struct SDL_WaylandTextRendererFcft *renderer_fcft;
	char *attrib;
	char *name;

	renderer_fcft = (struct SDL_WaylandTextRendererFcft *)renderer;

	if(renderer_fcft->cfont) {
		renderer_fcft->fcft_destroy(renderer_fcft->cfont);
	}
	
	name = "sans";	
	SDL_asprintf(&attrib, "size=%d", pt_sz);
    renderer_fcft->cfont = renderer_fcft->fcft_from_name(1, (const char **)&name, attrib);
    SDL_free(attrib);
}

static void WaylandToolkit_SetTextRendererColorFcft(SDL_WaylandTextRenderer *renderer, SDL_Color *color) {
	struct SDL_WaylandTextRendererFcft *renderer_fcft;
	pixman_color_t pcolor;

	renderer_fcft = (struct SDL_WaylandTextRendererFcft *)renderer;

	if (renderer_fcft->color_fill) {
		renderer_fcft->pixman_image_unref(renderer_fcft->color_fill);
	}
	
	pcolor.red = color->r * 257;
	pcolor.green = color->g * 257;
	pcolor.blue = color->b * 257;
	pcolor.alpha = color->a * 257;
	renderer_fcft->color_fill = renderer_fcft->pixman_image_create_solid_fill(&pcolor);
}

static void WaylandToolkit_FreeTextRendererFcft(SDL_WaylandTextRenderer *renderer) {
	struct SDL_WaylandTextRendererFcft *renderer_fcft;

	renderer_fcft = (struct SDL_WaylandTextRendererFcft *)renderer;

	if(renderer_fcft->cfont) {
		renderer_fcft->fcft_destroy(renderer_fcft->cfont); 
	}

	if(renderer_fcft->color_fill) {
		renderer_fcft->pixman_image_unref(renderer_fcft->color_fill); 
	}	

#ifdef SDL_WAYLAND_TOOLKIT_FCFT_FINI
	renderer_fcft->fcft_fini();
#endif

	if(renderer_fcft->fcft_lib) {
		SDL_UnloadObject(renderer_fcft->fcft_lib); 
	}

	SDL_free(renderer_fcft);
}

static SDL_Surface *WaylandToolkit_RenderTextFcft(SDL_WaylandTextRenderer *renderer, Uint32 *utf32, int sz, SDL_Color *bg_fill) {
	SDL_Surface *complete_surface;
	pixman_image_t *complete_surface_pixman;
	struct SDL_WaylandTextRendererFcft *renderer_fcft;
	SDL_Rect rct;
	enum fcft_capabilities caps;
	int i;

	complete_surface = NULL;
	renderer_fcft = (struct SDL_WaylandTextRendererFcft *)renderer;
	caps = renderer_fcft->fcft_capabilities();
	rct.x = 0;
	rct.y = 0;
	rct.h = 0;
	rct.w = 0;

	/* TODO: FCFT_CAPABILITY_GRAPHEME_SHAPING */
	if (caps & FCFT_CAPABILITY_TEXT_RUN_SHAPING) {
		struct fcft_text_run *run;
		
		run = renderer_fcft->fcft_rasterize_text_run_utf32(renderer_fcft->cfont, sz, utf32, FCFT_SUBPIXEL_DEFAULT);
		
		/* Calculate extents */
		for (i = 0; i < run->count; i++) {
			const struct fcft_glyph *glyph;

			glyph = run->glyphs[i];
			
			if (i > 0) {
				long x_kern;

				x_kern = 0;
				renderer_fcft->fcft_kerning(renderer_fcft->cfont, utf32[i - 1], utf32[i], &x_kern, NULL);
				rct.w += x_kern;
			}

			rct.w += glyph->advance.x;
			rct.h = SDL_max(rct.h, (renderer_fcft->cfont->ascent - glyph->y + glyph->height));
		}	

		/* Create target surface */
		complete_surface = SDL_CreateSurface(rct.w, rct.h, SDL_PIXELFORMAT_ARGB8888);
		complete_surface_pixman = renderer_fcft->pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, rct.w, rct.h, complete_surface->pixels, complete_surface->pitch);
		
		/* Background fill */
		if (bg_fill) {
			pixman_image_t *bg_fill_img;
			pixman_color_t bg_pcolor;

			bg_pcolor.red = bg_fill->r * 257;
			bg_pcolor.green = bg_fill->g * 257;
			bg_pcolor.blue = bg_fill->b * 257;
			bg_pcolor.alpha = bg_fill->a * 257;
			bg_fill_img = renderer_fcft->pixman_image_create_solid_fill(&bg_pcolor);
			renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, bg_fill_img, NULL, complete_surface_pixman, 0, 0, 0, 0, 0, 0, rct.w, rct.h);
			renderer_fcft->pixman_image_unref(bg_fill_img);
		}
	
		/* Blit glyphs on to target */
		for (i = 0; i < run->count; i++) {
			const struct fcft_glyph *glyph;

			glyph = run->glyphs[i];

			if (i > 0) {
				long x_kern;

				x_kern = 0;
				renderer_fcft->fcft_kerning(renderer_fcft->cfont, utf32[i - 1], utf32[i], &x_kern, NULL);
				rct.x += x_kern;
			}

			if (renderer_fcft->pixman_image_get_format(glyph->pix) == PIXMAN_a8r8g8b8) {
				renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, glyph->pix, NULL, complete_surface_pixman, 0, 0, 0, 0,  rct.x + glyph->x, renderer_fcft->cfont->ascent - glyph->y, glyph->width, glyph->height);
			} else {
				renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, renderer_fcft->color_fill, glyph->pix, complete_surface_pixman, 0, 0, 0, 0, rct.x + glyph->x, renderer_fcft->cfont->ascent - glyph->y, glyph->width, glyph->height);
			}

			rct.x += glyph->advance.x;
		}
		
		renderer_fcft->fcft_text_run_destroy(run);
	} else {
		for (i = 0; i < sz; i++) {
			const struct fcft_glyph *glyph;

			glyph = renderer_fcft->fcft_rasterize_char_utf32(renderer_fcft->cfont, utf32[i], FCFT_SUBPIXEL_DEFAULT);
			if (!glyph) {
				continue;
			}

			if (i > 0) {
				long x_kern;

				x_kern = 0;
				renderer_fcft->fcft_kerning(renderer_fcft->cfont, utf32[i - 1], utf32[i], &x_kern, NULL);
				rct.w += x_kern;
			}

			rct.w += glyph->advance.x;
			rct.h = SDL_max(rct.h, (renderer_fcft->cfont->ascent - glyph->y + glyph->height));
		}

		complete_surface = SDL_CreateSurface(rct.w, rct.h, SDL_PIXELFORMAT_ARGB8888);
		complete_surface_pixman = renderer_fcft->pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, rct.w, rct.h, complete_surface->pixels, complete_surface->pitch);
		
		if (bg_fill) {
			pixman_image_t *bg_fill_img;
			pixman_color_t bg_pcolor;

			bg_pcolor.red = bg_fill->r * 257;
			bg_pcolor.green = bg_fill->g * 257;
			bg_pcolor.blue = bg_fill->b * 257;
			bg_pcolor.alpha = bg_fill->a * 257;
			bg_fill_img = renderer_fcft->pixman_image_create_solid_fill(&bg_pcolor);
			renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, bg_fill_img, NULL, complete_surface_pixman, 0, 0, 0, 0, 0, 0, rct.w, rct.h);
			renderer_fcft->pixman_image_unref(bg_fill_img);
		}
	
		for (i = 0; i < sz; i++) {
			const struct fcft_glyph *glyph;

			glyph = renderer_fcft->fcft_rasterize_char_utf32(renderer_fcft->cfont, utf32[i], FCFT_SUBPIXEL_DEFAULT);
			if (!glyph) {
				continue;
			}

			if (i > 0) {
				long x_kern;

				x_kern = 0;
				renderer_fcft->fcft_kerning(renderer_fcft->cfont, utf32[i - 1], utf32[i], &x_kern, NULL);
				rct.x += x_kern;
			}

			if (renderer_fcft->pixman_image_get_format(glyph->pix) == PIXMAN_a8r8g8b8) {
				renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, glyph->pix, NULL, complete_surface_pixman, 0, 0, 0, 0,  rct.x + glyph->x, renderer_fcft->cfont->ascent - glyph->y, glyph->width, glyph->height);
			} else {
				renderer_fcft->pixman_image_composite32(PIXMAN_OP_OVER, renderer_fcft->color_fill, glyph->pix, complete_surface_pixman, 0, 0, 0, 0, rct.x + glyph->x, renderer_fcft->cfont->ascent - glyph->y, glyph->width, glyph->height);
			}

			rct.x += glyph->advance.x;
		}

		renderer_fcft->pixman_image_unref(complete_surface_pixman); 
	}

	return complete_surface;
}

SDL_WaylandTextRenderer *WaylandToolkit_CreateTextRendererFcft() {
	struct SDL_WaylandTextRendererFcft *renderer;
	SDL_Color default_col;

	// Alloc renderer struct
	renderer = (struct SDL_WaylandTextRendererFcft *)SDL_malloc(sizeof(struct SDL_WaylandTextRendererFcft)); 
	if (!renderer) {
		return NULL;
	}

	// Symbols 
#ifdef SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_FCFT	
	#define SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(x, n, t) x = ((t)SDL_LoadFunction(renderer->fcft_lib, n)); if (!x) { SDL_UnloadObject(renderer->fcft_lib); SDL_free(renderer); return NULL; }

	renderer->fcft_lib = SDL_LoadObject(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_FCFT);
	if (!renderer->fcft_lib) {
		SDL_free(renderer);
		return NULL;	
	}

	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_init, "fcft_init", SDL_WaylandFcftInit);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_fini, "fcft_fini", SDL_WaylandFcftFini);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_from_name, "fcft_from_name", SDL_WaylandFcftFromName);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_destroy, "fcft_destroy", SDL_WaylandFcftDestroy);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_capabilities, "fcft_capabilities", SDL_WaylandFcftCaps);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_kerning, "fcft_kerning", SDL_WaylandFcftKern);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_rasterize_char_utf32, "fcft_rasterize_char_utf32", SDL_WaylandFcftRastChr);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_rasterize_text_run_utf32, "fcft_rasterize_text_run_utf32", SDL_WaylandFcftRastRun);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->fcft_text_run_destroy, "fcft_text_run_destroy", SDL_WaylandFcftDestroyRun);

	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->pixman_image_get_format, "pixman_image_get_format", SDL_WaylandFcftPixmanImgGetFmt);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->pixman_image_create_bits_no_clear, "pixman_image_create_bits_no_clear", SDL_WaylandFcftPixmanImgCreate);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->pixman_image_unref, "pixman_image_unref", SDL_WaylandFcftPixmanImgUnref);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->pixman_image_create_solid_fill, "pixman_image_create_solid_fill", SDL_WaylandFcftPixmanImgColFill);
	SDL_WAYLAND_TOOLKIT_FCFT_LOAD_SYM(renderer->pixman_image_composite32, "pixman_image_composite32", SDL_WaylandFcftPixmanImgComposite);
#else 
	renderer->fcft_lib = NULL;
	renderer->fcft_init = fcft_init;
	renderer->fcft_fini = fcft_fini;
	renderer->fcft_from_name = fcft_from_name;
	renderer->fcft_destroy = fcft_destroy;
	renderer->fcft_capabilities = fcft_capabilities;	
	renderer->fcft_rasterize_char_utf32 = fcft_rasterize_char_utf32;
	renderer->fcft_kerning = fcft_kerning;
	renderer->fcft_rasterize_text_run_utf32 = fcft_rasterize_text_run_utf32;
	renderer->fcft_text_run_destroy = fcft_text_run_destroy;

	renderer->pixman_image_create_bits_no_clear = pixman_image_create_bits_no_clear;
	renderer->pixman_image_unref = pixman_image_unref;	
	renderer->pixman_image_create_solid_fill = pixman_image_create_solid_fill;	
	renderer->pixman_image_composite32 = pixman_image_composite32;
	renderer->pixman_image_get_format = pixman_image_get_format;
#endif

	// Init
#ifdef SDL_WAYLAND_TOOLKIT_FCFT_DEBUG
	renderer->fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_DEBUG);
#else
	renderer->fcft_init(FCFT_LOG_COLORIZE_NEVER, false, FCFT_LOG_CLASS_NONE);
#endif
	renderer->color_fill = NULL;
	renderer->cfont = NULL;
	default_col.r = 255;
	default_col.g = 255;
	default_col.b = 255;
	default_col.a = 255;
	WaylandToolkit_SetTextRendererSizeFcft((SDL_WaylandTextRenderer *)renderer, 11);
	WaylandToolkit_SetTextRendererColorFcft((SDL_WaylandTextRenderer *)renderer, &default_col);

	// Object vtable
	renderer->base.free = WaylandToolkit_FreeTextRendererFcft;
	renderer->base.set_color = WaylandToolkit_SetTextRendererColorFcft;
	renderer->base.set_pt_sz = WaylandToolkit_SetTextRendererSizeFcft;
	renderer->base.render = WaylandToolkit_RenderTextFcft;

	return (SDL_WaylandTextRenderer *)renderer;
}

#endif // defined(SDL_VIDEO_DRIVER_WAYLAND) && defined(HAVE_FCFT_H) 
