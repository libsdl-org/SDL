/**
 * Original code: automated SDL surface test written by Edgar Simo "bobbens"
 * Adapted/rewritten for test lib by Andreas Schiffler
 */

/* Suppress C4996 VS compiler warnings for unlink() */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE
#endif
#if defined(_MSC_VER) && !defined(_CRT_NONSTDC_NO_DEPRECATE)
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <sys/stat.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_suites.h"
#include "testautomation_images.h"


#define CHECK_FUNC(FUNC, PARAMS)    \
{                                   \
    int result = FUNC PARAMS;       \
    if (result != 0) {              \
        SDLTest_AssertCheck(result == 0, "Validate result from %s, expected: 0, got: %i, %s", #FUNC, result, SDL_GetError()); \
    }                               \
}

/* ================= Test Case Implementation ================== */

/* Shared test surface */

static SDL_Surface *referenceSurface = NULL;
static SDL_Surface *testSurface = NULL;

/* Fixture */

/* Create a 32-bit writable surface for blitting tests */
static void surfaceSetUp(void *arg)
{
    int result;
    SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
    SDL_BlendMode currentBlendMode;

    referenceSurface = SDLTest_ImageBlit(); /* For size info */
    testSurface = SDL_CreateSurface(referenceSurface->w, referenceSurface->h, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(testSurface != NULL, "Check that testSurface is not NULL");
    if (testSurface != NULL) {
        /* Disable blend mode for target surface */
        result = SDL_SetSurfaceBlendMode(testSurface, blendMode);
        SDLTest_AssertCheck(result == 0, "Validate result from SDL_SetSurfaceBlendMode, expected: 0, got: %i", result);
        result = SDL_GetSurfaceBlendMode(testSurface, &currentBlendMode);
        SDLTest_AssertCheck(result == 0, "Validate result from SDL_GetSurfaceBlendMode, expected: 0, got: %i", result);
        SDLTest_AssertCheck(currentBlendMode == blendMode, "Validate blendMode, expected: %" SDL_PRIu32 ", got: %" SDL_PRIu32, blendMode, currentBlendMode);
    }
}

static void surfaceTearDown(void *arg)
{
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;
    SDL_DestroySurface(testSurface);
    testSurface = NULL;
}

/**
 * Helper that clears the test surface
 */
static void clearTestSurface(void)
{
    int ret;
    Uint32 color;

    /* Clear surface. */
    color = SDL_MapSurfaceRGBA(testSurface, 0, 0, 0, 0);
    SDLTest_AssertPass("Call to SDL_MapSurfaceRGBA()");
    ret = SDL_FillSurfaceRect(testSurface, NULL, color);
    SDLTest_AssertPass("Call to SDL_FillSurfaceRect()");
    SDLTest_AssertCheck(ret == 0, "Verify result from SDL_FillSurfaceRect, expected: 0, got: %i", ret);
}

/**
 * Helper that blits in a specific blend mode, -1 for basic blitting, -2 for color mod, -3 for alpha mod, -4 for mixed blend modes.
 */
static void testBlitBlendMode(int mode)
{
    int ret;
    int i, j, ni, nj;
    SDL_Surface *face;
    SDL_Rect rect;
    int nmode;
    SDL_BlendMode bmode;
    int checkFailCount1;
    int checkFailCount2;
    int checkFailCount3;
    int checkFailCount4;

    /* Check test surface */
    SDLTest_AssertCheck(testSurface != NULL, "Verify testSurface is not NULL");
    if (testSurface == NULL) {
        return;
    }

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return;
    }

    /* Reset alpha modulation */
    ret = SDL_SetSurfaceAlphaMod(face, 255);
    SDLTest_AssertPass("Call to SDL_SetSurfaceAlphaMod()");
    SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceAlphaMod(), expected: 0, got: %i", ret);

    /* Reset color modulation */
    ret = SDL_SetSurfaceColorMod(face, 255, 255, 255);
    SDLTest_AssertPass("Call to SDL_SetSurfaceColorMod()");
    SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceColorMod(), expected: 0, got: %i", ret);

    /* Reset color key */
    ret = SDL_SetSurfaceColorKey(face, SDL_FALSE, 0);
    SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
    SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceColorKey(), expected: 0, got: %i", ret);

    /* Clear the test surface */
    clearTestSurface();

    /* Target rect size */
    rect.w = face->w;
    rect.h = face->h;

    /* Steps to take */
    ni = testSurface->w - face->w;
    nj = testSurface->h - face->h;

    /* Optionally set blend mode. */
    if (mode >= 0) {
        ret = SDL_SetSurfaceBlendMode(face, (SDL_BlendMode)mode);
        SDLTest_AssertPass("Call to SDL_SetSurfaceBlendMode()");
        SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceBlendMode(..., %i), expected: 0, got: %i", mode, ret);
    }

    /* Test blend mode. */
    checkFailCount1 = 0;
    checkFailCount2 = 0;
    checkFailCount3 = 0;
    checkFailCount4 = 0;
    for (j = 0; j <= nj; j += 4) {
        for (i = 0; i <= ni; i += 4) {
            if (mode == -2) {
                /* Set color mod. */
                ret = SDL_SetSurfaceColorMod(face, (Uint8)((255 / nj) * j), (Uint8)((255 / ni) * i), (Uint8)((255 / nj) * j));
                if (ret != 0) {
                    checkFailCount2++;
                }
            } else if (mode == -3) {
                /* Set alpha mod. */
                ret = SDL_SetSurfaceAlphaMod(face, (Uint8)((255 / ni) * i));
                if (ret != 0) {
                    checkFailCount3++;
                }
            } else if (mode == -4) {
                /* Crazy blending mode magic. */
                nmode = (i / 4 * j / 4) % 4;
                if (nmode == 0) {
                    bmode = SDL_BLENDMODE_NONE;
                } else if (nmode == 1) {
                    bmode = SDL_BLENDMODE_BLEND;
                } else if (nmode == 2) {
                    bmode = SDL_BLENDMODE_ADD;
                } else if (nmode == 3) {
                    bmode = SDL_BLENDMODE_MOD;
                } else {
                    /* Should be impossible, but some static checkers are too imprecise and will complain */
                    SDLTest_LogError("Invalid: nmode=%d", nmode);
                    return;
                }
                ret = SDL_SetSurfaceBlendMode(face, bmode);
                if (ret != 0) {
                    checkFailCount4++;
                }
            }

            /* Blitting. */
            rect.x = i;
            rect.y = j;
            ret = SDL_BlitSurface(face, NULL, testSurface, &rect);
            if (ret != 0) {
                checkFailCount1++;
            }
        }
    }
    SDLTest_AssertCheck(checkFailCount1 == 0, "Validate results from calls to SDL_BlitSurface, expected: 0, got: %i", checkFailCount1);
    SDLTest_AssertCheck(checkFailCount2 == 0, "Validate results from calls to SDL_SetSurfaceColorMod, expected: 0, got: %i", checkFailCount2);
    SDLTest_AssertCheck(checkFailCount3 == 0, "Validate results from calls to SDL_SetSurfaceAlphaMod, expected: 0, got: %i", checkFailCount3);
    SDLTest_AssertCheck(checkFailCount4 == 0, "Validate results from calls to SDL_SetSurfaceBlendMode, expected: 0, got: %i", checkFailCount4);

    /* Clean up */
    SDL_DestroySurface(face);
    face = NULL;
}

/* Helper to check that a file exists */
static void AssertFileExist(const char *filename)
{
    struct stat st;
    int ret = stat(filename, &st);

    SDLTest_AssertCheck(ret == 0, "Verify file '%s' exists", filename);
}

/* Test case functions */

/**
 * Tests sprite saving and loading
 */
static int surface_testSaveLoadBitmap(void *arg)
{
    int ret;
    const char *sampleFilename = "testSaveLoadBitmap.bmp";
    SDL_Surface *face;
    SDL_Surface *rface;

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Delete test file; ignore errors */
    unlink(sampleFilename);

    /* Save a surface */
    ret = SDL_SaveBMP(face, sampleFilename);
    SDLTest_AssertPass("Call to SDL_SaveBMP()");
    SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SaveBMP, expected: 0, got: %i", ret);
    AssertFileExist(sampleFilename);

    /* Load a surface */
    rface = SDL_LoadBMP(sampleFilename);
    SDLTest_AssertPass("Call to SDL_LoadBMP()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_LoadBMP is not NULL");
    if (rface != NULL) {
        SDLTest_AssertCheck(face->w == rface->w, "Verify width of loaded surface, expected: %i, got: %i", face->w, rface->w);
        SDLTest_AssertCheck(face->h == rface->h, "Verify height of loaded surface, expected: %i, got: %i", face->h, rface->h);
    }

    /* Delete test file; ignore errors */
    unlink(sampleFilename);

    /* Clean up */
    SDL_DestroySurface(face);
    face = NULL;
    SDL_DestroySurface(rface);
    rface = NULL;

    return TEST_COMPLETED;
}

/**
 *  Tests surface conversion.
 */
static int surface_testSurfaceConversion(void *arg)
{
    SDL_Surface *rface = NULL, *face = NULL;
    int ret = 0;

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Set transparent pixel as the pixel at (0,0) */
    if (SDL_GetSurfacePalette(face)) {
        ret = SDL_SetSurfaceColorKey(face, SDL_TRUE, *(Uint8 *)face->pixels);
        SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
        SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceColorKey, expected: 0, got: %i", ret);
    }

    /* Convert to 32 bit to compare. */
    rface = SDL_ConvertSurface(face, testSurface->format);
    SDLTest_AssertPass("Call to SDL_ConvertSurface()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_ConvertSurface is not NULL");

    /* Compare surface. */
    ret = SDLTest_CompareSurfaces(rface, face, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(face);
    face = NULL;
    SDL_DestroySurface(rface);
    rface = NULL;

    return TEST_COMPLETED;
}

/**
 *  Tests surface conversion across all pixel formats.
 */
static int surface_testCompleteSurfaceConversion(void *arg)
{
    Uint32 pixel_formats[] = {
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
#if 0 /* We aren't testing HDR10 colorspace conversion */
        SDL_PIXELFORMAT_XRGB2101010,
        SDL_PIXELFORMAT_XBGR2101010,
        SDL_PIXELFORMAT_ARGB2101010,
        SDL_PIXELFORMAT_ABGR2101010,
#endif
    };
    SDL_Surface *face = NULL, *cvt1, *cvt2, *final;
    const SDL_PixelFormatDetails *fmt1, *fmt2;
    int i, j, ret = 0;

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Set transparent pixel as the pixel at (0,0) */
    if (SDL_GetSurfacePalette(face)) {
        ret = SDL_SetSurfaceColorKey(face, SDL_TRUE, *(Uint8 *)face->pixels);
        SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
        SDLTest_AssertCheck(ret == 0, "Verify result from SDL_SetSurfaceColorKey, expected: 0, got: %i", ret);
    }

    for (i = 0; i < SDL_arraysize(pixel_formats); ++i) {
        for (j = 0; j < SDL_arraysize(pixel_formats); ++j) {
            fmt1 = SDL_GetPixelFormatDetails(pixel_formats[i]);
            SDLTest_AssertCheck(fmt1 != NULL, "SDL_GetPixelFormatDetails(%s[0x%08" SDL_PRIx32 "]) should return a non-null pixel format",
                                SDL_GetPixelFormatName(pixel_formats[i]), pixel_formats[i]);
            cvt1 = SDL_ConvertSurface(face, fmt1->format);
            SDLTest_AssertCheck(cvt1 != NULL, "SDL_ConvertSurface(..., %s[0x%08" SDL_PRIx32 "]) should return a non-null surface",
                                SDL_GetPixelFormatName(pixel_formats[i]), pixel_formats[i]);

            fmt2 = SDL_GetPixelFormatDetails(pixel_formats[j]);
            SDLTest_AssertCheck(fmt2 != NULL, "SDL_GetPixelFormatDetails(%s[0x%08" SDL_PRIx32 "]) should return a non-null pixel format",
                                SDL_GetPixelFormatName(pixel_formats[i]), pixel_formats[i]);
            cvt2 = SDL_ConvertSurface(cvt1, fmt2->format);
            SDLTest_AssertCheck(cvt2 != NULL, "SDL_ConvertSurface(..., %s[0x%08" SDL_PRIx32 "]) should return a non-null surface",
                                SDL_GetPixelFormatName(pixel_formats[i]), pixel_formats[i]);

            if (fmt1 && fmt2 &&
                fmt1->bytes_per_pixel == SDL_BYTESPERPIXEL(face->format) &&
                fmt2->bytes_per_pixel == SDL_BYTESPERPIXEL(face->format) &&
                SDL_ISPIXELFORMAT_ALPHA(fmt1->format) == SDL_ISPIXELFORMAT_ALPHA(face->format) &&
                SDL_ISPIXELFORMAT_ALPHA(fmt2->format) == SDL_ISPIXELFORMAT_ALPHA(face->format)) {
                final = SDL_ConvertSurface(cvt2, face->format);
                SDL_assert(final != NULL);

                /* Compare surface. */
                ret = SDLTest_CompareSurfaces(face, final, 0);
                SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
                SDL_DestroySurface(final);
            }

            SDL_DestroySurface(cvt1);
            SDL_DestroySurface(cvt2);
        }
    }

    /* Clean up. */
    SDL_DestroySurface(face);

    return TEST_COMPLETED;
}

/**
 * Tests sprite loading. A failure case.
 */
static int surface_testLoadFailure(void *arg)
{
    SDL_Surface *face = SDL_LoadBMP("nonexistant.bmp");
    SDLTest_AssertCheck(face == NULL, "SDL_CreateLoadBmp");

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines.
 */
static int surface_testBlit(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Basic blitting */
    testBlitBlendMode(-1);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlit();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines with color mod
 */
static int surface_testBlitColorMod(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Basic blitting with color mod */
    testBlitBlendMode(-2);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitColor();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines with alpha mod
 */
static int surface_testBlitAlphaMod(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Basic blitting with alpha mod */
    testBlitBlendMode(-3);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitAlpha();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int surface_testBlitBlendNone(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Basic blitting */
    testBlitBlendMode(SDL_BLENDMODE_NONE);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitBlendNone();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int surface_testBlitBlendBlend(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Blend blitting */
    testBlitBlendMode(SDL_BLENDMODE_BLEND);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitBlend();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int surface_testBlitBlendAdd(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Add blitting */
    testBlitBlendMode(SDL_BLENDMODE_ADD);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitBlendAdd();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int surface_testBlitBlendMod(void *arg)
{
    int ret;
    SDL_Surface *compareSurface;

    /* Mod blitting */
    testBlitBlendMode(SDL_BLENDMODE_MOD);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitBlendMod();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines with loop
 */
static int surface_testBlitBlendLoop(void *arg)
{

    int ret;
    SDL_Surface *compareSurface;

    /* All blitting modes */
    testBlitBlendMode(-4);

    /* Verify result by comparing surfaces */
    compareSurface = SDLTest_ImageBlitBlendAll();
    ret = SDLTest_CompareSurfaces(testSurface, compareSurface, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(compareSurface);

    return TEST_COMPLETED;
}

static int surface_testOverflow(void *arg)
{
    char buf[1024];
    const char *expectedError;
    SDL_Surface *surface;

    SDL_memset(buf, '\0', sizeof(buf));

    expectedError = "Parameter 'width' is invalid";
    surface = SDL_CreateSurface(-3, 100, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative width");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(-1, 1, SDL_PIXELFORMAT_INDEX8, buf, 4);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative width");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(-1, 1, SDL_PIXELFORMAT_RGBA8888, buf, 4);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative width");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    expectedError = "Parameter 'height' is invalid";
    surface = SDL_CreateSurface(100, -3, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative height");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(1, -1, SDL_PIXELFORMAT_INDEX8, buf, 4);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative height");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(1, -1, SDL_PIXELFORMAT_RGBA8888, buf, 4);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative height");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    expectedError = "Parameter 'pitch' is invalid";
    surface = SDL_CreateSurfaceFrom(4, 1, SDL_PIXELFORMAT_INDEX8, buf, -1);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative pitch");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA8888, buf, -1);
    SDLTest_AssertCheck(surface == NULL, "Should detect negative pitch");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA8888, buf, 0);
    SDLTest_AssertCheck(surface == NULL, "Should detect zero pitch");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(1, 1, SDL_PIXELFORMAT_RGBA8888, NULL, 0);
    SDLTest_AssertCheck(surface != NULL, "Allow zero pitch for partially set up surfaces: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    /* Less than 1 byte per pixel: the pitch can legitimately be less than
     * the width, but it must be enough to hold the appropriate number of
     * bits per pixel. SDL_PIXELFORMAT_INDEX4* needs 1 byte per 2 pixels. */
    surface = SDL_CreateSurfaceFrom(6, 1, SDL_PIXELFORMAT_INDEX4LSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "6px * 4 bits per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(6, 1, SDL_PIXELFORMAT_INDEX4MSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "6px * 4 bits per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(7, 1, SDL_PIXELFORMAT_INDEX4LSB, buf, 3);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(7, 1, SDL_PIXELFORMAT_INDEX4MSB, buf, 3);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    surface = SDL_CreateSurfaceFrom(7, 1, SDL_PIXELFORMAT_INDEX4LSB, buf, 4);
    SDLTest_AssertCheck(surface != NULL, "7px * 4 bits per px fits in 4 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(7, 1, SDL_PIXELFORMAT_INDEX4MSB, buf, 4);
    SDLTest_AssertCheck(surface != NULL, "7px * 4 bits per px fits in 4 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    /* SDL_PIXELFORMAT_INDEX2* needs 1 byte per 4 pixels. */
    surface = SDL_CreateSurfaceFrom(12, 1, SDL_PIXELFORMAT_INDEX2LSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "12px * 2 bits per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(12, 1, SDL_PIXELFORMAT_INDEX2MSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "12px * 2 bits per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(13, 1, SDL_PIXELFORMAT_INDEX2LSB, buf, 3);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp (%d)", surface ? surface->pitch : 0);
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(13, 1, SDL_PIXELFORMAT_INDEX2MSB, buf, 3);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    surface = SDL_CreateSurfaceFrom(13, 1, SDL_PIXELFORMAT_INDEX2LSB, buf, 4);
    SDLTest_AssertCheck(surface != NULL, "13px * 2 bits per px fits in 4 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(13, 1, SDL_PIXELFORMAT_INDEX2MSB, buf, 4);
    SDLTest_AssertCheck(surface != NULL, "13px * 2 bits per px fits in 4 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    /* SDL_PIXELFORMAT_INDEX1* needs 1 byte per 8 pixels. */
    surface = SDL_CreateSurfaceFrom(16, 1, SDL_PIXELFORMAT_INDEX1LSB, buf, 2);
    SDLTest_AssertCheck(surface != NULL, "16px * 1 bit per px fits in 2 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(16, 1, SDL_PIXELFORMAT_INDEX1MSB, buf, 2);
    SDLTest_AssertCheck(surface != NULL, "16px * 1 bit per px fits in 2 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(17, 1, SDL_PIXELFORMAT_INDEX1LSB, buf, 2);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(17, 1, SDL_PIXELFORMAT_INDEX1MSB, buf, 2);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    surface = SDL_CreateSurfaceFrom(17, 1, SDL_PIXELFORMAT_INDEX1LSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "17px * 1 bit per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(17, 1, SDL_PIXELFORMAT_INDEX1MSB, buf, 3);
    SDLTest_AssertCheck(surface != NULL, "17px * 1 bit per px fits in 3 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    /* SDL_PIXELFORMAT_INDEX8 and SDL_PIXELFORMAT_RGB332 require 1 byte per pixel. */
    surface = SDL_CreateSurfaceFrom(5, 1, SDL_PIXELFORMAT_RGB332, buf, 5);
    SDLTest_AssertCheck(surface != NULL, "5px * 8 bits per px fits in 5 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(5, 1, SDL_PIXELFORMAT_INDEX8, buf, 5);
    SDLTest_AssertCheck(surface != NULL, "5px * 8 bits per px fits in 5 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(6, 1, SDL_PIXELFORMAT_RGB332, buf, 5);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(6, 1, SDL_PIXELFORMAT_INDEX8, buf, 5);
    SDLTest_AssertCheck(surface == NULL, "Should detect pitch < width * bpp");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    /* Everything else requires more than 1 byte per pixel, and rounds up
     * each pixel to an integer number of bytes (e.g. RGB555 is really
     * XRGB1555, with 1 bit per pixel wasted). */
    surface = SDL_CreateSurfaceFrom(3, 1, SDL_PIXELFORMAT_XRGB1555, buf, 6);
    SDLTest_AssertCheck(surface != NULL, "3px * 15 (really 16) bits per px fits in 6 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurfaceFrom(3, 1, SDL_PIXELFORMAT_XRGB1555, buf, 6);
    SDLTest_AssertCheck(surface != NULL, "5px * 15 (really 16) bits per px fits in 6 bytes: %s",
                        surface != NULL ? "(success)" : SDL_GetError());
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(4, 1, SDL_PIXELFORMAT_XRGB1555, buf, 6);
    SDLTest_AssertCheck(surface == NULL, "4px * 15 (really 16) bits per px doesn't fit in 6 bytes");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    surface = SDL_CreateSurfaceFrom(4, 1, SDL_PIXELFORMAT_XRGB1555, buf, 6);
    SDLTest_AssertCheck(surface == NULL, "4px * 15 (really 16) bits per px doesn't fit in 6 bytes");
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    if (sizeof(size_t) == 4 && sizeof(int) >= 4) {
        SDL_ClearError();
        expectedError = "aligning pitch would overflow";
        /* 0x5555'5555 * 3bpp = 0xffff'ffff which fits in size_t, but adding
         * alignment padding makes it overflow */
        surface = SDL_CreateSurface(0x55555555, 1, SDL_PIXELFORMAT_RGB24);
        SDLTest_AssertCheck(surface == NULL, "Should detect overflow in pitch + alignment");
        SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                            "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
        SDL_ClearError();
        expectedError = "width * bpp would overflow";
        /* 0x4000'0000 * 4bpp = 0x1'0000'0000 which (just) overflows */
        surface = SDL_CreateSurface(0x40000000, 1, SDL_PIXELFORMAT_ARGB8888);
        SDLTest_AssertCheck(surface == NULL, "Should detect overflow in width * bytes per pixel");
        SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                            "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
        SDL_ClearError();
        expectedError = "height * pitch would overflow";
        surface = SDL_CreateSurface((1 << 29) - 1, (1 << 29) - 1, SDL_PIXELFORMAT_INDEX8);
        SDLTest_AssertCheck(surface == NULL, "Should detect overflow in width * height");
        SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                            "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
        SDL_ClearError();
        expectedError = "height * pitch would overflow";
        surface = SDL_CreateSurface((1 << 15) + 1, (1 << 15) + 1, SDL_PIXELFORMAT_ARGB8888);
        SDLTest_AssertCheck(surface == NULL, "Should detect overflow in width * height * bytes per pixel");
        SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                            "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());
    } else {
        SDLTest_Log("Can't easily overflow size_t on this platform");
    }

    return TEST_COMPLETED;
}

static int surface_testFlip(void *arg)
{
    SDL_Surface *surface;
    Uint8 *pixels;
    int offset;
    const char *expectedError;

    surface = SDL_CreateSurface(3, 3, SDL_PIXELFORMAT_RGB24);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");

    SDL_ClearError();
    expectedError = "Parameter 'surface' is invalid";
    SDL_FlipSurface(NULL, SDL_FLIP_HORIZONTAL);
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    SDL_ClearError();
    expectedError = "Parameter 'flip' is invalid";
    SDL_FlipSurface(surface, SDL_FLIP_NONE);
    SDLTest_AssertCheck(SDL_strcmp(SDL_GetError(), expectedError) == 0,
                        "Expected \"%s\", got \"%s\"", expectedError, SDL_GetError());

    pixels = (Uint8 *)surface->pixels;
    *pixels = 0xFF;
    offset = 0;

    SDLTest_AssertPass("Call to SDL_FlipSurface(surface, SDL_FLIP_VERTICAL)");
    CHECK_FUNC(SDL_FlipSurface, (surface, SDL_FLIP_VERTICAL));
    SDLTest_AssertCheck(pixels[offset] == 0x00,
                        "Expected pixels[%d] == 0x00 got 0x%.2X", offset, pixels[offset]);
    offset = 2 * surface->pitch;
    SDLTest_AssertCheck(pixels[offset] == 0xFF,
                        "Expected pixels[%d] == 0xFF got 0x%.2X", offset, pixels[offset]);

    SDLTest_AssertPass("Call to SDL_FlipSurface(surface, SDL_FLIP_HORIZONTAL)");
    CHECK_FUNC(SDL_FlipSurface, (surface, SDL_FLIP_HORIZONTAL));
    SDLTest_AssertCheck(pixels[offset] == 0x00,
                        "Expected pixels[%d] == 0x00 got 0x%.2X", offset, pixels[offset]);
    offset += (surface->w - 1) * SDL_BYTESPERPIXEL(surface->format);
    SDLTest_AssertCheck(pixels[offset] == 0xFF,
                        "Expected pixels[%d] == 0xFF got 0x%.2X", offset, pixels[offset]);

    SDL_DestroySurface(surface);

    return TEST_COMPLETED;
}

static int surface_testPalette(void *arg)
{
    SDL_Surface *source, *surface, *output;
    SDL_Palette *palette;
    Uint8 *pixels;

    palette = SDL_CreatePalette(2);
    SDLTest_AssertCheck(palette != NULL, "SDL_CreatePalette()");

    source = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(source != NULL, "SDL_CreateSurface()");

    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");

    pixels = (Uint8 *)surface->pixels;
    SDLTest_AssertCheck(*pixels == 0, "Expected *pixels == 0 got %u", *pixels);

    /* Identity copy between indexed surfaces without a palette */
    *(Uint8 *)source->pixels = 1;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Identity copy between indexed surfaces where the destination has a palette */
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    palette->colors[1].r = 0xFF;
    palette->colors[1].g = 0;
    palette->colors[1].b = 0;
    SDL_SetSurfacePalette(surface, palette);
    *pixels = 0;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    output = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(output != NULL, "SDL_CreateSurface()");

    pixels = (Uint8 *)output->pixels;
    SDL_BlitSurface(surface, NULL, output, NULL);
    SDLTest_AssertCheck(*pixels == 0xFF, "Expected *pixels == 0xFF got 0x%.2X", *pixels);

    /* Set the palette color and blit again */
    palette->colors[1].r = 0xAA;
    SDL_SetSurfacePalette(surface, palette);
    SDL_BlitSurface(surface, NULL, output, NULL);
    SDLTest_AssertCheck(*pixels == 0xAA, "Expected *pixels == 0xAA got 0x%.2X", *pixels);

    SDL_DestroyPalette(palette);
    SDL_DestroySurface(source);
    SDL_DestroySurface(surface);
    SDL_DestroySurface(output);

    return TEST_COMPLETED;
}


/* ================= Test References ================== */

/* Surface test cases */
static const SDLTest_TestCaseReference surfaceTest1 = {
    (SDLTest_TestCaseFp)surface_testSaveLoadBitmap, "surface_testSaveLoadBitmap", "Tests sprite saving and loading.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest2 = {
    (SDLTest_TestCaseFp)surface_testBlit, "surface_testBlit", "Tests basic blitting.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest3 = {
    (SDLTest_TestCaseFp)surface_testBlitBlendNone, "surface_testBlitBlendNone", "Tests blitting routines with none blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest4 = {
    (SDLTest_TestCaseFp)surface_testLoadFailure, "surface_testLoadFailure", "Tests sprite loading. A failure case.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest5 = {
    (SDLTest_TestCaseFp)surface_testSurfaceConversion, "surface_testSurfaceConversion", "Tests surface conversion.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest6 = {
    (SDLTest_TestCaseFp)surface_testCompleteSurfaceConversion, "surface_testCompleteSurfaceConversion", "Tests surface conversion across all pixel formats", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest7 = {
    (SDLTest_TestCaseFp)surface_testBlitColorMod, "surface_testBlitColorMod", "Tests some blitting routines with color mod.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest8 = {
    (SDLTest_TestCaseFp)surface_testBlitAlphaMod, "surface_testBlitAlphaMod", "Tests some blitting routines with alpha mod.", TEST_ENABLED
};

/* TODO: rewrite test case, define new test data and re-enable; current implementation fails */
static const SDLTest_TestCaseReference surfaceTest9 = {
    (SDLTest_TestCaseFp)surface_testBlitBlendLoop, "surface_testBlitBlendLoop", "Test blitting routines with various blending modes", TEST_DISABLED
};

/* TODO: rewrite test case, define new test data and re-enable; current implementation fails */
static const SDLTest_TestCaseReference surfaceTest10 = {
    (SDLTest_TestCaseFp)surface_testBlitBlendBlend, "surface_testBlitBlendBlend", "Tests blitting routines with blend blending mode.", TEST_DISABLED
};

/* TODO: rewrite test case, define new test data and re-enable; current implementation fails */
static const SDLTest_TestCaseReference surfaceTest11 = {
    (SDLTest_TestCaseFp)surface_testBlitBlendAdd, "surface_testBlitBlendAdd", "Tests blitting routines with add blending mode.", TEST_DISABLED
};

static const SDLTest_TestCaseReference surfaceTest12 = {
    (SDLTest_TestCaseFp)surface_testBlitBlendMod, "surface_testBlitBlendMod", "Tests blitting routines with mod blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestOverflow = {
    surface_testOverflow, "surface_testOverflow", "Test overflow detection.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestFlip = {
    surface_testFlip, "surface_testFlip", "Test surface flipping.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestPalette = {
    surface_testPalette, "surface_testPalette", "Test surface palette operations.", TEST_ENABLED
};

/* Sequence of Surface test cases */
static const SDLTest_TestCaseReference *surfaceTests[] = {
    &surfaceTest1, &surfaceTest2, &surfaceTest3, &surfaceTest4, &surfaceTest5,
    &surfaceTest6, &surfaceTest7, &surfaceTest8, &surfaceTest9, &surfaceTest10,
    &surfaceTest11, &surfaceTest12, &surfaceTestOverflow, &surfaceTestFlip,
    &surfaceTestPalette, NULL
};

/* Surface test suite (global) */
SDLTest_TestSuiteReference surfaceTestSuite = {
    "Surface",
    surfaceSetUp,
    surfaceTests,
    surfaceTearDown

};
