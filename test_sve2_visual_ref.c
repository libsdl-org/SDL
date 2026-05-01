// test_sve2_visual_ref.c — Fixed for SDL3 API + Endian-safe
#include <SDL3/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>


// SDL3 helper: get format details once, palette is NULL for RGBA8888
static const SDL_PixelFormatDetails *fmt_rgba = NULL;
static const SDL_PixelFormatDetails *get_rgba_fmt(void) {
    if (!fmt_rgba) fmt_rgba = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA8888);
    return fmt_rgba;
}

// Endian-safe color macros using SDL3's 6-arg SDL_MapRGBA
#define C(r,g,b,a) SDL_MapRGBA(get_rgba_fmt(), NULL, (r),(g),(b),(a))
#define C_OPAQUE(r,g,b) C((r),(g),(b),255)

static void draw_circle(SDL_Surface *s, int cx, int cy, int r, Uint32 color) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy < r*r) {
                pixels[y * pitch + x] = color;
            }
        }
    }
}

static void draw_hgradient(SDL_Surface *s) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            int r = (x * 255) / s->w;
            int g = (y * 255) / s->h;
            int b = 255 - ((x + y) * 255) / (s->w + s->h);
            pixels[y * pitch + x] = C_OPAQUE(r, g, b);
        }
    }
}

static void draw_color_wheel(SDL_Surface *s, int cx, int cy, int radius) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            int dx = x - cx, dy = y - cy;
            int dist = (int)sqrt(dx*dx + dy*dy);
            if (dist < radius) {
                float angle = atan2f(dy, dx);
                float hue = (angle + M_PI) / (2.0f * M_PI);
                int h_i = (int)(hue * 6.0f) % 6;
                float f = hue * 6.0f - h_i;
                int v = 255, p = 0;
                int q = (int)(255 * (1.0f - f));
                int t = (int)(255 * f);
                int r, g, b;
                switch (h_i) {
                    case 0: r = v; g = t; b = p; break;
                    case 1: r = q; g = v; b = p; break;
                    case 2: r = p; g = v; b = t; break;
                    case 3: r = p; g = q; b = v; break;
                    case 4: r = t; g = p; b = v; break;
                    default: r = v; g = p; b = q; break;
                }
                Uint8 a = 255 - ((dist * 128) / radius);
                pixels[y * pitch + x] = C(r, g, b, a);
            } else {
                pixels[y * pitch + x] = C_OPAQUE(128, 128, 128);
            }
        }
    }
}

static void draw_stripes(SDL_Surface *s, int period, Uint32 c1, Uint32 c2) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            pixels[y * pitch + x] = ((x / period) % 2 == 0) ? c1 : c2;
        }
    }
}

static void draw_sharp_edges(SDL_Surface *s, Uint32 fg, Uint32 bg) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    for (int i = 0; i < s->h * pitch; i++) pixels[i] = bg;
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x += 10) {
            if (x < s->w) pixels[y * pitch + x] = fg;
        }
    }
    for (int y = 0; y < s->h; y += 10) {
        for (int x = 0; x < s->w; x++) {
            if (y < s->h) pixels[y * pitch + x] = fg;
        }
    }
}

static void save_bmp(SDL_Surface *s, const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/sve2_ref_%s.bmp", name);
    if (SDL_SaveBMP(s, path)) {
        printf("Saved: %s\n", path);
    } else {
        printf("Failed to save %s: %s\n", path, SDL_GetError());
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    // CRITICAL: Remove stale reference images to avoid comparing old runs
    printf("=== Generating Enhanced SVE2 Reference Images ===\n");
    printf("Cleaning stale /tmp/sve2_ref_*.bmp ...\n");
    system("rm -f /tmp/sve2_ref_*.bmp");
    
    // TEST 1: Alpha Blend with RGB Gradient + Variable Alpha
    // Correct: smooth RGB gradient on gray, alpha varies vertically
    {
        SDL_Surface *src = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA8888);
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        draw_hgradient(src);
        Uint32 *sp = (Uint32 *)src->pixels;
        for (int y = 0; y < 256; y++) {
            Uint8 a = 128 + ((y * 127) / 256);
            for (int x = 0; x < 256; x++) {
                Uint32 c = sp[y * (src->pitch/4) + x];
                Uint8 r, g, b, a_old;
                SDL_GetRGBA(c, get_rgba_fmt(), NULL, &r, &g, &b, &a_old);
                sp[y * (src->pitch/4) + x] = C(r, g, b, a);
            }
        }
        SDL_FillSurfaceRect(dst, NULL, C_OPAQUE(128, 128, 128));
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
        SDL_Rect dstrect = {128, 128, 256, 256};
        SDL_BlitSurface(src, NULL, dst, &dstrect);
        save_bmp(dst, "01_gradient_alpha_blend");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
    }

    // TEST 2: Color Wheel on Dark Background
    {
        SDL_Surface *src = SDL_CreateSurface(300, 300, SDL_PIXELFORMAT_RGBA8888);
        draw_color_wheel(src, 150, 150, 140);
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        SDL_FillSurfaceRect(dst, NULL, C_OPAQUE(32, 32, 32));
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
        SDL_Rect dstrect = {106, 106, 300, 300};
        SDL_BlitSurface(src, NULL, dst, &dstrect);
        save_bmp(dst, "02_color_wheel");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
    }

    // TEST 3: Red Alpha Gradient over Blue Checkerboard
    // Foreground: red with vertical alpha fade (top opaque -> bottom transparent)
    // Background: dark blue / navy checkerboard (high contrast with red)
    {
        SDL_Surface *src = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA8888);
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        for (int y = 0; y < 512; y++) {
            for (int x = 0; x < 512; x++) {
                Uint32 c = ((x/32 + y/32) % 2 == 0) ? C_OPAQUE(0, 0, 128) : C_OPAQUE(0, 0, 64);
                ((Uint32 *)dst->pixels)[y * (dst->pitch/4) + x] = c;
            }
        }
        for (int y = 0; y < 256; y++) {
            Uint8 a = 128 + ((y * 127) / 256);
            Uint32 color = C(255, 0, 0, a);
            for (int x = 0; x < 256; x++) {
                ((Uint32 *)src->pixels)[y * (src->pitch/4) + x] = color;
            }
        }
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);
        SDL_Rect dstrect = {128, 128, 256, 256};
        SDL_BlitSurface(src, NULL, dst, &dstrect);
        save_bmp(dst, "03_alpha_gradient");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
    }

    // TEST 4: Scaled High-Frequency Stripes
    {
        SDL_Surface *src = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA8888);
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        draw_stripes(src, 1, C_OPAQUE(255, 255, 255), C_OPAQUE(0, 0, 0));
        SDL_FillSurfaceRect(dst, NULL, C_OPAQUE(255, 0, 0));
        SDL_Rect srcrect = {0, 0, 64, 64};
        SDL_Rect dstrect = {64, 64, 384, 384};
        SDL_BlitSurfaceScaled(src, &srcrect, dst, &dstrect, SDL_SCALEMODE_NEAREST);
        save_bmp(dst, "04_scaled_stripes");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
    }

    // TEST 5: Yellow Grid on Black, ColorKey to Dark Blue Background
    {
        SDL_Surface *src = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA8888);
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        draw_sharp_edges(src, C_OPAQUE(255, 255, 0), C_OPAQUE(0, 0, 0));
        SDL_SetSurfaceColorKey(src, true, C_OPAQUE(0, 0, 0));
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
        SDL_FillSurfaceRect(dst, NULL, C_OPAQUE(0, 0, 128));
        SDL_Rect dstrect = {128, 128, 256, 256};
        SDL_BlitSurface(src, NULL, dst, &dstrect);
        save_bmp(dst, "05_sharp_edges_colorkey");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst);
    }

    // TEST 6: RGB565 with Smooth Gradient
    {
        SDL_Surface *src = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA8888);
        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 256; x++) {
                int r = x;
                int g = y;
                int b = 128;
                ((Uint32 *)src->pixels)[y * (src->pitch/4) + x] = C_OPAQUE(r, g, b);
            }
        }
        SDL_Surface *dst565 = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGB565);
        SDL_FillSurfaceRect(dst565, NULL, 0x0000);
        SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
        SDL_Rect dstrect = {128, 128, 256, 256};
        SDL_BlitSurface(src, NULL, dst565, &dstrect);
        SDL_Surface *dst_rgba = SDL_ConvertSurface(dst565, SDL_PIXELFORMAT_RGBA8888);
        save_bmp(dst_rgba, "06_rgb565_gradient");
        SDL_DestroySurface(src);
        SDL_DestroySurface(dst565);
        SDL_DestroySurface(dst_rgba);
    }

    // TEST 7: Fill Rects with Rainbow Gradient
    {
        SDL_Surface *dst = SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_RGBA8888);
        SDL_FillSurfaceRect(dst, NULL, C_OPAQUE(0, 0, 0));
        for (int i = 0; i < 16; i++) {
            int r = (i * 16) % 256;
            int g = (255 - i * 16) % 256;
            int b = (128 + i * 8) % 256;
            SDL_Rect rct = {32 + (i % 4) * 120, 32 + (i / 4) * 120, 100, 100};
            SDL_FillSurfaceRect(dst, &rct, C_OPAQUE(r, g, b));
        }
        save_bmp(dst, "07_fill_gradient");
        SDL_DestroySurface(dst);
    }

    printf("\nAll reference images saved to /tmp/sve2_ref_*.bmp\n");
    
    SDL_Quit();
    return 0;
}