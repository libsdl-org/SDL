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

#ifndef SDL_waylandtoolkit_h_
#define SDL_waylandtoolkit_h_

#ifdef SDL_VIDEO_DRIVER_WAYLAND

/* TEXT RENDERING */
typedef struct SDL_WaylandTextRenderer {
	SDL_Surface *(*render)(struct SDL_WaylandTextRenderer *renderer, Uint32 *utf32, int sz, SDL_Color *bg_fill);
    void (*set_color)(struct SDL_WaylandTextRenderer *renderer, SDL_Color *color);
    void (*set_pt_sz)(struct SDL_WaylandTextRenderer *renderer, int pt_sz);
	void (*free)(struct SDL_WaylandTextRenderer *renderer);
} SDL_WaylandTextRenderer;

extern SDL_WaylandTextRenderer *WaylandToolkit_CreateTextRenderer();
extern void WaylandToolkit_SetTextRendererSize(SDL_WaylandTextRenderer *renderer, int pt_sz);
extern void WaylandToolkit_SetTextRendererColor(SDL_WaylandTextRenderer *renderer, SDL_Color *color);
extern SDL_Surface *WaylandToolkit_RenderText(SDL_WaylandTextRenderer *renderer, char *utf8, SDL_Color *bg_fill);
extern void WaylandToolkit_FreeTextRenderer(SDL_WaylandTextRenderer *renderer);

#endif // SDL_VIDEO_DRIVER_WAYLAND

#endif // SDL_waylandtoolkit_h_
