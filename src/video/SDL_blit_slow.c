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
#include "SDL_blit_slow.h"
#include "SDL_pixels_c.h"

#define FORMAT_ALPHA                0
#define FORMAT_NO_ALPHA             (-1)
#define FORMAT_2101010              1
#define FORMAT_HAS_ALPHA(format)    format == 0
#define FORMAT_HAS_NO_ALPHA(format) format < 0
static int detect_format(SDL_PixelFormat *pf)
{
    if (SDL_ISPIXELFORMAT_10BIT(pf->format)) {
        return FORMAT_2101010;
    } else if (pf->Amask) {
        return FORMAT_ALPHA;
    } else {
        return FORMAT_NO_ALPHA;
    }
}

/* The ONE TRUE BLITTER
 * This puppy has to handle all the unoptimized cases - yes, it's slow.
 */
void SDL_Blit_Slow(SDL_BlitInfo *info)
{
    const int flags = info->flags;
    const Uint32 modulateR = info->r;
    const Uint32 modulateG = info->g;
    const Uint32 modulateB = info->b;
    const Uint32 modulateA = info->a;
    Uint32 srcpixel;
    Uint32 srcR, srcG, srcB, srcA;
    Uint32 dstpixel;
    Uint32 dstR, dstG, dstB, dstA;
    Uint64 srcy, srcx;
    Uint64 posy, posx;
    Uint64 incy, incx;
    SDL_PixelFormat *src_fmt = info->src_fmt;
    SDL_PixelFormat *dst_fmt = info->dst_fmt;
    int srcbpp = src_fmt->BytesPerPixel;
    int dstbpp = dst_fmt->BytesPerPixel;
    int srcfmt_val;
    int dstfmt_val;
    Uint32 rgbmask = ~src_fmt->Amask;
    Uint32 ckey = info->colorkey & rgbmask;

    srcfmt_val = detect_format(src_fmt);
    dstfmt_val = detect_format(dst_fmt);

    incy = ((Uint64)info->src_h << 16) / info->dst_h;
    incx = ((Uint64)info->src_w << 16) / info->dst_w;
    posy = incy / 2; /* start at the middle of pixel */

    while (info->dst_h--) {
        Uint8 *src = 0;
        Uint8 *dst = info->dst;
        int n = info->dst_w;
        posx = incx / 2; /* start at the middle of pixel */
        srcy = posy >> 16;
        while (n--) {
            srcx = posx >> 16;
            src = (info->src + (srcy * info->src_pitch) + (srcx * srcbpp));

            if (FORMAT_HAS_ALPHA(srcfmt_val)) {
                DISEMBLE_RGBA(src, srcbpp, src_fmt, srcpixel, srcR, srcG, srcB, srcA);
            } else if (FORMAT_HAS_NO_ALPHA(srcfmt_val)) {
                DISEMBLE_RGB(src, srcbpp, src_fmt, srcpixel, srcR, srcG, srcB);
                srcA = 0xFF;
            } else {
                /* 10-bit pixel format */
                srcpixel = *((Uint32 *)(src));
                switch (src_fmt->format) {
                case SDL_PIXELFORMAT_XRGB2101010:
                    RGBA_FROM_ARGB2101010(srcpixel, srcR, srcG, srcB, srcA);
                    srcA = 0xFF;
                    break;
                case SDL_PIXELFORMAT_XBGR2101010:
                    RGBA_FROM_ABGR2101010(srcpixel, srcR, srcG, srcB, srcA);
                    srcA = 0xFF;
                    break;
                case SDL_PIXELFORMAT_ARGB2101010:
                    RGBA_FROM_ARGB2101010(srcpixel, srcR, srcG, srcB, srcA);
                    break;
                case SDL_PIXELFORMAT_ABGR2101010:
                    RGBA_FROM_ABGR2101010(srcpixel, srcR, srcG, srcB, srcA);
                    break;
                default:
                    srcR = srcG = srcB = srcA = 0;
                    break;
                }
            }

            if (flags & SDL_COPY_COLORKEY) {
                /* srcpixel isn't set for 24 bpp */
                if (srcbpp == 3) {
                    srcpixel = (srcR << src_fmt->Rshift) |
                               (srcG << src_fmt->Gshift) | (srcB << src_fmt->Bshift);
                }
                if ((srcpixel & rgbmask) == ckey) {
                    posx += incx;
                    dst += dstbpp;
                    continue;
                }
            }
            if ((flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL))) {
                if (FORMAT_HAS_ALPHA(dstfmt_val)) {
                    DISEMBLE_RGBA(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB, dstA);
                } else if (FORMAT_HAS_NO_ALPHA(dstfmt_val)) {
                    DISEMBLE_RGB(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB);
                    dstA = 0xFF;
                } else {
                    /* SDL_PIXELFORMAT_ARGB2101010 */
                    dstpixel = *((Uint32 *) (dst));
                    RGBA_FROM_ARGB2101010(dstpixel, dstR, dstG, dstB, dstA);
                }
            } else {
                /* don't care */
                dstR = dstG = dstB = dstA = 0;
            }

            if (flags & SDL_COPY_MODULATE_COLOR) {
                srcR = (srcR * modulateR) / 255;
                srcG = (srcG * modulateG) / 255;
                srcB = (srcB * modulateB) / 255;
            }
            if (flags & SDL_COPY_MODULATE_ALPHA) {
                srcA = (srcA * modulateA) / 255;
            }
            if (flags & (SDL_COPY_BLEND | SDL_COPY_ADD)) {
                /* This goes away if we ever use premultiplied alpha */
                if (srcA < 255) {
                    srcR = (srcR * srcA) / 255;
                    srcG = (srcG * srcA) / 255;
                    srcB = (srcB * srcA) / 255;
                }
            }
            switch (flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL)) {
            case 0:
                dstR = srcR;
                dstG = srcG;
                dstB = srcB;
                dstA = srcA;
                break;
            case SDL_COPY_BLEND:
                dstR = srcR + ((255 - srcA) * dstR) / 255;
                dstG = srcG + ((255 - srcA) * dstG) / 255;
                dstB = srcB + ((255 - srcA) * dstB) / 255;
                dstA = srcA + ((255 - srcA) * dstA) / 255;
                break;
            case SDL_COPY_ADD:
                dstR = srcR + dstR;
                if (dstR > 255) {
                    dstR = 255;
                }
                dstG = srcG + dstG;
                if (dstG > 255) {
                    dstG = 255;
                }
                dstB = srcB + dstB;
                if (dstB > 255) {
                    dstB = 255;
                }
                break;
            case SDL_COPY_MOD:
                dstR = (srcR * dstR) / 255;
                dstG = (srcG * dstG) / 255;
                dstB = (srcB * dstB) / 255;
                break;
            case SDL_COPY_MUL:
                dstR = ((srcR * dstR) + (dstR * (255 - srcA))) / 255;
                if (dstR > 255) {
                    dstR = 255;
                }
                dstG = ((srcG * dstG) + (dstG * (255 - srcA))) / 255;
                if (dstG > 255) {
                    dstG = 255;
                }
                dstB = ((srcB * dstB) + (dstB * (255 - srcA))) / 255;
                if (dstB > 255) {
                    dstB = 255;
                }
                break;
            }
            if (FORMAT_HAS_ALPHA(dstfmt_val)) {
                ASSEMBLE_RGBA(dst, dstbpp, dst_fmt, dstR, dstG, dstB, dstA);
            } else if (FORMAT_HAS_NO_ALPHA(dstfmt_val)) {
                ASSEMBLE_RGB(dst, dstbpp, dst_fmt, dstR, dstG, dstB);
            } else {
                /* 10-bit pixel format */
                Uint32 pixel;
                switch (dst_fmt->format) {
                case SDL_PIXELFORMAT_XRGB2101010:
                    dstA = 0xFF;
                    SDL_FALLTHROUGH;
                case SDL_PIXELFORMAT_ARGB2101010:
                    ARGB2101010_FROM_RGBA(pixel, dstR, dstG, dstB, dstA);
                    break;
                case SDL_PIXELFORMAT_XBGR2101010:
                    dstA = 0xFF;
                    SDL_FALLTHROUGH;
                case SDL_PIXELFORMAT_ABGR2101010:
                    ABGR2101010_FROM_RGBA(pixel, dstR, dstG, dstB, dstA);
                    break;
                default:
                    pixel = 0;
                    break;
                }
                *(Uint32 *)dst = pixel;
            }
            posx += incx;
            dst += dstbpp;
        }
        posy += incy;
        info->dst += info->dst_pitch;
    }
}

static void MatrixMultiply(float v[3], const float *matrix)
{
    float out[3];

    out[0] = matrix[0 * 3 + 0] * v[0] + matrix[0 * 3 + 1] * v[1] + matrix[0 * 3 + 2] * v[2];
    out[1] = matrix[1 * 3 + 0] * v[0] + matrix[1 * 3 + 1] * v[1] + matrix[1 * 3 + 2] * v[2];
    out[2] = matrix[2 * 3 + 0] * v[0] + matrix[2 * 3 + 1] * v[1] + matrix[2 * 3 + 2] * v[2];
    v[0] = out[0];
    v[1] = out[1];
    v[2] = out[2];
}

static float PQtoNits(float pq)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;

    const float oo_m1 = 1.0f / 0.1593017578125f;
    const float oo_m2 = 1.0f / 78.84375f;

    float num = SDL_max(SDL_powf(pq, oo_m2) - c1, 0.0f);
    float den = c2 - c3 * SDL_powf(pq, oo_m2);

    return 10000.0f * SDL_powf(num / den, oo_m1);
}


/* This isn't really a tone mapping algorithm but it generally works well for HDR -> SDR display */
static void PQtoSDR(const float *color_primaries_matrix, float floatR, float floatG, float floatB, Uint32 *outR, Uint32 *outG, Uint32 *outB)
{
    float v[3];
    int i;

    v[0] = PQtoNits(floatR);
    v[1] = PQtoNits(floatG);
    v[2] = PQtoNits(floatB);

    MatrixMultiply(v, color_primaries_matrix);

    for (i = 0; i < SDL_arraysize(v); ++i) {
        v[i] /= 400.0f;
        v[i] = SDL_clamp(v[i], 0.0f, 1.0f);
        v[i] = SDL_powf(v[i], 1.0f / 2.2f);
    }

    *outR = (Uint32)(v[0] * 255);
    *outG = (Uint32)(v[1] * 255);
    *outB = (Uint32)(v[2] * 255);
}

void SDL_Blit_Slow_PQtoSDR(SDL_BlitInfo *info)
{
    const int flags = info->flags;
    const Uint32 modulateR = info->r;
    const Uint32 modulateG = info->g;
    const Uint32 modulateB = info->b;
    const Uint32 modulateA = info->a;
    Uint32 srcpixel;
    float srcFloatR, srcFloatG, srcFloatB, srcFloatA;
    Uint32 srcR, srcG, srcB, srcA;
    Uint32 dstpixel;
    Uint32 dstR, dstG, dstB, dstA;
    Uint64 srcy, srcx;
    Uint64 posy, posx;
    Uint64 incy, incx;
    SDL_PixelFormat *src_fmt = info->src_fmt;
    SDL_PixelFormat *dst_fmt = info->dst_fmt;
    int srcbpp = src_fmt->BytesPerPixel;
    int dstbpp = dst_fmt->BytesPerPixel;
    int dstfmt_val;
    Uint32 rgbmask = ~src_fmt->Amask;
    Uint32 ckey = info->colorkey & rgbmask;
    SDL_PropertiesID props = SDL_GetSurfaceProperties(info->src_surface);
    SDL_ColorPrimaries src_primaries = (SDL_ColorPrimaries)SDL_GetNumberProperty(props, SDL_PROP_SURFACE_COLOR_PRIMARIES_NUMBER, SDL_COLOR_PRIMARIES_BT2020);
    const float *color_primaries_matrix = SDL_GetColorPrimariesConversionMatrix(src_primaries, SDL_COLOR_PRIMARIES_BT709);

    dstfmt_val = detect_format(dst_fmt);

    incy = ((Uint64)info->src_h << 16) / info->dst_h;
    incx = ((Uint64)info->src_w << 16) / info->dst_w;
    posy = incy / 2; /* start at the middle of pixel */

    while (info->dst_h--) {
        Uint8 *src = 0;
        Uint8 *dst = info->dst;
        int n = info->dst_w;
        posx = incx / 2; /* start at the middle of pixel */
        srcy = posy >> 16;
        while (n--) {
            srcx = posx >> 16;
            src = (info->src + (srcy * info->src_pitch) + (srcx * srcbpp));

            /* 10-bit pixel format */
            srcpixel = *((Uint32 *)(src));
            switch (src_fmt->format) {
            case SDL_PIXELFORMAT_XRGB2101010:
                RGBAFLOAT_FROM_ARGB2101010(srcpixel, srcFloatR, srcFloatG, srcFloatB, srcFloatA);
                srcFloatA = 1.0f;
                break;
            case SDL_PIXELFORMAT_XBGR2101010:
                RGBAFLOAT_FROM_ABGR2101010(srcpixel, srcFloatR, srcFloatG, srcFloatB, srcFloatA);
                srcFloatA = 1.0f;
                break;
            case SDL_PIXELFORMAT_ARGB2101010:
                RGBAFLOAT_FROM_ARGB2101010(srcpixel, srcFloatR, srcFloatG, srcFloatB, srcFloatA);
                break;
            case SDL_PIXELFORMAT_ABGR2101010:
                RGBAFLOAT_FROM_ABGR2101010(srcpixel, srcFloatR, srcFloatG, srcFloatB, srcFloatA);
                break;
            default:
                /* Unsupported pixel format */
                srcFloatR = srcFloatG = srcFloatB = srcFloatA = 0.0f;
                break;
            }

            PQtoSDR(color_primaries_matrix, srcFloatR, srcFloatG, srcFloatB, &srcR, &srcG, &srcB);
            srcA = (Uint32)(srcFloatA * 255);

            if (flags & SDL_COPY_COLORKEY) {
                /* srcpixel isn't set for 24 bpp */
                if (srcbpp == 3) {
                    srcpixel = (srcR << src_fmt->Rshift) |
                               (srcG << src_fmt->Gshift) | (srcB << src_fmt->Bshift);
                }
                if ((srcpixel & rgbmask) == ckey) {
                    posx += incx;
                    dst += dstbpp;
                    continue;
                }
            }
            if ((flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL))) {
                if (FORMAT_HAS_ALPHA(dstfmt_val)) {
                    DISEMBLE_RGBA(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB, dstA);
                } else if (FORMAT_HAS_NO_ALPHA(dstfmt_val)) {
                    DISEMBLE_RGB(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB);
                    dstA = 0xFF;
                } else {
                    /* SDL_PIXELFORMAT_ARGB2101010 */
                    dstpixel = *((Uint32 *)(dst));
                    RGBA_FROM_ARGB2101010(dstpixel, dstR, dstG, dstB, dstA);
                }
            } else {
                /* don't care */
                dstR = dstG = dstB = dstA = 0;
            }

            if (flags & SDL_COPY_MODULATE_COLOR) {
                srcR = (srcR * modulateR) / 255;
                srcG = (srcG * modulateG) / 255;
                srcB = (srcB * modulateB) / 255;
            }
            if (flags & SDL_COPY_MODULATE_ALPHA) {
                srcA = (srcA * modulateA) / 255;
            }
            if (flags & (SDL_COPY_BLEND | SDL_COPY_ADD)) {
                /* This goes away if we ever use premultiplied alpha */
                if (srcA < 255) {
                    srcR = (srcR * srcA) / 255;
                    srcG = (srcG * srcA) / 255;
                    srcB = (srcB * srcA) / 255;
                }
            }
            switch (flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL)) {
            case 0:
                dstR = srcR;
                dstG = srcG;
                dstB = srcB;
                dstA = srcA;
                break;
            case SDL_COPY_BLEND:
                dstR = srcR + ((255 - srcA) * dstR) / 255;
                dstG = srcG + ((255 - srcA) * dstG) / 255;
                dstB = srcB + ((255 - srcA) * dstB) / 255;
                dstA = srcA + ((255 - srcA) * dstA) / 255;
                break;
            case SDL_COPY_ADD:
                dstR = srcR + dstR;
                if (dstR > 255) {
                    dstR = 255;
                }
                dstG = srcG + dstG;
                if (dstG > 255) {
                    dstG = 255;
                }
                dstB = srcB + dstB;
                if (dstB > 255) {
                    dstB = 255;
                }
                break;
            case SDL_COPY_MOD:
                dstR = (srcR * dstR) / 255;
                dstG = (srcG * dstG) / 255;
                dstB = (srcB * dstB) / 255;
                break;
            case SDL_COPY_MUL:
                dstR = ((srcR * dstR) + (dstR * (255 - srcA))) / 255;
                if (dstR > 255) {
                    dstR = 255;
                }
                dstG = ((srcG * dstG) + (dstG * (255 - srcA))) / 255;
                if (dstG > 255) {
                    dstG = 255;
                }
                dstB = ((srcB * dstB) + (dstB * (255 - srcA))) / 255;
                if (dstB > 255) {
                    dstB = 255;
                }
                break;
            }
            if (FORMAT_HAS_ALPHA(dstfmt_val)) {
                ASSEMBLE_RGBA(dst, dstbpp, dst_fmt, dstR, dstG, dstB, dstA);
            } else if (FORMAT_HAS_NO_ALPHA(dstfmt_val)) {
                ASSEMBLE_RGB(dst, dstbpp, dst_fmt, dstR, dstG, dstB);
            } else {
                /* 10-bit pixel format */
                Uint32 pixel;
                switch (dst_fmt->format) {
                case SDL_PIXELFORMAT_XRGB2101010:
                    dstA = 0xFF;
                    SDL_FALLTHROUGH;
                case SDL_PIXELFORMAT_ARGB2101010:
                    ARGB2101010_FROM_RGBA(pixel, dstR, dstG, dstB, dstA);
                    break;
                case SDL_PIXELFORMAT_XBGR2101010:
                    dstA = 0xFF;
                    SDL_FALLTHROUGH;
                case SDL_PIXELFORMAT_ABGR2101010:
                    ABGR2101010_FROM_RGBA(pixel, dstR, dstG, dstB, dstA);
                    break;
                default:
                    pixel = 0;
                    break;
                }
                *(Uint32 *)dst = pixel;
            }
            posx += incx;
            dst += dstbpp;
        }
        posy += incy;
        info->dst += info->dst_pitch;
    }
}
