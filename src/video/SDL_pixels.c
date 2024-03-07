/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

/* General (mostly internal) pixel/color manipulation routines for SDL */

#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_pixels_c.h"
#include "SDL_RLEaccel_c.h"
#include "../SDL_list.h"

/* Lookup tables to expand partial bytes to the full 0..255 range */

static Uint8 lookup_0[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

static Uint8 lookup_1[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 255
};

static Uint8 lookup_2[] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 214, 218, 222, 226, 230, 234, 238, 242, 246, 250, 255
};

static Uint8 lookup_3[] = {
    0, 8, 16, 24, 32, 41, 49, 57, 65, 74, 82, 90, 98, 106, 115, 123, 131, 139, 148, 156, 164, 172, 180, 189, 197, 205, 213, 222, 230, 238, 246, 255
};

static Uint8 lookup_4[] = {
    0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255
};

static Uint8 lookup_5[] = {
    0, 36, 72, 109, 145, 182, 218, 255
};

static Uint8 lookup_6[] = {
    0, 85, 170, 255
};

static Uint8 lookup_7[] = {
    0, 255
};

static Uint8 lookup_8[] = {
    255
};

Uint8 *SDL_expand_byte[9] = {
    lookup_0,
    lookup_1,
    lookup_2,
    lookup_3,
    lookup_4,
    lookup_5,
    lookup_6,
    lookup_7,
    lookup_8
};

/* Helper functions */

#define CASE(X) \
    case X:     \
        return #X;
const char *SDL_GetPixelFormatName(SDL_PixelFormatEnum format)
{
    switch (format) {

        CASE(SDL_PIXELFORMAT_INDEX1LSB)
        CASE(SDL_PIXELFORMAT_INDEX1MSB)
        CASE(SDL_PIXELFORMAT_INDEX2LSB)
        CASE(SDL_PIXELFORMAT_INDEX2MSB)
        CASE(SDL_PIXELFORMAT_INDEX4LSB)
        CASE(SDL_PIXELFORMAT_INDEX4MSB)
        CASE(SDL_PIXELFORMAT_INDEX8)
        CASE(SDL_PIXELFORMAT_RGB332)
        CASE(SDL_PIXELFORMAT_RGB444)
        CASE(SDL_PIXELFORMAT_BGR444)
        CASE(SDL_PIXELFORMAT_RGB555)
        CASE(SDL_PIXELFORMAT_BGR555)
        CASE(SDL_PIXELFORMAT_ARGB4444)
        CASE(SDL_PIXELFORMAT_RGBA4444)
        CASE(SDL_PIXELFORMAT_ABGR4444)
        CASE(SDL_PIXELFORMAT_BGRA4444)
        CASE(SDL_PIXELFORMAT_ARGB1555)
        CASE(SDL_PIXELFORMAT_RGBA5551)
        CASE(SDL_PIXELFORMAT_ABGR1555)
        CASE(SDL_PIXELFORMAT_BGRA5551)
        CASE(SDL_PIXELFORMAT_RGB565)
        CASE(SDL_PIXELFORMAT_BGR565)
        CASE(SDL_PIXELFORMAT_RGB24)
        CASE(SDL_PIXELFORMAT_BGR24)
        CASE(SDL_PIXELFORMAT_XRGB8888)
        CASE(SDL_PIXELFORMAT_RGBX8888)
        CASE(SDL_PIXELFORMAT_XBGR8888)
        CASE(SDL_PIXELFORMAT_BGRX8888)
        CASE(SDL_PIXELFORMAT_ARGB8888)
        CASE(SDL_PIXELFORMAT_RGBA8888)
        CASE(SDL_PIXELFORMAT_ABGR8888)
        CASE(SDL_PIXELFORMAT_BGRA8888)
        CASE(SDL_PIXELFORMAT_XRGB2101010)
        CASE(SDL_PIXELFORMAT_XBGR2101010)
        CASE(SDL_PIXELFORMAT_ARGB2101010)
        CASE(SDL_PIXELFORMAT_ABGR2101010)
        CASE(SDL_PIXELFORMAT_RGB48)
        CASE(SDL_PIXELFORMAT_BGR48)
        CASE(SDL_PIXELFORMAT_RGBA64)
        CASE(SDL_PIXELFORMAT_ARGB64)
        CASE(SDL_PIXELFORMAT_BGRA64)
        CASE(SDL_PIXELFORMAT_ABGR64)
        CASE(SDL_PIXELFORMAT_RGB48_FLOAT)
        CASE(SDL_PIXELFORMAT_BGR48_FLOAT)
        CASE(SDL_PIXELFORMAT_RGBA64_FLOAT)
        CASE(SDL_PIXELFORMAT_ARGB64_FLOAT)
        CASE(SDL_PIXELFORMAT_BGRA64_FLOAT)
        CASE(SDL_PIXELFORMAT_ABGR64_FLOAT)
        CASE(SDL_PIXELFORMAT_RGB96_FLOAT)
        CASE(SDL_PIXELFORMAT_BGR96_FLOAT)
        CASE(SDL_PIXELFORMAT_RGBA128_FLOAT)
        CASE(SDL_PIXELFORMAT_ARGB128_FLOAT)
        CASE(SDL_PIXELFORMAT_BGRA128_FLOAT)
        CASE(SDL_PIXELFORMAT_ABGR128_FLOAT)
        CASE(SDL_PIXELFORMAT_YV12)
        CASE(SDL_PIXELFORMAT_IYUV)
        CASE(SDL_PIXELFORMAT_YUY2)
        CASE(SDL_PIXELFORMAT_UYVY)
        CASE(SDL_PIXELFORMAT_YVYU)
        CASE(SDL_PIXELFORMAT_NV12)
        CASE(SDL_PIXELFORMAT_NV21)
        CASE(SDL_PIXELFORMAT_P010)
        CASE(SDL_PIXELFORMAT_EXTERNAL_OES)

    default:
        return "SDL_PIXELFORMAT_UNKNOWN";
    }
}
#undef CASE

SDL_bool SDL_GetMasksForPixelFormatEnum(SDL_PixelFormatEnum format, int *bpp, Uint32 *Rmask,
                                        Uint32 *Gmask, Uint32 *Bmask, Uint32 *Amask)
{
    Uint32 masks[4];

#if SDL_HAVE_YUV
    /* Partial support for SDL_Surface with FOURCC */
    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        /* Not a format that uses masks */
        *Rmask = *Gmask = *Bmask = *Amask = 0;
        // however, some of these are packed formats, and can legit declare bits-per-pixel!
        switch (format) {
            case SDL_PIXELFORMAT_YUY2:
            case SDL_PIXELFORMAT_UYVY:
            case SDL_PIXELFORMAT_YVYU:
                *bpp = 32;
                break;
            default:
                *bpp = 0;  // oh well.
        }
        return SDL_TRUE;
    }
#else
    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        SDL_SetError("SDL not built with YUV support");
        return SDL_FALSE;
    }
#endif

    /* Initialize the values here */
    if (SDL_BYTESPERPIXEL(format) <= 2) {
        *bpp = SDL_BITSPERPIXEL(format);
    } else {
        *bpp = SDL_BYTESPERPIXEL(format) * 8;
    }
    *Rmask = *Gmask = *Bmask = *Amask = 0;

    if (format == SDL_PIXELFORMAT_RGB24) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        *Rmask = 0x00FF0000;
        *Gmask = 0x0000FF00;
        *Bmask = 0x000000FF;
#else
        *Rmask = 0x000000FF;
        *Gmask = 0x0000FF00;
        *Bmask = 0x00FF0000;
#endif
        return SDL_TRUE;
    }

    if (format == SDL_PIXELFORMAT_BGR24) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        *Rmask = 0x000000FF;
        *Gmask = 0x0000FF00;
        *Bmask = 0x00FF0000;
#else
        *Rmask = 0x00FF0000;
        *Gmask = 0x0000FF00;
        *Bmask = 0x000000FF;
#endif
        return SDL_TRUE;
    }

    if (SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED8 &&
        SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED16 &&
        SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED32) {
        /* Not a format that uses masks */
        return SDL_TRUE;
    }

    switch (SDL_PIXELLAYOUT(format)) {
    case SDL_PACKEDLAYOUT_332:
        masks[0] = 0x00000000;
        masks[1] = 0x000000E0;
        masks[2] = 0x0000001C;
        masks[3] = 0x00000003;
        break;
    case SDL_PACKEDLAYOUT_4444:
        masks[0] = 0x0000F000;
        masks[1] = 0x00000F00;
        masks[2] = 0x000000F0;
        masks[3] = 0x0000000F;
        break;
    case SDL_PACKEDLAYOUT_1555:
        masks[0] = 0x00008000;
        masks[1] = 0x00007C00;
        masks[2] = 0x000003E0;
        masks[3] = 0x0000001F;
        break;
    case SDL_PACKEDLAYOUT_5551:
        masks[0] = 0x0000F800;
        masks[1] = 0x000007C0;
        masks[2] = 0x0000003E;
        masks[3] = 0x00000001;
        break;
    case SDL_PACKEDLAYOUT_565:
        masks[0] = 0x00000000;
        masks[1] = 0x0000F800;
        masks[2] = 0x000007E0;
        masks[3] = 0x0000001F;
        break;
    case SDL_PACKEDLAYOUT_8888:
        masks[0] = 0xFF000000;
        masks[1] = 0x00FF0000;
        masks[2] = 0x0000FF00;
        masks[3] = 0x000000FF;
        break;
    case SDL_PACKEDLAYOUT_2101010:
        masks[0] = 0xC0000000;
        masks[1] = 0x3FF00000;
        masks[2] = 0x000FFC00;
        masks[3] = 0x000003FF;
        break;
    case SDL_PACKEDLAYOUT_1010102:
        masks[0] = 0xFFC00000;
        masks[1] = 0x003FF000;
        masks[2] = 0x00000FFC;
        masks[3] = 0x00000003;
        break;
    default:
        SDL_SetError("Unknown pixel format");
        return SDL_FALSE;
    }

    switch (SDL_PIXELORDER(format)) {
    case SDL_PACKEDORDER_XRGB:
        *Rmask = masks[1];
        *Gmask = masks[2];
        *Bmask = masks[3];
        break;
    case SDL_PACKEDORDER_RGBX:
        *Rmask = masks[0];
        *Gmask = masks[1];
        *Bmask = masks[2];
        break;
    case SDL_PACKEDORDER_ARGB:
        *Amask = masks[0];
        *Rmask = masks[1];
        *Gmask = masks[2];
        *Bmask = masks[3];
        break;
    case SDL_PACKEDORDER_RGBA:
        *Rmask = masks[0];
        *Gmask = masks[1];
        *Bmask = masks[2];
        *Amask = masks[3];
        break;
    case SDL_PACKEDORDER_XBGR:
        *Bmask = masks[1];
        *Gmask = masks[2];
        *Rmask = masks[3];
        break;
    case SDL_PACKEDORDER_BGRX:
        *Bmask = masks[0];
        *Gmask = masks[1];
        *Rmask = masks[2];
        break;
    case SDL_PACKEDORDER_BGRA:
        *Bmask = masks[0];
        *Gmask = masks[1];
        *Rmask = masks[2];
        *Amask = masks[3];
        break;
    case SDL_PACKEDORDER_ABGR:
        *Amask = masks[0];
        *Bmask = masks[1];
        *Gmask = masks[2];
        *Rmask = masks[3];
        break;
    default:
        SDL_SetError("Unknown pixel format");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

SDL_PixelFormatEnum SDL_GetPixelFormatEnumForMasks(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    switch (bpp) {
    case 1:
        /* SDL defaults to MSB ordering */
        return SDL_PIXELFORMAT_INDEX1MSB;
    case 2:
        /* SDL defaults to MSB ordering */
        return SDL_PIXELFORMAT_INDEX2MSB;
    case 4:
        /* SDL defaults to MSB ordering */
        return SDL_PIXELFORMAT_INDEX4MSB;
    case 8:
        if (Rmask == 0xE0 &&
            Gmask == 0x1C &&
            Bmask == 0x03 &&
            Amask == 0x00) {
            return SDL_PIXELFORMAT_RGB332;
        }
        return SDL_PIXELFORMAT_INDEX8;
    case 12:
        if (Rmask == 0) {
            return SDL_PIXELFORMAT_RGB444;
        }
        if (Rmask == 0x0F00 &&
            Gmask == 0x00F0 &&
            Bmask == 0x000F &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_RGB444;
        }
        if (Rmask == 0x000F &&
            Gmask == 0x00F0 &&
            Bmask == 0x0F00 &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_BGR444;
        }
        break;
    case 15:
        if (Rmask == 0) {
            return SDL_PIXELFORMAT_RGB555;
        }
        SDL_FALLTHROUGH;
    case 16:
        if (Rmask == 0) {
            return SDL_PIXELFORMAT_RGB565;
        }
        if (Rmask == 0x7C00 &&
            Gmask == 0x03E0 &&
            Bmask == 0x001F &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_RGB555;
        }
        if (Rmask == 0x001F &&
            Gmask == 0x03E0 &&
            Bmask == 0x7C00 &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_BGR555;
        }
        if (Rmask == 0x0F00 &&
            Gmask == 0x00F0 &&
            Bmask == 0x000F &&
            Amask == 0xF000) {
            return SDL_PIXELFORMAT_ARGB4444;
        }
        if (Rmask == 0xF000 &&
            Gmask == 0x0F00 &&
            Bmask == 0x00F0 &&
            Amask == 0x000F) {
            return SDL_PIXELFORMAT_RGBA4444;
        }
        if (Rmask == 0x000F &&
            Gmask == 0x00F0 &&
            Bmask == 0x0F00 &&
            Amask == 0xF000) {
            return SDL_PIXELFORMAT_ABGR4444;
        }
        if (Rmask == 0x00F0 &&
            Gmask == 0x0F00 &&
            Bmask == 0xF000 &&
            Amask == 0x000F) {
            return SDL_PIXELFORMAT_BGRA4444;
        }
        if (Rmask == 0x7C00 &&
            Gmask == 0x03E0 &&
            Bmask == 0x001F &&
            Amask == 0x8000) {
            return SDL_PIXELFORMAT_ARGB1555;
        }
        if (Rmask == 0xF800 &&
            Gmask == 0x07C0 &&
            Bmask == 0x003E &&
            Amask == 0x0001) {
            return SDL_PIXELFORMAT_RGBA5551;
        }
        if (Rmask == 0x001F &&
            Gmask == 0x03E0 &&
            Bmask == 0x7C00 &&
            Amask == 0x8000) {
            return SDL_PIXELFORMAT_ABGR1555;
        }
        if (Rmask == 0x003E &&
            Gmask == 0x07C0 &&
            Bmask == 0xF800 &&
            Amask == 0x0001) {
            return SDL_PIXELFORMAT_BGRA5551;
        }
        if (Rmask == 0xF800 &&
            Gmask == 0x07E0 &&
            Bmask == 0x001F &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_RGB565;
        }
        if (Rmask == 0x001F &&
            Gmask == 0x07E0 &&
            Bmask == 0xF800 &&
            Amask == 0x0000) {
            return SDL_PIXELFORMAT_BGR565;
        }
        if (Rmask == 0x003F &&
            Gmask == 0x07C0 &&
            Bmask == 0xF800 &&
            Amask == 0x0000) {
            /* Technically this would be BGR556, but Witek says this works in bug 3158 */
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case 24:
        switch (Rmask) {
        case 0:
        case 0x00FF0000:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            return SDL_PIXELFORMAT_RGB24;
#else
            return SDL_PIXELFORMAT_BGR24;
#endif
        case 0x000000FF:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            return SDL_PIXELFORMAT_BGR24;
#else
            return SDL_PIXELFORMAT_RGB24;
#endif
        }
        break;
    case 30:
        if (Rmask == 0x3FF00000 &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x000003FF &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XRGB2101010;
        }
        if (Rmask == 0x000003FF &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x3FF00000 &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XBGR2101010;
        }
        break;
    case 32:
        if (Rmask == 0) {
            return SDL_PIXELFORMAT_XRGB8888;
        }
        if (Rmask == 0x00FF0000 &&
            Gmask == 0x0000FF00 &&
            Bmask == 0x000000FF &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XRGB8888;
        }
        if (Rmask == 0xFF000000 &&
            Gmask == 0x00FF0000 &&
            Bmask == 0x0000FF00 &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_RGBX8888;
        }
        if (Rmask == 0x000000FF &&
            Gmask == 0x0000FF00 &&
            Bmask == 0x00FF0000 &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XBGR8888;
        }
        if (Rmask == 0x0000FF00 &&
            Gmask == 0x00FF0000 &&
            Bmask == 0xFF000000 &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_BGRX8888;
        }
        if (Rmask == 0x00FF0000 &&
            Gmask == 0x0000FF00 &&
            Bmask == 0x000000FF &&
            Amask == 0xFF000000) {
            return SDL_PIXELFORMAT_ARGB8888;
        }
        if (Rmask == 0xFF000000 &&
            Gmask == 0x00FF0000 &&
            Bmask == 0x0000FF00 &&
            Amask == 0x000000FF) {
            return SDL_PIXELFORMAT_RGBA8888;
        }
        if (Rmask == 0x000000FF &&
            Gmask == 0x0000FF00 &&
            Bmask == 0x00FF0000 &&
            Amask == 0xFF000000) {
            return SDL_PIXELFORMAT_ABGR8888;
        }
        if (Rmask == 0x0000FF00 &&
            Gmask == 0x00FF0000 &&
            Bmask == 0xFF000000 &&
            Amask == 0x000000FF) {
            return SDL_PIXELFORMAT_BGRA8888;
        }
        if (Rmask == 0x3FF00000 &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x000003FF &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XRGB2101010;
        }
        if (Rmask == 0x000003FF &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x3FF00000 &&
            Amask == 0x00000000) {
            return SDL_PIXELFORMAT_XBGR2101010;
        }
        if (Rmask == 0x3FF00000 &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x000003FF &&
            Amask == 0xC0000000) {
            return SDL_PIXELFORMAT_ARGB2101010;
        }
        if (Rmask == 0x000003FF &&
            Gmask == 0x000FFC00 &&
            Bmask == 0x3FF00000 &&
            Amask == 0xC0000000) {
            return SDL_PIXELFORMAT_ABGR2101010;
        }
        break;
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}

static SDL_PixelFormat *formats;
static SDL_SpinLock formats_lock = 0;

SDL_PixelFormat *SDL_CreatePixelFormat(SDL_PixelFormatEnum pixel_format)
{
    SDL_PixelFormat *format;

    SDL_LockSpinlock(&formats_lock);

    /* Look it up in our list of previously allocated formats */
    for (format = formats; format; format = format->next) {
        if (pixel_format == format->format) {
            ++format->refcount;
            SDL_UnlockSpinlock(&formats_lock);
            return format;
        }
    }

    /* Allocate an empty pixel format structure, and initialize it */
    format = (SDL_PixelFormat *)SDL_malloc(sizeof(*format));
    if (!format) {
        SDL_UnlockSpinlock(&formats_lock);
        return NULL;
    }
    if (SDL_InitFormat(format, pixel_format) < 0) {
        SDL_UnlockSpinlock(&formats_lock);
        SDL_free(format);
        return NULL;
    }

    if (!SDL_ISPIXELFORMAT_INDEXED(pixel_format)) {
        /* Cache the RGB formats */
        format->next = formats;
        formats = format;
    }

    SDL_UnlockSpinlock(&formats_lock);

    return format;
}

int SDL_InitFormat(SDL_PixelFormat *format, SDL_PixelFormatEnum pixel_format)
{
    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 mask;

    if (!SDL_GetMasksForPixelFormatEnum(pixel_format, &bpp,
                                    &Rmask, &Gmask, &Bmask, &Amask)) {
        return -1;
    }

    /* Set up the format */
    SDL_zerop(format);
    format->format = pixel_format;
    format->bits_per_pixel = (Uint8)bpp;
    format->bytes_per_pixel = (Uint8)((bpp + 7) / 8);

    format->Rmask = Rmask;
    format->Rshift = 0;
    format->Rloss = 8;
    if (Rmask) {
        for (mask = Rmask; !(mask & 0x01); mask >>= 1) {
            ++format->Rshift;
        }
        for (; (mask & 0x01); mask >>= 1) {
            --format->Rloss;
        }
    }

    format->Gmask = Gmask;
    format->Gshift = 0;
    format->Gloss = 8;
    if (Gmask) {
        for (mask = Gmask; !(mask & 0x01); mask >>= 1) {
            ++format->Gshift;
        }
        for (; (mask & 0x01); mask >>= 1) {
            --format->Gloss;
        }
    }

    format->Bmask = Bmask;
    format->Bshift = 0;
    format->Bloss = 8;
    if (Bmask) {
        for (mask = Bmask; !(mask & 0x01); mask >>= 1) {
            ++format->Bshift;
        }
        for (; (mask & 0x01); mask >>= 1) {
            --format->Bloss;
        }
    }

    format->Amask = Amask;
    format->Ashift = 0;
    format->Aloss = 8;
    if (Amask) {
        for (mask = Amask; !(mask & 0x01); mask >>= 1) {
            ++format->Ashift;
        }
        for (; (mask & 0x01); mask >>= 1) {
            --format->Aloss;
        }
    }

    format->palette = NULL;
    format->refcount = 1;
    format->next = NULL;

    return 0;
}

void SDL_DestroyPixelFormat(SDL_PixelFormat *format)
{
    SDL_PixelFormat *prev;

    if (!format) {
        return;
    }

    SDL_LockSpinlock(&formats_lock);

    if (--format->refcount > 0) {
        SDL_UnlockSpinlock(&formats_lock);
        return;
    }

    /* Remove this format from our list */
    if (format == formats) {
        formats = format->next;
    } else if (formats) {
        for (prev = formats; prev->next; prev = prev->next) {
            if (prev->next == format) {
                prev->next = format->next;
                break;
            }
        }
    }

    SDL_UnlockSpinlock(&formats_lock);

    if (format->palette) {
        SDL_DestroyPalette(format->palette);
    }
    SDL_free(format);
    return;
}

SDL_Colorspace SDL_GetDefaultColorspaceForFormat(SDL_PixelFormatEnum format)
{
    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        if (format == SDL_PIXELFORMAT_P010) {
            return SDL_COLORSPACE_HDR10;
        } else {
            return SDL_COLORSPACE_YUV_DEFAULT;
        }
    } else if (SDL_ISPIXELFORMAT_FLOAT(format)) {
        return SDL_COLORSPACE_SRGB_LINEAR;
    } else if (SDL_ISPIXELFORMAT_10BIT(format)) {
        return SDL_COLORSPACE_HDR10;
    } else {
        return SDL_COLORSPACE_RGB_DEFAULT;
    }
}

float SDL_sRGBtoLinear(float v)
{
    if (v <= 0.04045f) {
        v = (v / 12.92f);
    } else {
        v = SDL_powf((v + 0.055f) / 1.055f, 2.4f);
    }
    return v;
}

float SDL_sRGBfromLinear(float v)
{
    if (v <= 0.0031308f) {
        v = (v * 12.92f);
    } else {
        v = (SDL_powf(v, 1.0f / 2.4f) * 1.055f - 0.055f);
    }
    return v;
}

float SDL_PQtoNits(float v)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float oo_m1 = 1.0f / 0.1593017578125f;
    const float oo_m2 = 1.0f / 78.84375f;

    float num = SDL_max(SDL_powf(v, oo_m2) - c1, 0.0f);
    float den = c2 - c3 * SDL_powf(v, oo_m2);
    return 10000.0f * SDL_powf(num / den, oo_m1);
}

float SDL_PQfromNits(float v)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;

    float y = SDL_clamp(v / 10000.0f, 0.0f, 1.0f);
    float num = c1 + c2 * SDL_powf(y, m1);
    float den = 1.0f + c3 * SDL_powf(y, m1);
    return SDL_powf(num / den, m2);
}

/* This is a helpful tool for deriving these:
 * https://kdashg.github.io/misc/colors/from-coeffs.html
 */
static const float mat_BT601_Limited_8bit[] = {
    -0.0627451017f, -0.501960814f, -0.501960814f, 0.0f, /* offset */
    1.1644f, 0.0000f, 1.5960f, 0.0f,                    /* Rcoeff */
    1.1644f, -0.3918f, -0.8130f, 0.0f,                  /* Gcoeff */
    1.1644f, 2.0172f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float mat_BT601_Full_8bit[] = {
    0.0f, -0.501960814f, -0.501960814f, 0.0f,           /* offset */
    1.0000f, 0.0000f, 1.4020f, 0.0f,                    /* Rcoeff */
    1.0000f, -0.3441f, -0.7141f, 0.0f,                  /* Gcoeff */
    1.0000f, 1.7720f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float mat_BT709_Limited_8bit[] = {
    -0.0627451017f, -0.501960814f, -0.501960814f, 0.0f, /* offset */
    1.1644f, 0.0000f, 1.7927f, 0.0f,                    /* Rcoeff */
    1.1644f, -0.2132f, -0.5329f, 0.0f,                  /* Gcoeff */
    1.1644f, 2.1124f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float mat_BT709_Full_8bit[] = {
    0.0f, -0.501960814f, -0.501960814f, 0.0f,           /* offset */
    1.0000f, 0.0000f, 1.5748f, 0.0f,                    /* Rcoeff */
    1.0000f, -0.1873f, -0.4681f, 0.0f,                  /* Gcoeff */
    1.0000f, 1.8556f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float mat_BT2020_Limited_10bit[] = {
    -0.062561095f, -0.500488759f, -0.500488759f, 0.0f,  /* offset */
    1.1678f, 0.0000f, 1.6836f, 0.0f,                    /* Rcoeff */
    1.1678f, -0.1879f, -0.6523f, 0.0f,                  /* Gcoeff */
    1.1678f, 2.1481f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float mat_BT2020_Full_10bit[] = {
    0.0f, -0.500488759f, -0.500488759f, 0.0f,           /* offset */
    1.0000f, 0.0000f, 1.4760f, 0.0f,                    /* Rcoeff */
    1.0000f, -0.1647f, -0.5719f, 0.0f,                  /* Gcoeff */
    1.0000f, 1.8832f, 0.0000f, 0.0f,                    /* Bcoeff */
};

static const float *SDL_GetBT601ConversionMatrix( SDL_Colorspace colorspace )
{
    switch (SDL_COLORSPACERANGE(colorspace)) {
    case SDL_COLOR_RANGE_LIMITED:
    case SDL_COLOR_RANGE_UNKNOWN:
        return mat_BT601_Limited_8bit;
    case SDL_COLOR_RANGE_FULL:
        return mat_BT601_Full_8bit;
    default:
        break;
    }
    return NULL;
}

static const float *SDL_GetBT709ConversionMatrix(SDL_Colorspace colorspace)
{
    switch (SDL_COLORSPACERANGE(colorspace)) {
    case SDL_COLOR_RANGE_LIMITED:
    case SDL_COLOR_RANGE_UNKNOWN:
        return mat_BT709_Limited_8bit;
    case SDL_COLOR_RANGE_FULL:
        return mat_BT709_Full_8bit;
    default:
        break;
    }
    return NULL;
}

static const float *SDL_GetBT2020ConversionMatrix(SDL_Colorspace colorspace)
{
    switch (SDL_COLORSPACERANGE(colorspace)) {
    case SDL_COLOR_RANGE_LIMITED:
    case SDL_COLOR_RANGE_UNKNOWN:
        return mat_BT2020_Limited_10bit;
    case SDL_COLOR_RANGE_FULL:
        return mat_BT2020_Full_10bit;
    default:
        break;
    }
    return NULL;
}

const float *SDL_GetYCbCRtoRGBConversionMatrix(SDL_Colorspace colorspace, int w, int h, int bits_per_pixel)
{
    const int YUV_SD_THRESHOLD = 576;

    switch (SDL_COLORSPACEMATRIX(colorspace)) {
    case SDL_MATRIX_COEFFICIENTS_BT601:
    case SDL_MATRIX_COEFFICIENTS_BT470BG:
        return SDL_GetBT601ConversionMatrix(colorspace);

    case SDL_MATRIX_COEFFICIENTS_BT709:
        return SDL_GetBT709ConversionMatrix(colorspace);

    case SDL_MATRIX_COEFFICIENTS_BT2020_NCL:
        return SDL_GetBT2020ConversionMatrix(colorspace);

    case SDL_MATRIX_COEFFICIENTS_UNSPECIFIED:
        switch (bits_per_pixel) {
        case 8:
            if (h <= YUV_SD_THRESHOLD) {
                return SDL_GetBT601ConversionMatrix(colorspace);
            } else {
                return SDL_GetBT709ConversionMatrix(colorspace);
            }
        case 10:
        case 16:
            return SDL_GetBT2020ConversionMatrix(colorspace);
        default:
            break;
        }
        break;
    default:
        break;
    }
    return NULL;
}

const float *SDL_GetColorPrimariesConversionMatrix(SDL_ColorPrimaries src, SDL_ColorPrimaries dst)
{
    /* Conversion matrices generated using gamescope color helpers and the primaries definitions at:
     * https://www.itu.int/rec/T-REC-H.273-201612-S/en
     *
     * You can also generate these online using the RGB-XYZ matrix calculator, and then multiplying
     * XYZ_to_dst * src_to_XYZ to get the combined conversion matrix:
     * https://www.russellcottrell.com/photo/matrixCalculator.htm
     */
    static const float mat601to709[] = {
        0.939542f, 0.050181f, 0.010277f,
        0.017772f, 0.965793f, 0.016435f,
        -0.001622f, -0.004370f, 1.005991f,
    };
    static const float mat601to2020[] = {
        0.595254f, 0.349314f, 0.055432f,
        0.081244f, 0.891503f, 0.027253f,
        0.015512f, 0.081912f, 0.902576f,
    };
    static const float mat709to601[] = {
        1.065379f, -0.055401f, -0.009978f,
        -0.019633f, 1.036363f, -0.016731f,
        0.001632f, 0.004412f, 0.993956f,
    };
    static const float mat709to2020[] = {
        0.627404f, 0.329283f, 0.043313f,
        0.069097f, 0.919541f, 0.011362f,
        0.016391f, 0.088013f, 0.895595f,
    };
    static const float mat2020to601[] = {
        1.776133f, -0.687820f, -0.088313f,
        -0.161376f, 1.187315f, -0.025940f,
        -0.015881f, -0.095931f, 1.111812f,
    };
    static const float mat2020to709[] = {
        1.660496f, -0.587656f, -0.072840f,
        -0.124547f, 1.132895f, -0.008348f,
        -0.018154f, -0.100597f, 1.118751f
    };
    static const float matSMPTE431to709[] = {
        1.120713f, -0.234649f, 0.000000f,
        -0.038478f, 1.087034f, 0.000000f,
        -0.017967f, -0.082030f, 0.954576f,
    };
    static const float matSMPTE431to2020[] = {
        0.689691f, 0.207169f, 0.041346f,
        0.041852f, 0.982426f, 0.010846f,
        -0.001107f, 0.018362f, 0.854914f,
    };
    static const float matSMPTE432to709[] = {
        1.224940f, -0.224940f, -0.000000f,
        -0.042057f, 1.042057f, 0.000000f,
        -0.019638f, -0.078636f, 1.098273f,
    };
    static const float matSMPTE432to2020[] = {
        0.753833f, 0.198597f, 0.047570f,
        0.045744f, 0.941777f, 0.012479f,
        -0.001210f, 0.017602f, 0.983609f,
    };

    switch (dst) {
    case SDL_COLOR_PRIMARIES_BT601:
    case SDL_COLOR_PRIMARIES_SMPTE240:
        switch (src) {
        case SDL_COLOR_PRIMARIES_BT709:
            return mat709to601;
        case SDL_COLOR_PRIMARIES_BT2020:
            return mat2020to601;
        default:
            break;
        }
        break;
    case SDL_COLOR_PRIMARIES_BT709:
        switch (src) {
        case SDL_COLOR_PRIMARIES_BT601:
        case SDL_COLOR_PRIMARIES_SMPTE240:
            return mat601to709;
        case SDL_COLOR_PRIMARIES_BT2020:
            return mat2020to709;
        case SDL_COLOR_PRIMARIES_SMPTE431:
            return matSMPTE431to709;
        case SDL_COLOR_PRIMARIES_SMPTE432:
            return matSMPTE432to709;
        default:
            break;
        }
        break;
    case SDL_COLOR_PRIMARIES_BT2020:
        switch (src) {
        case SDL_COLOR_PRIMARIES_BT601:
        case SDL_COLOR_PRIMARIES_SMPTE240:
            return mat601to2020;
        case SDL_COLOR_PRIMARIES_BT709:
            return mat709to2020;
        case SDL_COLOR_PRIMARIES_SMPTE431:
            return matSMPTE431to2020;
        case SDL_COLOR_PRIMARIES_SMPTE432:
            return matSMPTE432to2020;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return NULL;
}

void SDL_ConvertColorPrimaries(float *fR, float *fG, float *fB, const float *matrix)
{
    float v[3];

    v[0] = *fR;
    v[1] = *fG;
    v[2] = *fB;

    *fR = matrix[0 * 3 + 0] * v[0] + matrix[0 * 3 + 1] * v[1] + matrix[0 * 3 + 2] * v[2];
    *fG = matrix[1 * 3 + 0] * v[0] + matrix[1 * 3 + 1] * v[1] + matrix[1 * 3 + 2] * v[2];
    *fB = matrix[2 * 3 + 0] * v[0] + matrix[2 * 3 + 1] * v[1] + matrix[2 * 3 + 2] * v[2];
}

SDL_Palette *SDL_CreatePalette(int ncolors)
{
    SDL_Palette *palette;

    /* Input validation */
    if (ncolors < 1) {
        SDL_InvalidParamError("ncolors");
        return NULL;
    }

    palette = (SDL_Palette *)SDL_malloc(sizeof(*palette));
    if (!palette) {
        return NULL;
    }
    palette->colors =
        (SDL_Color *)SDL_malloc(ncolors * sizeof(*palette->colors));
    if (!palette->colors) {
        SDL_free(palette);
        return NULL;
    }
    palette->ncolors = ncolors;
    palette->version = 1;
    palette->refcount = 1;

    SDL_memset(palette->colors, 0xFF, ncolors * sizeof(*palette->colors));

    return palette;
}

int SDL_SetPixelFormatPalette(SDL_PixelFormat *format, SDL_Palette *palette)
{
    if (!format) {
        return SDL_InvalidParamError("SDL_SetPixelFormatPalette(): format");
    }

    if (palette && palette->ncolors > (1 << format->bits_per_pixel)) {
        return SDL_SetError("SDL_SetPixelFormatPalette() passed a palette that doesn't match the format");
    }

    if (format->palette == palette) {
        return 0;
    }

    if (format->palette) {
        SDL_DestroyPalette(format->palette);
    }

    format->palette = palette;

    if (format->palette) {
        ++format->palette->refcount;
    }

    return 0;
}

int SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors,
                         int firstcolor, int ncolors)
{
    int status = 0;

    /* Verify the parameters */
    if (!palette) {
        return -1;
    }
    if (ncolors > (palette->ncolors - firstcolor)) {
        ncolors = (palette->ncolors - firstcolor);
        status = -1;
    }

    if (colors != (palette->colors + firstcolor)) {
        SDL_memcpy(palette->colors + firstcolor, colors,
                   ncolors * sizeof(*colors));
    }
    ++palette->version;
    if (!palette->version) {
        palette->version = 1;
    }

    return status;
}

void SDL_DestroyPalette(SDL_Palette *palette)
{
    if (!palette) {
        return;
    }
    if (--palette->refcount > 0) {
        return;
    }
    SDL_free(palette->colors);
    SDL_free(palette);
}

/*
 * Calculate an 8-bit (3 red, 3 green, 2 blue) dithered palette of colors
 */
void SDL_DitherColors(SDL_Color *colors, int bpp)
{
    int i;
    if (bpp != 8) {
        return; /* only 8bpp supported right now */
    }

    for (i = 0; i < 256; i++) {
        int r, g, b;
        /* map each bit field to the full [0, 255] interval,
           so 0 is mapped to (0, 0, 0) and 255 to (255, 255, 255) */
        r = i & 0xe0;
        r |= r >> 3 | r >> 6;
        colors[i].r = (Uint8)r;
        g = (i << 3) & 0xe0;
        g |= g >> 3 | g >> 6;
        colors[i].g = (Uint8)g;
        b = i & 0x3;
        b |= b << 2;
        b |= b << 4;
        colors[i].b = (Uint8)b;
        colors[i].a = SDL_ALPHA_OPAQUE;
    }
}

/*
 * Match an RGB value to a particular palette index
 */
Uint8 SDL_FindColor(SDL_Palette *pal, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /* Do colorspace distance matching */
    unsigned int smallest;
    unsigned int distance;
    int rd, gd, bd, ad;
    int i;
    Uint8 pixel = 0;

    smallest = ~0U;
    for (i = 0; i < pal->ncolors; ++i) {
        rd = pal->colors[i].r - r;
        gd = pal->colors[i].g - g;
        bd = pal->colors[i].b - b;
        ad = pal->colors[i].a - a;
        distance = (rd * rd) + (gd * gd) + (bd * bd) + (ad * ad);
        if (distance < smallest) {
            pixel = (Uint8)i;
            if (distance == 0) { /* Perfect match! */
                break;
            }
            smallest = distance;
        }
    }
    return pixel;
}

/* Tell whether palette is opaque, and if it has an alpha_channel */
void SDL_DetectPalette(SDL_Palette *pal, SDL_bool *is_opaque, SDL_bool *has_alpha_channel)
{
    int i;

    {
        SDL_bool all_opaque = SDL_TRUE;
        for (i = 0; i < pal->ncolors; i++) {
            Uint8 alpha_value = pal->colors[i].a;
            if (alpha_value != SDL_ALPHA_OPAQUE) {
                all_opaque = SDL_FALSE;
                break;
            }
        }

        if (all_opaque) {
            /* Palette is opaque, with an alpha channel */
            *is_opaque = SDL_TRUE;
            *has_alpha_channel = SDL_TRUE;
            return;
        }
    }

    {
        SDL_bool all_transparent = SDL_TRUE;
        for (i = 0; i < pal->ncolors; i++) {
            Uint8 alpha_value = pal->colors[i].a;
            if (alpha_value != SDL_ALPHA_TRANSPARENT) {
                all_transparent = SDL_FALSE;
                break;
            }
        }

        if (all_transparent) {
            /* Palette is opaque, without an alpha channel */
            *is_opaque = SDL_TRUE;
            *has_alpha_channel = SDL_FALSE;
            return;
        }
    }

    /* Palette has alpha values */
    *is_opaque = SDL_FALSE;
    *has_alpha_channel = SDL_TRUE;
}

/* Find the opaque pixel value corresponding to an RGB triple */
Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    if (!format) {
        SDL_InvalidParamError("format");
        return 0;
    }
    if (!format->palette) {
        return (r >> format->Rloss) << format->Rshift | (g >> format->Gloss) << format->Gshift | (b >> format->Bloss) << format->Bshift | format->Amask;
    } else {
        return SDL_FindColor(format->palette, r, g, b, SDL_ALPHA_OPAQUE);
    }
}

/* Find the pixel value corresponding to an RGBA quadruple */
Uint32 SDL_MapRGBA(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b,
            Uint8 a)
{
    if (!format) {
        SDL_InvalidParamError("format");
        return 0;
    }
    if (!format->palette) {
        return (r >> format->Rloss) << format->Rshift | (g >> format->Gloss) << format->Gshift | (b >> format->Bloss) << format->Bshift | ((Uint32)(a >> format->Aloss) << format->Ashift & format->Amask);
    } else {
        return SDL_FindColor(format->palette, r, g, b, a);
    }
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g,
                Uint8 *b)
{
    if (!format->palette) {
        unsigned v;
        v = (pixel & format->Rmask) >> format->Rshift;
        *r = SDL_expand_byte[format->Rloss][v];
        v = (pixel & format->Gmask) >> format->Gshift;
        *g = SDL_expand_byte[format->Gloss][v];
        v = (pixel & format->Bmask) >> format->Bshift;
        *b = SDL_expand_byte[format->Bloss][v];
    } else {
        if (pixel < (unsigned)format->palette->ncolors) {
            *r = format->palette->colors[pixel].r;
            *g = format->palette->colors[pixel].g;
            *b = format->palette->colors[pixel].b;
        } else {
            *r = *g = *b = 0;
        }
    }
}

void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *format,
                 Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    if (!format->palette) {
        unsigned v;
        v = (pixel & format->Rmask) >> format->Rshift;
        *r = SDL_expand_byte[format->Rloss][v];
        v = (pixel & format->Gmask) >> format->Gshift;
        *g = SDL_expand_byte[format->Gloss][v];
        v = (pixel & format->Bmask) >> format->Bshift;
        *b = SDL_expand_byte[format->Bloss][v];
        v = (pixel & format->Amask) >> format->Ashift;
        *a = SDL_expand_byte[format->Aloss][v];
    } else {
        if (pixel < (unsigned)format->palette->ncolors) {
            *r = format->palette->colors[pixel].r;
            *g = format->palette->colors[pixel].g;
            *b = format->palette->colors[pixel].b;
            *a = format->palette->colors[pixel].a;
        } else {
            *r = *g = *b = *a = 0;
        }
    }
}

/* Map from Palette to Palette */
static Uint8 *Map1to1(SDL_Palette *src, SDL_Palette *dst, int *identical)
{
    Uint8 *map;
    int i;

    if (identical) {
        if (src->ncolors <= dst->ncolors) {
            /* If an identical palette, no need to map */
            if (src == dst ||
                (SDL_memcmp(src->colors, dst->colors,
                            src->ncolors * sizeof(SDL_Color)) == 0)) {
                *identical = 1;
                return NULL;
            }
        }
        *identical = 0;
    }
    map = (Uint8 *)SDL_calloc(256, sizeof(Uint8));
    if (!map) {
        return NULL;
    }
    for (i = 0; i < src->ncolors; ++i) {
        map[i] = SDL_FindColor(dst,
                               src->colors[i].r, src->colors[i].g,
                               src->colors[i].b, src->colors[i].a);
    }
    return map;
}

/* Map from Palette to BitField */
static Uint8 *Map1toN(SDL_PixelFormat *src, Uint8 Rmod, Uint8 Gmod, Uint8 Bmod, Uint8 Amod,
                      SDL_PixelFormat *dst)
{
    Uint8 *map;
    int i;
    int bpp;
    SDL_Palette *pal = src->palette;

    bpp = ((dst->bytes_per_pixel == 3) ? 4 : dst->bytes_per_pixel);
    map = (Uint8 *)SDL_calloc(256, bpp);
    if (!map) {
        return NULL;
    }

    /* We memory copy to the pixel map so the endianness is preserved */
    for (i = 0; i < pal->ncolors; ++i) {
        Uint8 R = (Uint8)((pal->colors[i].r * Rmod) / 255);
        Uint8 G = (Uint8)((pal->colors[i].g * Gmod) / 255);
        Uint8 B = (Uint8)((pal->colors[i].b * Bmod) / 255);
        Uint8 A = (Uint8)((pal->colors[i].a * Amod) / 255);
        ASSEMBLE_RGBA(&map[i * bpp], dst->bytes_per_pixel, dst, (Uint32)R,
                      (Uint32)G, (Uint32)B, (Uint32)A);
    }
    return map;
}

/* Map from BitField to Dithered-Palette to Palette */
static Uint8 *MapNto1(SDL_PixelFormat *src, SDL_PixelFormat *dst, int *identical)
{
    /* Generate a 256 color dither palette */
    SDL_Palette dithered;
    SDL_Color colors[256];
    SDL_Palette *pal = dst->palette;

    dithered.ncolors = 256;
    SDL_DitherColors(colors, 8);
    dithered.colors = colors;
    return Map1to1(&dithered, pal, identical);
}

SDL_BlitMap *SDL_AllocBlitMap(void)
{
    SDL_BlitMap *map;

    /* Allocate the empty map */
    map = (SDL_BlitMap *)SDL_calloc(1, sizeof(*map));
    if (!map) {
        return NULL;
    }
    map->info.r = 0xFF;
    map->info.g = 0xFF;
    map->info.b = 0xFF;
    map->info.a = 0xFF;

    /* It's ready to go */
    return map;
}

void SDL_InvalidateAllBlitMap(SDL_Surface *surface)
{
    SDL_ListNode *l = (SDL_ListNode *)surface->list_blitmap;

    surface->list_blitmap = NULL;

    while (l) {
        SDL_ListNode *tmp = l;
        SDL_InvalidateMap((SDL_BlitMap *)l->entry);
        l = l->next;
        SDL_free(tmp);
    }
}

void SDL_InvalidateMap(SDL_BlitMap *map)
{
    if (!map) {
        return;
    }
    if (map->dst) {
        /* Un-register from the destination surface */
        SDL_ListRemove((SDL_ListNode **)&(map->dst->list_blitmap), map);
    }
    map->dst = NULL;
    map->src_palette_version = 0;
    map->dst_palette_version = 0;
    SDL_free(map->info.table);
    map->info.table = NULL;
}

int SDL_MapSurface(SDL_Surface *src, SDL_Surface *dst)
{
    SDL_PixelFormat *srcfmt;
    SDL_PixelFormat *dstfmt;
    SDL_BlitMap *map;

    /* Clear out any previous mapping */
    map = src->map;
#if SDL_HAVE_RLE
    if ((src->flags & SDL_RLEACCEL) == SDL_RLEACCEL) {
        SDL_UnRLESurface(src, 1);
    }
#endif
    SDL_InvalidateMap(map);

    /* Figure out what kind of mapping we're doing */
    map->identity = 0;
    srcfmt = src->format;
    dstfmt = dst->format;
    if (SDL_ISPIXELFORMAT_INDEXED(srcfmt->format)) {
        if (SDL_ISPIXELFORMAT_INDEXED(dstfmt->format)) {
            /* Palette --> Palette */
            map->info.table =
                Map1to1(srcfmt->palette, dstfmt->palette, &map->identity);
            if (!map->identity) {
                if (!map->info.table) {
                    return -1;
                }
            }
            if (srcfmt->bits_per_pixel != dstfmt->bits_per_pixel) {
                map->identity = 0;
            }
        } else {
            /* Palette --> BitField */
            map->info.table =
                Map1toN(srcfmt, src->map->info.r, src->map->info.g,
                        src->map->info.b, src->map->info.a, dstfmt);
            if (!map->info.table) {
                return -1;
            }
        }
    } else {
        if (SDL_ISPIXELFORMAT_INDEXED(dstfmt->format)) {
            /* BitField --> Palette */
            map->info.table = MapNto1(srcfmt, dstfmt, &map->identity);
            if (!map->identity) {
                if (!map->info.table) {
                    return -1;
                }
            }
            map->identity = 0; /* Don't optimize to copy */
        } else {
            /* BitField --> BitField */
            if (srcfmt == dstfmt) {
                map->identity = 1;
            }
        }
    }

    map->dst = dst;

    if (map->dst) {
        /* Register BlitMap to the destination surface, to be invalidated when needed */
        SDL_ListAdd((SDL_ListNode **)&(map->dst->list_blitmap), map);
    }

    if (dstfmt->palette) {
        map->dst_palette_version = dstfmt->palette->version;
    } else {
        map->dst_palette_version = 0;
    }

    if (srcfmt->palette) {
        map->src_palette_version = srcfmt->palette->version;
    } else {
        map->src_palette_version = 0;
    }

    /* Choose your blitters wisely */
    return SDL_CalculateBlit(src);
}

void SDL_FreeBlitMap(SDL_BlitMap *map)
{
    if (map) {
        SDL_InvalidateMap(map);
        SDL_free(map);
    }
}

