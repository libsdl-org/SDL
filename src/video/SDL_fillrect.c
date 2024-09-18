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

#include "SDL_blit.h"

#ifdef SDL_SSE_INTRINSICS
/* *INDENT-OFF* */ // clang-format off

#if defined(_MSC_VER) && !defined(__clang__)
#define SSE_BEGIN \
    __m128 c128; \
    c128.m128_u32[0] = color; \
    c128.m128_u32[1] = color; \
    c128.m128_u32[2] = color; \
    c128.m128_u32[3] = color;
#else
#define SSE_BEGIN \
    __m128 c128; \
    DECLARE_ALIGNED(uint32_t, cccc[4], 16); \
    cccc[0] = color; \
    cccc[1] = color; \
    cccc[2] = color; \
    cccc[3] = color; \
    c128 = *(__m128 *)cccc;
#endif

#define SSE_WORK \
    for (i = n / 64; i--;) { \
        _mm_stream_ps((float *)(p+0), c128); \
        _mm_stream_ps((float *)(p+16), c128); \
        _mm_stream_ps((float *)(p+32), c128); \
        _mm_stream_ps((float *)(p+48), c128); \
        p += 64; \
    }

#define SSE_END

#define DEFINE_SSE_FILLRECT(bpp, type) \
static void SDL_TARGETING("sse") SDL_FillSurfaceRect##bpp##SSE(uint8_t *pixels, int pitch, uint32_t color, int w, int h) \
{ \
    int i, n; \
    uint8_t *p = NULL; \
 \
    SSE_BEGIN; \
 \
    while (h--) { \
        n = (w) * (bpp); \
        p = pixels; \
 \
        if (n > 63) { \
            int adjust = 16 - ((uintptr_t)p & 15); \
            if (adjust < 16) { \
                n -= adjust; \
                adjust /= (bpp); \
                while (adjust--) { \
                    *((type *)p) = (type)color; \
                    p += (bpp); \
                } \
            } \
            SSE_WORK; \
        } \
        if (n & 63) { \
            int remainder = (n & 63); \
            remainder /= (bpp); \
            while (remainder--) { \
                *((type *)p) = (type)color; \
                p += (bpp); \
            } \
        } \
        pixels += pitch; \
    } \
 \
    SSE_END; \
}

static void SDL_TARGETING("sse") SDL_FillSurfaceRect1SSE(uint8_t *pixels, int pitch, uint32_t color, int w, int h)
{
    int i, n;

    SSE_BEGIN;
    while (h--) {
        uint8_t *p = pixels;
        n = w;

        if (n > 63) {
            int adjust = 16 - ((uintptr_t)p & 15);
            if (adjust) {
                n -= adjust;
                SDL_memset(p, color, adjust);
                p += adjust;
            }
            SSE_WORK;
        }
        if (n & 63) {
            int remainder = (n & 63);
            SDL_memset(p, color, remainder);
        }
        pixels += pitch;
    }

    SSE_END;
}
// DEFINE_SSE_FILLRECT(1, uint8_t)
DEFINE_SSE_FILLRECT(2, uint16_t)
DEFINE_SSE_FILLRECT(4, uint32_t)

/* *INDENT-ON* */ // clang-format on
#endif            // __SSE__

static void SDL_FillSurfaceRect1(uint8_t *pixels, int pitch, uint32_t color, int w, int h)
{
    int n;
    uint8_t *p = NULL;

    while (h--) {
        n = w;
        p = pixels;

        if (n > 3) {
            switch ((uintptr_t)p & 3) {
            case 1:
                *p++ = (uint8_t)color;
                --n;
                SDL_FALLTHROUGH;
            case 2:
                *p++ = (uint8_t)color;
                --n;
                SDL_FALLTHROUGH;
            case 3:
                *p++ = (uint8_t)color;
                --n;
            }
            SDL_memset4(p, color, (n >> 2));
        }
        if (n & 3) {
            p += (n & ~3);
            switch (n & 3) {
            case 3:
                *p++ = (uint8_t)color;
                SDL_FALLTHROUGH;
            case 2:
                *p++ = (uint8_t)color;
                SDL_FALLTHROUGH;
            case 1:
                *p++ = (uint8_t)color;
            }
        }
        pixels += pitch;
    }
}

static void SDL_FillSurfaceRect2(uint8_t *pixels, int pitch, uint32_t color, int w, int h)
{
    int n;
    uint16_t *p = NULL;

    while (h--) {
        n = w;
        p = (uint16_t *)pixels;

        if (n > 1) {
            if ((uintptr_t)p & 2) {
                *p++ = (uint16_t)color;
                --n;
            }
            SDL_memset4(p, color, (n >> 1));
        }
        if (n & 1) {
            p[n - 1] = (uint16_t)color;
        }
        pixels += pitch;
    }
}

static void SDL_FillSurfaceRect3(uint8_t *pixels, int pitch, uint32_t color, int w, int h)
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    uint8_t b1 = (uint8_t)(color & 0xFF);
    uint8_t b2 = (uint8_t)((color >> 8) & 0xFF);
    uint8_t b3 = (uint8_t)((color >> 16) & 0xFF);
#elif SDL_BYTEORDER == SDL_BIG_ENDIAN
    uint8_t b1 = (uint8_t)((color >> 16) & 0xFF);
    uint8_t b2 = (uint8_t)((color >> 8) & 0xFF);
    uint8_t b3 = (uint8_t)(color & 0xFF);
#endif
    int n;
    uint8_t *p = NULL;

    while (h--) {
        n = w;
        p = pixels;

        while (n--) {
            *p++ = b1;
            *p++ = b2;
            *p++ = b3;
        }
        pixels += pitch;
    }
}

static void SDL_FillSurfaceRect4(uint8_t *pixels, int pitch, uint32_t color, int w, int h)
{
    while (h--) {
        SDL_memset4(pixels, color, w);
        pixels += pitch;
    }
}

/*
 * This function performs a fast fill of the given rectangle with 'color'
 */
bool SDL_FillSurfaceRect(SDL_Surface *dst, const SDL_Rect *rect, uint32_t color)
{
    if (!SDL_SurfaceValid(dst)) {
        return SDL_InvalidParamError("SDL_FillSurfaceRect(): dst");
    }

    // If 'rect' == NULL, then fill the whole surface
    if (!rect) {
        rect = &dst->internal->clip_rect;
        // Don't attempt to fill if the surface's clip_rect is empty
        if (SDL_RectEmpty(rect)) {
            return true;
        }
    }

    return SDL_FillSurfaceRects(dst, rect, 1, color);
}

bool SDL_FillSurfaceRects(SDL_Surface *dst, const SDL_Rect *rects, int count, uint32_t color)
{
    SDL_Rect clipped;
    uint8_t *pixels;
    const SDL_Rect *rect;
    void (*fill_function)(uint8_t * pixels, int pitch, uint32_t color, int w, int h) = NULL;
    int i;

    if (!SDL_SurfaceValid(dst)) {
        return SDL_InvalidParamError("SDL_FillSurfaceRects(): dst");
    }

    // Nothing to do
    if (dst->w == 0 || dst->h == 0) {
        return true;
    }

    // Perform software fill
    if (!dst->pixels) {
        return SDL_SetError("SDL_FillSurfaceRects(): You must lock the surface");
    }

    if (!rects) {
        return SDL_InvalidParamError("SDL_FillSurfaceRects(): rects");
    }

    /* This function doesn't usually work on surfaces < 8 bpp
     * Except: support for 4bits, when filling full size.
     */
    if (SDL_BITSPERPIXEL(dst->format) < 8) {
        if (count == 1) {
            const SDL_Rect *r = &rects[0];
            if (r->x == 0 && r->y == 0 && r->w == dst->w && r->h == dst->h) {
                if (SDL_BITSPERPIXEL(dst->format) == 4) {
                    uint8_t b = (((uint8_t)color << 4) | (uint8_t)color);
                    SDL_memset(dst->pixels, b, (size_t)dst->h * dst->pitch);
                    return true;
                }
            }
        }
        return SDL_SetError("SDL_FillSurfaceRects(): Unsupported surface format");
    }

    if (fill_function == NULL) {
        switch (SDL_BYTESPERPIXEL(dst->format)) {
        case 1:
        {
            color |= (color << 8);
            color |= (color << 16);
#ifdef SDL_SSE_INTRINSICS
            if (SDL_HasSSE()) {
                fill_function = SDL_FillSurfaceRect1SSE;
                break;
            }
#endif
            fill_function = SDL_FillSurfaceRect1;
            break;
        }

        case 2:
        {
            color |= (color << 16);
#ifdef SDL_SSE_INTRINSICS
            if (SDL_HasSSE()) {
                fill_function = SDL_FillSurfaceRect2SSE;
                break;
            }
#endif
            fill_function = SDL_FillSurfaceRect2;
            break;
        }

        case 3:
            // 24-bit RGB is a slow path, at least for now.
            {
                fill_function = SDL_FillSurfaceRect3;
                break;
            }

        case 4:
        {
#ifdef SDL_SSE_INTRINSICS
            if (SDL_HasSSE()) {
                fill_function = SDL_FillSurfaceRect4SSE;
                break;
            }
#endif
            fill_function = SDL_FillSurfaceRect4;
            break;
        }

        default:
            return SDL_SetError("Unsupported pixel format");
        }
    }

    for (i = 0; i < count; ++i) {
        rect = &rects[i];
        // Perform clipping
        if (!SDL_GetRectIntersection(rect, &dst->internal->clip_rect, &clipped)) {
            continue;
        }
        rect = &clipped;

        pixels = (uint8_t *)dst->pixels + rect->y * dst->pitch +
                 rect->x * SDL_BYTESPERPIXEL(dst->format);

        fill_function(pixels, dst->pitch, color, rect->w, rect->h);
    }

    // We're done!
    return true;
}
