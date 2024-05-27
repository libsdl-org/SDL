/**
 * Pixels test suite
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"

/* Test case functions */

/* Definition of all RGB formats used to test pixel conversions */
static const SDL_PixelFormatEnum g_AllFormats[] = {
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

/* Test case functions */

/**
 * Call to SDL_CreatePixelFormat and SDL_DestroyPixelFormat
 *
 * \sa SDL_CreatePixelFormat
 * \sa SDL_DestroyPixelFormat
 */
static int pixels_allocFreeFormat(void *arg)
{
    const char *unknownFormat = "SDL_PIXELFORMAT_UNKNOWN";
    const char *expectedError = "Unknown pixel format";
    const char *error;
    int i;
    SDL_PixelFormatEnum format;
    Uint32 masks;
    SDL_PixelFormat *result;

    /* Blank/unknown format */
    format = SDL_PIXELFORMAT_UNKNOWN;
    SDLTest_Log("Pixel Format: %s (%d)", unknownFormat, format);

    /* Allocate format */
    result = SDL_CreatePixelFormat(format);
    SDLTest_AssertPass("Call to SDL_CreatePixelFormat()");
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

        /* Deallocate again */
        SDL_DestroyPixelFormat(result);
        SDLTest_AssertPass("Call to SDL_DestroyPixelFormat()");
    }

    /* RGB formats */
    for (i = 0; i < g_numAllFormats; i++) {
        format = g_AllFormats[i];
        SDLTest_Log("Pixel Format: %s (%d)", g_AllFormatsVerbose[i], format);

        /* Allocate format */
        result = SDL_CreatePixelFormat(format);
        SDLTest_AssertPass("Call to SDL_CreatePixelFormat()");
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
                }
            }

            /* Deallocate again */
            SDL_DestroyPixelFormat(result);
            SDLTest_AssertPass("Call to SDL_DestroyPixelFormat()");
        }
    }

    /* Negative cases */

    /* Invalid Formats */
    for (i = 0; i < g_numInvalidPixelFormats; i++) {
        SDL_ClearError();
        SDLTest_AssertPass("Call to SDL_ClearError()");
        format = g_invalidPixelFormats[i];
        result = SDL_CreatePixelFormat(format);
        SDLTest_AssertPass("Call to SDL_CreatePixelFormat(%d)", format);
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
    SDL_DestroyPixelFormat(NULL);
    SDLTest_AssertPass("Call to SDL_DestroyPixelFormat(NULL)");
    error = SDL_GetError();
    SDLTest_AssertPass("Call to SDL_GetError()");
    SDLTest_AssertCheck(error == NULL || error[0] == '\0', "Validate that error message is empty");

    return TEST_COMPLETED;
}

/**
 * Call to SDL_GetPixelFormatName
 *
 * \sa SDL_GetPixelFormatName
 */
static int pixels_getPixelFormatName(void *arg)
{
    const char *unknownFormat = "SDL_PIXELFORMAT_UNKNOWN";
    const char *error;
    int i;
    SDL_PixelFormatEnum format;
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
static int pixels_allocFreePalette(void *arg)
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
    (SDLTest_TestCaseFp)pixels_allocFreeFormat, "pixels_allocFreeFormat", "Call to SDL_CreatePixelFormat and SDL_DestroyPixelFormat", TEST_ENABLED
};

static const SDLTest_TestCaseReference pixelsTest2 = {
    (SDLTest_TestCaseFp)pixels_allocFreePalette, "pixels_allocFreePalette", "Call to SDL_CreatePalette and SDL_DestroyPalette", TEST_ENABLED
};

static const SDLTest_TestCaseReference pixelsTest3 = {
    (SDLTest_TestCaseFp)pixels_getPixelFormatName, "pixels_getPixelFormatName", "Call to SDL_GetPixelFormatName", TEST_ENABLED
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
