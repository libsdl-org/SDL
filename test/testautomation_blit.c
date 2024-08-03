/**
 * SDL_BlitSurface bit-perfect rendering test suite written by Isaac Aronson
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
#else
/* Suppress uint64 to uint32 conversion warning within the PRNG engine */
#pragma warning( disable : 4244 )
#endif
#include <sys/stat.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>
#include "testautomation_images.h"

/* ====== xoroshiro128+ PRNG engine for deterministic blit input ===== */
Uint64 rotl(Uint64 x, int k) { return (x << k) | (x >> (-k & 63)); }
Uint64 next(Uint64 state[2]) {
    Uint64 s0 = state[0], s1 = state[1];
    Uint64 result = rotl((s0 + s1) * 9, 29) + s0;
    state[0] = s0 ^ rotl(s1, 29);
    state[1] = s0 ^ s1 << 9;
    return result;
}
static Uint64 rngState[2] = {1, 2};
Uint32 getRandomUint32() {
    return (Uint32)next(rngState);
}
/* ================= Test Case Helper Functions ================== */
/*
 * Resets PRNG state to initialize tests using PRNG
 */
void blitSetUp(void *arg) {
    rngState[0] = 1;
    rngState[1] = 2;
}
/*
 * Generates a stream of PRNG pixel data given length
 */
Uint32 *getNextRandomBuffer(const int width, const int height) {
    Uint32* buf = SDL_malloc(sizeof(Uint32) * width * height);
    int i;
    for (i = 0; i < width * height; i++) {
        buf[i] = getRandomUint32();
    }
    return buf;
}
/*
 * Generates a small 15 x 15px surface of PRNG pixel data
 */
SDL_Surface* getRandomBlitChunk(Uint32 *pixels, SDL_PixelFormat format) {
    return SDL_CreateSurfaceFrom(15, 15, format, pixels, 15 * 4);
}
/*
 * Generates a 800 x 600 surface of PRNG pixel data
 */
SDL_Surface* getRandomSVGASurface(Uint32 *pixels, SDL_PixelFormat format) {
    return SDL_CreateSurfaceFrom(800, 600, format, pixels, 800 * 4);
}
/*
 * Calculates the FNV-1a hash of input pixel data
 */
Uint32 FNVHash(Uint32* buf, int length) {
    const Uint32 fnv_prime = 0x811C9DC5;
    Uint32 hash = 0;
    int i;

    for (i = 0; i < length; buf++, i++)
    {
        hash *= fnv_prime;
        hash ^= (*buf);
    }

    return hash;
}
/*
 * Wraps the FNV-1a hash for an input surface's pixels
 */
Uint32 hashSurfacePixels(SDL_Surface * surface) {
    Uint64 buffer_size = surface->w * surface->h;
    return FNVHash(surface->pixels, buffer_size);
}
/* ================= Test Case Implementation ================== */
/**
 * Tests rendering a rainbow gradient background onto a blank surface, then rendering a sprite with complex geometry and
 * transparency on top of said surface, and comparing the result to known accurate renders with a hash.
 */
int blit_testExampleApplicationRender(void *arg) {
    const int width = 32;
    const int height = 32;
    const Uint32 correct_hash = 0xe345d7a7;
    SDL_Surface* dest_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    SDL_Surface* rainbow_background = SDLTest_ImageBlendingBackground();
    SDL_Surface* gearbrain_sprite = SDLTest_ImageBlendingSprite();
    // Blit background into "screen"
    SDL_BlitSurface(rainbow_background, NULL, dest_surface, NULL);
    // Blit example game sprite onto "screen"
    SDL_BlitSurface(gearbrain_sprite, NULL, dest_surface, NULL);
    // Check result
    const Uint32 hash = hashSurfacePixels(dest_surface);
    SDLTest_AssertCheck(hash == correct_hash,
                        "Should render identically, expected hash 0x%" SDL_PRIx32 ", got 0x%" SDL_PRIx32,
                        correct_hash, hash);
    // Clean up
    SDL_DestroySurface(rainbow_background);
    SDL_DestroySurface(gearbrain_sprite);
    SDL_DestroySurface(dest_surface);
    return TEST_COMPLETED;
}
/**
 * Tests rendering PRNG noise onto a surface of PRNG noise, while also testing color shift operations between the
 * different source and destination pixel formats, without an alpha shuffle, at SVGA resolution. Compares to known
 * accurate renders with a hash.
 */
int blit_testRandomToRandomSVGA(void *arg) {
    const int width = 800;
    const int height = 600;
    const Uint32 correct_hash = 0x42140c5f;
    // Allocate random buffers
    Uint32 *dest_pixels = getNextRandomBuffer(width, height);
    Uint32 *src_pixels = getNextRandomBuffer(width, height);
    // Create surfaces of different pixel formats
    SDL_Surface* dest_surface = getRandomSVGASurface(dest_pixels, SDL_PIXELFORMAT_BGRA8888);
    SDL_Surface* src_surface = getRandomSVGASurface(src_pixels, SDL_PIXELFORMAT_RGBA8888);
    // Blit surfaces
    SDL_BlitSurface(src_surface, NULL, dest_surface, NULL);
    // Check result
    const Uint32 hash = hashSurfacePixels(dest_surface);
    SDLTest_AssertCheck(hash == correct_hash,
                        "Should render identically, expected hash 0x%" SDL_PRIx32 ", got 0x%" SDL_PRIx32,
                        correct_hash, hash);
    // Clean up
    SDL_DestroySurface(dest_surface);
    SDL_DestroySurface(src_surface);
    SDL_free(dest_pixels);
    SDL_free(src_pixels);
    return TEST_COMPLETED;
}
/**
 * Tests rendering small chunks of 15 by 15px PRNG noise onto an initially blank SVGA surface, while also testing color
 * shift operations between the different source and destination pixel formats, including an alpha shuffle. Compares to
 * known accurate renders with a hash.
 */
int blit_testRandomToRandomSVGAMultipleIterations(void *arg) {
    const int width = 800;
    const int height = 600;
    int i;
    const Uint32 correct_hash = 0x5d26be78;
    // Create blank source surface
    SDL_Surface* dest_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888);

    // Perform 250k random blits into random areas of the blank surface
    for (i = 0; i < 250000; i++) {
        Uint32 *buf = getNextRandomBuffer(15, 15);
        SDL_Surface *sourceSurface = getRandomBlitChunk(buf, SDL_PIXELFORMAT_RGBA8888);

        SDL_Rect dest_rect;
        int location = (int)getRandomUint32();
        dest_rect.x = location % (width - 15 - 1);
        dest_rect.y = location % (height - 15 - 1);

        SDL_BlitSurface(sourceSurface, NULL, dest_surface, &dest_rect);

        SDL_DestroySurface(sourceSurface);
        SDL_free(buf);
    }
    // Check result
    const Uint32 hash = hashSurfacePixels(dest_surface);
    // Clean up
    SDL_DestroySurface(dest_surface);
    SDLTest_AssertCheck(hash == correct_hash,
                        "Should render identically, expected hash 0x%" SDL_PRIx32 ", got 0x%" SDL_PRIx32,
                        correct_hash, hash);
    return TEST_COMPLETED;
}

static const SDLTest_TestCaseReference blitTest1 = {
        (SDLTest_TestCaseFp)blit_testExampleApplicationRender, "blit_testExampleApplicationRender",
        "Test example application render.", TEST_ENABLED
};
static const SDLTest_TestCaseReference blitTest2 = {
        (SDLTest_TestCaseFp)blit_testRandomToRandomSVGA, "blit_testRandomToRandomSVGA",
        "Test SVGA noise render.", TEST_ENABLED
};
static const SDLTest_TestCaseReference blitTest3 = {
        (SDLTest_TestCaseFp)blit_testRandomToRandomSVGAMultipleIterations, "blit_testRandomToRandomSVGAMultipleIterations",
        "Test SVGA noise render (250k iterations).", TEST_ENABLED
};
static const SDLTest_TestCaseReference *blitTests[] = {
        &blitTest1, &blitTest2, &blitTest3, NULL
};

SDLTest_TestSuiteReference blitTestSuite = {
        "Blending",
        blitSetUp,
        blitTests,
        NULL
};
