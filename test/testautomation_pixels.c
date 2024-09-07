/**
 * Pixels test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/* Definition of all RGB formats used to test pixel conversions */
static const SDL_PixelFormat g_AllFormats[] = {
    SDL_PIXELFORMAT_INDEX1LSB,
    SDL_PIXELFORMAT_INDEX1MSB,
    SDL_PIXELFORMAT_INDEX2LSB,
    SDL_PIXELFORMAT_INDEX2MSB,
    SDL_PIXELFORMAT_INDEX4LSB,
    SDL_PIXELFORMAT_INDEX4MSB,
    SDL_PIXELFORMAT_INDEX8,
    SDL_PIXELFORMAT_RGB332,
    SDL_PIXELFORMAT_XRGB4444,
    SDL_PIXELFORMAT_XBGR4444,
    SDL_PIXELFORMAT_XRGB1555,
    SDL_PIXELFORMAT_XBGR1555,
    SDL_PIXELFORMAT_ARGB4444,
    SDL_PIXELFORMAT_RGBA4444,
    SDL_PIXELFORMAT_ABGR4444,
    SDL_PIXELFORMAT_BGRA4444,
    SDL_PIXELFORMAT_ARGB1555,
    SDL_PIXELFORMAT_RGBA5551,
    SDL_PIXELFORMAT_ABGR1555,
    SDL_PIXELFORMAT_BGRA5551,
    SDL_PIXELFORMAT_RGB565,
    SDL_PIXELFORMAT_BGR565,
    SDL_PIXELFORMAT_RGB24,
    SDL_PIXELFORMAT_BGR24,
    SDL_PIXELFORMAT_XRGB8888,
    SDL_PIXELFORMAT_RGBX8888,
    SDL_PIXELFORMAT_XBGR8888,
    SDL_PIXELFORMAT_BGRX8888,
    SDL_PIXELFORMAT_ARGB8888,
    SDL_PIXELFORMAT_RGBA8888,
    SDL_PIXELFORMAT_ABGR8888,
    SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_XRGB2101010,
    SDL_PIXELFORMAT_XBGR2101010,
    SDL_PIXELFORMAT_ARGB2101010,
    SDL_PIXELFORMAT_ABGR2101010,
    SDL_PIXELFORMAT_YV12,
    SDL_PIXELFORMAT_IYUV,
    SDL_PIXELFORMAT_YUY2,
    SDL_PIXELFORMAT_UYVY,
    SDL_PIXELFORMAT_YVYU,
    SDL_PIXELFORMAT_NV12,
    SDL_PIXELFORMAT_NV21
};
static const int g_numAllFormats = SDL_arraysize(g_AllFormats);

static const char *g_AllFormatsVerbose[] = {
    "SDL_PIXELFORMAT_INDEX1LSB",
    "SDL_PIXELFORMAT_INDEX1MSB",
    "SDL_PIXELFORMAT_INDEX2LSB",
    "SDL_PIXELFORMAT_INDEX2MSB",
    "SDL_PIXELFORMAT_INDEX4LSB",
    "SDL_PIXELFORMAT_INDEX4MSB",
    "SDL_PIXELFORMAT_INDEX8",
    "SDL_PIXELFORMAT_RGB332",
    "SDL_PIXELFORMAT_XRGB4444",
    "SDL_PIXELFORMAT_XBGR4444",
    "SDL_PIXELFORMAT_XRGB1555",
    "SDL_PIXELFORMAT_XBGR1555",
    "SDL_PIXELFORMAT_ARGB4444",
    "SDL_PIXELFORMAT_RGBA4444",
    "SDL_PIXELFORMAT_ABGR4444",
    "SDL_PIXELFORMAT_BGRA4444",
    "SDL_PIXELFORMAT_ARGB1555",
    "SDL_PIXELFORMAT_RGBA5551",
    "SDL_PIXELFORMAT_ABGR1555",
    "SDL_PIXELFORMAT_BGRA5551",
    "SDL_PIXELFORMAT_RGB565",
    "SDL_PIXELFORMAT_BGR565",
    "SDL_PIXELFORMAT_RGB24",
    "SDL_PIXELFORMAT_BGR24",
    "SDL_PIXELFORMAT_XRGB8888",
    "SDL_PIXELFORMAT_RGBX8888",
    "SDL_PIXELFORMAT_XBGR8888",
    "SDL_PIXELFORMAT_BGRX8888",
    "SDL_PIXELFORMAT_ARGB8888",
    "SDL_PIXELFORMAT_RGBA8888",
    "SDL_PIXELFORMAT_ABGR8888",
    "SDL_PIXELFORMAT_BGRA8888",
    "SDL_PIXELFORMAT_XRGB2101010",
    "SDL_PIXELFORMAT_XBGR2101010",
    "SDL_PIXELFORMAT_ARGB2101010",
    "SDL_PIXELFORMAT_ABGR2101010",
    "SDL_PIXELFORMAT_YV12",
    "SDL_PIXELFORMAT_IYUV",
    "SDL_PIXELFORMAT_YUY2",
    "SDL_PIXELFORMAT_UYVY",
    "SDL_PIXELFORMAT_YVYU",
    "SDL_PIXELFORMAT_NV12",
    "SDL_PIXELFORMAT_NV21"
};

/* Definition of some invalid formats for negative tests */
static Uint32 g_invalidPixelFormats[] = {
    SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_1010102 + 1, 32, 4),
    SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_1010102 + 2, 32, 4)
};
static const int g_numInvalidPixelFormats = SDL_arraysize(g_invalidPixelFormats);
static const char *g_invalidPixelFormatsVerbose[] = {
    "SDL_PIXELFORMAT_UNKNOWN",
    "SDL_PIXELFORMAT_UNKNOWN"
};

/* Verify the pixel formats are laid out as expected */
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX1LSB_FORMAT, SDL_PIXELFORMAT_INDEX1LSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX1, SDL_BITMAPORDER_4321, 0, 1, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX1MSB_FORMAT, SDL_PIXELFORMAT_INDEX1MSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX1, SDL_BITMAPORDER_1234, 0, 1, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX2LSB_FORMAT, SDL_PIXELFORMAT_INDEX2LSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX2, SDL_BITMAPORDER_4321, 0, 2, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX2MSB_FORMAT, SDL_PIXELFORMAT_INDEX2MSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX2, SDL_BITMAPORDER_1234, 0, 2, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX4LSB_FORMAT, SDL_PIXELFORMAT_INDEX4LSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX4, SDL_BITMAPORDER_4321, 0, 4, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX4MSB_FORMAT, SDL_PIXELFORMAT_INDEX4MSB == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX4, SDL_BITMAPORDER_1234, 0, 4, 0));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_INDEX8_FORMAT, SDL_PIXELFORMAT_INDEX8 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 8, 1));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB332_FORMAT, SDL_PIXELFORMAT_RGB332 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED8, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_332, 8, 1));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XRGB4444_FORMAT, SDL_PIXELFORMAT_XRGB4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_4444, 12, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XBGR4444_FORMAT, SDL_PIXELFORMAT_XBGR4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_4444, 12, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XRGB1555_FORMAT, SDL_PIXELFORMAT_XRGB1555 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_1555, 15, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XBGR1555_FORMAT, SDL_PIXELFORMAT_XBGR1555 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_1555, 15, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB4444_FORMAT, SDL_PIXELFORMAT_ARGB4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_4444, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA4444_FORMAT, SDL_PIXELFORMAT_RGBA4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_RGBA, SDL_PACKEDLAYOUT_4444, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR4444_FORMAT, SDL_PIXELFORMAT_ABGR4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_4444, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA4444_FORMAT, SDL_PIXELFORMAT_BGRA4444 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_BGRA, SDL_PACKEDLAYOUT_4444, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB1555_FORMAT, SDL_PIXELFORMAT_ARGB1555 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_1555, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA5551_FORMAT, SDL_PIXELFORMAT_RGBA5551 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_RGBA, SDL_PACKEDLAYOUT_5551, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR1555_FORMAT, SDL_PIXELFORMAT_ABGR1555 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_1555, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA5551_FORMAT, SDL_PIXELFORMAT_BGRA5551 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_BGRA, SDL_PACKEDLAYOUT_5551, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB565_FORMAT, SDL_PIXELFORMAT_RGB565 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_565, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGR565_FORMAT, SDL_PIXELFORMAT_BGR565 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_565, 16, 2));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB24_FORMAT, SDL_PIXELFORMAT_RGB24 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_RGB, 0, 24, 3));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGR24_FORMAT, SDL_PIXELFORMAT_BGR24 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_BGR, 0, 24, 3));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XRGB8888_FORMAT, SDL_PIXELFORMAT_XRGB8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_8888, 24, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBX8888_FORMAT, SDL_PIXELFORMAT_RGBX8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBX, SDL_PACKEDLAYOUT_8888, 24, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XBGR8888_FORMAT, SDL_PIXELFORMAT_XBGR8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_8888, 24, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRX8888_FORMAT, SDL_PIXELFORMAT_BGRX8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_BGRX, SDL_PACKEDLAYOUT_8888, 24, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB8888_FORMAT, SDL_PIXELFORMAT_ARGB8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_8888, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA8888_FORMAT, SDL_PIXELFORMAT_RGBA8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBA, SDL_PACKEDLAYOUT_8888, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR8888_FORMAT, SDL_PIXELFORMAT_ABGR8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_8888, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA8888_FORMAT, SDL_PIXELFORMAT_BGRA8888 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_BGRA, SDL_PACKEDLAYOUT_8888, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XRGB2101010_FORMAT, SDL_PIXELFORMAT_XRGB2101010 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_2101010, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_XBGR2101010_FORMAT, SDL_PIXELFORMAT_XBGR2101010 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_2101010, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB2101010_FORMAT, SDL_PIXELFORMAT_ARGB2101010 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_2101010, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR2101010_FORMAT, SDL_PIXELFORMAT_ABGR2101010 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_2101010, 32, 4));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB48_FORMAT, SDL_PIXELFORMAT_RGB48 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_RGB, 0, 48, 6));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGR48_FORMAT, SDL_PIXELFORMAT_BGR48 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_BGR, 0, 48, 6));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA64_FORMAT, SDL_PIXELFORMAT_RGBA64 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_RGBA, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB64_FORMAT, SDL_PIXELFORMAT_ARGB64 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_ARGB, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA64_FORMAT, SDL_PIXELFORMAT_BGRA64 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_BGRA, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR64_FORMAT, SDL_PIXELFORMAT_ABGR64 == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU16, SDL_ARRAYORDER_ABGR, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB48_FLOAT_FORMAT, SDL_PIXELFORMAT_RGB48_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_RGB, 0, 48, 6));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGR48_FLOAT_FORMAT, SDL_PIXELFORMAT_BGR48_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_BGR, 0, 48, 6));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA64_FLOAT_FORMAT, SDL_PIXELFORMAT_RGBA64_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_RGBA, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB64_FLOAT_FORMAT, SDL_PIXELFORMAT_ARGB64_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_ARGB, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA64_FLOAT_FORMAT, SDL_PIXELFORMAT_BGRA64_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_BGRA, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR64_FLOAT_FORMAT, SDL_PIXELFORMAT_ABGR64_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF16, SDL_ARRAYORDER_ABGR, 0, 64, 8));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGB96_FLOAT_FORMAT, SDL_PIXELFORMAT_RGB96_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_RGB, 0, 96, 12));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGR96_FLOAT_FORMAT, SDL_PIXELFORMAT_BGR96_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_BGR, 0, 96, 12));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_RGBA128_FLOAT_FORMAT, SDL_PIXELFORMAT_RGBA128_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_RGBA, 0, 128, 16));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ARGB128_FLOAT_FORMAT, SDL_PIXELFORMAT_ARGB128_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_ARGB, 0, 128, 16));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_BGRA128_FLOAT_FORMAT, SDL_PIXELFORMAT_BGRA128_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_BGRA, 0, 128, 16));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_ABGR128_FLOAT_FORMAT, SDL_PIXELFORMAT_ABGR128_FLOAT == SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYF32, SDL_ARRAYORDER_ABGR, 0, 128, 16));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_YV12_FORMAT, SDL_PIXELFORMAT_YV12 == SDL_DEFINE_PIXELFOURCC('Y', 'V', '1', '2'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_IYUV_FORMAT, SDL_PIXELFORMAT_IYUV == SDL_DEFINE_PIXELFOURCC('I', 'Y', 'U', 'V'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_YUY2_FORMAT, SDL_PIXELFORMAT_YUY2 == SDL_DEFINE_PIXELFOURCC('Y', 'U', 'Y', '2'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_UYVY_FORMAT, SDL_PIXELFORMAT_UYVY == SDL_DEFINE_PIXELFOURCC('U', 'Y', 'V', 'Y'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_YVYU_FORMAT, SDL_PIXELFORMAT_YVYU == SDL_DEFINE_PIXELFOURCC('Y', 'V', 'Y', 'U'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_NV12_FORMAT, SDL_PIXELFORMAT_NV12 == SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_NV21_FORMAT, SDL_PIXELFORMAT_NV21 == SDL_DEFINE_PIXELFOURCC('N', 'V', '2', '1'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_P010_FORMAT, SDL_PIXELFORMAT_P010 == SDL_DEFINE_PIXELFOURCC('P', '0', '1', '0'));
SDL_COMPILE_TIME_ASSERT(SDL_PIXELFORMAT_EXTERNAL_OES_FORMAT, SDL_PIXELFORMAT_EXTERNAL_OES == SDL_DEFINE_PIXELFOURCC('O', 'E', 'S', ' '));

/* Verify the colorspaces are laid out as expected */
SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_SRGB_FORMAT, SDL_COLORSPACE_SRGB ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT709,
                                 SDL_TRANSFER_CHARACTERISTICS_SRGB,
                                 SDL_MATRIX_COEFFICIENTS_IDENTITY,
                                 SDL_CHROMA_LOCATION_NONE));
SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_SRGB_LINEAR_FORMAT, SDL_COLORSPACE_SRGB_LINEAR ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT709,
                                 SDL_TRANSFER_CHARACTERISTICS_LINEAR,
                                 SDL_MATRIX_COEFFICIENTS_IDENTITY,
                                 SDL_CHROMA_LOCATION_NONE));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_HDR10_FORMAT, SDL_COLORSPACE_HDR10 ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT2020,
                                 SDL_TRANSFER_CHARACTERISTICS_PQ,
                                 SDL_MATRIX_COEFFICIENTS_IDENTITY,
                                 SDL_CHROMA_LOCATION_NONE));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_JPEG_FORMAT, SDL_COLORSPACE_JPEG ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT709,
                                 SDL_TRANSFER_CHARACTERISTICS_BT601,
                                 SDL_MATRIX_COEFFICIENTS_BT601,
                                 SDL_CHROMA_LOCATION_NONE));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT601_LIMITED_FORMAT, SDL_COLORSPACE_BT601_LIMITED ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_LIMITED,
                                 SDL_COLOR_PRIMARIES_BT601,
                                 SDL_TRANSFER_CHARACTERISTICS_BT601,
                                 SDL_MATRIX_COEFFICIENTS_BT601,
                                 SDL_CHROMA_LOCATION_LEFT));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT601_FULL_FORMAT, SDL_COLORSPACE_BT601_FULL ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT601,
                                 SDL_TRANSFER_CHARACTERISTICS_BT601,
                                 SDL_MATRIX_COEFFICIENTS_BT601,
                                 SDL_CHROMA_LOCATION_LEFT));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT709_LIMITED_FORMAT, SDL_COLORSPACE_BT709_LIMITED ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_LIMITED,
                                 SDL_COLOR_PRIMARIES_BT709,
                                 SDL_TRANSFER_CHARACTERISTICS_BT709,
                                 SDL_MATRIX_COEFFICIENTS_BT709,
                                 SDL_CHROMA_LOCATION_LEFT));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT709_FULL_FORMAT, SDL_COLORSPACE_BT709_FULL ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT709,
                                 SDL_TRANSFER_CHARACTERISTICS_BT709,
                                 SDL_MATRIX_COEFFICIENTS_BT709,
                                 SDL_CHROMA_LOCATION_LEFT));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT2020_LIMITED_FORMAT, SDL_COLORSPACE_BT2020_LIMITED ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_LIMITED,
                                 SDL_COLOR_PRIMARIES_BT2020,
                                 SDL_TRANSFER_CHARACTERISTICS_PQ,
                                 SDL_MATRIX_COEFFICIENTS_BT2020_NCL,
                                 SDL_CHROMA_LOCATION_LEFT));

SDL_COMPILE_TIME_ASSERT(SDL_COLORSPACE_BT2020_FULL_FORMAT, SDL_COLORSPACE_BT2020_FULL ==
        SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_YCBCR,
                                 SDL_COLOR_RANGE_FULL,
                                 SDL_COLOR_PRIMARIES_BT2020,
                                 SDL_TRANSFER_CHARACTERISTICS_PQ,
                                 SDL_MATRIX_COEFFICIENTS_BT2020_NCL,
                                 SDL_CHROMA_LOCATION_LEFT));
/* Test case functions */

/**
 * Call to SDL_GetPixelFormatDetails
 */
static int SDLCALL pixels_getPixelFormatDetails(void *arg)
{
    const char *unknownFormat = "SDL_PIXELFORMAT_UNKNOWN";
    const char *expectedError = "Unknown pixel format";
    const char *error;
    int i;
    SDL_PixelFormat format;
    Uint32 masks;
    const SDL_PixelFormatDetails *result;

    /* Blank/unknown format */
    format = SDL_PIXELFORMAT_UNKNOWN;
    SDLTest_Log("Pixel Format: %s (%d)", unknownFormat, format);

    /* Allocate format */
    result = SDL_GetPixelFormatDetails(format);
    SDLTest_AssertPass("Call to SDL_GetPixelFormatDetails()");
    SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
    if (result != NULL) {
        SDLTest_AssertCheck(result->format == format, "Verify value of result.format; expected: %d, got %d", format, result->format);
        SDLTest_AssertCheck(result->bits_per_pixel == 0,
                            "Verify value of result.bits_per_pixel; expected: 0, got %u",
                            result->bits_per_pixel);
        SDLTest_AssertCheck(result->bytes_per_pixel == 0,
                            "Verify value of result.bytes_per_pixel; expected: 0, got %u",
                            result->bytes_per_pixel);
        masks = result->Rmask | result->Gmask | result->Bmask | result->Amask;
        SDLTest_AssertCheck(masks == 0, "Verify value of result.[RGBA]mask combined; expected: 0, got %" SDL_PRIu32, masks);
    }

    /* RGB formats */
    for (i = 0; i < g_numAllFormats; i++) {
        format = g_AllFormats[i];
        SDLTest_Log("Pixel Format: %s (%d)", g_AllFormatsVerbose[i], format);

        /* Allocate format */
        result = SDL_GetPixelFormatDetails(format);
        SDLTest_AssertPass("Call to SDL_GetPixelFormatDetails()");
        SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
        if (result != NULL) {
            SDLTest_AssertCheck(result->format == format, "Verify value of result.format; expected: %d, got %d", format, result->format);
            if (!SDL_ISPIXELFORMAT_FOURCC(format)) {
                SDLTest_AssertCheck(result->bits_per_pixel > 0,
                                    "Verify value of result.bits_per_pixel; expected: >0, got %u",
                                    result->bits_per_pixel);
                SDLTest_AssertCheck(result->bytes_per_pixel > 0,
                                    "Verify value of result.bytes_per_pixel; expected: >0, got %u",
                                    result->bytes_per_pixel);
                if (!SDL_ISPIXELFORMAT_INDEXED(format)) {
                    masks = result->Rmask | result->Gmask | result->Bmask | result->Amask;
                    SDLTest_AssertCheck(masks > 0, "Verify value of result.[RGBA]mask combined; expected: >0, got %" SDL_PRIu32, masks);
                    if (SDL_ISPIXELFORMAT_10BIT(format)) {
                        SDLTest_AssertCheck(result->Rbits == 10 && result->Gbits == 10 && result->Bbits == 10, "Verify value of result.[RGB]bits; expected: 10, got %d/%d/%d", result->Rbits, result->Gbits, result->Bbits);
                    } else if (SDL_BITSPERPIXEL(format) == 32) {
                        SDLTest_AssertCheck(result->Rbits == 8 && result->Gbits == 8 && result->Bbits == 8, "Verify value of result.[RGB]bits; expected: 8, got %d/%d/%d", result->Rbits, result->Gbits, result->Bbits);
                    }
                }
            }
        }
    }

    /* Negative cases */

    /* Invalid Formats */
    for (i = 0; i < g_numInvalidPixelFormats; i++) {
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
        format = g_invalidPixelFormats[i];
        result = SDL_GetPixelFormatDetails(format);
        SDLTest_AssertPass("Call to SDL_GetPixelFormatDetails(%d)", format);
        SDLTest_AssertCheck(result == NULL, "Verify result is NULL");
        error = SDL_GetError();
        SDLTest_AssertPass("Call to SDL_GetError()");
        SDLTest_AssertCheck(error != NULL, "Validate that error message was not NULL");
        if (error != NULL) {
            SDLTest_AssertCheck(SDL_strcmp(error, expectedError) == 0,
                                "Validate error message, expected: '%s', got: '%s'", expectedError, error);
        }
    }

    return TEST_COMPLETED;
}

/**
 * Call to SDL_GetPixelFormatName
 *
 * \sa SDL_GetPixelFormatName
 */
static int SDLCALL pixels_getPixelFormatName(void *arg)
{
    const char *unknownFormat = "SDL_PIXELFORMAT_UNKNOWN";
    const char *error;
    int i;
    SDL_PixelFormat format;
    const char *result;

    /* Blank/undefined format */
    format = SDL_PIXELFORMAT_UNKNOWN;
    SDLTest_Log("RGB Format: %s (%d)", unknownFormat, format);

    /* Get name of format */
    result = SDL_GetPixelFormatName(format);
    SDLTest_AssertPass("Call to SDL_GetPixelFormatName()");
    SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
    if (result != NULL) {
        SDLTest_AssertCheck(result[0] != '\0', "Verify result is non-empty");
        SDLTest_AssertCheck(SDL_strcmp(result, unknownFormat) == 0,
                            "Verify result text; expected: %s, got %s", unknownFormat, result);
    }

    /* RGB formats */
    for (i = 0; i < g_numAllFormats; i++) {
        format = g_AllFormats[i];
        SDLTest_Log("RGB Format: %s (%d)", g_AllFormatsVerbose[i], format);

        /* Get name of format */
        result = SDL_GetPixelFormatName(format);
        SDLTest_AssertPass("Call to SDL_GetPixelFormatName()");
        SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
        if (result != NULL) {
            SDLTest_AssertCheck(result[0] != '\0', "Verify result is non-empty");
            SDLTest_AssertCheck(SDL_strcmp(result, g_AllFormatsVerbose[i]) == 0,
                                "Verify result text; expected: %s, got %s", g_AllFormatsVerbose[i], result);
        }
    }

    /* Negative cases */

    /* Invalid Formats */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    for (i = 0; i < g_numInvalidPixelFormats; i++) {
        format = g_invalidPixelFormats[i];
        result = SDL_GetPixelFormatName(format);
        SDLTest_AssertPass("Call to SDL_GetPixelFormatName(%d)", format);
        SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
        if (result != NULL) {
            SDLTest_AssertCheck(result[0] != '\0',
                                "Verify result is non-empty; got: %s", result);
            SDLTest_AssertCheck(SDL_strcmp(result, g_invalidPixelFormatsVerbose[i]) == 0,
                                "Validate name is UNKNOWN, expected: '%s', got: '%s'", g_invalidPixelFormatsVerbose[i], result);
        }
        error = SDL_GetError();
        SDLTest_AssertPass("Call to SDL_GetError()");
        SDLTest_AssertCheck(error == NULL || error[0] == '\0', "Validate that error message is empty");
    }

    return TEST_COMPLETED;
}

/**
 * Call to SDL_CreatePalette and SDL_DestroyPalette
 *
 * \sa SDL_CreatePalette
 * \sa SDL_DestroyPalette
 */
static int SDLCALL pixels_allocFreePalette(void *arg)
{
    const char *expectedError = "Parameter 'ncolors' is invalid";
    const char *error;
    int variation;
    int i;
    int ncolors;
    SDL_Palette *result;

    /* Allocate palette */
    for (variation = 1; variation <= 3; variation++) {
        switch (variation) {
        /* Just one color */
        default:
        case 1:
            ncolors = 1;
            break;
        /* Two colors */
        case 2:
            ncolors = 2;
            break;
        /* More than two colors */
        case 3:
            ncolors = SDLTest_RandomIntegerInRange(8, 16);
            break;
        }

        result = SDL_CreatePalette(ncolors);
        SDLTest_AssertPass("Call to SDL_CreatePalette(%d)", ncolors);
        SDLTest_AssertCheck(result != NULL, "Verify result is not NULL");
        if (result != NULL) {
            SDLTest_AssertCheck(result->ncolors == ncolors, "Verify value of result.ncolors; expected: %u, got %u", ncolors, result->ncolors);
            if (result->ncolors > 0) {
                SDLTest_AssertCheck(result->colors != NULL, "Verify value of result.colors is not NULL");
                if (result->colors != NULL) {
                    for (i = 0; i < result->ncolors; i++) {
                        SDLTest_AssertCheck(result->colors[i].r == 255, "Verify value of result.colors[%d].r; expected: 255, got %u", i, result->colors[i].r);
                        SDLTest_AssertCheck(result->colors[i].g == 255, "Verify value of result.colors[%d].g; expected: 255, got %u", i, result->colors[i].g);
                        SDLTest_AssertCheck(result->colors[i].b == 255, "Verify value of result.colors[%d].b; expected: 255, got %u", i, result->colors[i].b);
                    }
                }
            }

            /* Deallocate again */
            SDL_DestroyPalette(result);
            SDLTest_AssertPass("Call to SDL_DestroyPalette()");
        }
    }

    /* Negative cases */

    /* Invalid number of colors */
    for (ncolors = 0; ncolors > -3; ncolors--) {
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
        result = SDL_CreatePalette(ncolors);
        SDLTest_AssertPass("Call to SDL_CreatePalette(%d)", ncolors);
        SDLTest_AssertCheck(result == NULL, "Verify result is NULL");
        error = SDL_GetError();
        SDLTest_AssertPass("Call to SDL_GetError()");
        SDLTest_AssertCheck(error != NULL, "Validate that error message was not NULL");
        if (error != NULL) {
            SDLTest_AssertCheck(SDL_strcmp(error, expectedError) == 0,
                                "Validate error message, expected: '%s', got: '%s'", expectedError, error);
        }
    }

    /* Invalid free pointer */
    SDL_ClearError();
    SDLTest_AssertPass("Call to SDL_ClearError()");
    SDL_DestroyPalette(NULL);
    SDLTest_AssertPass("Call to SDL_DestroyPalette(NULL)");
    error = SDL_GetError();
    SDLTest_AssertPass("Call to SDL_GetError()");
    SDLTest_AssertCheck(error == NULL || error[0] == '\0', "Validate that error message is empty");

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Pixels test cases */
static const SDLTest_TestCaseReference pixelsTest1 = {
    pixels_getPixelFormatDetails, "pixels_allocFreeFormat", "Call to SDL_GetPixelFormatDetails", TEST_ENABLED
};

static const SDLTest_TestCaseReference pixelsTest2 = {
    pixels_allocFreePalette, "pixels_allocFreePalette", "Call to SDL_CreatePalette and SDL_DestroyPalette", TEST_ENABLED
};

static const SDLTest_TestCaseReference pixelsTest3 = {
    pixels_getPixelFormatName, "pixels_getPixelFormatName", "Call to SDL_GetPixelFormatName", TEST_ENABLED
};

/* Sequence of Pixels test cases */
static const SDLTest_TestCaseReference *pixelsTests[] = {
    &pixelsTest1, &pixelsTest2, &pixelsTest3, NULL
};

/* Pixels test suite (global) */
SDLTest_TestSuiteReference pixelsTestSuite = {
    "Pixels",
    NULL,
    pixelsTests,
    NULL
};
