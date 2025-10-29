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

#ifdef BROKEN_ALTIVEC_BLITTERS	// This doesn't properly expand to the lower destination bits
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
        RGB_FROM_RGB565(Pixel, sR, sG, sB);               \
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
        RGB_FROM_RGB555(Pixel, sR, sG, sB);               \
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
#endif // BROKEN_ALTIVEC_BLITTERS

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

// !!!! FIXME: Check for G5 or later, not the cache size! Always prefetch on a G4.
#define GetBlitFeatures()   \
            ((SDL_HasAltiVec() ? BLIT_FEATURE_HAS_ALTIVEC : 0) | \
             ((GetL3CacheSize() == 0) ? BLIT_FEATURE_ALTIVEC_DONT_USE_PREFETCH : 0))

#ifdef __MWERKS__
#pragma altivec_model off
#endif
#else
#define GetBlitFeatures()   \
             (SDL_HasSSE41() ? BLIT_FEATURE_HAS_SSE41 : 0)
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

static void SDL_TARGETING("sse4.1") Blit_RGB565_32_SSE41(SDL_BlitInfo *info)
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

    // Red and blue channel multiplier to repeat 5 bits
    __m128i rb_mult = _mm_shuffle_epi32(_mm_cvtsi32_si128(0x01080108), 0);

    // Green channel multiplier to shift by 5 and then repeat 6 bits
    __m128i g_mult = _mm_shuffle_epi32(_mm_cvtsi32_si128(0x20802080), 0);

    // Red channel mask
    __m128i r_mask = _mm_shuffle_epi32(_mm_cvtsi32_si128(0xf800f800), 0);

    // Green channel mask
    __m128i g_mask = _mm_shuffle_epi32(_mm_cvtsi32_si128(0x07e007e0), 0);

    // Alpha channel mask
    __m128i a_mask = _mm_shuffle_epi32(_mm_cvtsi32_si128(0xff00ff00), 0);

    // Get the masks for converting from ARGB
    const SDL_PixelFormatDetails *dstfmt = info->dst_fmt;
    const Uint32 Rshift = dstfmt->Rshift;
    const Uint32 Gshift = dstfmt->Gshift;
    const Uint32 Bshift = dstfmt->Bshift;
    Uint32 Amask, Ashift;

    SDL_Get8888AlphaMaskAndShift(dstfmt, &Amask, &Ashift);

    // The byte offsets for the start of each pixel
    const __m128i mask_offsets = _mm_set_epi8(12, 12, 12, 12, 8, 8, 8, 8, 4, 4, 4, 4, 0, 0, 0, 0);
    const __m128i convert_mask = _mm_add_epi32(
            _mm_set1_epi32(
                ((16 >> 3) << Rshift) |
                (( 8 >> 3) << Gshift) |
                (( 0 >> 3) << Bshift) |
                ((24 >> 3) << Ashift)),
            mask_offsets);

    while (height--) {
        // Copy in 8 pixel chunks
        for (c = width / 8; c; --c) {
            __m128i pixel = _mm_loadu_si128((__m128i *)src);
            __m128i red = pixel;
            __m128i green = pixel;
            __m128i blue = pixel;

            // Get red in the upper 5 bits and then multiply
            red = _mm_and_si128(red, r_mask);
            red = _mm_mulhi_epu16(red, rb_mult);

            // Get blue in the upper 5 bits and then multiply
            blue = _mm_slli_epi16(blue, 11);
            blue = _mm_mulhi_epu16(blue, rb_mult);

            // Combine the red and blue channels
            __m128i red_blue = _mm_or_si128(_mm_slli_epi16(red, 8), blue);

            // Get the green channel and then multiply into place
            green = _mm_and_si128(green, g_mask);
            green = _mm_mulhi_epu16(green, g_mult);

            // Combine the green and alpha channels
            __m128i green_alpha = _mm_or_si128(green, a_mask);

            // Unpack them into output ARGB pixels
            __m128i out1 = _mm_unpacklo_epi8(red_blue, green_alpha);
            __m128i out2 = _mm_unpackhi_epi8(red_blue, green_alpha);

            // Convert to dst format and save!
            // This is an SSSE3 instruction
            out1 = _mm_shuffle_epi8(out1, convert_mask);
            out2 = _mm_shuffle_epi8(out2, convert_mask);

            _mm_storeu_si128((__m128i*)dst, out1);
            _mm_storeu_si128((__m128i*)(dst + 4), out2);

            src += 8;
            dst += 8;
        }

        // Get any leftovers
        switch (width & 7) {
        case 7:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
        case 6:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
        case 5:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
        case 4:
            RGB_FROM_RGB565(*src, r, g, b);
            *dst++ = (r << Rshift) | (g << Gshift) | (b << Bshift) | Amask;
            ++src;
            SDL_FALLTHROUGH;
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

#endif // SDL_SSE4_1_INTRINSICS

#ifdef SDL_HAVE_BLIT_N_RGB565

// Special optimized blit for RGB 5-6-5 --> 32-bit RGB surfaces
#define RGB565_32(dst, src, map) (map[src[LO] * 2] | map[src[HI] * 2 + 1])
static void Blit_RGB565_32(SDL_BlitInfo *info, const Uint32 *map)
{
#ifndef USE_DUFFS_LOOP
    int c;
#endif
    int width, height;
    Uint8 *src;
    Uint32 *dst;
    int srcskip, dstskip;

    // Set up some basic variables
    width = info->dst_w;
    height = info->dst_h;
    src = info->src;
    srcskip = info->src_skip;
    dst = (Uint32 *)info->dst;
    dstskip = info->dst_skip / 4;

#ifdef USE_DUFFS_LOOP
    while (height--) {
        /* *INDENT-OFF* */ // clang-format off
        DUFFS_LOOP(
        {
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
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
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
        }
        // Get any leftovers
        switch (width & 3) {
        case 3:
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            SDL_FALLTHROUGH;
        case 2:
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            SDL_FALLTHROUGH;
        case 1:
            *dst++ = RGB565_32(dst, src, map);
            src += 2;
            break;
        }
        src += srcskip;
        dst += dstskip;
    }
#endif // USE_DUFFS_LOOP
}

// This is the code used to generate the lookup tables below:
#if 0
#include <SDL3/SDL.h>
#include <stdio.h>

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

static Uint32 CalculateARGB(int v, const SDL_PixelFormatDetails *sfmt, const SDL_PixelFormatDetails *dfmt)
{
    Uint8 r = (v & sfmt->Rmask) >> sfmt->Rshift;
    Uint8 g = (v & sfmt->Gmask) >> sfmt->Gshift;
    Uint8 b = (v & sfmt->Bmask) >> sfmt->Bshift;
    return dfmt->Amask |
            Calculate(r, sfmt->Rbits, (1 << sfmt->Rbits) - 1, dfmt->Rshift) |
            Calculate(g, sfmt->Gbits, (1 << sfmt->Gbits) - 1, dfmt->Gshift) |
            Calculate(b, sfmt->Bbits, (1 << sfmt->Bbits) - 1, dfmt->Bshift);
}

static void GenerateLUT(SDL_PixelFormat src, SDL_PixelFormat dst)
{
    static Uint32 lut[512];
    const char *src_name = SDL_GetPixelFormatName(src) + 16;
    const char *dst_name = SDL_GetPixelFormatName(dst) + 16;
    const SDL_PixelFormatDetails *sfmt = SDL_GetPixelFormatDetails(src);
    const SDL_PixelFormatDetails *dfmt = SDL_GetPixelFormatDetails(dst);
    int i;

    for (i = 0; i < 256; ++i) {
        lut[i * 2] = CalculateARGB(i, sfmt, dfmt);
        lut[i * 2 + 1] = CalculateARGB(i << 8, sfmt, dfmt);
    }

    printf("// Special optimized blit for %s -> %s\n\n", src_name, dst_name);
    printf("static const Uint32 %s_%s_LUT[%d] = {", src_name, dst_name, (int)SDL_arraysize(lut));
    for (i = 0; i < SDL_arraysize(lut); ++i) {
        if ((i % 8) == 0) {
            printf("\n    ");
        }
        printf("0x%.8x", lut[i]);
        if (i < (SDL_arraysize(lut) - 1)) {
            printf(",");
            if (((i + 1) % 8) != 0) {
                printf(" ");
            }
        }
    }
    printf("\n};\n\n");
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

static const Uint32 RGB565_ARGB8888_LUT[512] = {
    0xff000000, 0xff000000, 0xff000008, 0xff002000, 0xff000010, 0xff004100, 0xff000018, 0xff006100,
    0xff000021, 0xff008200, 0xff000029, 0xff00a200, 0xff000031, 0xff00c300, 0xff000039, 0xff00e300,
    0xff000042, 0xff080000, 0xff00004a, 0xff082000, 0xff000052, 0xff084100, 0xff00005a, 0xff086100,
    0xff000063, 0xff088200, 0xff00006b, 0xff08a200, 0xff000073, 0xff08c300, 0xff00007b, 0xff08e300,
    0xff000084, 0xff100000, 0xff00008c, 0xff102000, 0xff000094, 0xff104100, 0xff00009c, 0xff106100,
    0xff0000a5, 0xff108200, 0xff0000ad, 0xff10a200, 0xff0000b5, 0xff10c300, 0xff0000bd, 0xff10e300,
    0xff0000c6, 0xff180000, 0xff0000ce, 0xff182000, 0xff0000d6, 0xff184100, 0xff0000de, 0xff186100,
    0xff0000e7, 0xff188200, 0xff0000ef, 0xff18a200, 0xff0000f7, 0xff18c300, 0xff0000ff, 0xff18e300,
    0xff000400, 0xff210000, 0xff000408, 0xff212000, 0xff000410, 0xff214100, 0xff000418, 0xff216100,
    0xff000421, 0xff218200, 0xff000429, 0xff21a200, 0xff000431, 0xff21c300, 0xff000439, 0xff21e300,
    0xff000442, 0xff290000, 0xff00044a, 0xff292000, 0xff000452, 0xff294100, 0xff00045a, 0xff296100,
    0xff000463, 0xff298200, 0xff00046b, 0xff29a200, 0xff000473, 0xff29c300, 0xff00047b, 0xff29e300,
    0xff000484, 0xff310000, 0xff00048c, 0xff312000, 0xff000494, 0xff314100, 0xff00049c, 0xff316100,
    0xff0004a5, 0xff318200, 0xff0004ad, 0xff31a200, 0xff0004b5, 0xff31c300, 0xff0004bd, 0xff31e300,
    0xff0004c6, 0xff390000, 0xff0004ce, 0xff392000, 0xff0004d6, 0xff394100, 0xff0004de, 0xff396100,
    0xff0004e7, 0xff398200, 0xff0004ef, 0xff39a200, 0xff0004f7, 0xff39c300, 0xff0004ff, 0xff39e300,
    0xff000800, 0xff420000, 0xff000808, 0xff422000, 0xff000810, 0xff424100, 0xff000818, 0xff426100,
    0xff000821, 0xff428200, 0xff000829, 0xff42a200, 0xff000831, 0xff42c300, 0xff000839, 0xff42e300,
    0xff000842, 0xff4a0000, 0xff00084a, 0xff4a2000, 0xff000852, 0xff4a4100, 0xff00085a, 0xff4a6100,
    0xff000863, 0xff4a8200, 0xff00086b, 0xff4aa200, 0xff000873, 0xff4ac300, 0xff00087b, 0xff4ae300,
    0xff000884, 0xff520000, 0xff00088c, 0xff522000, 0xff000894, 0xff524100, 0xff00089c, 0xff526100,
    0xff0008a5, 0xff528200, 0xff0008ad, 0xff52a200, 0xff0008b5, 0xff52c300, 0xff0008bd, 0xff52e300,
    0xff0008c6, 0xff5a0000, 0xff0008ce, 0xff5a2000, 0xff0008d6, 0xff5a4100, 0xff0008de, 0xff5a6100,
    0xff0008e7, 0xff5a8200, 0xff0008ef, 0xff5aa200, 0xff0008f7, 0xff5ac300, 0xff0008ff, 0xff5ae300,
    0xff000c00, 0xff630000, 0xff000c08, 0xff632000, 0xff000c10, 0xff634100, 0xff000c18, 0xff636100,
    0xff000c21, 0xff638200, 0xff000c29, 0xff63a200, 0xff000c31, 0xff63c300, 0xff000c39, 0xff63e300,
    0xff000c42, 0xff6b0000, 0xff000c4a, 0xff6b2000, 0xff000c52, 0xff6b4100, 0xff000c5a, 0xff6b6100,
    0xff000c63, 0xff6b8200, 0xff000c6b, 0xff6ba200, 0xff000c73, 0xff6bc300, 0xff000c7b, 0xff6be300,
    0xff000c84, 0xff730000, 0xff000c8c, 0xff732000, 0xff000c94, 0xff734100, 0xff000c9c, 0xff736100,
    0xff000ca5, 0xff738200, 0xff000cad, 0xff73a200, 0xff000cb5, 0xff73c300, 0xff000cbd, 0xff73e300,
    0xff000cc6, 0xff7b0000, 0xff000cce, 0xff7b2000, 0xff000cd6, 0xff7b4100, 0xff000cde, 0xff7b6100,
    0xff000ce7, 0xff7b8200, 0xff000cef, 0xff7ba200, 0xff000cf7, 0xff7bc300, 0xff000cff, 0xff7be300,
    0xff001000, 0xff840000, 0xff001008, 0xff842000, 0xff001010, 0xff844100, 0xff001018, 0xff846100,
    0xff001021, 0xff848200, 0xff001029, 0xff84a200, 0xff001031, 0xff84c300, 0xff001039, 0xff84e300,
    0xff001042, 0xff8c0000, 0xff00104a, 0xff8c2000, 0xff001052, 0xff8c4100, 0xff00105a, 0xff8c6100,
    0xff001063, 0xff8c8200, 0xff00106b, 0xff8ca200, 0xff001073, 0xff8cc300, 0xff00107b, 0xff8ce300,
    0xff001084, 0xff940000, 0xff00108c, 0xff942000, 0xff001094, 0xff944100, 0xff00109c, 0xff946100,
    0xff0010a5, 0xff948200, 0xff0010ad, 0xff94a200, 0xff0010b5, 0xff94c300, 0xff0010bd, 0xff94e300,
    0xff0010c6, 0xff9c0000, 0xff0010ce, 0xff9c2000, 0xff0010d6, 0xff9c4100, 0xff0010de, 0xff9c6100,
    0xff0010e7, 0xff9c8200, 0xff0010ef, 0xff9ca200, 0xff0010f7, 0xff9cc300, 0xff0010ff, 0xff9ce300,
    0xff001400, 0xffa50000, 0xff001408, 0xffa52000, 0xff001410, 0xffa54100, 0xff001418, 0xffa56100,
    0xff001421, 0xffa58200, 0xff001429, 0xffa5a200, 0xff001431, 0xffa5c300, 0xff001439, 0xffa5e300,
    0xff001442, 0xffad0000, 0xff00144a, 0xffad2000, 0xff001452, 0xffad4100, 0xff00145a, 0xffad6100,
    0xff001463, 0xffad8200, 0xff00146b, 0xffada200, 0xff001473, 0xffadc300, 0xff00147b, 0xffade300,
    0xff001484, 0xffb50000, 0xff00148c, 0xffb52000, 0xff001494, 0xffb54100, 0xff00149c, 0xffb56100,
    0xff0014a5, 0xffb58200, 0xff0014ad, 0xffb5a200, 0xff0014b5, 0xffb5c300, 0xff0014bd, 0xffb5e300,
    0xff0014c6, 0xffbd0000, 0xff0014ce, 0xffbd2000, 0xff0014d6, 0xffbd4100, 0xff0014de, 0xffbd6100,
    0xff0014e7, 0xffbd8200, 0xff0014ef, 0xffbda200, 0xff0014f7, 0xffbdc300, 0xff0014ff, 0xffbde300,
    0xff001800, 0xffc60000, 0xff001808, 0xffc62000, 0xff001810, 0xffc64100, 0xff001818, 0xffc66100,
    0xff001821, 0xffc68200, 0xff001829, 0xffc6a200, 0xff001831, 0xffc6c300, 0xff001839, 0xffc6e300,
    0xff001842, 0xffce0000, 0xff00184a, 0xffce2000, 0xff001852, 0xffce4100, 0xff00185a, 0xffce6100,
    0xff001863, 0xffce8200, 0xff00186b, 0xffcea200, 0xff001873, 0xffcec300, 0xff00187b, 0xffcee300,
    0xff001884, 0xffd60000, 0xff00188c, 0xffd62000, 0xff001894, 0xffd64100, 0xff00189c, 0xffd66100,
    0xff0018a5, 0xffd68200, 0xff0018ad, 0xffd6a200, 0xff0018b5, 0xffd6c300, 0xff0018bd, 0xffd6e300,
    0xff0018c6, 0xffde0000, 0xff0018ce, 0xffde2000, 0xff0018d6, 0xffde4100, 0xff0018de, 0xffde6100,
    0xff0018e7, 0xffde8200, 0xff0018ef, 0xffdea200, 0xff0018f7, 0xffdec300, 0xff0018ff, 0xffdee300,
    0xff001c00, 0xffe70000, 0xff001c08, 0xffe72000, 0xff001c10, 0xffe74100, 0xff001c18, 0xffe76100,
    0xff001c21, 0xffe78200, 0xff001c29, 0xffe7a200, 0xff001c31, 0xffe7c300, 0xff001c39, 0xffe7e300,
    0xff001c42, 0xffef0000, 0xff001c4a, 0xffef2000, 0xff001c52, 0xffef4100, 0xff001c5a, 0xffef6100,
    0xff001c63, 0xffef8200, 0xff001c6b, 0xffefa200, 0xff001c73, 0xffefc300, 0xff001c7b, 0xffefe300,
    0xff001c84, 0xfff70000, 0xff001c8c, 0xfff72000, 0xff001c94, 0xfff74100, 0xff001c9c, 0xfff76100,
    0xff001ca5, 0xfff78200, 0xff001cad, 0xfff7a200, 0xff001cb5, 0xfff7c300, 0xff001cbd, 0xfff7e300,
    0xff001cc6, 0xffff0000, 0xff001cce, 0xffff2000, 0xff001cd6, 0xffff4100, 0xff001cde, 0xffff6100,
    0xff001ce7, 0xffff8200, 0xff001cef, 0xffffa200, 0xff001cf7, 0xffffc300, 0xff001cff, 0xffffe300
};

static void Blit_RGB565_ARGB8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_ARGB8888_LUT);
}

// Special optimized blit for RGB565 -> ABGR8888

static const Uint32 RGB565_ABGR8888_LUT[512] = {
    0xff000000, 0xff000000, 0xff080000, 0xff002000, 0xff100000, 0xff004100, 0xff180000, 0xff006100,
    0xff210000, 0xff008200, 0xff290000, 0xff00a200, 0xff310000, 0xff00c300, 0xff390000, 0xff00e300,
    0xff420000, 0xff000008, 0xff4a0000, 0xff002008, 0xff520000, 0xff004108, 0xff5a0000, 0xff006108,
    0xff630000, 0xff008208, 0xff6b0000, 0xff00a208, 0xff730000, 0xff00c308, 0xff7b0000, 0xff00e308,
    0xff840000, 0xff000010, 0xff8c0000, 0xff002010, 0xff940000, 0xff004110, 0xff9c0000, 0xff006110,
    0xffa50000, 0xff008210, 0xffad0000, 0xff00a210, 0xffb50000, 0xff00c310, 0xffbd0000, 0xff00e310,
    0xffc60000, 0xff000018, 0xffce0000, 0xff002018, 0xffd60000, 0xff004118, 0xffde0000, 0xff006118,
    0xffe70000, 0xff008218, 0xffef0000, 0xff00a218, 0xfff70000, 0xff00c318, 0xffff0000, 0xff00e318,
    0xff000400, 0xff000021, 0xff080400, 0xff002021, 0xff100400, 0xff004121, 0xff180400, 0xff006121,
    0xff210400, 0xff008221, 0xff290400, 0xff00a221, 0xff310400, 0xff00c321, 0xff390400, 0xff00e321,
    0xff420400, 0xff000029, 0xff4a0400, 0xff002029, 0xff520400, 0xff004129, 0xff5a0400, 0xff006129,
    0xff630400, 0xff008229, 0xff6b0400, 0xff00a229, 0xff730400, 0xff00c329, 0xff7b0400, 0xff00e329,
    0xff840400, 0xff000031, 0xff8c0400, 0xff002031, 0xff940400, 0xff004131, 0xff9c0400, 0xff006131,
    0xffa50400, 0xff008231, 0xffad0400, 0xff00a231, 0xffb50400, 0xff00c331, 0xffbd0400, 0xff00e331,
    0xffc60400, 0xff000039, 0xffce0400, 0xff002039, 0xffd60400, 0xff004139, 0xffde0400, 0xff006139,
    0xffe70400, 0xff008239, 0xffef0400, 0xff00a239, 0xfff70400, 0xff00c339, 0xffff0400, 0xff00e339,
    0xff000800, 0xff000042, 0xff080800, 0xff002042, 0xff100800, 0xff004142, 0xff180800, 0xff006142,
    0xff210800, 0xff008242, 0xff290800, 0xff00a242, 0xff310800, 0xff00c342, 0xff390800, 0xff00e342,
    0xff420800, 0xff00004a, 0xff4a0800, 0xff00204a, 0xff520800, 0xff00414a, 0xff5a0800, 0xff00614a,
    0xff630800, 0xff00824a, 0xff6b0800, 0xff00a24a, 0xff730800, 0xff00c34a, 0xff7b0800, 0xff00e34a,
    0xff840800, 0xff000052, 0xff8c0800, 0xff002052, 0xff940800, 0xff004152, 0xff9c0800, 0xff006152,
    0xffa50800, 0xff008252, 0xffad0800, 0xff00a252, 0xffb50800, 0xff00c352, 0xffbd0800, 0xff00e352,
    0xffc60800, 0xff00005a, 0xffce0800, 0xff00205a, 0xffd60800, 0xff00415a, 0xffde0800, 0xff00615a,
    0xffe70800, 0xff00825a, 0xffef0800, 0xff00a25a, 0xfff70800, 0xff00c35a, 0xffff0800, 0xff00e35a,
    0xff000c00, 0xff000063, 0xff080c00, 0xff002063, 0xff100c00, 0xff004163, 0xff180c00, 0xff006163,
    0xff210c00, 0xff008263, 0xff290c00, 0xff00a263, 0xff310c00, 0xff00c363, 0xff390c00, 0xff00e363,
    0xff420c00, 0xff00006b, 0xff4a0c00, 0xff00206b, 0xff520c00, 0xff00416b, 0xff5a0c00, 0xff00616b,
    0xff630c00, 0xff00826b, 0xff6b0c00, 0xff00a26b, 0xff730c00, 0xff00c36b, 0xff7b0c00, 0xff00e36b,
    0xff840c00, 0xff000073, 0xff8c0c00, 0xff002073, 0xff940c00, 0xff004173, 0xff9c0c00, 0xff006173,
    0xffa50c00, 0xff008273, 0xffad0c00, 0xff00a273, 0xffb50c00, 0xff00c373, 0xffbd0c00, 0xff00e373,
    0xffc60c00, 0xff00007b, 0xffce0c00, 0xff00207b, 0xffd60c00, 0xff00417b, 0xffde0c00, 0xff00617b,
    0xffe70c00, 0xff00827b, 0xffef0c00, 0xff00a27b, 0xfff70c00, 0xff00c37b, 0xffff0c00, 0xff00e37b,
    0xff001000, 0xff000084, 0xff081000, 0xff002084, 0xff101000, 0xff004184, 0xff181000, 0xff006184,
    0xff211000, 0xff008284, 0xff291000, 0xff00a284, 0xff311000, 0xff00c384, 0xff391000, 0xff00e384,
    0xff421000, 0xff00008c, 0xff4a1000, 0xff00208c, 0xff521000, 0xff00418c, 0xff5a1000, 0xff00618c,
    0xff631000, 0xff00828c, 0xff6b1000, 0xff00a28c, 0xff731000, 0xff00c38c, 0xff7b1000, 0xff00e38c,
    0xff841000, 0xff000094, 0xff8c1000, 0xff002094, 0xff941000, 0xff004194, 0xff9c1000, 0xff006194,
    0xffa51000, 0xff008294, 0xffad1000, 0xff00a294, 0xffb51000, 0xff00c394, 0xffbd1000, 0xff00e394,
    0xffc61000, 0xff00009c, 0xffce1000, 0xff00209c, 0xffd61000, 0xff00419c, 0xffde1000, 0xff00619c,
    0xffe71000, 0xff00829c, 0xffef1000, 0xff00a29c, 0xfff71000, 0xff00c39c, 0xffff1000, 0xff00e39c,
    0xff001400, 0xff0000a5, 0xff081400, 0xff0020a5, 0xff101400, 0xff0041a5, 0xff181400, 0xff0061a5,
    0xff211400, 0xff0082a5, 0xff291400, 0xff00a2a5, 0xff311400, 0xff00c3a5, 0xff391400, 0xff00e3a5,
    0xff421400, 0xff0000ad, 0xff4a1400, 0xff0020ad, 0xff521400, 0xff0041ad, 0xff5a1400, 0xff0061ad,
    0xff631400, 0xff0082ad, 0xff6b1400, 0xff00a2ad, 0xff731400, 0xff00c3ad, 0xff7b1400, 0xff00e3ad,
    0xff841400, 0xff0000b5, 0xff8c1400, 0xff0020b5, 0xff941400, 0xff0041b5, 0xff9c1400, 0xff0061b5,
    0xffa51400, 0xff0082b5, 0xffad1400, 0xff00a2b5, 0xffb51400, 0xff00c3b5, 0xffbd1400, 0xff00e3b5,
    0xffc61400, 0xff0000bd, 0xffce1400, 0xff0020bd, 0xffd61400, 0xff0041bd, 0xffde1400, 0xff0061bd,
    0xffe71400, 0xff0082bd, 0xffef1400, 0xff00a2bd, 0xfff71400, 0xff00c3bd, 0xffff1400, 0xff00e3bd,
    0xff001800, 0xff0000c6, 0xff081800, 0xff0020c6, 0xff101800, 0xff0041c6, 0xff181800, 0xff0061c6,
    0xff211800, 0xff0082c6, 0xff291800, 0xff00a2c6, 0xff311800, 0xff00c3c6, 0xff391800, 0xff00e3c6,
    0xff421800, 0xff0000ce, 0xff4a1800, 0xff0020ce, 0xff521800, 0xff0041ce, 0xff5a1800, 0xff0061ce,
    0xff631800, 0xff0082ce, 0xff6b1800, 0xff00a2ce, 0xff731800, 0xff00c3ce, 0xff7b1800, 0xff00e3ce,
    0xff841800, 0xff0000d6, 0xff8c1800, 0xff0020d6, 0xff941800, 0xff0041d6, 0xff9c1800, 0xff0061d6,
    0xffa51800, 0xff0082d6, 0xffad1800, 0xff00a2d6, 0xffb51800, 0xff00c3d6, 0xffbd1800, 0xff00e3d6,
    0xffc61800, 0xff0000de, 0xffce1800, 0xff0020de, 0xffd61800, 0xff0041de, 0xffde1800, 0xff0061de,
    0xffe71800, 0xff0082de, 0xffef1800, 0xff00a2de, 0xfff71800, 0xff00c3de, 0xffff1800, 0xff00e3de,
    0xff001c00, 0xff0000e7, 0xff081c00, 0xff0020e7, 0xff101c00, 0xff0041e7, 0xff181c00, 0xff0061e7,
    0xff211c00, 0xff0082e7, 0xff291c00, 0xff00a2e7, 0xff311c00, 0xff00c3e7, 0xff391c00, 0xff00e3e7,
    0xff421c00, 0xff0000ef, 0xff4a1c00, 0xff0020ef, 0xff521c00, 0xff0041ef, 0xff5a1c00, 0xff0061ef,
    0xff631c00, 0xff0082ef, 0xff6b1c00, 0xff00a2ef, 0xff731c00, 0xff00c3ef, 0xff7b1c00, 0xff00e3ef,
    0xff841c00, 0xff0000f7, 0xff8c1c00, 0xff0020f7, 0xff941c00, 0xff0041f7, 0xff9c1c00, 0xff0061f7,
    0xffa51c00, 0xff0082f7, 0xffad1c00, 0xff00a2f7, 0xffb51c00, 0xff00c3f7, 0xffbd1c00, 0xff00e3f7,
    0xffc61c00, 0xff0000ff, 0xffce1c00, 0xff0020ff, 0xffd61c00, 0xff0041ff, 0xffde1c00, 0xff0061ff,
    0xffe71c00, 0xff0082ff, 0xffef1c00, 0xff00a2ff, 0xfff71c00, 0xff00c3ff, 0xffff1c00, 0xff00e3ff
};

static void Blit_RGB565_ABGR8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_ABGR8888_LUT);
}

// Special optimized blit for RGB565 -> RGBA8888

static const Uint32 RGB565_RGBA8888_LUT[512] = {
    0x000000ff, 0x000000ff, 0x000008ff, 0x002000ff, 0x000010ff, 0x004100ff, 0x000018ff, 0x006100ff,
    0x000021ff, 0x008200ff, 0x000029ff, 0x00a200ff, 0x000031ff, 0x00c300ff, 0x000039ff, 0x00e300ff,
    0x000042ff, 0x080000ff, 0x00004aff, 0x082000ff, 0x000052ff, 0x084100ff, 0x00005aff, 0x086100ff,
    0x000063ff, 0x088200ff, 0x00006bff, 0x08a200ff, 0x000073ff, 0x08c300ff, 0x00007bff, 0x08e300ff,
    0x000084ff, 0x100000ff, 0x00008cff, 0x102000ff, 0x000094ff, 0x104100ff, 0x00009cff, 0x106100ff,
    0x0000a5ff, 0x108200ff, 0x0000adff, 0x10a200ff, 0x0000b5ff, 0x10c300ff, 0x0000bdff, 0x10e300ff,
    0x0000c6ff, 0x180000ff, 0x0000ceff, 0x182000ff, 0x0000d6ff, 0x184100ff, 0x0000deff, 0x186100ff,
    0x0000e7ff, 0x188200ff, 0x0000efff, 0x18a200ff, 0x0000f7ff, 0x18c300ff, 0x0000ffff, 0x18e300ff,
    0x000400ff, 0x210000ff, 0x000408ff, 0x212000ff, 0x000410ff, 0x214100ff, 0x000418ff, 0x216100ff,
    0x000421ff, 0x218200ff, 0x000429ff, 0x21a200ff, 0x000431ff, 0x21c300ff, 0x000439ff, 0x21e300ff,
    0x000442ff, 0x290000ff, 0x00044aff, 0x292000ff, 0x000452ff, 0x294100ff, 0x00045aff, 0x296100ff,
    0x000463ff, 0x298200ff, 0x00046bff, 0x29a200ff, 0x000473ff, 0x29c300ff, 0x00047bff, 0x29e300ff,
    0x000484ff, 0x310000ff, 0x00048cff, 0x312000ff, 0x000494ff, 0x314100ff, 0x00049cff, 0x316100ff,
    0x0004a5ff, 0x318200ff, 0x0004adff, 0x31a200ff, 0x0004b5ff, 0x31c300ff, 0x0004bdff, 0x31e300ff,
    0x0004c6ff, 0x390000ff, 0x0004ceff, 0x392000ff, 0x0004d6ff, 0x394100ff, 0x0004deff, 0x396100ff,
    0x0004e7ff, 0x398200ff, 0x0004efff, 0x39a200ff, 0x0004f7ff, 0x39c300ff, 0x0004ffff, 0x39e300ff,
    0x000800ff, 0x420000ff, 0x000808ff, 0x422000ff, 0x000810ff, 0x424100ff, 0x000818ff, 0x426100ff,
    0x000821ff, 0x428200ff, 0x000829ff, 0x42a200ff, 0x000831ff, 0x42c300ff, 0x000839ff, 0x42e300ff,
    0x000842ff, 0x4a0000ff, 0x00084aff, 0x4a2000ff, 0x000852ff, 0x4a4100ff, 0x00085aff, 0x4a6100ff,
    0x000863ff, 0x4a8200ff, 0x00086bff, 0x4aa200ff, 0x000873ff, 0x4ac300ff, 0x00087bff, 0x4ae300ff,
    0x000884ff, 0x520000ff, 0x00088cff, 0x522000ff, 0x000894ff, 0x524100ff, 0x00089cff, 0x526100ff,
    0x0008a5ff, 0x528200ff, 0x0008adff, 0x52a200ff, 0x0008b5ff, 0x52c300ff, 0x0008bdff, 0x52e300ff,
    0x0008c6ff, 0x5a0000ff, 0x0008ceff, 0x5a2000ff, 0x0008d6ff, 0x5a4100ff, 0x0008deff, 0x5a6100ff,
    0x0008e7ff, 0x5a8200ff, 0x0008efff, 0x5aa200ff, 0x0008f7ff, 0x5ac300ff, 0x0008ffff, 0x5ae300ff,
    0x000c00ff, 0x630000ff, 0x000c08ff, 0x632000ff, 0x000c10ff, 0x634100ff, 0x000c18ff, 0x636100ff,
    0x000c21ff, 0x638200ff, 0x000c29ff, 0x63a200ff, 0x000c31ff, 0x63c300ff, 0x000c39ff, 0x63e300ff,
    0x000c42ff, 0x6b0000ff, 0x000c4aff, 0x6b2000ff, 0x000c52ff, 0x6b4100ff, 0x000c5aff, 0x6b6100ff,
    0x000c63ff, 0x6b8200ff, 0x000c6bff, 0x6ba200ff, 0x000c73ff, 0x6bc300ff, 0x000c7bff, 0x6be300ff,
    0x000c84ff, 0x730000ff, 0x000c8cff, 0x732000ff, 0x000c94ff, 0x734100ff, 0x000c9cff, 0x736100ff,
    0x000ca5ff, 0x738200ff, 0x000cadff, 0x73a200ff, 0x000cb5ff, 0x73c300ff, 0x000cbdff, 0x73e300ff,
    0x000cc6ff, 0x7b0000ff, 0x000cceff, 0x7b2000ff, 0x000cd6ff, 0x7b4100ff, 0x000cdeff, 0x7b6100ff,
    0x000ce7ff, 0x7b8200ff, 0x000cefff, 0x7ba200ff, 0x000cf7ff, 0x7bc300ff, 0x000cffff, 0x7be300ff,
    0x001000ff, 0x840000ff, 0x001008ff, 0x842000ff, 0x001010ff, 0x844100ff, 0x001018ff, 0x846100ff,
    0x001021ff, 0x848200ff, 0x001029ff, 0x84a200ff, 0x001031ff, 0x84c300ff, 0x001039ff, 0x84e300ff,
    0x001042ff, 0x8c0000ff, 0x00104aff, 0x8c2000ff, 0x001052ff, 0x8c4100ff, 0x00105aff, 0x8c6100ff,
    0x001063ff, 0x8c8200ff, 0x00106bff, 0x8ca200ff, 0x001073ff, 0x8cc300ff, 0x00107bff, 0x8ce300ff,
    0x001084ff, 0x940000ff, 0x00108cff, 0x942000ff, 0x001094ff, 0x944100ff, 0x00109cff, 0x946100ff,
    0x0010a5ff, 0x948200ff, 0x0010adff, 0x94a200ff, 0x0010b5ff, 0x94c300ff, 0x0010bdff, 0x94e300ff,
    0x0010c6ff, 0x9c0000ff, 0x0010ceff, 0x9c2000ff, 0x0010d6ff, 0x9c4100ff, 0x0010deff, 0x9c6100ff,
    0x0010e7ff, 0x9c8200ff, 0x0010efff, 0x9ca200ff, 0x0010f7ff, 0x9cc300ff, 0x0010ffff, 0x9ce300ff,
    0x001400ff, 0xa50000ff, 0x001408ff, 0xa52000ff, 0x001410ff, 0xa54100ff, 0x001418ff, 0xa56100ff,
    0x001421ff, 0xa58200ff, 0x001429ff, 0xa5a200ff, 0x001431ff, 0xa5c300ff, 0x001439ff, 0xa5e300ff,
    0x001442ff, 0xad0000ff, 0x00144aff, 0xad2000ff, 0x001452ff, 0xad4100ff, 0x00145aff, 0xad6100ff,
    0x001463ff, 0xad8200ff, 0x00146bff, 0xada200ff, 0x001473ff, 0xadc300ff, 0x00147bff, 0xade300ff,
    0x001484ff, 0xb50000ff, 0x00148cff, 0xb52000ff, 0x001494ff, 0xb54100ff, 0x00149cff, 0xb56100ff,
    0x0014a5ff, 0xb58200ff, 0x0014adff, 0xb5a200ff, 0x0014b5ff, 0xb5c300ff, 0x0014bdff, 0xb5e300ff,
    0x0014c6ff, 0xbd0000ff, 0x0014ceff, 0xbd2000ff, 0x0014d6ff, 0xbd4100ff, 0x0014deff, 0xbd6100ff,
    0x0014e7ff, 0xbd8200ff, 0x0014efff, 0xbda200ff, 0x0014f7ff, 0xbdc300ff, 0x0014ffff, 0xbde300ff,
    0x001800ff, 0xc60000ff, 0x001808ff, 0xc62000ff, 0x001810ff, 0xc64100ff, 0x001818ff, 0xc66100ff,
    0x001821ff, 0xc68200ff, 0x001829ff, 0xc6a200ff, 0x001831ff, 0xc6c300ff, 0x001839ff, 0xc6e300ff,
    0x001842ff, 0xce0000ff, 0x00184aff, 0xce2000ff, 0x001852ff, 0xce4100ff, 0x00185aff, 0xce6100ff,
    0x001863ff, 0xce8200ff, 0x00186bff, 0xcea200ff, 0x001873ff, 0xcec300ff, 0x00187bff, 0xcee300ff,
    0x001884ff, 0xd60000ff, 0x00188cff, 0xd62000ff, 0x001894ff, 0xd64100ff, 0x00189cff, 0xd66100ff,
    0x0018a5ff, 0xd68200ff, 0x0018adff, 0xd6a200ff, 0x0018b5ff, 0xd6c300ff, 0x0018bdff, 0xd6e300ff,
    0x0018c6ff, 0xde0000ff, 0x0018ceff, 0xde2000ff, 0x0018d6ff, 0xde4100ff, 0x0018deff, 0xde6100ff,
    0x0018e7ff, 0xde8200ff, 0x0018efff, 0xdea200ff, 0x0018f7ff, 0xdec300ff, 0x0018ffff, 0xdee300ff,
    0x001c00ff, 0xe70000ff, 0x001c08ff, 0xe72000ff, 0x001c10ff, 0xe74100ff, 0x001c18ff, 0xe76100ff,
    0x001c21ff, 0xe78200ff, 0x001c29ff, 0xe7a200ff, 0x001c31ff, 0xe7c300ff, 0x001c39ff, 0xe7e300ff,
    0x001c42ff, 0xef0000ff, 0x001c4aff, 0xef2000ff, 0x001c52ff, 0xef4100ff, 0x001c5aff, 0xef6100ff,
    0x001c63ff, 0xef8200ff, 0x001c6bff, 0xefa200ff, 0x001c73ff, 0xefc300ff, 0x001c7bff, 0xefe300ff,
    0x001c84ff, 0xf70000ff, 0x001c8cff, 0xf72000ff, 0x001c94ff, 0xf74100ff, 0x001c9cff, 0xf76100ff,
    0x001ca5ff, 0xf78200ff, 0x001cadff, 0xf7a200ff, 0x001cb5ff, 0xf7c300ff, 0x001cbdff, 0xf7e300ff,
    0x001cc6ff, 0xff0000ff, 0x001cceff, 0xff2000ff, 0x001cd6ff, 0xff4100ff, 0x001cdeff, 0xff6100ff,
    0x001ce7ff, 0xff8200ff, 0x001cefff, 0xffa200ff, 0x001cf7ff, 0xffc300ff, 0x001cffff, 0xffe300ff
};

static void Blit_RGB565_RGBA8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_RGBA8888_LUT);
}

// Special optimized blit for RGB565 -> BGRA8888

static const Uint32 RGB565_BGRA8888_LUT[512] = {
    0x000000ff, 0x000000ff, 0x080000ff, 0x002000ff, 0x100000ff, 0x004100ff, 0x180000ff, 0x006100ff,
    0x210000ff, 0x008200ff, 0x290000ff, 0x00a200ff, 0x310000ff, 0x00c300ff, 0x390000ff, 0x00e300ff,
    0x420000ff, 0x000008ff, 0x4a0000ff, 0x002008ff, 0x520000ff, 0x004108ff, 0x5a0000ff, 0x006108ff,
    0x630000ff, 0x008208ff, 0x6b0000ff, 0x00a208ff, 0x730000ff, 0x00c308ff, 0x7b0000ff, 0x00e308ff,
    0x840000ff, 0x000010ff, 0x8c0000ff, 0x002010ff, 0x940000ff, 0x004110ff, 0x9c0000ff, 0x006110ff,
    0xa50000ff, 0x008210ff, 0xad0000ff, 0x00a210ff, 0xb50000ff, 0x00c310ff, 0xbd0000ff, 0x00e310ff,
    0xc60000ff, 0x000018ff, 0xce0000ff, 0x002018ff, 0xd60000ff, 0x004118ff, 0xde0000ff, 0x006118ff,
    0xe70000ff, 0x008218ff, 0xef0000ff, 0x00a218ff, 0xf70000ff, 0x00c318ff, 0xff0000ff, 0x00e318ff,
    0x000400ff, 0x000021ff, 0x080400ff, 0x002021ff, 0x100400ff, 0x004121ff, 0x180400ff, 0x006121ff,
    0x210400ff, 0x008221ff, 0x290400ff, 0x00a221ff, 0x310400ff, 0x00c321ff, 0x390400ff, 0x00e321ff,
    0x420400ff, 0x000029ff, 0x4a0400ff, 0x002029ff, 0x520400ff, 0x004129ff, 0x5a0400ff, 0x006129ff,
    0x630400ff, 0x008229ff, 0x6b0400ff, 0x00a229ff, 0x730400ff, 0x00c329ff, 0x7b0400ff, 0x00e329ff,
    0x840400ff, 0x000031ff, 0x8c0400ff, 0x002031ff, 0x940400ff, 0x004131ff, 0x9c0400ff, 0x006131ff,
    0xa50400ff, 0x008231ff, 0xad0400ff, 0x00a231ff, 0xb50400ff, 0x00c331ff, 0xbd0400ff, 0x00e331ff,
    0xc60400ff, 0x000039ff, 0xce0400ff, 0x002039ff, 0xd60400ff, 0x004139ff, 0xde0400ff, 0x006139ff,
    0xe70400ff, 0x008239ff, 0xef0400ff, 0x00a239ff, 0xf70400ff, 0x00c339ff, 0xff0400ff, 0x00e339ff,
    0x000800ff, 0x000042ff, 0x080800ff, 0x002042ff, 0x100800ff, 0x004142ff, 0x180800ff, 0x006142ff,
    0x210800ff, 0x008242ff, 0x290800ff, 0x00a242ff, 0x310800ff, 0x00c342ff, 0x390800ff, 0x00e342ff,
    0x420800ff, 0x00004aff, 0x4a0800ff, 0x00204aff, 0x520800ff, 0x00414aff, 0x5a0800ff, 0x00614aff,
    0x630800ff, 0x00824aff, 0x6b0800ff, 0x00a24aff, 0x730800ff, 0x00c34aff, 0x7b0800ff, 0x00e34aff,
    0x840800ff, 0x000052ff, 0x8c0800ff, 0x002052ff, 0x940800ff, 0x004152ff, 0x9c0800ff, 0x006152ff,
    0xa50800ff, 0x008252ff, 0xad0800ff, 0x00a252ff, 0xb50800ff, 0x00c352ff, 0xbd0800ff, 0x00e352ff,
    0xc60800ff, 0x00005aff, 0xce0800ff, 0x00205aff, 0xd60800ff, 0x00415aff, 0xde0800ff, 0x00615aff,
    0xe70800ff, 0x00825aff, 0xef0800ff, 0x00a25aff, 0xf70800ff, 0x00c35aff, 0xff0800ff, 0x00e35aff,
    0x000c00ff, 0x000063ff, 0x080c00ff, 0x002063ff, 0x100c00ff, 0x004163ff, 0x180c00ff, 0x006163ff,
    0x210c00ff, 0x008263ff, 0x290c00ff, 0x00a263ff, 0x310c00ff, 0x00c363ff, 0x390c00ff, 0x00e363ff,
    0x420c00ff, 0x00006bff, 0x4a0c00ff, 0x00206bff, 0x520c00ff, 0x00416bff, 0x5a0c00ff, 0x00616bff,
    0x630c00ff, 0x00826bff, 0x6b0c00ff, 0x00a26bff, 0x730c00ff, 0x00c36bff, 0x7b0c00ff, 0x00e36bff,
    0x840c00ff, 0x000073ff, 0x8c0c00ff, 0x002073ff, 0x940c00ff, 0x004173ff, 0x9c0c00ff, 0x006173ff,
    0xa50c00ff, 0x008273ff, 0xad0c00ff, 0x00a273ff, 0xb50c00ff, 0x00c373ff, 0xbd0c00ff, 0x00e373ff,
    0xc60c00ff, 0x00007bff, 0xce0c00ff, 0x00207bff, 0xd60c00ff, 0x00417bff, 0xde0c00ff, 0x00617bff,
    0xe70c00ff, 0x00827bff, 0xef0c00ff, 0x00a27bff, 0xf70c00ff, 0x00c37bff, 0xff0c00ff, 0x00e37bff,
    0x001000ff, 0x000084ff, 0x081000ff, 0x002084ff, 0x101000ff, 0x004184ff, 0x181000ff, 0x006184ff,
    0x211000ff, 0x008284ff, 0x291000ff, 0x00a284ff, 0x311000ff, 0x00c384ff, 0x391000ff, 0x00e384ff,
    0x421000ff, 0x00008cff, 0x4a1000ff, 0x00208cff, 0x521000ff, 0x00418cff, 0x5a1000ff, 0x00618cff,
    0x631000ff, 0x00828cff, 0x6b1000ff, 0x00a28cff, 0x731000ff, 0x00c38cff, 0x7b1000ff, 0x00e38cff,
    0x841000ff, 0x000094ff, 0x8c1000ff, 0x002094ff, 0x941000ff, 0x004194ff, 0x9c1000ff, 0x006194ff,
    0xa51000ff, 0x008294ff, 0xad1000ff, 0x00a294ff, 0xb51000ff, 0x00c394ff, 0xbd1000ff, 0x00e394ff,
    0xc61000ff, 0x00009cff, 0xce1000ff, 0x00209cff, 0xd61000ff, 0x00419cff, 0xde1000ff, 0x00619cff,
    0xe71000ff, 0x00829cff, 0xef1000ff, 0x00a29cff, 0xf71000ff, 0x00c39cff, 0xff1000ff, 0x00e39cff,
    0x001400ff, 0x0000a5ff, 0x081400ff, 0x0020a5ff, 0x101400ff, 0x0041a5ff, 0x181400ff, 0x0061a5ff,
    0x211400ff, 0x0082a5ff, 0x291400ff, 0x00a2a5ff, 0x311400ff, 0x00c3a5ff, 0x391400ff, 0x00e3a5ff,
    0x421400ff, 0x0000adff, 0x4a1400ff, 0x0020adff, 0x521400ff, 0x0041adff, 0x5a1400ff, 0x0061adff,
    0x631400ff, 0x0082adff, 0x6b1400ff, 0x00a2adff, 0x731400ff, 0x00c3adff, 0x7b1400ff, 0x00e3adff,
    0x841400ff, 0x0000b5ff, 0x8c1400ff, 0x0020b5ff, 0x941400ff, 0x0041b5ff, 0x9c1400ff, 0x0061b5ff,
    0xa51400ff, 0x0082b5ff, 0xad1400ff, 0x00a2b5ff, 0xb51400ff, 0x00c3b5ff, 0xbd1400ff, 0x00e3b5ff,
    0xc61400ff, 0x0000bdff, 0xce1400ff, 0x0020bdff, 0xd61400ff, 0x0041bdff, 0xde1400ff, 0x0061bdff,
    0xe71400ff, 0x0082bdff, 0xef1400ff, 0x00a2bdff, 0xf71400ff, 0x00c3bdff, 0xff1400ff, 0x00e3bdff,
    0x001800ff, 0x0000c6ff, 0x081800ff, 0x0020c6ff, 0x101800ff, 0x0041c6ff, 0x181800ff, 0x0061c6ff,
    0x211800ff, 0x0082c6ff, 0x291800ff, 0x00a2c6ff, 0x311800ff, 0x00c3c6ff, 0x391800ff, 0x00e3c6ff,
    0x421800ff, 0x0000ceff, 0x4a1800ff, 0x0020ceff, 0x521800ff, 0x0041ceff, 0x5a1800ff, 0x0061ceff,
    0x631800ff, 0x0082ceff, 0x6b1800ff, 0x00a2ceff, 0x731800ff, 0x00c3ceff, 0x7b1800ff, 0x00e3ceff,
    0x841800ff, 0x0000d6ff, 0x8c1800ff, 0x0020d6ff, 0x941800ff, 0x0041d6ff, 0x9c1800ff, 0x0061d6ff,
    0xa51800ff, 0x0082d6ff, 0xad1800ff, 0x00a2d6ff, 0xb51800ff, 0x00c3d6ff, 0xbd1800ff, 0x00e3d6ff,
    0xc61800ff, 0x0000deff, 0xce1800ff, 0x0020deff, 0xd61800ff, 0x0041deff, 0xde1800ff, 0x0061deff,
    0xe71800ff, 0x0082deff, 0xef1800ff, 0x00a2deff, 0xf71800ff, 0x00c3deff, 0xff1800ff, 0x00e3deff,
    0x001c00ff, 0x0000e7ff, 0x081c00ff, 0x0020e7ff, 0x101c00ff, 0x0041e7ff, 0x181c00ff, 0x0061e7ff,
    0x211c00ff, 0x0082e7ff, 0x291c00ff, 0x00a2e7ff, 0x311c00ff, 0x00c3e7ff, 0x391c00ff, 0x00e3e7ff,
    0x421c00ff, 0x0000efff, 0x4a1c00ff, 0x0020efff, 0x521c00ff, 0x0041efff, 0x5a1c00ff, 0x0061efff,
    0x631c00ff, 0x0082efff, 0x6b1c00ff, 0x00a2efff, 0x731c00ff, 0x00c3efff, 0x7b1c00ff, 0x00e3efff,
    0x841c00ff, 0x0000f7ff, 0x8c1c00ff, 0x0020f7ff, 0x941c00ff, 0x0041f7ff, 0x9c1c00ff, 0x0061f7ff,
    0xa51c00ff, 0x0082f7ff, 0xad1c00ff, 0x00a2f7ff, 0xb51c00ff, 0x00c3f7ff, 0xbd1c00ff, 0x00e3f7ff,
    0xc61c00ff, 0x0000ffff, 0xce1c00ff, 0x0020ffff, 0xd61c00ff, 0x0041ffff, 0xde1c00ff, 0x0061ffff,
    0xe71c00ff, 0x0082ffff, 0xef1c00ff, 0x00a2ffff, 0xf71c00ff, 0x00c3ffff, 0xff1c00ff, 0x00e3ffff
};

static void Blit_RGB565_BGRA8888(SDL_BlitInfo * info)
{
    Blit_RGB565_32(info, RGB565_BGRA8888_LUT);
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
            // This is an SSSE3 instruction
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
#ifdef BROKEN_ALTIVEC_BLITTERS
    // has-altivec
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC, Blit_RGB565_32Altivec, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x00007C00, 0x000003E0, 0x0000001F, 4, 0x00000000, 0x00000000, 0x00000000,
      BLIT_FEATURE_HAS_ALTIVEC, Blit_RGB555_32Altivec, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
#endif // BROKEN_ALTIVEC_BLITTERS
#endif
#ifdef SDL_SSE4_1_INTRINSICS
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x00FF0000, 0x0000FF00, 0x000000FF,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_32_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_32_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0xFF000000, 0x00FF0000, 0x0000FF00,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_32_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
    { 0x0000F800, 0x000007E0, 0x0000001F, 4, 0x0000FF00, 0x00FF0000, 0xFF000000,
      BLIT_FEATURE_HAS_SSE41, Blit_RGB565_32_SSE41, NO_ALPHA | COPY_ALPHA | SET_ALPHA },
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
