/**
 * Original code: automated SDL surface test written by Edgar Simo "bobbens"
 * Adapted/rewritten for test lib by Andreas Schiffler
 */

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
    bool result = FUNC PARAMS;  \
    if (!result) {                  \
        SDLTest_AssertCheck(result, "Validate result from %s, expected: true, got: false, %s", #FUNC, SDL_GetError()); \
    }                               \
}

/* ================= Test Case Implementation ================== */

/* Shared test surface */

static SDL_Surface *referenceSurface = NULL;
static SDL_Surface *testSurface = NULL;

/* Fixture */

/* Create a 32-bit writable surface for blitting tests */
static void SDLCALL surfaceSetUp(void **arg)
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
        SDLTest_AssertCheck(result == true, "Validate result from SDL_SetSurfaceBlendMode, expected: true, got: %i", result);
        result = SDL_GetSurfaceBlendMode(testSurface, &currentBlendMode);
        SDLTest_AssertCheck(result == true, "Validate result from SDL_GetSurfaceBlendMode, expected: true, got: %i", result);
        SDLTest_AssertCheck(currentBlendMode == blendMode, "Validate blendMode, expected: %" SDL_PRIu32 ", got: %" SDL_PRIu32, blendMode, currentBlendMode);

        /* Clear the target surface */
        result = SDL_FillSurfaceRect(testSurface, NULL, SDL_MapSurfaceRGBA(testSurface, 0, 0, 0, 255));
        SDLTest_AssertCheck(result == true, "Validate result from SDL_FillSurfaceRect, expected: true, got: %i", result);
    }
}

static void SDLCALL surfaceTearDown(void *arg)
{
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;
    SDL_DestroySurface(testSurface);
    testSurface = NULL;
}

static void DitherPalette(SDL_Palette *palette)
{
    int i;

    for (i = 0; i < palette->ncolors; i++) {
        int r, g, b;
        /* map each bit field to the full [0, 255] interval,
           so 0 is mapped to (0, 0, 0) and 255 to (255, 255, 255) */
        r = i & 0xe0;
        r |= r >> 3 | r >> 6;
        palette->colors[i].r = (Uint8)r;
        g = (i << 3) & 0xe0;
        g |= g >> 3 | g >> 6;
        palette->colors[i].g = (Uint8)g;
        b = i & 0x3;
        b |= b << 2;
        b |= b << 4;
        palette->colors[i].b = (Uint8)b;
        palette->colors[i].a = SDL_ALPHA_OPAQUE;
    }
}

/**
 * Helper that blits in a specific blend mode, -1 for color mod, -2 for alpha mod
 */
static void testBlitBlendModeWithFormats(int mode, SDL_PixelFormat src_format, SDL_PixelFormat dst_format)
{
    /* Allow up to 1 delta from theoretical value to account for rounding error */
    const int MAXIMUM_ERROR = 1;
    int ret;
    SDL_Surface *src;
    SDL_Surface *dst;
    Uint32 color;
    Uint8 srcR = 10, srcG = 128, srcB = 240, srcA = 100;
    Uint8 dstR = 128, dstG = 128, dstB = 128, dstA = 128;
    Uint8 expectedR, expectedG, expectedB, expectedA;
    Uint8 actualR, actualG, actualB, actualA;
    int deltaR, deltaG, deltaB, deltaA;

    /* Create dst surface */
    dst = SDL_CreateSurface(9, 1, dst_format);
    SDLTest_AssertCheck(dst != NULL, "Verify dst surface is not NULL");
    if (dst == NULL) {
        return;
    }

    /* Clear surface. */
    if (SDL_ISPIXELFORMAT_INDEXED(dst_format)) {
        SDL_Palette *palette = SDL_CreateSurfacePalette(dst);
        DitherPalette(palette);
        palette->colors[0].r = dstR;
        palette->colors[0].g = dstG;
        palette->colors[0].b = dstB;
        palette->colors[0].a = dstA;
        color = 0;
    } else {
        color = SDL_MapSurfaceRGBA(dst, dstR, dstG, dstB, dstA);
        SDLTest_AssertPass("Call to SDL_MapSurfaceRGBA()");
    }
    ret = SDL_FillSurfaceRect(dst, NULL, color);
    SDLTest_AssertPass("Call to SDL_FillSurfaceRect()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_FillSurfaceRect, expected: true, got: %i", ret);
    SDL_GetRGBA(color, SDL_GetPixelFormatDetails(dst->format), SDL_GetSurfacePalette(dst), &dstR, &dstG, &dstB, &dstA);

    /* Create src surface */
    src = SDL_CreateSurface(9, 1, src_format);
    SDLTest_AssertCheck(src != NULL, "Verify src surface is not NULL");
    if (src == NULL) {
        return;
    }
    if (SDL_ISPIXELFORMAT_INDEXED(src_format)) {
        SDL_Palette *palette = SDL_CreateSurfacePalette(src);
        palette->colors[0].r = srcR;
        palette->colors[0].g = srcG;
        palette->colors[0].b = srcB;
        palette->colors[0].a = srcA;
    }

    /* Reset alpha modulation */
    ret = SDL_SetSurfaceAlphaMod(src, 255);
    SDLTest_AssertPass("Call to SDL_SetSurfaceAlphaMod()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceAlphaMod(), expected: true, got: %i", ret);

    /* Reset color modulation */
    ret = SDL_SetSurfaceColorMod(src, 255, 255, 255);
    SDLTest_AssertPass("Call to SDL_SetSurfaceColorMod()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceColorMod(), expected: true, got: %i", ret);

    /* Reset color key */
    ret = SDL_SetSurfaceColorKey(src, false, 0);
    SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceColorKey(), expected: true, got: %i", ret);

    /* Clear surface. */
    color = SDL_MapSurfaceRGBA(src, srcR, srcG, srcB, srcA);
    SDLTest_AssertPass("Call to SDL_MapSurfaceRGBA()");
    ret = SDL_FillSurfaceRect(src, NULL, color);
    SDLTest_AssertPass("Call to SDL_FillSurfaceRect()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_FillSurfaceRect, expected: true, got: %i", ret);
    SDL_GetRGBA(color, SDL_GetPixelFormatDetails(src->format), SDL_GetSurfacePalette(src), &srcR, &srcG, &srcB, &srcA);

    /* Set blend mode. */
    if (mode >= 0) {
        ret = SDL_SetSurfaceBlendMode(src, (SDL_BlendMode)mode);
        SDLTest_AssertPass("Call to SDL_SetSurfaceBlendMode()");
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceBlendMode(..., %i), expected: true, got: %i", mode, ret);
    } else {
        ret = SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
        SDLTest_AssertPass("Call to SDL_SetSurfaceBlendMode()");
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceBlendMode(..., %i), expected: true, got: %i", mode, ret);
    }

    /* Test blend mode. */
#define FLOAT(X)    ((float)X / 255.0f)
    switch (mode) {
    case -1:
        /* Set color mod. */
        ret = SDL_SetSurfaceColorMod(src, srcR, srcG, srcB);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_SetSurfaceColorMod, expected: true, got: %i", ret);
        expectedR = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcR) * FLOAT(srcR)) * FLOAT(srcA) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcG) * FLOAT(srcG)) * FLOAT(srcA) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcB) * FLOAT(srcB)) * FLOAT(srcA) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case -2:
        /* Set alpha mod. */
        ret = SDL_SetSurfaceAlphaMod(src, srcA);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_SetSurfaceAlphaMod, expected: true, got: %i", ret);
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstR) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstG) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstB) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstA) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_NONE:
        expectedR = srcR;
        expectedG = srcG;
        expectedB = srcB;
        expectedA = SDL_ISPIXELFORMAT_ALPHA(dst_format) ? srcA : 255;
        break;
    case SDL_BLENDMODE_BLEND:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(srcA) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(srcA) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(srcA) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_BLEND_PREMULTIPLIED:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_ADD:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(srcA) + FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(srcA) + FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(srcA) + FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_ADD_PREMULTIPLIED:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) + FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) + FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) + FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_MOD:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_MUL:
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(dstR) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(dstG) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(dstB) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    default:
        SDLTest_LogError("Invalid blending mode: %d", mode);
        return;
    }

    if (SDL_ISPIXELFORMAT_INDEXED(dst_format)) {
        SDL_Palette *palette = SDL_GetSurfacePalette(dst);
        palette->colors[1].r = expectedR;
        palette->colors[1].g = expectedG;
        palette->colors[1].b = expectedB;
        palette->colors[1].a = expectedA;
    }

    /* Blitting. */
    ret = SDL_BlitSurface(src, NULL, dst, NULL);
    SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_BlitSurface, expected: true, got: %i: %s", ret, !ret ? SDL_GetError() : "success");
    if (ret) {
        SDL_ReadSurfacePixel(dst, 0, 0, &actualR, &actualG, &actualB, &actualA);
        deltaR = SDL_abs((int)actualR - expectedR);
        deltaG = SDL_abs((int)actualG - expectedG);
        deltaB = SDL_abs((int)actualB - expectedB);
        deltaA = SDL_abs((int)actualA - expectedA);
        SDLTest_AssertCheck(
            deltaR <= MAXIMUM_ERROR &&
            deltaG <= MAXIMUM_ERROR &&
            deltaB <= MAXIMUM_ERROR &&
            deltaA <= MAXIMUM_ERROR,
            "Checking %s -> %s blit results, expected %d,%d,%d,%d, got %d,%d,%d,%d",
            SDL_GetPixelFormatName(src_format),
            SDL_GetPixelFormatName(dst_format),
            expectedR, expectedG, expectedB, expectedA, actualR, actualG, actualB, actualA);
    }

    /* Clean up */
    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);
}

static void testBlitBlendMode(int mode)
{
    const SDL_PixelFormat src_formats[] = {
        SDL_PIXELFORMAT_INDEX8, SDL_PIXELFORMAT_XRGB8888, SDL_PIXELFORMAT_ARGB8888
    };
    const SDL_PixelFormat dst_formats[] = {
        SDL_PIXELFORMAT_XRGB8888, SDL_PIXELFORMAT_ARGB8888
    };
    int i, j;

    for (i = 0; i < SDL_arraysize(src_formats); ++i) {
        for (j = 0; j < SDL_arraysize(dst_formats); ++j) {
            testBlitBlendModeWithFormats(mode, src_formats[i], dst_formats[j]);
        }
    }
}

/* Helper to check that a file exists */
static void AssertFileExist(const char *filename)
{
    SDLTest_AssertCheck(SDL_GetPathInfo(filename, NULL), "Verify file '%s' exists", filename);
}

/* Test case functions */

/**
 * Tests creating surface with invalid format
 */
static int SDLCALL surface_testInvalidFormat(void *arg)
{
    SDL_Surface *surface;

    surface = SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_UNKNOWN);
    SDLTest_AssertCheck(surface == NULL, "Verify SDL_CreateSurface(SDL_PIXELFORMAT_UNKNOWN) returned NULL");
    SDL_DestroySurface(surface);

    surface = SDL_CreateSurfaceFrom(32, 32, SDL_PIXELFORMAT_UNKNOWN, NULL, 0);
    SDLTest_AssertCheck(surface == NULL, "Verify SDL_CreateSurfaceFrom(SDL_PIXELFORMAT_UNKNOWN) returned NULL");
    SDL_DestroySurface(surface);

    return TEST_COMPLETED;
}

/**
 * Tests sprite saving and loading
 */
static int SDLCALL surface_testSaveLoad(void *arg)
{
    int ret;
    const char *sampleFilename = "testSaveLoad.tmp";
    SDL_Surface *face;
    SDL_Surface *rface;
    SDL_Palette *palette;
    SDL_Color colors[] = {
        { 255, 0, 0, SDL_ALPHA_OPAQUE },    /* Red */
        { 0, 255, 0, SDL_ALPHA_OPAQUE }     /* Green */
    };
    SDL_IOStream *stream;
    Uint8 r, g, b, a;

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Delete test file; ignore errors */
    SDL_RemovePath(sampleFilename);

    /* Save a BMP surface */
    ret = SDL_SaveBMP(face, sampleFilename);
    SDLTest_AssertPass("Call to SDL_SaveBMP()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SaveBMP, expected: true, got: %i", ret);
    AssertFileExist(sampleFilename);

    /* Load a BMP surface */
    rface = SDL_LoadBMP(sampleFilename);
    SDLTest_AssertPass("Call to SDL_LoadBMP()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_LoadBMP is not NULL");
    if (rface != NULL) {
        SDLTest_AssertCheck(face->w == rface->w, "Verify width of loaded surface, expected: %i, got: %i", face->w, rface->w);
        SDLTest_AssertCheck(face->h == rface->h, "Verify height of loaded surface, expected: %i, got: %i", face->h, rface->h);
        SDL_DestroySurface(rface);
        rface = NULL;
    }

    /* Delete test file; ignore errors */
    SDL_RemovePath(sampleFilename);

    /* Save a PNG surface */
    ret = SDL_SavePNG(face, sampleFilename);
    SDLTest_AssertPass("Call to SDL_SavePNG()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SavePNG, expected: true, got: %i", ret);
    AssertFileExist(sampleFilename);

    /* Load a PNG surface */
    rface = SDL_LoadPNG(sampleFilename);
    SDLTest_AssertPass("Call to SDL_LoadPNG()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_LoadPNG is not NULL");
    if (rface != NULL) {
        SDLTest_AssertCheck(face->w == rface->w, "Verify width of loaded surface, expected: %i, got: %i", face->w, rface->w);
        SDLTest_AssertCheck(face->h == rface->h, "Verify height of loaded surface, expected: %i, got: %i", face->h, rface->h);
        SDL_DestroySurface(rface);
        rface = NULL;
    }

    /* Delete test file; ignore errors */
    SDL_RemovePath(sampleFilename);

    /* Clean up */
    SDL_DestroySurface(face);
    face = NULL;

    /* Create an 8-bit image */
    face = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(face != NULL, "Verify 8-bit surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    palette = SDL_CreatePalette(2);
    SDLTest_AssertCheck(palette != NULL, "Verify palette is not NULL");
    if (palette == NULL) {
        return TEST_ABORTED;
    }
    SDL_SetPaletteColors(palette, colors, 0, SDL_arraysize(colors));
    SDL_SetSurfacePalette(face, palette);
    SDL_DestroyPalette(palette);

    /* Set a green pixel */
    *(Uint8 *)face->pixels = 1;

    /* Save and reload as a BMP */
    stream = SDL_IOFromDynamicMem();
    SDLTest_AssertCheck(stream != NULL, "Verify iostream is not NULL");
    if (stream == NULL) {
        return TEST_ABORTED;
    }
    ret = SDL_SaveBMP_IO(face, stream, false);
    SDLTest_AssertPass("Call to SDL_SaveBMP()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SaveBMP, expected: true, got: %i", ret);
    SDL_SeekIO(stream, 0, SDL_IO_SEEK_SET);
    rface = SDL_LoadBMP_IO(stream, false);
    SDLTest_AssertPass("Call to SDL_LoadBMP()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_LoadBMP is not NULL");
    if (rface != NULL) {
        SDLTest_AssertCheck(face->w == rface->w, "Verify width of loaded surface, expected: %i, got: %i", face->w, rface->w);
        SDLTest_AssertCheck(face->h == rface->h, "Verify height of loaded surface, expected: %i, got: %i", face->h, rface->h);
        SDLTest_AssertCheck(rface->format == SDL_PIXELFORMAT_INDEX8, "Verify format of loaded surface, expected: %s, got: %s", SDL_GetPixelFormatName(face->format), SDL_GetPixelFormatName(rface->format));
        SDL_ReadSurfacePixel(rface, 0, 0, &r, &g, &b, &a);
        SDLTest_AssertCheck(r == colors[1].r &&
                            g == colors[1].g &&
                            b == colors[1].b &&
                            a == colors[1].a,
                            "Verify color of loaded surface, expected: %d,%d,%d,%d, got: %d,%d,%d,%d",
                            r, g, b, a,
                            colors[1].r, colors[1].g, colors[1].b, colors[1].a);
        SDL_DestroySurface(rface);
        rface = NULL;
    }
    SDL_CloseIO(stream);
    stream = NULL;

    /* Save and reload as a PNG */
    stream = SDL_IOFromDynamicMem();
    SDLTest_AssertCheck(stream != NULL, "Verify iostream is not NULL");
    if (stream == NULL) {
        return TEST_ABORTED;
    }
    ret = SDL_SavePNG_IO(face, stream, false);
    SDLTest_AssertPass("Call to SDL_SavePNG()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SavePNG, expected: true, got: %i", ret);
    SDL_SeekIO(stream, 0, SDL_IO_SEEK_SET);
    rface = SDL_LoadPNG_IO(stream, false);
    SDLTest_AssertPass("Call to SDL_LoadPNG()");
    SDLTest_AssertCheck(rface != NULL, "Verify result from SDL_LoadPNG is not NULL");
    if (rface != NULL) {
        SDLTest_AssertCheck(face->w == rface->w, "Verify width of loaded surface, expected: %i, got: %i", face->w, rface->w);
        SDLTest_AssertCheck(face->h == rface->h, "Verify height of loaded surface, expected: %i, got: %i", face->h, rface->h);
        SDLTest_AssertCheck(rface->format == SDL_PIXELFORMAT_INDEX8, "Verify format of loaded surface, expected: %s, got: %s", SDL_GetPixelFormatName(face->format), SDL_GetPixelFormatName(rface->format));
        SDL_ReadSurfacePixel(rface, 0, 0, &r, &g, &b, &a);
        SDLTest_AssertCheck(r == colors[1].r &&
                            g == colors[1].g &&
                            b == colors[1].b &&
                            a == colors[1].a,
                            "Verify color of loaded surface, expected: %d,%d,%d,%d, got: %d,%d,%d,%d",
                            r, g, b, a,
                            colors[1].r, colors[1].g, colors[1].b, colors[1].a);
        SDL_DestroySurface(rface);
        rface = NULL;
    }
    SDL_CloseIO(stream);
    stream = NULL;

    SDL_DestroySurface(face);

    return TEST_COMPLETED;
}

/**
 *  Tests tiled blitting.
 */
static int SDLCALL surface_testBlitTiled(void *arg)
{
    SDL_Surface *face = NULL;
    SDL_Surface *testSurface2x = NULL;
    SDL_Surface *referenceSurface2x = NULL;
    int ret = 0;

    /* Create sample surface */
    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Tiled blit - 1.0 scale */
    {
        ret = SDL_BlitSurfaceTiled(face, NULL, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_BlitSurfaceTiled expected: true, got: %i", ret);

        /* See if it's the same */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDLTest_ImageBlitTiled();
        ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* Tiled blit - 2.0 scale */
    {
        testSurface2x = SDL_CreateSurface(testSurface->w * 2, testSurface->h * 2, testSurface->format);
        SDLTest_AssertCheck(testSurface != NULL, "Check that testSurface2x is not NULL");
        ret = SDL_FillSurfaceRect(testSurface2x, NULL, SDL_MapSurfaceRGBA(testSurface2x, 0, 0, 0, 255));
        SDLTest_AssertCheck(ret == true, "Validate result from SDL_FillSurfaceRect, expected: true, got: %i", ret);

        ret = SDL_BlitSurfaceTiledWithScale(face, NULL, 2.0f, SDL_SCALEMODE_NEAREST, testSurface2x, NULL);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_BlitSurfaceTiledWithScale, expected: true, got: %i", ret);

        /* See if it's the same */
        referenceSurface2x = SDL_CreateSurface(referenceSurface->w * 2, referenceSurface->h * 2, referenceSurface->format);
        SDL_BlitSurfaceScaled(referenceSurface, NULL, referenceSurface2x, NULL, SDL_SCALEMODE_NEAREST);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_BlitSurfaceScaled, expected: true, got: %i", ret);
        ret = SDLTest_CompareSurfaces(testSurface2x, referenceSurface2x, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* Tiled blit - very small scale */
    {
        float tiny_scale = 0.01f;
        ret = SDL_BlitSurfaceTiledWithScale(face, NULL, tiny_scale, SDL_SCALEMODE_NEAREST, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Expected SDL_BlitSurfaceTiledWithScale to succeed with very small scale: %f, got: %i", tiny_scale, ret);
    }

    /* Clean up. */
    SDL_DestroySurface(face);
    SDL_DestroySurface(testSurface2x);
    SDL_DestroySurface(referenceSurface2x);

    return TEST_COMPLETED;
}

static const Uint8 COLOR_SEPARATION = 85;

static void Fill9GridReferenceSurface(SDL_Surface *surface, int left_width, int right_width, int top_height, int bottom_height)
{
    SDL_Rect rect;

    // Upper left
    rect.x = 0;
    rect.y = 0;
    rect.w = left_width;
    rect.h = top_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 1 * COLOR_SEPARATION, 1 * COLOR_SEPARATION, 0));

    // Top
    rect.x = left_width;
    rect.y = 0;
    rect.w = surface->w - left_width - right_width;
    rect.h = top_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 2 * COLOR_SEPARATION, 1 * COLOR_SEPARATION, 0));

    // Upper right
    rect.x = surface->w - right_width;
    rect.y = 0;
    rect.w = right_width;
    rect.h = top_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 3 * COLOR_SEPARATION, 1 * COLOR_SEPARATION, 0));

    // Left
    rect.x = 0;
    rect.y = top_height;
    rect.w = left_width;
    rect.h = surface->h - top_height - bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 1 * COLOR_SEPARATION, 2 * COLOR_SEPARATION, 0));

    // Center
    rect.x = left_width;
    rect.y = top_height;
    rect.w = surface->w - right_width - left_width;
    rect.h = surface->h - top_height - bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 2 * COLOR_SEPARATION, 2 * COLOR_SEPARATION, 0));

    // Right
    rect.x = surface->w - right_width;
    rect.y = top_height;
    rect.w = right_width;
    rect.h = surface->h - top_height - bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 3 * COLOR_SEPARATION, 2 * COLOR_SEPARATION, 0));

    // Lower left
    rect.x = 0;
    rect.y = surface->h - bottom_height;
    rect.w = left_width;
    rect.h = bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 1 * COLOR_SEPARATION, 3 * COLOR_SEPARATION, 0));

    // Bottom
    rect.x = left_width;
    rect.y = surface->h - bottom_height;
    rect.w = surface->w - left_width - right_width;
    rect.h = bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 2 * COLOR_SEPARATION, 3 * COLOR_SEPARATION, 0));

    // Lower right
    rect.x = surface->w - right_width;
    rect.y = surface->h - bottom_height;
    rect.w = right_width;
    rect.h = bottom_height;
    SDL_FillSurfaceRect(surface, &rect, SDL_MapSurfaceRGB(surface, 3 * COLOR_SEPARATION, 3 * COLOR_SEPARATION, 0));
}

/**
 *  Tests 9-grid blitting.
 */
static int SDLCALL surface_testBlit9Grid(void *arg)
{
    SDL_Surface *source = NULL;
    int x, y;
    int ret = 0;

    /* Create source surface */
    source = SDL_CreateSurface(3, 3, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(source != NULL, "Verify source surface is not NULL");
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 3; ++x) {
            SDL_WriteSurfacePixel(source, x, y, (Uint8)((1 + x) * COLOR_SEPARATION), (Uint8)((1 + y) * COLOR_SEPARATION), 0, 255);
        }
    }

    /* 9-grid blit - 1.0 scale */
    {
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(testSurface->w, testSurface->h, testSurface->format);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 1, 1, 1);

        ret = SDL_BlitSurface9Grid(source, NULL, 1, 1, 1, 1, 0.0f, SDL_SCALEMODE_NEAREST, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Validate result from SDL_BlitSurface9Grid, expected: true, got: %i", ret);

        ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* 9-grid blit - 2.0 scale */
    {
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(testSurface->w, testSurface->h, testSurface->format);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 2, 2, 2);

        ret = SDL_BlitSurface9Grid(source, NULL, 1, 1, 1, 1, 2.0f, SDL_SCALEMODE_NEAREST, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Validate result from SDL_BlitSurface9Grid, expected: true, got: %i", ret);

        ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* Clean up. */
    SDL_DestroySurface(source);

    /* Create complex source surface */
    source = SDL_CreateSurface(5, 5, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(source != NULL, "Verify source surface is not NULL");
    SDL_WriteSurfacePixel(source, 0, 0, (Uint8)((1) * COLOR_SEPARATION), (Uint8)((1) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 1, 0, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((1) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 2, 0, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((1) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 3, 0, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((1) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 4, 0, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((1) * COLOR_SEPARATION), 0, 255);

    SDL_WriteSurfacePixel(source, 0, 1, (Uint8)((1) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 1, 1, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 2, 1, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 3, 1, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 4, 1, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);

    SDL_WriteSurfacePixel(source, 0, 2, (Uint8)((1) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 1, 2, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 2, 2, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 3, 2, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 4, 2, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((2) * COLOR_SEPARATION), 0, 255);

    SDL_WriteSurfacePixel(source, 0, 3, (Uint8)((1) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 1, 3, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 2, 3, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 3, 3, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 4, 3, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);

    SDL_WriteSurfacePixel(source, 0, 4, (Uint8)((1) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 1, 4, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 2, 4, (Uint8)((2) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 3, 4, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);
    SDL_WriteSurfacePixel(source, 4, 4, (Uint8)((3) * COLOR_SEPARATION), (Uint8)((3) * COLOR_SEPARATION), 0, 255);

    /* complex 9-grid blit - 1.0 scale */
    {
        SDLTest_Log("complex 9-grid blit - 1.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(testSurface->w, testSurface->h, testSurface->format);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 2, 1, 2);

        ret = SDL_BlitSurface9Grid(source, NULL, 1, 2, 1, 2, 0.0f, SDL_SCALEMODE_NEAREST, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Validate result from SDL_BlitSurface9Grid, expected: true, got: %i", ret);

        ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* complex 9-grid blit - 2.0 scale */
    {
        SDLTest_Log("complex 9-grid blit - 2.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(testSurface->w, testSurface->h, testSurface->format);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 4, 2, 4);

        ret = SDL_BlitSurface9Grid(source, NULL, 1, 2, 1, 2, 2.0f, SDL_SCALEMODE_NEAREST, testSurface, NULL);
        SDLTest_AssertCheck(ret == true, "Validate result from SDL_BlitSurface9Grid, expected: true, got: %i", ret);

        ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    }

    /* Clean up. */
    SDL_DestroySurface(source);

    return TEST_COMPLETED;
}

/**
 *  Tests blitting between multiple surfaces of the same format
 */
static int SDLCALL surface_testBlitMultiple(void *arg)
{
    SDL_Surface *source, *surface;
    SDL_Palette *palette;
    Uint8 *pixels;

    palette = SDL_CreatePalette(2);
    SDLTest_AssertCheck(palette != NULL, "SDL_CreatePalette()");
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    palette->colors[1].r = 0xFF;
    palette->colors[1].g = 0;
    palette->colors[1].b = 0;

    source = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(source != NULL, "SDL_CreateSurface()");
    SDL_SetSurfacePalette(source, palette);
    *(Uint8 *)source->pixels = 1;

    /* Set up a blit to a surface using the palette */
    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
    SDL_SetSurfacePalette(surface, palette);
    pixels = (Uint8 *)surface->pixels;
    *pixels = 0;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Set up a blit to another surface using the same palette */
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
    SDL_SetSurfacePalette(surface, palette);
    pixels = (Uint8 *)surface->pixels;
    *pixels = 0;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Set up a blit to new surface with a different format */
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
    pixels = (Uint8 *)surface->pixels;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 0xFF, "Expected *pixels == 0xFF got 0x%.2X", *pixels);

    /* Set up a blit to another surface with the same format */
    SDL_DestroySurface(surface);
    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
    pixels = (Uint8 *)surface->pixels;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 0xFF, "Expected *pixels == 0xFF got 0x%.2X", *pixels);

    SDL_DestroyPalette(palette);
    SDL_DestroySurface(source);
    SDL_DestroySurface(surface);

    return TEST_COMPLETED;
}

/**
 *  Tests operations on surfaces with NULL pixels
 */
static int SDLCALL surface_testSurfaceNULLPixels(void *arg)
{
    SDL_Surface *a, *b, *face;
    bool result;

    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* Test blitting with NULL pixels */
    a = SDL_CreateSurfaceFrom(face->w, face->h, SDL_PIXELFORMAT_ARGB8888, NULL, 0);
    SDLTest_AssertCheck(a != NULL, "Verify result from SDL_CreateSurfaceFrom() with NULL pixels is not NULL");
    result = SDL_BlitSurface(a, NULL, face, NULL);
    SDLTest_AssertCheck(!result, "Verify result from SDL_BlitSurface() with src having NULL pixels is false");
    result = SDL_BlitSurface(face, NULL, a, NULL);
    SDLTest_AssertCheck(!result, "Verify result from SDL_BlitSurface() with dst having NULL pixels is false");

    b = SDL_CreateSurfaceFrom(face->w * 2, face->h * 2, SDL_PIXELFORMAT_ARGB8888, NULL, 0);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_CreateSurfaceFrom() with NULL pixels is not NULL");
    result = SDL_BlitSurfaceScaled(b, NULL, face, NULL, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(!result, "Verify result from SDL_BlitSurfaceScaled() with src having NULL pixels is false");
    result = SDL_BlitSurfaceScaled(face, NULL, b, NULL, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(!result, "Verify result from SDL_BlitSurfaceScaled() with dst having NULL pixels is false");
    SDL_DestroySurface(b);
    b = NULL;

    /* Test conversion with NULL pixels */
    b = SDL_ConvertSurfaceAndColorspace(a, SDL_PIXELFORMAT_ABGR8888, NULL, SDL_COLORSPACE_UNKNOWN, 0);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_ConvertSurfaceAndColorspace() with NULL pixels is not NULL");
    SDL_DestroySurface(b);
    b = NULL;

    /* Test duplication with NULL pixels */
    b = SDL_DuplicateSurface(a);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_DuplicateSurface() with NULL pixels is not NULL");
    SDL_DestroySurface(b);
    b = NULL;

    /* Test scaling with NULL pixels */
    b = SDL_ScaleSurface(a, a->w * 2, a->h * 2, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_ScaleSurface() with NULL pixels is not NULL");
    SDLTest_AssertCheck(b->pixels == NULL, "Verify pixels from SDL_ScaleSurface() is NULL");
    SDL_DestroySurface(b);
    b = NULL;

    /* Test filling surface with NULL pixels */
    result = SDL_FillSurfaceRect(a, NULL, 0);
    SDLTest_AssertCheck(result, "Verify result from SDL_FillSurfaceRect() with dst having NULL pixels is true");

    /* Clean up. */
    SDL_DestroySurface(face);
    SDL_DestroySurface(a);
    SDL_DestroySurface(b);

    return TEST_COMPLETED;
}

/**
 *  Tests operations on surfaces with RLE pixels
 */
static int SDLCALL surface_testSurfaceRLEPixels(void *arg)
{
    SDL_Surface *face, *a, *b, *tmp;
    int ret;
    bool result;

    face = SDLTest_ImageFace();
    SDLTest_AssertCheck(face != NULL, "Verify face surface is not NULL");
    if (face == NULL) {
        return TEST_ABORTED;
    }

    /* RLE encoding only works for 32-bit surfaces with alpha in the high bits */
    if (face->format != SDL_PIXELFORMAT_ARGB8888) {
        tmp = SDL_ConvertSurface(face, SDL_PIXELFORMAT_ARGB8888);
        SDLTest_AssertCheck(tmp != NULL, "Verify tmp surface is not NULL");
        if (tmp == NULL) {
            return TEST_ABORTED;
        }
        SDL_DestroySurface(face);
        face = tmp;
    }

    /* Create a temporary surface to trigger RLE encoding during blit */
    tmp = SDL_DuplicateSurface(face);
    SDLTest_AssertCheck(tmp != NULL, "Verify result from SDL_DuplicateSurface() with RLE pixels is not NULL");

    result = SDL_SetSurfaceRLE(face, true);
    SDLTest_AssertCheck(result, "Verify result from SDL_SetSurfaceRLE() is true");

    /* Test duplication with RLE pixels */
    a = SDL_DuplicateSurface(face);
    SDLTest_AssertCheck(a != NULL, "Verify result from SDL_DuplicateSurface() with RLE pixels is not NULL");
    SDLTest_AssertCheck(SDL_SurfaceHasRLE(a), "Verify result from SDL_DuplicateSurface() with RLE pixels has RLE set");
    ret = SDLTest_CompareSurfaces(a, face, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Verify that blitting from an RLE surface does RLE encode it */
    SDLTest_AssertCheck(!SDL_MUSTLOCK(a), "Verify initial RLE surface does not need to be locked");
    SDLTest_AssertCheck(a->pixels != NULL, "Verify initial RLE surface has pixels available");
    result = SDL_BlitSurface(a, NULL, tmp, NULL);
    SDLTest_AssertCheck(result, "Verify result from SDL_BlitSurface() with RLE surface is true");
    SDLTest_AssertCheck(SDL_MUSTLOCK(a), "Verify RLE surface after blit needs to be locked");
    SDLTest_AssertCheck(a->pixels == NULL, "Verify RLE surface after blit does not have pixels available");
    ret = SDLTest_CompareSurfaces(tmp, face, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Test scaling with RLE pixels */
    b = SDL_ScaleSurface(a, a->w * 2, a->h * 2, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_ScaleSurface() is not NULL");
    SDLTest_AssertCheck(SDL_SurfaceHasRLE(b), "Verify result from SDL_ScaleSurface() with RLE pixels has RLE set");

    /* Test scaling blitting with RLE pixels */
    result = SDL_BlitSurfaceScaled(a, NULL, b, NULL, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(result, "Verify result from SDL_BlitSurfaceScaled() with src having RLE pixels is true");
    SDL_BlitSurface(a, NULL, tmp, NULL);
    SDL_DestroySurface(b);
    b = NULL;

    /* Test conversion with RLE pixels */
    b = SDL_ConvertSurfaceAndColorspace(a, SDL_PIXELFORMAT_ABGR8888, NULL, SDL_COLORSPACE_UNKNOWN, 0);
    SDLTest_AssertCheck(b != NULL, "Verify result from SDL_ConvertSurfaceAndColorspace() with RLE pixels is not NULL");
    SDLTest_AssertCheck(SDL_SurfaceHasRLE(b), "Verify result from SDL_ConvertSurfaceAndColorspace() with RLE pixels has RLE set");
    ret = SDLTest_CompareSurfacesIgnoreTransparentPixels(b, face, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    SDL_BlitSurface(a, NULL, tmp, NULL);
    SDL_DestroySurface(b);
    b = NULL;

#if 0 /* This will currently fail, you must lock the surface first */
    /* Test filling surface with RLE pixels */
    result = SDL_FillSurfaceRect(a, NULL, 0);
    SDLTest_AssertCheck(result, "Verify result from SDL_FillSurfaceRect() with dst having RLE pixels is true");
#endif

    /* Make sure the RLE surface still needs to be locked after surface operations */
    SDLTest_AssertCheck(a->pixels == NULL, "Verify RLE surface after operations does not have pixels available");

    /* Clean up. */
    SDL_DestroySurface(face);
    SDL_DestroySurface(a);
    SDL_DestroySurface(b);
    SDL_DestroySurface(tmp);

    return TEST_COMPLETED;
}

/**
 *  Tests surface conversion.
 */
static int SDLCALL surface_testSurfaceConversion(void *arg)
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
        ret = SDL_SetSurfaceColorKey(face, true, *(Uint8 *)face->pixels);
        SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceColorKey, expected: true, got: %i", ret);
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
    SDL_DestroySurface(rface);

    return TEST_COMPLETED;
}

/**
 *  Tests surface conversion across all pixel formats.
 */
static int SDLCALL surface_testCompleteSurfaceConversion(void *arg)
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
        ret = SDL_SetSurfaceColorKey(face, true, *(Uint8 *)face->pixels);
        SDLTest_AssertPass("Call to SDL_SetSurfaceColorKey()");
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetSurfaceColorKey, expected: true, got: %i", ret);
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
static int SDLCALL surface_testLoadFailure(void *arg)
{
    SDL_Surface *face = SDL_LoadBMP("nonexistant.bmp");
    SDLTest_AssertCheck(face == NULL, "SDL_CreateLoadBmp");

    return TEST_COMPLETED;
}

/**
 * Tests blitting from a zero sized source rectangle
 */
static int SDLCALL surface_testBlitZeroSource(void *arg)
{
    SDL_Surface *src = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA8888);
    SDL_Surface *dst = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA8888);
    SDL_Rect srcrect = { 0, 0, 0, 0 };
    int ret;

    SDLTest_AssertPass("Call to SDL_BlitSurfaceScaled() with zero sized source rectangle");
    SDL_FillSurfaceRect(src, NULL, SDL_MapSurfaceRGB(src, 255, 255, 255));
    SDL_BlitSurfaceScaled(src, &srcrect, dst, NULL, SDL_SCALEMODE_NEAREST);
    ret = SDLTest_CompareSurfaces(dst, src, 0);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines.
 */
static int SDLCALL surface_testBlit(void *arg)
{
    /* Basic blitting */
    testBlitBlendMode(SDL_BLENDMODE_NONE);

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines with color mod
 */
static int SDLCALL surface_testBlitColorMod(void *arg)
{
    /* Basic blitting with color mod */
    testBlitBlendMode(-1);

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines with alpha mod
 */
static int SDLCALL surface_testBlitAlphaMod(void *arg)
{
    /* Basic blitting with alpha mod */
    testBlitBlendMode(-2);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendBlend(void *arg)
{
    /* Blend blitting */
    testBlitBlendMode(SDL_BLENDMODE_BLEND);

    return TEST_COMPLETED;
}

/**
 * @brief Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendPremultiplied(void *arg)
{
   /* Blend premultiplied blitting */
   testBlitBlendMode(SDL_BLENDMODE_BLEND_PREMULTIPLIED);

   return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendAdd(void *arg)
{
    /* Add blitting */
    testBlitBlendMode(SDL_BLENDMODE_ADD);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendAddPremultiplied(void *arg)
{
    /* Add premultiplied blitting */
    testBlitBlendMode(SDL_BLENDMODE_ADD_PREMULTIPLIED);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendMod(void *arg)
{
    /* Mod blitting */
    testBlitBlendMode(SDL_BLENDMODE_MOD);

    return TEST_COMPLETED;
}

/**
 * Tests some more blitting routines.
 */
static int SDLCALL surface_testBlitBlendMul(void *arg)
{
    /* Mod blitting */
    testBlitBlendMode(SDL_BLENDMODE_MUL);

    return TEST_COMPLETED;
}

/**
 * Tests blitting bitmaps
 */
static int SDLCALL surface_testBlitBitmap(void *arg)
{
    const SDL_PixelFormat formats[] = {
        SDL_PIXELFORMAT_INDEX1LSB,
        SDL_PIXELFORMAT_INDEX1MSB,
        SDL_PIXELFORMAT_INDEX2LSB,
        SDL_PIXELFORMAT_INDEX2MSB,
        SDL_PIXELFORMAT_INDEX4LSB,
        SDL_PIXELFORMAT_INDEX4MSB
    };
    Uint8 pixel;
    int i, j;
    bool result;
    SDL_Surface *dst = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ARGB8888);
    SDL_Color colors[] = {
        { 0x00, 0x00, 0x00, 0xFF },
        { 0xFF, 0xFF, 0xFF, 0xFF }
    };
    SDL_Palette *palette;
    Uint32 value, expected = 0xFFFFFFFF;

    palette = SDL_CreatePalette(SDL_arraysize(colors));
    SDLTest_AssertCheck(palette != NULL, "SDL_CreatePalette() != NULL, result = %p", palette);

    result = SDL_SetPaletteColors(palette, colors, 0, SDL_arraysize(colors));
    SDLTest_AssertCheck(result, "SDL_SetPaletteColors, result = %s", result ? "true" : "false");

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        SDL_PixelFormat format = formats[i];
        int bpp = SDL_BITSPERPIXEL(format);
        int width = (8 / bpp);

        if (SDL_PIXELORDER(format) == SDL_BITMAPORDER_1234) {
            switch (bpp) {
            case 1:
                pixel = 0x80;
                break;
            case 2:
                pixel = 0x40;
                break;
            case 4:
                pixel = 0x10;
                break;
            default:
                SDL_assert(!"Unexpected bpp");
                break;
            }
        } else {
            pixel = 0x01;
        }
        for (j = 0; j < width; ++j) {
            SDL_Rect rect = { j, 0, 1, 1 };
            SDL_Surface *src = SDL_CreateSurfaceFrom(width, 1, format, &pixel, 1);
            SDL_SetSurfacePalette(src, palette);
            *(Uint32 *)dst->pixels = 0;
            result = SDL_BlitSurface(src, &rect, dst, NULL);
            SDLTest_AssertCheck(result, "SDL_BlitSurface(%s pixel %d), result = %s", SDL_GetPixelFormatName(format), j, result ? "true" : "false");
            value = *(Uint32 *)dst->pixels;
            SDLTest_AssertCheck(value == expected, "Expected value == 0x%" SDL_PRIx32 ", actually = 0x%" SDL_PRIx32, expected, value);
            SDL_DestroySurface(src);

            if (SDL_PIXELORDER(format) == SDL_BITMAPORDER_1234) {
                pixel >>= bpp;
            } else {
                pixel <<= bpp;
            }
        }
    }
    SDL_DestroyPalette(palette);
    SDL_DestroySurface(dst);

    return TEST_COMPLETED;
}

/**
 * Tests blitting invalid surfaces.
 */
static int SDLCALL surface_testBlitInvalid(void *arg)
{
    SDL_Surface *valid, *invalid;
    bool result;

    valid = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA8888);
    SDLTest_AssertCheck(valid != NULL, "Check surface creation");
    invalid = SDL_CreateSurface(0, 0, SDL_PIXELFORMAT_RGBA8888);
    SDLTest_AssertCheck(invalid != NULL, "Check surface creation");
    SDLTest_AssertCheck(invalid->pixels == NULL, "Check surface pixels are NULL");

    result = SDL_BlitSurface(invalid, NULL, valid, NULL);
    SDLTest_AssertCheck(result == false, "SDL_BlitSurface(invalid, NULL, valid, NULL), result = %s", result ? "true" : "false");
    result = SDL_BlitSurface(valid, NULL, invalid, NULL);
    SDLTest_AssertCheck(result == false, "SDL_BlitSurface(valid, NULL, invalid, NULL), result = %s", result ? "true" : "false");

    result = SDL_BlitSurfaceScaled(invalid, NULL, valid, NULL, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(result == false, "SDL_BlitSurfaceScaled(invalid, NULL, valid, NULL, SDL_SCALEMODE_NEAREST), result = %s", result ? "true" : "false");
    result = SDL_BlitSurfaceScaled(valid, NULL, invalid, NULL, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(result == false, "SDL_BlitSurfaceScaled(valid, NULL, invalid, NULL, SDL_SCALEMODE_NEAREST), result = %s", result ? "true" : "false");

    SDL_DestroySurface(valid);
    SDL_DestroySurface(invalid);

    return TEST_COMPLETED;
}

static int SDLCALL surface_testBlitsWithBadCoordinates(void *arg)
{
    const SDL_Rect rect[8] = {
        { SDL_MAX_SINT32, 0, 2, 2 },
        { 0, SDL_MAX_SINT32, 2, 2 },
        { 0, 0, SDL_MAX_SINT32, 2 },
        { 0, 0, 2, SDL_MAX_SINT32 },
        { SDL_MIN_SINT32, 0, 2, 2 },
        { 0, SDL_MIN_SINT32, 2, 2 },
        { 0, 0, SDL_MIN_SINT32, 2 },
        { 0, 0, 2, SDL_MIN_SINT32 }
    };

    SDL_Surface *s;
    bool result;
    int i;

    s = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA8888);
    SDLTest_AssertCheck(s != NULL, "Check surface creation");

    for (i = 0; i < 8; i++) {
        result = SDL_BlitSurface(s, NULL, s, &rect[i]);
        SDLTest_AssertCheck(result == true, "SDL_BlitSurface(valid, NULL, valid, &rect), result = %s", result ? "true" : "false");

        result = SDL_BlitSurface(s, &rect[i], s, NULL);
        SDLTest_AssertCheck(result == true, "SDL_BlitSurface(valid, &rect, valid, NULL), result = %s", result ? "true" : "false");

        result = SDL_BlitSurfaceScaled(s, NULL, s, &rect[i], SDL_SCALEMODE_NEAREST);
        SDLTest_AssertCheck(result == true, "SDL_BlitSurfaceScaled(valid, NULL, valid, &rect, SDL_SCALEMODE_NEAREST), result = %s", result ? "true" : "false");

        result = SDL_BlitSurfaceScaled(s, &rect[i], s, NULL, SDL_SCALEMODE_NEAREST);
        SDLTest_AssertCheck(result == true, "SDL_BlitSurfaceScaled(valid, &rect, valid, NULL, SDL_SCALEMODE_NEAREST), result = %s", result ? "true" : "false");
    }

    SDL_DestroySurface(s);

    return TEST_COMPLETED;
}

static int SDLCALL surface_testOverflow(void *arg)
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

static int surface_testSetGetSurfaceClipRect(void *args)
{
    const struct {
        SDL_Rect r;
        bool clipsetval;
        bool cmpval;
    } rect_list[] = {
        { {   0,   0,   0,   0}, false, true},
        { {   2,   2,   0,   0}, false, true},
        { {   2,   2,   5,   1}, true,  true},
        { {   6,   5,  10,   3}, true,  false},
        { {   0,   0,  10,  10}, true,  true},
        { {   0,   0, -10,  10}, false, true},
        { {   0,   0, -10, -10}, false, true},
        { { -10, -10,  10,  10}, false, false},
        { {  10, -10,  10,  10}, false, false},
        { {  10,  10,  10,  10}, true,  false}
    };
    SDL_Surface *s;
    SDL_Rect r;
    int i;
    bool b;

    SDLTest_AssertPass("About to call SDL_CreateSurface(15, 15, SDL_PIXELFORMAT_RGBA32)");
    s = SDL_CreateSurface(15, 15, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(s != NULL, "SDL_CreateSurface returned non-null surface");
    SDL_zero(r);
    b = SDL_GetSurfaceClipRect(s, &r);
    SDLTest_AssertCheck(b, "SDL_GetSurfaceClipRect succeeded (%s)", SDL_GetError());
    SDLTest_AssertCheck(r.x == 0 && r.y == 0 && r.w == 15 && r.h == 15,
        "SDL_GetSurfaceClipRect of just-created surface. Got {%d, %d, %d, %d}. (Expected {%d, %d, %d, %d})",
        r.x, r.y, r.w, r.h, 0, 0, 15, 15);

    for (i = 0; i < SDL_arraysize(rect_list); i++) {
        const SDL_Rect *r_in = &rect_list[i].r;
        SDL_Rect r_out;

        SDLTest_AssertPass("About to do SDL_SetClipRect({%d, %d, %d, %d})", r_in->x, r_in->y, r_in->w, r_in->h);
        b = SDL_SetSurfaceClipRect(s, r_in);
        SDLTest_AssertCheck(b == rect_list[i].clipsetval, "SDL_SetSurfaceClipRect returned %d (expected %d)", b, rect_list[i].clipsetval);
        SDL_zero(r_out);
        SDL_GetSurfaceClipRect(s, &r_out);
        SDLTest_AssertPass("SDL_GetSurfaceClipRect returned {%d, %d, %d, %d}", r_out.x, r_out.y, r_out.w, r_out.h);
        b = r_out.x == r_in->x && r_out.y == r_in->y && r_out.w == r_in->w && r_out.h == r_in->h;
        SDLTest_AssertCheck(b == rect_list[i].cmpval, "Current clipping rect is identical to input clipping rect: %d (expected %d)",
            b, rect_list[i].cmpval);
    }
    SDL_DestroySurface(s);
    return TEST_COMPLETED;
};

static int SDLCALL surface_testFlip(void *arg)
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

static int SDLCALL surface_testPalette(void *arg)
{
    SDL_Surface *source, *surface, *output;
    SDL_Palette *palette;
    Uint8 *pixels;

    palette = SDL_CreatePalette(2);
    SDLTest_AssertCheck(palette != NULL, "SDL_CreatePalette()");

    source = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(source != NULL, "SDL_CreateSurface()");
    SDLTest_AssertCheck(SDL_GetSurfacePalette(source) == NULL, "SDL_GetSurfacePalette(source)");

    surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_INDEX8);
    SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
    SDLTest_AssertCheck(SDL_GetSurfacePalette(surface) == NULL, "SDL_GetSurfacePalette(surface)");

    pixels = (Uint8 *)surface->pixels;
    SDLTest_AssertCheck(*pixels == 0, "Expected *pixels == 0 got %u", *pixels);

    /* Identity copy between indexed surfaces without a palette */
    *(Uint8 *)source->pixels = 1;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Identity copy between indexed surfaces where the source has a palette */
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    palette->colors[1].r = 0xFF;
    palette->colors[1].g = 0;
    palette->colors[1].b = 0;
    SDL_SetSurfacePalette(source, palette);
    *pixels = 0;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Identity copy between indexed surfaces where the destination has a palette */
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    palette->colors[1].r = 0xFF;
    palette->colors[1].g = 0;
    palette->colors[1].b = 0;
    SDL_SetSurfacePalette(source, NULL);
    SDL_SetSurfacePalette(surface, palette);
    *pixels = 0;
    SDL_BlitSurface(source, NULL, surface, NULL);
    SDLTest_AssertCheck(*pixels == 1, "Expected *pixels == 1 got %u", *pixels);

    /* Identity copy between indexed surfaces where the source and destination share a palette */
    palette->colors[0].r = 0;
    palette->colors[0].g = 0;
    palette->colors[0].b = 0;
    palette->colors[1].r = 0xFF;
    palette->colors[1].g = 0;
    palette->colors[1].b = 0;
    SDL_SetSurfacePalette(source, palette);
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

static int SDLCALL surface_testPalettization(void *arg)
{
    const SDL_Color palette_colors[] = {
        { 0x80, 0x00, 0x00, 0xff },
        { 0x00, 0x80, 0x00, 0xff },
        { 0x00, 0x00, 0x80, 0xff },
        { 0x40, 0x00, 0x00, 0xff },
        { 0x00, 0x40, 0x00, 0xff },
        { 0x00, 0x00, 0x40, 0xff },
        { 0x00, 0x00, 0x00, 0xff },
        { 0xff, 0x00, 0x00, 0xff },
        { 0x00, 0xff, 0x00, 0xff },
        { 0x00, 0x00, 0xff, 0xff },
        { 0xff, 0xff, 0x00, 0xff },
        { 0x00, 0xff, 0xff, 0xff },
        { 0xff, 0x00, 0xff, 0xff },
    };
    const struct {
        SDL_Color c;
        Uint8 e;
    } colors[] = {
        { { 0xff, 0x00, 0x00, 0xff }, 7 },
        { { 0xfe, 0x00, 0x00, 0xff }, 7 },
        { { 0xfd, 0x00, 0x00, 0xff }, 7 },
        { { 0xf0, 0x00, 0x00, 0xff }, 7 },
        { { 0xd0, 0x00, 0x00, 0xff }, 7 },
        { { 0xb0, 0x00, 0x00, 0xff }, 0 },
        { { 0xa0, 0x00, 0x00, 0xff }, 0 },
        { { 0xff, 0x00, 0x00, 0x00 }, 7 },
        { { 0x00, 0x10, 0x21, 0xff }, 5 },
        { { 0x00, 0x10, 0x19, 0xff }, 6 },
        { { 0x81, 0x00, 0x41, 0xff }, 0 },
        { { 0x80, 0xf0, 0xf0, 0x7f }, 11 },
        { { 0x00, 0x00, 0x00, 0xff }, 6 },
        { { 0x00, 0x00, 0x00, 0x01 }, 6 },
    };
    int i;
    int result;
    SDL_Surface *source, *output;
    SDL_Palette *palette;
    Uint8 *pixels;

    palette = SDL_CreatePalette(SDL_arraysize(palette_colors));
    SDLTest_AssertCheck(palette != NULL, "SDL_CreatePalette()");

    result = SDL_SetPaletteColors(palette, palette_colors, 0, SDL_arraysize(palette_colors));
    SDLTest_AssertCheck(result, "SDL_SetPaletteColors()");

    source = SDL_CreateSurface(SDL_arraysize(palette_colors) + SDL_arraysize(colors), 1, SDL_PIXELFORMAT_RGBA8888);
    SDLTest_AssertCheck(source != NULL, "SDL_CreateSurface()");
    SDLTest_AssertCheck(source->w == SDL_arraysize(palette_colors) + SDL_arraysize(colors), "Expected source->w == %d, got %d", (int)(SDL_arraysize(palette_colors) + SDL_arraysize(colors)), source->w);
    SDLTest_AssertCheck(source->h == 1, "Expected source->h == %d, got %d", 1, source->h);
    SDLTest_AssertCheck(source->format == SDL_PIXELFORMAT_RGBA8888, "Expected source->format == SDL_PIXELFORMAT_RGBA8888, got 0x%x (%s)", source->format, SDL_GetPixelFormatName(source->format));
    for (i = 0; i < SDL_arraysize(colors); i++) {
        result = SDL_WriteSurfacePixel(source, i, 0, colors[i].c.r, colors[i].c.g, colors[i].c.b, colors[i].c.a);
        SDLTest_AssertCheck(result == true, "SDL_WriteSurfacePixel");
    }
    for (i = 0; i < SDL_arraysize(palette_colors); i++) {
        result = SDL_WriteSurfacePixel(source, SDL_arraysize(colors) + i, 0, palette_colors[i].r, palette_colors[i].g, palette_colors[i].b, palette_colors[i].a);
        SDLTest_AssertCheck(result == true, "SDL_WriteSurfacePixel");
    }

    output = SDL_ConvertSurfaceAndColorspace(source, SDL_PIXELFORMAT_INDEX8, palette, SDL_COLORSPACE_UNKNOWN, 0);
    SDLTest_AssertCheck(output != NULL, "SDL_ConvertSurfaceAndColorspace()");
    SDLTest_AssertCheck(output->w == source->w, "Expected output->w == %d, got %d", source->w, output->w);
    SDLTest_AssertCheck(output->h == source->h, "Expected output->h == %d, got %d", source->h, output->h);
    SDLTest_AssertCheck(output->format == SDL_PIXELFORMAT_INDEX8, "Expected output->format == SDL_PIXELFORMAT_INDEX8, got 0x%x (%s)", output->format, SDL_GetPixelFormatName(output->format));

    pixels = output->pixels;
    for (i = 0; i < SDL_arraysize(colors); i++) {
        int idx = i;
        Uint8 actual = pixels[idx];
        Uint8 expected = colors[i].e;
        SDLTest_AssertCheck(actual < SDL_arraysize(palette_colors), "output->pixels[%d] < %d", idx, (int)SDL_arraysize(palette_colors));
        SDLTest_AssertCheck(actual == expected, "Expected output->pixels[%d] == %u, got %u", idx, expected, actual);
    }
    SDLTest_AssertPass("Check palette 1:1 mapping");
    for (i = 0; i < SDL_arraysize(palette_colors); i++) {
        int idx = SDL_arraysize(colors) + i;
        Uint8 actual = pixels[idx];
        Uint8 expected = i;
        SDLTest_AssertCheck(actual < SDL_arraysize(palette_colors), "output->pixels[%d] < %d", idx, (int)SDL_arraysize(palette_colors));
        SDLTest_AssertCheck(actual == expected, "Expected output->pixels[%d] == %u, got %u", idx, expected, actual);
    }
    SDL_DestroyPalette(palette);
    SDL_DestroySurface(source);
    SDL_DestroySurface(output);

    return TEST_COMPLETED;
}

static int SDLCALL surface_testClearSurface(void *arg)
{
    SDL_PixelFormat formats[] = {
        SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_RGBA8888,
        SDL_PIXELFORMAT_ARGB2101010, SDL_PIXELFORMAT_ABGR2101010,
        SDL_PIXELFORMAT_ARGB64, SDL_PIXELFORMAT_RGBA64,
        SDL_PIXELFORMAT_ARGB128_FLOAT, SDL_PIXELFORMAT_RGBA128_FLOAT,
        SDL_PIXELFORMAT_YV12, SDL_PIXELFORMAT_UYVY, SDL_PIXELFORMAT_NV12
    };
    SDL_Surface *surface;
    SDL_PixelFormat format;
    const float MAXIMUM_ERROR_RGB = 0.0001f;
    const float MAXIMUM_ERROR_YUV = 0.01f;
    float srcR = 10 / 255.0f, srcG = 128 / 255.0f, srcB = 240 / 255.0f, srcA = 1.0f;
    float actualR, actualG, actualB, actualA;
    float deltaR, deltaG, deltaB, deltaA;
    int i, ret;

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        const float MAXIMUM_ERROR = SDL_ISPIXELFORMAT_FOURCC(formats[i]) ? MAXIMUM_ERROR_YUV : MAXIMUM_ERROR_RGB;

        format = formats[i];

        surface = SDL_CreateSurface(1, 1, format);
        SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
        ret = SDL_ClearSurface(surface, srcR, srcG, srcB, srcA);
        SDLTest_AssertCheck(ret == true, "SDL_ClearSurface()");
        ret = SDL_ReadSurfacePixelFloat(surface, 0, 0, &actualR, &actualG, &actualB, &actualA);
        SDLTest_AssertCheck(ret == true, "SDL_ReadSurfacePixelFloat()");
        deltaR = SDL_fabsf(actualR - srcR);
        deltaG = SDL_fabsf(actualG - srcG);
        deltaB = SDL_fabsf(actualB - srcB);
        deltaA = SDL_fabsf(actualA - srcA);
        SDLTest_AssertCheck(
            deltaR <= MAXIMUM_ERROR &&
            deltaG <= MAXIMUM_ERROR &&
            deltaB <= MAXIMUM_ERROR &&
            deltaA <= MAXIMUM_ERROR,
            "Checking %s surface clear results, expected %.4f,%.4f,%.4f,%.4f, got %.4f,%.4f,%.4f,%.4f",
            SDL_GetPixelFormatName(format),
            srcR, srcG, srcB, srcA, actualR, actualG, actualB, actualA);

        SDL_DestroySurface(surface);
    }

    return TEST_COMPLETED;
}

static int SDLCALL surface_testPremultiplyAlpha(void *arg)
{
    SDL_PixelFormat formats[] = {
        SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_RGBA8888,
        SDL_PIXELFORMAT_ARGB2101010, SDL_PIXELFORMAT_ABGR2101010,
        SDL_PIXELFORMAT_ARGB64, SDL_PIXELFORMAT_RGBA64,
        SDL_PIXELFORMAT_ARGB128_FLOAT, SDL_PIXELFORMAT_RGBA128_FLOAT,
    };
    SDL_Surface *surface;
    SDL_PixelFormat format;
    const float MAXIMUM_ERROR_LOW_PRECISION = 1 / 255.0f;
    const float MAXIMUM_ERROR_HIGH_PRECISION = 0.0001f;
    float srcR = 10 / 255.0f, srcG = 128 / 255.0f, srcB = 240 / 255.0f, srcA = 170 / 255.0f;
    float expectedR = srcR * srcA;
    float expectedG = srcG * srcA;
    float expectedB = srcB * srcA;
    float actualR, actualG, actualB;
    float deltaR, deltaG, deltaB;
    int i, ret;

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        const float MAXIMUM_ERROR = (SDL_BITSPERPIXEL(formats[i]) > 32) ? MAXIMUM_ERROR_HIGH_PRECISION : MAXIMUM_ERROR_LOW_PRECISION;

        format = formats[i];

        surface = SDL_CreateSurface(1, 1, format);
        SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
        ret = SDL_SetSurfaceColorspace(surface, SDL_COLORSPACE_SRGB);
        SDLTest_AssertCheck(ret == true, "SDL_SetSurfaceColorspace()");
        ret = SDL_ClearSurface(surface, srcR, srcG, srcB, srcA);
        SDLTest_AssertCheck(ret == true, "SDL_ClearSurface()");
        ret = SDL_PremultiplySurfaceAlpha(surface, false);
        SDLTest_AssertCheck(ret == true, "SDL_PremultiplySurfaceAlpha()");
        ret = SDL_ReadSurfacePixelFloat(surface, 0, 0, &actualR, &actualG, &actualB, NULL);
        SDLTest_AssertCheck(ret == true, "SDL_ReadSurfacePixelFloat()");
        deltaR = SDL_fabsf(actualR - expectedR);
        deltaG = SDL_fabsf(actualG - expectedG);
        deltaB = SDL_fabsf(actualB - expectedB);
        SDLTest_AssertCheck(
            deltaR <= MAXIMUM_ERROR &&
            deltaG <= MAXIMUM_ERROR &&
            deltaB <= MAXIMUM_ERROR,
            "Checking %s alpha premultiply results, expected %.4f,%.4f,%.4f, got %.4f,%.4f,%.4f",
            SDL_GetPixelFormatName(format),
            expectedR, expectedG, expectedB, actualR, actualG, actualB);

        SDL_DestroySurface(surface);
    }

    return TEST_COMPLETED;
}


static int SDLCALL surface_testScale(void *arg)
{
    SDL_PixelFormat formats[] = {
        SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_RGBA8888,
        SDL_PIXELFORMAT_ARGB2101010, SDL_PIXELFORMAT_ABGR2101010,
        SDL_PIXELFORMAT_ARGB64, SDL_PIXELFORMAT_RGBA64,
        SDL_PIXELFORMAT_ARGB128_FLOAT, SDL_PIXELFORMAT_RGBA128_FLOAT,
    };
    SDL_ScaleMode modes[] = {
        SDL_SCALEMODE_NEAREST, SDL_SCALEMODE_LINEAR, SDL_SCALEMODE_PIXELART
    };
    SDL_Surface *surface, *result;
    SDL_PixelFormat format;
    SDL_ScaleMode mode;
    const float MAXIMUM_ERROR = 0.0001f;
    float srcR = 10 / 255.0f, srcG = 128 / 255.0f, srcB = 240 / 255.0f, srcA = 170 / 255.0f;
    float actualR, actualG, actualB, actualA;
    float deltaR, deltaG, deltaB, deltaA;
    int i, j, ret;

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        for (j = 0; j < SDL_arraysize(modes); ++j) {
            format = formats[i];
            mode = modes[j];

            surface = SDL_CreateSurface(1, 1, format);
            SDLTest_AssertCheck(surface != NULL, "SDL_CreateSurface()");
            ret = SDL_SetSurfaceColorspace(surface, SDL_COLORSPACE_SRGB);
            SDLTest_AssertCheck(ret == true, "SDL_SetSurfaceColorspace()");
            ret = SDL_ClearSurface(surface, srcR, srcG, srcB, srcA);
            SDLTest_AssertCheck(ret == true, "SDL_ClearSurface()");
            result = SDL_ScaleSurface(surface, 2, 2, mode);
            SDLTest_AssertCheck(ret == true, "SDL_PremultiplySurfaceAlpha()");
            ret = SDL_ReadSurfacePixelFloat(result, 1, 1, &actualR, &actualG, &actualB, &actualA);
            SDLTest_AssertCheck(ret == true, "SDL_ReadSurfacePixelFloat()");
            deltaR = SDL_fabsf(actualR - srcR);
            deltaG = SDL_fabsf(actualG - srcG);
            deltaB = SDL_fabsf(actualB - srcB);
            deltaA = SDL_fabsf(actualA - srcA);
            SDLTest_AssertCheck(
                deltaR <= MAXIMUM_ERROR &&
                deltaG <= MAXIMUM_ERROR &&
                deltaB <= MAXIMUM_ERROR &&
                deltaA <= MAXIMUM_ERROR,
                "Checking %s %s scaling results, expected %.4f,%.4f,%.4f,%.4f got %.4f,%.4f,%.4f,%.4f",
                SDL_GetPixelFormatName(format),
                mode == SDL_SCALEMODE_NEAREST ? "nearest" :
                mode == SDL_SCALEMODE_LINEAR ? "linear" :
                mode == SDL_SCALEMODE_PIXELART ? "pixelart" : "unknown",
                srcR, srcG, srcB, srcA, actualR, actualG, actualB, actualA);

            SDL_DestroySurface(surface);
            SDL_DestroySurface(result);
        }
    }

    return TEST_COMPLETED;
}

#define GENERATE_SHIFTS

static Uint32 Calculate(int v, int bits, int vmax, int shift)
{
#if defined(GENERATE_FLOOR)
    return (Uint32)SDL_floor(v * 255.0f / vmax) << shift;
#elif defined(GENERATE_ROUND)
    return (Uint32)SDL_roundf(v * 255.0f / vmax) << shift;
#elif defined(GENERATE_SHIFTS)
    switch (bits) {
    case 1:
        v = (v << 7) | (v << 6) | (v << 5) | (v << 4) | (v << 3) | (v << 2) | (v << 1) | v;
        break;
    case 2:
        v = (v << 6) | (v << 4) | (v << 2) | v;
        break;
    case 3:
        v = (v << 5) | (v << 2) | (v >> 1);
        break;
    case 4:
        v = (v << 4) | v;
        break;
    case 5:
        v = (v << 3) | (v >> 2);
        break;
    case 6:
        v = (v << 2) | (v >> 4);
        break;
    case 7:
        v = (v << 1) | (v >> 6);
        break;
    case 8:
        break;
    }
    return (Uint32)v << shift;
#endif
}

static Uint32 Calculate565toARGB(int v, const SDL_PixelFormatDetails *fmt)
{
    Uint8 r = (v & 0xF800) >> 11;
    Uint8 g = (v & 0x07E0) >> 5;
    Uint8 b = (v & 0x001F);
    return fmt->Amask |
            Calculate(r, 5, 31, fmt->Rshift) |
            Calculate(g, 6, 63, fmt->Gshift) |
            Calculate(b, 5, 31, fmt->Bshift);
}

static int SDLCALL surface_test16BitTo32Bit(void *arg)
{
    static const SDL_PixelFormat formats[] = {
        SDL_PIXELFORMAT_ARGB8888,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_PIXELFORMAT_BGRA8888
    };
    static Uint16 pixels[1 << 16];
    static Uint32 expected[1 << 16];
    int i, p, ret;
    SDL_Surface *surface16;
    SDL_Surface *surface32;
    SDL_Surface *expected32;

    for (p = 0; p < SDL_arraysize(pixels); ++p) {
        pixels[p] = p;
    }
    surface16 = SDL_CreateSurfaceFrom(SDL_arraysize(pixels), 1, SDL_PIXELFORMAT_RGB565, pixels, sizeof(pixels));

    for (i = 0; i < SDL_arraysize(formats); ++i) {
        SDL_PixelFormat format = formats[i];
        const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(format);

        SDLTest_Log("Checking conversion from SDL_PIXELFORMAT_RGB565 to %s", SDL_GetPixelFormatName(format));
        surface32 = SDL_ConvertSurface(surface16, format);
        for (p = 0; p < SDL_arraysize(pixels); ++p) {
            expected[p] = Calculate565toARGB(p, fmt);
        }
        expected32 = SDL_CreateSurfaceFrom(SDL_arraysize(expected), 1, format, expected, sizeof(expected));
        ret = SDLTest_CompareSurfaces(surface32, expected32, 0);
        SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);
        SDL_DestroySurface(surface32);
        SDL_DestroySurface(expected32);
    }
    SDL_DestroySurface(surface16);

    return TEST_COMPLETED;
}


/* ================= Test References ================== */

/* Surface test cases */
static const SDLTest_TestCaseReference surfaceTestInvalidFormat = {
    surface_testInvalidFormat, "surface_testInvalidFormat", "Tests creating surface with invalid format", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestSaveLoad = {
    surface_testSaveLoad, "surface_testSaveLoad", "Tests sprite saving and loading.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitZeroSource = {
    surface_testBlitZeroSource, "surface_testBlitZeroSource", "Tests blitting from a zero sized source rectangle", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlit = {
    surface_testBlit, "surface_testBlit", "Tests basic blitting.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitTiled = {
    surface_testBlitTiled, "surface_testBlitTiled", "Tests tiled blitting.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlit9Grid = {
    surface_testBlit9Grid, "surface_testBlit9Grid", "Tests 9-grid blitting.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitMultiple = {
    surface_testBlitMultiple, "surface_testBlitMultiple", "Tests blitting between multiple surfaces of the same format.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestLoadFailure = {
    surface_testLoadFailure, "surface_testLoadFailure", "Tests sprite loading. A failure case.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestNULLPixels = {
    surface_testSurfaceNULLPixels, "surface_testSurfaceNULLPixels", "Tests surface operations with NULL pixels.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestRLEPixels = {
    surface_testSurfaceRLEPixels, "surface_testSurfaceRLEPixels", "Tests surface operations with RLE surfaces.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestSurfaceConversion = {
    surface_testSurfaceConversion, "surface_testSurfaceConversion", "Tests surface conversion.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestCompleteSurfaceConversion = {
    surface_testCompleteSurfaceConversion, "surface_testCompleteSurfaceConversion", "Tests surface conversion across all pixel formats", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitColorMod = {
    surface_testBlitColorMod, "surface_testBlitColorMod", "Tests some blitting routines with color mod.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitAlphaMod = {
    surface_testBlitAlphaMod, "surface_testBlitAlphaMod", "Tests some blitting routines with alpha mod.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendBlend = {
    surface_testBlitBlendBlend, "surface_testBlitBlendBlend", "Tests blitting routines with blend blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendPremultiplied = {
    surface_testBlitBlendPremultiplied, "surface_testBlitBlendPremultiplied", "Tests blitting routines with premultiplied blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendAdd = {
    surface_testBlitBlendAdd, "surface_testBlitBlendAdd", "Tests blitting routines with add blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendAddPremultiplied = {
    surface_testBlitBlendAddPremultiplied, "surface_testBlitBlendAddPremultiplied", "Tests blitting routines with premultiplied add blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendMod = {
    surface_testBlitBlendMod, "surface_testBlitBlendMod", "Tests blitting routines with mod blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBlendMul = {
    surface_testBlitBlendMul, "surface_testBlitBlendMul", "Tests blitting routines with mul blending mode.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitBitmap = {
    surface_testBlitBitmap, "surface_testBlitBitmap", "Tests blitting routines with bitmap surfaces.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitInvalid = {
    surface_testBlitInvalid, "surface_testBlitInvalid", "Tests blitting routines with invalid surfaces.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestBlitsWithBadCoordinates = {
    surface_testBlitsWithBadCoordinates, "surface_testBlitsWithBadCoordinates", "Test blitting routines with bad coordinates.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestOverflow = {
    surface_testOverflow, "surface_testOverflow", "Test overflow detection.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestSetGetClipRect = {
    surface_testSetGetSurfaceClipRect, "surface_testSetGetSurfaceClipRect", "Test SDL_(Set|Get)SurfaceClipRect.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestFlip = {
    surface_testFlip, "surface_testFlip", "Test surface flipping.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestPalette = {
    surface_testPalette, "surface_testPalette", "Test surface palette operations.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestPalettization = {
    surface_testPalettization, "surface_testPalettization", "Test surface palettization.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestClearSurface = {
    surface_testClearSurface, "surface_testClearSurface", "Test clear surface operations.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestPremultiplyAlpha = {
    surface_testPremultiplyAlpha, "surface_testPremultiplyAlpha", "Test alpha premultiply operations.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTestScale = {
    surface_testScale, "surface_testScale", "Test scaling operations.", TEST_ENABLED
};

static const SDLTest_TestCaseReference surfaceTest16BitTo32Bit = {
    surface_test16BitTo32Bit, "surface_test16BitTo32Bit", "Test conversion from 16-bit to 32-bit pixels.", TEST_ENABLED
};

/* Sequence of Surface test cases */
static const SDLTest_TestCaseReference *surfaceTests[] = {
    &surfaceTestInvalidFormat,
    &surfaceTestSaveLoad,
    &surfaceTestBlitZeroSource,
    &surfaceTestBlit,
    &surfaceTestBlitTiled,
    &surfaceTestBlit9Grid,
    &surfaceTestBlitMultiple,
    &surfaceTestLoadFailure,
    &surfaceTestNULLPixels,
    &surfaceTestRLEPixels,
    &surfaceTestSurfaceConversion,
    &surfaceTestCompleteSurfaceConversion,
    &surfaceTestBlitColorMod,
    &surfaceTestBlitAlphaMod,
    &surfaceTestBlitBlendBlend,
    &surfaceTestBlitBlendPremultiplied,
    &surfaceTestBlitBlendAdd,
    &surfaceTestBlitBlendAddPremultiplied,
    &surfaceTestBlitBlendMod,
    &surfaceTestBlitBlendMul,
    &surfaceTestBlitBitmap,
    &surfaceTestBlitInvalid,
    &surfaceTestBlitsWithBadCoordinates,
    &surfaceTestOverflow,
    &surfaceTestSetGetClipRect,
    &surfaceTestFlip,
    &surfaceTestPalette,
    &surfaceTestPalettization,
    &surfaceTestClearSurface,
    &surfaceTestPremultiplyAlpha,
    &surfaceTestScale,
    &surfaceTest16BitTo32Bit,
    NULL
};

/* Surface test suite (global) */
SDLTest_TestSuiteReference surfaceTestSuite = {
    "Surface",
    surfaceSetUp,
    surfaceTests,
    surfaceTearDown

};
