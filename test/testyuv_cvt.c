/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>

#include "testyuv_cvt.h"

#define YUV_SD_THRESHOLD 576

static YUV_CONVERSION_MODE YUV_ConversionMode = YUV_CONVERSION_BT601;

void SetYUVConversionMode(YUV_CONVERSION_MODE mode)
{
    YUV_ConversionMode = mode;
}

YUV_CONVERSION_MODE GetYUVConversionMode(void)
{
    return YUV_ConversionMode;
}

YUV_CONVERSION_MODE GetYUVConversionModeForResolution(int width, int height)
{
    YUV_CONVERSION_MODE mode = GetYUVConversionMode();
    if (mode == YUV_CONVERSION_AUTOMATIC) {
        if (height <= YUV_SD_THRESHOLD) {
            mode = YUV_CONVERSION_BT601;
        } else {
            mode = YUV_CONVERSION_BT709;
        }
    }
    return mode;
}

SDL_Colorspace GetColorspaceForYUVConversionMode(YUV_CONVERSION_MODE mode)
{
    SDL_Colorspace colorspace;

    switch (mode) {
    case YUV_CONVERSION_JPEG:
        colorspace = SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                           SDL_COLOR_RANGE_FULL,
                                           SDL_COLOR_PRIMARIES_BT709,
                                           SDL_TRANSFER_CHARACTERISTICS_BT601,
                                           SDL_MATRIX_COEFFICIENTS_BT601,
                                           SDL_CHROMA_LOCATION_CENTER);
        break;
    case YUV_CONVERSION_BT601:
        colorspace = SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                           SDL_COLOR_RANGE_LIMITED,
                                           SDL_COLOR_PRIMARIES_BT709,
                                           SDL_TRANSFER_CHARACTERISTICS_BT601,
                                           SDL_MATRIX_COEFFICIENTS_BT601,
                                           SDL_CHROMA_LOCATION_CENTER);
        break;
    case YUV_CONVERSION_BT709:
        colorspace = SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                           SDL_COLOR_RANGE_LIMITED,
                                           SDL_COLOR_PRIMARIES_BT709,
                                           SDL_TRANSFER_CHARACTERISTICS_BT709,
                                           SDL_MATRIX_COEFFICIENTS_BT709,
                                           SDL_CHROMA_LOCATION_CENTER);
        break;
    case YUV_CONVERSION_BT2020:
        colorspace = SDL_COLORSPACE_BT2020_FULL;
        break;
    default:
        colorspace = SDL_COLORSPACE_UNKNOWN;
        break;
    }
    return colorspace;
}

static float clip3(float x, float y, float z)
{
    return (z < x) ? x : ((z > y) ? y : z);
}

static float sRGBtoNits(float v)
{
    /* Normalize from 0..255 */
    v /= 255.0f;

    /* Convert from sRGB */
    v = v <= 0.04045f ? (v / 12.92f) : SDL_powf(((v + 0.055f) / 1.055f), 2.4f);

    /* Convert to nits, using a default SDR whitepoint of 203 */
    v *= 203.0f;

    return v;
}

static float PQfromNits(float v)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;

    float y = SDL_clamp(v / 10000.0f, 0.0f, 1.0f);
    float num = c1 + c2 * SDL_powf(y, m1);
    float den = 1.0f + c3 * SDL_powf(y, m1);
    return SDL_powf(num / den, m2);
}

void ConvertRec709toRec2020(float *fR, float *fG, float *fB)
{
    static const float mat709to2020[] = {
        0.627404f, 0.329283f, 0.043313f,
        0.069097f, 0.919541f, 0.011362f,
        0.016391f, 0.088013f, 0.895595f,
    };
    const float *matrix = mat709to2020;
    float v[3];

    v[0] = *fR;
    v[1] = *fG;
    v[2] = *fB;

    *fR = matrix[0 * 3 + 0] * v[0] + matrix[0 * 3 + 1] * v[1] + matrix[0 * 3 + 2] * v[2];
    *fG = matrix[1 * 3 + 0] * v[0] + matrix[1 * 3 + 1] * v[1] + matrix[1 * 3 + 2] * v[2];
    *fB = matrix[2 * 3 + 0] * v[0] + matrix[2 * 3 + 1] * v[1] + matrix[2 * 3 + 2] * v[2];
}

static void RGBtoYUV(const Uint8 *rgb, int rgb_bits, int *yuv, int yuv_bits, YUV_CONVERSION_MODE mode, int monochrome, int luminance)
{
    /**
     * This formula is from Microsoft's documentation:
     * https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
     * L = Kr * R + Kb * B + (1 - Kr - Kb) * G
     * Y =                   floor(2^(M-8) * (219*(L-Z)/S + 16) + 0.5);
     * U = clip3(0, (2^M)-1, floor(2^(M-8) * (112*(B-L) / ((1-Kb)*S) + 128) + 0.5));
     * V = clip3(0, (2^M)-1, floor(2^(M-8) * (112*(R-L) / ((1-Kr)*S) + 128) + 0.5));
     */
    SDL_bool studio_RGB = SDL_FALSE;
    SDL_bool full_range_YUV = SDL_FALSE;
    float N, M, S, Z, R, G, B, L, Kr, Kb, Y, U, V;

    N = (float)rgb_bits;
    M = (float)yuv_bits;
    switch (mode) {
    case YUV_CONVERSION_JPEG:
    case YUV_CONVERSION_BT601:
        /* BT.601 */
        Kr = 0.299f;
        Kb = 0.114f;
        break;
    case YUV_CONVERSION_BT709:
        /* BT.709 */
        Kr = 0.2126f;
        Kb = 0.0722f;
        break;
    case YUV_CONVERSION_BT2020:
        /* BT.2020 */
        Kr = 0.2627f;
        Kb = 0.0593f;
        break;
    default:
        /* Invalid */
        Kr = 1.0f;
        Kb = 1.0f;
        break;
    }

    R = rgb[0];
    G = rgb[1];
    B = rgb[2];

    if (mode == YUV_CONVERSION_JPEG || mode == YUV_CONVERSION_BT2020) {
        full_range_YUV = SDL_TRUE;
    }

    if (mode == YUV_CONVERSION_BT2020) {
        /* Input is sRGB, need to convert to BT.2020 PQ YUV */
        R = sRGBtoNits(R);
        G = sRGBtoNits(G);
        B = sRGBtoNits(B);
        ConvertRec709toRec2020(&R, &G, &B);
        R = PQfromNits(R);
        G = PQfromNits(G);
        B = PQfromNits(B);
        S = 1.0f;
        Z = 0.0f;
    } else if (studio_RGB) {
        S = 219.0f * SDL_powf(2.0f, N - 8);
        Z = 16.0f * SDL_powf(2.0f, N - 8);
    } else {
        S = 255.0f;
        Z = 0.0f;
    }
    L = Kr * R + Kb * B + (1 - Kr - Kb) * G;
    if (monochrome) {
        R = L;
        B = L;
    }
    if (full_range_YUV) {
        Y =                                 SDL_floorf((SDL_powf(2.0f, M) - 1) * ((L - Z) / S) + 0.5f);
        U = clip3(0, SDL_powf(2.0f, M) - 1, SDL_floorf((SDL_powf(2.0f, M) / 2 - 1) * ((B - L) / ((1.0f - Kb) * S)) + SDL_powf(2.0f, M) / 2 + 0.5f));
        V = clip3(0, SDL_powf(2.0f, M) - 1, SDL_floorf((SDL_powf(2.0f, M) / 2 - 1) * ((R - L) / ((1.0f - Kr) * S)) + SDL_powf(2.0f, M) / 2 + 0.5f));
    } else {
        Y =                                 SDL_floorf(SDL_powf(2.0f, (M - 8)) * (219.0f * (L - Z) / S + 16) + 0.5f);
        U = clip3(0, SDL_powf(2.0f, M) - 1, SDL_floorf(SDL_powf(2.0f, (M - 8)) * (112.0f * (B - L) / ((1.0f - Kb) * S) + 128) + 0.5f));
        V = clip3(0, SDL_powf(2.0f, M) - 1, SDL_floorf(SDL_powf(2.0f, (M - 8)) * (112.0f * (R - L) / ((1.0f - Kr) * S) + 128) + 0.5f));
    }

    yuv[0] = (int)Y;
    yuv[1] = (int)U;
    yuv[2] = (int)V;

    if (luminance != 100) {
        yuv[0] = (int)clip3(0, SDL_powf(2.0f, M) - 1, SDL_roundf(yuv[0] * (luminance / 100.0f)));
    }
}

static void ConvertRGBtoPlanar2x2(Uint32 format, Uint8 *src, int pitch, Uint8 *out, int w, int h, YUV_CONVERSION_MODE mode, int monochrome, int luminance)
{
    int x, y;
    int yuv[4][3];
    Uint8 *Y1, *Y2, *U, *V;
    Uint8 *rgb1, *rgb2;
    int rgb_row_advance = (pitch - w * 3) + pitch;
    int UV_advance;

    rgb1 = src;
    rgb2 = src + pitch;

    Y1 = out;
    Y2 = Y1 + w;
    switch (format) {
    case SDL_PIXELFORMAT_YV12:
        V = (Y1 + h * w);
        U = V + ((h + 1) / 2) * ((w + 1) / 2);
        UV_advance = 1;
        break;
    case SDL_PIXELFORMAT_IYUV:
        U = (Y1 + h * w);
        V = U + ((h + 1) / 2) * ((w + 1) / 2);
        UV_advance = 1;
        break;
    case SDL_PIXELFORMAT_NV12:
        U = (Y1 + h * w);
        V = U + 1;
        UV_advance = 2;
        break;
    case SDL_PIXELFORMAT_NV21:
        V = (Y1 + h * w);
        U = V + 1;
        UV_advance = 2;
        break;
    default:
        SDL_assert(!"Unsupported planar YUV format");
        return;
    }

    for (y = 0; y < (h - 1); y += 2) {
        for (x = 0; x < (w - 1); x += 2) {
            RGBtoYUV(rgb1, 8, yuv[0], 8, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = (Uint8)yuv[0][0];

            RGBtoYUV(rgb1, 8, yuv[1], 8, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = (Uint8)yuv[1][0];

            RGBtoYUV(rgb2, 8, yuv[2], 8, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = (Uint8)yuv[2][0];

            RGBtoYUV(rgb2, 8, yuv[3], 8, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = (Uint8)yuv[3][0];

            *U = (Uint8)SDL_floorf((yuv[0][1] + yuv[1][1] + yuv[2][1] + yuv[3][1]) / 4.0f + 0.5f);
            U += UV_advance;

            *V = (Uint8)SDL_floorf((yuv[0][2] + yuv[1][2] + yuv[2][2] + yuv[3][2]) / 4.0f + 0.5f);
            V += UV_advance;
        }
        /* Last column */
        if (x == (w - 1)) {
            RGBtoYUV(rgb1, 8, yuv[0], 8, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = (Uint8)yuv[0][0];

            RGBtoYUV(rgb2, 8, yuv[2], 8, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = (Uint8)yuv[2][0];

            *U = (Uint8)SDL_floorf((yuv[0][1] + yuv[2][1]) / 2.0f + 0.5f);
            U += UV_advance;

            *V = (Uint8)SDL_floorf((yuv[0][2] + yuv[2][2]) / 2.0f + 0.5f);
            V += UV_advance;
        }
        Y1 += w;
        Y2 += w;
        rgb1 += rgb_row_advance;
        rgb2 += rgb_row_advance;
    }
    /* Last row */
    if (y == (h - 1)) {
        for (x = 0; x < (w - 1); x += 2) {
            RGBtoYUV(rgb1, 8, yuv[0], 8, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = (Uint8)yuv[0][0];

            RGBtoYUV(rgb1, 8, yuv[1], 8, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = (Uint8)yuv[1][0];

            *U = (Uint8)SDL_floorf((yuv[0][1] + yuv[1][1]) / 2.0f + 0.5f);
            U += UV_advance;

            *V = (Uint8)SDL_floorf((yuv[0][2] + yuv[1][2]) / 2.0f + 0.5f);
            V += UV_advance;
        }
        /* Last column */
        if (x == (w - 1)) {
            RGBtoYUV(rgb1, 8, yuv[0], 8, mode, monochrome, luminance);
            *Y1++ = (Uint8)yuv[0][0];

            *U = (Uint8)yuv[0][1];
            U += UV_advance;

            *V = (Uint8)yuv[0][2];
            V += UV_advance;
        }
    }
}

static Uint16 Pack10to16(int v)
{
    return (Uint16)(v << 6);
}

static void ConvertRGBtoPlanar2x2_P010(Uint32 format, Uint8 *src, int pitch, Uint8 *out, int w, int h, YUV_CONVERSION_MODE mode, int monochrome, int luminance)
{
    int x, y;
    int yuv[4][3];
    Uint16 *Y1, *Y2, *U, *V;
    Uint8 *rgb1, *rgb2;
    int rgb_row_advance = (pitch - w * 3) + pitch;
    int UV_advance;

    rgb1 = src;
    rgb2 = src + pitch;

    Y1 = (Uint16 *)out;
    Y2 = Y1 + w;
    switch (format) {
    case SDL_PIXELFORMAT_P010:
        U = (Y1 + h * w);
        V = U + 1;
        UV_advance = 2;
        break;
    default:
        SDL_assert(!"Unsupported planar YUV format");
        return;
    }

    for (y = 0; y < (h - 1); y += 2) {
        for (x = 0; x < (w - 1); x += 2) {
            RGBtoYUV(rgb1, 8, yuv[0], 10, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = Pack10to16(yuv[0][0]);

            RGBtoYUV(rgb1, 8, yuv[1], 10, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = Pack10to16(yuv[1][0]);

            RGBtoYUV(rgb2, 8, yuv[2], 10, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = Pack10to16(yuv[2][0]);

            RGBtoYUV(rgb2, 8, yuv[3], 10, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = Pack10to16(yuv[3][0]);

            *U = Pack10to16((int)SDL_floorf((yuv[0][1] + yuv[1][1] + yuv[2][1] + yuv[3][1]) / 4.0f + 0.5f));
            U += UV_advance;

            *V = Pack10to16((int)SDL_floorf((yuv[0][2] + yuv[1][2] + yuv[2][2] + yuv[3][2]) / 4.0f + 0.5f));
            V += UV_advance;
        }
        /* Last column */
        if (x == (w - 1)) {
            RGBtoYUV(rgb1, 8, yuv[0], 10, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = Pack10to16(yuv[0][0]);

            RGBtoYUV(rgb2, 8, yuv[2], 10, mode, monochrome, luminance);
            rgb2 += 3;
            *Y2++ = Pack10to16(yuv[2][0]);

            *U = Pack10to16((int)SDL_floorf((yuv[0][1] + yuv[2][1]) / 2.0f + 0.5f));
            U += UV_advance;

            *V = Pack10to16((int)SDL_floorf((yuv[0][2] + yuv[2][2]) / 2.0f + 0.5f));
            V += UV_advance;
        }
        Y1 += w;
        Y2 += w;
        rgb1 += rgb_row_advance;
        rgb2 += rgb_row_advance;
    }
    /* Last row */
    if (y == (h - 1)) {
        for (x = 0; x < (w - 1); x += 2) {
            RGBtoYUV(rgb1, 8, yuv[0], 10, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = Pack10to16(yuv[0][0]);

            RGBtoYUV(rgb1, 8, yuv[1], 10, mode, monochrome, luminance);
            rgb1 += 3;
            *Y1++ = Pack10to16(yuv[1][0]);

            *U = Pack10to16((int)SDL_floorf((yuv[0][1] + yuv[1][1]) / 2.0f + 0.5f));
            U += UV_advance;

            *V = Pack10to16((int)SDL_floorf((yuv[0][2] + yuv[1][2]) / 2.0f + 0.5f));
            V += UV_advance;
        }
        /* Last column */
        if (x == (w - 1)) {
            RGBtoYUV(rgb1, 8, yuv[0], 10, mode, monochrome, luminance);
            *Y1++ = Pack10to16(yuv[0][0]);

            *U = Pack10to16(yuv[0][1]);
            U += UV_advance;

            *V = Pack10to16(yuv[0][2]);
            V += UV_advance;
        }
    }
}

static void ConvertRGBtoPacked4(Uint32 format, Uint8 *src, int pitch, Uint8 *out, int w, int h, YUV_CONVERSION_MODE mode, int monochrome, int luminance)
{
    int x, y;
    int yuv[2][3];
    Uint8 *Y1, *Y2, *U, *V;
    Uint8 *rgb;
    int rgb_row_advance = (pitch - w * 3);

    rgb = src;

    switch (format) {
    case SDL_PIXELFORMAT_YUY2:
        Y1 = out;
        U = out + 1;
        Y2 = out + 2;
        V = out + 3;
        break;
    case SDL_PIXELFORMAT_UYVY:
        U = out;
        Y1 = out + 1;
        V = out + 2;
        Y2 = out + 3;
        break;
    case SDL_PIXELFORMAT_YVYU:
        Y1 = out;
        V = out + 1;
        Y2 = out + 2;
        U = out + 3;
        break;
    default:
        SDL_assert(!"Unsupported packed YUV format");
        return;
    }

    for (y = 0; y < h; ++y) {
        for (x = 0; x < (w - 1); x += 2) {
            RGBtoYUV(rgb, 8, yuv[0], 8, mode, monochrome, luminance);
            rgb += 3;
            *Y1 = (Uint8)yuv[0][0];
            Y1 += 4;

            RGBtoYUV(rgb, 8, yuv[1], 8, mode, monochrome, luminance);
            rgb += 3;
            *Y2 = (Uint8)yuv[1][0];
            Y2 += 4;

            *U = (Uint8)SDL_floorf((yuv[0][1] + yuv[1][1]) / 2.0f + 0.5f);
            U += 4;

            *V = (Uint8)SDL_floorf((yuv[0][2] + yuv[1][2]) / 2.0f + 0.5f);
            V += 4;
        }
        /* Last column */
        if (x == (w - 1)) {
            RGBtoYUV(rgb, 8, yuv[0], 8, mode, monochrome, luminance);
            rgb += 3;
            *Y2 = *Y1 = (Uint8)yuv[0][0];
            Y1 += 4;
            Y2 += 4;

            *U = (Uint8)yuv[0][1];
            U += 4;

            *V = (Uint8)yuv[0][2];
            V += 4;
        }
        rgb += rgb_row_advance;
    }
}

SDL_bool ConvertRGBtoYUV(Uint32 format, Uint8 *src, int pitch, Uint8 *out, int w, int h, YUV_CONVERSION_MODE mode, int monochrome, int luminance)
{
    switch (format) {
    case SDL_PIXELFORMAT_P010:
        ConvertRGBtoPlanar2x2_P010(format, src, pitch, out, w, h, mode, monochrome, luminance);
        return SDL_TRUE;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        ConvertRGBtoPlanar2x2(format, src, pitch, out, w, h, mode, monochrome, luminance);
        return SDL_TRUE;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        ConvertRGBtoPacked4(format, src, pitch, out, w, h, mode, monochrome, luminance);
        return SDL_TRUE;
    default:
        return SDL_FALSE;
    }
}

int CalculateYUVPitch(Uint32 format, int width)
{
    switch (format) {
    case SDL_PIXELFORMAT_P010:
        return width * 2;
    case SDL_PIXELFORMAT_YV12:
    case SDL_PIXELFORMAT_IYUV:
    case SDL_PIXELFORMAT_NV12:
    case SDL_PIXELFORMAT_NV21:
        return width;
    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
        return 4 * ((width + 1) / 2);
    default:
        return 0;
    }
}
