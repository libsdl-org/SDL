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

#ifdef SDL_VIDEO_RENDER_NGAGE

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef Int2Fix
#define Int2Fix(i) ((i) << 16)
#endif

#ifndef Fix2Int
#define Fix2Int(i) ((((unsigned int)(i) > 0xFFFF0000) ? 0 : ((i) >> 16)))
#endif

#ifndef Fix2Real
#define Fix2Real(i) ((i) / 65536.0)
#endif

#ifndef Real2Fix
#define Real2Fix(i) ((int)((i) * 65536.0))
#endif

#include "../SDL_sysrender.h"
#include "SDL_render_ngage_c.h"

static void NGAGE_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event);
static bool NGAGE_GetOutputSize(SDL_Renderer *renderer, int *w, int *h);
static bool NGAGE_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);
static bool NGAGE_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_PropertiesID create_props);
static bool NGAGE_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static bool NGAGE_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
static bool NGAGE_QueueDrawVertices(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count);
static bool NGAGE_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);
static bool NGAGE_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect);
static bool NGAGE_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcquad, const SDL_FRect *dstrect, const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y);
static bool NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices, float scale_x, float scale_y);

static void NGAGE_InvalidateCachedState(SDL_Renderer *renderer);
static bool NGAGE_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);
static bool NGAGE_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);

static bool NGAGE_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
static void NGAGE_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void NGAGE_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);
static bool NGAGE_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
static SDL_Surface *NGAGE_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect);
static bool NGAGE_RenderPresent(SDL_Renderer *renderer);
static void NGAGE_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);

static void NGAGE_DestroyRenderer(SDL_Renderer *renderer);

static bool NGAGE_SetVSync(SDL_Renderer *renderer, int vsync);

static bool NGAGE_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_SetupRendererColorspace(renderer, create_props);

    if (renderer->output_colorspace != SDL_COLORSPACE_RGB_DEFAULT) {
        return SDL_SetError("Unsupported output colorspace");
    }

    NGAGE_RendererData *phdata = SDL_calloc(1, sizeof(NGAGE_RendererData));
    if (!phdata) {
        SDL_OutOfMemory();
        return false;
    }

    renderer->WindowEvent = NGAGE_WindowEvent;
    renderer->GetOutputSize = NGAGE_GetOutputSize;
    renderer->SupportsBlendMode = NGAGE_SupportsBlendMode;
    renderer->CreateTexture = NGAGE_CreateTexture;
    renderer->QueueSetViewport = NGAGE_QueueSetViewport;
    renderer->QueueSetDrawColor = NGAGE_QueueSetDrawColor;
    renderer->QueueDrawPoints = NGAGE_QueueDrawVertices;
    renderer->QueueDrawLines = NGAGE_QueueDrawVertices;
    renderer->QueueFillRects = NGAGE_QueueFillRects;
    renderer->QueueCopy = NGAGE_QueueCopy;
    renderer->QueueCopyEx = NGAGE_QueueCopyEx;
    renderer->QueueGeometry = NGAGE_QueueGeometry;

    renderer->InvalidateCachedState = NGAGE_InvalidateCachedState;
    renderer->RunCommandQueue = NGAGE_RunCommandQueue;
    renderer->UpdateTexture = NGAGE_UpdateTexture;
    renderer->LockTexture = NGAGE_LockTexture;
    renderer->UnlockTexture = NGAGE_UnlockTexture;
    // renderer->SetTextureScaleMode = NGAGE_SetTextureScaleMode;
    renderer->SetRenderTarget = NGAGE_SetRenderTarget;
    renderer->RenderReadPixels = NGAGE_RenderReadPixels;
    renderer->RenderPresent = NGAGE_RenderPresent;
    renderer->DestroyTexture = NGAGE_DestroyTexture;

    renderer->DestroyRenderer = NGAGE_DestroyRenderer;

    renderer->SetVSync = NGAGE_SetVSync;

    renderer->name = NGAGE_RenderDriver.name;
    renderer->window = window;
    renderer->internal = phdata;

    SDL_AddSupportedTextureFormat(renderer, SDL_PIXELFORMAT_XRGB4444);
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 1024);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_LINE_METHOD, "2", SDL_HINT_OVERRIDE);

    return true;
}

SDL_RenderDriver NGAGE_RenderDriver = {
    NGAGE_CreateRenderer,
    "N-Gage"
};

static void NGAGE_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
    return;
}

static bool NGAGE_GetOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    return true;
}

static bool NGAGE_SupportsBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
    case SDL_BLENDMODE_MOD:
        return true;
    default:
        return false;
    }
}

static bool NGAGE_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture, SDL_PropertiesID create_props)
{
    NGAGE_TextureData *data = (NGAGE_TextureData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return false;
    }

    if (!NGAGE_CreateTextureData(data, texture->w, texture->h)) {
        SDL_free(data);
        return false;
    }

    SDL_Surface *surface = SDL_CreateSurface(texture->w, texture->h, texture->format);
    if (!surface) {
        SDL_free(data);
        return false;
    }

    data->surface = surface;
    texture->internal = data;

    return true;
}

static bool NGAGE_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    if (!cmd->data.viewport.rect.w && !cmd->data.viewport.rect.h) {
        SDL_Rect viewport = { 0, 0, NGAGE_SCREEN_WIDTH, NGAGE_SCREEN_HEIGHT };
        SDL_SetRenderViewport(renderer, &viewport);
    }

    return true;
}

static bool NGAGE_QueueSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return true;
}

static bool NGAGE_QueueDrawVertices(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    NGAGE_Vertex *verts = (NGAGE_Vertex *)SDL_AllocateRenderVertices(renderer, count * sizeof(NGAGE_Vertex), 0, &cmd->data.draw.first);
    if (!verts) {
        return false;
    }

    cmd->data.draw.count = count;

    for (int i = 0; i < count; i++, points++) {
        int fixed_x = Real2Fix(points->x);
        int fixed_y = Real2Fix(points->y);

        verts[i].x = Fix2Int(fixed_x);
        verts[i].y = Fix2Int(fixed_y);

        Uint32 color = NGAGE_ConvertColor(cmd->data.draw.color.r, cmd->data.draw.color.g, cmd->data.draw.color.b, cmd->data.draw.color.a, cmd->data.draw.color_scale);

        verts[i].color.a = (Uint8)(color >> 24);
        verts[i].color.b = (Uint8)(color >> 16);
        verts[i].color.g = (Uint8)(color >> 8);
        verts[i].color.r = (Uint8)color;
    }

    return true;
}

static bool NGAGE_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count)
{
    NGAGE_Vertex *verts = (NGAGE_Vertex *)SDL_AllocateRenderVertices(renderer, count * 2 * sizeof(NGAGE_Vertex), 0, &cmd->data.draw.first);
    if (!verts) {
        return false;
    }

    cmd->data.draw.count = count;

    for (int i = 0; i < count; i++, rects++) {
        verts[i * 2].x = Real2Fix(rects->x);
        verts[i * 2].y = Real2Fix(rects->y);
        verts[i * 2 + 1].x = Real2Fix(rects->w);
        verts[i * 2 + 1].y = Real2Fix(rects->h);

        verts[i * 2].x = Fix2Int(verts[i * 2].x);
        verts[i * 2].y = Fix2Int(verts[i * 2].y);
        verts[i * 2 + 1].x = Fix2Int(verts[i * 2 + 1].x);
        verts[i * 2 + 1].y = Fix2Int(verts[i * 2 + 1].y);

        Uint32 color = NGAGE_ConvertColor(cmd->data.draw.color.r, cmd->data.draw.color.g, cmd->data.draw.color.b, cmd->data.draw.color.a, cmd->data.draw.color_scale);

        verts[i * 2].color.a = (Uint8)(color >> 24);
        verts[i * 2].color.b = (Uint8)(color >> 16);
        verts[i * 2].color.g = (Uint8)(color >> 8);
        verts[i * 2].color.r = (Uint8)color;
    }

    return true;
}

static bool NGAGE_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect)
{
    SDL_Rect *verts = (SDL_Rect *)SDL_AllocateRenderVertices(renderer, 2 * sizeof(SDL_Rect), 0, &cmd->data.draw.first);

    if (!verts) {
        return false;
    }

    cmd->data.draw.count = 1;

    verts->x = (int)srcrect->x;
    verts->y = (int)srcrect->y;
    verts->w = (int)srcrect->w;
    verts->h = (int)srcrect->h;

    verts++;

    verts->x = (int)dstrect->x;
    verts->y = (int)dstrect->y;
    verts->w = (int)dstrect->w;
    verts->h = (int)dstrect->h;

    return true;
}

static bool NGAGE_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcquad, const SDL_FRect *dstrect, const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y)
{
    NGAGE_CopyExData *verts = (NGAGE_CopyExData *)SDL_AllocateRenderVertices(renderer, sizeof(NGAGE_CopyExData), 0, &cmd->data.draw.first);

    if (!verts) {
        return false;
    }

    cmd->data.draw.count = 1;

    verts->srcrect.x = (int)srcquad->x;
    verts->srcrect.y = (int)srcquad->y;
    verts->srcrect.w = (int)srcquad->w;
    verts->srcrect.h = (int)srcquad->h;
    verts->dstrect.x = (int)dstrect->x;
    verts->dstrect.y = (int)dstrect->y;
    verts->dstrect.w = (int)dstrect->w;
    verts->dstrect.h = (int)dstrect->h;

    verts->angle = Real2Fix(angle);
    verts->center.x = Real2Fix(center->x);
    verts->center.y = Real2Fix(center->y);
    verts->scale_x = Real2Fix(scale_x);
    verts->scale_y = Real2Fix(scale_y);

    verts->flip = flip;

    return true;
}

static bool NGAGE_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices, float scale_x, float scale_y)
{
    return true;
}

static void NGAGE_InvalidateCachedState(SDL_Renderer *renderer)
{
    return;
}

static bool NGAGE_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    NGAGE_RendererData *phdata = (NGAGE_RendererData *)renderer->internal;
    if (!phdata) {
        return false;
    }
    phdata->viewport = 0;

    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_NO_OP:
            break;
        case SDL_RENDERCMD_SETVIEWPORT:
            phdata->viewport = &cmd->data.viewport.rect;
            break;

        case SDL_RENDERCMD_SETCLIPRECT:
        {
            const SDL_Rect *rect = &cmd->data.cliprect.rect;

            if (cmd->data.cliprect.enabled) {
                NGAGE_SetClipRect(rect);
            }

            break;
        }

        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            break;
        }

        case SDL_RENDERCMD_CLEAR:
        {
            Uint32 color = NGAGE_ConvertColor(cmd->data.color.color.r, cmd->data.color.color.g, cmd->data.color.color.b, cmd->data.color.color.a, cmd->data.color.color_scale);

            NGAGE_Clear(color);
            break;
        }

        case SDL_RENDERCMD_DRAW_POINTS:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                for (int i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_DrawPoints(verts, count);
            break;
        }
        case SDL_RENDERCMD_DRAW_LINES:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                for (int i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_DrawLines(verts, count);
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS:
        {
            NGAGE_Vertex *verts = (NGAGE_Vertex *)(((Uint8 *)vertices) + cmd->data.draw.first);
            const int count = cmd->data.draw.count;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                for (int i = 0; i < count; i++) {
                    verts[i].x += phdata->viewport->x;
                    verts[i].y += phdata->viewport->y;
                }
            }

            NGAGE_FillRects(verts, count);
            break;
        }

        case SDL_RENDERCMD_COPY:
        {
            SDL_Rect *verts = (SDL_Rect *)(((Uint8 *)vertices) + cmd->data.draw.first);
            SDL_Rect *srcrect = verts;
            SDL_Rect *dstrect = verts + 1;
            SDL_Texture *texture = cmd->data.draw.texture;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                dstrect->x += phdata->viewport->x;
                dstrect->y += phdata->viewport->y;
            }

            NGAGE_Copy(renderer, texture, srcrect, dstrect);
            break;
        }

        case SDL_RENDERCMD_COPY_EX:
        {
            NGAGE_CopyExData *copydata = (NGAGE_CopyExData *)(((Uint8 *)vertices) + cmd->data.draw.first);
            SDL_Texture *texture = cmd->data.draw.texture;

            // Apply viewport.
            if (phdata->viewport && (phdata->viewport->x || phdata->viewport->y)) {
                copydata->dstrect.x += phdata->viewport->x;
                copydata->dstrect.y += phdata->viewport->y;
            }

            NGAGE_CopyEx(renderer, texture, copydata);
            break;
        }

        case SDL_RENDERCMD_GEOMETRY:
        {
            break;
        }
        }
        cmd = cmd->next;
    }

    return true;
}

static bool NGAGE_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->internal;

    SDL_Surface *surface = phdata->surface;
    Uint8 *src, *dst;
    int row;
    size_t length;

    if (SDL_MUSTLOCK(surface)) {
        if (!SDL_LockSurface(surface)) {
            return false;
        }
    }
    src = (Uint8 *)pixels;
    dst = (Uint8 *)surface->pixels +
          rect->y * surface->pitch +
          rect->x * surface->fmt->bytes_per_pixel;

    length = (size_t)rect->w * surface->fmt->bytes_per_pixel;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += surface->pitch;
    }
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    return true;
}

static bool NGAGE_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->internal;
    SDL_Surface *surface = phdata->surface;

    *pixels =
        (void *)((Uint8 *)surface->pixels + rect->y * surface->pitch +
                 rect->x * surface->fmt->bytes_per_pixel);
    *pitch = surface->pitch;
    return true;
}

static void NGAGE_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
}

static void NGAGE_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
}

static bool NGAGE_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return true;
}

static SDL_Surface *NGAGE_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    return (SDL_Surface *)0;
}

static bool NGAGE_RenderPresent(SDL_Renderer *renderer)
{
    NGAGE_Flip();

    return true;
}

static void NGAGE_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    NGAGE_TextureData *data = (NGAGE_TextureData *)texture->internal;
    if (data) {
        SDL_DestroySurface(data->surface);
        NGAGE_DestroyTextureData(data);
        SDL_free(data);
        texture->internal = 0;
    }
}

static void NGAGE_DestroyRenderer(SDL_Renderer *renderer)
{
    NGAGE_RendererData *phdata = (NGAGE_RendererData *)renderer->internal;
    if (phdata) {
        SDL_free(phdata);
        renderer->internal = 0;
    }
}

static bool NGAGE_SetVSync(SDL_Renderer *renderer, int vsync)
{
    return true;
}

#endif // SDL_VIDEO_RENDER_NGAGE
