/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* This a stretch blit implementation based on ideas given to me by
   Tomasz Cejner - thanks! :)

   April 27, 2000 - Sam Lantinga
*/

#include "SDL_video.h"
#include "SDL_blit.h"
#include "SDL_render.h"

/* This isn't ready for general consumption yet - it should be folded
   into the general blitting mechanism.
*/

#if ((defined(_MSC_VER) && defined(_M_IX86))    || \
     (defined(__WATCOMC__) && defined(__386__)) || \
     (defined(__GNUC__) && defined(__i386__))) && SDL_ASSEMBLY_ROUTINES
/* There's a bug with gcc 4.4.1 and -O2 where srcp doesn't get the correct
 * value after the first scanline.  FIXME? */
/* #define USE_ASM_STRETCH */
#endif

#ifdef USE_ASM_STRETCH

#ifdef HAVE_MPROTECT
#include <sys/types.h>
#include <sys/mman.h>
#endif
#ifdef __GNUC__
#define PAGE_ALIGNED __attribute__((__aligned__(4096)))
#else
#define PAGE_ALIGNED
#endif

#if defined(_M_IX86) || defined(__i386__) || defined(__386__)
#define PREFIX16    0x66
#define STORE_BYTE  0xAA
#define STORE_WORD  0xAB
#define LOAD_BYTE   0xAC
#define LOAD_WORD   0xAD
#define RETURN      0xC3
#else
#error Need assembly opcodes for this architecture
#endif

static unsigned char copy_row[4096] PAGE_ALIGNED;

static int
generate_rowbytes(int src_w, int dst_w, int bpp)
{
    static struct
    {
        int bpp;
        int src_w;
        int dst_w;
        int status;
    } last;

    int i;
    int pos, inc;
    unsigned char *eip, *fence;
    unsigned char load, store;

    /* See if we need to regenerate the copy buffer */
    if ((src_w == last.src_w) && (dst_w == last.dst_w) && (bpp == last.bpp)) {
        return (last.status);
    }
    last.bpp = bpp;
    last.src_w = src_w;
    last.dst_w = dst_w;
    last.status = -1;

    switch (bpp) {
    case 1:
        load = LOAD_BYTE;
        store = STORE_BYTE;
        break;
    case 2:
    case 4:
        load = LOAD_WORD;
        store = STORE_WORD;
        break;
    default:
        return SDL_SetError("ASM stretch of %d bytes isn't supported", bpp);
    }
#ifdef HAVE_MPROTECT
    /* Make the code writeable */
    if (mprotect(copy_row, sizeof(copy_row), PROT_READ | PROT_WRITE) < 0) {
        return SDL_SetError("Couldn't make copy buffer writeable");
    }
#endif
    pos = 0x10000;
    inc = (src_w << 16) / dst_w;
    eip = copy_row;
    fence = copy_row + sizeof(copy_row)-2;
    for (i = 0; i < dst_w; ++i) {
        while (pos >= 0x10000L) {
            if (eip == fence) {
                return -1;
            }
            if (bpp == 2) {
                *eip++ = PREFIX16;
            }
            *eip++ = load;
            pos -= 0x10000L;
        }
        if (eip == fence) {
            return -1;
        }
        if (bpp == 2) {
            *eip++ = PREFIX16;
        }
        *eip++ = store;
        pos += inc;
    }
    *eip++ = RETURN;

#ifdef HAVE_MPROTECT
    /* Make the code executable but not writeable */
    if (mprotect(copy_row, sizeof(copy_row), PROT_READ | PROT_EXEC) < 0) {
        return SDL_SetError("Couldn't make copy buffer executable");
    }
#endif
    last.status = 0;
    return (0);
}

#endif /* USE_ASM_STRETCH */

#define DEFINE_COPY_ROW(name, type)         \
static void name(type *src, int src_w, type *dst, int dst_w)    \
{                                           \
    int i;                                  \
    int pos, inc;                           \
    type pixel = 0;                         \
                                            \
    pos = 0x10000;                          \
    inc = (src_w << 16) / dst_w;            \
    for ( i=dst_w; i>0; --i ) {             \
        while ( pos >= 0x10000L ) {         \
            pixel = *src++;                 \
            pos -= 0x10000L;                \
        }                                   \
        *dst++ = pixel;                     \
        pos += inc;                         \
    }                                       \
}
/* *INDENT-OFF* */
DEFINE_COPY_ROW(copy_row1, Uint8)
DEFINE_COPY_ROW(copy_row2, Uint16)
DEFINE_COPY_ROW(copy_row4, Uint32)
/* *INDENT-ON* */

/* The ASM code doesn't handle 24-bpp stretch blits */
static void
copy_row3(Uint8 * src, int src_w, Uint8 * dst, int dst_w)
{
    int i;
    int pos, inc;
    Uint8 pixel[3] = { 0, 0, 0 };

    pos = 0x10000;
    inc = (src_w << 16) / dst_w;
    for (i = dst_w; i > 0; --i) {
        while (pos >= 0x10000L) {
            pixel[0] = *src++;
            pixel[1] = *src++;
            pixel[2] = *src++;
            pos -= 0x10000L;
        }
        *dst++ = pixel[0];
        *dst++ = pixel[1];
        *dst++ = pixel[2];
        pos += inc;
    }
}

static int SDL_LowerSoftStretchNearest(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
static int SDL_LowerSoftStretchLinear(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
static int SDL_UpperSoftStretch(SDL_Surface * src, const SDL_Rect * srcrect, SDL_Surface * dst, const SDL_Rect * dstrect, SDL_ScaleMode scaleMode);

/* Perform a stretch blit between two surfaces of the same format.
   NOTE:  This function is not safe to call from multiple threads!
*/
int
SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
                SDL_Surface *dst, const SDL_Rect *dstrect)
{
    return SDL_UpperSoftStretch(src, srcrect, dst, dstrect, SDL_ScaleModeNearest);
}

int
SDL_SoftStretchLinear(SDL_Surface *src, const SDL_Rect *srcrect,
                      SDL_Surface *dst, const SDL_Rect *dstrect)
{
    return SDL_UpperSoftStretch(src, srcrect, dst, dstrect, SDL_ScaleModeLinear);
}

static int
SDL_UpperSoftStretch(SDL_Surface * src, const SDL_Rect * srcrect,
                SDL_Surface * dst, const SDL_Rect * dstrect, SDL_ScaleMode scaleMode)
{
    int ret;
    int src_locked;
    int dst_locked;
    SDL_Rect full_src;
    SDL_Rect full_dst;

    if (src->format->format != dst->format->format) {
        return SDL_SetError("Only works with same format surfaces");
    }

    if (scaleMode != SDL_ScaleModeNearest) {
        if (src->format->BytesPerPixel != 4 || src->format->format == SDL_PIXELFORMAT_ARGB2101010) {
            return SDL_SetError("Wrong format");
        }
    }

    /* Verify the blit rectangles */
    if (srcrect) {
        if ((srcrect->x < 0) || (srcrect->y < 0) ||
            ((srcrect->x + srcrect->w) > src->w) ||
            ((srcrect->y + srcrect->h) > src->h)) {
            return SDL_SetError("Invalid source blit rectangle");
        }
    } else {
        full_src.x = 0;
        full_src.y = 0;
        full_src.w = src->w;
        full_src.h = src->h;
        srcrect = &full_src;
    }
    if (dstrect) {
        if ((dstrect->x < 0) || (dstrect->y < 0) ||
            ((dstrect->x + dstrect->w) > dst->w) ||
            ((dstrect->y + dstrect->h) > dst->h)) {
            return SDL_SetError("Invalid destination blit rectangle");
        }
    } else {
        full_dst.x = 0;
        full_dst.y = 0;
        full_dst.w = dst->w;
        full_dst.h = dst->h;
        dstrect = &full_dst;
    }

    if (dstrect->w <= 0 || dstrect->h <= 0) {
        return 0;
    }

    /* Lock the destination if it's in hardware */
    dst_locked = 0;
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return SDL_SetError("Unable to lock destination surface");
        }
        dst_locked = 1;
    }
    /* Lock the source if it's in hardware */
    src_locked = 0;
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) {
            if (dst_locked) {
                SDL_UnlockSurface(dst);
            }
            return SDL_SetError("Unable to lock source surface");
        }
        src_locked = 1;
    }

    if (scaleMode == SDL_ScaleModeNearest) {
        ret = SDL_LowerSoftStretchNearest(src, srcrect, dst, dstrect);
    } else {
        ret = SDL_LowerSoftStretchLinear(src, srcrect, dst, dstrect);
    }

    /* We need to unlock the surfaces if they're locked */
    if (dst_locked) {
        SDL_UnlockSurface(dst);
    }
    if (src_locked) {
        SDL_UnlockSurface(src);
    }

    return ret;
}


int
SDL_LowerSoftStretchNearest(SDL_Surface *src, const SDL_Rect *srcrect,
                SDL_Surface *dst, const SDL_Rect *dstrect)
{
    int pos, inc;
    int dst_maxrow;
    int src_row, dst_row;
    Uint8 *srcp = NULL;
    Uint8 *dstp;
#ifdef USE_ASM_STRETCH
    SDL_bool use_asm = SDL_TRUE;
#ifdef __GNUC__
    int u1, u2;
#endif
#endif /* USE_ASM_STRETCH */
    const int bpp = dst->format->BytesPerPixel;

    /* Set up the data... */
    pos = 0x10000;
    inc = (srcrect->h << 16) / dstrect->h;
    src_row = srcrect->y;
    dst_row = dstrect->y;

#ifdef USE_ASM_STRETCH
    /* Write the opcodes for this stretch */
    if ((bpp == 3) || (generate_rowbytes(srcrect->w, dstrect->w, bpp) < 0)) {
        use_asm = SDL_FALSE;
    }
#endif

    /* Perform the stretch blit */
    for (dst_maxrow = dst_row + dstrect->h; dst_row < dst_maxrow; ++dst_row) {
        dstp = (Uint8 *) dst->pixels + (dst_row * dst->pitch)
            + (dstrect->x * bpp);
        while (pos >= 0x10000L) {
            srcp = (Uint8 *) src->pixels + (src_row * src->pitch)
                + (srcrect->x * bpp);
            ++src_row;
            pos -= 0x10000L;
        }
#ifdef USE_ASM_STRETCH
        if (use_asm) {
#ifdef __GNUC__
            __asm__ __volatile__("call *%4":"=&D"(u1), "=&S"(u2)
                                 :"0"(dstp), "1"(srcp), "r"(copy_row)
                                 :"memory");
#elif defined(_MSC_VER) || defined(__WATCOMC__)
            /* *INDENT-OFF* */
            {
                void *code = copy_row;
                __asm {
                    push edi
                    push esi
                    mov edi, dstp
                    mov esi, srcp
                    call dword ptr code
                    pop esi
                    pop edi
                }
            }
            /* *INDENT-ON* */
#else
#error Need inline assembly for this compiler
#endif
        } else
#endif
            switch (bpp) {
            case 1:
                copy_row1(srcp, srcrect->w, dstp, dstrect->w);
                break;
            case 2:
                copy_row2((Uint16 *) srcp, srcrect->w,
                          (Uint16 *) dstp, dstrect->w);
                break;
            case 3:
                copy_row3(srcp, srcrect->w, dstp, dstrect->w);
                break;
            case 4:
                copy_row4((Uint32 *) srcp, srcrect->w,
                          (Uint32 *) dstp, dstrect->w);
                break;
            }
        pos += inc;
    }

    return 0;
}


/* bilinear interpolation precision must be < 8
   Because with SSE: add-multiply: _mm_madd_epi16 works with signed int
   so pixels 0xb1...... are negatives and false the result
   same in NEON probably */
#define PRECISION      7

#define FIXED_POINT(i)  ((uint32_t)(i)  << 16)
#define SRC_INDEX(fp)   ((uint32_t)(fp) >> 16)
#define INTEGER(fp)     ((uint32_t)(fp) >> PRECISION)
#define FRAC(fp)        ((uint32_t)(fp >> (16 - PRECISION)) & ((1<<PRECISION) - 1))
#define FRAC_ZERO       0
#define FRAC_ONE        (1 << PRECISION)
#define FP_ONE          FIXED_POINT(1)


#define BILINEAR___START                                                                        \
    int i;                                                                                      \
    int fp_sum_h, fp_step_h, left_pad_h, right_pad_h;                                           \
    int fp_sum_w, fp_step_w, left_pad_w, right_pad_w;                                           \
    int fp_sum_w_init, left_pad_w_init, right_pad_w_init, dst_gap, middle_init;                 \
    get_scaler_datas(src_h, dst_h, &fp_sum_h, &fp_step_h, &left_pad_h, &right_pad_h);           \
    get_scaler_datas(src_w, dst_w, &fp_sum_w, &fp_step_w, &left_pad_w, &right_pad_w);           \
    fp_sum_w_init    = fp_sum_w + left_pad_w * fp_step_w;                                       \
    left_pad_w_init  = left_pad_w;                                                              \
    right_pad_w_init = right_pad_w;                                                             \
    dst_gap          = dst_pitch - 4 * dst_w;                                                   \
    middle_init      = dst_w - left_pad_w - right_pad_w;                                        \

#define BILINEAR___HEIGHT                                                                       \
    int index_h, frac_h0, frac_h1, middle;                                                      \
    const Uint32 *src_h0, *src_h1;                                                              \
    int no_padding, incr_h0, incr_h1;                                                           \
                                                                                                \
    no_padding = !(i < left_pad_h || i > dst_h - 1 - right_pad_h);                              \
    index_h    = SRC_INDEX(fp_sum_h);                                                           \
    frac_h0    = FRAC(fp_sum_h);                                                                \
                                                                                                \
    index_h = no_padding ? index_h : (i < left_pad_h ? 0 : src_h - 1);                          \
    frac_h0 = no_padding ? frac_h0 : 0;                                                         \
    incr_h1 = no_padding ? src_pitch : 0;                                                       \
    incr_h0 = index_h * src_pitch;                                                              \
                                                                                                \
    src_h0  = (const Uint32 *)((const Uint8 *)src + incr_h0);                                   \
    src_h1  = (const Uint32 *)((const Uint8 *)src_h0 + incr_h1);                                \
                                                                                                \
    fp_sum_h += fp_step_h;                                                                      \
                                                                                                \
    frac_h1  = FRAC_ONE - frac_h0;                                                              \
    fp_sum_w = fp_sum_w_init;                                                                   \
    right_pad_w = right_pad_w_init;                                                             \
    left_pad_w  = left_pad_w_init;                                                              \
    middle      = middle_init;                                                                  \



#if defined(__clang__)
// Remove inlining of this function
// Compiler crash with clang 9.0.8 / android-ndk-r21d
// Compiler crash with clang 11.0.3 / Xcode
// OK with clang 11.0.5 / android-ndk-22
// OK with clang 12.0.0 / Xcode
__attribute__((noinline))
#endif
static void
get_scaler_datas(int src_nb, int dst_nb, int *fp_start, int *fp_step, int *left_pad, int *right_pad)
{

    int step = FIXED_POINT(src_nb) / (dst_nb);  /* source step in fixed point */
    int x0 = FP_ONE / 2;     /* dst first pixel center at 0.5 in fixed point */
    int fp_sum;
    int i;
#if 0
    /* scale to source coordinates */
    x0 *= src_nb;
    x0 /= dst_nb; /* x0 == step / 2 */
#else
    /* Use this code for perfect match with pixman */
    Sint64 tmp[2];
    tmp[0] = (Sint64)step * (x0 >> 16);
    tmp[1] = (Sint64)step * (x0 & 0xFFFF);
    x0 = (int) (tmp[0] + ((tmp[1] + 0x8000) >> 16)); /*  x0 == (step + 1) / 2  */
#endif
    /* -= 0.5, get back the pixel origin, in source coordinates  */
    x0 -= FP_ONE / 2;

    *fp_start = x0;
    *fp_step = step;
    *left_pad = 0;
    *right_pad = 0;

    fp_sum = x0;
    for (i = 0; i < dst_nb; i++) {
        if (fp_sum < 0) {
            *left_pad += 1;
        } else {
            int index = SRC_INDEX(fp_sum);
            if (index > src_nb - 2) {
                *right_pad += 1;
            }
        }
        fp_sum += step;
    }
//    SDL_Log("%d -> %d  x0=%d step=%d left_pad=%d right_pad=%d", src_nb, dst_nb, *fp_start, *fp_step, *left_pad, *right_pad);
}

typedef struct color_t {
   Uint8 a;
   Uint8 b;
   Uint8 c;
   Uint8 d;
} color_t;

#if 0
static void
printf_64(const char *str, void *var)
{
    uint8_t *val = (uint8_t*) var;
    printf(" *   %s: %02x %02x %02x %02x _ %02x %02x %02x %02x\n",
           str, val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
}
#endif

/* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

static SDL_INLINE void
INTERPOL(const Uint32 *src_x0, const Uint32 *src_x1, int frac0, int frac1, Uint32 *dst)
{
    const color_t *c0 = (const color_t *)src_x0;
    const color_t *c1 = (const color_t *)src_x1;
    color_t       *cx = (color_t *)dst;
#if 0
    cx->a = c0->a + INTEGER(frac0 * (c1->a - c0->a));
    cx->b = c0->b + INTEGER(frac0 * (c1->b - c0->b));
    cx->c = c0->c + INTEGER(frac0 * (c1->c - c0->c));
    cx->d = c0->d + INTEGER(frac0 * (c1->d - c0->d));
#else
    cx->a = INTEGER(frac1 * c0->a + frac0 * c1->a);
    cx->b = INTEGER(frac1 * c0->b + frac0 * c1->b);
    cx->c = INTEGER(frac1 * c0->c + frac0 * c1->c);
    cx->d = INTEGER(frac1 * c0->d + frac0 * c1->d);
#endif
}

static SDL_INLINE void
INTERPOL_BILINEAR(const Uint32 *s0, const Uint32 *s1, int frac_w0, int frac_h0, int frac_h1, Uint32 *dst)
{
    Uint32 tmp[2];
    unsigned int frac_w1 = FRAC_ONE - frac_w0;

    /* Vertical first, store to 'tmp' */
    INTERPOL(s0,     s1,     frac_h0, frac_h1, tmp);
    INTERPOL(s0 + 1, s1 + 1, frac_h0, frac_h1, tmp + 1);

    /* Horizontal, store to 'dst' */
    INTERPOL(tmp,   tmp + 1, frac_w0, frac_w1, dst);
}

static int
scale_mat(const Uint32 *src, int src_w, int src_h, int src_pitch,
        Uint32 *dst, int dst_w, int dst_h, int dst_pitch)
{
    BILINEAR___START

    for (i = 0; i < dst_h; i++) {

        BILINEAR___HEIGHT

        while (left_pad_w--) {
            INTERPOL_BILINEAR(src_h0, src_h1, FRAC_ZERO, frac_h0, frac_h1, dst);
            dst += 1;
        }

        while (middle--) {
            const Uint32 *s_00_01;
            const Uint32 *s_10_11;
            int index_w = 4 * SRC_INDEX(fp_sum_w);
            int frac_w = FRAC(fp_sum_w);
            fp_sum_w += fp_step_w;

/*
            x00 ... x0_ ..... x01
            .       .         .
            .       x         .
            .       .         .
            .       .         .
            x10 ... x1_ ..... x11
*/
            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);

            INTERPOL_BILINEAR(s_00_01, s_10_11, frac_w, frac_h0, frac_h1, dst);

            dst += 1;
        }

        while (right_pad_w--) {
            int index_w = 4 * (src_w - 2);
            const Uint32 *s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            const Uint32 *s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);
            INTERPOL_BILINEAR(s_00_01, s_10_11, FRAC_ONE, frac_h0, frac_h1, dst);
            dst += 1;
        }
        dst = (Uint32 *)((Uint8 *)dst + dst_gap);
    }
    return 0;
}

#if defined(__SSE2__)
#  define HAVE_SSE2_INTRINSICS 1
#endif

#if defined(__ARM_NEON)
#  define HAVE_NEON_INTRINSICS 1
#endif

/* TODO: this didn't compile on Window10 universal package last time I tried .. */
#if defined(__WINRT__) || defined(_MSC_VER)
#  if defined(HAVE_NEON_INTRINSICS)
#    undef HAVE_NEON_INTRINSICS
#  endif
#endif

#if defined(HAVE_SSE2_INTRINSICS)

#if 0
static void
printf_128(const char *str, __m128i var)
{
    uint16_t *val = (uint16_t*) &var;
    printf(" *   %s: %04x %04x %04x %04x _ %04x %04x %04x %04x\n",
           str, val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
}
#endif

static SDL_INLINE int
hasSSE2()
{
    static int val = -1;
    if (val != -1) {
        return val;
    }
    val = SDL_HasSSE2();
    return val;
}

static SDL_INLINE void
INTERPOL_BILINEAR_SSE(const Uint32 *s0, const Uint32 *s1, int frac_w, __m128i v_frac_h0, __m128i v_frac_h1, Uint32 *dst, __m128i zero)
{
    __m128i x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    __m128i v_frac_w0, k0, l0, d0, e0;

    int f, f2;
    f = frac_w;
    f2 = FRAC_ONE - frac_w;
    v_frac_w0 = _mm_set_epi16(f, f2, f, f2, f, f2, f, f2);


    x_00_01 = _mm_loadl_epi64((const __m128i *)s0);  /* Load x00 and x01 */
    x_10_11 = _mm_loadl_epi64((const __m128i *)s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

    /* Interpolation vertical */
    k0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_00_01, zero), v_frac_h1);
    l0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_10_11, zero), v_frac_h0);
    k0 = _mm_add_epi16(k0, l0);

    /* For perfect match, clear the factionnal part eventually. */
    /*
    k0 = _mm_srli_epi16(k0, PRECISION);
    k0 = _mm_slli_epi16(k0, PRECISION);
    */

    /* Interpolation horizontal */
    l0 = _mm_unpacklo_epi64(/* unused */ l0, k0);
    k0 = _mm_madd_epi16(_mm_unpackhi_epi16(l0, k0), v_frac_w0);

    /* Store 1 pixel */
    d0 = _mm_srli_epi32(k0, PRECISION * 2);
    e0 = _mm_packs_epi32(d0, d0);
    e0 = _mm_packus_epi16(e0, e0);
    *dst = _mm_cvtsi128_si32(e0);
}

static int
scale_mat_SSE(const Uint32 *src, int src_w, int src_h, int src_pitch, Uint32 *dst, int dst_w, int dst_h, int dst_pitch)
{
    BILINEAR___START

    for (i = 0; i < dst_h; i++) {
        int nb_block2;
        __m128i v_frac_h0;
        __m128i v_frac_h1;
        __m128i zero;

        BILINEAR___HEIGHT

        nb_block2 = middle / 2;

        v_frac_h0 = _mm_set_epi16(frac_h0, frac_h0, frac_h0, frac_h0, frac_h0, frac_h0, frac_h0, frac_h0);
        v_frac_h1 = _mm_set_epi16(frac_h1, frac_h1, frac_h1, frac_h1, frac_h1, frac_h1, frac_h1, frac_h1);
        zero = _mm_setzero_si128();

        while (left_pad_w--) {
            INTERPOL_BILINEAR_SSE(src_h0, src_h1, FRAC_ZERO, v_frac_h0, v_frac_h1, dst, zero);
            dst += 1;
        }

        while (nb_block2--) {
            int index_w_0, frac_w_0;
            int index_w_1, frac_w_1;

            const Uint32 *s_00_01, *s_02_03, *s_10_11, *s_12_13;

            __m128i x_00_01, x_10_11, x_02_03, x_12_13;/* Pixels in 4*uint8 in row */
            __m128i v_frac_w0, k0, l0, d0, e0;
            __m128i v_frac_w1, k1, l1, d1, e1;

            int f, f2;
            index_w_0 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_0 = FRAC(fp_sum_w);
            fp_sum_w += fp_step_w;
            index_w_1 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_1 = FRAC(fp_sum_w);
            fp_sum_w += fp_step_w;
/*
            x00............ x01   x02...........x03
            .      .         .     .       .     .
            j0     f0        j1    j2      f1    j3
            .      .         .     .       .     .
            .      .         .     .       .     .
            .      .         .     .       .     .
            x10............ x11   x12...........x13
 */
            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_0);
            s_02_03 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_1);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_0);
            s_12_13 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_1);

            f = frac_w_0;
            f2 = FRAC_ONE - frac_w_0;
            v_frac_w0 = _mm_set_epi16(f, f2, f, f2, f, f2, f, f2);

            f = frac_w_1;
            f2 = FRAC_ONE - frac_w_1;
            v_frac_w1 = _mm_set_epi16(f, f2, f, f2, f, f2, f, f2);

            x_00_01 = _mm_loadl_epi64((const __m128i *)s_00_01); /* Load x00 and x01 */
            x_02_03 = _mm_loadl_epi64((const __m128i *)s_02_03);
            x_10_11 = _mm_loadl_epi64((const __m128i *)s_10_11);
            x_12_13 = _mm_loadl_epi64((const __m128i *)s_12_13);

            /* Interpolation vertical */
            k0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_00_01, zero), v_frac_h1);
            l0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_10_11, zero), v_frac_h0);
            k0 = _mm_add_epi16(k0, l0);
            k1 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_02_03, zero), v_frac_h1);
            l1 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_12_13, zero), v_frac_h0);
            k1 = _mm_add_epi16(k1, l1);

            /* Interpolation horizontal */
            l0 = _mm_unpacklo_epi64(/* unused */ l0, k0);
            k0 = _mm_madd_epi16(_mm_unpackhi_epi16(l0, k0), v_frac_w0);
            l1 = _mm_unpacklo_epi64(/* unused */ l1, k1);
            k1 = _mm_madd_epi16(_mm_unpackhi_epi16(l1, k1), v_frac_w1);

            /* Store 1 pixel */
            d0 = _mm_srli_epi32(k0, PRECISION * 2);
            e0 = _mm_packs_epi32(d0, d0);
            e0 = _mm_packus_epi16(e0, e0);
            *dst++ = _mm_cvtsi128_si32(e0);

            /* Store 1 pixel */
            d1 = _mm_srli_epi32(k1, PRECISION * 2);
            e1 = _mm_packs_epi32(d1, d1);
            e1 = _mm_packus_epi16(e1, e1);
            *dst++ = _mm_cvtsi128_si32(e1);
        }

        /* Last point */
        if (middle & 0x1) {
            const Uint32 *s_00_01;
            const Uint32 *s_10_11;
            int index_w = 4 * SRC_INDEX(fp_sum_w);
            int frac_w = FRAC(fp_sum_w);
            fp_sum_w += fp_step_w;
            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);
            INTERPOL_BILINEAR_SSE(s_00_01, s_10_11, frac_w, v_frac_h0, v_frac_h1, dst, zero);
            dst += 1;
        }

        while (right_pad_w--) {
            int index_w = 4 * (src_w - 2);
            const Uint32 *s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            const Uint32 *s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);
            INTERPOL_BILINEAR_SSE(s_00_01, s_10_11, FRAC_ONE, v_frac_h0, v_frac_h1, dst, zero);
            dst += 1;
        }
        dst = (Uint32 *)((Uint8 *)dst + dst_gap);
    }
    return 0;
}
#endif

#if defined(HAVE_NEON_INTRINSICS)

static SDL_INLINE int
hasNEON()
{
    static int val = -1;
    if (val != -1) {
        return val;
    }
    val = SDL_HasNEON();
    return val;
}

static SDL_INLINE void
INTERPOL_BILINEAR_NEON(const Uint32 *s0, const Uint32 *s1, int frac_w, uint8x8_t v_frac_h0, uint8x8_t v_frac_h1, Uint32 *dst)
{
    uint8x8_t x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    uint16x8_t k0;
    uint32x4_t l0;
    uint16x8_t d0;
    uint8x8_t e0;

    x_00_01 = (uint8x8_t)vld1_u32(s0); /* Load 2 pixels */
    x_10_11 = (uint8x8_t)vld1_u32(s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
    k0 = vmull_u8(x_00_01, v_frac_h1);                          /* k0 := x0 * (1 - frac)    */
    k0 = vmlal_u8(k0, x_10_11, v_frac_h0);                      /* k0 += x1 * frac          */

    /* k0 now contains 2 interpolated pixels { j0, j1 } */
    l0 = vshll_n_u16(vget_low_u16(k0), PRECISION);
    l0 = vmlsl_n_u16(l0, vget_low_u16(k0), frac_w);
    l0 = vmlal_n_u16(l0, vget_high_u16(k0), frac_w);

    /* Shift and narrow */
    d0 = vcombine_u16(
            /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION),
            /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION)
            );

    /* Narrow again */
    e0 = vmovn_u16(d0);

    /* Store 1 pixel */
    *dst = vget_lane_u32((uint32x2_t)e0, 0);
}

    static int
scale_mat_NEON(const Uint32 *src, int src_w, int src_h, int src_pitch, Uint32 *dst, int dst_w, int dst_h, int dst_pitch)
{
    BILINEAR___START

    for (i = 0; i < dst_h; i++) {
        int nb_block4;
        uint8x8_t v_frac_h0, v_frac_h1;

        BILINEAR___HEIGHT

        nb_block4 = middle / 4;

        v_frac_h0 = vmov_n_u8(frac_h0);
        v_frac_h1 = vmov_n_u8(frac_h1);

        while (left_pad_w--) {
            INTERPOL_BILINEAR_NEON(src_h0, src_h1, FRAC_ZERO, v_frac_h0, v_frac_h1, dst);
            dst += 1;
        }

        while (nb_block4--) {
            int index_w_0, frac_w_0;
            int index_w_1, frac_w_1;
            int index_w_2, frac_w_2;
            int index_w_3, frac_w_3;

            const Uint32 *s_00_01, *s_02_03, *s_04_05, *s_06_07;
            const Uint32 *s_10_11, *s_12_13, *s_14_15, *s_16_17;

            uint8x8_t x_00_01, x_10_11, x_02_03, x_12_13;/* Pixels in 4*uint8 in row */
            uint8x8_t x_04_05, x_14_15, x_06_07, x_16_17;

            uint16x8_t k0, k1, k2, k3;
            uint32x4_t l0, l1, l2, l3;
            uint16x8_t d0, d1;
            uint8x8_t e0, e1;
            uint32x4_t f0;

            index_w_0 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_0  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;
            index_w_1 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_1  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;
            index_w_2 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_2  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;
            index_w_3 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_3  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;

            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_0);
            s_02_03 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_1);
            s_04_05 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_2);
            s_06_07 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_3);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_0);
            s_12_13 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_1);
            s_14_15 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_2);
            s_16_17 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_3);

            /* Interpolation vertical */
            x_00_01 = (uint8x8_t)vld1_u32(s_00_01); /* Load 2 pixels */
            x_02_03 = (uint8x8_t)vld1_u32(s_02_03);
            x_04_05 = (uint8x8_t)vld1_u32(s_04_05);
            x_06_07 = (uint8x8_t)vld1_u32(s_06_07);
            x_10_11 = (uint8x8_t)vld1_u32(s_10_11);
            x_12_13 = (uint8x8_t)vld1_u32(s_12_13);
            x_14_15 = (uint8x8_t)vld1_u32(s_14_15);
            x_16_17 = (uint8x8_t)vld1_u32(s_16_17);

            /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
            k0 = vmull_u8(x_00_01, v_frac_h1);                          /* k0 := x0 * (1 - frac)    */
            k0 = vmlal_u8(k0, x_10_11, v_frac_h0);                      /* k0 += x1 * frac          */

            k1 = vmull_u8(x_02_03, v_frac_h1);
            k1 = vmlal_u8(k1, x_12_13, v_frac_h0);

            k2 = vmull_u8(x_04_05, v_frac_h1);
            k2 = vmlal_u8(k2, x_14_15, v_frac_h0);

            k3 = vmull_u8(x_06_07, v_frac_h1);
            k3 = vmlal_u8(k3, x_16_17, v_frac_h0);

            /* k0 now contains 2 interpolated pixels { j0, j1 } */
            /* k1 now contains 2 interpolated pixels { j2, j3 } */
            /* k2 now contains 2 interpolated pixels { j4, j5 } */
            /* k3 now contains 2 interpolated pixels { j6, j7 } */

            l0 = vshll_n_u16(vget_low_u16(k0), PRECISION);
            l0 = vmlsl_n_u16(l0, vget_low_u16(k0), frac_w_0);
            l0 = vmlal_n_u16(l0, vget_high_u16(k0), frac_w_0);

            l1 = vshll_n_u16(vget_low_u16(k1), PRECISION);
            l1 = vmlsl_n_u16(l1, vget_low_u16(k1), frac_w_1);
            l1 = vmlal_n_u16(l1, vget_high_u16(k1), frac_w_1);

            l2 = vshll_n_u16(vget_low_u16(k2), PRECISION);
            l2 = vmlsl_n_u16(l2, vget_low_u16(k2), frac_w_2);
            l2 = vmlal_n_u16(l2, vget_high_u16(k2), frac_w_2);

            l3 = vshll_n_u16(vget_low_u16(k3), PRECISION);
            l3 = vmlsl_n_u16(l3, vget_low_u16(k3), frac_w_3);
            l3 = vmlal_n_u16(l3, vget_high_u16(k3), frac_w_3);

            /* shift and narrow */
            d0 = vcombine_u16(
                    /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION),
                    /* uint16x4_t */ vshrn_n_u32(l1, 2 * PRECISION)
            );
            /* narrow again */
            e0 = vmovn_u16(d0);

            /* Shift and narrow */
            d1 = vcombine_u16(
                    /* uint16x4_t */ vshrn_n_u32(l2, 2 * PRECISION),
                    /* uint16x4_t */ vshrn_n_u32(l3, 2 * PRECISION)
            );
            /* Narrow again */
            e1 = vmovn_u16(d1);

            f0 = vcombine_u32((uint32x2_t)e0, (uint32x2_t)e1);
            /* Store 4 pixels */
            vst1q_u32(dst, f0);

            dst += 4;
        }

        if (middle & 0x2) {
            int index_w_0, frac_w_0;
            int index_w_1, frac_w_1;
            const Uint32 *s_00_01, *s_02_03;
            const Uint32 *s_10_11, *s_12_13;
            uint8x8_t x_00_01, x_10_11, x_02_03, x_12_13;/* Pixels in 4*uint8 in row */
            uint16x8_t k0, k1;
            uint32x4_t l0, l1;
            uint16x8_t d0;
            uint8x8_t e0;

            index_w_0 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_0  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;
            index_w_1 = 4 * SRC_INDEX(fp_sum_w);
            frac_w_1  = FRAC(fp_sum_w);
            fp_sum_w  += fp_step_w;
/*
            x00............ x01   x02...........x03
            .      .         .     .       .     .
            j0   dest0       j1    j2    dest1   j3
            .      .         .     .       .     .
            .      .         .     .       .     .
            .      .         .     .       .     .
            x10............ x11   x12...........x13
*/
            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_0);
            s_02_03 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_1);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_0);
            s_12_13 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_1);

            /* Interpolation vertical */
            x_00_01 = (uint8x8_t)vld1_u32(s_00_01);/* Load 2 pixels */
            x_02_03 = (uint8x8_t)vld1_u32(s_02_03);
            x_10_11 = (uint8x8_t)vld1_u32(s_10_11);
            x_12_13 = (uint8x8_t)vld1_u32(s_12_13);

            /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
            k0 = vmull_u8(x_00_01, v_frac_h1);                          /* k0 := x0 * (1 - frac)    */
            k0 = vmlal_u8(k0, x_10_11, v_frac_h0);                      /* k0 += x1 * frac          */

            k1 = vmull_u8(x_02_03, v_frac_h1);
            k1 = vmlal_u8(k1, x_12_13, v_frac_h0);

            /* k0 now contains 2 interpolated pixels { j0, j1 } */
            /* k1 now contains 2 interpolated pixels { j2, j3 } */

            l0 = vshll_n_u16(vget_low_u16(k0), PRECISION);
            l0 = vmlsl_n_u16(l0, vget_low_u16(k0), frac_w_0);
            l0 = vmlal_n_u16(l0, vget_high_u16(k0), frac_w_0);

            l1 = vshll_n_u16(vget_low_u16(k1), PRECISION);
            l1 = vmlsl_n_u16(l1, vget_low_u16(k1), frac_w_1);
            l1 = vmlal_n_u16(l1, vget_high_u16(k1), frac_w_1);

            /* Shift and narrow */

            d0 = vcombine_u16(
                    /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION),
                    /* uint16x4_t */ vshrn_n_u32(l1, 2 * PRECISION)
            );

            /* Narrow again */
            e0 = vmovn_u16(d0);

            /* Store 2 pixels */
            vst1_u32(dst, (uint32x2_t)e0);
            dst += 2;
        }

        /* Last point */
        if (middle & 0x1) {
            int index_w = 4 * SRC_INDEX(fp_sum_w);
            int frac_w = FRAC(fp_sum_w);
            const Uint32 *s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            const Uint32 *s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);
            INTERPOL_BILINEAR_NEON(s_00_01, s_10_11, frac_w, v_frac_h0, v_frac_h1, dst);
            dst += 1;
        }

        while (right_pad_w--) {
            int index_w = 4 * (src_w - 2);
            const Uint32 *s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
            const Uint32 *s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);
            INTERPOL_BILINEAR_NEON(s_00_01, s_10_11, FRAC_ONE, v_frac_h0, v_frac_h1, dst);
            dst += 1;
        }

        dst = (Uint32 *)((Uint8 *)dst + dst_gap);
    }
    return 0;
}
#endif

int
SDL_LowerSoftStretchLinear(SDL_Surface *s, const SDL_Rect *srcrect,
                SDL_Surface *d, const SDL_Rect *dstrect)
{
    int ret = -1;
    int src_w = srcrect->w;
    int src_h = srcrect->h;
    int dst_w = dstrect->w;
    int dst_h = dstrect->h;
    int src_pitch = s->pitch;
    int dst_pitch = d->pitch;
    Uint32 *src = (Uint32 *) ((Uint8 *)s->pixels + srcrect->x * 4 + srcrect->y * src_pitch);
    Uint32 *dst = (Uint32 *) ((Uint8 *)d->pixels + dstrect->x * 4 + dstrect->y * dst_pitch);

#if defined(HAVE_NEON_INTRINSICS)
    if (ret == -1 && hasNEON()) {
        ret = scale_mat_NEON(src, src_w, src_h, src_pitch, dst, dst_w, dst_h, dst_pitch);
    }
#endif

#if defined(HAVE_SSE2_INTRINSICS)
    if (ret == -1 && hasSSE2()) {
        ret = scale_mat_SSE(src, src_w, src_h, src_pitch, dst, dst_w, dst_h, dst_pitch);
    }
#endif

    if (ret == -1) {
        scale_mat(src, src_w, src_h, src_pitch, dst, dst_w, dst_h, dst_pitch);
    }

    return ret;
}

/* vi: set ts=4 sw=4 expandtab: */
