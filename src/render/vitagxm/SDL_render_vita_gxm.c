/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_RENDER_VITA_GXM

#include "SDL_hints.h"
#include "../SDL_sysrender.h"
#include "SDL_log.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include "SDL_render_vita_gxm_types.h"
#include "SDL_render_vita_gxm_tools.h"
#include "SDL_render_vita_gxm_memory.h"

static SDL_Renderer *VITA_GXM_CreateRenderer(SDL_Window *window, Uint32 flags);

static void VITA_GXM_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event);

static SDL_bool VITA_GXM_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode);

static int VITA_GXM_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);

static int VITA_GXM_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, const void *pixels, int pitch);

static int VITA_GXM_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
    const SDL_Rect * rect,
    const Uint8 *Yplane, int Ypitch,
    const Uint8 *Uplane, int Upitch,
    const Uint8 *Vplane, int Vpitch);

static int VITA_GXM_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, void **pixels, int *pitch);

static void VITA_GXM_UnlockTexture(SDL_Renderer *renderer,
    SDL_Texture *texture);

static void VITA_GXM_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode);

static int VITA_GXM_SetRenderTarget(SDL_Renderer *renderer,
    SDL_Texture *texture);


static int VITA_GXM_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd);

static int VITA_GXM_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd);


static int VITA_GXM_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count);
static int VITA_GXM_QueueDrawLines(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count);

static int VITA_GXM_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcrect, const SDL_FRect * dstrect);

static int VITA_GXM_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcrect, const SDL_FRect * dstrect,
    const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);

static int VITA_GXM_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd);

static int VITA_GXM_RenderDrawPoints(SDL_Renderer *renderer, const SDL_RenderCommand *cmd);

static int VITA_GXM_RenderDrawLines(SDL_Renderer *renderer, const SDL_RenderCommand *cmd);

static int VITA_GXM_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count);

static int VITA_GXM_RenderFillRects(SDL_Renderer *renderer, const SDL_RenderCommand *cmd);


static int VITA_GXM_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);

static int VITA_GXM_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
    Uint32 pixel_format, void *pixels, int pitch);


static void VITA_GXM_RenderPresent(SDL_Renderer *renderer);
static void VITA_GXM_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void VITA_GXM_DestroyRenderer(SDL_Renderer *renderer);


SDL_RenderDriver VITA_GXM_RenderDriver = {
    .CreateRenderer = VITA_GXM_CreateRenderer,
    .info = {
        .name = "VITA gxm",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC,
        .num_texture_formats = 1,
        .texture_formats = {
            [0] = SDL_PIXELFORMAT_ABGR8888, // TODO: support more formats? ARGB8888 should be enough?
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
     }
};

void
StartDrawing(SDL_Renderer *renderer)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    if(data->drawing)
    {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "uh-oh, already drawing\n");
        return;
    }

    // reset blend mode
//    data->currentBlendMode = SDL_BLENDMODE_BLEND;
//    fragment_programs *in = &data->blendFragmentPrograms.blend_mode_blend;
//    data->colorFragmentProgram = in->color;
//    data->textureFragmentProgram = in->texture;
//    data->textureTintFragmentProgram = in->textureTint;

    if (renderer->target == NULL) {
        sceGxmBeginScene(
            data->gxm_context,
            0,
            data->renderTarget,
            NULL,
            NULL,
            data->displayBufferSync[data->backBufferIndex],
            &data->displaySurface[data->backBufferIndex],
            &data->depthSurface
        );
    } else {
        VITA_GXM_TextureData *vita_texture = (VITA_GXM_TextureData *) renderer->target->driverdata;

        sceGxmBeginScene(
            data->gxm_context,
            0,
            vita_texture->tex->gxm_rendertarget,
            NULL,
            NULL,
            NULL,
            &vita_texture->tex->gxm_colorsurface,
            &vita_texture->tex->gxm_depthstencil
        );
    }

//    unset_clip_rectangle(data);

    data->drawing = SDL_TRUE;
}

SDL_Renderer *
VITA_GXM_CreateRenderer(SDL_Window *window, Uint32 flags)
{
    SDL_Renderer *renderer;
    VITA_GXM_RenderData *data;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (VITA_GXM_RenderData *) SDL_calloc(1, sizeof(VITA_GXM_RenderData));
    if (!data) {
        VITA_GXM_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->WindowEvent = VITA_GXM_WindowEvent;
    renderer->SupportsBlendMode = VITA_GXM_SupportsBlendMode;
    renderer->CreateTexture = VITA_GXM_CreateTexture;
    renderer->UpdateTexture = VITA_GXM_UpdateTexture;
    renderer->UpdateTextureYUV = VITA_GXM_UpdateTextureYUV;
    renderer->LockTexture = VITA_GXM_LockTexture;
    renderer->UnlockTexture = VITA_GXM_UnlockTexture;
    renderer->SetTextureScaleMode = VITA_GXM_SetTextureScaleMode;
    renderer->SetRenderTarget = VITA_GXM_SetRenderTarget;
    renderer->QueueSetViewport = VITA_GXM_QueueSetViewport;
    renderer->QueueSetDrawColor = VITA_GXM_QueueSetDrawColor;
    renderer->QueueDrawPoints = VITA_GXM_QueueDrawPoints;
    renderer->QueueDrawLines = VITA_GXM_QueueDrawLines;
    renderer->QueueFillRects = VITA_GXM_QueueFillRects;
    renderer->QueueCopy = VITA_GXM_QueueCopy;
    renderer->QueueCopyEx = VITA_GXM_QueueCopyEx;
    renderer->RunCommandQueue = VITA_GXM_RunCommandQueue;
    renderer->RenderReadPixels = VITA_GXM_RenderReadPixels;
    renderer->RenderPresent = VITA_GXM_RenderPresent;
    renderer->DestroyTexture = VITA_GXM_DestroyTexture;
    renderer->DestroyRenderer = VITA_GXM_DestroyRenderer;

    renderer->info = VITA_GXM_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    renderer->driverdata = data;
    renderer->window = window;

    if (data->initialized != SDL_FALSE)
        return 0;
    data->initialized = SDL_TRUE;

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        data->displayData.wait_vblank = SDL_TRUE;
    } else {
        data->displayData.wait_vblank = SDL_FALSE;
    }

    if (gxm_init(renderer) != 0)
    {
        return NULL;
    }

    return renderer;
}

static void
VITA_GXM_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
}

static SDL_bool
VITA_GXM_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
    // only for custom modes. we build all modes on init, so no custom modes, sorry
    return SDL_FALSE;
}

static int
VITA_GXM_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    VITA_GXM_TextureData* vita_texture = (VITA_GXM_TextureData*) SDL_calloc(1, sizeof(VITA_GXM_TextureData));

    if (!vita_texture) {
        return SDL_OutOfMemory();
    }

    vita_texture->tex = create_gxm_texture(data, texture->w, texture->h, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8, (texture->access == SDL_TEXTUREACCESS_TARGET));

    if (!vita_texture->tex) {
        SDL_free(vita_texture);
        return SDL_OutOfMemory();
    }

    texture->driverdata = vita_texture;

    VITA_GXM_SetTextureScaleMode(renderer, texture, texture->scaleMode);

    vita_texture->w = gxm_texture_get_width(vita_texture->tex);
    vita_texture->h = gxm_texture_get_height(vita_texture->tex);
    vita_texture->pitch = gxm_texture_get_stride(vita_texture->tex);

    return 0;
}


static int
VITA_GXM_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, const void *pixels, int pitch)
{
    const Uint8 *src;
    Uint8 *dst;
    int row, length,dpitch;
    src = pixels;

    VITA_GXM_LockTexture(renderer, texture, rect, (void **)&dst, &dpitch);
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

    return 0;
}

static int
VITA_GXM_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
    const SDL_Rect * rect,
    const Uint8 *Yplane, int Ypitch,
    const Uint8 *Uplane, int Upitch,
    const Uint8 *Vplane, int Vpitch)
{
    return 0;
}

static int
VITA_GXM_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, void **pixels, int *pitch)
{
    VITA_GXM_TextureData *vita_texture = (VITA_GXM_TextureData *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) gxm_texture_get_datap(vita_texture->tex)
            + (rect->y * vita_texture->pitch) + rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = vita_texture->pitch;
    return 0;
}

static void
VITA_GXM_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    // No need to update texture data on ps vita.
    // VITA_GXM_LockTexture already returns a pointer to the texture pixels buffer.
    // This really improves framerate when using lock/unlock.
}

static void
VITA_GXM_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
    VITA_GXM_TextureData *vita_texture = (VITA_GXM_TextureData *) texture->driverdata;

    /*
     set texture filtering according to scaleMode
     suported hint values are nearest (0, default) or linear (1)
     vitaScaleMode is either SCE_GXM_TEXTURE_FILTER_POINT (good for tile-map)
     or SCE_GXM_TEXTURE_FILTER_LINEAR (good for scaling)
     */

    int vitaScaleMode = (scaleMode == SDL_ScaleModeNearest
                        ? SCE_GXM_TEXTURE_FILTER_POINT
                        : SCE_GXM_TEXTURE_FILTER_LINEAR);
    gxm_texture_set_filters(vita_texture->tex, vitaScaleMode, vitaScaleMode);

    return;
}

static int
VITA_GXM_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0; // nothing to do here
}

static void
VITA_GXM_SetBlendMode(SDL_Renderer *renderer, int blendMode)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    if (blendMode != data->currentBlendMode)
    {
        fragment_programs *in = &data->blendFragmentPrograms.blend_mode_blend;

        switch (blendMode)
        {
            case SDL_BLENDMODE_NONE:
                in = &data->blendFragmentPrograms.blend_mode_none;
                break;
            case SDL_BLENDMODE_BLEND:
                in = &data->blendFragmentPrograms.blend_mode_blend;
                break;
            case SDL_BLENDMODE_ADD:
                in = &data->blendFragmentPrograms.blend_mode_add;
                break;
            case SDL_BLENDMODE_MOD:
                in = &data->blendFragmentPrograms.blend_mode_mod;
                break;
            case SDL_BLENDMODE_MUL:
                in = &data->blendFragmentPrograms.blend_mode_mul;
                break;
        }
        data->colorFragmentProgram = in->color;
        data->textureFragmentProgram = in->texture;
        data->textureTintFragmentProgram = in->textureTint;
        data->currentBlendMode = blendMode;
    }
}

static int
VITA_GXM_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0; // TODO
}

static int
VITA_GXM_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    const Uint8 r = cmd->data.color.r;
    const Uint8 g = cmd->data.color.g;
    const Uint8 b = cmd->data.color.b;
    const Uint8 a = cmd->data.color.a;
    data->drawstate.color = ((a << 24) | (b << 16) | (g << 8) | r);

    return 0;
}

static int
VITA_GXM_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    int color = data->drawstate.color;

    color_vertex *vertex = (color_vertex *)pool_memalign(
        data,
        count * sizeof(color_vertex),
        sizeof(color_vertex)
    );

    cmd->data.draw.first = (size_t)vertex;
    cmd->data.draw.count = count;

    for (int i = 0; i < count; i++)
    {
        vertex[i].x = points[i].x;
        vertex[i].y = points[i].y;
        vertex[i].z = +0.5f;
        vertex[i].color = color;
    }
    return 0;
}

static int
VITA_GXM_QueueDrawLines(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    int color = data->drawstate.color;

    color_vertex *vertex = (color_vertex *)pool_memalign(
        data,
        (count-1) * 2 * sizeof(color_vertex),
        sizeof(color_vertex)
    );

    cmd->data.draw.first = (size_t)vertex;
    cmd->data.draw.count = (count-1) * 2;

    for (int i = 0; i < count - 1; i++)
    {
        vertex[i*2].x = points[i].x;
        vertex[i*2].y = points[i].y;
        vertex[i*2].z = +0.5f;
        vertex[i*2].color = color;

        vertex[i*2+1].x = points[i+1].x;
        vertex[i*2+1].y = points[i+1].y;
        vertex[i*2+1].z = +0.5f;
        vertex[i*2+1].color = color;
    }

    return 0;
}

static int
VITA_GXM_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    cmd->data.draw.count = count;
    int color = data->drawstate.color;

    color_vertex *vertices = (color_vertex *)pool_memalign(
            data,
            4 * count * sizeof(color_vertex), // 4 vertices * count
            sizeof(color_vertex));

    for (int i =0; i < count; i++)
    {
        const SDL_FRect *rect = &rects[i];

        vertices[4*i+0].x = rect->x;
        vertices[4*i+0].y = rect->y;
        vertices[4*i+0].z = +0.5f;
        vertices[4*i+0].color = color;

        vertices[4*i+1].x = rect->x + rect->w;
        vertices[4*i+1].y = rect->y;
        vertices[4*i+1].z = +0.5f;
        vertices[4*i+1].color = color;

        vertices[4*i+2].x = rect->x;
        vertices[4*i+2].y = rect->y + rect->h;
        vertices[4*i+2].z = +0.5f;
        vertices[4*i+2].color = color;

        vertices[4*i+3].x = rect->x + rect->w;
        vertices[4*i+3].y = rect->y + rect->h;
        vertices[4*i+3].z = +0.5f;
        vertices[4*i+3].color = color;
    }

    cmd->data.draw.first = (size_t)vertices;

    return 0;
}


#define PI   3.14159265358979f

#define degToRad(x) ((x)*PI/180.f)

void MathSincos(float r, float *s, float *c)
{
    *s = sinf(r);
    *c = cosf(r);
}

void Swap(float *a, float *b)
{
    float n=*a;
    *a = *b;
    *b = n;
}

static int
VITA_GXM_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{

    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    Uint8 r, g, b, a;
    SDL_GetTextureColorMod(texture, &r, &g, &b);
    SDL_GetTextureAlphaMod(texture, &a);

    cmd->data.draw.r = r;
    cmd->data.draw.g = g;
    cmd->data.draw.b = b;
    cmd->data.draw.a = a;
    cmd->data.draw.blend = renderer->blendMode;

    cmd->data.draw.count = 1;

    texture_vertex *vertices = (texture_vertex *)pool_memalign(
            data,
            4 * sizeof(texture_vertex), // 4 vertices
            sizeof(texture_vertex));

    cmd->data.draw.first = (size_t)vertices;
    cmd->data.draw.texture = texture;

    const float u0 = (float)srcrect->x / (float)texture->w;
    const float v0 = (float)srcrect->y / (float)texture->h;
    const float u1 = (float)(srcrect->x + srcrect->w) / (float)texture->w;
    const float v1 = (float)(srcrect->y + srcrect->h) / (float)texture->h;

    vertices[0].x = dstrect->x;
    vertices[0].y = dstrect->y;
    vertices[0].z = +0.5f;
    vertices[0].u = u0;
    vertices[0].v = v0;

    vertices[1].x = dstrect->x + dstrect->w;
    vertices[1].y = dstrect->y;
    vertices[1].z = +0.5f;
    vertices[1].u = u1;
    vertices[1].v = v0;

    vertices[2].x = dstrect->x;
    vertices[2].y = dstrect->y + dstrect->h;
    vertices[2].z = +0.5f;
    vertices[2].u = u0;
    vertices[2].v = v1;

    vertices[3].x = dstrect->x + dstrect->w;
    vertices[3].y = dstrect->y + dstrect->h;
    vertices[3].z = +0.5f;
    vertices[3].u = u1;
    vertices[3].v = v1;

    return 0;
}

static int
VITA_GXM_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcrect, const SDL_FRect * dstrect,
    const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    Uint8 r, g, b, a;
    SDL_GetTextureColorMod(texture, &r, &g, &b);
    SDL_GetTextureAlphaMod(texture, &a);

    cmd->data.draw.r = r;
    cmd->data.draw.g = g;
    cmd->data.draw.b = b;
    cmd->data.draw.a = a;
    cmd->data.draw.blend = renderer->blendMode;

    cmd->data.draw.count = 1;

    texture_vertex *vertices = (texture_vertex *)pool_memalign(
            data,
            4 * sizeof(texture_vertex), // 4 vertices
            sizeof(texture_vertex));

    cmd->data.draw.first = (size_t)vertices;
    cmd->data.draw.texture = texture;

    float u0 = (float)srcrect->x / (float)texture->w;
    float v0 = (float)srcrect->y / (float)texture->h;
    float u1 = (float)(srcrect->x + srcrect->w) / (float)texture->w;
    float v1 = (float)(srcrect->y + srcrect->h) / (float)texture->h;

    if (flip & SDL_FLIP_VERTICAL) {
        Swap(&v0, &v1);
    }

    if (flip & SDL_FLIP_HORIZONTAL) {
        Swap(&u0, &u1);
    }

    const float centerx = center->x;
    const float centery = center->y;
    const float x = dstrect->x + centerx;
    const float y = dstrect->y + centery;
    const float width = dstrect->w - centerx;
    const float height = dstrect->h - centery;
    float s, c;

    MathSincos(degToRad(angle), &s, &c);

    const float cw = c * width;
    const float sw = s * width;
    const float ch = c * height;
    const float sh = s * height;

    vertices[0].x = x - cw + sh;
    vertices[0].y = y - sw - ch;
    vertices[0].z = +0.5f;
    vertices[0].u = u0;
    vertices[0].v = v0;

    vertices[1].x = x + cw + sh;
    vertices[1].y = y + sw - ch;
    vertices[1].z = +0.5f;
    vertices[1].u = u1;
    vertices[1].v = v0;


    vertices[2].x = x - cw - sh;
    vertices[2].y = y - sw + ch;
    vertices[2].z = +0.5f;
    vertices[2].u = u0;
    vertices[2].v = v1;

    vertices[3].x = x + cw - sh;
    vertices[3].y = y + sw + ch;
    vertices[3].z = +0.5f;
    vertices[3].u = u1;
    vertices[3].v = v1;

    return 0;
}


static int
VITA_GXM_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    float clear_color[4];
    clear_color[0] = (cmd->data.color.r)/255.0f;
    clear_color[1] = (cmd->data.color.g)/255.0f;
    clear_color[2] = (cmd->data.color.b)/255.0f;
    clear_color[3] = (cmd->data.color.a)/255.0f;

    // set clear shaders
    sceGxmSetVertexProgram(data->gxm_context, data->clearVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->clearFragmentProgram);

    // set the clear color
    void *color_buffer;
    sceGxmReserveFragmentDefaultUniformBuffer(data->gxm_context, &color_buffer);
    sceGxmSetUniformDataF(color_buffer, data->clearClearColorParam, 0, 4, clear_color);

    // draw the clear triangle
    sceGxmSetVertexStream(data->gxm_context, 0, data->clearVertices);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, 3);

    return 0;
}


static int
VITA_GXM_RenderDrawPoints(SDL_Renderer *renderer, const SDL_RenderCommand *cmd)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    sceGxmSetVertexProgram(data->gxm_context, data->colorVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->colorFragmentProgram);

    void *vertexDefaultBuffer;
    sceGxmReserveVertexDefaultUniformBuffer(data->gxm_context, &vertexDefaultBuffer);
    sceGxmSetUniformDataF(vertexDefaultBuffer, data->colorWvpParam, 0, 16, data->ortho_matrix);

    sceGxmSetVertexStream(data->gxm_context, 0, (const void*)cmd->data.draw.first);

    sceGxmSetFrontPolygonMode(data->gxm_context, SCE_GXM_POLYGON_MODE_POINT);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_POINTS, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, cmd->data.draw.count);
    sceGxmSetFrontPolygonMode(data->gxm_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);

    return 0;
}

static int
VITA_GXM_RenderDrawLines(SDL_Renderer *renderer, const SDL_RenderCommand *cmd)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    sceGxmSetVertexProgram(data->gxm_context, data->colorVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->colorFragmentProgram);

    void *vertexDefaultBuffer;

    sceGxmReserveVertexDefaultUniformBuffer(data->gxm_context, &vertexDefaultBuffer);

    sceGxmSetUniformDataF(vertexDefaultBuffer, data->colorWvpParam, 0, 16, data->ortho_matrix);

    sceGxmSetVertexStream(data->gxm_context, 0, (const void*)cmd->data.draw.first);

    sceGxmSetFrontPolygonMode(data->gxm_context, SCE_GXM_POLYGON_MODE_LINE);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_LINES, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, cmd->data.draw.count);
    sceGxmSetFrontPolygonMode(data->gxm_context, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
    return 0;
}


static int
VITA_GXM_RenderFillRects(SDL_Renderer *renderer, const SDL_RenderCommand *cmd)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    sceGxmSetVertexProgram(data->gxm_context, data->colorVertexProgram);
    sceGxmSetFragmentProgram(data->gxm_context, data->colorFragmentProgram);

    void *vertexDefaultBuffer;
    sceGxmReserveVertexDefaultUniformBuffer(data->gxm_context, &vertexDefaultBuffer);
    sceGxmSetUniformDataF(vertexDefaultBuffer, data->colorWvpParam, 0, 16, data->ortho_matrix);

    sceGxmSetVertexStream(data->gxm_context, 0, (const void*)cmd->data.draw.first);
    sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, 4 * cmd->data.draw.count);

    return 0;
}



static int
VITA_GXM_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    StartDrawing(renderer);
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR: {
                break;
            }

            case SDL_RENDERCMD_SETVIEWPORT: {
                // TODO
                break;
            }

            case SDL_RENDERCMD_SETCLIPRECT: {
                const SDL_Rect *rect = &cmd->data.cliprect.rect;
                if (cmd->data.cliprect.enabled)
                {
                    set_clip_rectangle(data, rect->x, rect->y, rect->w, rect->h);
                }
                else
                {
                    unset_clip_rectangle(data);
                }
                break;
            }

            case SDL_RENDERCMD_CLEAR: {
                VITA_GXM_RenderClear(renderer, cmd);
                break;
            }

            case SDL_RENDERCMD_DRAW_POINTS: {
                VITA_GXM_SetBlendMode(renderer, cmd->data.draw.blend);
                VITA_GXM_RenderDrawPoints(renderer, cmd);
                break;
            }

            case SDL_RENDERCMD_DRAW_LINES: {
                VITA_GXM_SetBlendMode(renderer, cmd->data.draw.blend);
                VITA_GXM_RenderDrawLines(renderer, cmd);
                break;
            }

            case SDL_RENDERCMD_FILL_RECTS: {
                VITA_GXM_SetBlendMode(renderer, cmd->data.draw.blend);
                VITA_GXM_RenderFillRects(renderer, cmd);
                break;
            }

            case SDL_RENDERCMD_COPY:
            case SDL_RENDERCMD_COPY_EX: {
                SDL_BlendMode blend;
                SDL_GetTextureBlendMode(cmd->data.draw.texture, &blend);
                VITA_GXM_SetBlendMode(renderer, blend);

                Uint8 r, g, b, a;
                r = cmd->data.draw.r;
                g = cmd->data.draw.g;
                b = cmd->data.draw.b;
                a = cmd->data.draw.a;

                sceGxmSetVertexProgram(data->gxm_context, data->textureVertexProgram);

                if(r == 255 && g == 255 && b == 255 && a == 255)
                {
                    sceGxmSetFragmentProgram(data->gxm_context, data->textureFragmentProgram);
                }
                else
                {
                    sceGxmSetFragmentProgram(data->gxm_context, data->textureTintFragmentProgram);
                    void *texture_tint_color_buffer;
                    sceGxmReserveFragmentDefaultUniformBuffer(data->gxm_context, &texture_tint_color_buffer);

                    float *tint_color = pool_memalign(
                        data,
                        4 * sizeof(float), // RGBA
                        sizeof(float)
                    );

                    tint_color[0] = r / 255.0f;
                    tint_color[1] = g / 255.0f;
                    tint_color[2] = b / 255.0f;
                    tint_color[3] = a / 255.0f;

                    sceGxmSetUniformDataF(texture_tint_color_buffer, data->textureTintColorParam, 0, 4, tint_color);

                }

                void *vertex_wvp_buffer;
                sceGxmReserveVertexDefaultUniformBuffer(data->gxm_context, &vertex_wvp_buffer);
                sceGxmSetUniformDataF(vertex_wvp_buffer, data->textureWvpParam, 0, 16, data->ortho_matrix);

                VITA_GXM_TextureData *vita_texture = (VITA_GXM_TextureData *) cmd->data.draw.texture->driverdata;

                sceGxmSetFragmentTexture(data->gxm_context, 0, &vita_texture->tex->gxm_tex);

                sceGxmSetVertexStream(data->gxm_context, 0, (const void*)cmd->data.draw.first);
                sceGxmDraw(data->gxm_context, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, data->linearIndices, 4 * cmd->data.draw.count);

                break;
            }

            case SDL_RENDERCMD_NO_OP:
                break;
        }

        cmd = cmd->next;
    }

    sceGxmEndScene(data->gxm_context, NULL, NULL);
    data->drawing = SDL_FALSE;

    return 0;
}

static int
VITA_GXM_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
    Uint32 pixel_format, void *pixels, int pitch)
{
    SceDisplayFrameBuf framebuf;
    SDL_memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
    sceDisplayGetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_IMMEDIATE);

    // TODO
    //pixels = framebuf.base;

    return 0;

}


static void
VITA_GXM_RenderPresent(SDL_Renderer *renderer)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;

    sceGxmFinish(data->gxm_context);

    data->displayData.address = data->displayBufferData[data->backBufferIndex];

    sceGxmDisplayQueueAddEntry(
        data->displayBufferSync[data->frontBufferIndex],    // OLD fb
        data->displayBufferSync[data->backBufferIndex],     // NEW fb
        &data->displayData
    );

    // update buffer indices
    data->frontBufferIndex = data->backBufferIndex;
    data->backBufferIndex = (data->backBufferIndex + 1) % VITA_GXM_BUFFERS;
    data->pool_index = 0;
    data->drawing = SDL_FALSE;
}

static void
VITA_GXM_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    VITA_GXM_TextureData *vita_texture = (VITA_GXM_TextureData *) texture->driverdata;

    if (data == 0)
        return;

    if(vita_texture == 0)
        return;

    if(vita_texture->tex == 0)
        return;

    sceGxmFinish(data->gxm_context);

    if (vita_texture->tex->gxm_rendertarget) {
        sceGxmDestroyRenderTarget(vita_texture->tex->gxm_rendertarget);
    }

    if (vita_texture->tex->depth_UID) {
        mem_gpu_free(vita_texture->tex->depth_UID);
    }

    if (vita_texture->tex->palette_UID) {
        mem_gpu_free(vita_texture->tex->palette_UID);
    }

    mem_gpu_free(vita_texture->tex->data_UID);
    SDL_free(vita_texture->tex);
    SDL_free(vita_texture);

    texture->driverdata = NULL;
}

static void
VITA_GXM_DestroyRenderer(SDL_Renderer *renderer)
{
    VITA_GXM_RenderData *data = (VITA_GXM_RenderData *) renderer->driverdata;
    if (data) {
        if (!data->initialized)
            return;

        gxm_finish(renderer);

        data->initialized = SDL_FALSE;
        data->drawing = SDL_FALSE;
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_VITA_GXM */

/* vi: set ts=4 sw=4 expandtab: */
