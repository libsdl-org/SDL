#include <SDL3/SDL.h>
#include <stdio.h>
#include <time.h>


static void test_rgb32_swizzle_test(SDL_PixelFormat srcfmt, SDL_PixelFormat dstfmt)
{
    SDL_Init(SDL_INIT_VIDEO);
    
    const int W = 1920, H = 1080;
    const int LOOPS = 1000;
    
    SDL_Surface *src = SDL_CreateSurface(W, H, srcfmt);
    SDL_Surface *dst = SDL_CreateSurface(W, H, dstfmt);
    
    SDL_FillSurfaceRect(src, NULL, 0x80808080);
    SDL_FillSurfaceRect(dst, NULL, 0xFFFFFFFF);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
    
    SDL_Rect rect = {0, 0, W, H};
    
    for (int i = 0; i < 10; i++) {
        SDL_BlitSurface(src, &rect, dst, &rect);
    }
    
    clock_t start = clock();
    for (int i = 0; i < LOOPS; i++) {
        SDL_BlitSurface(src, &rect, dst, &rect);
    }
    clock_t end = clock();
    
    double ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    double throughput = (W * H * 4.0 * LOOPS) / (ms / 1000.0) / 1e9;
    
    printf("RGB32 Blit Swizzle:\r\n\t%dx%d, %d loops, %.2f ms total, %.3f ms/frame, %.2f GB/s\n",
           W, H, LOOPS, ms, ms / LOOPS, throughput);
    
    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);
    SDL_Quit();
}

static void test_rgb32_rgb565_swizzle_test(SDL_PixelFormat srcfmt)
{
    SDL_Init(SDL_INIT_VIDEO);
    
    const int W = 1920, H = 1080;
    const int LOOPS = 1000;
    
    SDL_Surface *src = SDL_CreateSurface(W, H, srcfmt);
    SDL_Surface *dst = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGB565);
    
    SDL_FillSurfaceRect(src, NULL, 0x80808080);
    SDL_FillSurfaceRect(dst, NULL, 0xFFFF);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
    
    SDL_Rect rect = {0, 0, W, H};
    
    for (int i = 0; i < 10; i++) {
        SDL_BlitSurface(src, &rect, dst, &rect);
    }
    
    clock_t start = clock();
    for (int i = 0; i < LOOPS; i++) {
        SDL_BlitSurface(src, &rect, dst, &rect);
    }
    clock_t end = clock();
    
    double ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    double throughput = (W * H * 4.0 * LOOPS) / (ms / 1000.0) / 1e9;
    
    printf("RGB32 to RGB565 Blit Swizzle:\r\n\t%dx%d, %d loops, %.2f ms total, %.3f ms/frame, %.2f GB/s\n",
           W, H, LOOPS, ms, ms / LOOPS, throughput);
    
    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);
    SDL_Quit();
}

int main(int argc, char *argv[]) 
{
    test_rgb32_swizzle_test(SDL_PIXELFORMAT_RGBA8888, SDL_PIXELFORMAT_ARGB8888);
    test_rgb32_swizzle_test(SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_ARGB8888);

    test_rgb32_rgb565_swizzle_test(SDL_PIXELFORMAT_RGBA8888);

    return 0;
}
