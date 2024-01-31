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

typedef enum
{
    SlowBlitPixelAccess_RGB,
    SlowBlitPixelAccess_RGBA,
    SlowBlitPixelAccess_10Bit,
    SlowBlitPixelAccess_Large,
} SlowBlitPixelAccess;

static SlowBlitPixelAccess GetPixelAccessMethod(SDL_PixelFormat *pf)
{
    if (pf->BytesPerPixel > 4) {
        return SlowBlitPixelAccess_Large;
    } else if (SDL_ISPIXELFORMAT_10BIT(pf->format)) {
        return SlowBlitPixelAccess_10Bit;
    } else if (pf->Amask) {
        return SlowBlitPixelAccess_RGBA;
    } else {
        return SlowBlitPixelAccess_RGB;
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
    SlowBlitPixelAccess src_access;
    SlowBlitPixelAccess dst_access;
    Uint32 rgbmask = ~src_fmt->Amask;
    Uint32 ckey = info->colorkey & rgbmask;

    src_access = GetPixelAccessMethod(src_fmt);
    dst_access = GetPixelAccessMethod(dst_fmt);

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

            switch (src_access) {
            case SlowBlitPixelAccess_RGB:
                DISEMBLE_RGB(src, srcbpp, src_fmt, srcpixel, srcR, srcG, srcB);
                srcA = 0xFF;
                break;
            case SlowBlitPixelAccess_RGBA:
                DISEMBLE_RGBA(src, srcbpp, src_fmt, srcpixel, srcR, srcG, srcB, srcA);
                break;
            case SlowBlitPixelAccess_10Bit:
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
                break;
            case SlowBlitPixelAccess_Large:
                /* Handled in SDL_Blit_Slow_Float() */
                srcpixel = srcR = srcG = srcB = srcA = 0;
                break;
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
                switch (dst_access) {
                case SlowBlitPixelAccess_RGB:
                    DISEMBLE_RGB(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB);
                    dstA = 0xFF;
                    break;
                case SlowBlitPixelAccess_RGBA:
                    DISEMBLE_RGBA(dst, dstbpp, dst_fmt, dstpixel, dstR, dstG, dstB, dstA);
                    break;
                case SlowBlitPixelAccess_10Bit:
                    dstpixel = *((Uint32 *)(dst));
                    switch (dst_fmt->format) {
                    case SDL_PIXELFORMAT_XRGB2101010:
                        RGBA_FROM_ARGB2101010(dstpixel, dstR, dstG, dstB, dstA);
                        dstA = 0xFF;
                        break;
                    case SDL_PIXELFORMAT_XBGR2101010:
                        RGBA_FROM_ABGR2101010(dstpixel, dstR, dstG, dstB, dstA);
                        dstA = 0xFF;
                        break;
                    case SDL_PIXELFORMAT_ARGB2101010:
                        RGBA_FROM_ARGB2101010(dstpixel, dstR, dstG, dstB, dstA);
                        break;
                    case SDL_PIXELFORMAT_ABGR2101010:
                        RGBA_FROM_ABGR2101010(dstpixel, dstR, dstG, dstB, dstA);
                        break;
                    default:
                        dstR = dstG = dstB = dstA = 0;
                        break;
                    }
                    break;
                case SlowBlitPixelAccess_Large:
                    /* Handled in SDL_Blit_Slow_Float() */
                    dstR = dstG = dstB = dstA = 0;
                    break;
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

            switch (dst_access) {
            case SlowBlitPixelAccess_RGB:
                ASSEMBLE_RGB(dst, dstbpp, dst_fmt, dstR, dstG, dstB);
                break;
            case SlowBlitPixelAccess_RGBA:
                ASSEMBLE_RGBA(dst, dstbpp, dst_fmt, dstR, dstG, dstB, dstA);
                break;
            case SlowBlitPixelAccess_10Bit:
            {
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
                break;
            }
            case SlowBlitPixelAccess_Large:
                /* Handled in SDL_Blit_Slow_Float() */
                break;
            }

            posx += incx;
            dst += dstbpp;
        }
        posy += incy;
        info->dst += info->dst_pitch;
    }
}

/* Convert from F16 to float
 * Public domain implementation from https://gist.github.com/rygorous/2144712
 */
typedef union
{
    Uint32 u;
    float f;
    struct
    {
        Uint32 Mantissa : 23;
        Uint32 Exponent : 8;
        Uint32 Sign : 1;
    } x;
} FP32;

typedef union
{
    Uint16 u;
    struct
    {
        Uint16 Mantissa : 10;
        Uint16 Exponent : 5;
        Uint16 Sign : 1;
    } x;
} FP16;

static float half_to_float(Uint16 unValue)
{
    static const FP32 magic = { (254 - 15) << 23 };
    static const FP32 was_infnan = { (127 + 16) << 23 };
    FP16 h;
    FP32 o;

    h.u = unValue;
    o.u = (h.u & 0x7fff) << 13;     // exponent/mantissa bits
    o.f *= magic.f;                 // exponent adjust
    if (o.f >= was_infnan.f)        // make sure Inf/NaN survive
        o.u |= 255 << 23;
    o.u |= (h.u & 0x8000) << 16;    // sign bit
    return o.f;
}

/* Convert from float to F16
 * Public domain implementation from https://stackoverflow.com/questions/76799117/how-to-convert-a-float-to-a-half-type-and-the-other-way-around-in-c
 */
static Uint16 float_to_half(float a)
{
    Uint32 ia;
    Uint16 ir;

    SDL_memcpy(&ia, &a, sizeof(ia));

    ir = (ia >> 16) & 0x8000;
    if ((ia & 0x7f800000) == 0x7f800000) {
        if ((ia & 0x7fffffff) == 0x7f800000) {
            ir |= 0x7c00; /* infinity */
        } else {
            ir |= 0x7e00 | ((ia >> (24 - 11)) & 0x1ff); /* NaN, quietened */
        }
    } else if ((ia & 0x7f800000) >= 0x33000000) {
        int shift = (int)((ia >> 23) & 0xff) - 127;
        if (shift > 15) {
            ir |= 0x7c00; /* infinity */
        } else {
            ia = (ia & 0x007fffff) | 0x00800000; /* extract mantissa */
            if (shift < -14) { /* denormal */
                ir |= ia >> (-1 - shift);
                ia = ia << (32 - (-1 - shift));
            } else { /* normal */
                ir |= ia >> (24 - 11);
                ia = ia << (32 - (24 - 11));
                ir = ir + ((14 + shift) << 10);
            }
            /* IEEE-754 round to nearest of even */
            if ((ia > 0x80000000) || ((ia == 0x80000000) && (ir & 1))) {
                ir++;
            }
        }
    }
    return ir;
}

static float scRGBtoNits(float v)
{
    return v * 80.0f;
}

static float scRGBfromNits(float v)
{
    return v / 80.0f;
}

static float sRGBtoNits(float v)
{
    if (v <= 0.04045f) {
        v = (v / 12.92f);
    } else {
        v = SDL_powf((v + 0.055f) / 1.055f, 2.4f);
    }
    return scRGBtoNits(v);
}

static float sRGBfromNits(float v)
{
    v = scRGBfromNits(v);

    if (v <= 0.0031308f) {
        v = (v * 12.92f);
    } else {
        v = (SDL_powf(v, 1.0f / 2.4f) * 1.055f - 0.055f);
    }
    return v;
}

static float PQtoNits(float v)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float oo_m1 = 1.0f / 0.1593017578125f;
    const float oo_m2 = 1.0f / 78.84375f;

    float num = SDL_max(SDL_powf(v, oo_m2) - c1, 0.0f);
    float den = c2 - c3 * SDL_powf(v, oo_m2);
    return 10000.0f * SDL_powf(num / den, oo_m1);
}

static float PQfromNits(float v)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;

    float y = SDL_clamp(v / 10000.0f, 0.0f, 1.0f);
    float num = c1 + c2 * pow(y, m1);
    float den = 1.0f + c3 * pow(y, m1);
    return pow(num / den, m2);
}

static void ReadFloatPixel(Uint8 *pixels, SlowBlitPixelAccess access, SDL_PixelFormat *fmt, SDL_Colorspace colorspace,
                           float *outR, float *outG, float *outB, float *outA)
{
    Uint32 pixel;
    Uint32 R, G, B, A;
    float fR = 0.0f, fG = 0.0f, fB = 0.0f, fA = 0.0f;
    float v[4];

    switch (access) {
    case SlowBlitPixelAccess_RGB:
        DISEMBLE_RGB(pixels, fmt->BytesPerPixel, fmt, pixel, R, G, B);
        fR = (float)R / 255.0f;
        fG = (float)G / 255.0f;
        fB = (float)B / 255.0f;
        fA = 1.0f;
        break;
    case SlowBlitPixelAccess_RGBA:
        DISEMBLE_RGBA(pixels, fmt->BytesPerPixel, fmt, pixel, R, G, B, A);
        fR = (float)R / 255.0f;
        fG = (float)G / 255.0f;
        fB = (float)B / 255.0f;
        fA = (float)A / 255.0f;
        break;
    case SlowBlitPixelAccess_10Bit:
        pixel = *((Uint32 *)pixels);
        switch (fmt->format) {
        case SDL_PIXELFORMAT_XRGB2101010:
            RGBAFLOAT_FROM_ARGB2101010(pixel, fR, fG, fB, fA);
            fA = 1.0f;
            break;
        case SDL_PIXELFORMAT_XBGR2101010:
            RGBAFLOAT_FROM_ABGR2101010(pixel, fR, fG, fB, fA);
            fA = 1.0f;
            break;
        case SDL_PIXELFORMAT_ARGB2101010:
            RGBAFLOAT_FROM_ARGB2101010(pixel, fR, fG, fB, fA);
            break;
        case SDL_PIXELFORMAT_ABGR2101010:
            RGBAFLOAT_FROM_ABGR2101010(pixel, fR, fG, fB, fA);
            break;
        default:
            fR = fG = fB = fA = 0.0f;
            break;
        }
        break;
    case SlowBlitPixelAccess_Large:
        switch (SDL_PIXELTYPE(fmt->format)) {
        case SDL_PIXELTYPE_ARRAYU16:
            v[0] = (float)(((Uint16 *)pixels)[0]) / SDL_MAX_UINT16;
            v[1] = (float)(((Uint16 *)pixels)[1]) / SDL_MAX_UINT16;
            v[2] = (float)(((Uint16 *)pixels)[2]) / SDL_MAX_UINT16;
            if (fmt->BytesPerPixel == 8) {
                v[3] = (float)(((Uint16 *)pixels)[3]) / SDL_MAX_UINT16;
            } else {
                v[3] = 1.0f;
            }
            break;
        case SDL_PIXELTYPE_ARRAYF16:
            v[0] = half_to_float(((Uint16 *)pixels)[0]);
            v[1] = half_to_float(((Uint16 *)pixels)[1]);
            v[2] = half_to_float(((Uint16 *)pixels)[2]);
            if (fmt->BytesPerPixel == 8) {
                v[3] = half_to_float(((Uint16 *)pixels)[3]);
            } else {
                v[3] = 1.0f;
            }
            break;
        case SDL_PIXELTYPE_ARRAYF32:
            v[0] = ((float *)pixels)[0];
            v[1] = ((float *)pixels)[1];
            v[2] = ((float *)pixels)[2];
            if (fmt->BytesPerPixel == 16) {
                v[3] = ((float *)pixels)[3];
            } else {
                v[3] = 1.0f;
            }
            break;
        default:
            /* Unknown array type */
            v[0] = v[1] = v[2] = v[3] = 0.0f;
            break;
        }
        switch (SDL_PIXELORDER(fmt->format)) {
        case SDL_ARRAYORDER_RGB:
            fR = v[0];
            fG = v[1];
            fB = v[2];
            fA = 1.0f;
            break;
        case SDL_ARRAYORDER_RGBA:
            fR = v[0];
            fG = v[1];
            fB = v[2];
            fA = v[3];
            break;
        case SDL_ARRAYORDER_ARGB:
            fA = v[0];
            fR = v[1];
            fG = v[2];
            fB = v[3];
            break;
        case SDL_ARRAYORDER_BGR:
            fB = v[0];
            fG = v[1];
            fR = v[2];
            fA = 1.0f;
            break;
        case SDL_ARRAYORDER_BGRA:
            fB = v[0];
            fG = v[1];
            fR = v[2];
            fA = v[3];
            break;
        case SDL_ARRAYORDER_ABGR:
            fA = v[0];
            fB = v[1];
            fG = v[2];
            fR = v[3];
            break;
        default:
            /* Unknown array order */
            fA = fR = fG = fB = 0.0f;
            break;
        }
        break;
    }

    /* Convert to nits so src and dst are guaranteed to be linear and in the same units */
    switch (SDL_COLORSPACETRANSFER(colorspace)) {
    case SDL_TRANSFER_CHARACTERISTICS_SRGB:
        fR = sRGBtoNits(fR);
        fG = sRGBtoNits(fG);
        fB = sRGBtoNits(fB);
        break;
    case SDL_TRANSFER_CHARACTERISTICS_PQ:
        fR = PQtoNits(fR);
        fG = PQtoNits(fG);
        fB = PQtoNits(fB);
        break;
    case SDL_TRANSFER_CHARACTERISTICS_LINEAR:
        /* Assuming scRGB for now */
        fR = scRGBtoNits(fR);
        fG = scRGBtoNits(fG);
        fB = scRGBtoNits(fB);
        break;
    default:
        /* Unknown, leave it alone */
        break;
    }

    *outR = fR;
    *outG = fG;
    *outB = fB;
    *outA = fA;
}

static void WriteFloatPixel(Uint8 *pixels, SlowBlitPixelAccess access, SDL_PixelFormat *fmt, SDL_Colorspace colorspace,
                            float fR, float fG, float fB, float fA)
{
    Uint32 R, G, B, A;
    float v[4];

    /* We converted to nits so src and dst are guaranteed to be linear and in the same units */
    switch (SDL_COLORSPACETRANSFER(colorspace)) {
    case SDL_TRANSFER_CHARACTERISTICS_SRGB:
        fR = sRGBfromNits(fR);
        fG = sRGBfromNits(fG);
        fB = sRGBfromNits(fB);
        break;
    case SDL_TRANSFER_CHARACTERISTICS_PQ:
        fR = PQfromNits(fR);
        fG = PQfromNits(fG);
        fB = PQfromNits(fB);
        break;
    case SDL_TRANSFER_CHARACTERISTICS_LINEAR:
        /* Assuming scRGB for now */
        fR = scRGBfromNits(fR);
        fG = scRGBfromNits(fG);
        fB = scRGBfromNits(fB);
        break;
    default:
        /* Unknown, leave it alone */
        break;
    }

    switch (access) {
    case SlowBlitPixelAccess_RGB:
        R = (Uint8)(SDL_clamp(fR, 0.0f, 1.0f) * 255.0f);
        G = (Uint8)(SDL_clamp(fG, 0.0f, 1.0f) * 255.0f);
        B = (Uint8)(SDL_clamp(fB, 0.0f, 1.0f) * 255.0f);
        ASSEMBLE_RGB(pixels, fmt->BytesPerPixel, fmt, R, G, B);
        break;
    case SlowBlitPixelAccess_RGBA:
        R = (Uint8)(SDL_clamp(fR, 0.0f, 1.0f) * 255.0f);
        G = (Uint8)(SDL_clamp(fG, 0.0f, 1.0f) * 255.0f);
        B = (Uint8)(SDL_clamp(fB, 0.0f, 1.0f) * 255.0f);
        A = (Uint8)(SDL_clamp(fA, 0.0f, 1.0f) * 255.0f);
        ASSEMBLE_RGBA(pixels, fmt->BytesPerPixel, fmt, R, G, B, A);
        break;
    case SlowBlitPixelAccess_10Bit:
    {
        Uint32 pixel;
        switch (fmt->format) {
        case SDL_PIXELFORMAT_XRGB2101010:
            fA = 1.0f;
            SDL_FALLTHROUGH;
        case SDL_PIXELFORMAT_ARGB2101010:
            ARGB2101010_FROM_RGBAFLOAT(pixel, fR, fG, fB, fA);
            break;
        case SDL_PIXELFORMAT_XBGR2101010:
            fA = 1.0f;
            SDL_FALLTHROUGH;
        case SDL_PIXELFORMAT_ABGR2101010:
            ABGR2101010_FROM_RGBAFLOAT(pixel, fR, fG, fB, fA);
            break;
        default:
            pixel = 0;
            break;
        }
        *(Uint32 *)pixels = pixel;
        break;
    }
    case SlowBlitPixelAccess_Large:
        switch (SDL_PIXELORDER(fmt->format)) {
        case SDL_ARRAYORDER_RGB:
            v[0] = fR;
            v[1] = fG;
            v[2] = fB;
            v[3] = 1.0f;
            break;
        case SDL_ARRAYORDER_RGBA:
            v[0] = fR;
            v[1] = fG;
            v[2] = fB;
            v[3] = fA;
            break;
        case SDL_ARRAYORDER_ARGB:
            v[0] = fA;
            v[1] = fR;
            v[2] = fG;
            v[3] = fB;
            break;
        case SDL_ARRAYORDER_BGR:
            v[0] = fB;
            v[1] = fG;
            v[2] = fR;
            v[3] = 1.0f;
            break;
        case SDL_ARRAYORDER_BGRA:
            v[0] = fB;
            v[1] = fG;
            v[2] = fR;
            v[3] = fA;
            break;
        case SDL_ARRAYORDER_ABGR:
            v[0] = fA;
            v[1] = fB;
            v[2] = fG;
            v[3] = fR;
            break;
        default:
            /* Unknown array order */
            v[0] = v[1] = v[2] = v[3] = 0.0f;
            break;
        }
        switch (SDL_PIXELTYPE(fmt->format)) {
        case SDL_PIXELTYPE_ARRAYU16:
            ((Uint16 *)pixels)[0] = (Uint16)(SDL_clamp(v[0], 0.0f, 1.0f) * SDL_MAX_UINT16);
            ((Uint16 *)pixels)[1] = (Uint16)(SDL_clamp(v[1], 0.0f, 1.0f) * SDL_MAX_UINT16);
            ((Uint16 *)pixels)[2] = (Uint16)(SDL_clamp(v[2], 0.0f, 1.0f) * SDL_MAX_UINT16);
            if (fmt->BytesPerPixel == 8) {
                ((Uint16 *)pixels)[3] = (Uint16)(SDL_clamp(v[3], 0.0f, 1.0f) * SDL_MAX_UINT16);
            }
            break;
        case SDL_PIXELTYPE_ARRAYF16:
            ((Uint16 *)pixels)[0] = float_to_half(v[0]);
            ((Uint16 *)pixels)[1] = float_to_half(v[1]);
            ((Uint16 *)pixels)[2] = float_to_half(v[2]);
            if (fmt->BytesPerPixel == 8) {
                ((Uint16 *)pixels)[3] = float_to_half(v[3]);
            }
            break;
        case SDL_PIXELTYPE_ARRAYF32:
            ((float *)pixels)[0] = v[0];
            ((float *)pixels)[1] = v[1];
            ((float *)pixels)[2] = v[2];
            if (fmt->BytesPerPixel == 16) {
                ((float *)pixels)[3] = v[3];
            }
            break;
        default:
            /* Unknown array type */
            break;
        }
        break;
    }
}

static void ConvertColorPrimaries(float *fR, float *fG, float *fB, const float *matrix)
{
    float v[3];

    v[0] = *fR;
    v[1] = *fG;
    v[2] = *fB;

    *fR = matrix[0 * 3 + 0] * v[0] + matrix[0 * 3 + 1] * v[1] + matrix[0 * 3 + 2] * v[2];
    *fG = matrix[1 * 3 + 0] * v[0] + matrix[1 * 3 + 1] * v[1] + matrix[1 * 3 + 2] * v[2];
    *fB = matrix[2 * 3 + 0] * v[0] + matrix[2 * 3 + 1] * v[1] + matrix[2 * 3 + 2] * v[2];
}

static float CompressPQtoSDR(float v)
{
    /* This gives generally good results for PQ HDR -> SDR conversion, scaling from 400 nits to 80 nits,
     * which is scRGB 1.0
     */
    return (v / 5.0f);
}

/* The SECOND TRUE BLITTER
 * This one is even slower than the first, but also handles large pixel formats and colorspace conversion
 */
void SDL_Blit_Slow_Float(SDL_BlitInfo *info)
{
    const int flags = info->flags;
    const Uint32 modulateR = info->r;
    const Uint32 modulateG = info->g;
    const Uint32 modulateB = info->b;
    const Uint32 modulateA = info->a;
    float srcR, srcG, srcB, srcA;
    float dstR, dstG, dstB, dstA;
    Uint64 srcy, srcx;
    Uint64 posy, posx;
    Uint64 incy, incx;
    SDL_PixelFormat *src_fmt = info->src_fmt;
    SDL_PixelFormat *dst_fmt = info->dst_fmt;
    int srcbpp = src_fmt->BytesPerPixel;
    int dstbpp = dst_fmt->BytesPerPixel;
    SlowBlitPixelAccess src_access;
    SlowBlitPixelAccess dst_access;
    SDL_PropertiesID src_props = SDL_GetSurfaceProperties(info->src_surface);
    SDL_Colorspace src_colorspace = (SDL_Colorspace)SDL_GetNumberProperty(src_props, SDL_PROP_SURFACE_COLORSPACE_NUMBER, SDL_COLORSPACE_RGB_DEFAULT);
    SDL_ColorPrimaries src_primaries = SDL_COLORSPACEPRIMARIES(src_colorspace);
    SDL_TransferCharacteristics src_transfer = SDL_COLORSPACETRANSFER(src_colorspace);
    SDL_PropertiesID dst_props = SDL_GetSurfaceProperties(info->dst_surface);
    SDL_Colorspace dst_colorspace = (SDL_Colorspace)SDL_GetNumberProperty(dst_props, SDL_PROP_SURFACE_COLORSPACE_NUMBER, SDL_COLORSPACE_RGB_DEFAULT);
    SDL_ColorPrimaries dst_primaries = SDL_COLORSPACEPRIMARIES(dst_colorspace);
    SDL_TransferCharacteristics dst_transfer = SDL_COLORSPACETRANSFER(dst_colorspace);
    const float *color_primaries_matrix = SDL_GetColorPrimariesConversionMatrix(src_primaries, dst_primaries);
    SDL_bool compress_PQ = SDL_FALSE;

    if (src_transfer == SDL_TRANSFER_CHARACTERISTICS_PQ &&
        dst_transfer != SDL_TRANSFER_CHARACTERISTICS_PQ &&
        dst_colorspace != SDL_COLORSPACE_SCRGB) {
        compress_PQ = SDL_TRUE;
    }

    src_access = GetPixelAccessMethod(src_fmt);
    dst_access = GetPixelAccessMethod(dst_fmt);

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

            ReadFloatPixel(src, src_access, src_fmt, src_colorspace, &srcR, &srcG, &srcB, &srcA);

            if (color_primaries_matrix) {
                ConvertColorPrimaries(&srcR, &srcG, &srcB, color_primaries_matrix);
            }

            if (compress_PQ) {
                srcR = CompressPQtoSDR(srcR);
                srcG = CompressPQtoSDR(srcG);
                srcB = CompressPQtoSDR(srcB);
            }

            if (flags & SDL_COPY_COLORKEY) {
                /* colorkey isn't supported */
            }
            if ((flags & (SDL_COPY_BLEND | SDL_COPY_ADD | SDL_COPY_MOD | SDL_COPY_MUL))) {
                ReadFloatPixel(dst, dst_access, dst_fmt, dst_colorspace, &dstR, &dstG, &dstB, &dstA);
            } else {
                /* don't care */
                dstR = dstG = dstB = dstA = 0.0f;
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
                if (srcA < 1.0f) {
                    srcR = (srcR * srcA);
                    srcG = (srcG * srcA);
                    srcB = (srcB * srcA);
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
                dstR = srcR + ((1.0f - srcA) * dstR);
                dstG = srcG + ((1.0f - srcA) * dstG);
                dstB = srcB + ((1.0f - srcA) * dstB);
                dstA = srcA + ((1.0f - srcA) * dstA);
                break;
            case SDL_COPY_ADD:
                dstR = srcR + dstR;
                dstG = srcG + dstG;
                dstB = srcB + dstB;
                break;
            case SDL_COPY_MOD:
                dstR = (srcR * dstR);
                dstG = (srcG * dstG);
                dstB = (srcB * dstB);
                break;
            case SDL_COPY_MUL:
                dstR = ((srcR * dstR) + (dstR * (1.0f - srcA)));
                dstG = ((srcG * dstG) + (dstG * (1.0f - srcA)));
                dstB = ((srcB * dstB) + (dstB * (1.0f - srcA)));
                break;
            }

            WriteFloatPixel(dst, dst_access, dst_fmt, dst_colorspace, dstR, dstG, dstB, dstA);

            posx += incx;
            dst += dstbpp;
        }
        posy += incy;
        info->dst += info->dst_pitch;
    }
}

