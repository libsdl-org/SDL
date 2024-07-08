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

#include "SDL_sysvideo.h"
#include "SDL_video_c.h"
#include "SDL_blit.h"
#include "SDL_RLEaccel_c.h"
#include "SDL_pixels_c.h"
#include "SDL_yuv_c.h"
#include "../render/SDL_sysrender.h"

#include "SDL_surface_c.h"


/* Check to make sure we can safely check multiplication of surface w and pitch and it won't overflow size_t */
SDL_COMPILE_TIME_ASSERT(surface_size_assumptions,
                        sizeof(int) == sizeof(Sint32) && sizeof(size_t) >= sizeof(Sint32));

SDL_COMPILE_TIME_ASSERT(can_indicate_overflow, SDL_SIZE_MAX > SDL_MAX_SINT32);

/* Public routines */

SDL_bool SDL_SurfaceValid(SDL_Surface *surface)
{
    return surface && surface->internal;
}

void SDL_UpdateSurfaceLockFlag(SDL_Surface *surface)
{
    if (surface->internal->flags & SDL_INTERNAL_SURFACE_RLEACCEL) {
        surface->flags |= SDL_SURFACE_LOCK_NEEDED;
    } else {
        surface->flags &= ~SDL_SURFACE_LOCK_NEEDED;
    }
}

/*
 * Calculate the pad-aligned scanline width of a surface.
 * Return SDL_SIZE_MAX on overflow.
 *
 * for FOURCC, use SDL_CalculateYUVSize()
 */
static int SDL_CalculateRGBSize(Uint32 format, size_t width, size_t height, size_t *size, size_t *pitch, SDL_bool minimal)
{
    if (SDL_BITSPERPIXEL(format) >= 8) {
        if (SDL_size_mul_overflow(width, SDL_BYTESPERPIXEL(format), pitch)) {
            return SDL_SetError("width * bpp would overflow");
        }
    } else {
        if (SDL_size_mul_overflow(width, SDL_BITSPERPIXEL(format), pitch)) {
            return SDL_SetError("width * bpp would overflow");
        }
        if (SDL_size_add_overflow(*pitch, 7, pitch)) {
            return SDL_SetError("aligning pitch would overflow");
        }
        *pitch /= 8;
    }
    if (!minimal) {
        /* 4-byte aligning for speed */
        if (SDL_size_add_overflow(*pitch, 3, pitch)) {
            return SDL_SetError("aligning pitch would overflow");
        }
        *pitch &= ~3;
    }

    if (SDL_size_mul_overflow(height, *pitch, size)) {
        return SDL_SetError("height * pitch would overflow");
    }

    return 0;
}

int SDL_CalculateSurfaceSize(SDL_PixelFormat format, int width, int height, size_t *size, size_t *pitch, SDL_bool minimalPitch)
{
    size_t p = 0, sz = 0;

    if (size) {
        *size = 0;
    }

    if (pitch) {
        *pitch = 0;
    }

    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        if (SDL_CalculateYUVSize(format, width, height, &sz, &p) < 0) {
            /* Overflow... */
            return -1;
        }
    } else {
        if (SDL_CalculateRGBSize(format, width, height, &sz, &p, minimalPitch) < 0) {
            /* Overflow... */
            return -1;
        }
    }

    if (size) {
        *size = sz;
    }

    if (pitch) {
        *pitch = p;
    }

    return 0;
}

static SDL_Surface *SDL_InitializeSurface(SDL_InternalSurface *mem, int width, int height, SDL_PixelFormat format, SDL_Colorspace colorspace, SDL_PropertiesID props, void *pixels, int pitch, SDL_bool onstack)
{
    SDL_Surface *surface = &mem->surface;

    SDL_zerop(mem);

    surface->flags = SDL_SURFACE_PREALLOCATED;
    surface->format = format;
    surface->w = width;
    surface->h = height;
    surface->pixels = pixels;
    surface->pitch = pitch;

    surface->internal = &mem->internal;
    if (onstack) {
        surface->internal->flags |= SDL_INTERNAL_SURFACE_STACK;
    }

    surface->internal->format = SDL_GetPixelFormatDetails(format);
    if (!surface->internal->format) {
        SDL_DestroySurface(surface);
        return NULL;
    }

    /* Initialize the clip rect */
    surface->internal->clip_rect.w = width;
    surface->internal->clip_rect.h = height;

    /* Allocate an empty mapping */
    surface->internal->map.info.r = 0xFF;
    surface->internal->map.info.g = 0xFF;
    surface->internal->map.info.b = 0xFF;
    surface->internal->map.info.a = 0xFF;

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        SDL_Palette *palette = SDL_CreatePalette((1 << SDL_BITSPERPIXEL(surface->format)));
        if (!palette) {
            SDL_DestroySurface(surface);
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
        SDL_DestroyPalette(palette);
    }

    if (colorspace != SDL_COLORSPACE_UNKNOWN &&
        colorspace != SDL_GetDefaultColorspaceForFormat(format)) {
        SDL_SetSurfaceColorspace(surface, colorspace);
    }

    if (props) {
        if (SDL_CopyProperties(props, SDL_GetSurfaceProperties(surface)) < 0) {
            return NULL;
        }
    }

    /* By default surfaces with an alpha mask are set up for blending */
    if (SDL_ISPIXELFORMAT_ALPHA(surface->format)) {
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    }

    /* The surface is ready to go */
    surface->refcount = 1;
    return surface;
}

/*
 * Create an empty surface of the appropriate depth using the given format
 */
SDL_Surface *SDL_CreateSurface(int width, int height, SDL_PixelFormat format)
{
    size_t pitch, size;
    SDL_InternalSurface *mem;
    SDL_Surface *surface;

    if (width < 0) {
        SDL_InvalidParamError("width");
        return NULL;
    }

    if (height < 0) {
        SDL_InvalidParamError("height");
        return NULL;
    }

    if (SDL_CalculateSurfaceSize(format, width, height, &size, &pitch, SDL_FALSE /* not minimal pitch */) < 0) {
        /* Overflow... */
        return NULL;
    }

    /* Allocate and initialize the surface */
    mem = (SDL_InternalSurface *)SDL_malloc(sizeof(*mem));
    if (!mem) {
        return NULL;
    }

    surface = SDL_InitializeSurface(mem, width, height, format, SDL_COLORSPACE_UNKNOWN, 0, NULL, (int)pitch, SDL_FALSE);
    if (surface) {
        if (surface->w && surface->h) {
            surface->flags &= ~SDL_SURFACE_PREALLOCATED;
            surface->pixels = SDL_aligned_alloc(SDL_GetSIMDAlignment(), size);
            if (!surface->pixels) {
                SDL_DestroySurface(surface);
                return NULL;
            }
            surface->flags |= SDL_SURFACE_SIMD_ALIGNED;

            /* This is important for bitmaps */
            SDL_memset(surface->pixels, 0, size);
        }
    }
    return surface;
}

/*
 * Create an RGB surface from an existing memory buffer using the given
 * enum SDL_PIXELFORMAT_* format
 */
SDL_Surface *SDL_CreateSurfaceFrom(int width, int height, SDL_PixelFormat format, void *pixels, int pitch)
{
    SDL_InternalSurface *mem;

    if (width < 0) {
        SDL_InvalidParamError("width");
        return NULL;
    }

    if (height < 0) {
        SDL_InvalidParamError("height");
        return NULL;
    }

    if (pitch == 0 && !pixels) {
        /* The application will fill these in later with valid values */
    } else {
        size_t minimalPitch;

        if (SDL_CalculateSurfaceSize(format, width, height, NULL, &minimalPitch, SDL_TRUE /* minimal pitch */) < 0) {
            /* Overflow... */
            return NULL;
        }

        if (pitch < 0 || (size_t)pitch < minimalPitch) {
            SDL_InvalidParamError("pitch");
            return NULL;
        }
    }

    /* Allocate and initialize the surface */
    mem = (SDL_InternalSurface *)SDL_malloc(sizeof(*mem));
    if (!mem) {
        return NULL;
    }

    return SDL_InitializeSurface(mem, width, height, format, SDL_COLORSPACE_UNKNOWN, 0, pixels, pitch, SDL_FALSE);
}

SDL_PropertiesID SDL_GetSurfaceProperties(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        return 0;
    }

    if (!surface->internal->props) {
        surface->internal->props = SDL_CreateProperties();
    }
    return surface->internal->props;
}

int SDL_SetSurfaceColorspace(SDL_Surface *surface, SDL_Colorspace colorspace)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (colorspace == SDL_GetDefaultColorspaceForFormat(surface->format)) {
        return 0;
    }
    return SDL_SetNumberProperty(SDL_GetSurfaceProperties(surface), SDL_PROP_SURFACE_COLORSPACE_NUMBER, colorspace);
}

SDL_Colorspace SDL_GetSurfaceColorspace(SDL_Surface *surface)
{
    SDL_Colorspace colorspace;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_COLORSPACE_UNKNOWN;
    }

    colorspace = (SDL_Colorspace)SDL_GetNumberProperty(surface->internal->props, SDL_PROP_SURFACE_COLORSPACE_NUMBER, SDL_COLORSPACE_UNKNOWN);
    if (colorspace == SDL_COLORSPACE_UNKNOWN) {
        colorspace = SDL_GetDefaultColorspaceForFormat(surface->format);
    }
    return colorspace;
}

float SDL_GetDefaultSDRWhitePoint(SDL_Colorspace colorspace)
{
    return SDL_GetSurfaceSDRWhitePoint(NULL, colorspace);
}

float SDL_GetSurfaceSDRWhitePoint(SDL_Surface *surface, SDL_Colorspace colorspace)
{
    SDL_TransferCharacteristics transfer = SDL_COLORSPACETRANSFER(colorspace);

    if (transfer == SDL_TRANSFER_CHARACTERISTICS_LINEAR ||
        transfer == SDL_TRANSFER_CHARACTERISTICS_PQ) {
        SDL_PropertiesID props;
        float default_value = 1.0f;

        if (SDL_SurfaceValid(surface)) {
            props = surface->internal->props;
        } else {
            props = 0;
        }
        if (transfer == SDL_TRANSFER_CHARACTERISTICS_PQ) {
            /* The older standards use an SDR white point of 100 nits.
             * ITU-R BT.2408-6 recommends using an SDR white point of 203 nits.
             * This is the default Chrome uses, and what a lot of game content
             * assumes, so we'll go with that.
             */
            const float DEFAULT_PQ_SDR_WHITE_POINT = 203.0f;
            default_value = DEFAULT_PQ_SDR_WHITE_POINT;
        }
        return SDL_GetFloatProperty(props, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, default_value);
    }
    return 1.0f;
}

float SDL_GetDefaultHDRHeadroom(SDL_Colorspace colorspace)
{
    return SDL_GetSurfaceHDRHeadroom(NULL, colorspace);
}

float SDL_GetSurfaceHDRHeadroom(SDL_Surface *surface, SDL_Colorspace colorspace)
{
    SDL_TransferCharacteristics transfer = SDL_COLORSPACETRANSFER(colorspace);

    if (transfer == SDL_TRANSFER_CHARACTERISTICS_LINEAR ||
        transfer == SDL_TRANSFER_CHARACTERISTICS_PQ) {
        SDL_PropertiesID props;
        float default_value = 0.0f;

        if (SDL_SurfaceValid(surface)) {
            props = surface->internal->props;
        } else {
            props = 0;
        }
        return SDL_GetFloatProperty(props, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, default_value);
    }
    return 1.0f;
}

int SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (palette && palette->ncolors > (1 << SDL_BITSPERPIXEL(surface->format))) {
        return SDL_SetError("SDL_SetSurfacePalette() passed a palette that doesn't match the surface format");
    }

    if (palette == surface->internal->palette) {
        return 0;
    }

    if (surface->internal->palette) {
        SDL_DestroyPalette(surface->internal->palette);
    }

    surface->internal->palette = palette;

    if (surface->internal->palette) {
        ++surface->internal->palette->refcount;
    }

    SDL_InvalidateMap(&surface->internal->map);

    return 0;
}

SDL_Palette *SDL_GetSurfacePalette(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return NULL;
    }

    return surface->internal->palette;
}

int SDL_SetSurfaceRLE(SDL_Surface *surface, SDL_bool enabled)
{
    int flags;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    flags = surface->internal->map.info.flags;
    if (enabled) {
        surface->internal->map.info.flags |= SDL_COPY_RLE_DESIRED;
    } else {
        surface->internal->map.info.flags &= ~SDL_COPY_RLE_DESIRED;
    }
    if (surface->internal->map.info.flags != flags) {
        SDL_InvalidateMap(&surface->internal->map);
    }
    return 0;
}

SDL_bool SDL_SurfaceHasRLE(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_FALSE;
    }

    if (!(surface->internal->map.info.flags & SDL_COPY_RLE_DESIRED)) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

int SDL_SetSurfaceColorKey(SDL_Surface *surface, SDL_bool enabled, Uint32 key)
{
    int flags;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (surface->internal->palette && key >= ((Uint32)surface->internal->palette->ncolors)) {
        return SDL_InvalidParamError("key");
    }

    flags = surface->internal->map.info.flags;
    if (enabled) {
        surface->internal->map.info.flags |= SDL_COPY_COLORKEY;
        surface->internal->map.info.colorkey = key;
    } else {
        surface->internal->map.info.flags &= ~SDL_COPY_COLORKEY;
    }
    if (surface->internal->map.info.flags != flags) {
        SDL_InvalidateMap(&surface->internal->map);
    }

    return 0;
}

SDL_bool SDL_SurfaceHasColorKey(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_FALSE;
    }

    if (!(surface->internal->map.info.flags & SDL_COPY_COLORKEY)) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

int SDL_GetSurfaceColorKey(SDL_Surface *surface, Uint32 *key)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (!(surface->internal->map.info.flags & SDL_COPY_COLORKEY)) {
        return SDL_SetError("Surface doesn't have a colorkey");
    }

    if (key) {
        *key = surface->internal->map.info.colorkey;
    }
    return 0;
}

/* This is a fairly slow function to switch from colorkey to alpha
   NB: it doesn't handle bpp 1 or 3, because they have no alpha channel */
static void SDL_ConvertColorkeyToAlpha(SDL_Surface *surface, SDL_bool ignore_alpha)
{
    int x, y, bpp;

    if (!SDL_SurfaceValid(surface)) {
        return;
    }

    if (!(surface->internal->map.info.flags & SDL_COPY_COLORKEY) ||
        !SDL_ISPIXELFORMAT_ALPHA(surface->format)) {
        return;
    }

    bpp = SDL_BYTESPERPIXEL(surface->format);

    SDL_LockSurface(surface);

    if (bpp == 2) {
        Uint16 *row, *spot;
        Uint16 ckey = (Uint16)surface->internal->map.info.colorkey;
        Uint16 mask = (Uint16)(~surface->internal->format->Amask);

        /* Ignore, or not, alpha in colorkey comparison */
        if (ignore_alpha) {
            ckey &= mask;
            row = (Uint16 *)surface->pixels;
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
        } else {
            row = (Uint16 *)surface->pixels;
            for (y = surface->h; y--;) {
                spot = row;
                for (x = surface->w; x--;) {
                    if (*spot == ckey) {
                        *spot &= mask;
                    }
                    ++spot;
                }
                row += surface->pitch / 2;
            }
        }
    } else if (bpp == 4) {
        Uint32 *row, *spot;
        Uint32 ckey = surface->internal->map.info.colorkey;
        Uint32 mask = ~surface->internal->format->Amask;

        /* Ignore, or not, alpha in colorkey comparison */
        if (ignore_alpha) {
            ckey &= mask;
            row = (Uint32 *)surface->pixels;
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
        } else {
            row = (Uint32 *)surface->pixels;
            for (y = surface->h; y--;) {
                spot = row;
                for (x = surface->w; x--;) {
                    if (*spot == ckey) {
                        *spot &= mask;
                    }
                    ++spot;
                }
                row += surface->pitch / 4;
            }
        }
    }

    SDL_UnlockSurface(surface);

    SDL_SetSurfaceColorKey(surface, 0, 0);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
}

int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b)
{
    int flags;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    surface->internal->map.info.r = r;
    surface->internal->map.info.g = g;
    surface->internal->map.info.b = b;

    flags = surface->internal->map.info.flags;
    if (r != 0xFF || g != 0xFF || b != 0xFF) {
        surface->internal->map.info.flags |= SDL_COPY_MODULATE_COLOR;
    } else {
        surface->internal->map.info.flags &= ~SDL_COPY_MODULATE_COLOR;
    }
    if (surface->internal->map.info.flags != flags) {
        SDL_InvalidateMap(&surface->internal->map);
    }
    return 0;
}

int SDL_GetSurfaceColorMod(SDL_Surface *surface, Uint8 *r, Uint8 *g, Uint8 *b)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (r) {
        *r = surface->internal->map.info.r;
    }
    if (g) {
        *g = surface->internal->map.info.g;
    }
    if (b) {
        *b = surface->internal->map.info.b;
    }
    return 0;
}

int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha)
{
    int flags;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    surface->internal->map.info.a = alpha;

    flags = surface->internal->map.info.flags;
    if (alpha != 0xFF) {
        surface->internal->map.info.flags |= SDL_COPY_MODULATE_ALPHA;
    } else {
        surface->internal->map.info.flags &= ~SDL_COPY_MODULATE_ALPHA;
    }
    if (surface->internal->map.info.flags != flags) {
        SDL_InvalidateMap(&surface->internal->map);
    }
    return 0;
}

int SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (alpha) {
        *alpha = surface->internal->map.info.a;
    }
    return 0;
}

int SDL_SetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode blendMode)
{
    int flags, status;

    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    status = 0;
    flags = surface->internal->map.info.flags;
    surface->internal->map.info.flags &= ~(SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL);
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
        break;
    case SDL_BLENDMODE_BLEND:
        surface->internal->map.info.flags |= SDL_COPY_BLEND;
        break;
    case SDL_BLENDMODE_ADD:
        surface->internal->map.info.flags |= SDL_COPY_ADD;
        break;
    case SDL_BLENDMODE_MOD:
        surface->internal->map.info.flags |= SDL_COPY_MOD;
        break;
    case SDL_BLENDMODE_MUL:
        surface->internal->map.info.flags |= SDL_COPY_MUL;
        break;
    default:
        status = SDL_Unsupported();
        break;
    }

    if (surface->internal->map.info.flags != flags) {
        SDL_InvalidateMap(&surface->internal->map);
    }

    return status;
}

int SDL_GetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode *blendMode)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (!blendMode) {
        return 0;
    }

    switch (surface->internal->map.info.flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL)) {
    case SDL_COPY_BLEND:
        *blendMode = SDL_BLENDMODE_BLEND;
        break;
    case SDL_COPY_ADD:
        *blendMode = SDL_BLENDMODE_ADD;
        break;
    case SDL_COPY_MOD:
        *blendMode = SDL_BLENDMODE_MOD;
        break;
    case SDL_COPY_MUL:
        *blendMode = SDL_BLENDMODE_MUL;
        break;
    default:
        *blendMode = SDL_BLENDMODE_NONE;
        break;
    }
    return 0;
}

SDL_bool SDL_SetSurfaceClipRect(SDL_Surface *surface, const SDL_Rect *rect)
{
    SDL_Rect full_rect;

    /* Don't do anything if there's no surface to act on */
    if (!SDL_SurfaceValid(surface)) {
        return SDL_FALSE;
    }

    /* Set up the full surface rectangle */
    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = surface->w;
    full_rect.h = surface->h;

    /* Set the clipping rectangle */
    if (!rect) {
        surface->internal->clip_rect = full_rect;
        return SDL_TRUE;
    }
    return SDL_GetRectIntersection(rect, &full_rect, &surface->internal->clip_rect);
}

int SDL_GetSurfaceClipRect(SDL_Surface *surface, SDL_Rect *rect)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }
    if (!rect) {
        return SDL_InvalidParamError("rect");
    }
    *rect = surface->internal->clip_rect;
    return 0;
}

/*
 * Set up a blit between two surfaces -- split into three parts:
 * The upper part, SDL_BlitSurface(), performs clipping and rectangle
 * verification.  The lower part is a pointer to a low level
 * accelerated blitting function.
 *
 * These parts are separated out and each used internally by this
 * library in the optimum places.  They are exported so that if
 * you know exactly what you are doing, you can optimize your code
 * by calling the one(s) you need.
 */
int SDL_BlitSurfaceUnchecked(SDL_Surface *src, const SDL_Rect *srcrect,
                             SDL_Surface *dst, const SDL_Rect *dstrect)
{
    /* Check to make sure the blit mapping is valid */
    if ((src->internal->map.dst != dst) ||
        (dst->internal->palette &&
         src->internal->map.dst_palette_version != dst->internal->palette->version) ||
        (src->internal->palette &&
         src->internal->map.src_palette_version != src->internal->palette->version)) {
        if (SDL_MapSurface(src, dst) < 0) {
            return -1;
        }
        /* just here for debugging */
        /*         printf */
        /*             ("src = 0x%08X src->flags = %08X src->internal->map.info.flags = %08x\ndst = 0x%08X dst->flags = %08X dst->internal->map.info.flags = %08X\nsrc->internal->map.blit = 0x%08x\n", */
        /*              src, dst->flags, src->internal->map.info.flags, dst, dst->flags, */
        /*              dst->internal->map.info.flags, src->internal->map.blit); */
    }
    return src->internal->map.blit(src, srcrect, dst, dstrect);
}

int SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect)
{
    SDL_Rect r_src, r_dst;

    /* Make sure the surfaces aren't locked */
    if (!SDL_SurfaceValid(src)) {
        return SDL_InvalidParamError("src");
    } else if (!SDL_SurfaceValid(dst)) {
        return SDL_InvalidParamError("dst");
    } else if ((src->flags & SDL_SURFACE_LOCKED) || (dst->flags & SDL_SURFACE_LOCKED)) {
        return SDL_SetError("Surfaces must not be locked during blit");
    }

    /* Full src surface */
    r_src.x = 0;
    r_src.y = 0;
    r_src.w = src->w;
    r_src.h = src->h;

    if (dstrect) {
        r_dst.x = dstrect->x;
        r_dst.y = dstrect->y;
    } else {
        r_dst.x = 0;
        r_dst.y = 0;
    }

    /* clip the source rectangle to the source surface */
    if (srcrect) {
        SDL_Rect tmp;
        if (SDL_GetRectIntersection(srcrect, &r_src, &tmp) == SDL_FALSE) {
            goto end;
        }

        /* Shift dstrect, if srcrect origin has changed */
        r_dst.x += tmp.x - srcrect->x;
        r_dst.y += tmp.y - srcrect->y;

        /* Update srcrect */
        r_src = tmp;
    }

    /* There're no dstrect.w/h parameters. It's the same as srcrect */
    r_dst.w = r_src.w;
    r_dst.h = r_src.h;

    /* clip the destination rectangle against the clip rectangle */
    {
        SDL_Rect tmp;
        if (SDL_GetRectIntersection(&r_dst, &dst->internal->clip_rect, &tmp) == SDL_FALSE) {
            goto end;
        }

        /* Shift srcrect, if dstrect has changed */
        r_src.x += tmp.x - r_dst.x;
        r_src.y += tmp.y - r_dst.y;
        r_src.w = tmp.w;
        r_src.h = tmp.h;

        /* Update dstrect */
        r_dst = tmp;
    }

    /* Switch back to a fast blit if we were previously stretching */
    if (src->internal->map.info.flags & SDL_COPY_NEAREST) {
        src->internal->map.info.flags &= ~SDL_COPY_NEAREST;
        SDL_InvalidateMap(&src->internal->map);
    }

    if (r_dst.w > 0 && r_dst.h > 0) {
        if (dstrect) { /* update output parameter */
            *dstrect = r_dst;
        }
        return SDL_BlitSurfaceUnchecked(src, &r_src, dst, &r_dst);
    }

end:
    if (dstrect) { /* update output parameter */
        dstrect->w = dstrect->h = 0;
    }
    return 0;
}

int SDL_BlitSurfaceScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                          SDL_Surface *dst, SDL_Rect *dstrect,
                          SDL_ScaleMode scaleMode)
{
    SDL_Rect *clip_rect;
    double src_x0, src_y0, src_x1, src_y1;
    double dst_x0, dst_y0, dst_x1, dst_y1;
    SDL_Rect final_src, final_dst;
    double scaling_w, scaling_h;
    int src_w, src_h;
    int dst_w, dst_h;

    /* Make sure the surfaces aren't locked */
    if (!SDL_SurfaceValid(src)) {
        return SDL_InvalidParamError("src");
    } else if (!SDL_SurfaceValid(dst)) {
        return SDL_InvalidParamError("dst");
    } else if ((src->flags & SDL_SURFACE_LOCKED) || (dst->flags & SDL_SURFACE_LOCKED)) {
        return SDL_SetError("Surfaces must not be locked during blit");
    }

    if (!srcrect) {
        src_w = src->w;
        src_h = src->h;
    } else {
        src_w = srcrect->w;
        src_h = srcrect->h;
    }

    if (!dstrect) {
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

    if (!dstrect) {
        dst_x0 = 0;
        dst_y0 = 0;
        dst_x1 = dst_w;
        dst_y1 = dst_h;
    } else {
        dst_x0 = dstrect->x;
        dst_y0 = dstrect->y;
        dst_x1 = dst_x0 + dst_w;
        dst_y1 = dst_y0 + dst_h;
    }

    if (!srcrect) {
        src_x0 = 0;
        src_y0 = 0;
        src_x1 = src_w;
        src_y1 = src_h;
    } else {
        src_x0 = srcrect->x;
        src_y0 = srcrect->y;
        src_x1 = src_x0 + src_w;
        src_y1 = src_y0 + src_h;

        /* Clip source rectangle to the source surface */

        if (src_x0 < 0) {
            dst_x0 -= src_x0 * scaling_w;
            src_x0 = 0;
        }

        if (src_x1 > src->w) {
            dst_x1 -= (src_x1 - src->w) * scaling_w;
            src_x1 = src->w;
        }

        if (src_y0 < 0) {
            dst_y0 -= src_y0 * scaling_h;
            src_y0 = 0;
        }

        if (src_y1 > src->h) {
            dst_y1 -= (src_y1 - src->h) * scaling_h;
            src_y1 = src->h;
        }
    }

    /* Clip destination rectangle to the clip rectangle */
    clip_rect = &dst->internal->clip_rect;

    /* Translate to clip space for easier calculations */
    dst_x0 -= clip_rect->x;
    dst_x1 -= clip_rect->x;
    dst_y0 -= clip_rect->y;
    dst_y1 -= clip_rect->y;

    if (dst_x0 < 0) {
        src_x0 -= dst_x0 / scaling_w;
        dst_x0 = 0;
    }

    if (dst_x1 > clip_rect->w) {
        src_x1 -= (dst_x1 - clip_rect->w) / scaling_w;
        dst_x1 = clip_rect->w;
    }

    if (dst_y0 < 0) {
        src_y0 -= dst_y0 / scaling_h;
        dst_y0 = 0;
    }

    if (dst_y1 > clip_rect->h) {
        src_y1 -= (dst_y1 - clip_rect->h) / scaling_h;
        dst_y1 = clip_rect->h;
    }

    /* Translate back to surface coordinates */
    dst_x0 += clip_rect->x;
    dst_x1 += clip_rect->x;
    dst_y0 += clip_rect->y;
    dst_y1 += clip_rect->y;

    final_src.x = (int)SDL_round(src_x0);
    final_src.y = (int)SDL_round(src_y0);
    final_src.w = (int)SDL_round(src_x1 - src_x0);
    final_src.h = (int)SDL_round(src_y1 - src_y0);

    final_dst.x = (int)SDL_round(dst_x0);
    final_dst.y = (int)SDL_round(dst_y0);
    final_dst.w = (int)SDL_round(dst_x1 - dst_x0);
    final_dst.h = (int)SDL_round(dst_y1 - dst_y0);

    /* Clip again */
    {
        SDL_Rect tmp;
        tmp.x = 0;
        tmp.y = 0;
        tmp.w = src->w;
        tmp.h = src->h;
        SDL_GetRectIntersection(&tmp, &final_src, &final_src);
    }

    /* Clip again */
    SDL_GetRectIntersection(clip_rect, &final_dst, &final_dst);

    if (dstrect) {
        *dstrect = final_dst;
    }

    if (final_dst.w == 0 || final_dst.h == 0 ||
        final_src.w <= 0 || final_src.h <= 0) {
        /* No-op. */
        return 0;
    }

    return SDL_BlitSurfaceUncheckedScaled(src, &final_src, dst, &final_dst, scaleMode);
}

/**
 *  This is a semi-private blit function and it performs low-level surface
 *  scaled blitting only.
 */
int SDL_BlitSurfaceUncheckedScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                                   SDL_Surface *dst, const SDL_Rect *dstrect,
                                   SDL_ScaleMode scaleMode)
{
    static const Uint32 complex_copy_flags = (SDL_COPY_MODULATE_COLOR | SDL_COPY_MODULATE_ALPHA |
                                              SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL |
                                              SDL_COPY_COLORKEY);

    if (scaleMode != SDL_SCALEMODE_NEAREST && scaleMode != SDL_SCALEMODE_LINEAR && scaleMode != SDL_SCALEMODE_BEST) {
        return SDL_InvalidParamError("scaleMode");
    }

    if (scaleMode != SDL_SCALEMODE_NEAREST) {
        scaleMode = SDL_SCALEMODE_LINEAR;
    }

    if (srcrect->w > SDL_MAX_UINT16 || srcrect->h > SDL_MAX_UINT16 ||
        dstrect->w > SDL_MAX_UINT16 || dstrect->h > SDL_MAX_UINT16) {
        return SDL_SetError("Size too large for scaling");
    }

    if (!(src->internal->map.info.flags & SDL_COPY_NEAREST)) {
        src->internal->map.info.flags |= SDL_COPY_NEAREST;
        SDL_InvalidateMap(&src->internal->map);
    }

    if (scaleMode == SDL_SCALEMODE_NEAREST) {
        if (!(src->internal->map.info.flags & complex_copy_flags) &&
            src->format == dst->format &&
            !SDL_ISPIXELFORMAT_INDEXED(src->format)) {
            return SDL_SoftStretch(src, srcrect, dst, dstrect, SDL_SCALEMODE_NEAREST);
        } else {
            return SDL_BlitSurfaceUnchecked(src, srcrect, dst, dstrect);
        }
    } else {
        if (!(src->internal->map.info.flags & complex_copy_flags) &&
            src->format == dst->format &&
            !SDL_ISPIXELFORMAT_INDEXED(src->format) &&
            SDL_BYTESPERPIXEL(src->format) == 4 &&
            src->format != SDL_PIXELFORMAT_ARGB2101010) {
            /* fast path */
            return SDL_SoftStretch(src, srcrect, dst, dstrect, SDL_SCALEMODE_LINEAR);
        } else {
            /* Use intermediate surface(s) */
            SDL_Surface *tmp1 = NULL;
            int ret;
            SDL_Rect srcrect2;
            int is_complex_copy_flags = (src->internal->map.info.flags & complex_copy_flags);

            Uint8 r, g, b;
            Uint8 alpha;
            SDL_BlendMode blendMode;

            /* Save source infos */
            SDL_GetSurfaceColorMod(src, &r, &g, &b);
            SDL_GetSurfaceAlphaMod(src, &alpha);
            SDL_GetSurfaceBlendMode(src, &blendMode);
            srcrect2.x = srcrect->x;
            srcrect2.y = srcrect->y;
            srcrect2.w = srcrect->w;
            srcrect2.h = srcrect->h;

            /* Change source format if not appropriate for scaling */
            if (SDL_BYTESPERPIXEL(src->format) != 4 || src->format == SDL_PIXELFORMAT_ARGB2101010) {
                SDL_Rect tmprect;
                SDL_PixelFormat fmt;
                tmprect.x = 0;
                tmprect.y = 0;
                tmprect.w = src->w;
                tmprect.h = src->h;
                if (SDL_BYTESPERPIXEL(dst->format) == 4 && dst->format != SDL_PIXELFORMAT_ARGB2101010) {
                    fmt = dst->format;
                } else {
                    fmt = SDL_PIXELFORMAT_ARGB8888;
                }
                tmp1 = SDL_CreateSurface(src->w, src->h, fmt);
                SDL_BlitSurfaceUnchecked(src, srcrect, tmp1, &tmprect);

                srcrect2.x = 0;
                srcrect2.y = 0;
                SDL_SetSurfaceColorMod(tmp1, r, g, b);
                SDL_SetSurfaceAlphaMod(tmp1, alpha);
                SDL_SetSurfaceBlendMode(tmp1, blendMode);

                src = tmp1;
            }

            /* Intermediate scaling */
            if (is_complex_copy_flags || src->format != dst->format) {
                SDL_Rect tmprect;
                SDL_Surface *tmp2 = SDL_CreateSurface(dstrect->w, dstrect->h, src->format);
                SDL_SoftStretch(src, &srcrect2, tmp2, NULL, SDL_SCALEMODE_LINEAR);

                SDL_SetSurfaceColorMod(tmp2, r, g, b);
                SDL_SetSurfaceAlphaMod(tmp2, alpha);
                SDL_SetSurfaceBlendMode(tmp2, blendMode);

                tmprect.x = 0;
                tmprect.y = 0;
                tmprect.w = dstrect->w;
                tmprect.h = dstrect->h;
                ret = SDL_BlitSurfaceUnchecked(tmp2, &tmprect, dst, dstrect);
                SDL_DestroySurface(tmp2);
            } else {
                ret = SDL_SoftStretch(src, &srcrect2, dst, dstrect, SDL_SCALEMODE_LINEAR);
            }

            SDL_DestroySurface(tmp1);
            return ret;
        }
    }
}

/*
 * Lock a surface to directly access the pixels
 */
int SDL_LockSurface(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }

    if (!surface->internal->locked) {
#if SDL_HAVE_RLE
        /* Perform the lock */
        if (surface->internal->flags & SDL_INTERNAL_SURFACE_RLEACCEL) {
            SDL_UnRLESurface(surface, SDL_TRUE);
            surface->internal->flags |= SDL_INTERNAL_SURFACE_RLEACCEL; /* save accel'd state */
            SDL_UpdateSurfaceLockFlag(surface);
        }
#endif
    }

    /* Increment the surface lock count, for recursive locks */
    ++surface->internal->locked;
    surface->flags |= SDL_SURFACE_LOCKED;

    /* Ready to go.. */
    return 0;
}

/*
 * Unlock a previously locked surface
 */
void SDL_UnlockSurface(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return;
    }

    /* Only perform an unlock if we are locked */
    if (!surface->internal->locked || (--surface->internal->locked > 0)) {
        return;
    }

#if SDL_HAVE_RLE
    /* Update RLE encoded surface with new data */
    if (surface->internal->flags & SDL_INTERNAL_SURFACE_RLEACCEL) {
        surface->internal->flags &= ~SDL_INTERNAL_SURFACE_RLEACCEL; /* stop lying */
        SDL_RLESurface(surface);
    }
#endif

    surface->flags &= ~SDL_SURFACE_LOCKED;
}

static int SDL_FlipSurfaceHorizontal(SDL_Surface *surface)
{
    SDL_bool isstack;
    Uint8 *row, *a, *b, *tmp;
    int i, j, bpp;

    if (SDL_BITSPERPIXEL(surface->format) < 8) {
        /* We could implement this if needed, but we'd have to flip sets of bits within a byte */
        return SDL_Unsupported();
    }

    if (surface->h <= 0) {
        return 0;
    }

    if (surface->w <= 1) {
        return 0;
    }

    bpp = SDL_BYTESPERPIXEL(surface->format);
    row = (Uint8 *)surface->pixels;
    tmp = SDL_small_alloc(Uint8, surface->pitch, &isstack);
    for (i = surface->h; i--; ) {
        a = row;
        b = a + (surface->w - 1) * bpp;
        for (j = surface->w / 2; j--; ) {
            SDL_memcpy(tmp, a, bpp);
            SDL_memcpy(a, b, bpp);
            SDL_memcpy(b, tmp, bpp);
            a += bpp;
            b -= bpp;
        }
        row += surface->pitch;
    }
    SDL_small_free(tmp, isstack);
    return 0;
}

static int SDL_FlipSurfaceVertical(SDL_Surface *surface)
{
    SDL_bool isstack;
    Uint8 *a, *b, *tmp;
    int i;

    if (surface->h <= 1) {
        return 0;
    }

    a = (Uint8 *)surface->pixels;
    b = a + (surface->h - 1) * surface->pitch;
    tmp = SDL_small_alloc(Uint8, surface->pitch, &isstack);
    for (i = surface->h / 2; i--; ) {
        SDL_memcpy(tmp, a, surface->pitch);
        SDL_memcpy(a, b, surface->pitch);
        SDL_memcpy(b, tmp, surface->pitch);
        a += surface->pitch;
        b -= surface->pitch;
    }
    SDL_small_free(tmp, isstack);
    return 0;
}

int SDL_FlipSurface(SDL_Surface *surface, SDL_FlipMode flip)
{
    if (!SDL_SurfaceValid(surface)) {
        return SDL_InvalidParamError("surface");
    }
    if (!surface->pixels) {
        return 0;
    }

    switch (flip) {
    case SDL_FLIP_HORIZONTAL:
        return SDL_FlipSurfaceHorizontal(surface);
    case SDL_FLIP_VERTICAL:
        return SDL_FlipSurfaceVertical(surface);
    default:
        return SDL_InvalidParamError("flip");
    }
}

SDL_Surface *SDL_ConvertSurfaceAndColorspace(SDL_Surface *surface, SDL_PixelFormat format, const SDL_Palette *palette, SDL_Colorspace colorspace, SDL_PropertiesID props)
{
    SDL_Surface *convert;
    SDL_Colorspace src_colorspace;
    SDL_PropertiesID src_properties;
    Uint32 copy_flags;
    SDL_Color copy_color;
    SDL_Rect bounds;
    int ret;
    SDL_bool palette_ck_transform = SDL_FALSE;
    Uint8 palette_ck_value = 0;
    SDL_bool palette_has_alpha = SDL_FALSE;
    Uint8 *palette_saved_alpha = NULL;
    int palette_saved_alpha_ncolors = 0;

    if (!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        return NULL;
    }

    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_InvalidParamError("format");
        return NULL;
    }

    /* Check for empty destination palette! (results in empty image) */
    if (palette) {
        int i;
        for (i = 0; i < palette->ncolors; ++i) {
            if ((palette->colors[i].r != 0xFF) || (palette->colors[i].g != 0xFF) || (palette->colors[i].b != 0xFF)) {
                break;
            }
        }
        if (i == palette->ncolors) {
            SDL_SetError("Empty destination palette");
            return NULL;
        }
    }

    src_colorspace = SDL_GetSurfaceColorspace(surface);
    src_properties = surface->internal->props;

    /* Create a new surface with the desired format */
    convert = SDL_CreateSurface(surface->w, surface->h, format);
    if (!convert) {
        return NULL;
    }

    if (colorspace == SDL_COLORSPACE_UNKNOWN) {
        colorspace = src_colorspace;
    }
    SDL_SetSurfaceColorspace(convert, colorspace);

    if (SDL_ISPIXELFORMAT_FOURCC(format) || SDL_ISPIXELFORMAT_FOURCC(surface->format)) {
        if (SDL_ConvertPixelsAndColorspace(surface->w, surface->h, surface->format, src_colorspace, src_properties, surface->pixels, surface->pitch, convert->format, colorspace, props, convert->pixels, convert->pitch) < 0) {
            SDL_DestroySurface(convert);
            return NULL;
        }

        /* Save the original copy flags */
        copy_flags = surface->internal->map.info.flags;

        goto end;
    }

    /* Copy the palette if any */
    if (palette && convert->internal->palette) {
        SDL_memcpy(convert->internal->palette->colors,
                   palette->colors,
                   palette->ncolors * sizeof(SDL_Color));
        convert->internal->palette->ncolors = palette->ncolors;
    }

    /* Save the original copy flags */
    copy_flags = surface->internal->map.info.flags;
    copy_color.r = surface->internal->map.info.r;
    copy_color.g = surface->internal->map.info.g;
    copy_color.b = surface->internal->map.info.b;
    copy_color.a = surface->internal->map.info.a;
    surface->internal->map.info.r = 0xFF;
    surface->internal->map.info.g = 0xFF;
    surface->internal->map.info.b = 0xFF;
    surface->internal->map.info.a = 0xFF;
    surface->internal->map.info.flags = (copy_flags & (SDL_COPY_RLE_COLORKEY | SDL_COPY_RLE_ALPHAKEY));
    SDL_InvalidateMap(&surface->internal->map);

    /* Copy over the image data */
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = surface->w;
    bounds.h = surface->h;

    /* Source surface has a palette with no real alpha (0 or OPAQUE).
     * Destination format has alpha.
     * -> set alpha channel to be opaque */
    if (surface->internal->palette && SDL_ISPIXELFORMAT_ALPHA(format)) {
        SDL_bool set_opaque = SDL_FALSE;

        SDL_bool is_opaque, has_alpha_channel;
        SDL_DetectPalette(surface->internal->palette, &is_opaque, &has_alpha_channel);

        if (is_opaque) {
            if (!has_alpha_channel) {
                set_opaque = SDL_TRUE;
            }
        } else {
            palette_has_alpha = SDL_TRUE;
        }

        /* Set opaque and backup palette alpha values */
        if (set_opaque) {
            int i;
            palette_saved_alpha_ncolors = surface->internal->palette->ncolors;
            if (palette_saved_alpha_ncolors > 0) {
                palette_saved_alpha = SDL_stack_alloc(Uint8, palette_saved_alpha_ncolors);
                for (i = 0; i < palette_saved_alpha_ncolors; i++) {
                    palette_saved_alpha[i] = surface->internal->palette->colors[i].a;
                    surface->internal->palette->colors[i].a = SDL_ALPHA_OPAQUE;
                }
            }
        }
    }

    /* Transform colorkey to alpha. for cases where source palette has duplicate values, and colorkey is one of them */
    if (copy_flags & SDL_COPY_COLORKEY) {
        if (surface->internal->palette && !palette) {
            palette_ck_transform = SDL_TRUE;
            palette_has_alpha = SDL_TRUE;
            palette_ck_value = surface->internal->palette->colors[surface->internal->map.info.colorkey].a;
            surface->internal->palette->colors[surface->internal->map.info.colorkey].a = SDL_ALPHA_TRANSPARENT;
        }
    }

    ret = SDL_BlitSurfaceUnchecked(surface, &bounds, convert, &bounds);

    /* Restore colorkey alpha value */
    if (palette_ck_transform) {
        surface->internal->palette->colors[surface->internal->map.info.colorkey].a = palette_ck_value;
    }

    /* Restore palette alpha values */
    if (palette_saved_alpha) {
        int i;
        for (i = 0; i < palette_saved_alpha_ncolors; i++) {
            surface->internal->palette->colors[i].a = palette_saved_alpha[i];
        }
        SDL_stack_free(palette_saved_alpha);
    }

    /* Clean up the original surface, and update converted surface */
    convert->internal->map.info.r = copy_color.r;
    convert->internal->map.info.g = copy_color.g;
    convert->internal->map.info.b = copy_color.b;
    convert->internal->map.info.a = copy_color.a;
    convert->internal->map.info.flags =
        (copy_flags &
         ~(SDL_COPY_COLORKEY | SDL_COPY_BLEND | SDL_COPY_RLE_DESIRED | SDL_COPY_RLE_COLORKEY |
           SDL_COPY_RLE_ALPHAKEY));
    surface->internal->map.info.r = copy_color.r;
    surface->internal->map.info.g = copy_color.g;
    surface->internal->map.info.b = copy_color.b;
    surface->internal->map.info.a = copy_color.a;
    surface->internal->map.info.flags = copy_flags;
    SDL_InvalidateMap(&surface->internal->map);

    /* SDL_BlitSurfaceUnchecked failed, and so the conversion */
    if (ret < 0) {
        SDL_DestroySurface(convert);
        return NULL;
    }

    if (copy_flags & SDL_COPY_COLORKEY) {
        SDL_bool set_colorkey_by_color = SDL_FALSE;
        SDL_bool convert_colorkey = SDL_TRUE;

        if (surface->internal->palette) {
            if (palette &&
                surface->internal->palette->ncolors <= palette->ncolors &&
                (SDL_memcmp(surface->internal->palette->colors, palette->colors,
                            surface->internal->palette->ncolors * sizeof(SDL_Color)) == 0)) {
                /* The palette is identical, just set the same colorkey */
                SDL_SetSurfaceColorKey(convert, 1, surface->internal->map.info.colorkey);
            } else if (!palette) {
                if (SDL_ISPIXELFORMAT_ALPHA(format)) {
                    /* No need to add the colorkey, transparency is in the alpha channel*/
                } else {
                    /* Only set the colorkey information */
                    set_colorkey_by_color = SDL_TRUE;
                    convert_colorkey = SDL_FALSE;
                }
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
            tmp = SDL_CreateSurface(1, 1, surface->format);
            if (!tmp) {
                SDL_DestroySurface(convert);
                return NULL;
            }

            /* Share the palette, if any */
            if (surface->internal->palette) {
                SDL_SetSurfacePalette(tmp, surface->internal->palette);
            }

            SDL_FillSurfaceRect(tmp, NULL, surface->internal->map.info.colorkey);

            tmp->internal->map.info.flags &= ~SDL_COPY_COLORKEY;

            /* Conversion of the colorkey */
            tmp2 = SDL_ConvertSurfaceAndColorspace(tmp, format, palette, colorspace, props);
            if (!tmp2) {
                SDL_DestroySurface(tmp);
                SDL_DestroySurface(convert);
                return NULL;
            }

            /* Get the converted colorkey */
            SDL_memcpy(&converted_colorkey, tmp2->pixels, tmp2->internal->format->bytes_per_pixel);

            SDL_DestroySurface(tmp);
            SDL_DestroySurface(tmp2);

            /* Set the converted colorkey on the new surface */
            SDL_SetSurfaceColorKey(convert, 1, converted_colorkey);

            /* This is needed when converting for 3D texture upload */
            if (convert_colorkey) {
                SDL_ConvertColorkeyToAlpha(convert, SDL_TRUE);
            }
        }
    }

end:

    SDL_SetSurfaceClipRect(convert, &surface->internal->clip_rect);

    /* Enable alpha blending by default if the new surface has an
     * alpha channel or alpha modulation */
    if ((SDL_ISPIXELFORMAT_ALPHA(surface->format) && SDL_ISPIXELFORMAT_ALPHA(format)) ||
        (palette_has_alpha && SDL_ISPIXELFORMAT_ALPHA(format)) ||
        (copy_flags & SDL_COPY_MODULATE_ALPHA)) {
        SDL_SetSurfaceBlendMode(convert, SDL_BLENDMODE_BLEND);
    }
    if (copy_flags & SDL_COPY_RLE_DESIRED) {
        SDL_SetSurfaceRLE(convert, SDL_TRUE);
    }

    /* We're ready to go! */
    return convert;
}

SDL_Surface *SDL_DuplicateSurface(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        return NULL;
    }

    return SDL_ConvertSurfaceAndColorspace(surface, surface->format, surface->internal->palette, SDL_GetSurfaceColorspace(surface), surface->internal->props);
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *surface, SDL_PixelFormat format)
{
    if (!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        return NULL;
    }

    return SDL_ConvertSurfaceAndColorspace(surface, format, NULL, SDL_GetDefaultColorspaceForFormat(format), surface->internal->props);
}

SDL_Surface *SDL_DuplicatePixels(int width, int height, SDL_PixelFormat format, SDL_Colorspace colorspace, void *pixels, int pitch)
{
    SDL_Surface *surface = SDL_CreateSurface(width, height, format);
    if (surface) {
        int length = width * SDL_BYTESPERPIXEL(format);
        Uint8 *src = (Uint8 *)pixels;
        Uint8 *dst = (Uint8 *)surface->pixels;
        int rows = height;
        while (rows--) {
            SDL_memcpy(dst, src, length);
            dst += surface->pitch;
            src += pitch;
        }

        SDL_SetSurfaceColorspace(surface, colorspace);
    }
    return surface;
}

int SDL_ConvertPixelsAndColorspace(int width, int height,
                      SDL_PixelFormat src_format, SDL_Colorspace src_colorspace, SDL_PropertiesID src_properties, const void *src, int src_pitch,
                      SDL_PixelFormat dst_format, SDL_Colorspace dst_colorspace, SDL_PropertiesID dst_properties, void *dst, int dst_pitch)
{
    SDL_InternalSurface src_data, dst_data;
    SDL_Surface *src_surface;
    SDL_Surface *dst_surface;
    SDL_Rect rect;
    void *nonconst_src = (void *)src;
    int ret;

    if (!src) {
        return SDL_InvalidParamError("src");
    }
    if (!src_pitch) {
        return SDL_InvalidParamError("src_pitch");
    }
    if (!dst) {
        return SDL_InvalidParamError("dst");
    }
    if (!dst_pitch) {
        return SDL_InvalidParamError("dst_pitch");
    }

    if (src_colorspace == SDL_COLORSPACE_UNKNOWN) {
        src_colorspace = SDL_GetDefaultColorspaceForFormat(src_format);
    }
    if (dst_colorspace == SDL_COLORSPACE_UNKNOWN) {
        dst_colorspace = SDL_GetDefaultColorspaceForFormat(dst_format);
    }

#if SDL_HAVE_YUV
    if (SDL_ISPIXELFORMAT_FOURCC(src_format) && SDL_ISPIXELFORMAT_FOURCC(dst_format)) {
        return SDL_ConvertPixels_YUV_to_YUV(width, height, src_format, src_colorspace, src_properties, src, src_pitch, dst_format, dst_colorspace, dst_properties, dst, dst_pitch);
    } else if (SDL_ISPIXELFORMAT_FOURCC(src_format)) {
        return SDL_ConvertPixels_YUV_to_RGB(width, height, src_format, src_colorspace, src_properties, src, src_pitch, dst_format, dst_colorspace, dst_properties, dst, dst_pitch);
    } else if (SDL_ISPIXELFORMAT_FOURCC(dst_format)) {
        return SDL_ConvertPixels_RGB_to_YUV(width, height, src_format, src_colorspace, src_properties, src, src_pitch, dst_format, dst_colorspace, dst_properties, dst, dst_pitch);
    }
#else
    if (SDL_ISPIXELFORMAT_FOURCC(src_format) || SDL_ISPIXELFORMAT_FOURCC(dst_format)) {
        return SDL_SetError("SDL not built with YUV support");
    }
#endif

    /* Fast path for same format copy */
    if (src_format == dst_format && src_colorspace == dst_colorspace) {
        int i;
        const int bpp = SDL_BYTESPERPIXEL(src_format);
        width *= bpp;
        for (i = height; i--;) {
            SDL_memcpy(dst, src, width);
            src = (const Uint8 *)src + src_pitch;
            dst = (Uint8 *)dst + dst_pitch;
        }
        return 0;
    }

    src_surface = SDL_InitializeSurface(&src_data, width, height, src_format, src_colorspace, src_properties, nonconst_src, src_pitch, SDL_TRUE);
    if (!src_surface) {
        return -1;
    }
    SDL_SetSurfaceBlendMode(src_surface, SDL_BLENDMODE_NONE);

    dst_surface = SDL_InitializeSurface(&dst_data, width, height, dst_format, dst_colorspace, dst_properties, dst, dst_pitch, SDL_TRUE);
    if (!dst_surface) {
        return -1;
    }

    /* Set up the rect and go! */
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;
    ret = SDL_BlitSurfaceUnchecked(src_surface, &rect, dst_surface, &rect);

    SDL_DestroySurface(src_surface);
    SDL_DestroySurface(dst_surface);

    return ret;
}

int SDL_ConvertPixels(int width, int height,
                      SDL_PixelFormat src_format, const void *src, int src_pitch,
                      SDL_PixelFormat dst_format, void *dst, int dst_pitch)
{
    return SDL_ConvertPixelsAndColorspace(width, height,
                      src_format, SDL_COLORSPACE_UNKNOWN, 0, src, src_pitch,
                      dst_format, SDL_COLORSPACE_UNKNOWN, 0, dst, dst_pitch);
}

/*
 * Premultiply the alpha on a block of pixels
 *
 * This is currently only implemented for SDL_PIXELFORMAT_ARGB8888
 *
 * Here are some ideas for optimization:
 * https://github.com/Wizermil/premultiply_alpha/tree/master/premultiply_alpha
 * https://developer.arm.com/documentation/101964/0201/Pre-multiplied-alpha-channel-data
 */
int SDL_PremultiplyAlpha(int width, int height,
                         SDL_PixelFormat src_format, const void *src, int src_pitch,
                         SDL_PixelFormat dst_format, void *dst, int dst_pitch)
{
    int c;
    Uint32 srcpixel;
    Uint32 srcR, srcG, srcB, srcA;
    Uint32 dstpixel;
    Uint32 dstR, dstG, dstB, dstA;

    if (!src) {
        return SDL_InvalidParamError("src");
    }
    if (!src_pitch) {
        return SDL_InvalidParamError("src_pitch");
    }
    if (!dst) {
        return SDL_InvalidParamError("dst");
    }
    if (!dst_pitch) {
        return SDL_InvalidParamError("dst_pitch");
    }
    if (src_format != SDL_PIXELFORMAT_ARGB8888) {
        return SDL_InvalidParamError("src_format");
    }
    if (dst_format != SDL_PIXELFORMAT_ARGB8888) {
        return SDL_InvalidParamError("dst_format");
    }

    while (height--) {
        const Uint32 *src_px = (const Uint32 *)src;
        Uint32 *dst_px = (Uint32 *)dst;
        for (c = width; c; --c) {
            /* Component bytes extraction. */
            srcpixel = *src_px++;
            RGBA_FROM_ARGB8888(srcpixel, srcR, srcG, srcB, srcA);

            /* Alpha pre-multiplication of each component. */
            dstA = srcA;
            dstR = (srcA * srcR) / 255;
            dstG = (srcA * srcG) / 255;
            dstB = (srcA * srcB) / 255;

            /* ARGB8888 pixel recomposition. */
            ARGB8888_FROM_RGBA(dstpixel, dstR, dstG, dstB, dstA);
            *dst_px++ = dstpixel;
        }
        src = (const Uint8 *)src + src_pitch;
        dst = (Uint8 *)dst + dst_pitch;
    }
    return 0;
}

Uint32 SDL_MapSurfaceRGB(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b)
{
    return SDL_MapSurfaceRGBA(surface, r, g, b, SDL_ALPHA_OPAQUE);
}

Uint32 SDL_MapSurfaceRGBA(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        return 0;
    }
    return SDL_MapRGBA(surface->internal->format, surface->internal->palette, r, g, b, a);
}

/* This function Copyright 2023 Collabora Ltd., contributed to SDL under the ZLib license */
int SDL_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    Uint32 pixel = 0;
    size_t bytes_per_pixel;
    Uint8 unused;
    Uint8 *p;
    int result = -1;

    if (!surface || !surface->format || !surface->pixels) {
        return SDL_InvalidParamError("surface");
    }

    if (x < 0 || x >= surface->w) {
        return SDL_InvalidParamError("x");
    }

    if (y < 0 || y >= surface->h) {
        return SDL_InvalidParamError("y");
    }

    if (!r) {
        r = &unused;
    }

    if (!g) {
        g = &unused;
    }

    if (!b) {
        b = &unused;
    }

    if (!a) {
        a = &unused;
    }

    bytes_per_pixel = SDL_BYTESPERPIXEL(surface->format);

    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }

    p = (Uint8 *)surface->pixels + y * surface->pitch + x * bytes_per_pixel;

    if (bytes_per_pixel > sizeof(pixel)) {
        /* This is really slow, but it gets the job done */
        Uint8 rgba[4];
        SDL_Colorspace colorspace = SDL_GetSurfaceColorspace(surface);

        if (SDL_ConvertPixelsAndColorspace(1, 1, surface->format, colorspace, surface->internal->props, p, surface->pitch, SDL_PIXELFORMAT_RGBA32, SDL_COLORSPACE_SRGB, 0, rgba, sizeof(rgba)) == 0) {
            *r = rgba[0];
            *g = rgba[1];
            *b = rgba[2];
            *a = rgba[3];
            result = 0;
        }
    } else {
        /* Fill the appropriate number of least-significant bytes of pixel,
         * leaving the most-significant bytes set to zero */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        SDL_memcpy(((Uint8 *)&pixel) + (sizeof(pixel) - bytes_per_pixel), p, bytes_per_pixel);
#else
        SDL_memcpy(&pixel, p, bytes_per_pixel);
#endif
        SDL_GetRGBA(pixel, surface->internal->format, surface->internal->palette, r, g, b, a);
        result = 0;
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
    return result;
}

/*
 * Free a surface created by the above function.
 */
void SDL_DestroySurface(SDL_Surface *surface)
{
    if (!SDL_SurfaceValid(surface)) {
        return;
    }
    if (surface->internal->flags & SDL_INTERNAL_SURFACE_DONTFREE) {
        return;
    }
    if (--surface->refcount > 0) {
        return;
    }

    SDL_DestroyProperties(surface->internal->props);

    SDL_InvalidateMap(&surface->internal->map);
    SDL_InvalidateAllBlitMap(surface);

    while (surface->internal->locked > 0) {
        SDL_UnlockSurface(surface);
    }
#if SDL_HAVE_RLE
    if (surface->internal->flags & SDL_INTERNAL_SURFACE_RLEACCEL) {
        SDL_UnRLESurface(surface, SDL_FALSE);
    }
#endif
    SDL_SetSurfacePalette(surface, NULL);

    if (surface->flags & SDL_SURFACE_PREALLOCATED) {
        /* Don't free */
    } else if (surface->flags & SDL_SURFACE_SIMD_ALIGNED) {
        /* Free aligned */
        SDL_aligned_free(surface->pixels);
    } else {
        /* Normal */
        SDL_free(surface->pixels);
    }
    if (!(surface->internal->flags & SDL_INTERNAL_SURFACE_STACK)) {
        SDL_free(surface);
    }
}
