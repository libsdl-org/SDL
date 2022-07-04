/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_PS2 && !SDL_RENDER_DISABLED

#include "SDL_surface.h"
#include "SDL_triangle.h"

#include "../../video/SDL_blit.h"

/* fixed points bits precision
 * Set to 1, so that it can start rendering wth middle of a pixel precision.
 * It doesn't need to be increased.
 * But, if increased too much, it overflows (srcx, srcy) coordinates used for filling with texture.
 * (which could be turned to int64).
 */
#define FP_BITS   1


void PS2_trianglepoint_2_fixedpoint(SDL_Point *a) {
    a->x <<= FP_BITS;
    a->y <<= FP_BITS;
}

/* bounding rect of three points (in fixed point) */
static void bounding_rect_fixedpoint(const SDL_Point *a, const SDL_Point *b, const SDL_Point *c, SDL_Rect *r)
{
    int min_x = SDL_min(a->x, SDL_min(b->x, c->x));
    int max_x = SDL_max(a->x, SDL_max(b->x, c->x));
    int min_y = SDL_min(a->y, SDL_min(b->y, c->y));
    int max_y = SDL_max(a->y, SDL_max(b->y, c->y));
    /* points are in fixed point, shift back */
    r->x = min_x >> FP_BITS;
    r->y = min_y >> FP_BITS;
    r->w = (max_x - min_x) >> FP_BITS;
    r->h = (max_y - min_y) >> FP_BITS;
}

/* bounding rect of three points */
static void bounding_rect(const SDL_Point *a, const SDL_Point *b, const SDL_Point *c, SDL_Rect *r)
{
    int min_x = SDL_min(a->x, SDL_min(b->x, c->x));
    int max_x = SDL_max(a->x, SDL_max(b->x, c->x));
    int min_y = SDL_min(a->y, SDL_min(b->y, c->y));
    int max_y = SDL_max(a->y, SDL_max(b->y, c->y));
    r->x = min_x;
    r->y = min_y;
    r->w = (max_x - min_x);
    r->h = (max_y - min_y);
}


/* Triangle rendering, using Barycentric coordinates (w0, w1, w2)
 *
 * The cross product isn't computed from scratch at each iteration,
 * but optimized using constant step increments
 *
 */

#define TRIANGLE_BEGIN_LOOP                                                                             \
    {                                                                                                   \
        int x, y;                                                                                       \
        for (y = 0; y < dstrect.h; y++) {                                                               \
            /* y start */                                                                               \
            int w0 = w0_row;                                                                            \
            int w1 = w1_row;                                                                            \
            int w2 = w2_row;                                                                            \
            for (x = 0; x < dstrect.w; x++) {                                                           \
                /* In triangle */                                                                       \
                if (w0 + bias_w0 >= 0 && w1 + bias_w1 >= 0 && w2 + bias_w2 >= 0) {                      \
                    Uint8 *dptr = (Uint8 *) dst_ptr + x * dstbpp;                                       \


/* Use 64 bits precision to prevent overflow when interpolating color / texture with wide triangles */
#define TRIANGLE_GET_TEXTCOORD                                                                          \
                    int srcx = (int)(((Sint64)w0 * s2s0_x + (Sint64)w1 * s2s1_x + s2_x_area.x) / area); \
                    int srcy = (int)(((Sint64)w0 * s2s0_y + (Sint64)w1 * s2s1_y + s2_x_area.y) / area); \

#define TRIANGLE_GET_MAPPED_COLOR                                                                       \
                    int r = (int)(((Sint64)w0 * c0.r + (Sint64)w1 * c1.r + (Sint64)w2 * c2.r) / area);  \
                    int g = (int)(((Sint64)w0 * c0.g + (Sint64)w1 * c1.g + (Sint64)w2 * c2.g) / area);  \
                    int b = (int)(((Sint64)w0 * c0.b + (Sint64)w1 * c1.b + (Sint64)w2 * c2.b) / area);  \
                    int a = (int)(((Sint64)w0 * c0.a + (Sint64)w1 * c1.a + (Sint64)w2 * c2.a) / area);  \
                    int color = SDL_MapRGBA(format, r, g, b, a);                                        \

#define TRIANGLE_GET_COLOR                                                                              \
                    int r = (int)(((Sint64)w0 * c0.r + (Sint64)w1 * c1.r + (Sint64)w2 * c2.r) / area);  \
                    int g = (int)(((Sint64)w0 * c0.g + (Sint64)w1 * c1.g + (Sint64)w2 * c2.g) / area);  \
                    int b = (int)(((Sint64)w0 * c0.b + (Sint64)w1 * c1.b + (Sint64)w2 * c2.b) / area);  \
                    int a = (int)(((Sint64)w0 * c0.a + (Sint64)w1 * c1.a + (Sint64)w2 * c2.a) / area);  \


#define TRIANGLE_END_LOOP                                                                               \
                }                                                                                       \
                /* x += 1 */                                                                            \
                w0 += d2d1_y;                                                                           \
                w1 += d0d2_y;                                                                           \
                w2 += d1d0_y;                                                                           \
            }                                                                                           \
            /* y += 1 */                                                                                \
            w0_row += d1d2_x;                                                                           \
            w1_row += d2d0_x;                                                                           \
            w2_row += d0d1_x;                                                                           \
            dst_ptr += dst_pitch;                                                                       \
        }                                                                                               \
    }                                                                                                   \

#endif /* SDL_VIDEO_RENDER_PS2 && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
