/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_RLEaccel_c.h"
#include "SDL_pixels_c.h"

/* Private routines */
static int
SDL_ConvertPixels_YUV_to_ARGB8888(int width, int height,
        Uint32 src_format, const void *src, 
        void *dst, int dst_pitch);

static int 
SDL_ConvertPixels_ARGB8888_to_YUV(int width, int height, 
        const void *src, int src_pitch,
        Uint32 dst_format, void *dst);

/* Check to make sure we can safely check multiplication of surface w and pitch and it won't overflow size_t */
SDL_COMPILE_TIME_ASSERT(surface_size_assumptions,
    sizeof(int) == sizeof(Sint32) && sizeof(size_t) >= sizeof(Sint32));

/* Public routines */

/*
 * Create an empty RGB surface of the appropriate depth using the given
 * enum SDL_PIXELFORMAT_* format
 */
SDL_Surface *
SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height, int depth,
                               Uint32 format)
{
    SDL_Surface *surface;

    /* The flags are no longer used, make the compiler happy */
    (void)flags;

    /* Allocate the surface */
    surface = (SDL_Surface *) SDL_calloc(1, sizeof(*surface));
    if (surface == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    surface->format = SDL_AllocFormat(format);
    if (!surface->format) {
        SDL_FreeSurface(surface);
        return NULL;
    }
    surface->w = width;
    surface->h = height;
    surface->pitch = SDL_CalculatePitch(surface);
    SDL_SetClipRect(surface, NULL);

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format->format)) {
        SDL_Palette *palette =
            SDL_AllocPalette((1 << surface->format->BitsPerPixel));
        if (!palette) {
            SDL_FreeSurface(surface);
            return NULL;
        }
        if (palette->ncolors == 2) {
            /* Create a black and white bitmap palette */
            palette->colors[0].r = 0xFF;
            palette->colors[0].g = 0xFF;
            palette->colors[0].b = 0xFF;
            palette->colors[1].r = 0x00;
            palette->colors[1].g = 0x00;
            palette->colors[1].b = 0x00;
        }
        SDL_SetSurfacePalette(surface, palette);
        SDL_FreePalette(palette);
    }

    /* Get the pixels */
    if (surface->w && surface->h) {
        /* Assumptions checked in surface_size_assumptions assert above */
        Sint64 size = ((Sint64)surface->h * surface->pitch);
        if (size < 0 || size > SDL_MAX_SINT32) {
            /* Overflow... */
            SDL_FreeSurface(surface);
            SDL_OutOfMemory();
            return NULL;
        }

        surface->pixels = SDL_malloc((size_t)size);
        if (!surface->pixels) {
            SDL_FreeSurface(surface);
            SDL_OutOfMemory();
            return NULL;
        }
        /* This is important for bitmaps */
        SDL_memset(surface->pixels, 0, surface->h * surface->pitch);
    }

    /* Allocate an empty mapping */
    surface->map = SDL_AllocBlitMap();
    if (!surface->map) {
        SDL_FreeSurface(surface);
        return NULL;
    }

    /* By default surface with an alpha mask are set up for blending */
    if (surface->format->Amask) {
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    }

    /* The surface is ready to go */
    surface->refcount = 1;
    return surface;
}

/*
 * Create an empty RGB surface of the appropriate depth
 */
SDL_Surface *
SDL_CreateRGBSurface(Uint32 flags,
                     int width, int height, int depth,
                     Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    Uint32 format;

    /* Get the pixel format */
    format = SDL_MasksToPixelFormatEnum(depth, Rmask, Gmask, Bmask, Amask);
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_SetError("Unknown pixel format");
        return NULL;
    }

    return SDL_CreateRGBSurfaceWithFormat(flags, width, height, depth, format);
}

/*
 * Create an RGB surface from an existing memory buffer
 */
SDL_Surface *
SDL_CreateRGBSurfaceFrom(void *pixels,
                         int width, int height, int depth, int pitch,
                         Uint32 Rmask, Uint32 Gmask, Uint32 Bmask,
                         Uint32 Amask)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurface(0, 0, 0, depth, Rmask, Gmask, Bmask, Amask);
    if (surface != NULL) {
        surface->flags |= SDL_PREALLOC;
        surface->pixels = pixels;
        surface->w = width;
        surface->h = height;
        surface->pitch = pitch;
        SDL_SetClipRect(surface, NULL);
    }
    return surface;
}

/*
 * Create an RGB surface from an existing memory buffer using the given given
 * enum SDL_PIXELFORMAT_* format
 */
SDL_Surface *
SDL_CreateRGBSurfaceWithFormatFrom(void *pixels,
                         int width, int height, int depth, int pitch,
                         Uint32 format)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceWithFormat(0, 0, 0, depth, format);
    if (surface != NULL) {
        surface->flags |= SDL_PREALLOC;
        surface->pixels = pixels;
        surface->w = width;
        surface->h = height;
        surface->pitch = pitch;
        SDL_SetClipRect(surface, NULL);
    }
    return surface;
}

int
SDL_SetSurfacePalette(SDL_Surface * surface, SDL_Palette * palette)
{
    if (!surface) {
        return SDL_SetError("SDL_SetSurfacePalette() passed a NULL surface");
    }
    if (SDL_SetPixelFormatPalette(surface->format, palette) < 0) {
        return -1;
    }
    SDL_InvalidateMap(surface->map);

    return 0;
}

int
SDL_SetSurfaceRLE(SDL_Surface * surface, int flag)
{
    int flags;

    if (!surface) {
        return -1;
    }

    flags = surface->map->info.flags;
    if (flag) {
        surface->map->info.flags |= SDL_COPY_RLE_DESIRED;
    } else {
        surface->map->info.flags &= ~SDL_COPY_RLE_DESIRED;
    }
    if (surface->map->info.flags != flags) {
        SDL_InvalidateMap(surface->map);
    }
    return 0;
}

int
SDL_SetColorKey(SDL_Surface * surface, int flag, Uint32 key)
{
    int flags;

    if (!surface) {
        return SDL_InvalidParamError("surface");
    }

    if (surface->format->palette && key >= ((Uint32) surface->format->palette->ncolors)) {
        return SDL_InvalidParamError("key");
    }

    if (flag & SDL_RLEACCEL) {
        SDL_SetSurfaceRLE(surface, 1);
    }

    flags = surface->map->info.flags;
    if (flag) {
        surface->map->info.flags |= SDL_COPY_COLORKEY;
        surface->map->info.colorkey = key;
        if (surface->format->palette) {
            surface->format->palette->colors[surface->map->info.colorkey].a = SDL_ALPHA_TRANSPARENT;
            ++surface->format->palette->version;
            if (!surface->format->palette->version) {
                surface->format->palette->version = 1;
            }
        }
    } else {
        if (surface->format->palette) {
            surface->format->palette->colors[surface->map->info.colorkey].a = SDL_ALPHA_OPAQUE;
            ++surface->format->palette->version;
            if (!surface->format->palette->version) {
                surface->format->palette->version = 1;
            }
        }
        surface->map->info.flags &= ~SDL_COPY_COLORKEY;
    }
    if (surface->map->info.flags != flags) {
        SDL_InvalidateMap(surface->map);
    }

    return 0;
}

int
SDL_GetColorKey(SDL_Surface * surface, Uint32 * key)
{
    if (!surface) {
        return -1;
    }

    if (!(surface->map->info.flags & SDL_COPY_COLORKEY)) {
        return -1;
    }

    if (key) {
        *key = surface->map->info.colorkey;
    }
    return 0;
}

/* This is a fairly slow function to switch from colorkey to alpha */
static void
SDL_ConvertColorkeyToAlpha(SDL_Surface * surface)
{
    int x, y;

    if (!surface) {
        return;
    }

    if (!(surface->map->info.flags & SDL_COPY_COLORKEY) ||
        !surface->format->Amask) {
        return;
    }

    SDL_LockSurface(surface);

    switch (surface->format->BytesPerPixel) {
    case 2:
        {
            Uint16 *row, *spot;
            Uint16 ckey = (Uint16) surface->map->info.colorkey;
            Uint16 mask = (Uint16) (~surface->format->Amask);

            /* Ignore alpha in colorkey comparison */
            ckey &= mask;
            row = (Uint16 *) surface->pixels;
            for (y = surface->h; y--;) {
                spot = row;
                for (x = surface->w; x--;) {
                    if ((*spot & mask) == ckey) {
                        *spot &= mask;
                    }
                    ++spot;
                }
                row += surface->pitch / 2;
            }
        }
        break;
    case 3:
        /* FIXME */
        break;
    case 4:
        {
            Uint32 *row, *spot;
            Uint32 ckey = surface->map->info.colorkey;
            Uint32 mask = ~surface->format->Amask;

            /* Ignore alpha in colorkey comparison */
            ckey &= mask;
            row = (Uint32 *) surface->pixels;
            for (y = surface->h; y--;) {
                spot = row;
                for (x = surface->w; x--;) {
                    if ((*spot & mask) == ckey) {
                        *spot &= mask;
                    }
                    ++spot;
                }
                row += surface->pitch / 4;
            }
        }
        break;
    }

    SDL_UnlockSurface(surface);

    SDL_SetColorKey(surface, 0, 0);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
}

int
SDL_SetSurfaceColorMod(SDL_Surface * surface, Uint8 r, Uint8 g, Uint8 b)
{
    int flags;

    if (!surface) {
        return -1;
    }

    surface->map->info.r = r;
    surface->map->info.g = g;
    surface->map->info.b = b;

    flags = surface->map->info.flags;
    if (r != 0xFF || g != 0xFF || b != 0xFF) {
        surface->map->info.flags |= SDL_COPY_MODULATE_COLOR;
    } else {
        surface->map->info.flags &= ~SDL_COPY_MODULATE_COLOR;
    }
    if (surface->map->info.flags != flags) {
        SDL_InvalidateMap(surface->map);
    }
    return 0;
}


int
SDL_GetSurfaceColorMod(SDL_Surface * surface, Uint8 * r, Uint8 * g, Uint8 * b)
{
    if (!surface) {
        return -1;
    }

    if (r) {
        *r = surface->map->info.r;
    }
    if (g) {
        *g = surface->map->info.g;
    }
    if (b) {
        *b = surface->map->info.b;
    }
    return 0;
}

int
SDL_SetSurfaceAlphaMod(SDL_Surface * surface, Uint8 alpha)
{
    int flags;

    if (!surface) {
        return -1;
    }

    surface->map->info.a = alpha;

    flags = surface->map->info.flags;
    if (alpha != 0xFF) {
        surface->map->info.flags |= SDL_COPY_MODULATE_ALPHA;
    } else {
        surface->map->info.flags &= ~SDL_COPY_MODULATE_ALPHA;
    }
    if (surface->map->info.flags != flags) {
        SDL_InvalidateMap(surface->map);
    }
    return 0;
}

int
SDL_GetSurfaceAlphaMod(SDL_Surface * surface, Uint8 * alpha)
{
    if (!surface) {
        return -1;
    }

    if (alpha) {
        *alpha = surface->map->info.a;
    }
    return 0;
}

int
SDL_SetSurfaceBlendMode(SDL_Surface * surface, SDL_BlendMode blendMode)
{
    int flags, status;

    if (!surface) {
        return -1;
    }

    status = 0;
    flags = surface->map->info.flags;
    surface->map->info.flags &=
        ~(SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD);
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
        break;
    case SDL_BLENDMODE_BLEND:
        surface->map->info.flags |= SDL_COPY_BLEND;
        break;
    case SDL_BLENDMODE_ADD:
        surface->map->info.flags |= SDL_COPY_ADD;
        break;
    case SDL_BLENDMODE_MOD:
        surface->map->info.flags |= SDL_COPY_MOD;
        break;
    default:
        status = SDL_Unsupported();
        break;
    }

    if (surface->map->info.flags != flags) {
        SDL_InvalidateMap(surface->map);
    }

    return status;
}

int
SDL_GetSurfaceBlendMode(SDL_Surface * surface, SDL_BlendMode *blendMode)
{
    if (!surface) {
        return -1;
    }

    if (!blendMode) {
        return 0;
    }

    switch (surface->map->
            info.flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD)) {
    case SDL_COPY_BLEND:
        *blendMode = SDL_BLENDMODE_BLEND;
        break;
    case SDL_COPY_ADD:
        *blendMode = SDL_BLENDMODE_ADD;
        break;
    case SDL_COPY_MOD:
        *blendMode = SDL_BLENDMODE_MOD;
        break;
    default:
        *blendMode = SDL_BLENDMODE_NONE;
        break;
    }
    return 0;
}

SDL_bool
SDL_SetClipRect(SDL_Surface * surface, const SDL_Rect * rect)
{
    SDL_Rect full_rect;

    /* Don't do anything if there's no surface to act on */
    if (!surface) {
        return SDL_FALSE;
    }

    /* Set up the full surface rectangle */
    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = surface->w;
    full_rect.h = surface->h;

    /* Set the clipping rectangle */
    if (!rect) {
        surface->clip_rect = full_rect;
        return SDL_TRUE;
    }
    return SDL_IntersectRect(rect, &full_rect, &surface->clip_rect);
}

void
SDL_GetClipRect(SDL_Surface * surface, SDL_Rect * rect)
{
    if (surface && rect) {
        *rect = surface->clip_rect;
    }
}

/*
 * Set up a blit between two surfaces -- split into three parts:
 * The upper part, SDL_UpperBlit(), performs clipping and rectangle
 * verification.  The lower part is a pointer to a low level
 * accelerated blitting function.
 *
 * These parts are separated out and each used internally by this
 * library in the optimimum places.  They are exported so that if
 * you know exactly what you are doing, you can optimize your code
 * by calling the one(s) you need.
 */
int
SDL_LowerBlit(SDL_Surface * src, SDL_Rect * srcrect,
              SDL_Surface * dst, SDL_Rect * dstrect)
{
    /* Check to make sure the blit mapping is valid */
    if ((src->map->dst != dst) ||
        (dst->format->palette &&
         src->map->dst_palette_version != dst->format->palette->version) ||
        (src->format->palette &&
         src->map->src_palette_version != src->format->palette->version)) {
        if (SDL_MapSurface(src, dst) < 0) {
            return (-1);
        }
        /* just here for debugging */
/*         printf */
/*             ("src = 0x%08X src->flags = %08X src->map->info.flags = %08x\ndst = 0x%08X dst->flags = %08X dst->map->info.flags = %08X\nsrc->map->blit = 0x%08x\n", */
/*              src, dst->flags, src->map->info.flags, dst, dst->flags, */
/*              dst->map->info.flags, src->map->blit); */
    }
    return (src->map->blit(src, srcrect, dst, dstrect));
}


int
SDL_UpperBlit(SDL_Surface * src, const SDL_Rect * srcrect,
              SDL_Surface * dst, SDL_Rect * dstrect)
{
    SDL_Rect fulldst;
    int srcx, srcy, w, h;

    /* Make sure the surfaces aren't locked */
    if (!src || !dst) {
        return SDL_SetError("SDL_UpperBlit: passed a NULL surface");
    }
    if (src->locked || dst->locked) {
        return SDL_SetError("Surfaces must not be locked during blit");
    }

    /* If the destination rectangle is NULL, use the entire dest surface */
    if (dstrect == NULL) {
        fulldst.x = fulldst.y = 0;
        fulldst.w = dst->w;
        fulldst.h = dst->h;
        dstrect = &fulldst;
    }

    /* clip the source rectangle to the source surface */
    if (srcrect) {
        int maxw, maxh;

        srcx = srcrect->x;
        w = srcrect->w;
        if (srcx < 0) {
            w += srcx;
            dstrect->x -= srcx;
            srcx = 0;
        }
        maxw = src->w - srcx;
        if (maxw < w)
            w = maxw;

        srcy = srcrect->y;
        h = srcrect->h;
        if (srcy < 0) {
            h += srcy;
            dstrect->y -= srcy;
            srcy = 0;
        }
        maxh = src->h - srcy;
        if (maxh < h)
            h = maxh;

    } else {
        srcx = srcy = 0;
        w = src->w;
        h = src->h;
    }

    /* clip the destination rectangle against the clip rectangle */
    {
        SDL_Rect *clip = &dst->clip_rect;
        int dx, dy;

        dx = clip->x - dstrect->x;
        if (dx > 0) {
            w -= dx;
            dstrect->x += dx;
            srcx += dx;
        }
        dx = dstrect->x + w - clip->x - clip->w;
        if (dx > 0)
            w -= dx;

        dy = clip->y - dstrect->y;
        if (dy > 0) {
            h -= dy;
            dstrect->y += dy;
            srcy += dy;
        }
        dy = dstrect->y + h - clip->y - clip->h;
        if (dy > 0)
            h -= dy;
    }

    /* Switch back to a fast blit if we were previously stretching */
    if (src->map->info.flags & SDL_COPY_NEAREST) {
        src->map->info.flags &= ~SDL_COPY_NEAREST;
        SDL_InvalidateMap(src->map);
    }

    if (w > 0 && h > 0) {
        SDL_Rect sr;
        sr.x = srcx;
        sr.y = srcy;
        sr.w = dstrect->w = w;
        sr.h = dstrect->h = h;
        return SDL_LowerBlit(src, &sr, dst, dstrect);
    }
    dstrect->w = dstrect->h = 0;
    return 0;
}

int
SDL_UpperBlitScaled(SDL_Surface * src, const SDL_Rect * srcrect,
              SDL_Surface * dst, SDL_Rect * dstrect)
{
    double src_x0, src_y0, src_x1, src_y1;
    double dst_x0, dst_y0, dst_x1, dst_y1;
    SDL_Rect final_src, final_dst;
    double scaling_w, scaling_h;
    int src_w, src_h;
    int dst_w, dst_h;

    /* Make sure the surfaces aren't locked */
    if (!src || !dst) {
        return SDL_SetError("SDL_UpperBlitScaled: passed a NULL surface");
    }
    if (src->locked || dst->locked) {
        return SDL_SetError("Surfaces must not be locked during blit");
    }

    if (NULL == srcrect) {
        src_w = src->w;
        src_h = src->h;
    } else {
        src_w = srcrect->w;
        src_h = srcrect->h;
    }

    if (NULL == dstrect) {
        dst_w = dst->w;
        dst_h = dst->h;
    } else {
        dst_w = dstrect->w;
        dst_h = dstrect->h;
    }

    if (dst_w == src_w && dst_h == src_h) {
        /* No scaling, defer to regular blit */
        return SDL_BlitSurface(src, srcrect, dst, dstrect);
    }

    scaling_w = (double)dst_w / src_w;
    scaling_h = (double)dst_h / src_h;

    if (NULL == dstrect) {
        dst_x0 = 0;
        dst_y0 = 0;
        dst_x1 = dst_w - 1;
        dst_y1 = dst_h - 1;
    } else {
        dst_x0 = dstrect->x;
        dst_y0 = dstrect->y;
        dst_x1 = dst_x0 + dst_w - 1;
        dst_y1 = dst_y0 + dst_h - 1;
    }

    if (NULL == srcrect) {
        src_x0 = 0;
        src_y0 = 0;
        src_x1 = src_w - 1;
        src_y1 = src_h - 1;
    } else {
        src_x0 = srcrect->x;
        src_y0 = srcrect->y;
        src_x1 = src_x0 + src_w - 1;
        src_y1 = src_y0 + src_h - 1;

        /* Clip source rectangle to the source surface */

        if (src_x0 < 0) {
            dst_x0 -= src_x0 * scaling_w;
            src_x0 = 0;
        }

        if (src_x1 >= src->w) {
            dst_x1 -= (src_x1 - src->w + 1) * scaling_w;
            src_x1 = src->w - 1;
        }

        if (src_y0 < 0) {
            dst_y0 -= src_y0 * scaling_h;
            src_y0 = 0;
        }

        if (src_y1 >= src->h) {
            dst_y1 -= (src_y1 - src->h + 1) * scaling_h;
            src_y1 = src->h - 1;
        }
    }

    /* Clip destination rectangle to the clip rectangle */

    /* Translate to clip space for easier calculations */
    dst_x0 -= dst->clip_rect.x;
    dst_x1 -= dst->clip_rect.x;
    dst_y0 -= dst->clip_rect.y;
    dst_y1 -= dst->clip_rect.y;

    if (dst_x0 < 0) {
        src_x0 -= dst_x0 / scaling_w;
        dst_x0 = 0;
    }

    if (dst_x1 >= dst->clip_rect.w) {
        src_x1 -= (dst_x1 - dst->clip_rect.w + 1) / scaling_w;
        dst_x1 = dst->clip_rect.w - 1;
    }

    if (dst_y0 < 0) {
        src_y0 -= dst_y0 / scaling_h;
        dst_y0 = 0;
    }

    if (dst_y1 >= dst->clip_rect.h) {
        src_y1 -= (dst_y1 - dst->clip_rect.h + 1) / scaling_h;
        dst_y1 = dst->clip_rect.h - 1;
    }

    /* Translate back to surface coordinates */
    dst_x0 += dst->clip_rect.x;
    dst_x1 += dst->clip_rect.x;
    dst_y0 += dst->clip_rect.y;
    dst_y1 += dst->clip_rect.y;

    final_src.x = (int)SDL_floor(src_x0 + 0.5);
    final_src.y = (int)SDL_floor(src_y0 + 0.5);
    final_src.w = (int)SDL_floor(src_x1 + 1 + 0.5) - (int)SDL_floor(src_x0 + 0.5);
    final_src.h = (int)SDL_floor(src_y1 + 1 + 0.5) - (int)SDL_floor(src_y0 + 0.5);

    final_dst.x = (int)SDL_floor(dst_x0 + 0.5);
    final_dst.y = (int)SDL_floor(dst_y0 + 0.5);
    final_dst.w = (int)SDL_floor(dst_x1 - dst_x0 + 1.5);
    final_dst.h = (int)SDL_floor(dst_y1 - dst_y0 + 1.5);

    if (final_dst.w < 0)
        final_dst.w = 0;
    if (final_dst.h < 0)
        final_dst.h = 0;

    if (dstrect)
        *dstrect = final_dst;

    if (final_dst.w == 0 || final_dst.h == 0 ||
        final_src.w <= 0 || final_src.h <= 0) {
        /* No-op. */
        return 0;
    }

    return SDL_LowerBlitScaled(src, &final_src, dst, &final_dst);
}

/**
 *  This is a semi-private blit function and it performs low-level surface
 *  scaled blitting only.
 */
int
SDL_LowerBlitScaled(SDL_Surface * src, SDL_Rect * srcrect,
                SDL_Surface * dst, SDL_Rect * dstrect)
{
    static const Uint32 complex_copy_flags = (
        SDL_COPY_MODULATE_COLOR | SDL_COPY_MODULATE_ALPHA |
        SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD |
        SDL_COPY_COLORKEY
    );

    if (!(src->map->info.flags & SDL_COPY_NEAREST)) {
        src->map->info.flags |= SDL_COPY_NEAREST;
        SDL_InvalidateMap(src->map);
    }

    if ( !(src->map->info.flags & complex_copy_flags) &&
         src->format->format == dst->format->format &&
         !SDL_ISPIXELFORMAT_INDEXED(src->format->format) ) {
        return SDL_SoftStretch( src, srcrect, dst, dstrect );
    } else {
        return SDL_LowerBlit( src, srcrect, dst, dstrect );
    }
}

/*
 * Lock a surface to directly access the pixels
 */
int
SDL_LockSurface(SDL_Surface * surface)
{
    if (!surface->locked) {
        /* Perform the lock */
        if (surface->flags & SDL_RLEACCEL) {
            SDL_UnRLESurface(surface, 1);
            surface->flags |= SDL_RLEACCEL;     /* save accel'd state */
        }
    }

    /* Increment the surface lock count, for recursive locks */
    ++surface->locked;

    /* Ready to go.. */
    return (0);
}

/*
 * Unlock a previously locked surface
 */
void
SDL_UnlockSurface(SDL_Surface * surface)
{
    /* Only perform an unlock if we are locked */
    if (!surface->locked || (--surface->locked > 0)) {
        return;
    }

    /* Update RLE encoded surface with new data */
    if ((surface->flags & SDL_RLEACCEL) == SDL_RLEACCEL) {
        surface->flags &= ~SDL_RLEACCEL;        /* stop lying */
        SDL_RLESurface(surface);
    }
}

/*
 * Creates a new surface identical to the existing surface
 */
SDL_Surface *
SDL_DuplicateSurface(SDL_Surface * surface)
{
    return SDL_ConvertSurface(surface, surface->format, surface->flags);
}

/*
 * Convert a surface into the specified pixel format.
 */
SDL_Surface *
SDL_ConvertSurface(SDL_Surface * surface, const SDL_PixelFormat * format,
                   Uint32 flags)
{
    SDL_Surface *convert;
    Uint32 copy_flags;
    SDL_Color copy_color;
    SDL_Rect bounds;

    if (!surface) {
        SDL_InvalidParamError("surface");
        return NULL;
    }
    if (!format) {
        SDL_InvalidParamError("format");
        return NULL;
    }

    /* Check for empty destination palette! (results in empty image) */
    if (format->palette != NULL) {
        int i;
        for (i = 0; i < format->palette->ncolors; ++i) {
            if ((format->palette->colors[i].r != 0xFF) ||
                (format->palette->colors[i].g != 0xFF) ||
                (format->palette->colors[i].b != 0xFF))
                break;
        }
        if (i == format->palette->ncolors) {
            SDL_SetError("Empty destination palette");
            return (NULL);
        }
    }

    /* Create a new surface with the desired format */
    convert = SDL_CreateRGBSurface(flags, surface->w, surface->h,
                                   format->BitsPerPixel, format->Rmask,
                                   format->Gmask, format->Bmask,
                                   format->Amask);
    if (convert == NULL) {
        return (NULL);
    }

    /* Copy the palette if any */
    if (format->palette && convert->format->palette) {
        SDL_memcpy(convert->format->palette->colors,
                   format->palette->colors,
                   format->palette->ncolors * sizeof(SDL_Color));
        convert->format->palette->ncolors = format->palette->ncolors;
    }

    /* Save the original copy flags */
    copy_flags = surface->map->info.flags;
    copy_color.r = surface->map->info.r;
    copy_color.g = surface->map->info.g;
    copy_color.b = surface->map->info.b;
    copy_color.a = surface->map->info.a;
    surface->map->info.r = 0xFF;
    surface->map->info.g = 0xFF;
    surface->map->info.b = 0xFF;
    surface->map->info.a = 0xFF;
    surface->map->info.flags = 0;
    SDL_InvalidateMap(surface->map);

    /* Copy over the image data */
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = surface->w;
    bounds.h = surface->h;
    SDL_LowerBlit(surface, &bounds, convert, &bounds);

    /* Clean up the original surface, and update converted surface */
    convert->map->info.r = copy_color.r;
    convert->map->info.g = copy_color.g;
    convert->map->info.b = copy_color.b;
    convert->map->info.a = copy_color.a;
    convert->map->info.flags =
        (copy_flags &
         ~(SDL_COPY_COLORKEY | SDL_COPY_BLEND
           | SDL_COPY_RLE_DESIRED | SDL_COPY_RLE_COLORKEY |
           SDL_COPY_RLE_ALPHAKEY));
    surface->map->info.r = copy_color.r;
    surface->map->info.g = copy_color.g;
    surface->map->info.b = copy_color.b;
    surface->map->info.a = copy_color.a;
    surface->map->info.flags = copy_flags;
    SDL_InvalidateMap(surface->map);
    if (copy_flags & SDL_COPY_COLORKEY) {
        SDL_bool set_colorkey_by_color = SDL_FALSE;

        if (surface->format->palette) {
            if (format->palette &&
                surface->format->palette->ncolors <= format->palette->ncolors &&
                (SDL_memcmp(surface->format->palette->colors, format->palette->colors,
                  surface->format->palette->ncolors * sizeof(SDL_Color)) == 0)) {
                /* The palette is identical, just set the same colorkey */
                SDL_SetColorKey(convert, 1, surface->map->info.colorkey);
            } else if (format->Amask) {
                /* The alpha was set in the destination from the palette */
            } else {
                set_colorkey_by_color = SDL_TRUE;
            }
        } else {
            set_colorkey_by_color = SDL_TRUE;
        }

        if (set_colorkey_by_color) {
            SDL_Surface *tmp;
            SDL_Surface *tmp2;
            int converted_colorkey = 0;

            /* Create a dummy surface to get the colorkey converted */
            tmp = SDL_CreateRGBSurface(0, 1, 1,
                                   surface->format->BitsPerPixel, surface->format->Rmask,
                                   surface->format->Gmask, surface->format->Bmask,
                                   surface->format->Amask);

            /* Share the palette, if any */
            if (surface->format->palette) {
                SDL_SetSurfacePalette(tmp, surface->format->palette);
            }
            
            SDL_FillRect(tmp, NULL, surface->map->info.colorkey);

            tmp->map->info.flags &= ~SDL_COPY_COLORKEY;

            /* Convertion of the colorkey */
            tmp2 = SDL_ConvertSurface(tmp, format, 0);

            /* Get the converted colorkey */
            SDL_memcpy(&converted_colorkey, tmp2->pixels, tmp2->format->BytesPerPixel);

            SDL_FreeSurface(tmp);
            SDL_FreeSurface(tmp2);

            /* Set the converted colorkey on the new surface */
            SDL_SetColorKey(convert, 1, converted_colorkey);

            /* This is needed when converting for 3D texture upload */
            SDL_ConvertColorkeyToAlpha(convert);
        }
    }
    SDL_SetClipRect(convert, &surface->clip_rect);

    /* Enable alpha blending by default if the new surface has an
     * alpha channel or alpha modulation */
    if ((surface->format->Amask && format->Amask) ||
        (copy_flags & SDL_COPY_MODULATE_ALPHA)) {
        SDL_SetSurfaceBlendMode(convert, SDL_BLENDMODE_BLEND);
    }
    if ((copy_flags & SDL_COPY_RLE_DESIRED) || (flags & SDL_RLEACCEL)) {
        SDL_SetSurfaceRLE(convert, SDL_RLEACCEL);
    }

    /* We're ready to go! */
    return (convert);
}

SDL_Surface *
SDL_ConvertSurfaceFormat(SDL_Surface * surface, Uint32 pixel_format,
                         Uint32 flags)
{
    SDL_PixelFormat *fmt;
    SDL_Surface *convert = NULL;

    fmt = SDL_AllocFormat(pixel_format);
    if (fmt) {
        convert = SDL_ConvertSurface(surface, fmt, flags);
        SDL_FreeFormat(fmt);
    }
    return convert;
}

/*
 * Create a surface on the stack for quick blit operations
 */
static SDL_INLINE SDL_bool
SDL_CreateSurfaceOnStack(int width, int height, Uint32 pixel_format,
                         void * pixels, int pitch, SDL_Surface * surface,
                         SDL_PixelFormat * format, SDL_BlitMap * blitmap)
{
    if (SDL_ISPIXELFORMAT_INDEXED(pixel_format)) {
        SDL_SetError("Indexed pixel formats not supported");
        return SDL_FALSE;
    }
    if (SDL_InitFormat(format, pixel_format) < 0) {
        return SDL_FALSE;
    }

    SDL_zerop(surface);
    surface->flags = SDL_PREALLOC;
    surface->format = format;
    surface->pixels = pixels;
    surface->w = width;
    surface->h = height;
    surface->pitch = pitch;
    /* We don't actually need to set up the clip rect for our purposes */
    /* SDL_SetClipRect(surface, NULL); */

    /* Allocate an empty mapping */
    SDL_zerop(blitmap);
    blitmap->info.r = 0xFF;
    blitmap->info.g = 0xFF;
    blitmap->info.b = 0xFF;
    blitmap->info.a = 0xFF;
    surface->map = blitmap;

    /* The surface is ready to go */
    surface->refcount = 1;
    return SDL_TRUE;
}

/*
 * Copy a block of pixels of one format to another format
 */
int SDL_ConvertPixels(int width, int height,
                      Uint32 src_format, const void * src, int src_pitch,
                      Uint32 dst_format, void * dst, int dst_pitch)
{
    SDL_Surface src_surface, dst_surface;
    SDL_PixelFormat src_fmt, dst_fmt;
    SDL_BlitMap src_blitmap, dst_blitmap;
    SDL_Rect rect;
    void *nonconst_src = (void *) src;

    /* Check to make sure we are blitting somewhere, so we don't crash */
    if (!dst) {
        return SDL_InvalidParamError("dst");
    }
    if (!dst_pitch) {
        return SDL_InvalidParamError("dst_pitch");
    }

    /* Fast path for same format copy */
    if (src_format == dst_format) {
        int i;

        if (SDL_ISPIXELFORMAT_FOURCC(src_format)) {
            switch (src_format) {
            case SDL_PIXELFORMAT_YUY2:
            case SDL_PIXELFORMAT_UYVY:
            case SDL_PIXELFORMAT_YVYU:
                /* Packed planes */
                width = 4 * ((width + 1) / 2);
                for (i = height; i--;) {
                    SDL_memcpy(dst, src, width);
                    src = (const Uint8*)src + src_pitch;
                    dst = (Uint8*)dst + dst_pitch;
                }
                break;
            case SDL_PIXELFORMAT_YV12:
            case SDL_PIXELFORMAT_IYUV:
            case SDL_PIXELFORMAT_NV12:
            case SDL_PIXELFORMAT_NV21:
                {
                    /* Y plane */
                    for (i = height; i--;) {
                        SDL_memcpy(dst, src, width);
                        src = (const Uint8*)src + src_pitch;
                        dst = (Uint8*)dst + dst_pitch;
                    }

                    /* not sure the pitch is relevant here.
                       this also works to add the size of two chroma planes */
#if 0
                    SDL_memcpy(dst, src, 2 * ((width + 1)/2) * ((height+1)/2));
#else

                    if (src_format == SDL_PIXELFORMAT_YV12 || src_format == SDL_PIXELFORMAT_IYUV) {
                        /* U and V planes are a quarter the size of the Y plane */
                        width = (width + 1) / 2;
                        height = (height + 1) / 2;
                        src_pitch = (src_pitch + 1) / 2;
                        dst_pitch = (dst_pitch + 1) / 2;
                        for (i = height * 2; i--;) {
                            SDL_memcpy(dst, src, width);
                            src = (const Uint8*)src + src_pitch;
                            dst = (Uint8*)dst + dst_pitch;
                        }
                    } else if (src_format == SDL_PIXELFORMAT_NV12 || src_format == SDL_PIXELFORMAT_NV21) {
                        /* U/V plane is half the height of the Y plane */
                        height = (height + 1) / 2;
                        width = (width + 1) / 2;
                        src_pitch = (src_pitch + 1) / 2;
                        dst_pitch = (dst_pitch + 1) / 2;
                        for (i = height; i--;) {
                            SDL_memcpy(dst, src, 2 * width);
                            src = (const Uint8*)src + 2 * src_pitch;
                            dst = (Uint8*)dst + 2 * dst_pitch;
                        }
                    }
#endif
                }
                break;
            default:
                return SDL_SetError("Unknown FOURCC pixel format");
            }
        } else {
            const int bpp = SDL_BYTESPERPIXEL(src_format);
            width *= bpp;
            for (i = height; i--;) {
                SDL_memcpy(dst, src, width);
                src = (const Uint8*)src + src_pitch;
                dst = (Uint8*)dst + dst_pitch;
            }
        }
        return 0;
    }

    /* FOURCC to Any */
    if (SDL_ISPIXELFORMAT_FOURCC(src_format)) {
        /* FOURCC to ARGB8888 */
        if (dst_format == SDL_PIXELFORMAT_ARGB8888) {
            SDL_ConvertPixels_YUV_to_ARGB8888(width, height, src_format, src, dst, dst_pitch);
            return 0;
        }
        else /* FOURCC to not(ARGB8888) : need an intermediate conversion */
        {
            int ret;
            void *tmp = SDL_malloc(width * height * 4);
            if (tmp == NULL) {
                return -1;
            }

            /* convert src/FOURCC to tmp/ARGB8888 */
            SDL_ConvertPixels_YUV_to_ARGB8888(width, height, src_format, src, tmp, width * 4);
            
            /* convert tmp/ARGB8888 to dst/dst_format */
            ret = SDL_ConvertPixels(width, height, SDL_PIXELFORMAT_ARGB8888, tmp, width * 4, dst_format, dst, dst_pitch);
            SDL_free(tmp);
            return ret;
        }
    }

    /* Any to FOURCC */
    if (SDL_ISPIXELFORMAT_FOURCC(dst_format)) {
        /* ARGB8888 to FOURCC */
        if (src_format == SDL_PIXELFORMAT_ARGB8888) {
            SDL_ConvertPixels_ARGB8888_to_YUV(width, height, src, src_pitch, dst_format, dst);
            return 0;
        }
        else /* not(ARGB8888) to FOURCC : need an intermediate conversion */
        {
            int ret;
            void *tmp = SDL_malloc(width * height * 4);
            if (tmp == NULL) {
                return -1;
            }
            /* convert src/src_format to tmp/ARGB8888 */
            ret = SDL_ConvertPixels(width, height, src_format, src, src_pitch, SDL_PIXELFORMAT_ARGB8888, tmp, width * 4);
            if (ret == -1) {
                SDL_free(tmp);
                return ret;
            }
            /* convert tmp/ARGB8888 to dst/FOURCC */
            SDL_ConvertPixels_ARGB8888_to_YUV(width, height, tmp, width * 4, dst_format, dst);

            SDL_free(tmp);
            return 0;
        }
    }

    if (!SDL_CreateSurfaceOnStack(width, height, src_format, nonconst_src,
                                  src_pitch,
                                  &src_surface, &src_fmt, &src_blitmap)) {
        return -1;
    }
    if (!SDL_CreateSurfaceOnStack(width, height, dst_format, dst, dst_pitch,
                                  &dst_surface, &dst_fmt, &dst_blitmap)) {
        return -1;
    }

    /* Set up the rect and go! */
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    return SDL_LowerBlit(&src_surface, &rect, &dst_surface, &rect);
}

/*
 * Free a surface created by the above function.
 */
void
SDL_FreeSurface(SDL_Surface * surface)
{
    if (surface == NULL) {
        return;
    }
    if (surface->flags & SDL_DONTFREE) {
        return;
    }
    SDL_InvalidateMap(surface->map);

    if (--surface->refcount > 0) {
        return;
    }
    while (surface->locked > 0) {
        SDL_UnlockSurface(surface);
    }
    if (surface->flags & SDL_RLEACCEL) {
        SDL_UnRLESurface(surface, 0);
    }
    if (surface->format) {
        SDL_SetSurfacePalette(surface, NULL);
        SDL_FreeFormat(surface->format);
        surface->format = NULL;
    }
    if (!(surface->flags & SDL_PREALLOC)) {
        SDL_free(surface->pixels);
    }
    if (surface->map) {
        SDL_FreeBlitMap(surface->map);
    }
    SDL_free(surface);
}


/* YUV-RGB conversion */
#define CLAMP(val) ((val) > 0 ? ((val) < 255 ? (val) : 255) : 0)

#if 1

/* Coefficients from CCIR 601 */
#define MAKE_Y(r, g, b) (int)( 0.29900f * (r) + 0.58700f * (g) + 0.11400f * (b))
#define MAKE_U(r, g, b) (int)(-0.16874f * (r) - 0.33126f * (g) + 0.50000f * (b) + 128)
#define MAKE_V(r, g, b) (int)( 0.50000f * (r) - 0.41869f * (g) - 0.08131f * (b) + 128)

#define MAKE_R(y, u, v) CLAMP((int)((y)                          + 1.40200f * ((v) - 128)))
#define MAKE_G(y, u, v) CLAMP((int)((y) - 0.34414f * ((u) - 128) - 0.71414f * ((v) - 128)))
#define MAKE_B(y, u, v) CLAMP((int)((y) + 1.77200f * ((u) - 128)                         ))

#else

/* Coefficients from Video Demystified */
#define MAKE_Y(r, g, b) (((  66 * (r) + 129 * (g) +  25 * (b) + 128) >> 8) + 16)
#define MAKE_U(r, g, b) ((( -38 * (r) -  74 * (g) + 112 * (b) + 128) >> 8) + 128)
#define MAKE_V(r, g, b) ((( 112 * (r) -  94 * (g) -  18 * (b) + 128) >> 8) + 128)

#define MAKE_R(y, u, v) CLAMP(( 298 * ((y) - 16)                     + 409 * ((v) - 128) + 128) >> 8) 
#define MAKE_G(y, u, v) CLAMP(( 298 * ((y) - 16) - 100 * ((u) - 128) - 208 * ((v) - 128) + 128) >> 8)
#define MAKE_B(y, u, v) CLAMP(( 298 * ((y) - 16) + 516 * ((u) - 128)                     + 128) >> 8)

#endif


static int
SDL_ConvertPixels_YUV_to_ARGB8888(int width, int height,
         Uint32 src_format, const void *src, 
         void *dst, int dst_pitch)
{   
    const int sz_plane         = width * height;
    const int sz_plane_chroma  = ((width + 1) / 2) * ((height + 1) / 2);
    const int width_remainder  = (width &  0x1);
    const int width_half       = width / 2;
    const int curr_row_padding = dst_pitch - 4 * width;
    int i, j;
    Uint8 *curr_row = (Uint8*)dst;

    // SDL_Log("SDL_ConvertPixels_YUV_to_ARGB8888 (from %s)", SDL_GetPixelFormatName(src_format));

#define WRITE_RGB_PIXEL(y, u, v)            \
    *((Uint32*)curr_row) =                  \
           (MAKE_B((y), (u), (v))           \
         | (MAKE_G((y), (u), (v)) << 8)     \
         | (MAKE_R((y), (u), (v)) << 16)    \
         | 0xff000000);                     \
    curr_row += 4;                          \

    switch (src_format) 
    {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
            {
                const Uint8 *plane_y = (const Uint8*)src;

                if (src_format == SDL_PIXELFORMAT_YV12 || src_format == SDL_PIXELFORMAT_IYUV)
                {
                    const Uint8 *plane_u = (src_format == SDL_PIXELFORMAT_YV12 ? plane_y + sz_plane + sz_plane_chroma : plane_y + sz_plane);
                    const Uint8 *plane_v = (src_format == SDL_PIXELFORMAT_YV12 ? plane_y + sz_plane : plane_y + sz_plane + sz_plane_chroma);

                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            const Uint8 u = *plane_u++;
                            const Uint8 v = *plane_v++;
                            const Uint8 y = *plane_y++;
                            const Uint8 y1 = *plane_y++;
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            const Uint8 u = *plane_u++;
                            const Uint8 v = *plane_v++;
                            const Uint8 y = *plane_y++;
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        /* Re-use the same line of chroma planes */
                        if ((j & 0x1) == 0x0) {
                            plane_u -= width_half + width_remainder;
                            plane_v -= width_half + width_remainder;
                        }
                        curr_row += curr_row_padding;
                    }
                }
                else if (src_format == SDL_PIXELFORMAT_NV12)
                {
                    const Uint8 *plane_interleaved_uv = plane_y + sz_plane;
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            const Uint8 y = *plane_y++;
                            const Uint8 y1 = *plane_y++;
                            const Uint8 u = *plane_interleaved_uv++;
                            const Uint8 v = *plane_interleaved_uv++;
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            const Uint8 y = *plane_y++;
                            const Uint8 u = *plane_interleaved_uv++;
                            const Uint8 v = *plane_interleaved_uv++;
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        /* Re-use the same line of chroma planes */
                        if ((j & 0x1) == 0x0) {
                            plane_interleaved_uv -= 2 * (width_half + width_remainder);
                        }
                        curr_row += curr_row_padding;
                    }
                } 
                else /* src_format == SDL_PIXELFORMAT_NV21 */
                {
                    const Uint8 *plane_interleaved_uv = plane_y + sz_plane;
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            const Uint8 y = *plane_y++;
                            const Uint8 y1 = *plane_y++;
                            const Uint8 v = *plane_interleaved_uv++;
                            const Uint8 u = *plane_interleaved_uv++;
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            const Uint8 y = *plane_y++;
                            const Uint8 v = *plane_interleaved_uv++;
                            const Uint8 u = *plane_interleaved_uv++;
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        /* Re-use the same line of chroma planes */
                        if ((j & 0x1) == 0x0) {
                            plane_interleaved_uv -= 2 * (width_half + width_remainder);
                        }
                        curr_row += curr_row_padding;
                    }
                }
            }
            break;

        case SDL_PIXELFORMAT_YUY2:
        case SDL_PIXELFORMAT_UYVY:
        case SDL_PIXELFORMAT_YVYU:
            {
                const Uint8 *plane = (const Uint8 *)src;

#define READ_PACKED_YUV(var1, var2, var3, var4) \
                const Uint8 var1 = plane[0];    \
                const Uint8 var2 = plane[1];    \
                const Uint8 var3 = plane[2];    \
                const Uint8 var4 = plane[3];    \
                plane += 4;                     \

                if (src_format == SDL_PIXELFORMAT_YUY2) /* Y U Y1 V */
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_PACKED_YUV(y, u, y1, v);
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            READ_PACKED_YUV(y, u, y1, v); 
                            (void)y1; /* y1 unused */
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        curr_row += curr_row_padding;
                    }
                } 
                else if (src_format == SDL_PIXELFORMAT_UYVY) /* U Y V Y1 */
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_PACKED_YUV(u, y, v, y1);
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            READ_PACKED_YUV(u, y, v, y1);
                            (void) y1; /* y1 unused */
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        curr_row += curr_row_padding;
                    }
                }
                else if (src_format == SDL_PIXELFORMAT_YVYU) /* Y V Y1 U */
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_PACKED_YUV(y, v, y1, u);
                            WRITE_RGB_PIXEL(y, u, v);
                            WRITE_RGB_PIXEL(y1, u, v);
                        }
                        if (width_remainder) {
                            READ_PACKED_YUV(y, v, y1, u);
                            (void) y1; /* y1 unused */
                            WRITE_RGB_PIXEL(y, u, v);
                        }
                        curr_row += curr_row_padding;
                    }
                } 
#undef READ_PACKED_YUV
            }
            break;
    }
#undef WRITE_RGB_PIXEL
    return 0;
}

static int
SDL_ConvertPixels_ARGB8888_to_YUV(int width, int height, const void *src, int src_pitch, Uint32 dst_format, void *dst)
{
    const int src_pitch_x_2    = src_pitch * 2;
    const int sz_plane         = width * height;
    const int sz_plane_chroma  = ((width + 1) / 2) * ((height + 1) / 2);
    const int height_half      = height / 2;
    const int height_remainder = (height &  0x1);
    const int width_half       = width / 2;
    const int width_remainder  = (width  &  0x1);
    int i, j;
    
    // SDL_Log("SDL_ConvertPixels_ARGB8888_to_YUV (to %s)", SDL_GetPixelFormatName(dst_format));

    switch (dst_format) 
    {
        case SDL_PIXELFORMAT_YV12:
        case SDL_PIXELFORMAT_IYUV:
        case SDL_PIXELFORMAT_NV12:
        case SDL_PIXELFORMAT_NV21:
            {
                const Uint8 *curr_row, *next_row;
                
                Uint8 *plane_y = (Uint8*) dst;
                Uint8 *plane_u = (dst_format == SDL_PIXELFORMAT_YV12 ? plane_y + sz_plane + sz_plane_chroma : plane_y + sz_plane);
                Uint8 *plane_v = (dst_format == SDL_PIXELFORMAT_YV12 ? plane_y + sz_plane : plane_y + sz_plane + sz_plane_chroma);
                Uint8 *plane_interleaved_uv = plane_y + sz_plane;

                curr_row = (const Uint8*)src;

                /* Write Y plane */
                for (j = 0; j < height; j++) {
                    for (i = 0; i < width; i++) {
                        const Uint8 b = curr_row[4 * i + 0];
                        const Uint8 g = curr_row[4 * i + 1];
                        const Uint8 r = curr_row[4 * i + 2];
                        *plane_y++ = MAKE_Y(r, g, b);
                    }
                    curr_row += src_pitch;
                }

                curr_row = (const Uint8*)src;
                next_row = (const Uint8*)src;
                next_row += src_pitch;

#if 1
/* slightly faster */
#define READ_2x2_PIXELS                                                                                                 \
                const Uint32 p1 = ((const Uint32 *)curr_row)[2 * i];                                                    \
                const Uint32 p2 = ((const Uint32 *)curr_row)[2 * i + 1];                                                \
                const Uint32 p3 = ((const Uint32 *)next_row)[2 * i];                                                    \
                const Uint32 p4 = ((const Uint32 *)next_row)[2 * i + 1];                                                \
                const Uint32 b = ((p1 & 0x000000ff) + (p2 & 0x000000ff) + (p3 & 0x000000ff) + (p4 & 0x000000ff)) >> 2;  \
                const Uint32 g = ((p1 & 0x0000ff00) + (p2 & 0x0000ff00) + (p3 & 0x0000ff00) + (p4 & 0x0000ff00)) >> 10; \
                const Uint32 r = ((p1 & 0x00ff0000) + (p2 & 0x00ff0000) + (p3 & 0x00ff0000) + (p4 & 0x00ff0000)) >> 18; \

#else

#define READ_2x2_PIXELS                                                             \
                const Uint8 b = (curr_row[8 * i + 0] + curr_row[8 * i + 4]          \
                               + next_row[8 * i + 0] + next_row[8 * i + 4] ) >> 2;  \
                const Uint8 g = (curr_row[8 * i + 1] + curr_row[8 * i + 5]          \
                               + next_row[8 * i + 1] + next_row[8 * i + 5] ) >> 2;  \
                const Uint8 r = (curr_row[8 * i + 2] + curr_row[8 * i + 6]          \
                               + next_row[8 * i + 2] + next_row[8 * i + 6] ) >> 2;  \

#endif

#define READ_2x1_PIXELS                                                             \
                const Uint8 b = (curr_row[8 * i + 0] + next_row[8 * i + 0]) >> 1;   \
                const Uint8 g = (curr_row[8 * i + 1] + next_row[8 * i + 1]) >> 1;   \
                const Uint8 r = (curr_row[8 * i + 2] + next_row[8 * i + 2]) >> 1;   \

#define READ_1x2_PIXELS                                                             \
                const Uint8 b = (curr_row[8 * i + 0] + curr_row[8 * i + 4]) >> 1;   \
                const Uint8 g = (curr_row[8 * i + 1] + curr_row[8 * i + 5]) >> 1;   \
                const Uint8 r = (curr_row[8 * i + 2] + curr_row[8 * i + 6]) >> 1;   \

#define READ_1x1_PIXEL                                                              \
                const Uint8 b = curr_row[8 * i + 0];                                \
                const Uint8 g = curr_row[8 * i + 1];                                \
                const Uint8 r = curr_row[8 * i + 2];                                \
                
                if (dst_format == SDL_PIXELFORMAT_YV12 || dst_format == SDL_PIXELFORMAT_IYUV)
                {
                    /* Write UV planes, not interleaved */
                    for (j = 0; j < height_half; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_2x2_PIXELS;
                            *plane_u++ = MAKE_U(r, g, b);
                            *plane_v++ = MAKE_V(r, g, b);
                        }
                        if (width_remainder) {
                            READ_2x1_PIXELS;
                            *plane_u++ = MAKE_U(r, g, b);
                            *plane_v++ = MAKE_V(r, g, b);
                        }
                        curr_row += src_pitch_x_2;
                        next_row += src_pitch_x_2;
                    }
                    if (height_remainder) {
                        for (i = 0; i < width_half; i++) {
                            READ_1x2_PIXELS;
                            *plane_u++ = MAKE_U(r, g, b);
                            *plane_v++ = MAKE_V(r, g, b);
                        }
                        if (width_remainder) {
                            READ_1x1_PIXEL;
                            *plane_u++ = MAKE_U(r, g, b);
                            *plane_v++ = MAKE_V(r, g, b);
                        }
                    }
                }
                else if (dst_format == SDL_PIXELFORMAT_NV12)
                {
                    for (j = 0; j < height_half; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_2x2_PIXELS;
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                        }
                        if (width_remainder) {
                            READ_2x1_PIXELS;
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                        }
                        curr_row += src_pitch_x_2;
                        next_row += src_pitch_x_2;
                    }
                    if (height_remainder) {
                        for (i = 0; i < width_half; i++) {
                            READ_1x2_PIXELS;
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                        }
                        if (width_remainder) {
                            READ_1x1_PIXEL;
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                        }
                    }
                } 
                else /* dst_format == SDL_PIXELFORMAT_NV21 */
                {
                    for (j = 0; j < height_half; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_2x2_PIXELS;
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                        }
                        if (width_remainder) {
                            READ_2x1_PIXELS;
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                        }
                        curr_row += src_pitch_x_2;
                        next_row += src_pitch_x_2;
                    }
                    if (height_remainder) {
                        for (i = 0; i < width_half; i++) {
                            READ_1x2_PIXELS;
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                        }
                        if (width_remainder) {
                            READ_1x1_PIXEL;
                            *plane_interleaved_uv++ = MAKE_V(r, g, b);
                            *plane_interleaved_uv++ = MAKE_U(r, g, b);
                        }
                    }
                }
#undef READ_2x2_PIXELS
#undef READ_2x1_PIXELS
#undef READ_1x2_PIXELS
#undef READ_1x1_PIXEL
            }
            break;

        case SDL_PIXELFORMAT_YUY2:
        case SDL_PIXELFORMAT_UYVY:
        case SDL_PIXELFORMAT_YVYU:
            {
                const Uint8 *curr_row = (const Uint8*) src;
                Uint8 *plane           = (Uint8*) dst;

#define READ_TWO_RGB_PIXELS \
                const Uint8 b = curr_row[8 * i + 0];    \
                const Uint8 g = curr_row[8 * i + 1];    \
                const Uint8 r = curr_row[8 * i + 2];    \
                const Uint8 b1 = curr_row[8 * i + 4];   \
                const Uint8 g1 = curr_row[8 * i + 5];   \
                const Uint8 r1 = curr_row[8 * i + 6];   \
                const Uint8 B = (b + b1) >> 1;          \
                const Uint8 G = (g + g1) >> 1;          \
                const Uint8 R = (r + r1) >> 1;          \

#define READ_ONE_RGB_PIXEL \
                const Uint8 b = curr_row[8 * i + 0];    \
                const Uint8 g = curr_row[8 * i + 1];    \
                const Uint8 r = curr_row[8 * i + 2];    \

                /* Write YUV plane, packed */
                if (dst_format == SDL_PIXELFORMAT_YUY2) 
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_TWO_RGB_PIXELS;
                            /* Y U Y1 V */
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_U(R, G, B);
                            *plane++ = MAKE_Y(r1, g1, b1);
                            *plane++ = MAKE_V(R, G, B);
                        }
                        if (width_remainder) {
                            READ_ONE_RGB_PIXEL;
                            /* Y U Y V */
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_U(r, g, b);
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_V(r, g, b);
                        }
                        curr_row += src_pitch;
                    }
                } 
                else if (dst_format == SDL_PIXELFORMAT_UYVY)
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_TWO_RGB_PIXELS;
                            /* U Y V Y1 */
                            *plane++ = MAKE_U(R, G, B);
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_V(R, G, B);
                            *plane++ = MAKE_Y(r1, g1, b1);
                        }
                        if (width_remainder) {
                            READ_ONE_RGB_PIXEL;
                            /* U Y V Y */
                            *plane++ = MAKE_U(r, g, b);
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_V(r, g, b);
                            *plane++ = MAKE_Y(r, g, b);
                        }
                        curr_row += src_pitch;
                    }
                }
                else if (dst_format == SDL_PIXELFORMAT_YVYU)
                {
                    for (j = 0; j < height; j++) {
                        for (i = 0; i < width_half; i++) {
                            READ_TWO_RGB_PIXELS;
                            /* Y V Y1 U */
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_V(R, G, B);
                            *plane++ = MAKE_Y(r1, g1, b1);
                            *plane++ = MAKE_U(R, G, B);
                        }
                        if (width_remainder) {
                            READ_ONE_RGB_PIXEL;
                            /* Y V Y U */
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_V(r, g, b);
                            *plane++ = MAKE_Y(r, g, b);
                            *plane++ = MAKE_U(r, g, b);
                        }
                        curr_row += src_pitch;
                    }
                }
#undef READ_TWO_RGB_PIXELS
#undef READ_ONE_RGB_PIXEL
            }
            break;
    }
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
