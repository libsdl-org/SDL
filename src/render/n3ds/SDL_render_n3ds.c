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
#include "3ds/gfx.h"
#include "3ds/services/gspgpu.h"
#include "SDL_error.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"

#if SDL_VIDEO_RENDER_N3DS

#include "SDL_hints.h"
#include "../SDL_sysrender.h"

#include <stdbool.h>
#include <3ds.h>
#include <citro3d.h>

#include "SDL_render_n3ds_shaders.h"

/* N3DS renderer implementation, derived from the PSP implementation  */

static int
PixelFormatToN3DSGPU(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_RGBA8888:
        return GPU_RGBA8;
    case SDL_PIXELFORMAT_RGB888:
        return GPU_RGB8;
    case SDL_PIXELFORMAT_RGBA5551:
        return GPU_RGBA5551;
    case SDL_PIXELFORMAT_RGB565:
        return GPU_RGB565;
    case SDL_PIXELFORMAT_RGBA4444:
        return GPU_RGBA4;
    default:
        return GPU_RGBA8;
    }
}

#define COL5650(r,g,b,a)    ((b>>3) | ((g>>2)<<5) | ((r>>3)<<11))
#define COL5551(r,g,b,a)    (((b>>3)<<1) | ((g>>3)<<6) | ((r>>3)<<11) | (a>0?1:0))
#define COL4444(r,g,b,a)    ((a>>4) | ((b>>4)<<4) | ((g>>4)<<8) | ((r>>4)<<12))
#define COL8888(r,g,b,a)    ((a) | ((b)<<8) | ((g)<<16) | ((r)<<24))
#define COL888(r,g,b)       ((b) | ((g)<<16) | ((r)<<24))

typedef struct
{
    C3D_Tex             texture;
    C3D_RenderTarget*   renderTarget;
    C3D_Mtx             renderProjMtx;
    unsigned int        width;                              /**< Image width. */
    unsigned int        height;                             /**< Image height. */
    unsigned int        pitch;
    unsigned int        size;
    /**
     * The 3DS GPU requires all textures to be *swizzled* before use.
     *
     * For textures considered STREAMING, we keep an unswizzled buffer in memory
     * at all times. For textures considered STATIC or TARGET, we generate an
     * unswizzled memory buffer on demand - this saves memory usage, but slows
     * down updates.
     *
     * To save on memory usage, we align the unswizzled buffer's width/height
     * to a multiple of 8, as opposed to the next power of two. The 3DS GPU can
     * deal with that.
     */
    void*               unswizzledBuffer;
    unsigned int        unswizzledWidth;
    unsigned int        unswizzledHeight;
    unsigned int        unswizzledPitch;
    unsigned int        unswizzledSize;
} N3DS_TextureData;

typedef struct
{
    SDL_BlendMode mode;
    SDL_Texture* texture;
} N3DS_BlendState;

typedef struct
{
    C3D_RenderTarget*   renderTarget;
    C3D_Mtx             renderProjMtx;
    SDL_Texture*        boundTarget;                         /**< currently bound rendertarget */
    SDL_bool            initialized;                         /**< is driver initialized */
    unsigned int        psm;                                 /**< format of the display buffers */
    unsigned int        bpp;                                 /**< bits per pixel of the main display */

    SDL_bool            vsync;                               /**< wether we do vsync */
    N3DS_BlendState     blendState;                          /**< current blend mode */

    C3D_TexEnv          envTex;
    C3D_TexEnv          envNoTex;

    DVLB_s *dvlb;
    shaderProgram_s shaderProgram;
    int projMtxShaderLoc;
} N3DS_RenderData;

typedef struct
{
    float     x, y;
    SDL_Color col;
    float     u, v;
} VertVCT;

#define PI   3.14159265358979f

#define radToDeg(x) ((x)*180.f/PI)
#define degToRad(x) ((x)*PI/180.f)

static inline void
Swap(float *a, float *b)
{
    float n=*a;
    *a = *b;
    *b = n;
}

static inline int
InVram(void* data)
{
    return data < (void*)0x20000000;
}

static int
TextureNextPow2(unsigned int w)
{
    if (w < 8)
        return 8;

    w -= 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    return w + 1;
}

static int
TextureAlign8(unsigned int w) {
    return (w + 7) & (~7);
}

static void
TextureActivate(SDL_Texture *texture)
{
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;
    C3D_TexBind(0, &N3DS_texture->texture);
}

static void
N3DS_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
}

static int
N3DS_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_TextureData* N3DS_texture = (N3DS_TextureData*) SDL_calloc(1, sizeof(*N3DS_texture));
    bool initialized = SDL_FALSE;

    if(!N3DS_texture)
        return SDL_OutOfMemory();

    N3DS_texture->width = texture->w;
    N3DS_texture->height = texture->h;

    initialized = C3D_TexInitVRAM(&N3DS_texture->texture,
        TextureNextPow2(texture->w),
        TextureNextPow2(texture->h),
        PixelFormatToN3DSGPU(texture->format));

    if (!initialized) {
        initialized = C3D_TexInit(&N3DS_texture->texture,
            TextureNextPow2(texture->w),
            TextureNextPow2(texture->h),
            PixelFormatToN3DSGPU(texture->format));
    }

    if (!initialized) {
        SDL_free(N3DS_texture);
        return SDL_OutOfMemory();
    }

    N3DS_texture->pitch = N3DS_texture->texture.width * SDL_BYTESPERPIXEL(texture->format);
    N3DS_texture->size = N3DS_texture->texture.height * N3DS_texture->pitch;

    N3DS_texture->unswizzledWidth = TextureAlign8(texture->w);
    N3DS_texture->unswizzledHeight = TextureAlign8(texture->h);
    N3DS_texture->unswizzledPitch = N3DS_texture->unswizzledWidth * SDL_BYTESPERPIXEL(texture->format);
    N3DS_texture->unswizzledSize = N3DS_texture->unswizzledHeight * N3DS_texture->unswizzledPitch;

    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
        N3DS_texture->renderTarget = C3D_RenderTargetCreateFromTex(&N3DS_texture->texture, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);
        if (N3DS_texture->renderTarget == NULL) {
            SDL_free(N3DS_texture);
            return SDL_OutOfMemory();
        }
        Mtx_Ortho(&N3DS_texture->renderProjMtx, 0.0, N3DS_texture->texture.width, 0.0, N3DS_texture->texture.height, -1.0, 1.0, true);
    } else if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        N3DS_texture->unswizzledBuffer = linearAlloc(N3DS_texture->unswizzledSize);
    }

    texture->driverdata = N3DS_texture;

    return 0;
}

static int
N3DS_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, void **pixels, int *pitch);

static void
N3DS_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);

static int
N3DS_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                   const SDL_Rect * rect, const void *pixels, int pitch)
{
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;

    const Uint8 *src;
    Uint8 *dst;
    int row, length,dpitch;
    src = pixels;

    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        N3DS_texture->unswizzledBuffer = linearAlloc(N3DS_texture->unswizzledSize);
        if (N3DS_texture->unswizzledBuffer == NULL) {
            return SDL_OutOfMemory();
        }
    }

    N3DS_LockTexture(renderer, texture, rect,(void **)&dst, &dpitch);
    length = rect->w * SDL_BYTESPERPIXEL(texture->format);
    if (length == pitch && length == dpitch) {
        SDL_memcpy(dst, src, length*rect->h);
    } else {
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += dpitch;
        }
    }
    N3DS_UnlockTexture(renderer, texture);

    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        linearFree(N3DS_texture->unswizzledBuffer);
    }

    return 0;
}

static int
N3DS_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, void **pixels, int *pitch)
{
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) N3DS_texture->unswizzledBuffer + rect->y * N3DS_texture->unswizzledPitch +
                  rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = N3DS_texture->unswizzledPitch;
    
    return 0;
}

static void
N3DS_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;

    /* We do whole texture updates, at least for now */
    GSPGPU_FlushDataCache(N3DS_texture->unswizzledBuffer, N3DS_texture->unswizzledSize);
    C3D_SyncDisplayTransfer(
        N3DS_texture->unswizzledBuffer,
        GX_BUFFER_DIM(N3DS_texture->unswizzledWidth, N3DS_texture->unswizzledHeight),
        N3DS_texture->texture.data,
        GX_BUFFER_DIM(N3DS_texture->texture.width, N3DS_texture->texture.height),
        GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(N3DS_texture->texture.fmt) | GX_TRANSFER_OUT_FORMAT(N3DS_texture->texture.fmt) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
    );
}

static void
N3DS_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;
    GPU_TEXTURE_FILTER_PARAM filter = (scaleMode == SDL_ScaleModeNearest) ? GPU_NEAREST : GPU_LINEAR;

    C3D_TexSetFilter(&N3DS_texture->texture, filter, filter);
}

static int
N3DS_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_RenderData *data = renderer->driverdata;
    data->boundTarget = texture;

    if (texture == NULL) {
        if (!C3D_FrameDrawOn(data->renderTarget)) {
            return SDL_Unsupported();
        }
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, data->projMtxShaderLoc, &data->renderProjMtx);
    } else {
        N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;
        if (N3DS_texture->renderTarget != NULL) {
            if (!C3D_FrameDrawOn(N3DS_texture->renderTarget)) {
                return SDL_Unsupported();
            }
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, data->projMtxShaderLoc, &N3DS_texture->renderProjMtx);
        } else {
            return SDL_Unsupported();
        }
    }

    return 0;
}

static int
N3DS_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;  /* nothing to do in this backend. */
}

static int
N3DS_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
        const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride,
        int num_vertices, const void *indices, int num_indices, int size_indices,
        float scale_x, float scale_y)
{
    int i;
    int count = indices ? num_indices : num_vertices;
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;
    VertVCT *verts;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    verts = (VertVCT *) SDL_AllocateRenderVertices(renderer, count * sizeof (VertVCT), 0, &cmd->data.draw.first);
    if (!verts) {
        return -1;
    }

    if (texture == NULL) {
        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            SDL_Color col_;
            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else if (size_indices == 1) {
                j = ((const Uint8 *)indices)[i];
            } else {
                j = i;
            }

            xy_ = (float *)((char*)xy + j * xy_stride);
            col_ = *(SDL_Color *)((char*)color + j * color_stride);

            verts->x = xy_[0] * scale_x;
            verts->y = xy_[1] * scale_y;

            verts->col = col_;

            verts++;
        }
    } else {
        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            SDL_Color col_;
            float *uv_;

            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else if (size_indices == 1) {
                j = ((const Uint8 *)indices)[i];
            } else {
                j = i;
            }

            xy_ = (float *)((char*)xy + j * xy_stride);
            col_ = *(SDL_Color *)((char*)color + j * color_stride);
            uv_ = (float *)((char*)uv + j * uv_stride);

            verts->x = xy_[0] * scale_x;
            verts->y = xy_[1] * scale_y;

            verts->col = col_;

            verts->u = uv_[0] * N3DS_texture->texture.width;
            verts->v = uv_[1] * N3DS_texture->texture.height;

            verts++;
        }
    }

    return 0;
}

static int
N3DS_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    SDL_Color color = *((SDL_Color*) &(cmd->data.draw.r));
    VertVCT *verts = (VertVCT *) SDL_AllocateRenderVertices(renderer, count * 2 * sizeof (VertVCT), 4, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count * 4;

    for (i = 0; i < count; i++, rects++) {
        verts->x = rects->x;
        verts->y = rects->y;
        verts->col = color;
        verts++;

        verts->x = rects->x + rects->w;
        verts->y = rects->y;
        verts->col = color;
        verts++;

        verts->x = rects->x;
        verts->y = rects->y + rects->h;
        verts->col = color;
        verts++;

        verts->x = rects->x + rects->w;
        verts->y = rects->y + rects->h;
        verts->col = color;
        verts++;
    }

    return 0;
}

static int
N3DS_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
             const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    SDL_Color color = *((SDL_Color*) &(cmd->data.draw.r));
    VertVCT *verts;
    const float x = dstrect->x;
    const float y = dstrect->y;
    const float width = dstrect->w;
    const float height = dstrect->h;

    const float u0 = srcrect->x;
    const float v0 = srcrect->y;
    const float u1 = srcrect->x + srcrect->w;
    const float v1 = srcrect->y + srcrect->h;

    verts = (VertVCT *) SDL_AllocateRenderVertices(renderer, 2 * sizeof (VertVCT), 4, &cmd->data.draw.first);
    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = 4;

    verts->u = u0;
    verts->v = v0;
    verts->x = x;
    verts->y = y;
    verts->col = color;
    verts++;

    verts->u = u1;
    verts->v = v0;
    verts->x = x + width;
    verts->y = y;
    verts->col = color;
    verts++;

    verts->u = u0;
    verts->v = v1;
    verts->x = x;
    verts->y = y + 1;
    verts->col = color;
    verts++;

    verts->u = u1;
    verts->v = v1;
    verts->x = x + width;
    verts->y = y + height;
    verts->col = color;
    verts++;

    return 0;
}

static int
N3DS_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
               const SDL_Rect * srcrect, const SDL_FRect * dstrect,
               const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip, float scale_x, float scale_y)
{
    SDL_Color color = *((SDL_Color*) &(cmd->data.draw.r));
    VertVCT *verts = (VertVCT *) SDL_AllocateRenderVertices(renderer, 4 * sizeof (VertVCT), 4, &cmd->data.draw.first);
    const float centerx = center->x;
    const float centery = center->y;
    const float x = dstrect->x + centerx;
    const float y = dstrect->y + centery;
    const float width = dstrect->w - centerx;
    const float height = dstrect->h - centery;
    float s, c;
    float cw1, sw1, ch1, sh1, cw2, sw2, ch2, sh2;

    float u0 = srcrect->x;
    float v0 = srcrect->y;
    float u1 = srcrect->x + srcrect->w;
    float v1 = srcrect->y + srcrect->h;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = 4;

    s = sinf(degToRad(360-angle));
    c = cosf(degToRad(360-angle));

    cw1 = c * -centerx;
    sw1 = s * -centerx;
    ch1 = c * -centery;
    sh1 = s * -centery;
    cw2 = c * width;
    sw2 = s * width;
    ch2 = c * height;
    sh2 = s * height;

    if (flip & SDL_FLIP_VERTICAL) {
        Swap(&v0, &v1);
    }

    if (flip & SDL_FLIP_HORIZONTAL) {
        Swap(&u0, &u1);
    }

    verts->u = u0;
    verts->v = v0;
    verts->x = x + cw1 + sh1;
    verts->y = y - sw1 + ch1;
    verts->col = color;
    verts++;

    verts->u = u0;
    verts->v = v1;
    verts->x = x + cw1 + sh2;
    verts->y = y - sw1 + ch2;
    verts->col = color;
    verts++;

    verts->u = u1;
    verts->v = v0;
    verts->x = x + cw2 + sh1;
    verts->y = y - sw2 + ch1;
    verts->col = color;
    verts++;

    verts->u = u1;
    verts->v = v1;
    verts->x = x + cw2 + sh2;
    verts->y = y - sw2 + ch2;
    verts->col = color;

    if (scale_x != 1.0f || scale_y != 1.0f) {
        verts->x *= scale_x;
        verts->y *= scale_y;
        verts--;
        verts->x *= scale_x;
        verts->y *= scale_y;
        verts--;
        verts->x *= scale_x;
        verts->y *= scale_y;
        verts--;
        verts->x *= scale_x;
        verts->y *= scale_y;
    }

    return 0;
}

static void
ResetBlendState(N3DS_RenderData *data, N3DS_BlendState* state) {
    state->mode = SDL_BLENDMODE_INVALID;
    state->texture = NULL;
    C3D_SetTexEnv(0, &data->envNoTex);
}

static void
N3DS_SetBlendState(N3DS_RenderData* data, N3DS_BlendState* state)
{
    N3DS_BlendState* current = &data->blendState;

    if (state->mode != current->mode) {
        switch (state->mode) {
        case SDL_BLENDMODE_NONE:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            break;
        case SDL_BLENDMODE_BLEND:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
            break;
        case SDL_BLENDMODE_ADD:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_ZERO, GPU_ONE);
            break;
        case SDL_BLENDMODE_MOD:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ZERO, GPU_ZERO, GPU_ONE);
            break;
        case SDL_BLENDMODE_MUL:
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_DST_COLOR, GPU_ONE_MINUS_SRC_ALPHA, GPU_DST_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
            break;
        case SDL_BLENDMODE_INVALID:
            break;
        }
    }

    if(state->texture != current->texture) {
        if(state->texture != NULL) {
            TextureActivate(state->texture);
            C3D_SetTexEnv(0, &data->envTex);
        } else {
            C3D_SetTexEnv(0,&data->envNoTex);
        }
    }

    *current = *state;
}

static int
N3DS_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;

    C3D_BufInfo *bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, vertices, sizeof(VertVCT), 3, 0x210);

    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETVIEWPORT: {
                SDL_Rect *viewport = &cmd->data.viewport.rect;
                C3D_SetViewport(viewport->x, viewport->y, viewport->w, viewport->h);
                C3D_SetScissor(GPU_SCISSOR_NORMAL, viewport->x, viewport->y, viewport->x + viewport->w, viewport->y + viewport->h);
                break;
            }

            case SDL_RENDERCMD_SETDRAWCOLOR: {
                break;
            }

            case SDL_RENDERCMD_DRAW_POINTS: {
                /* Output as geometry */
                break;
            }

            case SDL_RENDERCMD_DRAW_LINES: {
                /* Output as geometry */
                break;
            }

            case SDL_RENDERCMD_SETCLIPRECT: {
                const SDL_Rect *rect = &cmd->data.cliprect.rect;
                if(cmd->data.cliprect.enabled){
                    C3D_SetScissor(GPU_SCISSOR_NORMAL, rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
                } else {
                    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
                }
                break;
            }

            case SDL_RENDERCMD_CLEAR: {
                const Uint8 r = cmd->data.color.r;
                const Uint8 g = cmd->data.color.g;
                const Uint8 b = cmd->data.color.b;
                const Uint8 a = cmd->data.color.a;
                C3D_FrameBufClear(
                    C3D_GetFrameBuf(),
                    C3D_CLEAR_ALL,
                    COL8888(r, g, b, a),
                    0
                );
                break;
            }

            case SDL_RENDERCMD_FILL_RECTS: {
                const size_t first = cmd->data.draw.first;
                const size_t count = cmd->data.draw.count;
                N3DS_BlendState state = {
                    .texture = NULL,
                    .mode = cmd->data.draw.blend
                };
                N3DS_SetBlendState(data, &state);
                C3D_DrawArrays(GPU_TRIANGLE_STRIP, first, count);
                break;
            }

            case SDL_RENDERCMD_COPY:
            case SDL_RENDERCMD_COPY_EX: {
                const size_t first = cmd->data.draw.first;
                const size_t count = cmd->data.draw.count;
                N3DS_BlendState state = {
                    .texture = cmd->data.draw.texture,
                    .mode = cmd->data.draw.blend
                };
                N3DS_SetBlendState(data, &state);
                C3D_DrawArrays(GPU_TRIANGLE_STRIP, first, count);
                break;
            }

            case SDL_RENDERCMD_GEOMETRY: {
                const size_t first = cmd->data.draw.first;
                const size_t count = cmd->data.draw.count;
                N3DS_BlendState state = {
                    .texture = cmd->data.draw.texture,
                    .mode = cmd->data.draw.blend
                };
                N3DS_SetBlendState(data, &state);
                C3D_DrawArrays(GPU_TRIANGLES, first, count);
                break;
            }

            case SDL_RENDERCMD_NO_OP:
                break;
        }

        cmd = cmd->next;
    }

    return 0;
}

static int
N3DS_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 pixel_format, void * pixels, int pitch)
{
    return SDL_Unsupported();
}

static int
N3DS_RenderPresent(SDL_Renderer * renderer)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    C3D_FrameEnd(0);

    C3D_FrameBegin(data->vsync ? C3D_FRAME_SYNCDRAW : 0);
    N3DS_SetRenderTarget(renderer, data->boundTarget);

    return 0;
}

static void
N3DS_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    N3DS_RenderData *renderdata = (N3DS_RenderData *) renderer->driverdata;
    N3DS_TextureData *N3DS_texture = (N3DS_TextureData *) texture->driverdata;

    if (renderdata == 0)
        return;

    if (N3DS_texture == 0)
        return;

    if (N3DS_texture->renderTarget != NULL) {
        C3D_RenderTargetDelete(N3DS_texture->renderTarget);
    }

    if (N3DS_texture->unswizzledBuffer != NULL) {
        linearFree(N3DS_texture->unswizzledBuffer);
    }

    C3D_TexDelete(&N3DS_texture->texture);
    SDL_free(N3DS_texture);
    texture->driverdata = NULL;
}

static void
N3DS_DestroyRenderer(SDL_Renderer * renderer)
{
    N3DS_RenderData *data = (N3DS_RenderData *) renderer->driverdata;
    if (data) {
        if (!data->initialized)
            return;

        C3D_RenderTargetDelete(data->renderTarget);

        shaderProgramFree(&data->shaderProgram);
        DVLB_Free(data->dvlb);

        C3D_Fini();

        data->initialized = SDL_FALSE;
        SDL_free(data);
    }
}

static int
N3DS_SetVSync(SDL_Renderer * renderer, const int vsync)
{
    N3DS_RenderData *data = renderer->driverdata;
    data->vsync = vsync;
    return 0;
}

int
N3DS_CreateRenderer(SDL_Renderer * renderer, SDL_Window * window, Uint32 flags)
{
    N3DS_RenderData *data;
    int width, height, pixelFormat;
    C3D_AttrInfo *attrInfo;
    bool windowIsBottom;

    data = (N3DS_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        N3DS_DestroyRenderer(renderer);
        return SDL_OutOfMemory();
    }

    renderer->WindowEvent = N3DS_WindowEvent;
    renderer->CreateTexture = N3DS_CreateTexture;
    renderer->UpdateTexture = N3DS_UpdateTexture;
    renderer->LockTexture = N3DS_LockTexture;
    renderer->UnlockTexture = N3DS_UnlockTexture;
    renderer->SetTextureScaleMode = N3DS_SetTextureScaleMode;
    renderer->SetRenderTarget = N3DS_SetRenderTarget;
    renderer->QueueSetViewport = N3DS_QueueSetViewport;
    renderer->QueueSetDrawColor = N3DS_QueueSetViewport;  /* SetViewport and SetDrawColor are (currently) no-ops. */
    renderer->QueueGeometry = N3DS_QueueGeometry;
    renderer->QueueFillRects = N3DS_QueueFillRects;
    renderer->QueueCopy = N3DS_QueueCopy;
    renderer->QueueCopyEx = N3DS_QueueCopyEx;
    renderer->RunCommandQueue = N3DS_RunCommandQueue;
    renderer->RenderReadPixels = N3DS_RenderReadPixels;
    renderer->RenderPresent = N3DS_RenderPresent;
    renderer->DestroyTexture = N3DS_DestroyTexture;
    renderer->DestroyRenderer = N3DS_DestroyRenderer;
    renderer->SetVSync = N3DS_SetVSync;
    renderer->info = N3DS_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    renderer->driverdata = data;
    renderer->window = window;
    renderer->point_method = SDL_RENDERPOINTMETHOD_GEOMETRY;
    renderer->line_method = SDL_RENDERLINEMETHOD_GEOMETRY;

    if (data->initialized != SDL_FALSE)
        return 0;
    data->initialized = SDL_TRUE;

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        data->vsync = SDL_TRUE;
    } else {
        data->vsync = SDL_FALSE;
    }

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    /* Load shader */

    data->dvlb = DVLB_ParseFile((uint32_t*) n3ds_shader_v, sizeof(n3ds_shader_v));
    shaderProgramInit(&data->shaderProgram);
    shaderProgramSetVsh(&data->shaderProgram, &data->dvlb->DVLE[0]);
    data->projMtxShaderLoc = shaderInstanceGetUniformLocation(data->shaderProgram.vertexShader, "projection");

    /* Create render targets */

    SDL_GetWindowSizeInPixels(window, &width, &height);
    pixelFormat = PixelFormatToN3DSGPU(SDL_GetWindowPixelFormat(window));
    /* FIXME: We might need a more resilient way of detecting the window<->screen mapping in the future. */
    windowIsBottom = (width == 320);

    data->renderTarget = C3D_RenderTargetCreate(height, width, pixelFormat, GPU_RB_DEPTH16);
    data->boundTarget = NULL;

    C3D_RenderTargetClear(data->renderTarget, C3D_CLEAR_ALL, 0, 0);
    C3D_RenderTargetSetOutput(data->renderTarget,
        windowIsBottom ? GFX_BOTTOM : GFX_TOP, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(pixelFormat) | GX_TRANSFER_OUT_FORMAT(GPU_RB_RGBA8));
    Mtx_OrthoTilt(&data->renderProjMtx, 0.0, width, 0.0, height, -1.0, 1.0, true);

    C3D_FrameBegin(data->vsync ? C3D_FRAME_SYNCDRAW : 0);
    N3DS_SetRenderTarget(renderer, NULL);

    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);

    /* Scissoring */
    C3D_SetScissor(GPU_SCISSOR_NORMAL, 0, 0, width, height);

    /* Bind shader */
    C3D_BindProgram(&data->shaderProgram);

    attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 4);
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2);
    C3D_SetAttrInfo(attrInfo);

    /* Texture environments */
    C3D_TexEnvInit(&data->envTex);
    C3D_TexEnvSrc(&data->envTex, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC) 0, (GPU_TEVSRC) 0);
    C3D_TexEnvOpRgb(&data->envTex, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0);
    C3D_TexEnvOpAlpha(&data->envTex, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0);
    C3D_TexEnvFunc(&data->envTex, C3D_Both, GPU_MODULATE);

    C3D_TexEnvInit(&data->envNoTex);
    C3D_TexEnvSrc(&data->envNoTex, C3D_Both, GPU_PRIMARY_COLOR, (GPU_TEVSRC) 0, (GPU_TEVSRC) 0);
    C3D_TexEnvOpRgb(&data->envNoTex, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0);
    C3D_TexEnvOpAlpha(&data->envNoTex, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0);
    C3D_TexEnvFunc(&data->envNoTex, C3D_Both, GPU_REPLACE);

    ResetBlendState(data, &data->blendState);

    return 0;
}

SDL_RenderDriver N3DS_RenderDriver = {
    .CreateRenderer = N3DS_CreateRenderer,
    .info = {
        .name = "N3DS",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 5,
        .texture_formats = {
            [0] = SDL_PIXELFORMAT_RGBA8888, // GPU_RGBA8
            [1] = SDL_PIXELFORMAT_RGB888, // GPU_RGB8
            [2] = SDL_PIXELFORMAT_RGBA5551, // GPU_RGBA5551
            [3] = SDL_PIXELFORMAT_RGB565, // GPU_RGB565
            [4] = SDL_PIXELFORMAT_RGBA4444 // GPU_RGBA4
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
     }
};

#endif /* SDL_VIDEO_RENDER_N3DS */

/* vi: set ts=4 sw=4 expandtab: */

