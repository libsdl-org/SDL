/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_surface_c_h_
#define SDL_surface_c_h_

// Useful functions and variables from SDL_surface.c

#include "../SDL_list.h"

// Surface internal flags
typedef Uint32 SDL_SurfaceDataFlags;

#define SDL_INTERNAL_SURFACE_DONTFREE   0x00000001u /**< Surface is referenced internally */
#define SDL_INTERNAL_SURFACE_STACK      0x00000002u /**< Surface is allocated on the stack */
#define SDL_INTERNAL_SURFACE_RLEACCEL   0x00000004u /**< Surface is RLE encoded */

// Surface internal data definition
struct SDL_SurfaceData
{
    /** flags for this surface */
    SDL_SurfaceDataFlags flags;

    /** properties for this surface */
    SDL_PropertiesID props;

    /** detailed format for this surface */
    const SDL_PixelFormatDetails *format;

    /** Pixel colorspace */
    SDL_Colorspace colorspace;

    /** palette for indexed surfaces */
    SDL_Palette *palette;

    /** Alternate representation of images */
    int num_images;
    SDL_Surface **images;

    /** information needed for surfaces requiring locks */
    int locked;

    /** clipping information */
    SDL_Rect clip_rect;

    /** info for fast blit mapping to other surfaces */
    SDL_BlitMap map;
};

typedef struct SDL_InternalSurface
{
    SDL_Surface surface;
    SDL_SurfaceData internal;

} SDL_InternalSurface;

// Surface functions
extern bool SDL_SurfaceValid(SDL_Surface *surface);
extern void SDL_UpdateSurfaceLockFlag(SDL_Surface *surface);
extern float SDL_GetDefaultSDRWhitePoint(SDL_Colorspace colorspace);
extern float SDL_GetSurfaceSDRWhitePoint(SDL_Surface *surface, SDL_Colorspace colorspace);
extern float SDL_GetDefaultHDRHeadroom(SDL_Colorspace colorspace);
extern float SDL_GetSurfaceHDRHeadroom(SDL_Surface *surface, SDL_Colorspace colorspace);
extern SDL_Surface *SDL_GetSurfaceImage(SDL_Surface *surface, float display_scale);
extern bool SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

#endif // SDL_surface_c_h_
