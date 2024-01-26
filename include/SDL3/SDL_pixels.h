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

/**
 *  \file SDL_pixels.h
 *
 *  Header for the enumerated pixel format definitions.
 *
 *  SDL's pixel formats have the following naming convention:
 *
 *      * Names with a list of components and a single bit count, such as
 *        RGB24 and ABGR32, define a platform-independent encoding into
 *        bytes in the order specified. For example, in RGB24 data, each
 *        pixel is encoded in 3 bytes (red, green, blue) in that order,
 *        and in ABGR32 data, each pixel is encoded in 4 bytes
 *        (alpha, blue, green, red) in that order. Use these names if the
 *        property of a format that is important to you is the order of
 *        the bytes in memory or on disk.
 *
 *      * Names with a bit count per component, such as ARGB8888 and
 *        XRGB1555, are "packed" into an appropriately-sized integer in
 *        the platform's native endianness. For example, ARGB8888 is
 *        a sequence of 32-bit integers; in each integer, the most
 *        significant bits are alpha, and the least significant bits are
 *        blue. On a little-endian CPU such as x86, the least significant
 *        bits of each integer are arranged first in memory, but on a
 *        big-endian CPU such as s390x, the most significant bits are
 *        arranged first. Use these names if the property of a format that
 *        is important to you is the meaning of each bit position within a
 *        native-endianness integer.
 *
 *      * In indexed formats such as INDEX4LSB, each pixel is represented
 *        by encoding an index into the palette into the indicated number
 *        of bits, with multiple pixels packed into each byte if appropriate.
 *        In LSB formats, the first (leftmost) pixel is stored in the
 *        least-significant bits of the byte; in MSB formats, it's stored
 *        in the most-significant bits. INDEX8 does not need LSB/MSB
 *        variants, because each pixel exactly fills one byte.
 *
 *  The 32-bit byte-array encodings such as RGBA32 are aliases for the
 *  appropriate 8888 encoding for the current platform. For example,
 *  RGBA32 is an alias for ABGR8888 on little-endian CPUs like x86,
 *  or an alias for RGBA8888 on big-endian CPUs.
 */

#ifndef SDL_pixels_h_
#define SDL_pixels_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_endian.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \name Transparency definitions
 *
 *  These define alpha as the opacity of a surface.
 */
/* @{ */
#define SDL_ALPHA_OPAQUE 255
#define SDL_ALPHA_TRANSPARENT 0
/* @} */

/** Pixel type. */
typedef enum
{
    SDL_PIXELTYPE_UNKNOWN,
    SDL_PIXELTYPE_INDEX1,
    SDL_PIXELTYPE_INDEX4,
    SDL_PIXELTYPE_INDEX8,
    SDL_PIXELTYPE_PACKED8,
    SDL_PIXELTYPE_PACKED16,
    SDL_PIXELTYPE_PACKED32,
    SDL_PIXELTYPE_ARRAYU8,
    SDL_PIXELTYPE_ARRAYU16,
    SDL_PIXELTYPE_ARRAYU32,
    SDL_PIXELTYPE_ARRAYF16,
    SDL_PIXELTYPE_ARRAYF32,
    /* appended at the end for compatibility with sdl2-compat:  */
    SDL_PIXELTYPE_INDEX2
} SDL_PixelType;

/** Bitmap pixel order, high bit -> low bit. */
typedef enum
{
    SDL_BITMAPORDER_NONE,
    SDL_BITMAPORDER_4321,
    SDL_BITMAPORDER_1234
} SDL_BitmapOrder;

/** Packed component order, high bit -> low bit. */
typedef enum
{
    SDL_PACKEDORDER_NONE,
    SDL_PACKEDORDER_XRGB,
    SDL_PACKEDORDER_RGBX,
    SDL_PACKEDORDER_ARGB,
    SDL_PACKEDORDER_RGBA,
    SDL_PACKEDORDER_XBGR,
    SDL_PACKEDORDER_BGRX,
    SDL_PACKEDORDER_ABGR,
    SDL_PACKEDORDER_BGRA
} SDL_PackedOrder;

/** Array component order, low byte -> high byte. */
typedef enum
{
    SDL_ARRAYORDER_NONE,
    SDL_ARRAYORDER_RGB,
    SDL_ARRAYORDER_RGBA,
    SDL_ARRAYORDER_ARGB,
    SDL_ARRAYORDER_BGR,
    SDL_ARRAYORDER_BGRA,
    SDL_ARRAYORDER_ABGR
} SDL_ArrayOrder;

/** Packed component layout. */
typedef enum
{
    SDL_PACKEDLAYOUT_NONE,
    SDL_PACKEDLAYOUT_332,
    SDL_PACKEDLAYOUT_4444,
    SDL_PACKEDLAYOUT_1555,
    SDL_PACKEDLAYOUT_5551,
    SDL_PACKEDLAYOUT_565,
    SDL_PACKEDLAYOUT_8888,
    SDL_PACKEDLAYOUT_2101010,
    SDL_PACKEDLAYOUT_1010102
} SDL_PackedLayout;

#define SDL_DEFINE_PIXELFOURCC(A, B, C, D) SDL_FOURCC(A, B, C, D)

#define SDL_DEFINE_PIXELFORMAT(type, order, layout, bits, bytes) \
    ((1 << 28) | ((type) << 24) | ((order) << 20) | ((layout) << 16) | \
     ((bits) << 8) | ((bytes) << 0))

#define SDL_PIXELFLAG(X)    (((X) >> 28) & 0x0F)
#define SDL_PIXELTYPE(X)    (((X) >> 24) & 0x0F)
#define SDL_PIXELORDER(X)   (((X) >> 20) & 0x0F)
#define SDL_PIXELLAYOUT(X)  (((X) >> 16) & 0x0F)
#define SDL_BITSPERPIXEL(X) (((X) >> 8) & 0xFF)
#define SDL_BYTESPERPIXEL(X) \
    (SDL_ISPIXELFORMAT_FOURCC(X) ? \
        ((((X) == SDL_PIXELFORMAT_YUY2) || \
          ((X) == SDL_PIXELFORMAT_UYVY) || \
          ((X) == SDL_PIXELFORMAT_YVYU)) ? 2 : 1) : (((X) >> 0) & 0xFF))

#define SDL_ISPIXELFORMAT_INDEXED(format)   \
    (!SDL_ISPIXELFORMAT_FOURCC(format) && \
     ((SDL_PIXELTYPE(format) == SDL_PIXELTYPE_INDEX1) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_INDEX2) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_INDEX4) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_INDEX8)))

#define SDL_ISPIXELFORMAT_PACKED(format) \
    (!SDL_ISPIXELFORMAT_FOURCC(format) && \
     ((SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED8) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED16) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32)))

#define SDL_ISPIXELFORMAT_ARRAY(format) \
    (!SDL_ISPIXELFORMAT_FOURCC(format) && \
     ((SDL_PIXELTYPE(format) == SDL_PIXELTYPE_ARRAYU8) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_ARRAYU16) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_ARRAYU32) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_ARRAYF16) || \
      (SDL_PIXELTYPE(format) == SDL_PIXELTYPE_ARRAYF32)))

#define SDL_ISPIXELFORMAT_ALPHA(format)   \
    ((SDL_ISPIXELFORMAT_PACKED(format) && \
     ((SDL_PIXELORDER(format) == SDL_PACKEDORDER_ARGB) || \
      (SDL_PIXELORDER(format) == SDL_PACKEDORDER_RGBA) || \
      (SDL_PIXELORDER(format) == SDL_PACKEDORDER_ABGR) || \
      (SDL_PIXELORDER(format) == SDL_PACKEDORDER_BGRA))))

#define SDL_ISPIXELFORMAT_10BIT(format)    \
      ((SDL_PIXELTYPE(format) == SDL_PIXELTYPE_PACKED32) && \
       (SDL_PIXELLAYOUT(format) == SDL_PACKEDLAYOUT_2101010))

/* The flag is set to 1 because 0x1? is not in the printable ASCII range */
#define SDL_ISPIXELFORMAT_FOURCC(format)    \
    ((format) && (SDL_PIXELFLAG(format) != 1))

/* Note: If you modify this list, update SDL_GetPixelFormatName() */
typedef enum
{
    SDL_PIXELFORMAT_UNKNOWN,
    SDL_PIXELFORMAT_INDEX1LSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX1, SDL_BITMAPORDER_4321, 0,
                               1, 0),
    SDL_PIXELFORMAT_INDEX1MSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX1, SDL_BITMAPORDER_1234, 0,
                               1, 0),
    SDL_PIXELFORMAT_INDEX2LSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX2, SDL_BITMAPORDER_4321, 0,
                               2, 0),
    SDL_PIXELFORMAT_INDEX2MSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX2, SDL_BITMAPORDER_1234, 0,
                               2, 0),
    SDL_PIXELFORMAT_INDEX4LSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX4, SDL_BITMAPORDER_4321, 0,
                               4, 0),
    SDL_PIXELFORMAT_INDEX4MSB =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX4, SDL_BITMAPORDER_1234, 0,
                               4, 0),
    SDL_PIXELFORMAT_INDEX8 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 8, 1),
    SDL_PIXELFORMAT_RGB332 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED8, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_332, 8, 1),
    SDL_PIXELFORMAT_XRGB4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_4444, 12, 2),
    SDL_PIXELFORMAT_RGB444 = SDL_PIXELFORMAT_XRGB4444,
    SDL_PIXELFORMAT_XBGR4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR,
                               SDL_PACKEDLAYOUT_4444, 12, 2),
    SDL_PIXELFORMAT_BGR444 = SDL_PIXELFORMAT_XBGR4444,
    SDL_PIXELFORMAT_XRGB1555 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_1555, 15, 2),
    SDL_PIXELFORMAT_RGB555 = SDL_PIXELFORMAT_XRGB1555,
    SDL_PIXELFORMAT_XBGR1555 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR,
                               SDL_PACKEDLAYOUT_1555, 15, 2),
    SDL_PIXELFORMAT_BGR555 = SDL_PIXELFORMAT_XBGR1555,
    SDL_PIXELFORMAT_ARGB4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ARGB,
                               SDL_PACKEDLAYOUT_4444, 16, 2),
    SDL_PIXELFORMAT_RGBA4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_RGBA,
                               SDL_PACKEDLAYOUT_4444, 16, 2),
    SDL_PIXELFORMAT_ABGR4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ABGR,
                               SDL_PACKEDLAYOUT_4444, 16, 2),
    SDL_PIXELFORMAT_BGRA4444 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_BGRA,
                               SDL_PACKEDLAYOUT_4444, 16, 2),
    SDL_PIXELFORMAT_ARGB1555 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ARGB,
                               SDL_PACKEDLAYOUT_1555, 16, 2),
    SDL_PIXELFORMAT_RGBA5551 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_RGBA,
                               SDL_PACKEDLAYOUT_5551, 16, 2),
    SDL_PIXELFORMAT_ABGR1555 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ABGR,
                               SDL_PACKEDLAYOUT_1555, 16, 2),
    SDL_PIXELFORMAT_BGRA5551 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_BGRA,
                               SDL_PACKEDLAYOUT_5551, 16, 2),
    SDL_PIXELFORMAT_RGB565 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_565, 16, 2),
    SDL_PIXELFORMAT_BGR565 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR,
                               SDL_PACKEDLAYOUT_565, 16, 2),
    SDL_PIXELFORMAT_RGB24 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_RGB, 0,
                               24, 3),
    SDL_PIXELFORMAT_BGR24 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_BGR, 0,
                               24, 3),
    SDL_PIXELFORMAT_XRGB8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_8888, 24, 4),
    SDL_PIXELFORMAT_RGBX8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBX,
                               SDL_PACKEDLAYOUT_8888, 24, 4),
    SDL_PIXELFORMAT_XBGR8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XBGR,
                               SDL_PACKEDLAYOUT_8888, 24, 4),
    SDL_PIXELFORMAT_BGRX8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_BGRX,
                               SDL_PACKEDLAYOUT_8888, 24, 4),
    SDL_PIXELFORMAT_ARGB8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB,
                               SDL_PACKEDLAYOUT_8888, 32, 4),
    SDL_PIXELFORMAT_RGBA8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBA,
                               SDL_PACKEDLAYOUT_8888, 32, 4),
    SDL_PIXELFORMAT_ABGR8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR,
                               SDL_PACKEDLAYOUT_8888, 32, 4),
    SDL_PIXELFORMAT_BGRA8888 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_BGRA,
                               SDL_PACKEDLAYOUT_8888, 32, 4),
    SDL_PIXELFORMAT_XRGB2101010 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB,
                               SDL_PACKEDLAYOUT_2101010, 32, 4),
    SDL_PIXELFORMAT_XBGR2101010 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XBGR,
                               SDL_PACKEDLAYOUT_2101010, 32, 4),
    SDL_PIXELFORMAT_ARGB2101010 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB,
                               SDL_PACKEDLAYOUT_2101010, 32, 4),
    SDL_PIXELFORMAT_ABGR2101010 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR,
                               SDL_PACKEDLAYOUT_2101010, 32, 4),
    SDL_PIXELFORMAT_RGB48 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_RGB, 0,
                               48, 3),
    SDL_PIXELFORMAT_BGR48 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_BGR, 0,
                               48, 3),
    SDL_PIXELFORMAT_RGBA64 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_RGBA, 0,
                               64, 4),
    SDL_PIXELFORMAT_ARGB64 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_ARGB, 0,
                               64, 4),
    SDL_PIXELFORMAT_BGRA64 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_BGRA, 0,
                               64, 4),
    SDL_PIXELFORMAT_ABGR64 =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_ABGR, 0,
                               64, 4),
    SDL_PIXELFORMAT_RGB48_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_RGB, 0,
                               48, 3),
    SDL_PIXELFORMAT_BGR48_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_BGR, 0,
                               48, 3),
    SDL_PIXELFORMAT_RGBA64_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_RGBA, 0,
                               64, 4),
    SDL_PIXELFORMAT_ARGB64_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_ARGB, 0,
                               64, 4),
    SDL_PIXELFORMAT_BGRA64_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_BGRA, 0,
                               64, 4),
    SDL_PIXELFORMAT_ABGR64_FLOAT =
        SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_ABGR, 0,
                               64, 4),

    /* Aliases for RGBA byte arrays of color data, for the current platform */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    SDL_PIXELFORMAT_RGBA32 = SDL_PIXELFORMAT_RGBA8888,
    SDL_PIXELFORMAT_ARGB32 = SDL_PIXELFORMAT_ARGB8888,
    SDL_PIXELFORMAT_BGRA32 = SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_ABGR32 = SDL_PIXELFORMAT_ABGR8888,
    SDL_PIXELFORMAT_RGBX32 = SDL_PIXELFORMAT_RGBX8888,
    SDL_PIXELFORMAT_XRGB32 = SDL_PIXELFORMAT_XRGB8888,
    SDL_PIXELFORMAT_BGRX32 = SDL_PIXELFORMAT_BGRX8888,
    SDL_PIXELFORMAT_XBGR32 = SDL_PIXELFORMAT_XBGR8888,
#else
    SDL_PIXELFORMAT_RGBA32 = SDL_PIXELFORMAT_ABGR8888,
    SDL_PIXELFORMAT_ARGB32 = SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_BGRA32 = SDL_PIXELFORMAT_ARGB8888,
    SDL_PIXELFORMAT_ABGR32 = SDL_PIXELFORMAT_RGBA8888,
    SDL_PIXELFORMAT_RGBX32 = SDL_PIXELFORMAT_XBGR8888,
    SDL_PIXELFORMAT_XRGB32 = SDL_PIXELFORMAT_BGRX8888,
    SDL_PIXELFORMAT_BGRX32 = SDL_PIXELFORMAT_XRGB8888,
    SDL_PIXELFORMAT_XBGR32 = SDL_PIXELFORMAT_RGBX8888,
#endif

    SDL_PIXELFORMAT_YV12 =      /**< Planar mode: Y + V + U  (3 planes) */
        SDL_DEFINE_PIXELFOURCC('Y', 'V', '1', '2'),
    SDL_PIXELFORMAT_IYUV =      /**< Planar mode: Y + U + V  (3 planes) */
        SDL_DEFINE_PIXELFOURCC('I', 'Y', 'U', 'V'),
    SDL_PIXELFORMAT_YUY2 =      /**< Packed mode: Y0+U0+Y1+V0 (1 plane) */
        SDL_DEFINE_PIXELFOURCC('Y', 'U', 'Y', '2'),
    SDL_PIXELFORMAT_UYVY =      /**< Packed mode: U0+Y0+V0+Y1 (1 plane) */
        SDL_DEFINE_PIXELFOURCC('U', 'Y', 'V', 'Y'),
    SDL_PIXELFORMAT_YVYU =      /**< Packed mode: Y0+V0+Y1+U0 (1 plane) */
        SDL_DEFINE_PIXELFOURCC('Y', 'V', 'Y', 'U'),
    SDL_PIXELFORMAT_NV12 =      /**< Planar mode: Y + U/V interleaved  (2 planes) */
        SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2'),
    SDL_PIXELFORMAT_NV21 =      /**< Planar mode: Y + V/U interleaved  (2 planes) */
        SDL_DEFINE_PIXELFOURCC('N', 'V', '2', '1'),
    SDL_PIXELFORMAT_EXTERNAL_OES =      /**< Android video texture format */
        SDL_DEFINE_PIXELFOURCC('O', 'E', 'S', ' ')
} SDL_PixelFormatEnum;

/**
 * The bits of this structure can be directly reinterpreted as an integer-packed
 * color which uses the SDL_PIXELFORMAT_RGBA32 format (SDL_PIXELFORMAT_ABGR8888
 * on little-endian systems and SDL_PIXELFORMAT_RGBA8888 on big-endian systems).
 */
typedef struct SDL_Color
{
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} SDL_Color;
#define SDL_Colour SDL_Color

typedef struct SDL_Palette
{
    int ncolors;
    SDL_Color *colors;
    Uint32 version;
    int refcount;
} SDL_Palette;

/**
 *  \note Everything in the pixel format structure is read-only.
 */
typedef struct SDL_PixelFormat
{
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 padding[2];
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    Uint8 Rloss;
    Uint8 Gloss;
    Uint8 Bloss;
    Uint8 Aloss;
    Uint8 Rshift;
    Uint8 Gshift;
    Uint8 Bshift;
    Uint8 Ashift;
    int refcount;
    struct SDL_PixelFormat *next;
} SDL_PixelFormat;

/**
 * Get the human readable name of a pixel format.
 *
 * \param format the pixel format to query
 * \returns the human readable name of the specified pixel format or
 *          `SDL_PIXELFORMAT_UNKNOWN` if the format isn't recognized.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char* SDLCALL SDL_GetPixelFormatName(Uint32 format);

/**
 * Convert one of the enumerated pixel formats to a bpp value and RGBA masks.
 *
 * \param format one of the SDL_PixelFormatEnum values
 * \param bpp a bits per pixel value; usually 15, 16, or 32
 * \param Rmask a pointer filled in with the red mask for the format
 * \param Gmask a pointer filled in with the green mask for the format
 * \param Bmask a pointer filled in with the blue mask for the format
 * \param Amask a pointer filled in with the alpha mask for the format
 * \returns SDL_TRUE on success or SDL_FALSE if the conversion wasn't
 *          possible; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPixelFormatEnumForMasks
 */
extern DECLSPEC SDL_bool SDLCALL SDL_GetMasksForPixelFormatEnum(Uint32 format,
                                                            int *bpp,
                                                            Uint32 * Rmask,
                                                            Uint32 * Gmask,
                                                            Uint32 * Bmask,
                                                            Uint32 * Amask);

/**
 * Convert a bpp value and RGBA masks to an enumerated pixel format.
 *
 * This will return `SDL_PIXELFORMAT_UNKNOWN` if the conversion wasn't
 * possible.
 *
 * \param bpp a bits per pixel value; usually 15, 16, or 32
 * \param Rmask the red mask for the format
 * \param Gmask the green mask for the format
 * \param Bmask the blue mask for the format
 * \param Amask the alpha mask for the format
 * \returns one of the SDL_PixelFormatEnum values
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetMasksForPixelFormatEnum
 */
extern DECLSPEC Uint32 SDLCALL SDL_GetPixelFormatEnumForMasks(int bpp,
                                                          Uint32 Rmask,
                                                          Uint32 Gmask,
                                                          Uint32 Bmask,
                                                          Uint32 Amask);

/**
 * Create an SDL_PixelFormat structure corresponding to a pixel format.
 *
 * Returned structure may come from a shared global cache (i.e. not newly
 * allocated), and hence should not be modified, especially the palette. Weird
 * errors such as `Blit combination not supported` may occur.
 *
 * \param pixel_format one of the SDL_PixelFormatEnum values
 * \returns the new SDL_PixelFormat structure or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyPixelFormat
 */
extern DECLSPEC SDL_PixelFormat * SDLCALL SDL_CreatePixelFormat(Uint32 pixel_format);

/**
 * Free an SDL_PixelFormat structure allocated by SDL_CreatePixelFormat().
 *
 * \param format the SDL_PixelFormat structure to free
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePixelFormat
 */
extern DECLSPEC void SDLCALL SDL_DestroyPixelFormat(SDL_PixelFormat *format);

/**
 * Create a palette structure with the specified number of color entries.
 *
 * The palette entries are initialized to white.
 *
 * \param ncolors represents the number of color entries in the color palette
 * \returns a new SDL_Palette structure on success or NULL on failure (e.g. if
 *          there wasn't enough memory); call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyPalette
 */
extern DECLSPEC SDL_Palette *SDLCALL SDL_CreatePalette(int ncolors);

/**
 * Set the palette for a pixel format structure.
 *
 * \param format the SDL_PixelFormat structure that will use the palette
 * \param palette the SDL_Palette structure that will be used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePalette
 * \sa SDL_DestroyPalette
 */
extern DECLSPEC int SDLCALL SDL_SetPixelFormatPalette(SDL_PixelFormat * format,
                                                      SDL_Palette *palette);

/**
 * Set a range of colors in a palette.
 *
 * \param palette the SDL_Palette structure to modify
 * \param colors an array of SDL_Color structures to copy into the palette
 * \param firstcolor the index of the first palette entry to modify
 * \param ncolors the number of entries to modify
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePalette
 * \sa SDL_CreateSurface
 */
extern DECLSPEC int SDLCALL SDL_SetPaletteColors(SDL_Palette * palette,
                                                 const SDL_Color * colors,
                                                 int firstcolor, int ncolors);

/**
 * Free a palette created with SDL_CreatePalette().
 *
 * \param palette the SDL_Palette structure to be freed
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreatePalette
 */
extern DECLSPEC void SDLCALL SDL_DestroyPalette(SDL_Palette * palette);

/**
 * Map an RGB triple to an opaque pixel value for a given pixel format.
 *
 * This function maps the RGB color value to the specified pixel format and
 * returns the pixel value best approximating the given RGB color value for
 * the given pixel format.
 *
 * If the format has a palette (8-bit) the index of the closest matching color
 * in the palette will be returned.
 *
 * If the specified pixel format has an alpha component it will be returned as
 * all 1 bits (fully opaque).
 *
 * If the pixel format bpp (color depth) is less than 32-bpp then the unused
 * upper bits of the return value can safely be ignored (e.g., with a 16-bpp
 * format the return value can be assigned to a Uint16, and similarly a Uint8
 * for an 8-bpp format).
 *
 * \param format an SDL_PixelFormat structure describing the pixel format
 * \param r the red component of the pixel in the range 0-255
 * \param g the green component of the pixel in the range 0-255
 * \param b the blue component of the pixel in the range 0-255
 * \returns a pixel value
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRGB
 * \sa SDL_GetRGBA
 * \sa SDL_MapRGBA
 */
extern DECLSPEC Uint32 SDLCALL SDL_MapRGB(const SDL_PixelFormat * format,
                                          Uint8 r, Uint8 g, Uint8 b);

/**
 * Map an RGBA quadruple to a pixel value for a given pixel format.
 *
 * This function maps the RGBA color value to the specified pixel format and
 * returns the pixel value best approximating the given RGBA color value for
 * the given pixel format.
 *
 * If the specified pixel format has no alpha component the alpha value will
 * be ignored (as it will be in formats with a palette).
 *
 * If the format has a palette (8-bit) the index of the closest matching color
 * in the palette will be returned.
 *
 * If the pixel format bpp (color depth) is less than 32-bpp then the unused
 * upper bits of the return value can safely be ignored (e.g., with a 16-bpp
 * format the return value can be assigned to a Uint16, and similarly a Uint8
 * for an 8-bpp format).
 *
 * \param format an SDL_PixelFormat structure describing the format of the
 *               pixel
 * \param r the red component of the pixel in the range 0-255
 * \param g the green component of the pixel in the range 0-255
 * \param b the blue component of the pixel in the range 0-255
 * \param a the alpha component of the pixel in the range 0-255
 * \returns a pixel value
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRGB
 * \sa SDL_GetRGBA
 * \sa SDL_MapRGB
 */
extern DECLSPEC Uint32 SDLCALL SDL_MapRGBA(const SDL_PixelFormat * format,
                                           Uint8 r, Uint8 g, Uint8 b,
                                           Uint8 a);

/**
 * Get RGB values from a pixel in the specified format.
 *
 * This function uses the entire 8-bit [0..255] range when converting color
 * components from pixel formats with less than 8-bits per RGB component
 * (e.g., a completely white pixel in 16-bit RGB565 format would return [0xff,
 * 0xff, 0xff] not [0xf8, 0xfc, 0xf8]).
 *
 * \param pixel a pixel value
 * \param format an SDL_PixelFormat structure describing the format of the
 *               pixel
 * \param r a pointer filled in with the red component
 * \param g a pointer filled in with the green component
 * \param b a pointer filled in with the blue component
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRGBA
 * \sa SDL_MapRGB
 * \sa SDL_MapRGBA
 */
extern DECLSPEC void SDLCALL SDL_GetRGB(Uint32 pixel,
                                        const SDL_PixelFormat * format,
                                        Uint8 * r, Uint8 * g, Uint8 * b);

/**
 * Get RGBA values from a pixel in the specified format.
 *
 * This function uses the entire 8-bit [0..255] range when converting color
 * components from pixel formats with less than 8-bits per RGB component
 * (e.g., a completely white pixel in 16-bit RGB565 format would return [0xff,
 * 0xff, 0xff] not [0xf8, 0xfc, 0xf8]).
 *
 * If the surface has no alpha component, the alpha will be returned as 0xff
 * (100% opaque).
 *
 * \param pixel a pixel value
 * \param format an SDL_PixelFormat structure describing the format of the
 *               pixel
 * \param r a pointer filled in with the red component
 * \param g a pointer filled in with the green component
 * \param b a pointer filled in with the blue component
 * \param a a pointer filled in with the alpha component
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRGB
 * \sa SDL_MapRGB
 * \sa SDL_MapRGBA
 */
extern DECLSPEC void SDLCALL SDL_GetRGBA(Uint32 pixel,
                                         const SDL_PixelFormat * format,
                                         Uint8 * r, Uint8 * g, Uint8 * b,
                                         Uint8 * a);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_pixels_h_ */
