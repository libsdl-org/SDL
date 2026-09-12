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

#ifdef SDL_VIDEO_RENDER_PS3

#include "../../video/SDL_sysvideo.h"
#include "../../video/ps3/SDL_ps3video.h"
#include "../SDL_sysrender.h"
#include "SDL_render_ps3.h"
#include "ps3_texture_vpo.h"
#include "ps3_texture_fpo.h"
#include "ps3_color_vpo.h"
#include "ps3_color_fpo.h"

#include <rsx/commands.h>
#include <rsx/gcm_sys.h>
#include <rsx/mm.h>
#include <sys/event_queue.h>
#include <sys/systime.h>

static PS3_RenderData *g_ps3_data;

void BuildOrthoMatrix(float *m, float left, float right, float bottom, float top, float near, float far)
{
    memset(m, 0, sizeof(float) * 16);
    m[0]  = 2.0f / (right - left);
    m[5]  = 2.0f / (top - bottom);
    m[10] = -2.0f / (far - near);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(far + near) / (far - near);
    m[15] = 1.0f;
}

static void HandlerFlip(const u32 head)
{
    (void)head;
    u32 v = g_ps3_data->fbFlipped;

    for (u32 i = g_ps3_data->fbOnDisplay; i != v; i=(i + 1)%FRAME_BUFFER_COUNT) {
        *((vu32*) gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + i)) = BUFFER_IDLE;
    }
    g_ps3_data->fbOnDisplay = v;
    g_ps3_data->fbOnFlip = false;

    sysEventPortSend(g_ps3_data->flipEventPort, 0, 0, 0);
}

static void HandlerVBlank(const u32 head)
{
    (void)head;
    u32 data;
    u32 bufferToFlip;
    u32 indexToFlip;

    data = *((vu32*) gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX));
    bufferToFlip = (data >> 8);
    indexToFlip = (data & 0x07);

    if (!g_ps3_data->fbOnFlip) {
        if (bufferToFlip != g_ps3_data->fbOnDisplay) {
            s32 ret = gcmSetFlipImmediate(indexToFlip);
            if (ret != 0) {
                // Flip immediate failed
                return;
            }
            g_ps3_data->fbFlipped = bufferToFlip;
            g_ps3_data->fbOnFlip = true;
        }
    }
}

void setDrawEnv(SDL_Renderer *renderer)
{
    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    rsxSetColorMask(data->context, GCM_COLOR_MASK_B |
                                GCM_COLOR_MASK_G |
                                GCM_COLOR_MASK_R |
                                GCM_COLOR_MASK_A);

    rsxSetColorMaskMrt(data->context,0);

    u16 x,y,w,h;
    f32 min, max;
    f32 scale[4],offset[4];

    x = 0;
    y = 0;
    w = data->screenw;
    h = data->screenh;
    min = 0.0f;
    max = 1.0f;
    scale[0] = w*0.5f;
    scale[1] = h*-0.5f;
    scale[2] = (max - min)*0.5f;
    scale[3] = 0.0f;
    offset[0] = x + w*0.5f;
    offset[1] = y + h*0.5f;
    offset[2] = (max + min)*0.5f;
    offset[3] = 0.0f;

    rsxSetViewport(data->context,x, y, w, h, min, max, scale, offset);
    rsxSetScissor(data->context,x,y,w,h);

    // Disable depth for 2D
    rsxSetDepthTestEnable(data->context, GCM_FALSE);
    rsxSetDepthFunc(data->context, GCM_LESS);
    rsxSetShadeModel(data->context,GCM_SHADE_MODEL_SMOOTH);
    // Disabe depth buffer updates
    rsxSetDepthWriteEnable(data->context, 0);
    rsxSetFrontFace(data->context,GCM_FRONTFACE_CCW);
}

void PS3_DrawColoredPrimitive(PS3_RenderData *data, u8 primitive_type,
                                ColorVertex *verts, u32 count,
                                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    ColorVertex *gpu_verts = (ColorVertex *)rsxMemalign(16, count * sizeof(ColorVertex));
    memcpy(gpu_verts, verts, count * sizeof(ColorVertex));

    u32 vert_offset;
    rsxAddressToOffset(gpu_verts, &vert_offset);

    rsxLoadVertexProgram(data->context, data->vpo_color, data->vp_ucode_color);
    rsxLoadFragmentProgramLocation(data->context, data->fpo_color, data->fp_offset_color, GCM_LOCATION_RSX);

    const rsxProgramConst *mvp_const = rsxVertexProgramGetConst(data->vpo_color, "modelViewProj");
    rsxSetVertexProgramParameter(data->context, data->vpo_color, mvp_const, data->ortho_matrix);

    const rsxProgramAttrib *pos_attr = rsxVertexProgramGetAttrib(data->vpo_color, "position");
    const rsxProgramAttrib *col_attr = rsxVertexProgramGetAttrib(data->vpo_color, "color");

    rsxBindVertexArrayAttrib(data->context, pos_attr->index, 0,
        vert_offset + offsetof(ColorVertex, x),
        sizeof(ColorVertex), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
    rsxBindVertexArrayAttrib(data->context, col_attr->index, 0,
        vert_offset + offsetof(ColorVertex, r),
        sizeof(ColorVertex), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

    rsxSetBlendFunc(data->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
                                    GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(data->context, GCM_FUNC_ADD, GCM_FUNC_ADD);
    // TODO: use blen only when needed
    // rsxSetBlendEnable(data->context, renderer->blendMode == SDL_BLENDMODE_NONE ? GCM_FALSE : GCM_TRUE);
    rsxSetBlendEnable(data->context, GCM_FALSE);
    // TODO: control pixel size?
    // rsxSetPointSize(data->context, 4.0f);
    rsxDrawVertexArray(data->context, primitive_type, 0, count);
}

static void PS3_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
}

static bool PS3_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    return false;
}

static bool PS3_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_PropertiesID create_props)
{
    int pitch;
    void *pixels;

    // Allocate GFX memory for textures
    pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
    pixels = rsxMemalign(64, texture->h * pitch);
    if (!pixels) {
        return SDL_SetError("rsxMemalign failed");
    }

    PS3_TextureData *tdata = (PS3_TextureData *)SDL_calloc(1, sizeof(PS3_TextureData));
    if (!tdata) {
        return SDL_OutOfMemory();
    }

    tdata->surface = SDL_CreateSurfaceFrom(texture->w, texture->h, texture->format, pixels, pitch);

    rsxAddressToOffset(pixels, &tdata->offset);

    tdata->rsx_texture.format    = GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN;
    tdata->rsx_texture.mipmap    = 1;
    tdata->rsx_texture.dimension = GCM_TEXTURE_DIMS_2D;
    tdata->rsx_texture.cubemap   = GCM_FALSE;
    tdata->rsx_texture.remap     = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
                                    (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
                                    (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
                                    (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
                                    (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
                                    (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
                                    (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
                                    (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
    tdata->rsx_texture.width  = texture->w;
    tdata->rsx_texture.height = texture->h;
    tdata->rsx_texture.depth  = 1;
    tdata->rsx_texture.pitch  = pitch;
    tdata->rsx_texture.offset = tdata->offset;

    texture->internal = (void *)tdata;

    SDL_SetSurfaceColorMod(tdata->surface, (Uint8)texture->color.r, (Uint8)texture->color.g,
                           (Uint8)texture->color.b);
    SDL_SetSurfaceAlphaMod(tdata->surface, (Uint8)texture->color.a);
    SDL_SetSurfaceBlendMode(tdata->surface, texture->blendMode);

    return true;
}

static bool PS3_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                              const SDL_Rect *rect, const void *pixels, int pitch)
{
    PS3_TextureData *tdata = (PS3_TextureData*)texture->internal;
    SDL_Surface *surface = (SDL_Surface *)tdata->surface;

    if (!tdata || !tdata->surface) {
        return SDL_SetError("PS3_UpdateTexture: texture not initialized");
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }

    const int bpp = SDL_BYTESPERPIXEL(texture->format);
    const int row_bytes = rect->w * bpp;

    Uint8 *dst = (Uint8 *)tdata->surface->pixels +
                 (size_t)rect->y * tdata->surface->pitch +
                 (size_t)rect->x * bpp;
    const Uint8 *src = (const Uint8 *)pixels;

    for (int y = 0; y < rect->h; y++) {
        SDL_memcpy(dst, src, row_bytes);
        dst += tdata->surface->pitch;
        src += pitch;
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    return true;
}

static bool PS3_UpdateTextureYUV(SDL_Renderer *renderer, SDL_Texture *texture,
                                 const SDL_Rect *rect,
                                 const Uint8 *Yplane, int Ypitch,
                                 const Uint8 *Uplane, int Upitch,
                                 const Uint8 *Vplane, int Vpitch)
{
    return false;
}

static bool PS3_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                            const SDL_Rect *rect, void **pixels, int *pitch)
{
    PS3_TextureData *tdata = (PS3_TextureData*)texture->internal;
    SDL_Surface *surface = (SDL_Surface *)tdata->surface;

    *pixels =
        (void *)((Uint8 *)surface->pixels + rect->y * surface->pitch +
                 rect->x * surface->fmt->bytes_per_pixel);
    *pitch = surface->pitch;
    return true;
}

static void PS3_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    PS3_TextureData *tdata = (PS3_TextureData*)texture->internal;
    SDL_Surface *surface = (SDL_Surface *)tdata->surface;
    SDL_Rect rect;

    // We do whole texture updates, at least for now
    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    PS3_UpdateTexture(renderer, texture, &rect, surface->pixels, surface->pitch);
}

void PS3_DrawTexturedQuad(PS3_RenderData *data, PS3_TextureData *tdata,
                          float dstx, float dsty, float dstw, float dsth,
                          float srcx, float srcy, float srcw, float srch)
{
    float x0 = dstx,        y0 = dsty;
    float x1 = dstx + dstw, y1 = dsty + dsth;

    float u0 = srcx / (float)tdata->rsx_texture.width;
    float v0 = srcy / (float)tdata->rsx_texture.height;
    float u1 = (srcx + srcw) / (float)tdata->rsx_texture.width;
    float v1 = (srcy + srch) / (float)tdata->rsx_texture.height;

    QuadSlot *slot = &data->quad_ring[data->quad_ring_index];
    data->quad_ring_index = (data->quad_ring_index + 1) % QUAD_RING_SIZE;

    slot->vbo[0] = (TexVertex){ x0, y0, 0.0f, u0, v0 };
    slot->vbo[1] = (TexVertex){ x1, y0, 0.0f, u1, v0 };
    slot->vbo[2] = (TexVertex){ x0, y1, 0.0f, u0, v1 };
    slot->vbo[3] = (TexVertex){ x1, y1, 0.0f, u1, v1 };

    rsxLoadVertexProgram(data->context, data->vpo, data->vp_ucode);
    rsxLoadFragmentProgramLocation(data->context, data->fpo, data->fp_offset, GCM_LOCATION_RSX);

    rsxBindVertexArrayAttrib(data->context, 0, 0,
        slot->offset + offsetof(TexVertex, x),
        sizeof(TexVertex), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

    rsxBindVertexArrayAttrib(data->context, 8, 0,
        slot->offset + offsetof(TexVertex, u),
        sizeof(TexVertex), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

    const rsxProgramConst *mvp_const = rsxVertexProgramGetConst(data->vpo, "modelViewProj");
    if (!mvp_const) {
        SDL_Log("modelViewProj constant not found in vertex program!");
    }

    rsxSetVertexProgramParameter(data->context, data->vpo, mvp_const, data->ortho_matrix);

    // Texture + sampler state
    rsxLoadTexture(data->context, 0, &tdata->rsx_texture);
    rsxTextureControl(data->context, 0, GCM_TRUE, 0, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
    rsxTextureFilter(data->context, 0, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_LINEAR, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(data->context, 0, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE,
                        GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

    rsxSetBlendFunc(data->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
                                    GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    rsxSetBlendEquation(data->context, GCM_FUNC_ADD, GCM_FUNC_ADD);
    // TODO: use blend only when need
    rsxSetBlendEnable(data->context, GCM_TRUE);

    rsxDrawVertexArray(data->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);
}

static bool PS3_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return true;
}

static bool PS3_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return true;
}

static bool PS3_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return true;
}

static bool PS3_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    ColorVertex *verts = (ColorVertex *)SDL_AllocateRenderVertices(renderer, count * sizeof(ColorVertex), 0, &cmd->data.draw.first);

    if (!verts) {
        return false;
    }

    float r,g,b,a;
    r = SDL_clamp(cmd->data.draw.color.r * cmd->data.draw.color_scale, 0.0f, 1.0f) ;
    g = SDL_clamp(cmd->data.draw.color.g * cmd->data.draw.color_scale, 0.0f, 1.0f);
    b = SDL_clamp(cmd->data.draw.color.b * cmd->data.draw.color_scale, 0.0f, 1.0f);
    a = SDL_clamp(cmd->data.draw.color.a, 0.0f, 1.0f);

    cmd->data.draw.count = count;
    for (int i = 0; i < count; i++, verts++, points++) {
        verts->x = points->x;
        verts->y = points->y;
        verts->z = 0.0f;
        verts->r = r;
        verts->g = g;
        verts->b = b;
        verts->a = a;
    }

    return true;
}

static bool PS3_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count)
{
    ColorVertex *verts = (ColorVertex *)SDL_AllocateRenderVertices(renderer, count * 4 * sizeof(ColorVertex), 4, &cmd->data.draw.first);

    if (!verts) {
        return false;
    }

    float rf,gf,bf,af;
    rf = SDL_clamp(cmd->data.draw.color.r * cmd->data.draw.color_scale, 0.0f, 1.0f);
    gf = SDL_clamp(cmd->data.draw.color.g * cmd->data.draw.color_scale, 0.0f, 1.0f);
    bf = SDL_clamp(cmd->data.draw.color.b * cmd->data.draw.color_scale, 0.0f, 1.0f);
    af = SDL_clamp(cmd->data.draw.color.a, 0.0f, 1.0f);

    cmd->data.draw.count = count;
    for (int i = 0; i < count; i++, rects++) {
        float x0 = rects->x,             y0 = rects->y;
        float x1 = rects->x + rects->w,  y1 = rects->y + rects->h;

        verts[0] = (ColorVertex){ x0, y0, 0.0f, rf, gf, bf, af };
        verts[1] = (ColorVertex){ x1, y0, 0.0f, rf, gf, bf, af };
        verts[2] = (ColorVertex){ x0, y1, 0.0f, rf, gf, bf, af };
        verts[3] = (ColorVertex){ x1, y1, 0.0f, rf, gf, bf, af };

        verts += 4;
    }

    return true;
}

static bool PS3_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                             const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride,
                             int num_vertices, const void *indices, int num_indices, int size_indices,
                             float scale_x, float scale_y)
{
    int i;
    int count = indices ? num_indices : num_vertices;
    const float color_scale = cmd->data.draw.color_scale;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    if (!texture) {
        ColorVertex *verts = (ColorVertex *)SDL_AllocateRenderVertices(renderer, count * sizeof(ColorVertex), 0, &cmd->data.draw.first);
        if (!verts) {
            return false;
        }

        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            SDL_FColor *col_;
            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else if (size_indices == 1) {
                j = ((const Uint8 *)indices)[i];
            } else {
                j = i;
            }

            xy_ = (float *)((char *)xy + j * xy_stride);
            col_ = (SDL_FColor *)((char *)color + j * color_stride);

            verts->x = xy_[0] * scale_x;
            verts->y = xy_[1] * scale_y;
            verts->z = 0;

            verts->r = (Uint8)SDL_roundf(SDL_clamp(col_->r * color_scale, 0.0f, 1.0f) * 255.0f);
            verts->g = (Uint8)SDL_roundf(SDL_clamp(col_->g * color_scale, 0.0f, 1.0f) * 255.0f);
            verts->b = (Uint8)SDL_roundf(SDL_clamp(col_->b * color_scale, 0.0f, 1.0f) * 255.0f);
            verts->a = (Uint8)SDL_roundf(SDL_clamp(col_->a, 0.0f, 1.0f) * 255.0f);

            verts++;
        }
    }

    return true;
}

static bool PS3_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect)
{
    const size_t outLen = sizeof(PS3_CopyData);
    PS3_CopyData *outData = (PS3_CopyData *)SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);

    if (!outData) {
        return false;
    }

    cmd->data.draw.count = 1;

    SDL_memcpy(&outData->srcRect, srcrect, sizeof(SDL_FRect));
    SDL_memcpy(&outData->dstRect, dstrect, sizeof(SDL_FRect));

    return true;
}

static bool PS3_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                            const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                            const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y)
{
    const size_t outLen = sizeof(PS3_CopyData);
    PS3_CopyData *outData = (PS3_CopyData *)SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);

    if (!outData) {
        return false;
    }

    cmd->data.draw.count = 1;

    SDL_memcpy(&outData->srcRect, srcrect, sizeof(SDL_FRect));
    SDL_memcpy(&outData->dstRect, dstrect, sizeof(SDL_FRect));

    return true;
}

static void PS3_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    // Setup screen
    rsxSetColorMask(data->context, GCM_COLOR_MASK_B |
                                   GCM_COLOR_MASK_G |
                                   GCM_COLOR_MASK_R |
                                   GCM_COLOR_MASK_A);

    rsxSetColorMaskMrt(data->context,0);

    u16 x,y,w,h;
    f32 min, max;
    f32 scale[4],offset[4];

    x = 0;
    y = 0;
    w = data->screenw;
    h = data->screenh;
    min = 0.0f;
    max = 1.0f;
    scale[0] = w*0.5f;
    scale[1] = h*-0.5f;
    scale[2] = (max - min)*0.5f;
    scale[3] = 0.0f;
    offset[0] = x + w*0.5f;
    offset[1] = y + h*0.5f;
    offset[2] = (max + min)*0.5f;
    offset[3] = 0.0f;

    rsxSetViewport(data->context,x, y, w, h, min, max, scale, offset);
    rsxSetScissor(data->context,x,y,w,h);

    // Disable depth for 2D.
    rsxSetDepthTestEnable(data->context, GCM_FALSE);
    rsxSetDepthFunc(data->context, GCM_LESS);
    rsxSetShadeModel(data->context,GCM_SHADE_MODEL_SMOOTH);
    rsxSetDepthWriteEnable(data->context, 0);
    rsxSetFrontFace(data->context,GCM_FRONTFACE_CCW);

    // Clear screen.
    Uint8 cr = (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.r * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f);
    Uint8 cg = (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.g * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f);
    Uint8 cb = (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.b * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f);
    Uint8 ca = (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.a, 0.0f, 1.0f) * 255.0f);

    u32 clear_color = ((u32)ca << 24) | ((u32)cr << 16) | ((u32)cg << 8) | (u32)cb;
    rsxSetClearColor(data->context, clear_color);

    rsxSetClearDepthStencil(data->context, 0xffff);
    rsxClearSurface(data->context, GCM_CLEAR_R |
                                   GCM_CLEAR_G |
                                   GCM_CLEAR_B |
                                   GCM_CLEAR_A |
                                   GCM_CLEAR_S |
                                   GCM_CLEAR_Z);

    rsxSetZMinMaxControl(data->context,GCM_FALSE, GCM_TRUE, GCM_FALSE);
}

static bool PS3_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    Uint8 r, g, b, a;
    while (cmd) {
        r = data->drawstate.color.r;
        g = data->drawstate.color.g;
        b = data->drawstate.color.b;
        a = data->drawstate.color.a;

        switch (cmd->command) {
        case SDL_RENDERCMD_GEOMETRY:
            break;

        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            data->drawstate.color = (SDL_Color){
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.r * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.g * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.b * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.a, 0.0f, 1.0f) * 255.0f)
            };
            break;
        }

        case SDL_RENDERCMD_SETVIEWPORT:
            data->drawstate.viewport = &cmd->data.viewport.rect;
            break;

        case SDL_RENDERCMD_SETCLIPRECT:
        {
            if (data->drawstate.cliprect_enabled != cmd->data.cliprect.enabled) {
                data->drawstate.cliprect_enabled = cmd->data.cliprect.enabled;
                data->drawstate.cliprect_enabled_dirty = true;
            }

            const SDL_Rect *rect = &cmd->data.cliprect.rect;
            if (SDL_memcmp(&data->drawstate.cliprect, rect, sizeof(*rect)) != 0) {
                SDL_copyp(&data->drawstate.cliprect, rect);
                data->drawstate.cliprect_dirty = true;
            }
            break;
        }
        case SDL_RENDERCMD_CLEAR:
        {
            PS3_RenderClear(renderer, cmd);
            break;
        }

        case SDL_RENDERCMD_DRAW_POINTS:
        {
            ColorVertex *verts = (ColorVertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (data->drawstate.viewport && (data->drawstate.viewport->x || data->drawstate.viewport->y)) {
                for (int i = 0; i < count; i++) {
                    verts[i].x += data->drawstate.viewport->x;
                    verts[i].y += data->drawstate.viewport->y;
                }
            }

            PS3_DrawColoredPrimitive(data, GCM_TYPE_POINTS, verts, count, r, g, b, a);
            break;
        }

        case SDL_RENDERCMD_DRAW_LINES:
        {
            ColorVertex *verts = (ColorVertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (data->drawstate.viewport && (data->drawstate.viewport->x || data->drawstate.viewport->y)) {
                for (int i = 0; i < count; i++) {
                    verts[i].x += data->drawstate.viewport->x;
                    verts[i].y += data->drawstate.viewport->y;
                }
            }

            PS3_DrawColoredPrimitive(data, GCM_TYPE_LINE_STRIP, verts, count, r, g, b, a);
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS:
        {
            ColorVertex *verts = (ColorVertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            for (int i = 0; i < count; i++) {
                // Apply viewport.
                if (data->drawstate.viewport && (data->drawstate.viewport->x || data->drawstate.viewport->y)) {
                    for (int j = i; j < (i+1)*4 ; j++) {
                        verts[j].x += data->drawstate.viewport->x;
                        verts[j].y += data->drawstate.viewport->y;
                    }
                }
                PS3_DrawColoredPrimitive(data, GCM_TYPE_TRIANGLE_STRIP, verts + (i * 4), 4, r, g, b, a);
            }
            break;
        }

        case SDL_RENDERCMD_COPY:
        case SDL_RENDERCMD_COPY_EX:
        {
            PS3_TextureData *tdata = (PS3_TextureData*)cmd->data.draw.texture->internal;

            PS3_CopyData copyData;
            SDL_memcpy(&copyData, ((Uint8 *)vertices) + cmd->data.draw.first, sizeof(PS3_CopyData));

            SDL_FRect *srcrect = &copyData.srcRect;
            SDL_FRect *dstrect = &copyData.dstRect;

            // Apply viewport
            if (data->drawstate.viewport && (data->drawstate.viewport->x || data->drawstate.viewport->y)) {
                dstrect->x += data->drawstate.viewport->x;
                dstrect->y += data->drawstate.viewport->y;
            }

            PS3_DrawTexturedQuad(data, tdata,
                dstrect->x, dstrect->y, dstrect->w, dstrect->h,
                srcrect->x, srcrect->y, srcrect->w, srcrect->h
            );
            break;
        }

        case SDL_RENDERCMD_NO_OP:
            break;
        }

        cmd = cmd->next;
    }

    return true;
}

static SDL_Surface *PS3_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    return (SDL_Surface *)0;
}

static bool PS3_RenderPresent(SDL_Renderer *renderer)
{
    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    s32 qid = gcmSetPrepareFlip(data->context, data->curr_fb);
    while (qid < 0) {
        sysUsleep(100);
        qid = gcmSetPrepareFlip(data->context, data->curr_fb);
    }

    rsxSetWriteBackendLabel(data->context, GCM_PREPARED_BUFFER_INDEX, ((data->curr_fb << 8) | qid));
    rsxFlushBuffer(data->context);

    vu32 *label = (vu32*) gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX);
    while(((data->curr_fb + FRAME_BUFFER_COUNT - ((*label)>>8))%FRAME_BUFFER_COUNT) > MAX_BUFFER_QUEUE_SIZE) {
        sys_event_t event;

        sysEventQueueReceive(data->flipEventQueue, &event, 0);
        sysEventQueueDrain(data->flipEventQueue);
    }

    data->curr_fb = (data->curr_fb + 1)%FRAME_BUFFER_COUNT;

    rsxSetWaitLabel(data->context, GCM_BUFFER_STATUS_INDEX + data->curr_fb, BUFFER_IDLE);
    rsxSetWriteCommandLabel(data->context, GCM_BUFFER_STATUS_INDEX + data->curr_fb, BUFFER_BUSY);

    // Set render target
    data->surface.colorOffset[0]    = data->color_offset[data->curr_fb];
    rsxSetSurface(data->context,&data->surface);

    return true;
}

static void PS3_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    PS3_TextureData *tdata = (PS3_TextureData*)texture->internal;
    SDL_Surface *surface = (SDL_Surface *)tdata->surface;

    if (!surface) {
        return;
    }

    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    rsxFinish(data->context, 1);
    rsxFree(surface->pixels);
    SDL_DestroySurface(surface);
    SDL_free(tdata);
    texture->internal = NULL;
}

static void PS3_DestroyRenderer(SDL_Renderer *renderer)
{
    PS3_RenderData *data = (PS3_RenderData *)renderer->internal;

    // Stop RSX before exit
    gcmSetWaitFlip(data->context);
    rsxFinish(data->context, 1);

    u32 dataBuffer = *((vu32*) gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX));
    u32 lastBuffer = (dataBuffer >> 8);
    while (lastBuffer != data->fbOnDisplay) {
        sysUsleep(100);
    }

    if (data) {
        rsxFree(data->fp_ucode_rsx);
        rsxFree(data->fp_ucode_rsx_color);
        SDL_free(data);
    }
    SDL_free(renderer);
}

static bool PS3_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID create_props)
{
    PS3_RenderData *data;

    SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
    if (displayID == 0) {
        return SDL_SetError("PS3_CreateRenderer: could not get display for window");
    }

    const SDL_DisplayMode *displayMode = SDL_GetCurrentDisplayMode(displayID);

    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!displayMode) {
        bpp = 32;
        Rmask = 0x00FF0000;
        Gmask = 0x0000FF00;
        Bmask = 0x000000FF;
        Amask = 0xFF000000;
    } else {
        SDL_GetMasksForPixelFormat(displayMode->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    }

    data = (PS3_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return false;
    }

    SDL_zerop(data);
    rsxHeapInit();

    g_ps3_data = data;

    SDL_VideoDevice *videoDevice = SDL_GetVideoDevice();
    if (!videoDevice || !videoDevice->internal) {
        SDL_free(data);
        return false;
    }

    SDL_VideoData *devdata = (SDL_VideoData *)videoDevice->internal;

    if (!devdata->_CommandBuffer) {
        SDL_free(data);
        return false;
    }

    // Get a copy of the command buffer
    data->context = devdata->_CommandBuffer;
    data->color_pitch = displayMode->w * SDL_BYTESPERPIXEL(displayMode->format);
    data->screenw = displayMode->w;
    data->screenh = displayMode->h;

    data->fbOnDisplay = 0;
    data->fbFlipped = 0;
    data->fbOnFlip = false;

    u32 zs_depth = 4;
    u32 color_depth = 4;

    data->color_pitch = data->screenw*color_depth;
    data->depth_pitch = data->screenw*zs_depth;

    // Wait RSX idle
    u32 sLabelVal = 1;
    rsxSetWriteBackendLabel(data->context,GCM_WAIT_LABEL_INDEX,sLabelVal);
    rsxSetWaitLabel(data->context,GCM_WAIT_LABEL_INDEX,sLabelVal);

    ++sLabelVal;

    // Wait RSX finish
    rsxSetWriteBackendLabel(data->context,GCM_WAIT_LABEL_INDEX,sLabelVal);
    rsxFlushBuffer(data->context);

    while(*(vu32*)gcmGetLabelAddress(GCM_WAIT_LABEL_INDEX)!=sLabelVal)
        sysUsleep(30);

    ++sLabelVal;

    gcmSetFlipMode(GCM_FLIP_HSYNC);

    void *buffer;
    for (u32 i=0;i < FRAME_BUFFER_COUNT;i++) {
        buffer = rsxMemalign(64, data->screenh*data->color_pitch);
        rsxAddressToOffset(buffer, &data->color_offset[i]);
        printf("fb[%d]: %p (%08x) [%dx%d] %d\n", i, buffer, data->color_offset[i], data->screenw, data->screenh, data->color_pitch);
        gcmSetDisplayBuffer(i, data->color_offset[i], data->color_pitch, data->screenw, data->screenh);
    }

    buffer = rsxMemalign(64, (data->screenh*data->depth_pitch) * 2);
    rsxAddressToOffset(buffer, &data->depth_offset);

    for (u32 i=0;i < FRAME_BUFFER_COUNT;i++) {
        *((vu32*) gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + i)) = BUFFER_IDLE;
    }
    *((vu32*) gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX)) = (data->fbOnDisplay << 8);
    *((vu32*) gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + data->fbOnDisplay)) = BUFFER_BUSY;

    data->curr_fb = (data->fbOnDisplay + 1)%FRAME_BUFFER_COUNT;

    // Init flip event
    sys_event_queue_attr_t queueAttr = { SYS_EVENT_QUEUE_PRIO, SYS_EVENT_QUEUE_PPU, "\0" };

    sysEventQueueCreate(&data->flipEventQueue, &queueAttr, SYS_EVENT_QUEUE_KEY_LOCAL, 32);
    sysEventPortCreate(&data->flipEventPort, SYS_EVENT_PORT_LOCAL, SYS_EVENT_PORT_NO_NAME);
    sysEventPortConnectLocal(data->flipEventPort, data->flipEventQueue);

    gcmSetFlipHandler(HandlerFlip);
    gcmSetVBlankHandler(HandlerVBlank);

    // Init render target
    memset(&data->surface, 0, sizeof(gcmSurface));

    data->surface.colorFormat = GCM_SURFACE_X8R8G8B8;
    data->surface.colorTarget = GCM_SURFACE_TARGET_0;
    data->surface.colorLocation[0] = GCM_LOCATION_RSX;
    data->surface.colorOffset[0] = data->color_offset[data->curr_fb];
    data->surface.colorPitch[0] = data->color_pitch;

    for(u32 i=1; i< GCM_MAX_MRT_COUNT;i++) {
        data->surface.colorLocation[i] = GCM_LOCATION_RSX;
        data->surface.colorOffset[i] = data->color_offset[data->curr_fb];
        data->surface.colorPitch[i] = 64;
    }

    data->surface.depthFormat = GCM_SURFACE_ZETA_Z16;
    data->surface.depthLocation = GCM_LOCATION_RSX;
    data->surface.depthOffset = data->depth_offset;
    data->surface.depthPitch = data->depth_pitch;

    data->surface.type = GCM_SURFACE_TYPE_LINEAR;
    data->surface.antiAlias = GCM_SURFACE_CENTER_1;

    data->surface.width = data->screenw;
    data->surface.height = data->screenh;
    data->surface.x = 0;
    data->surface.y = 0;

    rsxSetWriteCommandLabel(data->context, GCM_BUFFER_STATUS_INDEX + data->curr_fb, BUFFER_BUSY);

    // VPO / FPO
    data->vpo = (rsxVertexProgram *)ps3_texture_vpo;
    data->fpo = (rsxFragmentProgram *)ps3_texture_fpo;

    rsxVertexProgramGetUCode(data->vpo, &data->vp_ucode, &data->vp_ucode_size);
    rsxLoadVertexProgram(data->context, data->vpo, data->vp_ucode);

    rsxFragmentProgramGetUCode(data->fpo, &data->fp_ucode_cpu, &data->fp_ucode_size);

    data->fp_ucode_rsx = rsxMemalign(64, data->fp_ucode_size);
    memcpy(data->fp_ucode_rsx, data->fp_ucode_cpu, data->fp_ucode_size);

    rsxAddressToOffset(data->fp_ucode_rsx, &data->fp_offset);

    rsxLoadFragmentProgramLocation(data->context, data->fpo, data->fp_offset, GCM_LOCATION_RSX);

    // VPO / FPO COLOR
    data->vpo_color = (rsxVertexProgram *)ps3_color_vpo;
    data->fpo_color = (rsxFragmentProgram *)ps3_color_fpo;

    rsxVertexProgramGetUCode(data->vpo_color, &data->vp_ucode_color, &data->vp_ucode_size_color);
    rsxLoadVertexProgram(data->context, data->vpo_color, data->vp_ucode_color);

    rsxFragmentProgramGetUCode(data->fpo_color, &data->fp_ucode_cpu_color, &data->fp_ucode_size_color);

    data->fp_ucode_rsx_color = rsxMemalign(64, data->fp_ucode_size_color);
    memcpy(data->fp_ucode_rsx_color, data->fp_ucode_cpu_color, data->fp_ucode_size_color);

    rsxAddressToOffset(data->fp_ucode_rsx_color, &data->fp_offset_color);

    rsxLoadFragmentProgramLocation(data->context, data->fpo_color, data->fp_offset_color, GCM_LOCATION_RSX);

    // Init quad
    for (int i = 0; i < QUAD_RING_SIZE; i++) {
        data->quad_ring[i].vbo = (TexVertex *)rsxMemalign(16, 4 * sizeof(TexVertex));
        rsxAddressToOffset(data->quad_ring[i].vbo, &data->quad_ring[i].offset);
    }
    data->quad_ring_index = 0;

    // Build matrix for textures
    BuildOrthoMatrix(data->ortho_matrix, 0.0f, data->screenw, data->screenh, 0.0f, -1.0f, 1.0f);

    renderer->name = PS3_RenderDriver.name;
    renderer->npot_texture_wrap_unsupported = false;
    renderer->internal = data;
    renderer->window = window;
    renderer->WindowEvent = PS3_WindowEvent;
    renderer->SupportsBlendMode = PS3_SupportsBlendMode;
    renderer->CreateTexture = PS3_CreateTexture;
    renderer->UpdateTexture = PS3_UpdateTexture;
    renderer->UpdateTextureYUV = PS3_UpdateTextureYUV;
    renderer->LockTexture = PS3_LockTexture;
    renderer->UnlockTexture = PS3_UnlockTexture;
    renderer->SetRenderTarget = PS3_SetRenderTarget;
    renderer->QueueSetViewport = PS3_QueueSetViewport;
    renderer->QueueSetDrawColor = PS3_QueueSetDrawColor;
    renderer->QueueDrawPoints = PS3_QueueDrawPoints;
    renderer->QueueDrawLines = PS3_QueueDrawPoints;
    renderer->QueueFillRects = PS3_QueueFillRects;
    renderer->QueueGeometry = PS3_QueueGeometry;
    renderer->QueueCopy = PS3_QueueCopy;
    renderer->QueueCopyEx = PS3_QueueCopyEx;
    renderer->RunCommandQueue = PS3_RunCommandQueue;
    renderer->RenderReadPixels = PS3_RenderReadPixels;
    renderer->RenderPresent = PS3_RenderPresent;
    renderer->DestroyTexture = PS3_DestroyTexture;
    renderer->DestroyRenderer = PS3_DestroyRenderer;

    // Use Lines instead of rects
    SDL_SetHintWithPriority(SDL_HINT_RENDER_LINE_METHOD, "2", SDL_HINT_OVERRIDE);

    SDL_AddSupportedTextureFormat(renderer, SDL_PIXELFORMAT_ARGB8888);
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 1024);

    return true;
}

SDL_RenderDriver PS3_RenderDriver = {
    PS3_CreateRenderer, "PS3"
};

#endif // SDL_VIDEO_RENDER_PS3
