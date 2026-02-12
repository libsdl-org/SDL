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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include "SDL_waylandtoolkit.h"
#include "SDL_waylandtoolkitbitmap.h"
#include "SDL_waylandtoolkitfont.h"

typedef struct {
	SDL_WaylandTextRenderer base;

	SDL_HashTable *map;
	SDL_Palette *palette;
	SDL_Color palette_colors[2];
	int px_sz;
} SDL_WaylandTextRendererBitmap;

void WaylandToolkit_FreeTextRendererBitmap(SDL_WaylandTextRenderer *renderer) {
	SDL_WaylandTextRendererBitmap *renderer_bmp;

	renderer_bmp = (SDL_WaylandTextRendererBitmap *)renderer;

	if (renderer_bmp->map) {
		SDL_DestroyHashTable(renderer_bmp->map);
	}

	if (renderer_bmp->palette) {
		SDL_DestroyPalette(renderer_bmp->palette);
	}

	SDL_free(renderer_bmp);
}

static SDL_Surface *WaylandToolkit_RenderTextBitmap(SDL_WaylandTextRenderer *renderer, Uint32 *utf32, int sz, SDL_Color *bg_fill) {
	SDL_WaylandTextRendererBitmap *renderer_bmp;
	SDL_Surface *complete_surface;
	SDL_Rect rct;
	SDL_ScaleMode mode;
	int i;
	int csz;
	#define COMPLETE_SURFACE_FORMAT SDL_PIXELFORMAT_ARGB8888
	
	/* Get renderer */
	renderer_bmp = (SDL_WaylandTextRendererBitmap *)renderer;		
	
	/* Zero out coords of rect */
	rct.y = 0;
	rct.x = 0;
	
	/* Calculate surface width */
	csz = renderer_bmp->px_sz * sz;
	if (csz <= 0) {
		csz = 1;
	}
	
	/* Create surface for rendering */
	complete_surface = SDL_CreateSurface(csz, renderer_bmp->px_sz, COMPLETE_SURFACE_FORMAT);
	if (!complete_surface) {
		return NULL;
	}	
	
	/* Background fill */
	if (bg_fill) {
		SDL_Rect rect;
		Uint32 color;
		
		rect.x = rect.y = 0;
		rect.w = csz;
		rect.h = renderer_bmp->px_sz;
		color = SDL_MapRGBA(SDL_GetPixelFormatDetails(COMPLETE_SURFACE_FORMAT), NULL, bg_fill->r, bg_fill->g, bg_fill->b, bg_fill->a);
		SDL_FillSurfaceRect(complete_surface, &rect, color);
	}
	
	/* Set scaling mode */
	if (renderer_bmp->px_sz <= 8) {
		mode = SDL_SCALEMODE_LINEAR;
	} else {
		mode = SDL_SCALEMODE_PIXELART;
	}

	/* Render */
	for (i = 0; i < sz; i++) {
		SDL_Surface *char_surface;

		if (!SDL_FindInHashTable(renderer_bmp->map, (const void *)(uintptr_t)(utf32[i]), (const void **)&char_surface)) {
			SDL_FindInHashTable(renderer_bmp->map, (const void *)(uintptr_t)('?'), (const void **)&char_surface);
		}
		
		if (renderer_bmp->px_sz == 8) {	
			SDL_BlitSurface(char_surface, NULL, complete_surface, &rct);
		} else {
			rct.w = renderer_bmp->px_sz;
			rct.h = renderer_bmp->px_sz;
			SDL_BlitSurfaceScaled(char_surface, NULL, complete_surface, &rct, mode);
		}
		rct.x += renderer_bmp->px_sz;
	}
	
	return complete_surface;
}

void WaylandToolkit_SetTextRendererSizeBitmap(SDL_WaylandTextRenderer *renderer, int pt_sz) {
	SDL_WaylandTextRendererBitmap *renderer_bmp;
	double px_sz;

	renderer_bmp = (SDL_WaylandTextRendererBitmap *)renderer;
	px_sz = pt_sz * 0.75; /* 96 DPI assumed */
	renderer_bmp->px_sz = (int)px_sz;
}

static void WaylandToolkit_SetTextRendererColorBitmap(SDL_WaylandTextRenderer *renderer, SDL_Color *color) {
	SDL_WaylandTextRendererBitmap *renderer_bmp;

	renderer_bmp = (SDL_WaylandTextRendererBitmap *)renderer;

	renderer_bmp->palette_colors[1] = *color;
}

static void WaylandToolkit_InsertCharIntoBitmapMap(SDL_HashTable *map, SDL_Palette *palette, char *bmp, long codepoint) {
	SDL_Surface *char_surface;

	char_surface = SDL_CreateSurfaceFrom(8, 8, SDL_PIXELFORMAT_INDEX1LSB, bmp, 1);
	if (char_surface) {
		SDL_SetSurfacePalette(char_surface, palette);
		SDL_InsertIntoHashTable(map, (const void *)(uintptr_t)codepoint, (void *)char_surface, false);
	} 
}

SDL_WaylandTextRenderer *WaylandToolkit_CreateTextRendererBitmap() {
	SDL_WaylandTextRendererBitmap *renderer;
	int i;

	/* Alloc */
	renderer = (SDL_WaylandTextRendererBitmap *)SDL_malloc(sizeof(SDL_WaylandTextRendererBitmap)); 
	if (!renderer) {
		return NULL;
	}
	renderer->px_sz = 8;

	/* Char map */
	renderer->map = SDL_CreateHashTable(606, false, SDL_HashID, SDL_KeyMatchID, NULL, NULL);
	if (!renderer->map) {
		SDL_free(renderer);
		return NULL;
	}

	/* Palette */
	renderer->palette = SDL_CreatePalette(2);
	if (!renderer->palette) {
		SDL_DestroyHashTable(renderer->map);
		SDL_free(renderer);
		return NULL;
	}
	renderer->palette_colors[0].r = 0;
	renderer->palette_colors[0].g = 0;
	renderer->palette_colors[0].b = 0;
	renderer->palette_colors[0].a = 0;
	renderer->palette_colors[1].r = 255;
	renderer->palette_colors[1].g = 255;
	renderer->palette_colors[1].b = 255;
	renderer->palette_colors[1].a = 255;
	SDL_SetPaletteColors(renderer->palette, renderer->palette_colors, 0, 2);

	/* Populate char map */
	for (i = 0; i < 128; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_basic[i], i);
	}
	for (i = 0; i < 32; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_control[i], 128 + i);
	}
	for (i = 0; i < 96; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_ext_latin[i], 160 + i);
	}
	for (i = 0; i < 58; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_greek[i], 912 + i);
	}
	WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_misc[0], 0x20A7);
	WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_misc[1], 0x0192);
	WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_misc[4], 0x2310);
	for (i = 0; i < 128; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_box[i], 9472 + i);
	}
	for (i = 0; i < 32; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_block[i], 0x2580 + i);
	}
	for (i = 0; i < 96; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_hiragana[i], 0x3040 + i);
	}
	for (i = 0; i < 26; i++) {
		WaylandToolkit_InsertCharIntoBitmapMap(renderer->map, renderer->palette, font8x8_sga[i], 0xE541 + i);
	}

	/* Functions */
	renderer->base.free = WaylandToolkit_FreeTextRendererBitmap;
	renderer->base.render = WaylandToolkit_RenderTextBitmap;
	renderer->base.set_color = WaylandToolkit_SetTextRendererColorBitmap;
	renderer->base.set_pt_sz = WaylandToolkit_SetTextRendererSizeBitmap;

	return (SDL_WaylandTextRenderer *)renderer;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND
