/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#include "SDL_stb_c.h"
#include "SDL_surface_c.h"

#ifdef SDL_HAVE_STB
////////////////////////////////////////////////////////////////////////////
#define malloc SDL_malloc
#define realloc SDL_realloc
#define free SDL_free
#undef memcpy
#define memcpy SDL_memcpy
#undef memset
#define memset SDL_memset
#undef strcmp
#define strcmp SDL_strcmp
#undef strncmp
#define strncmp SDL_strncmp
#define strtol SDL_strtol

#define abs SDL_abs
#define pow SDL_pow
#define ldexp SDL_scalbn

#define STB_IMAGE_STATIC
#define STBI_NO_THREAD_LOCALS
#define STBI_FAILURE_USERMSG
#if defined(SDL_NEON_INTRINSICS)
#define STBI_NEON
#endif
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_STDIO
#define STBI_ASSERT SDL_assert
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

////////////////////////////////////////////////////////////////////////////
#define MZ_ASSERT(x) SDL_assert(x)
//#undef memcpy
//#define memcpy SDL_memcpy
//#undef memset
//#define memset SDL_memset
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define MINIZ_LITTLE_ENDIAN 1
#else
#define MINIZ_LITTLE_ENDIAN 0
#endif
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#define MINIZ_SDL_NOUNUSED
#include "miniz.h"

#undef memset
#endif // SDL_HAVE_STB

#ifdef SDL_HAVE_STB
static bool SDL_ConvertPixels_MJPG_to_NV12(int width, int height, const void *src, int src_pitch, void *dst, int dst_pitch)
{
    int w = 0, h = 0, format = 0;
    stbi__context s;
    stbi__start_mem(&s, src, src_pitch);

    stbi__result_info ri;
    SDL_zero(ri);
    ri.bits_per_channel = 8;
    ri.channel_order = STBI_ORDER_RGB;
    ri.num_channels = 0;

    stbi__nv12 nv12;
    nv12.w = width;
    nv12.h = height;
    nv12.pitch = dst_pitch;
    nv12.y = (stbi_uc *)dst;
    nv12.uv = nv12.y + (nv12.h * nv12.pitch);

    void *pixels = stbi__jpeg_load(&s, &w, &h, &format, 4, &nv12, &ri);
    if (!pixels) {
        return false;
    }
    return true;
}
#endif // SDL_HAVE_STB

bool SDL_ConvertPixels_STB(int width, int height,
                           SDL_PixelFormat src_format, SDL_Colorspace src_colorspace, SDL_PropertiesID src_properties, const void *src, int src_pitch,
                           SDL_PixelFormat dst_format, SDL_Colorspace dst_colorspace, SDL_PropertiesID dst_properties, void *dst, int dst_pitch)
{
#ifdef SDL_HAVE_STB
    if (src_format == SDL_PIXELFORMAT_MJPG && dst_format == SDL_PIXELFORMAT_NV12) {
        return SDL_ConvertPixels_MJPG_to_NV12(width, height, src, src_pitch, dst, dst_pitch);
    }

    bool result;
    int w = 0, h = 0, format = 0;
    int len = (src_format == SDL_PIXELFORMAT_MJPG) ? src_pitch : (height * src_pitch);
    void *pixels = stbi_load_from_memory(src, len, &w, &h, &format, 4);
    if (!pixels) {
        return false;
    }

    if (w == width && h == height) {
        result = SDL_ConvertPixelsAndColorspace(w, h, SDL_PIXELFORMAT_RGBA32, SDL_COLORSPACE_SRGB, 0, pixels, width * 4, dst_format, dst_colorspace, dst_properties, dst, dst_pitch);
    } else {
        result = SDL_SetError("Expected image size %dx%d, actual size %dx%d", width, height, w, h);
    }
    stbi_image_free(pixels);

    return result;
#else
    return SDL_SetError("SDL not built with STB image support");
#endif
}

#ifdef SDL_HAVE_STB
static int IMG_LoadSTB_IO_read(void *user, char *data, int size)
{
    size_t amount = SDL_ReadIO((SDL_IOStream*)user, data, size);
    return (int)amount;
}

static void IMG_LoadSTB_IO_skip(void *user, int n)
{
    SDL_SeekIO((SDL_IOStream*)user, n, SDL_IO_SEEK_CUR);
}

static int IMG_LoadSTB_IO_eof(void *user)
{
    SDL_IOStream *src = (SDL_IOStream*)user;
    return SDL_GetIOStatus(src) == SDL_IO_STATUS_EOF;
}

static SDL_Surface *SDL_LoadSTB_IO(SDL_IOStream *src)
{
    Sint64 start;
    Uint8 magic[26];
    int w, h, format;
    stbi_uc *pixels;
    stbi_io_callbacks rw_callbacks;
    SDL_Surface *surface = NULL;
    bool use_palette = false;
    unsigned int palette_colors[256];

    // src has already been validated
    start = SDL_TellIO(src);

    if (SDL_ReadIO(src, magic, sizeof(magic)) == sizeof(magic)) {
        const Uint8 PNG_COLOR_INDEXED = 3;
        if (magic[0] == 0x89 &&
            magic[1] == 'P' &&
            magic[2] == 'N' &&
            magic[3] == 'G' &&
            magic[12] == 'I' &&
            magic[13] == 'H' &&
            magic[14] == 'D' &&
            magic[15] == 'R' &&
            magic[25] == PNG_COLOR_INDEXED) {
            use_palette = true;
        }
    }
    SDL_SeekIO(src, start, SDL_IO_SEEK_SET);

    /* Load the image data */
    rw_callbacks.read = IMG_LoadSTB_IO_read;
    rw_callbacks.skip = IMG_LoadSTB_IO_skip;
    rw_callbacks.eof = IMG_LoadSTB_IO_eof;
    w = h = format = 0; /* silence warning */
    if (use_palette) {
        /* Unused palette entries will be opaque white */
        SDL_memset(palette_colors, 0xff, sizeof(palette_colors));

        pixels = stbi_load_from_callbacks_with_palette(
            &rw_callbacks,
            src,
            &w,
            &h,
            palette_colors,
            SDL_arraysize(palette_colors)
        );
    } else {
        pixels = stbi_load_from_callbacks(
            &rw_callbacks,
            src,
            &w,
            &h,
            &format,
            STBI_default
        );
    }
    if (!pixels) {
        SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
        return NULL;
    }

    if (use_palette) {
        surface = SDL_CreateSurfaceFrom(
            w,
            h,
            SDL_PIXELFORMAT_INDEX8,
            pixels,
            w
        );
        if (surface) {
            bool has_colorkey = false;
            int colorkey_index = -1;
            bool has_alpha = false;
            SDL_Palette *palette = SDL_CreateSurfacePalette(surface);
            if (palette) {
                int i;
                Uint8 *palette_bytes = (Uint8 *)palette_colors;

                for (i = 0; i < palette->ncolors; i++) {
                    palette->colors[i].r = *palette_bytes++;
                    palette->colors[i].g = *palette_bytes++;
                    palette->colors[i].b = *palette_bytes++;
                    palette->colors[i].a = *palette_bytes++;
                    if (palette->colors[i].a != SDL_ALPHA_OPAQUE) {
                        if (palette->colors[i].a == SDL_ALPHA_TRANSPARENT && !has_colorkey) {
                            has_colorkey = true;
                            colorkey_index = i;
                        } else {
                            /* Partial opacity or multiple colorkeys */
                            has_alpha = true;
                        }
                    }
                }
            }
            if (has_alpha) {
                SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
            } else if (has_colorkey) {
                SDL_SetSurfaceColorKey(surface, true, colorkey_index);
            }

            /* FIXME: This sucks. It'd be better to allocate the surface first, then
             * write directly to the pixel buffer:
             * https://github.com/nothings/stb/issues/58
             * -flibit
             */
            surface->flags &= ~SDL_SURFACE_PREALLOCATED;
        }

    } else if (format == STBI_grey || format == STBI_rgb || format == STBI_rgb_alpha) {
        surface = SDL_CreateSurfaceFrom(
            w,
            h,
            (format == STBI_rgb_alpha) ? SDL_PIXELFORMAT_RGBA32 :
            (format == STBI_rgb) ? SDL_PIXELFORMAT_RGB24 :
            SDL_PIXELFORMAT_INDEX8,
            pixels,
            w * format
        );
        if (surface) {
            /* Set a grayscale palette for gray images */
            if (surface->format == SDL_PIXELFORMAT_INDEX8) {
                SDL_Palette *palette = SDL_CreateSurfacePalette(surface);
                if (palette) {
                    int i;

                    for (i = 0; i < palette->ncolors; i++) {
                        palette->colors[i].r = (Uint8)i;
                        palette->colors[i].g = (Uint8)i;
                        palette->colors[i].b = (Uint8)i;
                    }
                }
            }

            /* FIXME: This sucks. It'd be better to allocate the surface first, then
             * write directly to the pixel buffer:
             * https://github.com/nothings/stb/issues/58
             * -flibit
             */
            surface->flags &= ~SDL_SURFACE_PREALLOCATED;
        }

    } else if (format == STBI_grey_alpha) {
        surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (surface) {
            Uint8 *src_ptr = pixels;
            Uint8 *dst = (Uint8 *)surface->pixels;
            int skip = surface->pitch - (surface->w * 4);
            int row, col;

            for (row = 0; row < h; ++row) {
                for (col = 0; col < w; ++col) {
                    Uint8 c = *src_ptr++;
                    Uint8 a = *src_ptr++;
                    *dst++ = c;
                    *dst++ = c;
                    *dst++ = c;
                    *dst++ = a;
                }
                dst += skip;
            }
            stbi_image_free(pixels);
        }
    } else {
        SDL_SetError("Unknown image format: %d", format);
    }

    if (!surface) {
        /* The error message should already be set */
        stbi_image_free(pixels); /* calls SDL_free() */
        SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
    }
    return surface;
}
#endif // SDL_HAVE_STB

bool SDL_IsPNG(SDL_IOStream *src)
{
    Sint64 start;
    Uint8 magic[4];
    bool is_PNG;

    is_PNG = false;
    start = SDL_TellIO(src);
    if (start >= 0) {
        if (SDL_ReadIO(src, magic, sizeof(magic)) == sizeof(magic)) {
            if (magic[0] == 0x89 &&
                magic[1] == 'P' &&
                magic[2] == 'N' &&
                magic[3] == 'G') {
                is_PNG = true;
            }
        }
        SDL_SeekIO(src, start, SDL_IO_SEEK_SET);
    }

    return is_PNG;
}

SDL_Surface *SDL_LoadPNG_IO(SDL_IOStream *src, bool closeio)
{
    SDL_Surface *surface = NULL;

    CHECK_PARAM(!src) {
        SDL_InvalidParamError("src");
        goto done;
    }

    if (!SDL_IsPNG(src)) {
        SDL_SetError("File is not a PNG file");
        goto done;
    }

#ifdef SDL_HAVE_STB
    surface = SDL_LoadSTB_IO(src);
#else
    SDL_SetError("SDL not built with STB image support");
#endif // SDL_HAVE_STB

done:
    if (src && closeio) {
        SDL_CloseIO(src);
    }
    return surface;
}

SDL_Surface *SDL_LoadPNG(const char *file)
{
    SDL_IOStream *stream = SDL_IOFromFile(file, "rb");
    if (!stream) {
        return NULL;
    }

    return SDL_LoadPNG_IO(stream, true);
}

bool SDL_SavePNG_IO(SDL_Surface *surface, SDL_IOStream *dst, bool closeio)
{
    bool retval = false;
    Uint8 *plte = NULL;
    Uint8 *trns = NULL;
    bool free_surface = false;

    // Make sure we have somewhere to save
    CHECK_PARAM(!SDL_SurfaceValid(surface)) {
        SDL_InvalidParamError("surface");
        goto done;
    }
    CHECK_PARAM(!dst) {
        SDL_InvalidParamError("dst");
        goto done;
    }

#ifdef SDL_HAVE_STB
    int plte_size = 0;
    int trns_size = 0;

    if (SDL_ISPIXELFORMAT_INDEXED(surface->format)) {
        if (!surface->palette) {
            SDL_SetError("Indexed surfaces must have a palette");
            goto done;
        }

        if (surface->format != SDL_PIXELFORMAT_INDEX8) {
            surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_INDEX8);
            if (!surface) {
                goto done;
            }
            free_surface = true;
        }

        plte_size = surface->palette->ncolors * 3;
        trns_size = surface->palette->ncolors;
        plte = (Uint8 *)SDL_malloc(plte_size);
        trns = (Uint8 *)SDL_malloc(trns_size);
        if (!plte || !trns) {
            goto done;
        }
        SDL_Color *colors = surface->palette->colors;
        for (int i = 0; i < surface->palette->ncolors; ++i) {
            plte[i * 3 + 0] = colors[i].r;
            plte[i * 3 + 1] = colors[i].g;
            plte[i * 3 + 2] = colors[i].b;
            trns[i] = colors[i].a;
        }
    } else {
        if (surface->format != SDL_PIXELFORMAT_RGBA32) {
            surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            if (!surface) {
                goto done;
            }
            free_surface = true;
        }
    }

    size_t size = 0;
    void *png = tdefl_write_image_to_png_file_in_memory_ex(surface->pixels, surface->w, surface->h, SDL_BYTESPERPIXEL(surface->format), surface->pitch, &size, 6, MZ_FALSE, plte, plte_size, trns, trns_size);
    if (png) {
        if (SDL_WriteIO(dst, png, size)) {
            retval = true;
        }
        mz_free(png); /* calls SDL_free() */
    } else {
        SDL_SetError("Failed to convert and save image");
    }

#else
    SDL_SetError("SDL not built with STB image support");
#endif

done:
    if (free_surface) {
        SDL_DestroySurface(surface);
    }
    SDL_free(plte);
    SDL_free(trns);

    if (dst && closeio) {
        retval &= SDL_CloseIO(dst);
    }
    return retval;
}

bool SDL_SavePNG(SDL_Surface *surface, const char *file)
{
#ifdef SDL_HAVE_STB
    SDL_IOStream *stream = SDL_IOFromFile(file, "wb");
    if (!stream) {
        return false;
    }

    return SDL_SavePNG_IO(surface, stream, true);
#else
    return SDL_SetError("SDL not built with STB image support");
#endif
}
