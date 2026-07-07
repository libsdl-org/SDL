/*
 * SDL3 Colour-Keying Performance Benchmark
 * Format: RGBA8888 -> RGBA8888
 * Statistic: Fixed 1000 frames, measure total elapsed time in milliseconds
 * Build: gcc -O2 -o sdl3_colourkey_bench sdl3_colourkey_bench.c `pkg-config --cflags --libs sdl3`
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Benchmark configuration */
#define WARMUP_FRAMES       60
#define BENCHMARK_FRAMES    1000
#define TEST_WIDTH          1920
#define TEST_HEIGHT         1080

/* Colour-key: pure green (0x00FF00) will be treated as transparent */
#define COLOUR_KEY          0x00FF00FF  /* RGBA: Green with full alpha */

typedef struct {
    const char *name;
    int src_w;
    int src_h;
    int dst_w;
    int dst_h;
    double total_ms;            /* Total milliseconds for 1000 frames */
    double avg_ms_per_blit;     /* Average milliseconds per blit */
    double pixels_per_second;   /* Derived throughput for reference */
} BenchResult;

static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static double ns_to_ms(uint64_t ns)
{
    return (double)ns / 1000000000.0 * 1000.0; /* Convert to milliseconds */
}

/*
 * Fill surface with a checkerboard pattern.
 * Even tiles: solid colour (non-keyed)
 * Odd tiles:  colour-key value (will be transparent)
 */
static void fill_checkerboard_pattern(SDL_Surface *surf, uint32_t key_colour)
{
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(surf->format);
    uint32_t solid_colour = SDL_MapRGBA(fmt, NULL, 0xFF, 0x00, 0x00, 0xFF); /* Red */

    int tile_w = surf->w / 8;
    if (tile_w < 1) tile_w = 1;
    int tile_h = surf->h / 8;
    if (tile_h < 1) tile_h = 1;

    for (int y = 0; y < surf->h; y++) {
        for (int x = 0; x < surf->w; x++) {
            int tile_x = x / tile_w;
            int tile_y = y / tile_h;
            uint32_t pixel = ((tile_x + tile_y) % 2 == 0) ? solid_colour : key_colour;
            SDL_WriteSurfacePixel(surf, x, y,
                (pixel >> 24) & 0xFF,
                (pixel >> 16) & 0xFF,
                (pixel >> 8) & 0xFF,
                pixel & 0xFF);
        }
    }
}

static BenchResult run_benchmark(const char *name, int src_w, int src_h,
                                  int dst_w, int dst_h, int enable_colourkey)
{
    BenchResult result = {0};
    result.name = name;
    result.src_w = src_w;
    result.src_h = src_h;
    result.dst_w = dst_w;
    result.dst_h = dst_h;

    /* Create source and destination surfaces */
    SDL_Surface *src = SDL_CreateSurface(src_w, src_h, SDL_PIXELFORMAT_RGBA8888);
    SDL_Surface *dst = SDL_CreateSurface(dst_w, dst_h, SDL_PIXELFORMAT_RGBA8888);

    if (!src || !dst) {
        fprintf(stderr, "Failed to create surfaces: %s\n", SDL_GetError());
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
        return result;
    }

    /* Fill source with pattern */
    fill_checkerboard_pattern(src, COLOUR_KEY);

    /* Setup colour-keying if requested */
    if (enable_colourkey) {
        if (!SDL_SetSurfaceColorKey(src, true, COLOUR_KEY)) {
            fprintf(stderr, "Failed to set colour key: %s\n", SDL_GetError());
        }
    }

    /* Warmup to stabilise caches */
    SDL_Rect src_rect = {0, 0, src_w, src_h};
    SDL_Rect dst_rect = {0, 0, src_w, src_h};
    for (int i = 0; i < WARMUP_FRAMES; i++) {
        SDL_BlitSurface(src, &src_rect, dst, &dst_rect);
    }

    /* Benchmark loop: fixed 1000 frames */
    uint64_t start_ns = get_time_ns();
    for (int frame = 0; frame < BENCHMARK_FRAMES; frame++) {
        SDL_BlitSurface(src, &src_rect, dst, &dst_rect);
    }
    uint64_t end_ns = get_time_ns();

    uint64_t elapsed_ns = end_ns - start_ns;
    result.total_ms = ns_to_ms(elapsed_ns);
    result.avg_ms_per_blit = result.total_ms / (double)BENCHMARK_FRAMES;
    result.pixels_per_second = ((double)BENCHMARK_FRAMES * (double)src_w * (double)src_h) 
                               / (elapsed_ns / 1000000000.0);

    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);

    return result;
}

static void print_results(const BenchResult *r)
{
    printf("  %-20s %4dx%-4d -> %4dx%-4d  %8.2f ms (1000f)  %8.4f ms/f  %10.2f MP/s\n",
           r->name,
           r->src_w, r->src_h,
           r->dst_w, r->dst_h,
           r->total_ms,
           r->avg_ms_per_blit,
           r->pixels_per_second / 1000000.0);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("SDL3 Colour-Keying Performance Benchmark\n");
    printf("========================================\n");
    printf("Build: SDL %s\n", SDL_GetRevision());
    printf("Target: RGBA8888 -> RGBA8888\n");
    printf("Colour-Key: 0x%08X (Green tiles transparent)\n", COLOUR_KEY);
    printf("Frames per test: %d\n\n", BENCHMARK_FRAMES);

    /* Test matrix: various common resolutions */
    struct {
        const char *name;
        int w, h;
    } sizes[] = {
        //{"640x480",   640,  480},
        //{"800x600",   800,  600},
        {"1280x720",  1280, 720},
        {"1920x1080", 1920, 1080},
        //{"4K",        3840, 2160},
    };

    printf("--- 1. Baseline (No Colour-Key) ---\n");
    for (size_t i = 0; i < SDL_arraysize(sizes); i++) {
        BenchResult r = run_benchmark(sizes[i].name,
                                      sizes[i].w, sizes[i].h,
                                      sizes[i].w, sizes[i].h,
                                      0);  /* No colour-key */
        print_results(&r);
    }

    printf("\n--- 2. Colour-Key Enabled ---\n");
    for (size_t i = 0; i < SDL_arraysize(sizes); i++) {
        BenchResult r = run_benchmark(sizes[i].name,
                                      sizes[i].w, sizes[i].h,
                                      sizes[i].w, sizes[i].h,
                                      1);  /* With colour-key */
        print_results(&r);
    }

    printf("\nBenchmark complete.\n");
    SDL_Quit();
    return 0;
}