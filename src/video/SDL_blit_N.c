/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_HAVE_BLIT_N

#include "SDL_pixels_c.h"
#include "SDL_surface_c.h"
#include "SDL_blit_copy.h"

// General optimized routines that write char by char
#define HAVE_FAST_WRITE_INT8 1

// On some CPU, it's slower than combining and write a word
#ifdef __MIPS__
#undef HAVE_FAST_WRITE_INT8
#define HAVE_FAST_WRITE_INT8 0
#endif

// Functions to blit from N-bit surfaces to other surfaces

#define BLIT_FEATURE_NONE                       0x00
#define BLIT_FEATURE_HAS_SSE41                  0x01
#define BLIT_FEATURE_HAS_ALTIVEC                0x02
#define BLIT_FEATURE_ALTIVEC_DONT_USE_PREFETCH  0x04

#ifdef SDL_ALTIVEC_BLITTERS
#ifdef SDL_PLATFORM_MACOS
#include <sys/sysctl.h>
static size_t GetL3CacheSize(void)
{
    const char key[] = "hw.l3cachesize";
    u_int64_t result = 0;
    size_t typeSize = sizeof(result);

    int err = sysctlbyname(key, &result, &typeSize, NULL, 0);
    if (0 != err) {
        return 0;
    }

    return result;
}
#else
static size_t GetL3CacheSize(void)
{
    // XXX: Just guess G4
    return 2097152;
}
#endif // SDL_PLATFORM_MACOS

#if (defined(SDL_PLATFORM_MACOS) && (__GNUC__ < 4))
#define VECUINT8_LITERAL(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    (vector unsigned char)(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)
#define VECUINT16_LITERAL(a, b, c, d, e, f, g, h) \
    (vector unsigned short)(a, b, c, d, e, f, g, h)
#else
#define VECUINT8_LITERAL(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    (vector unsigned char)                                               \
    {                                                                    \
        a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p                   \
    }
#define VECUINT16_LITERAL(a, b, c, d, e, f, g, h) \
    (vector unsigned short)                       \
    {                                             \
        a, b, c, d, e, f, g, h                    \
    }
#endif

#define UNALIGNED_PTR(x)       (((size_t)x) & 0x0000000F)
#define VSWIZZLE32(a, b, c, d) (vector unsigned char)(0x00 + a, 0x00 + b, 0x00 + c, 0x00 + d, \
                                                      0x04 + a, 0x04 + b, 0x04 + c, 0x04 + d, \
                                                      0x08 + a, 0x08 + b, 0x08 + c, 0x08 + d, \
                                                      0x0C + a, 0x0C + b, 0x0C + c, 0x0C + d)

#define MAKE8888(dstfmt, r, g, b, a)           \
    (((r << dstfmt->Rshift) & dstfmt->Rmask) | \
     ((g << dstfmt->Gshift) & dstfmt->Gmask) | \
     ((b << dstfmt->Bshift) & dstfmt->Bmask) | \
     ((a << dstfmt->Ashift) & dstfmt->Amask))

/*
 * Data Stream Touch...Altivec cache prefetching.
 *
 *  Don't use this on a G5...however, the speed boost is very significant
 *   on a G4.
 */
#define DST_CHAN_SRC  1
#define DST_CHAN_DEST 2

// macro to set DST control word value...
#define DST_CTRL(size, count, stride) \
    (((size) << 24) | ((count) << 16) | (stride))

#define VEC_ALIGNER(src) ((UNALIGNED_PTR(src))   \
                              ? vec_lvsl(0, src) \
                              : vec_add(vec_lvsl(8, src), vec_splat_u8(8)))

// Calculate the permute vector used for 32->32 swizzling
static vector unsigned char calc_swizzle32(const SDL_PixelFormatDetails *srcfmt, const SDL_PixelFormatDetails *dstfmt)
{
    /*
     * We have to assume that the bits that aren't used by other
     *  colors is alpha, and it's one complete byte, since some formats
     *  leave alpha with a zero mask, but we should still swizzle the bits.
     */
    // ARGB
    static const SDL_PixelFormatDetails default_pixel_format = {
        SDL_PIXELFORMAT_ARGB8888, 0, 0, { 0, 0 }, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 8, 8, 8, 8, 16, 8, 0, 24
    };
    const vector unsigned char plus = VECUINT8_LITERAL(0x00, 0x00, 0x00, 0x00,
                                                       0x04, 0x04, 0x04, 0x04,
                                                       0x08, 0x08, 0x08, 0x08,
                                                       0x0C, 0x0C, 0x0C,
                                                       0x0C);
    vector unsigned char vswiz;
    vector unsigned int srcvec;
    Uint32 rmask, gmask, bmask, amask;

    if (!srcfmt) {
        srcfmt = &default_pixel_format;
    }
    if (!dstfmt) {
        dstfmt = &default_pixel_format;
    }

#define RESHIFT(X) (3 - ((X) >> 3))
    rmask = RESHIFT(srcfmt->Rshift) << (dstfmt->Rshift);
    gmask = RESHIFT(srcfmt->Gshift) << (dstfmt->Gshift);
    bmask = RESHIFT(srcfmt->Bshift) << (dstfmt->Bshift);

    // Use zero for alpha if either surface doesn't have alpha
    if (dstfmt->Amask) {
        amask =
            ((srcfmt->Amask) ? RESHIFT(srcfmt->Ashift) : 0x10) << (dstfmt->Ashift);
    } else {
        amask =
            0x10101010 & ((dstfmt->Rmask | dstfmt->Gmask | dstfmt->Bmask) ^
                          0xFFFFFFFF);
    }
#undef RESHIFT

    ((unsigned int *)(char *)&srcvec)[0] = (rmask | gmask | bmask | amask);
    vswiz = vec_add(plus, (vector unsigned char)vec_splat(srcvec, 0));
    return (vswiz);
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
// reorder bytes for PowerPC little endian
static vector unsigned char reorder_ppc64le_vec(vector unsigned char vpermute)
{
    /* The result vector of calc_swizzle32 reorder bytes using vec_perm.
       The LE transformation for vec_perm has an implicit assumption
       that the permutation is being used to reorder vector elements,
       not to reorder bytes within those elements.
       Unfortunately the result order is not the expected one for powerpc
       little endian when the two first vector parameters of vec_perm are
       not of type 'vector char'. This is because the numbering from the
       left for BE, and numbering from the right for LE, produces a
       different interpretation of what the odd and even lanes are.
       Refer to fedora bug 1392465
     */

    const vector unsigned char ppc64le_reorder = VECUINT8_LITERAL(
        0x01, 0x00, 0x03, 0x02,
        0x05, 0x04, 0x07, 0x06,
        0x09, 0x08, 0x0B, 0x0A,
        0x0D, 0x0C, 0x0F, 0x0E);

    vector unsigned char vswiz_ppc64le;
    vswiz_ppc64le = vec_perm(vpermute, vpermute, ppc64le_reorder);
    return (vswiz_ppc64le);
}
#endif

static void Blit_XRGB8888_RGB565(SDL_BlitInfo *info);
static void Blit_XRGB8888_RGB565Altivec(SDL_BlitInfo *info)
{
    int height = info->dst_h;
    Uint8 *src = (Uint8 *)info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = (Uint8 *)info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    vector unsigned char valpha = vec_splat_u8(0);
    vector unsigned char vpermute = calc_swizzle32(srcfmt, NULL);
    vector unsigned char vgmerge = VECUINT8_LITERAL(0x00, 0x02, 0x00, 0x06,
                                                    0x00, 0x0a, 0x00, 0x0e,
                                                    0x00, 0x12, 0x00, 0x16,
                                                    0x00, 0x1a, 0x00, 0x1e);
    vector unsigned short v1 = vec_splat_u16(1);
    vector unsigned short v3 = vec_splat_u16(3);
    vector unsigned short v3f =
        VECUINT16_LITERAL(0x003f, 0x003f, 0x003f, 0x003f,
                          0x003f, 0x003f, 0x003f, 0x003f);
    vector unsigned short vfc =
        VECUINT16_LITERAL(0x00fc, 0x00fc, 0x00fc, 0x00fc,
                          0x00fc, 0x00fc, 0x00fc, 0x00fc);
    vector unsigned short vf800 = (vector unsigned short)vec_splat_u8(-7);
    vf800 = vec_sl(vf800, vec_splat_u16(8));

    while (height--) {
        vector unsigned char valigner;
        vector unsigned char voverflow;
        vector unsigned char vsrc;

        int width = info->dst_w;
        int extrawidth;

        // do scalar until we can align...
#define ONE_PIXEL_BLEND(condition, widthvar)           \
    while (condition) {                                \
        Uint32 Pixel;                                  \
        unsigned sR, sG, sB, sA;                       \
        DISEMBLE_RGBA((Uint8 *)src, 4, srcfmt, Pixel,  \
                      sR, sG, sB, sA);                 \
        *(Uint16 *)(dst) = (((sR << 8) & 0x0000F800) | \
                            ((sG << 3) & 0x000007E0) | \
                            ((sB >> 3) & 0x0000001F)); \
        dst += 2;                                      \
        src += 4;                                      \
        widthvar--;                                    \
    }

        ONE_PIXEL_BLEND(((UNALIGNED_PTR(dst)) && (width)), width);

        // After all that work, here's the vector part!
        extrawidth = (width % 8); // trailing unaligned stores
        width -= extrawidth;
        vsrc = vec_ld(0, src);
        valigner = VEC_ALIGNER(src);

        while (width) {
            vector unsigned short vpixel, vrpixel, vgpixel, vbpixel;
            vector unsigned int vsrc1, vsrc2;
            vector unsigned char vdst;

            voverflow = vec_ld(15, src);
            vsrc = vec_perm(vsrc, voverflow, valigner);
            vsrc1 = (vector unsigned int)vec_perm(vsrc, valpha, vpermute);
            src += 16;
            vsrc = voverflow;
            voverflow = vec_ld(15, src);
            vsrc = vec_perm(vsrc, voverflow, valigner);
            vsrc2 = (vector unsigned int)vec_perm(vsrc, valpha, vpermute);
            // 1555
            vpixel = (vector unsigned short)vec_packpx(vsrc1, vsrc2);
            vgpixel = (vector unsigned short)vec_perm(vsrc1, vsrc2, vgmerge);
            vgpixel = vec_and(vgpixel, vfc);
            vgpixel = vec_sl(vgpixel, v3);
            vrpixel = vec_sl(vpixel, v1);
            vrpixel = vec_and(vrpixel, vf800);
            vbpixel = vec_and(vpixel, v3f);
            vdst =
                vec_or((vector unsigned char)vrpixel,
                       (vector unsigned char)vgpixel);
            // 565
            vdst = vec_or(vdst, (vector unsigned char)vbpixel);
            vec_st(vdst, 0, dst);

            width -= 8;
            src += 16;
            dst += 16;
            vsrc = voverflow;
        }

        SDL_assert(width == 0);

        // do scalar until we can align...
        ONE_PIXEL_BLEND((extrawidth), extrawidth);
#undef ONE_PIXEL_BLEND

        src += srcskip; // move to next row, accounting for pitch.
        dst += dstskip;
    }
}

static void Blit_RGB565_32Altivec(SDL_BlitInfo *info)
{
    int height = info->dst_h;
    Uint8 *src = (Uint8 *)info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = (Uint8 *)info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    unsigned alpha;
    vector unsigned char valpha;
    vector unsigned char vpermute;
    vector unsigned short vf800;
    vector unsigned int v8 = vec_splat_u32(8);
    vector unsigned int v16 = vec_add(v8, v8);
    vector unsigned short v2 = vec_splat_u16(2);
    vector unsigned short v3 = vec_splat_u16(3);
    /*
       0x10 - 0x1f is the alpha
       0x00 - 0x0e evens are the red
       0x01 - 0x0f odds are zero
     */
    vector unsigned char vredalpha1 = VECUINT8_LITERAL(0x10, 0x00, 0x01, 0x01,
                                                       0x10, 0x02, 0x01, 0x01,
                                                       0x10, 0x04, 0x01, 0x01,
                                                       0x10, 0x06, 0x01,
                                                       0x01);
    vector unsigned char vredalpha2 =
        (vector unsigned char)(vec_add((vector unsigned int)vredalpha1, vec_sl(v8, v16)));
    /*
       0x00 - 0x0f is ARxx ARxx ARxx ARxx
       0x11 - 0x0f odds are blue
     */
    vector unsigned char vblue1 = VECUINT8_LITERAL(0x00, 0x01, 0x02, 0x11,
                                                   0x04, 0x05, 0x06, 0x13,
                                                   0x08, 0x09, 0x0a, 0x15,
                                                   0x0c, 0x0d, 0x0e, 0x17);
    vector unsigned char vblue2 =
        (vector unsigned char)(vec_add((vector unsigned int)vblue1, v8));
    /*
       0x00 - 0x0f is ARxB ARxB ARxB ARxB
       0x10 - 0x0e evens are green
     */
    vector unsigned char vgreen1 = VECUINT8_LITERAL(0x00, 0x01, 0x10, 0x03,
                                                    0x04, 0x05, 0x12, 0x07,
                                                    0x08, 0x09, 0x14, 0x0b,
                                                    0x0c, 0x0d, 0x16, 0x0f);
    vector unsigned char vgreen2 =
        (vector unsigned char)(vec_add((vector unsigned int)vgreen1, vec_sl(v8, v8)));

    SDL_assert(srcfmt->bytes_per_pixel == 2);
    SDL_assert(dstfmt->bytes_per_pixel == 4);

    vf800 = (vector unsigned short)vec_splat_u8(-7);
    vf800 = vec_sl(vf800, vec_splat_u16(8));

    if (dstfmt->Amask && info->a) {
        ((unsigned char *)&valpha)[0] = alpha = info->a;
        valpha = vec_splat(valpha, 0);
    } else {
        alpha = 0;
        valpha = vec_splat_u8(0);
    }

    vpermute = calc_swizzle32(NULL, dstfmt);
    while (height--) {
        vector unsigned char valigner;
        vector unsigned char voverflow;
        vector unsigned char vsrc;

        int width = info->dst_w;
        int extrawidth;

        // do scalar until we can align...
#define ONE_PIXEL_BLEND(condition, widthvar)              \
    while (condition) {                                   \
        unsigned sR, sG, sB;                              \
        unsigned short Pixel = *((unsigned short *)src);  \
        sR = (Pixel >> 8) & 0xf8;                         \
        sG = (Pixel >> 3) & 0xfc;                         \
        sB = (Pixel << 3) & 0xf8;                         \
        ASSEMBLE_RGBA(dst, 4, dstfmt, sR, sG, sB, alpha); \
        src += 2;                                         \
        dst += 4;                                         \
        widthvar--;                                       \
    }
        ONE_PIXEL_BLEND(((UNALIGNED_PTR(dst)) && (width)), width);

        // After all that work, here's the vector part!
        extrawidth = (width % 8); // trailing unaligned stores
        width -= extrawidth;
        vsrc = vec_ld(0, src);
        valigner = VEC_ALIGNER(src);

        while (width) {
            vector unsigned short vR, vG, vB;
            vector unsigned char vdst1, vdst2;

            voverflow = vec_ld(15, src);
            vsrc = vec_perm(vsrc, voverflow, valigner);

            vR = vec_and((vector unsigned short)vsrc, vf800);
            vB = vec_sl((vector unsigned short)vsrc, v3);
            vG = vec_sl(vB, v2);

            vdst1 =
                (vector unsigned char)vec_perm((vector unsigned char)vR,
                                               valpha, vredalpha1);
            vdst1 = vec_perm(vdst1, (vector unsigned char)vB, vblue1);
            vdst1 = vec_perm(vdst1, (vector unsigned char)vG, vgreen1);
            vdst1 = vec_perm(vdst1, valpha, vpermute);
            vec_st(vdst1, 0, dst);

            vdst2 =
                (vector unsigned char)vec_perm((vector unsigned char)vR,
                                               valpha, vredalpha2);
            vdst2 = vec_perm(vdst2, (vector unsigned char)vB, vblue2);
            vdst2 = vec_perm(vdst2, (vector unsigned char)vG, vgreen2);
            vdst2 = vec_perm(vdst2, valpha, vpermute);
            vec_st(vdst2, 16, dst);

            width -= 8;
            dst += 32;
            src += 16;
            vsrc = voverflow;
        }

        SDL_assert(width == 0);

        // do scalar until we can align...
        ONE_PIXEL_BLEND((extrawidth), extrawidth);
#undef ONE_PIXEL_BLEND

        src += srcskip; // move to next row, accounting for pitch.
        dst += dstskip;
    }
}

static void Blit_RGB555_32Altivec(SDL_BlitInfo *info)
{
    int height = info->dst_h;
    Uint8 *src = (Uint8 *)info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = (Uint8 *)info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    unsigned alpha;
    vector unsigned char valpha;
    vector unsigned char vpermute;
    vector unsigned short vf800;
    vector unsigned int v8 = vec_splat_u32(8);
    vector unsigned int v16 = vec_add(v8, v8);
    vector unsigned short v1 = vec_splat_u16(1);
    vector unsigned short v3 = vec_splat_u16(3);
    /*
       0x10 - 0x1f is the alpha
       0x00 - 0x0e evens are the red
       0x01 - 0x0f odds are zero
     */
    vector unsigned char vredalpha1 = VECUINT8_LITERAL(0x10, 0x00, 0x01, 0x01,
                                                       0x10, 0x02, 0x01, 0x01,
                                                       0x10, 0x04, 0x01, 0x01,
                                                       0x10, 0x06, 0x01,
                                                       0x01);
    vector unsigned char vredalpha2 =
        (vector unsigned char)(vec_add((vector unsigned int)vredalpha1, vec_sl(v8, v16)));
    /*
       0x00 - 0x0f is ARxx ARxx ARxx ARxx
       0x11 - 0x0f odds are blue
     */
    vector unsigned char vblue1 = VECUINT8_LITERAL(0x00, 0x01, 0x02, 0x11,
                                                   0x04, 0x05, 0x06, 0x13,
                                                   0x08, 0x09, 0x0a, 0x15,
                                                   0x0c, 0x0d, 0x0e, 0x17);
    vector unsigned char vblue2 =
        (vector unsigned char)(vec_add((vector unsigned int)vblue1, v8));
    /*
       0x00 - 0x0f is ARxB ARxB ARxB ARxB
       0x10 - 0x0e evens are green
     */
    vector unsigned char vgreen1 = VECUINT8_LITERAL(0x00, 0x01, 0x10, 0x03,
                                                    0x04, 0x05, 0x12, 0x07,
                                                    0x08, 0x09, 0x14, 0x0b,
                                                    0x0c, 0x0d, 0x16, 0x0f);
    vector unsigned char vgreen2 =
        (vector unsigned char)(vec_add((vector unsigned int)vgreen1, vec_sl(v8, v8)));

    SDL_assert(srcfmt->bytes_per_pixel == 2);
    SDL_assert(dstfmt->bytes_per_pixel == 4);

    vf800 = (vector unsigned short)vec_splat_u8(-7);
    vf800 = vec_sl(vf800, vec_splat_u16(8));

    if (dstfmt->Amask && info->a) {
        ((unsigned char *)&valpha)[0] = alpha = info->a;
        valpha = vec_splat(valpha, 0);
    } else {
        alpha = 0;
        valpha = vec_splat_u8(0);
    }

    vpermute = calc_swizzle32(NULL, dstfmt);
    while (height--) {
        vector unsigned char valigner;
        vector unsigned char voverflow;
        vector unsigned char vsrc;

        int width = info->dst_w;
        int extrawidth;

        // do scalar until we can align...
#define ONE_PIXEL_BLEND(condition, widthvar)              \
    while (condition) {                                   \
        unsigned sR, sG, sB;                              \
        unsigned short Pixel = *((unsigned short *)src);  \
        sR = (Pixel >> 7) & 0xf8;                         \
        sG = (Pixel >> 2) & 0xf8;                         \
        sB = (Pixel << 3) & 0xf8;                         \
        ASSEMBLE_RGBA(dst, 4, dstfmt, sR, sG, sB, alpha); \
        src += 2;                                         \
        dst += 4;                                         \
        widthvar--;                                       \
    }
        ONE_PIXEL_BLEND(((UNALIGNED_PTR(dst)) && (width)), width);

        // After all that work, here's the vector part!
        extrawidth = (width % 8); // trailing unaligned stores
        width -= extrawidth;
        vsrc = vec_ld(0, src);
        valigner = VEC_ALIGNER(src);

        while (width) {
            vector unsigned short vR, vG, vB;
            vector unsigned char vdst1, vdst2;

            voverflow = vec_ld(15, src);
            vsrc = vec_perm(vsrc, voverflow, valigner);

            vR = vec_and(vec_sl((vector unsigned short)vsrc, v1), vf800);
            vB = vec_sl((vector unsigned short)vsrc, v3);
            vG = vec_sl(vB, v3);

            vdst1 =
                (vector unsigned char)vec_perm((vector unsigned char)vR,
                                               valpha, vredalpha1);
            vdst1 = vec_perm(vdst1, (vector unsigned char)vB, vblue1);
            vdst1 = vec_perm(vdst1, (vector unsigned char)vG, vgreen1);
            vdst1 = vec_perm(vdst1, valpha, vpermute);
            vec_st(vdst1, 0, dst);

            vdst2 =
                (vector unsigned char)vec_perm((vector unsigned char)vR,
                                               valpha, vredalpha2);
            vdst2 = vec_perm(vdst2, (vector unsigned char)vB, vblue2);
            vdst2 = vec_perm(vdst2, (vector unsigned char)vG, vgreen2);
            vdst2 = vec_perm(vdst2, valpha, vpermute);
            vec_st(vdst2, 16, dst);

            width -= 8;
            dst += 32;
            src += 16;
            vsrc = voverflow;
        }

        SDL_assert(width == 0);

        // do scalar until we can align...
        ONE_PIXEL_BLEND((extrawidth), extrawidth);
#undef ONE_PIXEL_BLEND

        src += srcskip; // move to next row, accounting for pitch.
        dst += dstskip;
    }
}

static void BlitNtoNKey(SDL_BlitInfo *info);
static void BlitNtoNKeyCopyAlpha(SDL_BlitInfo *info);
static void Blit32to32KeyAltivec(SDL_BlitInfo *info)
{
    int height = info->dst_h;
    Uint32 *srcp = (Uint32 *)info->src;
    int srcskip = info->src_skip / 4;
    Uint32 *dstp = (Uint32 *)info->dst;
    int dstskip = info->dst_skip / 4;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;
    int copy_alpha = (srcfmt->Amask && dstfmt->Amask);
    unsigned alpha = dstfmt->Amask ? info->a : 0;
    Uint32 rgbmask = srcfmt->Rmask | srcfmt->Gmask | srcfmt->Bmask;
    Uint32 ckey = info->colorkey;
    vector unsigned int valpha;
    vector unsigned char vpermute;
    vector unsigned char vzero;
    vector unsigned int vckey;
    vector unsigned int vrgbmask;
    vpermute = calc_swizzle32(srcfmt, dstfmt);
    if (info->dst_w < 16) {
        if (copy_alpha) {
            BlitNtoNKeyCopyAlpha(info);
        } else {
            BlitNtoNKey(info);
        }
        return;
    }
    vzero = vec_splat_u8(0);
    if (alpha) {
        ((unsigned char *)&valpha)[0] = (unsigned char)alpha;
        valpha =
            (vector unsigned int)vec_splat((vector unsigned char)valpha, 0);
    } else {
        valpha = (vector unsigned int)vzero;
    }
    ckey &= rgbmask;
    ((unsigned int *)(char *)&vckey)[0] = ckey;
    vckey = vec_splat(vckey, 0);
    ((unsigned int *)(char *)&vrgbmask)[0] = rgbmask;
    vrgbmask = vec_splat(vrgbmask, 0);

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    // reorder bytes for PowerPC little endian
    vpermute = reorder_ppc64le_vec(vpermute);
#endif

    while (height--) {
#define ONE_PIXEL_BLEND(condition, widthvar)                    \
    if (copy_alpha) {                                           \
        while (condition) {                                     \
            Uint32 Pixel;                                       \
            unsigned sR, sG, sB, sA;                            \
            DISEMBLE_RGBA((Uint8 *)srcp, srcbpp, srcfmt, Pixel, \
                          sR, sG, sB, sA);                      \
            if ((Pixel & rgbmask) != ckey) {                    \
                ASSEMBLE_RGBA((Uint8 *)dstp, dstbpp, dstfmt,    \
                              sR, sG, sB, sA);                  \
            }                                                   \
            dstp = (Uint32 *)(((Uint8 *)dstp) + dstbpp);        \
            srcp = (Uint32 *)(((Uint8 *)srcp) + srcbpp);        \
            widthvar--;                                         \
        }                                                       \
    } else {                                                    \
        while (condition) {                                     \
            Uint32 Pixel;                                       \
            unsigned sR, sG, sB;                                \
            RETRIEVE_RGB_PIXEL((Uint8 *)srcp, srcbpp, Pixel);   \
            if (Pixel != ckey) {                                \
                RGB_FROM_PIXEL(Pixel, srcfmt, sR, sG, sB);      \
                ASSEMBLE_RGBA((Uint8 *)dstp, dstbpp, dstfmt,    \
                              sR, sG, sB, alpha);               \
            }                                                   \
            dstp = (Uint32 *)(((Uint8 *)dstp) + dstbpp);        \
            srcp = (Uint32 *)(((Uint8 *)srcp) + srcbpp);        \
            widthvar--;                                         \
        }                                                       \
    }
        int width = info->dst_w;
        ONE_PIXEL_BLEND((UNALIGNED_PTR(dstp)) && (width), width);
        SDL_assert(width > 0);
        if (width > 0) {
            int extrawidth = (width % 4);
            vector unsigned char valigner = VEC_ALIGNER(srcp);
            vector unsigned int vs = vec_ld(0, srcp);
            width -= extrawidth;
            SDL_assert(width >= 4);
            while (width) {
                vector unsigned char vsel;
                vector unsigned int vd;
                vector unsigned int voverflow = vec_ld(15, srcp);
                // load the source vec
                vs = vec_perm(vs, voverflow, valigner);
                // vsel is set for items that match the key
                vsel = (vector unsigned char)vec_and(vs, vrgbmask);
                vsel = (vector unsigned char)vec_cmpeq(vs, vckey);
                // permute the src vec to the dest format
                vs = vec_perm(vs, valpha, vpermute);
                // load the destination vec
                vd = vec_ld(0, dstp);
                // select the source and dest into vs
                vd = (vector unsigned int)vec_sel((vector unsigned char)vs,
                                                  (vector unsigned char)vd,
                                                  vsel);

                vec_st(vd, 0, dstp);
                srcp += 4;
                width -= 4;
                dstp += 4;
                vs = voverflow;
            }
            ONE_PIXEL_BLEND((extrawidth), extrawidth);
#undef ONE_PIXEL_BLEND
            srcp += srcskip;
            dstp += dstskip;
        }
    }
}

// Altivec code to swizzle one 32-bit surface to a different 32-bit format.
// Use this on a G5
static void ConvertAltivec32to32_noprefetch(SDL_BlitInfo *info)
{
    int height = info->dst_h;
    Uint32 *src = (Uint32 *)info->src;
    int srcskip = info->src_skip / 4;
    Uint32 *dst = (Uint32 *)info->dst;
    int dstskip = info->dst_skip / 4;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    vector unsigned int vzero = vec_splat_u32(0);
    vector unsigned char vpermute = calc_swizzle32(srcfmt, dstfmt);
    if (dstfmt->Amask && !srcfmt->Amask) {
        if (info->a) {
            vector unsigned char valpha;
            ((unsigned char *)&valpha)[0] = info->a;
            vzero = (vector unsigned int)vec_splat(valpha, 0);
        }
    }

    SDL_assert(srcfmt->bytes_per_pixel == 4);
    SDL_assert(dstfmt->bytes_per_pixel == 4);

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    // reorder bytes for PowerPC little endian
    vpermute = reorder_ppc64le_vec(vpermute);
#endif

    while (height--) {
        vector unsigned char valigner;
        vector unsigned int vbits;
        vector unsigned int voverflow;
        Uint32 bits;
        Uint8 r, g, b, a;

        int width = info->dst_w;
        int extrawidth;

        // do scalar until we can align...
        while ((UNALIGNED_PTR(dst)) && (width)) {
            bits = *(src++);
            RGBA_FROM_8888(bits, srcfmt, r, g, b, a);
            if (!srcfmt->Amask)
                a = info->a;
            *(dst++) = MAKE8888(dstfmt, r, g, b, a);
            width--;
        }

        // After all that work, here's the vector part!
        extrawidth = (width % 4);
        width -= extrawidth;
        valigner = VEC_ALIGNER(src);
        vbits = vec_ld(0, src);

        while (width) {
            voverflow = vec_ld(15, src);
            src += 4;
            width -= 4;
            vbits = vec_perm(vbits, voverflow, valigner); // src is ready.
            vbits = vec_perm(vbits, vzero, vpermute); // swizzle it.
            vec_st(vbits, 0, dst);                    // store it back out.
            dst += 4;
            vbits = voverflow;
        }

        SDL_assert(width == 0);

        // cover pixels at the end of the row that didn't fit in 16 bytes.
        while (extrawidth) {
            bits = *(src++); // max 7 pixels, don't bother with prefetch.
            RGBA_FROM_8888(bits, srcfmt, r, g, b, a);
            if (!srcfmt->Amask)
                a = info->a;
            *(dst++) = MAKE8888(dstfmt, r, g, b, a);
            extrawidth--;
        }

        src += srcskip;
        dst += dstskip;
    }
}

// Altivec code to swizzle one 32-bit surface to a different 32-bit format.
// Use this on a G4
static void ConvertAltivec32to32_prefetch(SDL_BlitInfo *info)
{
    const int scalar_dst_lead = sizeof(Uint32) * 4;
    const int vector_dst_lead = sizeof(Uint32) * 16;

    int height = info->dst_h;
    Uint32 *src = (Uint32 *)info->src;
    int srcskip = info->src_skip / 4;
    Uint32 *dst = (Uint32 *)info->dst;
    int dstskip = info->dst_skip / 4;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    vector unsigned int vzero = vec_splat_u32(0);
    vector unsigned char vpermute = calc_swizzle32(srcfmt, dstfmt);
    if (dstfmt->Amask && !srcfmt->Amask) {
        if (info->a) {
            vector unsigned char valpha;
            ((unsigned char *)&valpha)[0] = info->a;
            vzero = (vector unsigned int)vec_splat(valpha, 0);
        }
    }

    SDL_assert(srcfmt->bytes_per_pixel == 4);
    SDL_assert(dstfmt->bytes_per_pixel == 4);

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    // reorder bytes for PowerPC little endian
    vpermute = reorder_ppc64le_vec(vpermute);
#endif

    while (height--) {
        vector unsigned char valigner;
        vector unsigned int vbits;
        vector unsigned int voverflow;
        Uint32 bits;
        Uint8 r, g, b, a;

        int width = info->dst_w;
        int extrawidth;

        // do scalar until we can align...
        while ((UNALIGNED_PTR(dst)) && (width)) {
            vec_dstt(src + scalar_dst_lead, DST_CTRL(2, 32, 1024),
                     DST_CHAN_SRC);
            vec_dstst(dst + scalar_dst_lead, DST_CTRL(2, 32, 1024),
                      DST_CHAN_DEST);
            bits = *(src++);
            RGBA_FROM_8888(bits, srcfmt, r, g, b, a);
            if (!srcfmt->Amask)
                a = info->a;
            *(dst++) = MAKE8888(dstfmt, r, g, b, a);
            width--;
        }

        // After all that work, here's the vector part!
        extrawidth = (width % 4);
        width -= extrawidth;
        valigner = VEC_ALIGNER(src);
        vbits = vec_ld(0, src);

        while (width) {
            vec_dstt(src + vector_dst_lead, DST_CTRL(2, 32, 1024),
                     DST_CHAN_SRC);
            vec_dstst(dst + vector_dst_lead, DST_CTRL(2, 32, 1024),
                      DST_CHAN_DEST);
            voverflow = vec_ld(15, src);
            src += 4;
            width -= 4;
            vbits = vec_perm(vbits, voverflow, valigner); // src is ready.
            vbits = vec_perm(vbits, vzero, vpermute); // swizzle it.
            vec_st(vbits, 0, dst);                    // store it back out.
            dst += 4;
            vbits = voverflow;
        }

        SDL_assert(width == 0);

        // cover pixels at the end of the row that didn't fit in 16 bytes.
        while (extrawidth) {
            bits = *(src++); // max 7 pixels, don't bother with prefetch.
            RGBA_FROM_8888(bits, srcfmt, r, g, b, a);
            if (!srcfmt->Amask)
                a = info->a;
            *(dst++) = MAKE8888(dstfmt, r, g, b, a);
            extrawidth--;
        }

        src += srcskip;
        dst += dstskip;
    }

    vec_dss(DST_CHAN_SRC);
    vec_dss(DST_CHAN_DEST);
}

static Uint32 GetBlitFeatures(void)
{
    static Uint32 features = ~0u;
    if (features == ~0u) {
        features = (0
                    // Feature 1 is has-SSE41
                    | ((SDL_HasSSE41()) ? BLIT_FEATURE_HAS_SSE41 : 0)
                    // Feature 2 is has-AltiVec
                    | ((SDL_HasAltiVec()) ? BLIT_FEATURE_HAS_ALTIVEC : 0)
                    // Feature 4 is dont-use-prefetch
                    // !!!! FIXME: Check for G5 or later, not the cache size! Always prefetch on a G4.
                    | ((GetL3CacheSize() == 0) ? BLIT_FEATURE_ALTIVEC_DONT_USE_PREFETCH : 0));
    }
    return features;
}

#ifdef __MWERKS__
#pragma altivec_model off
#endif
#else
// Feature 1 is has-SSE41
#define GetBlitFeatures() ((SDL_HasSSE41() ? BLIT_FEATURE_HAS_SSE41 : 0))
#endif

// This is now endian dependent
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define HI 1
#define LO 0
#else // SDL_BYTEORDER == SDL_BIG_ENDIAN
#define HI 0
#define LO 1
#endif

// Special optimized blit for RGB 8-8-8 --> RGB 5-5-5
#define RGB888_RGB555(dst, src)                                    \
    {                                                              \
        *(Uint16 *)(dst) = (Uint16)((((*src) & 0x00F80000) >> 9) | \
                                    (((*src) & 0x0000F800) >> 6) | \
                                    (((*src) & 0x000000F8) >> 3)); \
    }
#ifndef USE_DUFFS_LOOP
#define RGB888_RGB555_TWO(dst, src)                            \
    {                                                          \
        *(Uint32 *)(dst) = (((((src[HI]) & 0x00F80000) >> 9) | \
                             (((src[HI]) & 0x0000F800) >> 6) | \
                             (((src[HI]) & 0x000000F8) >> 3))  \
                            << 16) |                           \
                           (((src[LO]) & 0x00F80000) >> 9) |   \
                           (((src[LO]) & 0x0000F800) >> 6) |   \
                           (((src[LO]) & 0x000000F8) >> 3);    \
    }
#endif
static void Blit_XRGB8888_RGB555(SDL_BlitInfo *info)
{
#ifndef USE_DUFFS_LOOP
    int c;
#endif
    int width, height;
    Uint32 *src;
    Uint16 *dst;
    int srcskip, dstskip;

    // Set up some basic variables
    width = info->dst_w;
    height = info->dst_h;
    src = (Uint32 *)info->src;
    srcskip = info->src_skip / 4;
    dst = (Uint16 *)info->dst;
    dstskip = info->dst_skip / 2;

#ifdef USE_DUFFS_LOOP
    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
            RGB888_RGB555(dst, src);
            ++src;
            ++dst;
        , width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
#else
    // Memory align at 4-byte boundary, if necessary
    if ((long)dst & 0x03) {
        // Don't do anything if width is 0
        if (width == 0) {
            return;
        }
        --width;

        while (height--) {
            // Perform copy alignment
            RGB888_RGB555(dst, src);
            ++src;
            ++dst;

            // Copy in 4 pixel chunks
            for (c = width / 4; c; --c) {
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
            }
            // Get any leftovers
            switch (width & 3) {
            case 3:
                RGB888_RGB555(dst, src);
                ++src;
                ++dst;
                SDL_FALLTHROUGH;
            case 2:
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
                break;
            case 1:
                RGB888_RGB555(dst, src);
                ++src;
                ++dst;
                break;
            }
            src += srcskip;
            dst += dstskip;
        }
    } else {
        while (height--) {
            // Copy in 4 pixel chunks
            for (c = width / 4; c; --c) {
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
            }
            // Get any leftovers
            switch (width & 3) {
            case 3:
                RGB888_RGB555(dst, src);
                ++src;
                ++dst;
                SDL_FALLTHROUGH;
            case 2:
                RGB888_RGB555_TWO(dst, src);
                src += 2;
                dst += 2;
                break;
            case 1:
                RGB888_RGB555(dst, src);
                ++src;
                ++dst;
                break;
            }
            src += srcskip;
            dst += dstskip;
        }
    }
#endif // USE_DUFFS_LOOP
}

// Special optimized blit for RGB 8-8-8 --> RGB 5-6-5
#define RGB888_RGB565(dst, src)                                    \
    {                                                              \
        *(Uint16 *)(dst) = (Uint16)((((*src) & 0x00F80000) >> 8) | \
                                    (((*src) & 0x0000FC00) >> 5) | \
                                    (((*src) & 0x000000F8) >> 3)); \
    }
#ifndef USE_DUFFS_LOOP
#define RGB888_RGB565_TWO(dst, src)                            \
    {                                                          \
        *(Uint32 *)(dst) = (((((src[HI]) & 0x00F80000) >> 8) | \
                             (((src[HI]) & 0x0000FC00) >> 5) | \
                             (((src[HI]) & 0x000000F8) >> 3))  \
                            << 16) |                           \
                           (((src[LO]) & 0x00F80000) >> 8) |   \
                           (((src[LO]) & 0x0000FC00) >> 5) |   \
                           (((src[LO]) & 0x000000F8) >> 3);    \
    }
#endif
static void Blit_XRGB8888_RGB565(SDL_BlitInfo *info)
{
#ifndef USE_DUFFS_LOOP
    int c;
#endif
    int width, height;
    Uint32 *src;
    Uint16 *dst;
    int srcskip, dstskip;

    // Set up some basic variables
    width = info->dst_w;
    height = info->dst_h;
    src = (Uint32 *)info->src;
    srcskip = info->src_skip / 4;
    dst = (Uint16 *)info->dst;
    dstskip = info->dst_skip / 2;

#ifdef USE_DUFFS_LOOP
    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
            RGB888_RGB565(dst, src);
            ++src;
            ++dst;
        , width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
#else
    // Memory align at 4-byte boundary, if necessary
    if ((long)dst & 0x03) {
        // Don't do anything if width is 0
        if (width == 0) {
            return;
        }
        --width;

        while (height--) {
            // Perform copy alignment
            RGB888_RGB565(dst, src);
            ++src;
            ++dst;

            // Copy in 4 pixel chunks
            for (c = width / 4; c; --c) {
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
            }
            // Get any leftovers
            switch (width & 3) {
            case 3:
                RGB888_RGB565(dst, src);
                ++src;
                ++dst;
                SDL_FALLTHROUGH;
            case 2:
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
                break;
            case 1:
                RGB888_RGB565(dst, src);
                ++src;
                ++dst;
                break;
            }
            src += srcskip;
            dst += dstskip;
        }
    } else {
        while (height--) {
            // Copy in 4 pixel chunks
            for (c = width / 4; c; --c) {
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
            }
            // Get any leftovers
            switch (width & 3) {
            case 3:
                RGB888_RGB565(dst, src);
                ++src;
                ++dst;
                SDL_FALLTHROUGH;
            case 2:
                RGB888_RGB565_TWO(dst, src);
                src += 2;
                dst += 2;
                break;
            case 1:
                RGB888_RGB565(dst, src);
                ++src;
                ++dst;
                break;
            }
            src += srcskip;
            dst += dstskip;
        }
    }
#endif // USE_DUFFS_LOOP
}

#ifdef SDL_SSE4_1_INTRINSICS

static void SDL_TARGETING("sse4.1") Blit_RGB565_32_SSE41(SDL_BlitInfo *info, int Rshift, int Gshift, int Bshift, int Amask)
{
    int c;
    int width, height;
    const Uint16 *src;
    Uint32 *dst;
    int srcskip, dstskip;
    Uint8 r, g, b;

    // Set up some basic variables
    width = info->dst_w;
    height = info->dst_h;
    src = (const Uint16 *)info->src;
    srcskip = info->src_skip / 2;
    dst = (Uint32 *)info->dst;
    dstskip = info->dst_skip / 4;

    while (height--) {
        // Copy in 4 pixel chunks
        for (c = width / 4; c; --c) {
            // Load 4 16-bit RGB565 pixels into an SSE register
            __m128i pixels_rgb565 = _mm_loadu_si128((__m128i*)src);

            // Extract Red components (5 bits)
            __m128i red_5bit = _mm_and_si128(pixels_rgb565, _mm_set1_epi16(0xF800)); // Mask for Red
            red_5bit = _mm_srli_epi16(red_5bit, 11); // Shift to get 5-bit value
            __m128i red_8bit = _mm_cvtepu16_epi32(red_5bit); // Convert to 32-bit and zero-extend
            red_8bit = _mm_slli_epi32(red_8bit, 3); // Scale to 8 bits (multiply by 8)
            red_8bit = _mm_or_si128(red_8bit, _mm_srli_epi32(red_8bit, 5)); // Replicate top 3 bits for better scaling

            // Extract Green components (6 bits)
            __m128i green_6bit = _mm_and_si128(pixels_rgb565, _mm_set1_epi16(0x07E0)); // Mask for Green
            green_6bit = _mm_srli_epi16(green_6bit, 5); // Shift to get 6-bit value
            __m128i green_8bit = _mm_cvtepu16_epi32(green_6bit); // Convert to 32-bit and zero-extend
            green_8bit = _mm_slli_epi32(green_8bit, 2); // Scale to 8 bits (multiply by 4)
            green_8bit = _mm_or_si128(green_8bit, _mm_srli_epi32(green_8bit, 6)); // Replicate top 2 bits

            // Extract Blue components (5 bits)
            __m128i blue_5bit = _mm_and_si128(pixels_rgb565, _mm_set1_epi16(0x001F)); // Mask for Blue
            __m128i blue_8bit = _mm_cvtepu16_epi32(blue_5bit); // Convert to 32-bit and zero-extend
            blue_8bit = _mm_slli_epi32(blue_8bit, 3); // Scale to 8 bits (multiply by 8)
            blue_8bit = _mm_or_si128(blue_8bit, _mm_srli_epi32(blue_8bit, 5)); // Replicate top 3 bits

            // Set Alpha to opaque (0xFF)
            __m128i alpha_8bit = _mm_set1_epi32(Amask);

            // Combine into 32-bit values
            __m128i argb_pixels_low = _mm_or_si128(alpha_8bit, _mm_slli_epi32(red_8bit, Rshift));
            argb_pixels_low = _mm_or_si128(argb_pixels_low, _mm_slli_epi32(green_8bit, Gshift));
            argb_pixels_low = _mm_or_si128(argb_pixels_low, _mm_slli_epi32(blue_8bit, Bshift));

            // Store the results
            _mm_storeu_si128((__m128i*)dst, argb_pixels_low);
            src += 4;
            dst += 4;
        }
        // Get any leftovers
        switch (width & 3) {
        case 3:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
        case 2:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
        case 1:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            break;
        }
        src += srcskip;
        dst += dstskip;
    }
}

static void Blit_RGB565_ARGB8888_SSE41(SDL_BlitInfo * info)
{
    Blit_RGB565_32_SSE41(info, 16, 8, 0, 0xFF000000);
}

static void Blit_RGB565_ABGR8888_SSE41(SDL_BlitInfo * info)
{
    Blit_RGB565_32_SSE41(info, 0, 8, 16, 0xFF000000);
}

static void Blit_RGB565_RGBA8888_SSE41(SDL_BlitInfo * info)
{
    Blit_RGB565_32_SSE41(info, 24, 16, 8, 0x000000FF);
}

static void Blit_RGB565_BGRA8888_SSE41(SDL_BlitInfo * info)
{
    Blit_RGB565_32_SSE41(info, 8, 16, 24, 0x000000FF);
}

#endif // SDL_SSE4_1_INTRINSICS

#ifdef SDL_HAVE_BLIT_N_RGB565

// Special optimized blit for RGB 5-6-5 --> 32-bit RGB surfaces
#define RGB565_32(src) (mapR[src >> 11] | mapG[(src >> 5) & 63] | mapB[src & 31] | Amask)

static void Blit_RGB565_32(SDL_BlitInfo *info, const Uint32 *mapR, const Uint32 *mapG, const Uint32 *mapB, Uint32 Amask)
{
#ifndef USE_DUFFS_LOOP
    int c;
#endif
    int width, height;
    const Uint16 *src;
    Uint32 *dst;
    int srcskip, dstskip;

    // Set up some basic variables
    width = info->dst_w;
    height = info->dst_h;
    src = (const Uint16 *)info->src;
    srcskip = info->src_skip / 2;
    dst = (Uint32 *)info->dst;
    dstskip = info->dst_skip / 4;

#ifdef USE_DUFFS_LOOP
    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
        {
            *dst++ = RGB565_32(*src);
            ++src;
        },
        width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
#else
    while (height--) {
        // Copy in 4 pixel chunks
        for (c = width / 4; c; --c) {
            *dst++ = RGB565_32(*src);
            ++src;
            *dst++ = RGB565_32(*src);
            ++src;
            *dst++ = RGB565_32(*src);
            ++src;
            *dst++ = RGB565_32(*src);
            ++src;
        }
        // Get any leftovers
        switch (width & 3) {
        case 3:
            *dst++ = RGB565_32(*src);
            ++src;
            SDL_FALLTHROUGH;
        case 2:
            *dst++ = RGB565_32(*src);
            ++src;
            SDL_FALLTHROUGH;
        case 1:
            *dst++ = RGB565_32(*src);
            ++src;
            break;
        }
        src += srcskip;
        dst += dstskip;
    }
#endif // USE_DUFFS_LOOP
}

#if 0
// This is the code used to generate the lookup tables below:
#include <SDL3/SDL.h>
#include <stdio.h>

static Uint32 Calculate(int v, int vmax, int shift)
{
    return (Uint32)SDL_roundf(v * 255.0f / vmax) << shift;
}

static void PrintLUT(const char *src, const char *dst, const char *channel, int bits, int shift)
{
    int v, vmax = (1 << bits) - 1;

    printf("static const Uint32 %s_%s_LUT_%s[%d] = {", src, dst, channel, vmax + 1);
    for (v = 0; v <= vmax; ++v) {
        if ((v % 4) == 0) {
            printf("\n    ");
        }
        printf("0x%.8x", Calculate(v, vmax, shift));
        if (v < vmax) {
            printf(",");
            if (((v + 1) % 4) != 0) {
                printf(" ");
            }
        }
    }
    printf("\n};\n\n");
}

static void GenerateLUT(SDL_PixelFormat src, SDL_PixelFormat dst)
{
    const char *src_name = SDL_GetPixelFormatName(src) + 16;
    const char *dst_name = SDL_GetPixelFormatName(dst) + 16;
    const SDL_PixelFormatDetails *sfmt = SDL_GetPixelFormatDetails(src);
    const SDL_PixelFormatDetails *dfmt = SDL_GetPixelFormatDetails(dst);

    printf("// Special optimized blit for %s -> %s\n\n", src_name, dst_name);
    PrintLUT(src_name, dst_name, "R", sfmt->Rbits, dfmt->Rshift);
    PrintLUT(src_name, dst_name, "G", sfmt->Gbits, dfmt->Gshift);
    PrintLUT(src_name, dst_name, "B", sfmt->Bbits, dfmt->Bshift);
}

int main(int argc, char *argv[])
{
    GenerateLUT(SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_ARGB8888);
    GenerateLUT(SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_ABGR8888);
    GenerateLUT(SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_RGBA8888);
    GenerateLUT(SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_BGRA8888);
}
#endif // 0

/* *INDENT-OFF* */ // clang-format off

// Special optimized blit for RGB565 -> ARGB8888

static const Uint32 RGB565_ARGB8888_LUT_R[32] = {
    0x00000000, 0x00080000, 0x00100000, 0x00190000,
    0x00210000, 0x00290000, 0x00310000, 0x003a0000,
    0x00420000, 0x004a0000, 0x00520000, 0x005a0000,
    0x00630000, 0x006b0000, 0x00730000, 0x007b0000,
    0x00840000, 0x008c0000, 0x00940000, 0x009c0000,
    0x00a50000, 0x00ad0000, 0x00b50000, 0x00bd0000,
    0x00c50000, 0x00ce0000, 0x00d60000, 0x00de0000,
    0x00e60000, 0x00ef0000, 0x00f70000, 0x00ff0000
};

static const Uint32 RGB565_ARGB8888_LUT_G[64] = {
    0x00000000, 0x00000400, 0x00000800, 0x00000c00,
    0x00001000, 0x00001400, 0x00001800, 0x00001c00,
    0x00002000, 0x00002400, 0x00002800, 0x00002d00,
    0x00003100, 0x00003500, 0x00003900, 0x00003d00,
    0x00004100, 0x00004500, 0x00004900, 0x00004d00,
    0x00005100, 0x00005500, 0x00005900, 0x00005d00,
    0x00006100, 0x00006500, 0x00006900, 0x00006d00,
    0x00007100, 0x00007500, 0x00007900, 0x00007d00,
    0x00008200, 0x00008600, 0x00008a00, 0x00008e00,
    0x00009200, 0x00009600, 0x00009a00, 0x00009e00,
    0x0000a200, 0x0000a600, 0x0000aa00, 0x0000ae00,
    0x0000b200, 0x0000b600, 0x0000ba00, 0x0000be00,
    0x0000c200, 0x0000c600, 0x0000ca00, 0x0000ce00,
    0x0000d200, 0x0000d700, 0x0000db00, 0x0000df00,
    0x0000e300, 0x0000e700, 0x0000eb00, 0x0000ef00,
    0x0000f300, 0x0000f700, 0x0000fb00, 0x0000ff00
};

static const Uint32 RGB565_ARGB8888_LUT_B[32] = {
    0x00000000, 0x00000008, 0x00000010, 0x00000019,
    0x00000021, 0x00000029, 0x00000031, 0x0000003a,
    0x00000042, 0x0000004a, 0x00000052, 0x0000005a,
    0x00000063, 0x0000006b, 0x00000073, 0x0000007b,
    0x00000084, 0x0000008c, 0x00000094, 0x0000009c,
    0x000000a5, 0x000000ad, 0x000000b5, 0x000000bd,
    0x000000c5, 0x000000ce, 0x000000d6, 0x000000de,
    0x000000e6, 0x000000ef, 0x000000f7, 0x000000ff
};

static void Blit_RGB565_ARGB8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_ARGB8888_LUT_R, RGB565_ARGB8888_LUT_G, RGB565_ARGB8888_LUT_B, 0xFF000000);
}

// Special optimized blit for RGB565 -> ABGR8888

static const Uint32 RGB565_ABGR8888_LUT_R[32] = {
    0x00000000, 0x00000008, 0x00000010, 0x00000019,
    0x00000021, 0x00000029, 0x00000031, 0x0000003a,
    0x00000042, 0x0000004a, 0x00000052, 0x0000005a,
    0x00000063, 0x0000006b, 0x00000073, 0x0000007b,
    0x00000084, 0x0000008c, 0x00000094, 0x0000009c,
    0x000000a5, 0x000000ad, 0x000000b5, 0x000000bd,
    0x000000c5, 0x000000ce, 0x000000d6, 0x000000de,
    0x000000e6, 0x000000ef, 0x000000f7, 0x000000ff
};

static const Uint32 RGB565_ABGR8888_LUT_G[64] = {
    0x00000000, 0x00000400, 0x00000800, 0x00000c00,
    0x00001000, 0x00001400, 0x00001800, 0x00001c00,
    0x00002000, 0x00002400, 0x00002800, 0x00002d00,
    0x00003100, 0x00003500, 0x00003900, 0x00003d00,
    0x00004100, 0x00004500, 0x00004900, 0x00004d00,
    0x00005100, 0x00005500, 0x00005900, 0x00005d00,
    0x00006100, 0x00006500, 0x00006900, 0x00006d00,
    0x00007100, 0x00007500, 0x00007900, 0x00007d00,
    0x00008200, 0x00008600, 0x00008a00, 0x00008e00,
    0x00009200, 0x00009600, 0x00009a00, 0x00009e00,
    0x0000a200, 0x0000a600, 0x0000aa00, 0x0000ae00,
    0x0000b200, 0x0000b600, 0x0000ba00, 0x0000be00,
    0x0000c200, 0x0000c600, 0x0000ca00, 0x0000ce00,
    0x0000d200, 0x0000d700, 0x0000db00, 0x0000df00,
    0x0000e300, 0x0000e700, 0x0000eb00, 0x0000ef00,
    0x0000f300, 0x0000f700, 0x0000fb00, 0x0000ff00
};

static const Uint32 RGB565_ABGR8888_LUT_B[32] = {
    0x00000000, 0x00080000, 0x00100000, 0x00190000,
    0x00210000, 0x00290000, 0x00310000, 0x003a0000,
    0x00420000, 0x004a0000, 0x00520000, 0x005a0000,
    0x00630000, 0x006b0000, 0x00730000, 0x007b0000,
    0x00840000, 0x008c0000, 0x00940000, 0x009c0000,
    0x00a50000, 0x00ad0000, 0x00b50000, 0x00bd0000,
    0x00c50000, 0x00ce0000, 0x00d60000, 0x00de0000,
    0x00e60000, 0x00ef0000, 0x00f70000, 0x00ff0000
};

static void Blit_RGB565_ABGR8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_ABGR8888_LUT_R, RGB565_ABGR8888_LUT_G, RGB565_ABGR8888_LUT_B, 0xFF000000);
}

// Special optimized blit for RGB565 -> RGBA8888

static const Uint32 RGB565_RGBA8888_LUT_R[32] = {
    0x00000000, 0x08000000, 0x10000000, 0x19000000,
    0x21000000, 0x29000000, 0x31000000, 0x3a000000,
    0x42000000, 0x4a000000, 0x52000000, 0x5a000000,
    0x63000000, 0x6b000000, 0x73000000, 0x7b000000,
    0x84000000, 0x8c000000, 0x94000000, 0x9c000000,
    0xa5000000, 0xad000000, 0xb5000000, 0xbd000000,
    0xc5000000, 0xce000000, 0xd6000000, 0xde000000,
    0xe6000000, 0xef000000, 0xf7000000, 0xff000000
};

static const Uint32 RGB565_RGBA8888_LUT_G[64] = {
    0x00000000, 0x00040000, 0x00080000, 0x000c0000,
    0x00100000, 0x00140000, 0x00180000, 0x001c0000,
    0x00200000, 0x00240000, 0x00280000, 0x002d0000,
    0x00310000, 0x00350000, 0x00390000, 0x003d0000,
    0x00410000, 0x00450000, 0x00490000, 0x004d0000,
    0x00510000, 0x00550000, 0x00590000, 0x005d0000,
    0x00610000, 0x00650000, 0x00690000, 0x006d0000,
    0x00710000, 0x00750000, 0x00790000, 0x007d0000,
    0x00820000, 0x00860000, 0x008a0000, 0x008e0000,
    0x00920000, 0x00960000, 0x009a0000, 0x009e0000,
    0x00a20000, 0x00a60000, 0x00aa0000, 0x00ae0000,
    0x00b20000, 0x00b60000, 0x00ba0000, 0x00be0000,
    0x00c20000, 0x00c60000, 0x00ca0000, 0x00ce0000,
    0x00d20000, 0x00d70000, 0x00db0000, 0x00df0000,
    0x00e30000, 0x00e70000, 0x00eb0000, 0x00ef0000,
    0x00f30000, 0x00f70000, 0x00fb0000, 0x00ff0000
};

static const Uint32 RGB565_RGBA8888_LUT_B[32] = {
    0x00000000, 0x00000800, 0x00001000, 0x00001900,
    0x00002100, 0x00002900, 0x00003100, 0x00003a00,
    0x00004200, 0x00004a00, 0x00005200, 0x00005a00,
    0x00006300, 0x00006b00, 0x00007300, 0x00007b00,
    0x00008400, 0x00008c00, 0x00009400, 0x00009c00,
    0x0000a500, 0x0000ad00, 0x0000b500, 0x0000bd00,
    0x0000c500, 0x0000ce00, 0x0000d600, 0x0000de00,
    0x0000e600, 0x0000ef00, 0x0000f700, 0x0000ff00
};

static void Blit_RGB565_RGBA8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_RGBA8888_LUT_R, RGB565_RGBA8888_LUT_G, RGB565_RGBA8888_LUT_B, 0x000000FF);
}

// Special optimized blit for RGB565 -> BGRA8888

static const Uint32 RGB565_BGRA8888_LUT_R[32] = {
    0x00000000, 0x00000800, 0x00001000, 0x00001900,
    0x00002100, 0x00002900, 0x00003100, 0x00003a00,
    0x00004200, 0x00004a00, 0x00005200, 0x00005a00,
    0x00006300, 0x00006b00, 0x00007300, 0x00007b00,
    0x00008400, 0x00008c00, 0x00009400, 0x00009c00,
    0x0000a500, 0x0000ad00, 0x0000b500, 0x0000bd00,
    0x0000c500, 0x0000ce00, 0x0000d600, 0x0000de00,
    0x0000e600, 0x0000ef00, 0x0000f700, 0x0000ff00
};

static const Uint32 RGB565_BGRA8888_LUT_G[64] = {
    0x00000000, 0x00040000, 0x00080000, 0x000c0000,
    0x00100000, 0x00140000, 0x00180000, 0x001c0000,
    0x00200000, 0x00240000, 0x00280000, 0x002d0000,
    0x00310000, 0x00350000, 0x00390000, 0x003d0000,
    0x00410000, 0x00450000, 0x00490000, 0x004d0000,
    0x00510000, 0x00550000, 0x00590000, 0x005d0000,
    0x00610000, 0x00650000, 0x00690000, 0x006d0000,
    0x00710000, 0x00750000, 0x00790000, 0x007d0000,
    0x00820000, 0x00860000, 0x008a0000, 0x008e0000,
    0x00920000, 0x00960000, 0x009a0000, 0x009e0000,
    0x00a20000, 0x00a60000, 0x00aa0000, 0x00ae0000,
    0x00b20000, 0x00b60000, 0x00ba0000, 0x00be0000,
    0x00c20000, 0x00c60000, 0x00ca0000, 0x00ce0000,
    0x00d20000, 0x00d70000, 0x00db0000, 0x00df0000,
    0x00e30000, 0x00e70000, 0x00eb0000, 0x00ef0000,
    0x00f30000, 0x00f70000, 0x00fb0000, 0x00ff0000
};

static const Uint32 RGB565_BGRA8888_LUT_B[32] = {
    0x00000000, 0x08000000, 0x10000000, 0x19000000,
    0x21000000, 0x29000000, 0x31000000, 0x3a000000,
    0x42000000, 0x4a000000, 0x52000000, 0x5a000000,
    0x63000000, 0x6b000000, 0x73000000, 0x7b000000,
    0x84000000, 0x8c000000, 0x94000000, 0x9c000000,
    0xa5000000, 0xad000000, 0xb5000000, 0xbd000000,
    0xc5000000, 0xce000000, 0xd6000000, 0xde000000,
    0xe6000000, 0xef000000, 0xf7000000, 0xff000000
};

static void Blit_RGB565_BGRA8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_BGRA8888_LUT_R, RGB565_BGRA8888_LUT_G, RGB565_BGRA8888_LUT_B, 0x000000FF);
}

/* *INDENT-ON* */ // clang-format on

#endif // SDL_HAVE_BLIT_N_RGB565

// blits 16 bit RGB<->RGBA with both surfaces having the same R,G,B fields
static void Blit2to2MaskAlpha(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint16 *src = (Uint16 *)info->src;
    int srcskip = info->src_skip;
    Uint16 *dst = (Uint16 *)info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;

    if (dstfmt->Amask) {
        // RGB->RGBA, SET_ALPHA
        Uint16 mask = ((Uint32)info->a >> (8 - dstfmt->Abits)) << dstfmt->Ashift;

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP_TRIVIAL(
            {
                *dst = *src | mask;
                ++dst;
                ++src;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src = (Uint16 *)((Uint8 *)src + srcskip);
            dst = (Uint16 *)((Uint8 *)dst + dstskip);
        }
    } else {
        // RGBA->RGB, NO_ALPHA
        Uint16 mask = srcfmt->Rmask | srcfmt->Gmask | srcfmt->Bmask;

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP_TRIVIAL(
            {
                *dst = *src & mask;
                ++dst;
                ++src;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src = (Uint16 *)((Uint8 *)src + srcskip);
            dst = (Uint16 *)((Uint8 *)dst + dstskip);
        }
    }
}

// blits 32 bit RGB<->RGBA with both surfaces having the same R,G,B fields
static void Blit4to4MaskAlpha(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint32 *src = (Uint32 *)info->src;
    int srcskip = info->src_skip;
    Uint32 *dst = (Uint32 *)info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;

    if (dstfmt->Amask) {
        // RGB->RGBA, SET_ALPHA
        Uint32 mask = ((Uint32)info->a >> (8 - dstfmt->Abits)) << dstfmt->Ashift;

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP_TRIVIAL(
            {
                *dst = *src | mask;
                ++dst;
                ++src;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src = (Uint32 *)((Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    } else {
        // RGBA->RGB, NO_ALPHA
        Uint32 mask = srcfmt->Rmask | srcfmt->Gmask | srcfmt->Bmask;

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP_TRIVIAL(
            {
                *dst = *src & mask;
                ++dst;
                ++src;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src = (Uint32 *)((Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    }
}

// permutation for mapping srcfmt to dstfmt, overloading or not the alpha channel
static void get_permutation(const SDL_PixelFormatDetails *srcfmt, const SDL_PixelFormatDetails *dstfmt,
                            int *_p0, int *_p1, int *_p2, int *_p3, int *_alpha_channel)
{
    int alpha_channel = 0, p0, p1, p2, p3;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    int Pixel = 0x04030201; // identity permutation
#else
    int Pixel = 0x01020304; // identity permutation
    int srcbpp = srcfmt->bytes_per_pixel;
    int dstbpp = dstfmt->bytes_per_pixel;
#endif

    if (srcfmt->Amask) {
        RGBA_FROM_PIXEL(Pixel, srcfmt, p0, p1, p2, p3);
    } else {
        RGB_FROM_PIXEL(Pixel, srcfmt, p0, p1, p2);
        p3 = 0;
    }

    if (dstfmt->Amask) {
        if (srcfmt->Amask) {
            PIXEL_FROM_RGBA(Pixel, dstfmt, p0, p1, p2, p3);
        } else {
            PIXEL_FROM_RGBA(Pixel, dstfmt, p0, p1, p2, 0);
        }
    } else {
        PIXEL_FROM_RGB(Pixel, dstfmt, p0, p1, p2);
    }

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    p0 = Pixel & 0xFF;
    p1 = (Pixel >> 8) & 0xFF;
    p2 = (Pixel >> 16) & 0xFF;
    p3 = (Pixel >> 24) & 0xFF;
#else
    p3 = Pixel & 0xFF;
    p2 = (Pixel >> 8) & 0xFF;
    p1 = (Pixel >> 16) & 0xFF;
    p0 = (Pixel >> 24) & 0xFF;
#endif

    if (p0 == 0) {
        p0 = 1;
        alpha_channel = 0;
    } else if (p1 == 0) {
        p1 = 1;
        alpha_channel = 1;
    } else if (p2 == 0) {
        p2 = 1;
        alpha_channel = 2;
    } else if (p3 == 0) {
        p3 = 1;
        alpha_channel = 3;
    }

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#else
    if (srcbpp == 3 && dstbpp == 4) {
        if (p0 != 1) {
            p0--;
        }
        if (p1 != 1) {
            p1--;
        }
        if (p2 != 1) {
            p2--;
        }
        if (p3 != 1) {
            p3--;
        }
    } else if (srcbpp == 4 && dstbpp == 3) {
        p0 = p1;
        p1 = p2;
        p2 = p3;
    }
#endif
    *_p0 = p0 - 1;
    *_p1 = p1 - 1;
    *_p2 = p2 - 1;
    *_p3 = p3 - 1;

    if (_alpha_channel) {
        *_alpha_channel = alpha_channel;
    }
}

static void BlitNtoN(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;
    unsigned alpha = dstfmt->Amask ? info->a : 0;

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 4->4
    if (srcbpp == 4 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format) &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

        // Find the appropriate permutation
        int alpha_channel, p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, &alpha_channel);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                dst[0] = src[p0];
                dst[1] = src[p1];
                dst[2] = src[p2];
                dst[3] = src[p3];
                dst[alpha_channel] = (Uint8)alpha;
                src += 4;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    // Blit with permutation: 4->3
    if (srcbpp == 4 && dstbpp == 3 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format)) {

        // Find the appropriate permutation
        int p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, NULL);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                dst[0] = src[p0];
                dst[1] = src[p1];
                dst[2] = src[p2];
                src += 4;
                dst += 3;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 3->4
    if (srcbpp == 3 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

        // Find the appropriate permutation
        int alpha_channel, p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, &alpha_channel);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                dst[0] = src[p0];
                dst[1] = src[p1];
                dst[2] = src[p2];
                dst[3] = src[p3];
                dst[alpha_channel] = (Uint8)alpha;
                src += 3;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
        {
            Uint32 Pixel;
            unsigned sR;
            unsigned sG;
            unsigned sB;
            DISEMBLE_RGB(src, srcbpp, srcfmt, Pixel, sR, sG, sB);
            ASSEMBLE_RGBA(dst, dstbpp, dstfmt, sR, sG, sB, alpha);
            dst += dstbpp;
            src += srcbpp;
        },
        width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
}

static void BlitNtoNCopyAlpha(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;
    int c;

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 4->4
    if (srcbpp == 4 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format) &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

        // Find the appropriate permutation
        int p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, NULL);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                dst[0] = src[p0];
                dst[1] = src[p1];
                dst[2] = src[p2];
                dst[3] = src[p3];
                src += 4;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    while (height--) {
        for (c = width; c; --c) {
            Uint32 Pixel;
            unsigned sR, sG, sB, sA;
            DISEMBLE_RGBA(src, srcbpp, srcfmt, Pixel, sR, sG, sB, sA);
            ASSEMBLE_RGBA(dst, dstbpp, dstfmt, sR, sG, sB, sA);
            dst += dstbpp;
            src += srcbpp;
        }
        src += srcskip;
        dst += dstskip;
    }
}

static void Blit2to2Key(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint16 *srcp = (Uint16 *)info->src;
    int srcskip = info->src_skip;
    Uint16 *dstp = (Uint16 *)info->dst;
    int dstskip = info->dst_skip;
    Uint32 ckey = info->colorkey;
    Uint32 rgbmask = ~info->src_fmt->Amask;

    // Set up some basic variables
    srcskip /= 2;
    dstskip /= 2;
    ckey &= rgbmask;

    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP_TRIVIAL(
        {
            if ( (*srcp & rgbmask) != ckey ) {
                *dstp = *srcp;
            }
            dstp++;
            srcp++;
        },
        width);
        /* *INDENT-ON* */ // clang-format on
        srcp += srcskip;
        dstp += dstskip;
    }
}

static void BlitNtoNKey(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    Uint32 ckey = info->colorkey;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    int dstbpp = dstfmt->bytes_per_pixel;
    unsigned alpha = dstfmt->Amask ? info->a : 0;
    Uint32 rgbmask = ~srcfmt->Amask;
    int sfmt = srcfmt->format;
    int dfmt = dstfmt->format;

    // Set up some basic variables
    ckey &= rgbmask;

    // BPP 4, same rgb
    if (srcbpp == 4 && dstbpp == 4 && srcfmt->Rmask == dstfmt->Rmask && srcfmt->Gmask == dstfmt->Gmask && srcfmt->Bmask == dstfmt->Bmask) {
        Uint32 *src32 = (Uint32 *)src;
        Uint32 *dst32 = (Uint32 *)dst;

        if (dstfmt->Amask) {
            // RGB->RGBA, SET_ALPHA
            Uint32 mask = ((Uint32)info->a) << dstfmt->Ashift;
            while (height--) {
                /* *INDENT-OFF* */ // clang-format off
                DUFFS_LOOP_TRIVIAL(
                {
                    if ((*src32 & rgbmask) != ckey) {
                        *dst32 = *src32 | mask;
                    }
                    ++dst32;
                    ++src32;
                }, width);
                /* *INDENT-ON* */ // clang-format on
                src32 = (Uint32 *)((Uint8 *)src32 + srcskip);
                dst32 = (Uint32 *)((Uint8 *)dst32 + dstskip);
            }
            return;
        } else {
            // RGBA->RGB, NO_ALPHA
            Uint32 mask = srcfmt->Rmask | srcfmt->Gmask | srcfmt->Bmask;
            while (height--) {
                /* *INDENT-OFF* */ // clang-format off
                DUFFS_LOOP_TRIVIAL(
                {
                    if ((*src32 & rgbmask) != ckey) {
                        *dst32 = *src32 & mask;
                    }
                    ++dst32;
                    ++src32;
                }, width);
                /* *INDENT-ON* */ // clang-format on
                src32 = (Uint32 *)((Uint8 *)src32 + srcskip);
                dst32 = (Uint32 *)((Uint8 *)dst32 + dstskip);
            }
            return;
        }
    }

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 4->4
    if (srcbpp == 4 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format) &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

        // Find the appropriate permutation
        int alpha_channel, p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, &alpha_channel);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint32 *src32 = (Uint32 *)src;

                if ((*src32 & rgbmask) != ckey) {
                    dst[0] = src[p0];
                    dst[1] = src[p1];
                    dst[2] = src[p2];
                    dst[3] = src[p3];
                    dst[alpha_channel] = (Uint8)alpha;
                }
                src += 4;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    // BPP 3, same rgb triplet
    if ((sfmt == SDL_PIXELFORMAT_RGB24 && dfmt == SDL_PIXELFORMAT_RGB24) ||
        (sfmt == SDL_PIXELFORMAT_BGR24 && dfmt == SDL_PIXELFORMAT_BGR24)) {

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        Uint8 k0 = ckey & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = (ckey >> 16) & 0xFF;
#else
        Uint8 k0 = (ckey >> 16) & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = ckey & 0xFF;
#endif

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint8 s0 = src[0];
                Uint8 s1 = src[1];
                Uint8 s2 = src[2];

                if (k0 != s0 || k1 != s1 || k2 != s2) {
                    dst[0] = s0;
                    dst[1] = s1;
                    dst[2] = s2;
                }
                src += 3;
                dst += 3;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }

    // BPP 3, inversed rgb triplet
    if ((sfmt == SDL_PIXELFORMAT_RGB24 && dfmt == SDL_PIXELFORMAT_BGR24) ||
        (sfmt == SDL_PIXELFORMAT_BGR24 && dfmt == SDL_PIXELFORMAT_RGB24)) {

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        Uint8 k0 = ckey & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = (ckey >> 16) & 0xFF;
#else
        Uint8 k0 = (ckey >> 16) & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = ckey & 0xFF;
#endif

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint8 s0 = src[0];
                Uint8 s1 = src[1];
                Uint8 s2 = src[2];
                if (k0 != s0 || k1 != s1 || k2 != s2) {
                    // Inversed RGB
                    dst[0] = s2;
                    dst[1] = s1;
                    dst[2] = s0;
                }
                src += 3;
                dst += 3;
            },
            width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }

    // Blit with permutation: 4->3
    if (srcbpp == 4 && dstbpp == 3 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format)) {

        // Find the appropriate permutation
        int p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, NULL);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint32 *src32 = (Uint32 *)src;
                if ((*src32 & rgbmask) != ckey) {
                    dst[0] = src[p0];
                    dst[1] = src[p1];
                    dst[2] = src[p2];
                }
                src += 4;
                dst += 3;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 3->4
    if (srcbpp == 3 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        Uint8 k0 = ckey & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = (ckey >> 16) & 0xFF;
#else
        Uint8 k0 = (ckey >> 16) & 0xFF;
        Uint8 k1 = (ckey >> 8) & 0xFF;
        Uint8 k2 = ckey & 0xFF;
#endif

        // Find the appropriate permutation
        int alpha_channel, p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, &alpha_channel);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint8 s0 = src[0];
                Uint8 s1 = src[1];
                Uint8 s2 = src[2];

                if (k0 != s0 || k1 != s1 || k2 != s2) {
                    dst[0] = src[p0];
                    dst[1] = src[p1];
                    dst[2] = src[p2];
                    dst[3] = src[p3];
                    dst[alpha_channel] = (Uint8)alpha;
                }
                src += 3;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
        {
            Uint32 Pixel;
            unsigned sR;
            unsigned sG;
            unsigned sB;
            RETRIEVE_RGB_PIXEL(src, srcbpp, Pixel);
            if ( (Pixel & rgbmask) != ckey ) {
                RGB_FROM_PIXEL(Pixel, srcfmt, sR, sG, sB);
                ASSEMBLE_RGBA(dst, dstbpp, dstfmt, sR, sG, sB, alpha);
            }
            dst += dstbpp;
            src += srcbpp;
        },
        width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
}

static void BlitNtoNKeyCopyAlpha(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    Uint32 ckey = info->colorkey;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    Uint32 rgbmask = ~srcfmt->Amask;

    Uint8 srcbpp;
    Uint8 dstbpp;
    Uint32 Pixel;
    unsigned sR, sG, sB, sA;

    // Set up some basic variables
    srcbpp = srcfmt->bytes_per_pixel;
    dstbpp = dstfmt->bytes_per_pixel;
    ckey &= rgbmask;

    // Fastpath: same source/destination format, with Amask, bpp 32, loop is vectorized. ~10x faster
    if (srcfmt->format == dstfmt->format) {

        if (srcfmt->format == SDL_PIXELFORMAT_ARGB8888 ||
            srcfmt->format == SDL_PIXELFORMAT_ABGR8888 ||
            srcfmt->format == SDL_PIXELFORMAT_BGRA8888 ||
            srcfmt->format == SDL_PIXELFORMAT_RGBA8888) {

            Uint32 *src32 = (Uint32 *)src;
            Uint32 *dst32 = (Uint32 *)dst;
            while (height--) {
                /* *INDENT-OFF* */ // clang-format off
                DUFFS_LOOP_TRIVIAL(
                {
                    if ((*src32 & rgbmask) != ckey) {
                        *dst32 = *src32;
                    }
                    ++src32;
                    ++dst32;
                },
                width);
                /* *INDENT-ON* */ // clang-format on
                src32 = (Uint32 *)((Uint8 *)src32 + srcskip);
                dst32 = (Uint32 *)((Uint8 *)dst32 + dstskip);
            }
        }
        return;
    }

#if HAVE_FAST_WRITE_INT8
    // Blit with permutation: 4->4
    if (srcbpp == 4 && dstbpp == 4 &&
        !SDL_ISPIXELFORMAT_10BIT(srcfmt->format) &&
        !SDL_ISPIXELFORMAT_10BIT(dstfmt->format)) {

        // Find the appropriate permutation
        int p0, p1, p2, p3;
        get_permutation(srcfmt, dstfmt, &p0, &p1, &p2, &p3, NULL);

        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint32 *src32 = (Uint32 *)src;
                if ((*src32 & rgbmask) != ckey) {
                    dst[0] = src[p0];
                    dst[1] = src[p1];
                    dst[2] = src[p2];
                    dst[3] = src[p3];
                }
                src += 4;
                dst += 4;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
        return;
    }
#endif

    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
        {
            DISEMBLE_RGBA(src, srcbpp, srcfmt, Pixel, sR, sG, sB, sA);
            if ( (Pixel & rgbmask) != ckey ) {
                  ASSEMBLE_RGBA(dst, dstbpp, dstfmt, sR, sG, sB, sA);
            }
            dst += dstbpp;
            src += srcbpp;
        },
        width);
        /* *INDENT-ON* */ // clang-format on
        src += srcskip;
        dst += dstskip;
    }
}

// Convert between two 8888 pixels with differing formats.
#define SWIZZLE_8888_SRC_ALPHA(src, dst, srcfmt, dstfmt)                \
    do {                                                                \
        dst = (((src >> srcfmt->Rshift) & 0xFF) << dstfmt->Rshift) |    \
              (((src >> srcfmt->Gshift) & 0xFF) << dstfmt->Gshift) |    \
              (((src >> srcfmt->Bshift) & 0xFF) << dstfmt->Bshift) |    \
              (((src >> srcfmt->Ashift) & 0xFF) << dstfmt->Ashift);     \
    } while (0)

#define SWIZZLE_8888_DST_ALPHA(src, dst, srcfmt, dstfmt, dstAmask)      \
    do {                                                                \
        dst = (((src >> srcfmt->Rshift) & 0xFF) << dstfmt->Rshift) |    \
              (((src >> srcfmt->Gshift) & 0xFF) << dstfmt->Gshift) |    \
              (((src >> srcfmt->Bshift) & 0xFF) << dstfmt->Bshift) |    \
              dstAmask;                                                 \
    } while (0)

#ifdef SDL_SSE4_1_INTRINSICS

static void SDL_TARGETING("sse4.1") Blit8888to8888PixelSwizzleSSE41(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    bool fill_alpha = (!srcfmt->Amask || !dstfmt->Amask);
    Uint32 srcAmask, srcAshift;
    Uint32 dstAmask, dstAshift;

    SDL_Get8888AlphaMaskAndShift(srcfmt, &srcAmask, &srcAshift);
    SDL_Get8888AlphaMaskAndShift(dstfmt, &dstAmask, &dstAshift);

    // The byte offsets for the start of each pixel
    const __m128i mask_offsets = _mm_set_epi8(
        12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m128i convert_mask = _mm_add_epi32(
        _mm_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift) |
            ((srcAshift >> 3) << dstAshift)),
        mask_offsets);

    const __m128i alpha_fill_mask = _mm_set1_epi32((int)dstAmask);

    while (height--) {
        int i = 0;

        for (; i + 4 <= width; i += 4) {
            // Load 4 src pixels
            __m128i src128 = _mm_loadu_si128((__m128i *)src);

            // Convert to dst format
            src128 = _mm_shuffle_epi8(src128, convert_mask);

            if (fill_alpha) {
                // Set the alpha channels of src to 255
                src128 = _mm_or_si128(src128, alpha_fill_mask);
            }

            // Save the result
            _mm_storeu_si128((__m128i *)dst, src128);

            src += 16;
            dst += 16;
        }

        for (; i < width; ++i) {
            Uint32 src32 = *(Uint32 *)src;
            Uint32 dst32;
            if (fill_alpha) {
                SWIZZLE_8888_DST_ALPHA(src32, dst32, srcfmt, dstfmt, dstAmask);
            } else {
                SWIZZLE_8888_SRC_ALPHA(src32, dst32, srcfmt, dstfmt);
            }
            *(Uint32 *)dst = dst32;
            src += 4;
            dst += 4;
        }

        src += srcskip;
        dst += dstskip;
    }
}

#endif

#ifdef SDL_AVX2_INTRINSICS

static void SDL_TARGETING("avx2") Blit8888to8888PixelSwizzleAVX2(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    bool fill_alpha = (!srcfmt->Amask || !dstfmt->Amask);
    Uint32 srcAmask, srcAshift;
    Uint32 dstAmask, dstAshift;

    SDL_Get8888AlphaMaskAndShift(srcfmt, &srcAmask, &srcAshift);
    SDL_Get8888AlphaMaskAndShift(dstfmt, &dstAmask, &dstAshift);

    // The byte offsets for the start of each pixel
    const __m256i mask_offsets = _mm256_set_epi8(
        28, 28, 28, 28, 24, 24, 24, 24, 20, 20, 20, 20, 16, 16, 16, 16, 12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);

    const __m256i convert_mask = _mm256_add_epi32(
        _mm256_set1_epi32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift) |
            ((srcAshift >> 3) << dstAshift)),
        mask_offsets);

    const __m256i alpha_fill_mask = _mm256_set1_epi32((int)dstAmask);

    while (height--) {
        int i = 0;

        for (; i + 8 <= width; i += 8) {
            // Load 8 src pixels
            __m256i src256 = _mm256_loadu_si256((__m256i *)src);

            // Convert to dst format
            src256 = _mm256_shuffle_epi8(src256, convert_mask);

            if (fill_alpha) {
                // Set the alpha channels of src to 255
                src256 = _mm256_or_si256(src256, alpha_fill_mask);
            }

            // Save the result
            _mm256_storeu_si256((__m256i *)dst, src256);

            src += 32;
            dst += 32;
        }

        for (; i < width; ++i) {
            Uint32 src32 = *(Uint32 *)src;
            Uint32 dst32;
            if (fill_alpha) {
                SWIZZLE_8888_DST_ALPHA(src32, dst32, srcfmt, dstfmt, dstAmask);
            } else {
                SWIZZLE_8888_SRC_ALPHA(src32, dst32, srcfmt, dstfmt);
            }
            *(Uint32 *)dst = dst32;
            src += 4;
            dst += 4;
        }

        src += srcskip;
        dst += dstskip;
    }
}

#endif

#if defined(SDL_NEON_INTRINSICS) && (__ARM_ARCH >= 8)

static void Blit8888to8888PixelSwizzleNEON(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    bool fill_alpha = (!srcfmt->Amask || !dstfmt->Amask);
    Uint32 srcAmask, srcAshift;
    Uint32 dstAmask, dstAshift;

    SDL_Get8888AlphaMaskAndShift(srcfmt, &srcAmask, &srcAshift);
    SDL_Get8888AlphaMaskAndShift(dstfmt, &dstAmask, &dstAshift);

    // The byte offsets for the start of each pixel
    const uint8x16_t mask_offsets = vreinterpretq_u8_u64(vcombine_u64(
        vcreate_u64(0x0404040400000000), vcreate_u64(0x0c0c0c0c08080808)));

    const uint8x16_t convert_mask = vreinterpretq_u8_u32(vaddq_u32(
        vreinterpretq_u32_u8(mask_offsets),
        vdupq_n_u32(
            ((srcfmt->Rshift >> 3) << dstfmt->Rshift) |
            ((srcfmt->Gshift >> 3) << dstfmt->Gshift) |
            ((srcfmt->Bshift >> 3) << dstfmt->Bshift) |
            ((srcAshift >> 3) << dstAshift))));

    const uint8x16_t alpha_fill_mask = vreinterpretq_u8_u32(vdupq_n_u32(dstAmask));

    while (height--) {
        int i = 0;

        for (; i + 4 <= width; i += 4) {
            // Load 4 src pixels
            uint8x16_t src128 = vld1q_u8(src);

            // Convert to dst format
            src128 = vqtbl1q_u8(src128, convert_mask);

            if (fill_alpha) {
                // Set the alpha channels of src to 255
                src128 = vorrq_u8(src128, alpha_fill_mask);
            }

            // Save the result
            vst1q_u8(dst, src128);

            src += 16;
            dst += 16;
        }

        // Process 1 pixel per iteration, max 3 iterations, same calculations as above
        for (; i < width; ++i) {
            // Top 32-bits will be not used in src32
            uint8x8_t src32 = vreinterpret_u8_u32(vld1_dup_u32((Uint32 *)src));

            // Convert to dst format
            src32 = vtbl1_u8(src32, vget_low_u8(convert_mask));

            if (fill_alpha) {
                // Set the alpha channels of src to 255
                src32 = vorr_u8(src32, vget_low_u8(alpha_fill_mask));
            }

            // Save the result, only low 32-bits
            vst1_lane_u32((Uint32 *)dst, vreinterpret_u32_u8(src32), 0);

            src += 4;
            dst += 4;
        }

        src += srcskip;
        dst += dstskip;
    }
}

#endif

// Blit_3or4_to_3or4__same_rgb: 3 or 4 bpp, same RGB triplet
static void Blit_3or4_to_3or4__same_rgb(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;

    if (dstfmt->Amask) {
        // SET_ALPHA
        Uint32 mask = ((Uint32)info->a) << dstfmt->Ashift;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        int i0 = 0, i1 = 1, i2 = 2;
#else
        int i0 = srcbpp - 1 - 0;
        int i1 = srcbpp - 1 - 1;
        int i2 = srcbpp - 1 - 2;
#endif
        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint32 *dst32 = (Uint32 *)dst;
                Uint8 s0 = src[i0];
                Uint8 s1 = src[i1];
                Uint8 s2 = src[i2];
                *dst32 = (s0) | (s1 << 8) | (s2 << 16) | mask;
                dst += 4;
                src += srcbpp;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
    } else {
        // NO_ALPHA
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        int i0 = 0, i1 = 1, i2 = 2;
        int j0 = 0, j1 = 1, j2 = 2;
#else
        int i0 = srcbpp - 1 - 0;
        int i1 = srcbpp - 1 - 1;
        int i2 = srcbpp - 1 - 2;
        int j0 = dstbpp - 1 - 0;
        int j1 = dstbpp - 1 - 1;
        int j2 = dstbpp - 1 - 2;
#endif
        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint8 s0 = src[i0];
                Uint8 s1 = src[i1];
                Uint8 s2 = src[i2];
                dst[j0] = s0;
                dst[j1] = s1;
                dst[j2] = s2;
                dst += dstbpp;
                src += srcbpp;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
    }
}

// Blit_3or4_to_3or4__inversed_rgb: 3 or 4 bpp, inversed RGB triplet
static void Blit_3or4_to_3or4__inversed_rgb(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    const SDL_PixelFormatDetails *srcfmt = info->src_fmt;
    int srcbpp = srcfmt->bytes_per_pixel;
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    int dstbpp = dstfmt->bytes_per_pixel;

    if (dstfmt->Amask) {
        if (srcfmt->Amask) {
            // COPY_ALPHA
            // Only to switch ABGR8888 <-> ARGB8888
            while (height--) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                int i0 = 0, i1 = 1, i2 = 2, i3 = 3;
#else
                int i0 = 3, i1 = 2, i2 = 1, i3 = 0;
#endif
                /* *INDENT-OFF* */ // clang-format off
                DUFFS_LOOP(
                {
                    Uint32 *dst32 = (Uint32 *)dst;
                    Uint8 s0 = src[i0];
                    Uint8 s1 = src[i1];
                    Uint8 s2 = src[i2];
                    Uint32 alphashift = ((Uint32)src[i3]) << dstfmt->Ashift;
                    // inversed, compared to Blit_3or4_to_3or4__same_rgb
                    *dst32 = (s0 << 16) | (s1 << 8) | (s2) | alphashift;
                    dst += 4;
                    src += 4;
                }, width);
                /* *INDENT-ON* */ // clang-format on
                src += srcskip;
                dst += dstskip;
            }
        } else {
            // SET_ALPHA
            Uint32 mask = ((Uint32)info->a) << dstfmt->Ashift;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            int i0 = 0, i1 = 1, i2 = 2;
#else
            int i0 = srcbpp - 1 - 0;
            int i1 = srcbpp - 1 - 1;
            int i2 = srcbpp - 1 - 2;
#endif
            while (height--) {
                /* *INDENT-OFF* */ // clang-format off
                DUFFS_LOOP(
                {
                    Uint32 *dst32 = (Uint32 *)dst;
                    Uint8 s0 = src[i0];
                    Uint8 s1 = src[i1];
                    Uint8 s2 = src[i2];
                    // inversed, compared to Blit_3or4_to_3or4__same_rgb
                    *dst32 = (s0 << 16) | (s1 << 8) | (s2) | mask;
                    dst += 4;
                    src += srcbpp;
                }, width);
                /* *INDENT-ON* */ // clang-format on
                src += srcskip;
                dst += dstskip;
            }
        }
    } else {
        // NO_ALPHA
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
        int i0 = 0, i1 = 1, i2 = 2;
        int j0 = 2, j1 = 1, j2 = 0;
#else
        int i0 = srcbpp - 1 - 0;
        int i1 = srcbpp - 1 - 1;
        int i2 = srcbpp - 1 - 2;
        int j0 = dstbpp - 1 - 2;
        int j1 = dstbpp - 1 - 1;
        int j2 = dstbpp - 1 - 0;
#endif
        while (height--) {
            /* *INDENT-OFF* */ // clang-format off
            DUFFS_LOOP(
            {
                Uint8 s0 = src[i0];
                Uint8 s1 = src[i1];
                Uint8 s2 = src[i2];
                // inversed, compared to Blit_3or4_to_3or4__same_rgb
                dst[j0] = s0;
                dst[j1] = s1;
                dst[j2] = s2;
                dst += dstbpp;
                src += srcbpp;
            }, width);
            /* *INDENT-ON* */ // clang-format on
            src += srcskip;
            dst += dstskip;
        }
    }
}

// Normal N to N optimized blitters
#define NO_ALPHA   1
#define SET_ALPHA  2
#define COPY_ALPHA 4
struct blit_table
{
    Uint32 srcR, srcG, srcB;
    int dstbpp;
    Uint32 dstR, dstG, dstB;
    Uint32 blit_features;
    SDL_BlitFunc blitfunc;
    Uint32 alpha; // bitwise NO_ALPHA, SET_ALPHA, COPY_ALPHA
};
static const struct blit_table normal_blit_1[] = {
    // Default for 8-bit RGB source, never optimized
    { 0, 0, 0, 0, 0, 0, 0, 0, BlitNtoN, 0 }
};

static const struct blit_table normal_blit_2[] = {
#ifdef SDL_ALTIVEC_BLITTERS
    // has-altivec
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC, Blit_RGB565_32Altivec, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x00007C00, 0x000003E0, 0x0000001F, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC, Blit_RGB555_32Altivec, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
#endif
#ifdef SDL_SSE4_1_INTRINSICS
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_ARGB8888_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_ABGR8888_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0xFF000000, 0x00FF0000, 0x0000FF00,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_RGBA8888_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x0000FF00, 0x00FF0000, 0xFF000000,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_BGRA8888_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
#endif
#ifdef SDL_HAVE_BLIT_N_RGB565
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_RGB565_ARGB8888, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_RGB565_ABGR8888, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0xFF000000, 0x00FF0000, 0x0000FF00,
      0, Blit_RGB565_RGBA8888, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x0000FF00, 0x00FF0000, 0xFF000000,
      0, Blit_RGB565_BGRA8888, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
#endif
    // Default for 16-bit RGB source, used if no other blitter matches
    { 0, 0, 0, 0, 0, 0, 0, 0, BlitNtoN, 0 }
};

static const struct blit_table normal_blit_3[] = {
    // 3->4 with same rgb triplet
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__same_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__same_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA },
    // 3->4 with inversed rgb triplet
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__inversed_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__inversed_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA },
    // 3->3 to switch RGB 24 <-> BGR 24
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 3, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__inversed_rgb, NO_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 3, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__inversed_rgb, NO_ALPHA },
    // Default for 24-bit RGB source, never optimized
    { 0, 0, 0, 0, 0, 0, 0, 0, BlitNtoN, 0 }
};

static const struct blit_table normal_blit_4[] = {
#ifdef SDL_ALTIVEC_BLITTERS
    // has-altivec | dont-use-prefetch
    { 0x00000000, 0x00000000, 0x00000000, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC | BLIT_FEATURE_ALTIVEC_DONT_USE_PREFETCH, ConvertAltivec32to32_noprefetch, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    // has-altivec
    { 0x00000000, 0x00000000, 0x00000000, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC, ConvertAltivec32to32_prefetch, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    // has-altivec
    { 0x00000000, 0x00000000, 0x00000000, 2, 0x0000F800, 0x000007E0, 0x0000001F,
      BLIT_FEATURE_HAS_ALTIVEC, Blit_XRGB8888_RGB565Altivec, NO_ALPHA },
#endif
    // 4->3 with same rgb triplet
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 3, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__same_rgb, NO_ALPHA | SET_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 3, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__same_rgb, NO_ALPHA | SET_ALPHA },
    // 4->3 with inversed rgb triplet
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 3, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__inversed_rgb, NO_ALPHA | SET_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 3, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__inversed_rgb, NO_ALPHA | SET_ALPHA },
    // 4->4 with inversed rgb triplet, and COPY_ALPHA to switch ABGR8888 <-> ARGB8888
    { 0x000000FF, 0x0000FF00, 0x00FF0000, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      0, Blit_3or4_to_3or4__inversed_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA | COPY_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      0, Blit_3or4_to_3or4__inversed_rgb,
#if HAVE_FAST_WRITE_INT8
      NO_ALPHA |
#endif
          SET_ALPHA | COPY_ALPHA },
    // RGB 888 and RGB 565
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 2, 0x0000F800, 0x000007E0, 0x0000001F,
      0, Blit_XRGB8888_RGB565, NO_ALPHA },
    { 0x00FF0000, 0x0000FF00, 0x000000FF, 2, 0x00007C00, 0x000003E0, 0x0000001F,
      0, Blit_XRGB8888_RGB555, NO_ALPHA },
    // Default for 32-bit RGB source, used if no other blitter matches
    { 0, 0, 0, 0, 0, 0, 0, 0, BlitNtoN, 0 }
};

static const struct blit_table *const normal_blit[] = {
    normal_blit_1, normal_blit_2, normal_blit_3, normal_blit_4
};

// Mask matches table, or table entry is zero
#define MASKOK(x, y) (((x) == (y)) || ((y) == 0x00000000))

SDL_BlitFunc SDL_CalculateBlitN(SDL_Surface *surface)
{
    const SDL_PixelFormatDetails *srcfmt;
    const SDL_PixelFormatDetails *dstfmt;
    const struct blit_table *table;
    int which;
    SDL_BlitFunc blitfun;

    // Set up data for choosing the blit
    srcfmt = surface->fmt;
    dstfmt = surface->map.info.dst_fmt;

    // We don't support destinations less than 8-bits
    if (dstfmt->bits_per_pixel < 8) {
        return NULL;
    }

    switch (surface->map.info.flags & ~SDL_COPY_RLE_MASK) {
    case 0:
        if (SDL_PIXELLAYOUT(srcfmt->format) == SDL_PACKEDLAYOUT_8888 &&
            SDL_PIXELLAYOUT(dstfmt->format) == SDL_PACKEDLAYOUT_8888) {
#ifdef SDL_AVX2_INTRINSICS
            if (SDL_HasAVX2()) {
                return Blit8888to8888PixelSwizzleAVX2;
            }
#endif
#ifdef SDL_SSE4_1_INTRINSICS
            if (SDL_HasSSE41()) {
                return Blit8888to8888PixelSwizzleSSE41;
            }
#endif
#if defined(SDL_NEON_INTRINSICS) && (__ARM_ARCH >= 8)
            return Blit8888to8888PixelSwizzleNEON;
#endif
        }

        blitfun = NULL;
        if (dstfmt->bits_per_pixel > 8) {
            Uint32 a_need = NO_ALPHA;
            if (dstfmt->Amask) {
                a_need = srcfmt->Amask ? COPY_ALPHA : SET_ALPHA;
            }
            if (srcfmt->bytes_per_pixel > 0 &&
                srcfmt->bytes_per_pixel <= SDL_arraysize(normal_blit)) {
                table = normal_blit[srcfmt->bytes_per_pixel - 1];
                for (which = 0; table[which].dstbpp; ++which) {
                    if (MASKOK(srcfmt->Rmask, table[which].srcR) &&
                        MASKOK(srcfmt->Gmask, table[which].srcG) &&
                        MASKOK(srcfmt->Bmask, table[which].srcB) &&
                        MASKOK(dstfmt->Rmask, table[which].dstR) &&
                        MASKOK(dstfmt->Gmask, table[which].dstG) &&
                        MASKOK(dstfmt->Bmask, table[which].dstB) &&
                        dstfmt->bytes_per_pixel == table[which].dstbpp &&
                        (a_need & table[which].alpha) == a_need &&
                        ((table[which].blit_features & GetBlitFeatures()) ==
                         table[which].blit_features)) {
                        break;
                    }
                }
                blitfun = table[which].blitfunc;
            }

            if (blitfun == BlitNtoN) { // default C fallback catch-all. Slow!
                if (srcfmt->bytes_per_pixel == dstfmt->bytes_per_pixel &&
                    srcfmt->Rmask == dstfmt->Rmask &&
                    srcfmt->Gmask == dstfmt->Gmask &&
                    srcfmt->Bmask == dstfmt->Bmask) {
                    if (a_need == COPY_ALPHA) {
                        if (srcfmt->Amask == dstfmt->Amask) {
                            // Fastpath C fallback: RGBA<->RGBA blit with matching RGBA
                            blitfun = SDL_BlitCopy;
                        } else {
                            blitfun = BlitNtoNCopyAlpha;
                        }
                    } else {
                        if (srcfmt->bytes_per_pixel == 4) {
                            // Fastpath C fallback: 32bit RGB<->RGBA blit with matching RGB
                            blitfun = Blit4to4MaskAlpha;
                        } else if (srcfmt->bytes_per_pixel == 2) {
                            // Fastpath C fallback: 16bit RGB<->RGBA blit with matching RGB
                            blitfun = Blit2to2MaskAlpha;
                        }
                    }
                } else if (a_need == COPY_ALPHA) {
                    blitfun = BlitNtoNCopyAlpha;
                }
            }
        }
        return blitfun;

    case SDL_COPY_COLORKEY:
        /* colorkey blit: Here we don't have too many options, mostly
           because RLE is the preferred fast way to deal with this.
           If a particular case turns out to be useful we'll add it. */

        if (srcfmt->bytes_per_pixel == 2 && surface->map.identity != 0) {
            return Blit2to2Key;
        } else {
#ifdef SDL_ALTIVEC_BLITTERS
            if ((srcfmt->bytes_per_pixel == 4) && (dstfmt->bytes_per_pixel == 4) && SDL_HasAltiVec()) {
                return Blit32to32KeyAltivec;
            } else
#endif
            if (srcfmt->Amask && dstfmt->Amask) {
                return BlitNtoNKeyCopyAlpha;
            } else {
                return BlitNtoNKey;
            }
        }
    }

    return NULL;
}

#endif // SDL_HAVE_BLIT_N
