/**
 * Original code: automated SDL platform test written by Edgar Simo "bobbens"
 * Extended and extensively updated by aschiffler at ferzkopp dot net
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_images.h"
#include "testautomation_suites.h"

/* ================= Test Case Implementation ================== */

#define TESTRENDER_SCREEN_W 80
#define TESTRENDER_SCREEN_H 60


#define RENDER_COMPARE_FORMAT SDL_PIXELFORMAT_ARGB8888
#define RENDER_COLOR_CLEAR  0xFF000000
#define RENDER_COLOR_GREEN  0xFF00FF00

#define ALLOWABLE_ERROR_OPAQUE  0
#define ALLOWABLE_ERROR_BLENDED 0

#define CHECK_FUNC(FUNC, PARAMS)    \
{                                   \
    bool result = FUNC PARAMS;  \
    if (!result) {                  \
        SDLTest_AssertCheck(result, "Validate result from %s, expected: true, got: false, %s", #FUNC, SDL_GetError()); \
    }                               \
}

/* Test window and renderer */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

/* Prototypes for helper functions */

static int clearScreen(void);
static void compare(SDL_Surface *reference, int allowable_error);
static void compare2x(SDL_Surface *reference, int allowable_error);
static SDL_Texture *loadTestFace(void);
static bool isSupported(int code);
static bool hasDrawColor(void);

/**
 * Create software renderer for tests
 */
static void SDLCALL InitCreateRenderer(void **arg)
{
    int width = 320, height = 240;
    const char *renderer_name = NULL;
    renderer = NULL;
    window = SDL_CreateWindow("render_testCreateRenderer", width, height, 0);
    SDLTest_AssertPass("SDL_CreateWindow()");
    SDLTest_AssertCheck(window != NULL, "Check SDL_CreateWindow result");
    if (window == NULL) {
        return;
    }

    renderer = SDL_CreateRenderer(window, renderer_name);
    SDLTest_AssertPass("SDL_CreateRenderer()");
    SDLTest_AssertCheck(renderer != NULL, "Check SDL_CreateRenderer result: %s", renderer != NULL ? "success" : SDL_GetError());
    if (renderer == NULL) {
        SDL_DestroyWindow(window);
        return;
    }
}

/**
 * Destroy renderer for tests
 */
static void SDLCALL CleanupDestroyRenderer(void *arg)
{
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
        SDLTest_AssertPass("SDL_DestroyRenderer()");
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
        SDLTest_AssertPass("SDL_DestroyWindow");
    }
}

/**
 * Tests call to SDL_GetNumRenderDrivers
 *
 * \sa SDL_GetNumRenderDrivers
 */
static int SDLCALL render_testGetNumRenderDrivers(void *arg)
{
    int n;
    n = SDL_GetNumRenderDrivers();
    SDLTest_AssertCheck(n >= 1, "Number of renderers >= 1, reported as %i", n);
    return TEST_COMPLETED;
}

/**
 * Tests the SDL primitives for rendering.
 *
 * \sa SDL_SetRenderDrawColor
 * \sa SDL_RenderFillRect
 * \sa SDL_RenderLine
 *
 */
static int SDLCALL render_testPrimitives(void *arg)
{
    int ret;
    int x, y;
    SDL_FRect rect;
    SDL_Surface *referenceSurface = NULL;
    int checkFailCount1;
    int checkFailCount2;

    /* Clear surface. */
    clearScreen();

    /* Need drawcolor or just skip test. */
    SDLTest_AssertCheck(hasDrawColor(), "hasDrawColor");

    /* Draw a rectangle. */
    rect.x = 40.0f;
    rect.y = 0.0f;
    rect.w = 40.0f;
    rect.h = 80.0f;

    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 13, 73, 200, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, &rect))

    /* Draw a rectangle. */
    rect.x = 10.0f;
    rect.y = 10.0f;
    rect.w = 60.0f;
    rect.h = 40.0f;
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 200, 0, 100, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, &rect))

    /* Draw some points like so:
     * X.X.X.X..
     * .X.X.X.X.
     * X.X.X.X.. */
    checkFailCount1 = 0;
    checkFailCount2 = 0;
    for (y = 0; y < 3; y++) {
        for (x = y % 2; x < TESTRENDER_SCREEN_W; x += 2) {
            ret = SDL_SetRenderDrawColor(renderer, (Uint8)(x * y), (Uint8)(x * y / 2), (Uint8)(x * y / 3), SDL_ALPHA_OPAQUE);
            if (!ret) {
                checkFailCount1++;
            }

            ret = SDL_RenderPoint(renderer, (float)x, (float)y);
            if (!ret) {
                checkFailCount2++;
            }
        }
    }
    SDLTest_AssertCheck(checkFailCount1 == 0, "Validate results from calls to SDL_SetRenderDrawColor, expected: 0, got: %i", checkFailCount1);
    SDLTest_AssertCheck(checkFailCount2 == 0, "Validate results from calls to SDL_RenderPoint, expected: 0, got: %i", checkFailCount2);

    /* Draw some lines. */
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderLine, (renderer, 0.0f, 30.0f, TESTRENDER_SCREEN_W, 30.0f))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 55, 55, 5, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderLine, (renderer, 40.0f, 30.0f, 40.0f, 60.0f))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 5, 105, 105, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderLine, (renderer, 0.0f, 0.0f, 29.0f, 29.0f))
    CHECK_FUNC(SDL_RenderLine, (renderer, 29.0f, 30.0f, 0.0f, 59.0f))
    CHECK_FUNC(SDL_RenderLine, (renderer, 79.0f, 0.0f, 50.0f, 29.0f))
    CHECK_FUNC(SDL_RenderLine, (renderer, 79.0f, 59.0f, 50.0f, 30.0f))

    /* See if it's the same. */
    referenceSurface = SDLTest_ImagePrimitives();
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    /* Clean up. */
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;

    return TEST_COMPLETED;
}

/**
 * Tests the SDL primitives for rendering within a viewport.
 *
 * \sa SDL_SetRenderDrawColor
 * \sa SDL_RenderFillRect
 * \sa SDL_RenderLine
 *
 */
static int SDLCALL render_testPrimitivesWithViewport(void *arg)
{
    SDL_Rect viewport;
    SDL_Surface *surface;

    /* Clear surface. */
    clearScreen();

    viewport.x = 2;
    viewport.y = 2;
    viewport.w = 2;
    viewport.h = 2;
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, &viewport));

    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 255, 255, 255, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderLine, (renderer, 0.0f, 0.0f, 1.0f, 1.0f));

    viewport.x = 3;
    viewport.y = 3;
    viewport.w = 1;
    viewport.h = 1;
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, &viewport));

    surface = SDL_RenderReadPixels(renderer, NULL);
    if (surface) {
        Uint8 r, g, b, a;
        CHECK_FUNC(SDL_ReadSurfacePixel, (surface, 0, 0, &r, &g, &b, &a));
        SDLTest_AssertCheck(r == 0xFF && g == 0xFF && b == 0xFF && a == 0xFF, "Validate diagonal line drawing with viewport, expected 0xFFFFFFFF, got 0x%.2x%.2x%.2x%.2x", r, g, b, a);
        SDL_DestroySurface(surface);
    } else {
        SDLTest_AssertCheck(surface != NULL, "Validate result from SDL_RenderReadPixels, got NULL, %s", SDL_GetError());
    }

    return TEST_COMPLETED;
}

/**
 * Tests some blitting routines.
 *
 * \sa SDL_RenderTexture
 * \sa SDL_DestroyTexture
 */
static int SDLCALL render_testBlit(void *arg)
{
    int ret;
    SDL_FRect rect;
    SDL_Texture *tface;
    SDL_Surface *referenceSurface = NULL;
    int i, j, ni, nj;
    int checkFailCount1;

    /* Clear surface. */
    clearScreen();

    /* Need drawcolor or just skip test. */
    SDLTest_AssertCheck(hasDrawColor(), "hasDrawColor)");

    /* Create face surface. */
    tface = loadTestFace();
    SDLTest_AssertCheck(tface != NULL, "Verify loadTestFace() result");
    if (tface == NULL) {
        return TEST_ABORTED;
    }

    /* Constant values. */
    rect.w = (float)tface->w;
    rect.h = (float)tface->h;
    ni = TESTRENDER_SCREEN_W - tface->w;
    nj = TESTRENDER_SCREEN_H - tface->h;

    /* Loop blit. */
    checkFailCount1 = 0;
    for (j = 0; j <= nj; j += 4) {
        for (i = 0; i <= ni; i += 4) {
            /* Blitting. */
            rect.x = (float)i;
            rect.y = (float)j;
            ret = SDL_RenderTexture(renderer, tface, NULL, &rect);
            if (!ret) {
                checkFailCount1++;
            }
        }
    }
    SDLTest_AssertCheck(checkFailCount1 == 0, "Validate results from calls to SDL_RenderTexture, expected: 0, got: %i", checkFailCount1);

    /* See if it's the same */
    referenceSurface = SDLTest_ImageBlit();
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    /* Clean up. */
    SDL_DestroyTexture(tface);
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;

    return TEST_COMPLETED;
}

/**
 * Tests tiled blitting routines.
 */
static int SDLCALL render_testBlitTiled(void *arg)
{
    int ret;
    SDL_FRect rect;
    SDL_Texture *tface;
    SDL_Surface *referenceSurface = NULL;
    SDL_Surface *referenceSurface2x = NULL;

    /* Create face surface. */
    tface = loadTestFace();
    SDLTest_AssertCheck(tface != NULL, "Verify loadTestFace() result");
    if (tface == NULL) {
        return TEST_ABORTED;
    }
    SDL_SetTextureScaleMode(tface, SDL_SCALEMODE_NEAREST);  /* So 2x scaling is pixel perfect */

    /* Tiled blit - 1.0 scale */
    {
        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTextureTiled(renderer, tface, NULL, 1.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTextureTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        referenceSurface = SDLTest_ImageBlitTiled();
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Tiled blit - 2.0 scale */
    {
        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W * 2;
        rect.h = (float)TESTRENDER_SCREEN_H * 2;
        ret = SDL_RenderTextureTiled(renderer, tface, NULL, 2.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTextureTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        referenceSurface2x = SDL_CreateSurface(referenceSurface->w * 2, referenceSurface->h * 2, referenceSurface->format);
        SDL_BlitSurfaceScaled(referenceSurface, NULL, referenceSurface2x, NULL, SDL_SCALEMODE_NEAREST);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_BlitSurfaceScaled, expected: 0, got: %i", ret);
        compare2x(referenceSurface2x, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Clean up. */
    SDL_DestroyTexture(tface);
    SDL_DestroySurface(referenceSurface);
    SDL_DestroySurface(referenceSurface2x);
    referenceSurface = NULL;

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
static int SDLCALL render_testBlit9Grid(void *arg)
{
    SDL_Surface *referenceSurface = NULL;
    SDL_Surface *source = NULL;
    SDL_Texture *texture;
    int x, y;
    SDL_FRect rect;
    int ret = 0;

    /* Create source surface */
    source = SDL_CreateSurface(3, 3, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(source != NULL, "Verify source surface is not NULL");
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 3; ++x) {
            SDL_WriteSurfacePixel(source, x, y, (Uint8)((1 + x) * COLOR_SEPARATION), (Uint8)((1 + y) * COLOR_SEPARATION), 0, 255);
        }
    }
    texture = SDL_CreateTextureFromSurface(renderer, source);
    SDLTest_AssertCheck(texture != NULL, "Verify source texture is not NULL");
    ret = SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_SetTextureScaleMode, expected: true, got: %i", ret);

    /* 9-grid blit - 1.0 scale */
    {
        SDLTest_Log("9-grid blit - 1.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 1, 1, 1);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9Grid(renderer, texture, NULL, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9Grid, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* 9-grid blit - 2.0 scale */
    {
        SDLTest_Log("9-grid blit - 2.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 2, 2, 2);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9Grid(renderer, texture, NULL, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9Grid, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Clean up. */
    SDL_DestroySurface(source);
    SDL_DestroyTexture(texture);

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

    texture = SDL_CreateTextureFromSurface(renderer, source);
    SDLTest_AssertCheck(texture != NULL, "Verify source texture is not NULL");
    ret = SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_SetTextureScaleMode, expected: true, got: %i", ret);

    /* complex 9-grid blit - 1.0 scale */
    {
        SDLTest_Log("complex 9-grid blit - 1.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 2, 1, 2);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9Grid(renderer, texture, NULL, 1.0f, 2.0f, 1.0f, 2.0f, 1.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9Grid, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* complex 9-grid blit - 2.0 scale */
    {
        SDLTest_Log("complex 9-grid blit - 2.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 4, 2, 4);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9Grid(renderer, texture, NULL, 1.0f, 2.0f, 1.0f, 2.0f, 2.0f, &rect);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9Grid, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Clean up. */
    SDL_DestroySurface(referenceSurface);
    SDL_DestroySurface(source);
    SDL_DestroyTexture(texture);

    return TEST_COMPLETED;
}

/**
 *  Tests tiled 9-grid blitting.
 */
static int SDLCALL render_testBlit9GridTiled(void *arg)
{
    SDL_Surface *referenceSurface = NULL;
    SDL_Surface *source = NULL;
    SDL_Texture *texture;
    int x, y;
    SDL_FRect rect;
    int ret = 0;

    /* Create source surface */
    source = SDL_CreateSurface(3, 3, SDL_PIXELFORMAT_RGBA32);
    SDLTest_AssertCheck(source != NULL, "Verify source surface is not NULL");
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 3; ++x) {
            SDL_WriteSurfacePixel(source, x, y, (Uint8)((1 + x) * COLOR_SEPARATION), (Uint8)((1 + y) * COLOR_SEPARATION), 0, 255);
        }
    }
    texture = SDL_CreateTextureFromSurface(renderer, source);
    SDLTest_AssertCheck(texture != NULL, "Verify source texture is not NULL");
    ret = SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_SetTextureScaleMode, expected: true, got: %i", ret);

    /* Tiled 9-grid blit - 1.0 scale */
    {
        SDLTest_Log("tiled 9-grid blit - 1.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 1, 1, 1);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9GridTiled(renderer, texture, NULL, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, &rect, 1.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9GridTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Tiled 9-grid blit - 2.0 scale */
    {
        SDLTest_Log("tiled 9-grid blit - 2.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 2, 2, 2);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9GridTiled(renderer, texture, NULL, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, &rect, 2.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9GridTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Clean up. */
    SDL_DestroySurface(source);
    SDL_DestroyTexture(texture);

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

    texture = SDL_CreateTextureFromSurface(renderer, source);
    SDLTest_AssertCheck(texture != NULL, "Verify source texture is not NULL");
    ret = SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_SetTextureScaleMode, expected: true, got: %i", ret);

    /* complex tiled 9-grid blit - 1.0 scale */
    {
        SDLTest_Log("complex tiled 9-grid blit - 1.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 1, 2, 1, 2);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9GridTiled(renderer, texture, NULL, 1.0f, 2.0f, 1.0f, 2.0f, 1.0f, &rect, 1.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9GridTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* complex tiled 9-grid blit - 2.0 scale */
    {
        SDLTest_Log("complex tiled 9-grid blit - 2.0 scale");
        /* Create reference surface */
        SDL_DestroySurface(referenceSurface);
        referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, SDL_PIXELFORMAT_RGBA32);
        SDLTest_AssertCheck(referenceSurface != NULL, "Verify reference surface is not NULL");
        Fill9GridReferenceSurface(referenceSurface, 2, 4, 2, 4);

        /* Clear surface. */
        clearScreen();

        /* Tiled blit. */
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = (float)TESTRENDER_SCREEN_W;
        rect.h = (float)TESTRENDER_SCREEN_H;
        ret = SDL_RenderTexture9GridTiled(renderer, texture, NULL, 1.0f, 2.0f, 1.0f, 2.0f, 2.0f, &rect, 2.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from call to SDL_RenderTexture9GridTiled, expected: true, got: %i", ret);

        /* See if it's the same */
        compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

        /* Make current */
        SDL_RenderPresent(renderer);
    }

    /* Clean up. */
    SDL_DestroySurface(referenceSurface);
    SDL_DestroySurface(source);
    SDL_DestroyTexture(texture);

    return TEST_COMPLETED;
}

/**
 * Blits doing color tests.
 *
 * \sa SDL_SetTextureColorMod
 * \sa SDL_RenderTexture
 * \sa SDL_DestroyTexture
 */
static int SDLCALL render_testBlitColor(void *arg)
{
    int ret;
    SDL_FRect rect;
    SDL_Texture *tface;
    SDL_Surface *referenceSurface = NULL;
    int i, j, ni, nj;
    int checkFailCount1;
    int checkFailCount2;

    /* Clear surface. */
    clearScreen();

    /* Create face surface. */
    tface = loadTestFace();
    SDLTest_AssertCheck(tface != NULL, "Verify loadTestFace() result");
    if (tface == NULL) {
        return TEST_ABORTED;
    }

    /* Constant values. */
    rect.w = (float)tface->w;
    rect.h = (float)tface->h;
    ni = TESTRENDER_SCREEN_W - tface->w;
    nj = TESTRENDER_SCREEN_H - tface->h;

    /* Test blitting with color mod. */
    checkFailCount1 = 0;
    checkFailCount2 = 0;
    for (j = 0; j <= nj; j += 4) {
        for (i = 0; i <= ni; i += 4) {
            /* Set color mod. */
            ret = SDL_SetTextureColorMod(tface, (Uint8)((255 / nj) * j), (Uint8)((255 / ni) * i), (Uint8)((255 / nj) * j));
            if (!ret) {
                checkFailCount1++;
            }

            /* Blitting. */
            rect.x = (float)i;
            rect.y = (float)j;
            ret = SDL_RenderTexture(renderer, tface, NULL, &rect);
            if (!ret) {
                checkFailCount2++;
            }
        }
    }
    SDLTest_AssertCheck(checkFailCount1 == 0, "Validate results from calls to SDL_SetTextureColorMod, expected: 0, got: %i", checkFailCount1);
    SDLTest_AssertCheck(checkFailCount2 == 0, "Validate results from calls to SDL_RenderTexture, expected: 0, got: %i", checkFailCount2);

    /* See if it's the same. */
    referenceSurface = SDLTest_ImageBlitColor();
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    /* Clean up. */
    SDL_DestroyTexture(tface);
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;

    return TEST_COMPLETED;
}

typedef enum TestRenderOperation
{
    TEST_RENDER_POINT,
    TEST_RENDER_LINE,
    TEST_RENDER_RECT,
    TEST_RENDER_COPY_XRGB,
    TEST_RENDER_COPY_ARGB,
} TestRenderOperation;

/**
 * Helper that tests a specific operation and blend mode, -1 for color mod, -2 for alpha mod
 */
static void testBlendModeOperation(TestRenderOperation op, int mode, SDL_PixelFormat dst_format)
{
    /* Allow up to 2 delta from theoretical value to account for rounding error.
     * We allow 2 rounding errors because the software renderer breaks drawing operations into alpha multiplication and a separate blend operation.
     */
    const int MAXIMUM_ERROR = 2;
    int ret;
    SDL_Texture *src = NULL;
    SDL_Texture *dst;
    SDL_Surface *result;
    Uint8 srcR = 10, srcG = 128, srcB = 240, srcA = 100;
    Uint8 dstR = 128, dstG = 128, dstB = 128, dstA = 128;
    Uint8 expectedR, expectedG, expectedB, expectedA;
    Uint8 actualR, actualG, actualB, actualA;
    int deltaR, deltaG, deltaB, deltaA;
    const char *operation = "UNKNOWN";
    const char *mode_name = "UNKNOWN";

    /* Create dst surface */
    dst = SDL_CreateTexture(renderer, dst_format, SDL_TEXTUREACCESS_TARGET, 3, 3);
    SDLTest_AssertCheck(dst != NULL, "Verify dst surface is not NULL");
    if (dst == NULL) {
        return;
    }
    if (SDL_ISPIXELFORMAT_ALPHA(dst_format)) {
        SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
        ret = SDL_GetTextureBlendMode(dst, &blendMode);
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_GetTextureBlendMode(), expected: true, got: %i", ret);
        SDLTest_AssertCheck(blendMode == SDL_BLENDMODE_BLEND, "Verify alpha texture blend mode, expected %d, got %" SDL_PRIu32, SDL_BLENDMODE_BLEND, blendMode);
    }

    /* Set as render target */
    SDL_SetRenderTarget(renderer, dst);

    /* Clear surface. */
    if (!SDL_ISPIXELFORMAT_ALPHA(dst_format)) {
        dstA = 255;
    }
    ret = SDL_SetRenderDrawColor(renderer, dstR, dstG, dstB, dstA);
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetRenderDrawColor(), expected: true, got: %i", ret);
    ret = SDL_RenderClear(renderer);
    SDLTest_AssertPass("Call to SDL_RenderClear()");
    SDLTest_AssertCheck(ret == true, "Verify result from SDL_RenderClear, expected: true, got: %i", ret);

    if (op == TEST_RENDER_COPY_XRGB || op == TEST_RENDER_COPY_ARGB) {
        Uint8 pixels[4];

        /* Create src surface */
        src = SDL_CreateTexture(renderer, op == TEST_RENDER_COPY_XRGB ? SDL_PIXELFORMAT_RGBX32 : SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
        SDLTest_AssertCheck(src != NULL, "Verify src surface is not NULL");
        if (src == NULL) {
            return;
        }

        /* Clear surface. */
        if (op == TEST_RENDER_COPY_XRGB) {
            srcA = 255;
        }
        pixels[0] = srcR;
        pixels[1] = srcG;
        pixels[2] = srcB;
        pixels[3] = srcA;
        SDL_UpdateTexture(src, NULL, pixels, sizeof(pixels));

        /* Set blend mode. */
        if (mode >= 0) {
            ret = SDL_SetTextureBlendMode(src, (SDL_BlendMode)mode);
            SDLTest_AssertPass("Call to SDL_SetTextureBlendMode()");
            SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetTextureBlendMode(..., %i), expected: true, got: %i", mode, ret);
        } else {
            ret = SDL_SetTextureBlendMode(src, SDL_BLENDMODE_BLEND);
            SDLTest_AssertPass("Call to SDL_SetTextureBlendMode()");
            SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetTextureBlendMode(..., %i), expected: true, got: %i", mode, ret);
        }
    } else {
        /* Set draw color */
        ret = SDL_SetRenderDrawColor(renderer, srcR, srcG, srcB, srcA);
        SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetRenderDrawColor(), expected: true, got: %i", ret);

        /* Set blend mode. */
        if (mode >= 0) {
            ret = SDL_SetRenderDrawBlendMode(renderer, (SDL_BlendMode)mode);
            SDLTest_AssertPass("Call to SDL_SetRenderDrawBlendMode()");
            SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetRenderDrawBlendMode(..., %i), expected: true, got: %i", mode, ret);
        } else {
            ret = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDLTest_AssertPass("Call to SDL_SetRenderDrawBlendMode()");
            SDLTest_AssertCheck(ret == true, "Verify result from SDL_SetRenderDrawBlendMode(..., %i), expected: true, got: %i", mode, ret);
        }
    }

    /* Test blend mode. */
#define FLOAT(X)    ((float)X / 255.0f)
    switch (mode) {
    case -1:
        mode_name = "color modulation";
        ret = SDL_SetTextureColorMod(src, srcR, srcG, srcB);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_SetTextureColorMod, expected: true, got: %i", ret);
        expectedR = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcR) * FLOAT(srcR)) * FLOAT(srcA) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcG) * FLOAT(srcG)) * FLOAT(srcA) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcB) * FLOAT(srcB)) * FLOAT(srcA) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case -2:
        mode_name = "alpha modulation";
        ret = SDL_SetTextureAlphaMod(src, srcA);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_SetTextureAlphaMod, expected: true, got: %i", ret);
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstR) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstG) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * (FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstB) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp((FLOAT(srcA) * FLOAT(srcA)) + FLOAT(dstA) * (1.0f - (FLOAT(srcA) * FLOAT(srcA))), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_NONE:
        mode_name = "SDL_BLENDMODE_NONE";
        expectedR = srcR;
        expectedG = srcG;
        expectedB = srcB;
        expectedA = SDL_ISPIXELFORMAT_ALPHA(dst_format) ? srcA : 255;
        break;
    case SDL_BLENDMODE_BLEND:
        mode_name = "SDL_BLENDMODE_BLEND";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(srcA) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(srcA) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(srcA) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_BLEND_PREMULTIPLIED:
        mode_name = "SDL_BLENDMODE_BLEND_PREMULTIPLIED";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcA) + FLOAT(dstA) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        break;
    case SDL_BLENDMODE_ADD:
        mode_name = "SDL_BLENDMODE_ADD";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(srcA) + FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(srcA) + FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(srcA) + FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_ADD_PREMULTIPLIED:
        mode_name = "SDL_BLENDMODE_ADD_PREMULTIPLIED";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) + FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) + FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) + FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_MOD:
        mode_name = "SDL_BLENDMODE_MOD";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(dstR), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(dstG), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(dstB), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    case SDL_BLENDMODE_MUL:
        mode_name = "SDL_BLENDMODE_MUL";
        expectedR = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcR) * FLOAT(dstR) + FLOAT(dstR) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedG = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcG) * FLOAT(dstG) + FLOAT(dstG) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedB = (Uint8)SDL_roundf(SDL_clamp(FLOAT(srcB) * FLOAT(dstB) + FLOAT(dstB) * (1.0f - FLOAT(srcA)), 0.0f, 1.0f) * 255.0f);
        expectedA = dstA;
        break;
    default:
        SDLTest_LogError("Invalid blending mode: %d", mode);
        return;
    }

    switch (op) {
    case TEST_RENDER_POINT:
        operation = "render point";
        ret = SDL_RenderPoint(renderer, 0.0f, 0.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_RenderPoint, expected: 0, got: %i", ret);
        break;
    case TEST_RENDER_LINE:
        operation = "render line";
        ret = SDL_RenderLine(renderer, 0.0f, 0.0f, 2.0f, 2.0f);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_RenderLine, expected: true, got: %i", ret);
        break;
    case TEST_RENDER_RECT:
        operation = "render rect";
        ret = SDL_RenderFillRect(renderer, NULL);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_RenderFillRect, expected: 0, got: %i", ret);
        break;
    case TEST_RENDER_COPY_XRGB:
    case TEST_RENDER_COPY_ARGB:
        operation = (op == TEST_RENDER_COPY_XRGB) ? "render XRGB" : "render ARGB";
        ret = SDL_RenderTexture(renderer, src, NULL, NULL);
        SDLTest_AssertCheck(ret == true, "Validate results from calls to SDL_RenderTexture, expected: true, got: %i", ret);
        break;
    default:
        SDLTest_LogError("Invalid blending operation: %d", op);
        return;
    }

    result = SDL_RenderReadPixels(renderer, NULL);
    SDL_ReadSurfacePixel(result, 0, 0, &actualR, &actualG, &actualB, &actualA);
    deltaR = SDL_abs((int)actualR - expectedR);
    deltaG = SDL_abs((int)actualG - expectedG);
    deltaB = SDL_abs((int)actualB - expectedB);
    deltaA = SDL_abs((int)actualA - expectedA);
    SDLTest_AssertCheck(
        deltaR <= MAXIMUM_ERROR &&
        deltaG <= MAXIMUM_ERROR &&
        deltaB <= MAXIMUM_ERROR &&
        deltaA <= MAXIMUM_ERROR,
        "Checking %s %s operation results, expected %d,%d,%d,%d, got %d,%d,%d,%d",
            operation, mode_name,
            expectedR, expectedG, expectedB, expectedA, actualR, actualG, actualB, actualA);

    /* Clean up */
    SDL_DestroySurface(result);
    SDL_DestroyTexture(src);
    SDL_DestroyTexture(dst);
}

static void testBlendMode(int mode)
{
    const TestRenderOperation operations[] = {
        TEST_RENDER_POINT,
        TEST_RENDER_LINE,
        TEST_RENDER_RECT,
        TEST_RENDER_COPY_XRGB,
        TEST_RENDER_COPY_ARGB
    };
    const SDL_PixelFormat dst_formats[] = {
        SDL_PIXELFORMAT_XRGB8888, SDL_PIXELFORMAT_ARGB8888
    };
    int i, j;

    for (i = 0; i < SDL_arraysize(operations); ++i) {
        for (j = 0; j < SDL_arraysize(dst_formats); ++j) {
            TestRenderOperation op = operations[i];

            if (mode < 0) {
                if (op != TEST_RENDER_COPY_XRGB && op != TEST_RENDER_COPY_ARGB) {
                    /* Unsupported mode for this operation */
                    continue;
                }
            }
            testBlendModeOperation(op, mode, dst_formats[j]);
        }
    }
}

/**
 * Tests render operations with blend modes
 */
static int SDLCALL render_testBlendModes(void *arg)
{
    testBlendMode(-1);
    testBlendMode(-2);
    testBlendMode(SDL_BLENDMODE_NONE);
    testBlendMode(SDL_BLENDMODE_BLEND);
    testBlendMode(SDL_BLENDMODE_BLEND_PREMULTIPLIED);
    testBlendMode(SDL_BLENDMODE_ADD);
    testBlendMode(SDL_BLENDMODE_ADD_PREMULTIPLIED);
    testBlendMode(SDL_BLENDMODE_MOD);
    testBlendMode(SDL_BLENDMODE_MUL);

    return TEST_COMPLETED;
}

/**
 * Test viewport
 */
static int SDLCALL render_testViewport(void *arg)
{
    SDL_Surface *referenceSurface;
    SDL_Rect viewport;

    viewport.x = TESTRENDER_SCREEN_W / 3;
    viewport.y = TESTRENDER_SCREEN_H / 3;
    viewport.w = TESTRENDER_SCREEN_W / 2;
    viewport.h = TESTRENDER_SCREEN_H / 2;

    /* Create expected result */
    referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, RENDER_COMPARE_FORMAT);
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_CLEAR))
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, &viewport, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the viewport and do a fill operation */
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, &viewport))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, NULL))
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, NULL))

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /*
     * Verify that clear ignores the viewport
     */

    /* Create expected result */
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the viewport and do a clear operation */
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, &viewport))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderClear, (renderer))
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, NULL))

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    SDL_DestroySurface(referenceSurface);

    return TEST_COMPLETED;
}

static int SDLCALL render_testRGBSurfaceNoAlpha(void* arg)
{
    SDL_Surface *surface;
    SDL_Renderer *software_renderer;
    SDL_Surface *surface2;
    SDL_Texture *texture2;
    bool result;
    SDL_FRect dest_rect;
    SDL_FPoint point;
    const SDL_PixelFormatDetails *format_details;
    Uint8 r, g, b, a;

    SDLTest_AssertPass("About to call SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBX32)");
    surface = SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBX32);
    SDLTest_AssertCheck(surface != NULL, "Returned surface must be not NULL");
    if (surface == NULL) {
        return TEST_ABORTED;
    }

    SDLTest_AssertPass("About to call SDL_GetPixelFormatDetails(surface->format)");
    format_details = SDL_GetPixelFormatDetails(surface->format);
    SDLTest_AssertCheck(format_details != NULL, "Result must be non-NULL, is %p", format_details);
    if (format_details == NULL) {
        SDL_DestroySurface(surface);
        return TEST_ABORTED;
    }

    SDLTest_AssertCheck(format_details->bits_per_pixel == 32, "format_details->bits_per_pixel is %d, should be %d", format_details->bits_per_pixel, 32);
    SDLTest_AssertCheck(format_details->bytes_per_pixel == 4, "format_details->bytes_per_pixel is %d, should be %d", format_details->bytes_per_pixel, 4);

    SDLTest_AssertPass("About to call SDL_CreateSoftwareRenderer(surface)");
    software_renderer = SDL_CreateSoftwareRenderer(surface);
    SDLTest_AssertCheck(software_renderer != NULL, "Returned renderer must be not NULL");
    if (software_renderer == NULL) {
        SDL_DestroySurface(surface);
        return TEST_ABORTED;
    }

    SDLTest_AssertPass("About to call SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBX32)");
    surface2 = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBX32);
    SDLTest_AssertCheck(surface2 != NULL, "Returned surface must be not NULL");
    if (surface2 == NULL) {
        SDL_DestroySurface(surface);
        SDL_DestroyRenderer(software_renderer);
        return TEST_ABORTED;
    }

    SDLTest_AssertPass("About to call SDL_FillRect(surface2, NULL, 0)");
    result = SDL_FillSurfaceRect(surface2, NULL, SDL_MapRGB(SDL_GetPixelFormatDetails(surface2->format), NULL, 0, 0, 0));
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);

    SDLTest_AssertPass("About to call SDL_CreateTextureFromSurface(software_renderer, surface2)");
    texture2 = SDL_CreateTextureFromSurface(software_renderer, surface2);
    SDLTest_AssertCheck(texture2 != NULL, "Returned texture is not NULL");

    SDLTest_AssertPass("About to call SDL_SetRenderDrawColor(renderer, 0xaa, 0xbb, 0xcc, 0x0)");
    result = SDL_SetRenderDrawColor(software_renderer, 0xaa, 0xbb, 0xcc, 0x0);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);

    SDLTest_AssertPass("About to call SDL_RenderClear(renderer)");
    result = SDL_RenderClear(software_renderer);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);

    SDLTest_AssertPass("About to call SDL_SetRenderDrawColor(renderer, 0x0, 0x0, 0x0, 0x0)");
    result = SDL_SetRenderDrawColor(software_renderer, 0x0, 0x0, 0x0, 0x0);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);

    dest_rect.x = 32;
    dest_rect.y = 32;
    dest_rect.w = (float)surface2->w;
    dest_rect.h = (float)surface2->h;
    point.x = 0;
    point.y = 0;
    SDLTest_AssertPass("About to call SDL_RenderCopy(software_renderer, texture, NULL, &{%g, %g, %g, %g})",
        dest_rect.x, dest_rect.h, dest_rect.w, dest_rect.h);
    result = SDL_RenderTextureRotated(software_renderer, texture2, NULL, &dest_rect, 180, &point, SDL_FLIP_NONE);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);

    SDLTest_AssertPass("About to call SDL_RenderPresent(software_renderer)");
    SDL_RenderPresent(software_renderer);

    SDLTest_AssertPass("About to call SDL_ReadSurfacePixel(0, 0)");
    result = SDL_ReadSurfacePixel(surface, 0, 0, &r, &g, &b, &a);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);
    SDLTest_AssertCheck(r == 0xaa && g == 0xbb && b == 0xcc && a == SDL_ALPHA_OPAQUE,
        "Pixel at (0, 0) is {0x%02x,0x%02x,0x%02x,0x%02x}, should be {0x%02x,0x%02x,0x%02x,0x%02x}",
        r, g, b, a, 0xaa, 0xbb, 0xcc, SDL_ALPHA_OPAQUE);

    SDLTest_AssertPass("About to call SDL_ReadSurfacePixel(15, 15)");
    result = SDL_ReadSurfacePixel(surface, 15, 15, &r, &g, &b, &a);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);
    SDLTest_AssertCheck(r == 0xaa && g == 0xbb && b == 0xcc && a == SDL_ALPHA_OPAQUE,
        "Pixel at (0, 0) is {0x%02x,0x%02x,0x%02x,0x%02x}, should be {0x%02x,0x%02x,0x%02x,0x%02x}",
        r, g, b, a, 0xaa, 0xbb, 0xcc, SDL_ALPHA_OPAQUE);

    SDLTest_AssertPass("About to call SDL_ReadSurfacePixel(16, 16)");
    result = SDL_ReadSurfacePixel(surface, 16, 16, &r, &g, &b, &a);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);
    SDLTest_AssertCheck(r == 0x00 && g == 0x00 && b == 0x00 && a == SDL_ALPHA_OPAQUE,
        "Pixel at (0, 0) is {0x%02x,0x%02x,0x%02x,0x%02x}, should be {0x%02x,0x%02x,0x%02x,0x%02x}",
        r, g, b, a, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);

    SDLTest_AssertPass("About to call SDL_ReadSurfacePixel(31, 31)");
    result = SDL_ReadSurfacePixel(surface, 31, 31, &r, &g, &b, &a);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);
    SDLTest_AssertCheck(r == 0x00 && g == 0x00 && b == 0x00 && a == SDL_ALPHA_OPAQUE,
        "Pixel at (0, 0) is {0x%02x,0x%02x,0x%02x,0x%02x}, should be {0x%02x,0x%02x,0x%02x,0x%02x}",
        r, g, b, a, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);

    SDLTest_AssertPass("About to call SDL_ReadSurfacePixel(32, 32)");
    result = SDL_ReadSurfacePixel(surface, 32, 32, &r, &g, &b, &a);
    SDLTest_AssertCheck(result == true, "Result is %d, should be %d", result, true);
    SDLTest_AssertCheck(r == 0xaa && g == 0xbb && b == 0xcc && a == SDL_ALPHA_OPAQUE,
        "Pixel at (0, 0) is {0x%02x,0x%02x,0x%02x,0x%02x}, should be {0x%02x,0x%02x,0x%02x,0x%02x}",
        r, g, b, a, 0xaa, 0xbb, 0xcc, SDL_ALPHA_OPAQUE);

    SDL_DestroyTexture(texture2);
    SDL_DestroySurface(surface2);
    SDL_DestroyRenderer(software_renderer);
    SDL_DestroySurface(surface);
    return TEST_COMPLETED;
}

/**
 * Test clip rect
 */
static int SDLCALL render_testClipRect(void *arg)
{
    SDL_Surface *referenceSurface;
    SDL_Rect cliprect;

    cliprect.x = TESTRENDER_SCREEN_W / 3;
    cliprect.y = TESTRENDER_SCREEN_H / 3;
    cliprect.w = TESTRENDER_SCREEN_W / 2;
    cliprect.h = TESTRENDER_SCREEN_H / 2;

    /* Create expected result */
    referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, RENDER_COMPARE_FORMAT);
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_CLEAR))
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, &cliprect, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the cliprect and do a fill operation */
    CHECK_FUNC(SDL_SetRenderClipRect, (renderer, &cliprect))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, NULL))
    CHECK_FUNC(SDL_SetRenderClipRect, (renderer, NULL))

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /*
     * Verify that clear ignores the cliprect
     */

    /* Create expected result */
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the cliprect and do a clear operation */
    CHECK_FUNC(SDL_SetRenderClipRect, (renderer, &cliprect))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderClear, (renderer))
    CHECK_FUNC(SDL_SetRenderClipRect, (renderer, NULL))

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    SDL_DestroySurface(referenceSurface);

    return TEST_COMPLETED;
}

/**
 * Test logical size
 */
static int SDLCALL render_testLogicalSize(void *arg)
{
    SDL_Surface *referenceSurface;
    SDL_Rect viewport;
    SDL_FRect rect;
    int w, h;
    int set_w, set_h;
    SDL_RendererLogicalPresentation set_presentation_mode;
    SDL_FRect set_rect;
    const int factor = 2;

    viewport.x = ((TESTRENDER_SCREEN_W / 4) / factor) * factor;
    viewport.y = ((TESTRENDER_SCREEN_H / 4) / factor) * factor;
    viewport.w = ((TESTRENDER_SCREEN_W / 2) / factor) * factor;
    viewport.h = ((TESTRENDER_SCREEN_H / 2) / factor) * factor;

    /* Create expected result */
    referenceSurface = SDL_CreateSurface(TESTRENDER_SCREEN_W, TESTRENDER_SCREEN_H, RENDER_COMPARE_FORMAT);
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_CLEAR))
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, &viewport, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the logical size and do a fill operation */
    CHECK_FUNC(SDL_GetCurrentRenderOutputSize, (renderer, &w, &h))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer, w / factor, h / factor, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    CHECK_FUNC(SDL_GetRenderLogicalPresentation, (renderer, &set_w, &set_h, &set_presentation_mode))
    SDLTest_AssertCheck(
        set_w == (w / factor) &&
        set_h == (h / factor) &&
        set_presentation_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX,
        "Validate result from SDL_GetRenderLogicalPresentation, got %d, %d, %d", set_w, set_h, set_presentation_mode);
    CHECK_FUNC(SDL_GetRenderLogicalPresentationRect, (renderer, &set_rect))
    SDLTest_AssertCheck(
        set_rect.x == 0.0f &&
        set_rect.y == 0.0f &&
        set_rect.w == 320.0f &&
        set_rect.h == 240.0f,
        "Validate result from SDL_GetRenderLogicalPresentationRect, got {%g, %g, %gx%g}", set_rect.x, set_rect.y, set_rect.w, set_rect.h);
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    rect.x = (float)viewport.x / factor;
    rect.y = (float)viewport.y / factor;
    rect.w = (float)viewport.w / factor;
    rect.h = (float)viewport.h / factor;
    CHECK_FUNC(SDL_RenderFillRect, (renderer, &rect))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED))
    CHECK_FUNC(SDL_GetRenderLogicalPresentation, (renderer, &set_w, &set_h, &set_presentation_mode))
    SDLTest_AssertCheck(
        set_w == 0 &&
        set_h == 0 &&
        set_presentation_mode == SDL_LOGICAL_PRESENTATION_DISABLED,
        "Validate result from SDL_GetRenderLogicalPresentation, got %d, %d, %d", set_w, set_h, set_presentation_mode);
    CHECK_FUNC(SDL_GetRenderLogicalPresentationRect, (renderer, &set_rect))
    SDLTest_AssertCheck(
        set_rect.x == 0.0f &&
        set_rect.y == 0.0f &&
        set_rect.w == 320.0f &&
        set_rect.h == 240.0f,
        "Validate result from SDL_GetRenderLogicalPresentationRect, got {%g, %g, %gx%g}", set_rect.x, set_rect.y, set_rect.w, set_rect.h);

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Clear surface. */
    clearScreen();

    /* Set the logical size and viewport and do a fill operation */
    CHECK_FUNC(SDL_GetCurrentRenderOutputSize, (renderer, &w, &h))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer, w / factor, h / factor, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    viewport.x = (TESTRENDER_SCREEN_W / 4) / factor;
    viewport.y = (TESTRENDER_SCREEN_H / 4) / factor;
    viewport.w = (TESTRENDER_SCREEN_W / 2) / factor;
    viewport.h = (TESTRENDER_SCREEN_H / 2) / factor;
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, &viewport))
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, NULL))
    CHECK_FUNC(SDL_SetRenderViewport, (renderer, NULL))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED))

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /*
     * Test a logical size that isn't the same aspect ratio as the window
     */

    viewport.x = (TESTRENDER_SCREEN_W / 4);
    viewport.y = 0;
    viewport.w = TESTRENDER_SCREEN_W;
    viewport.h = TESTRENDER_SCREEN_H;

    /* Create expected result */
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, NULL, RENDER_COLOR_CLEAR))
    CHECK_FUNC(SDL_FillSurfaceRect, (referenceSurface, &viewport, RENDER_COLOR_GREEN))

    /* Clear surface. */
    clearScreen();

    /* Set the logical size and do a fill operation */
    CHECK_FUNC(SDL_GetCurrentRenderOutputSize, (renderer, &w, &h))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer,
                                           w - 2 * (TESTRENDER_SCREEN_W / 4),
                                           h,
                                           SDL_LOGICAL_PRESENTATION_LETTERBOX))
    CHECK_FUNC(SDL_GetRenderLogicalPresentation, (renderer, &set_w, &set_h, &set_presentation_mode))
    SDLTest_AssertCheck(
        set_w == w - 2 * (TESTRENDER_SCREEN_W / 4) &&
        set_h == h &&
        set_presentation_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX,
        "Validate result from SDL_GetRenderLogicalPresentation, got %d, %d, %d", set_w, set_h, set_presentation_mode);
    CHECK_FUNC(SDL_GetRenderLogicalPresentationRect, (renderer, &set_rect))
    SDLTest_AssertCheck(
        set_rect.x == 20.0f &&
        set_rect.y == 0.0f &&
        set_rect.w == 280.0f &&
        set_rect.h == 240.0f,
        "Validate result from SDL_GetRenderLogicalPresentationRect, got {%g, %g, %gx%g}", set_rect.x, set_rect.y, set_rect.w, set_rect.h);
    CHECK_FUNC(SDL_SetRenderDrawColor, (renderer, 0, 255, 0, SDL_ALPHA_OPAQUE))
    CHECK_FUNC(SDL_RenderFillRect, (renderer, NULL))
    CHECK_FUNC(SDL_SetRenderLogicalPresentation, (renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED))
    CHECK_FUNC(SDL_GetRenderLogicalPresentation, (renderer, &set_w, &set_h, &set_presentation_mode))
    SDLTest_AssertCheck(
        set_w == 0 &&
        set_h == 0 &&
        set_presentation_mode == SDL_LOGICAL_PRESENTATION_DISABLED,
        "Validate result from SDL_GetRenderLogicalPresentation, got %d, %d, %d", set_w, set_h, set_presentation_mode);
    CHECK_FUNC(SDL_GetRenderLogicalPresentationRect, (renderer, &set_rect))
    SDLTest_AssertCheck(
        set_rect.x == 0.0f &&
        set_rect.y == 0.0f &&
        set_rect.w == 320.0f &&
        set_rect.h == 240.0f,
        "Validate result from SDL_GetRenderLogicalPresentationRect, got {%g, %g, %gx%g}", set_rect.x, set_rect.y, set_rect.w, set_rect.h);

    /* Check to see if final image matches. */
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Clear surface. */
    clearScreen();

    /* Make current */
    SDL_RenderPresent(renderer);

    SDL_DestroySurface(referenceSurface);

    return TEST_COMPLETED;
}

/**
 * @brief Tests setting and getting texture scale mode.
 *
 * \sa
 * http://wiki.libsdl.org/SDL2/SDL_SetTextureScaleMode
 * http://wiki.libsdl.org/SDL2/SDL_GetTextureScaleMode
 */
static int SDLCALL render_testGetSetTextureScaleMode(void *arg)
{
    const struct {
        const char *name;
        SDL_ScaleMode mode;
    } modes[] = {
        { "SDL_SCALEMODE_NEAREST", SDL_SCALEMODE_NEAREST },
        { "SDL_SCALEMODE_LINEAR",  SDL_SCALEMODE_LINEAR },
        { "SDL_SCALEMODE_PIXELART",  SDL_SCALEMODE_PIXELART },
    };
    size_t i;

    for (i = 0; i < SDL_arraysize(modes); i++) {
        SDL_Texture *texture;
        bool result;
        SDL_ScaleMode actual_mode = SDL_SCALEMODE_NEAREST;

        SDL_ClearError();
        SDLTest_AssertPass("About to call SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 16, 16)");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 16, 16);
        SDLTest_AssertCheck(texture != NULL, "SDL_CreateTexture must return a non-NULL texture");
        SDLTest_AssertPass("About to call SDL_SetTextureScaleMode(texture, %s)", modes[i].name);
        result = SDL_SetTextureScaleMode(texture, modes[i].mode);
        SDLTest_AssertCheck(result == true, "SDL_SetTextureScaleMode returns %d, expected %d", result, true);
        SDLTest_AssertPass("About to call SDL_GetTextureScaleMode(texture)");
        result = SDL_GetTextureScaleMode(texture, &actual_mode);
        SDLTest_AssertCheck(result == true, "SDL_SetTextureScaleMode returns %d, expected %d", result, true);
        SDLTest_AssertCheck(actual_mode == modes[i].mode, "SDL_GetTextureScaleMode must return %s (%d), actual=%d",
                            modes[i].name, modes[i].mode, actual_mode);
    }
    return TEST_COMPLETED;
}

/* Helper functions */

/**
 * Checks to see if functionality is supported. Helper function.
 */
static bool isSupported(int code)
{
    return (code != false);
}

/**
 * Test to see if we can vary the draw color. Helper function.
 *
 * \sa SDL_SetRenderDrawColor
 * \sa SDL_GetRenderDrawColor
 */
static bool hasDrawColor(void)
{
    int ret, fail;
    Uint8 r, g, b, a;

    fail = 0;

    /* Set color. */
    ret = SDL_SetRenderDrawColor(renderer, 100, 100, 100, 100);
    if (!isSupported(ret)) {
        fail = 1;
    }
    ret = SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    if (!isSupported(ret)) {
        fail = 1;
    }

    /* Restore natural. */
    ret = SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    if (!isSupported(ret)) {
        fail = 1;
    }

    /* Something failed, consider not available. */
    if (fail) {
        return false;
    }
    /* Not set properly, consider failed. */
    else if ((r != 100) || (g != 100) || (b != 100) || (a != 100)) {
        return false;
    }
    return true;
}

/**
 * Loads the test image 'Face' as texture. Helper function.
 *
 * \sa SDL_CreateTextureFromSurface
 */
static SDL_Texture *
loadTestFace(void)
{
    SDL_Surface *face;
    SDL_Texture *tface;

    face = SDLTest_ImageFace();
    if (!face) {
        return NULL;
    }

    tface = SDL_CreateTextureFromSurface(renderer, face);
    if (!tface) {
        SDLTest_LogError("SDL_CreateTextureFromSurface() failed with error: %s", SDL_GetError());
    }

    SDL_DestroySurface(face);

    return tface;
}

/**
 * Compares screen pixels with image pixels. Helper function.
 *
 * \param referenceSurface Image to compare against.
 * \param allowable_error allowed difference from the reference image
 *
 * \sa SDL_RenderReadPixels
 * \sa SDL_CreateSurfaceFrom
 * \sa SDL_DestroySurface
 */
static void compare(SDL_Surface *referenceSurface, int allowable_error)
{
    int ret;
    SDL_Rect rect;
    SDL_Surface *surface, *testSurface;

    /* Explicitly specify the rect in case the window isn't the expected size... */
    rect.x = 0;
    rect.y = 0;
    rect.w = TESTRENDER_SCREEN_W;
    rect.h = TESTRENDER_SCREEN_H;

    surface = SDL_RenderReadPixels(renderer, &rect);
    if (!surface) {
        SDLTest_AssertCheck(surface != NULL, "Validate result from SDL_RenderReadPixels, got NULL, %s", SDL_GetError());
        return;
    }

    testSurface = SDL_ConvertSurface(surface, RENDER_COMPARE_FORMAT);
    SDL_DestroySurface(surface);
    if (!testSurface) {
        SDLTest_AssertCheck(testSurface != NULL, "Validate result from SDL_ConvertSurface, got NULL, %s", SDL_GetError());
        return;
    }

    /* Compare surface. */
    ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, allowable_error);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(testSurface);
}
static void compare2x(SDL_Surface *referenceSurface, int allowable_error)
{
    int ret;
    SDL_Rect rect;
    SDL_Surface *surface, *testSurface;

    /* Explicitly specify the rect in case the window isn't the expected size... */
    rect.x = 0;
    rect.y = 0;
    rect.w = TESTRENDER_SCREEN_W * 2;
    rect.h = TESTRENDER_SCREEN_H * 2;

    surface = SDL_RenderReadPixels(renderer, &rect);
    if (!surface) {
        SDLTest_AssertCheck(surface != NULL, "Validate result from SDL_RenderReadPixels, got NULL, %s", SDL_GetError());
        return;
    }

    testSurface = SDL_ConvertSurface(surface, RENDER_COMPARE_FORMAT);
    SDL_DestroySurface(surface);
    if (!testSurface) {
        SDLTest_AssertCheck(testSurface != NULL, "Validate result from SDL_ConvertSurface, got NULL, %s", SDL_GetError());
        return;
    }

    /* Compare surface. */
    ret = SDLTest_CompareSurfaces(testSurface, referenceSurface, allowable_error);
    SDLTest_AssertCheck(ret == 0, "Validate result from SDLTest_CompareSurfaces, expected: 0, got: %i", ret);

    /* Clean up. */
    SDL_DestroySurface(testSurface);
}

/**
 * Clears the screen. Helper function.
 *
 * \sa SDL_SetRenderDrawColor
 * \sa SDL_RenderClear
 * \sa SDL_RenderPresent
 * \sa SDL_SetRenderDrawBlendMode
 */
static int
clearScreen(void)
{
    int ret;

    /* Make current */
    SDL_RenderPresent(renderer);

    /* Set color. */
    ret = SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDLTest_AssertCheck(ret == true, "Validate result from SDL_SetRenderDrawColor, expected: true, got: %i", ret);

    /* Clear screen. */
    ret = SDL_RenderClear(renderer);
    SDLTest_AssertCheck(ret == true, "Validate result from SDL_RenderClear, expected: true, got: %i", ret);

    /* Set defaults. */
    ret = SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDLTest_AssertCheck(ret == true, "Validate result from SDL_SetRenderDrawBlendMode, expected: true, got: %i", ret);

    ret = SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDLTest_AssertCheck(ret == true, "Validate result from SDL_SetRenderDrawColor, expected: true, got: %i", ret);

    return 0;
}

/**
 * Tests geometry UV wrapping
 */
static int SDLCALL render_testUVWrapping(void *arg)
{
    SDL_Vertex vertices[6];
    SDL_Vertex *verts = vertices;
    SDL_FColor color = { 1.0f, 1.0f, 1.0f, 1.0f };
    SDL_FRect rect;
    float min_U = -0.5f;
    float max_U = 1.5f;
    float min_V = -0.5f;
    float max_V = 1.5f;
    SDL_Texture *tface;
    SDL_Surface *referenceSurface = NULL;

    /* Clear surface. */
    clearScreen();

    /* Create face surface. */
    tface = loadTestFace();
    SDLTest_AssertCheck(tface != NULL, "Verify loadTestFace() result");
    if (tface == NULL) {
        return TEST_ABORTED;
    }

    rect.w = (float)tface->w * 2;
    rect.h = (float)tface->h * 2;
    rect.x = (TESTRENDER_SCREEN_W - rect.w) / 2;
    rect.y = (TESTRENDER_SCREEN_H - rect.h) / 2;

    /*
     *   0--1
     *   | /|
     *   |/ |
     *   3--2
     *
     *  Draw sprite2 as triangles that can be recombined as rect by software renderer
     */

    /* 0 */
    verts->position.x = rect.x;
    verts->position.y = rect.y;
    verts->color = color;
    verts->tex_coord.x = min_U;
    verts->tex_coord.y = min_V;
    verts++;
    /* 1 */
    verts->position.x = rect.x + rect.w;
    verts->position.y = rect.y;
    verts->color = color;
    verts->tex_coord.x = max_U;
    verts->tex_coord.y = min_V;
    verts++;
    /* 2 */
    verts->position.x = rect.x + rect.w;
    verts->position.y = rect.y + rect.h;
    verts->color = color;
    verts->tex_coord.x = max_U;
    verts->tex_coord.y = max_V;
    verts++;
    /* 0 */
    verts->position.x = rect.x;
    verts->position.y = rect.y;
    verts->color = color;
    verts->tex_coord.x = min_U;
    verts->tex_coord.y = min_V;
    verts++;
    /* 2 */
    verts->position.x = rect.x + rect.w;
    verts->position.y = rect.y + rect.h;
    verts->color = color;
    verts->tex_coord.x = max_U;
    verts->tex_coord.y = max_V;
    verts++;
    /* 3 */
    verts->position.x = rect.x;
    verts->position.y = rect.y + rect.h;
    verts->color = color;
    verts->tex_coord.x = min_U;
    verts->tex_coord.y = max_V;
    verts++;

    /* Blit sprites as triangles onto the screen */
    SDL_RenderGeometry(renderer, tface, vertices, 6, NULL, 0);

    /* See if it's the same */
    referenceSurface = SDLTest_ImageWrappingSprite();
    compare(referenceSurface, ALLOWABLE_ERROR_OPAQUE);

    /* Make current */
    SDL_RenderPresent(renderer);

    /* Clean up. */
    SDL_DestroyTexture(tface);
    SDL_DestroySurface(referenceSurface);
    referenceSurface = NULL;

    return TEST_COMPLETED;
}

/**
 * Tests texture state changes
 */
static int SDLCALL render_testTextureState(void *arg)
{
    const Uint8 pixels[8] = {
        0x00, 0x00, 0x00, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF
    };
    const SDL_Color expected[] = {
        /* Step 0: plain copy */
        { 0x00, 0x00, 0x00, 0xFF },
        { 0xFF, 0xFF, 0xFF, 0xFF },
        /* Step 1: color mod to red */
        { 0x00, 0x00, 0x00, 0xFF },
        { 0xFF, 0x00, 0x00, 0xFF },
        /* Step 2: alpha mod to 128 (cleared to green) */
        { 0x00, 0x7F, 0x00, 0xFF },
        { 0x80, 0xFF, 0x80, 0xFF },
        /* Step 3: nearest stretch */
        { 0xFF, 0xFF, 0xFF, 0xFF },
        { 0x00, 0xFF, 0x00, 0xFF },
        /* Step 4: linear stretch */
        { 0x80, 0x80, 0x80, 0xFF },
        { 0x00, 0xFF, 0x00, 0xFF },
    };
    SDL_Texture *texture;
    SDL_Rect rect;
    SDL_FRect dst;
    int i;

    /* Clear surface to green */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    /* Create 2-pixel surface. */
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 2, 1);
    SDLTest_AssertCheck(texture != NULL, "Verify SDL_CreateTexture() result");
    if (texture == NULL) {
        return TEST_ABORTED;
    }
    SDL_UpdateTexture(texture, NULL, pixels, sizeof(pixels));

    dst.x = 0.0f;
    dst.y = 0.0f;
    dst.w = 2.0f;
    dst.h = 1.0f;

    /* Step 0: plain copy */
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    dst.y += 1;

    /* Step 1: color mod to red */
    SDL_SetTextureColorMod(texture, 0xFF, 0x00, 0x00);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_SetTextureColorMod(texture, 0xFF, 0xFF, 0xFF);
    dst.y += 1;

    /* Step 2: alpha mod to 128 */
    SDL_SetTextureAlphaMod(texture, 0x80);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_SetTextureAlphaMod(texture, 0xFF);
    dst.y += 1;

    /* Step 3: nearest stretch */
    dst.w = 1;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    dst.y += 1;

    /* Step 4: linear stretch */
    dst.w = 1;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    dst.y += 1;

    /* Verify results */
    rect.x = 0;
    rect.y = 0;
    rect.w = 2;
    rect.h = 1;
    for (i = 0; i < SDL_arraysize(expected); ) {
        const int MAX_DELTA = 1;
        SDL_Color actual;
        int deltaR, deltaG, deltaB, deltaA;
        SDL_Surface *surface = SDL_RenderReadPixels(renderer, &rect);

        SDL_ReadSurfacePixel(surface, 0, 0, &actual.r, &actual.g, &actual.b, &actual.a);
        deltaR = (actual.r - expected[i].r);
        deltaG = (actual.g - expected[i].g);
        deltaB = (actual.b - expected[i].b);
        deltaA = (actual.a - expected[i].a);
        SDLTest_AssertCheck(SDL_abs(deltaR) <= MAX_DELTA &&
                            SDL_abs(deltaG) <= MAX_DELTA &&
                            SDL_abs(deltaB) <= MAX_DELTA &&
                            SDL_abs(deltaA) <= MAX_DELTA,
                            "Validate left pixel at step %d, expected %d,%d,%d,%d, got %d,%d,%d,%d", i/2,
                            expected[i].r, expected[i].g, expected[i].b, expected[i].a,
                            actual.r, actual.g, actual.b, actual.a);
        ++i;

        SDL_ReadSurfacePixel(surface, 1, 0, &actual.r, &actual.g, &actual.b, &actual.a);
        deltaR = (actual.r - expected[i].r);
        deltaG = (actual.g - expected[i].g);
        deltaB = (actual.b - expected[i].b);
        deltaA = (actual.a - expected[i].a);
        SDLTest_AssertCheck(SDL_abs(deltaR) <= MAX_DELTA &&
                            SDL_abs(deltaG) <= MAX_DELTA &&
                            SDL_abs(deltaB) <= MAX_DELTA &&
                            SDL_abs(deltaA) <= MAX_DELTA,
                            "Validate right pixel at step %d, expected %d,%d,%d,%d, got %d,%d,%d,%d", i/2,
                            expected[i].r, expected[i].g, expected[i].b, expected[i].a,
                            actual.r, actual.g, actual.b, actual.a);
        ++i;

        SDL_DestroySurface(surface);

        rect.y += 1;
    }

    /* Clean up. */
    SDL_DestroyTexture(texture);

    return TEST_COMPLETED;
}

/* ================= Test References ================== */

/* Render test cases */
static const SDLTest_TestCaseReference renderTestGetNumRenderDrivers = {
    render_testGetNumRenderDrivers, "render_testGetNumRenderDrivers", "Tests call to SDL_GetNumRenderDrivers", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestPrimitives = {
    render_testPrimitives, "render_testPrimitives", "Tests rendering primitives", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestPrimitivesWithViewport = {
    render_testPrimitivesWithViewport, "render_testPrimitivesWithViewport", "Tests rendering primitives within a viewport", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlit = {
    render_testBlit, "render_testBlit", "Tests blitting", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlitTiled = {
    render_testBlitTiled, "render_testBlitTiled", "Tests tiled blitting", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlit9Grid = {
    render_testBlit9Grid, "render_testBlit9Grid", "Tests 9-grid blitting", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlit9GridTiled = {
    render_testBlit9GridTiled, "render_testBlit9GridTiled", "Tests tiled 9-grid blitting", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlitColor = {
    render_testBlitColor, "render_testBlitColor", "Tests blitting with color", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestBlendModes = {
    render_testBlendModes, "render_testBlendModes", "Tests rendering blend modes", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestViewport = {
    render_testViewport, "render_testViewport", "Tests viewport", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestClipRect = {
    render_testClipRect, "render_testClipRect", "Tests clip rect", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestLogicalSize = {
    render_testLogicalSize, "render_testLogicalSize", "Tests logical size", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestUVWrapping = {
    render_testUVWrapping, "render_testUVWrapping", "Tests geometry UV wrapping", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestTextureState = {
    render_testTextureState, "render_testTextureState", "Tests texture state changes", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestGetSetTextureScaleMode = {
    render_testGetSetTextureScaleMode, "render_testGetSetTextureScaleMode", "Tests setting/getting texture scale mode", TEST_ENABLED
};

static const SDLTest_TestCaseReference renderTestRGBSurfaceNoAlpha = {
    render_testRGBSurfaceNoAlpha, "render_testRGBSurfaceNoAlpha", "Tests RGB surface with no alpha using software renderer", TEST_ENABLED
};

/* Sequence of Render test cases */
static const SDLTest_TestCaseReference *renderTests[] = {
    &renderTestGetNumRenderDrivers,
    &renderTestPrimitives,
    &renderTestPrimitivesWithViewport,
    &renderTestBlit,
    &renderTestBlitTiled,
    &renderTestBlit9Grid,
    &renderTestBlit9GridTiled,
    &renderTestBlitColor,
    &renderTestBlendModes,
    &renderTestViewport,
    &renderTestClipRect,
    &renderTestLogicalSize,
    &renderTestUVWrapping,
    &renderTestTextureState,
    &renderTestGetSetTextureScaleMode,
    &renderTestRGBSurfaceNoAlpha,
    NULL
};

/* Render test suite (global) */
SDLTest_TestSuiteReference renderTestSuite = {
    "Render",
    InitCreateRenderer,
    renderTests,
    CleanupDestroyRenderer
};
