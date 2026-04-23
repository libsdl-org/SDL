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

#ifdef SDL_VIDEO_RENDER_PS3

#include "../SDL_sysrender.h"
#include "../../video/SDL_sysvideo.h"
#include "../../video/ps3/SDL_PS3video.h"

#include "../software/SDL_draw.h"
#include "../software/SDL_blendfillrect.h"
#include "../software/SDL_blendline.h"
#include "../software/SDL_blendpoint.h"
#include "../software/SDL_drawline.h"
#include "../software/SDL_drawpoint.h"

#include <rsx/gcm_sys.h>
#include <unistd.h>
#include <assert.h>
#include <sys/systime.h>
#include <rsx/mm.h>
#include <rsx/rsx.h>
#include <rsx/commands.h>

/* SDL surface based renderer implementation */

static bool PS3_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID create_props);
static void PS3_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event);
static bool PS3_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode);
static bool PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture, SDL_PropertiesID create_props);
static bool PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, const void *pixels,
                            int pitch);
static bool PS3_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect,
                     const Uint8 *Yplane, int Ypitch,
                     const Uint8 *Uplane, int Upitch,
                     const Uint8 *Vplane, int Vpitch);
static bool PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * rect, void **pixels, int *pitch);
static void PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static bool PS3_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
static bool PS3_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd);
static bool PS3_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd);
static void PS3_SetTextureScaleMode(SDL_ScaleMode scaleMode);
static bool PS3_UpdateViewport(SDL_Renderer * renderer);
static bool PS3_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count);
static bool PS3_QueueFillRects(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects, int count);
static bool PS3_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect);
static bool PS3_QueueCopyEx(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                           const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                           const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y);
static bool PS3_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);
static SDL_Surface * PS3_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect);
static bool PS3_RenderPresent(SDL_Renderer * renderer);
static void PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void PS3_DestroyRenderer(SDL_Renderer * renderer);

SDL_RenderDriver PS3_RenderDriver = {
    PS3_CreateRenderer, "PS3"
};

#define NUM_BUFFERS 2

typedef struct
{
    const SDL_Rect *viewport;
    const SDL_Rect *cliprect;
    bool surface_cliprect_dirty;
    SDL_Color color;
} PS3_DrawStateCache;

typedef struct
{
    int current_screen;
    SDL_Surface *screens[NUM_BUFFERS];
    void *textures[NUM_BUFFERS];
    gcmContextData *context; // Context to keep track of the RSX buffer.
} PS3_RenderData;

typedef struct
{
    SDL_FRect srcRect;
    SDL_FRect dstRect;
} PS3_CopyData;

static SDL_Surface *PS3_ActivateRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    return data->screens[data->current_screen];
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
    int pitch;
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

    data = (PS3_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return false;
    }
    
    SDL_zerop(data);
    rsxHeapInit();

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
    data->current_screen = 0;
    
    pitch = displayMode->w * SDL_BYTESPERPIXEL(displayMode->format);
    // pitch = (displayMode->w * 4 + 63) & ~63;

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        deprintf (1,  "\t\tAllocate RSX memory for pixels\n");

        /* Allocate RSX memory for pixels */
        data->textures[i] = rsxMemalign(64, displayMode->h * pitch);
        if (!data->textures[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return false;
        }

        deprintf (1,  "\t\tSDL_CreateRGBSurfaceFrom( w: %d, h: %d)\n", displayMode->w, displayMode->h);
        data->screens[i] =
            SDL_CreateSurfaceFrom(displayMode->w, displayMode->h, displayMode->format, data->textures[i], pitch);

        if (!data->screens[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            return false;
        }

        u32 offset = 0;
        if (gcmAddressToOffset(data->screens[i]->pixels, &offset) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return false;
        }
        
        deprintf (1,  "\t\tSetup the display buffers\n");
        // Setup the display buffers
        if (gcmSetDisplayBuffer(i, offset, data->screens[i]->pitch, data->screens[i]->w, data->screens[i]->h) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return false;
        }
    }

    // gcmSetFlipMode(GCM_FLIP_VSYNC);
    // Needs to be called once at init before the render loop starts.
    gcmResetFlipStatus();

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
    renderer->QueueCopy = PS3_QueueCopy;
    renderer->QueueCopyEx = PS3_QueueCopyEx;
    renderer->RunCommandQueue = PS3_RunCommandQueue;
    renderer->RenderReadPixels = PS3_RenderReadPixels;
    renderer->RenderPresent = PS3_RenderPresent;
    renderer->DestroyTexture = PS3_DestroyTexture;
    renderer->DestroyRenderer = PS3_DestroyRenderer;

    // // Enable alpha blending globally
    // rsxSetBlendFunc(data->context,
    //                GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
    //                GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    // rsxSetBlendEquation(data->context, GCM_FUNC_ADD, GCM_FUNC_ADD);
    // rsxSetBlendEnable(data->context, GCM_TRUE);

    SDL_AddSupportedTextureFormat(renderer, SDL_PIXELFORMAT_ABGR8888);
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 1024);

    PS3_UpdateViewport(renderer);
    PS3_ActivateRenderer(renderer);
    
    return true;
}

static void PS3_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
}

static bool PS3_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
    return false;
}

static bool PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture, SDL_PropertiesID create_props)
{
    int pitch;
    void *pixels;

    // Allocate GFX memory for textures
    pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
    // pitch = (texture->w * 4 + 63) & ~63;
    pixels = rsxMemalign(64, texture->h * pitch);
    if (!pixels) {
        return SDL_SetError("rsxMemalign failed");
    }

    texture->internal = SDL_CreateSurfaceFrom(texture->w, texture->h, texture->format, pixels, pitch);

    SDL_SetSurfaceColorMod(texture->internal, (Uint8)texture->color.r, (Uint8)texture->color.g,
                           (Uint8)texture->color.b);
    SDL_SetSurfaceAlphaMod(texture->internal, (Uint8)texture->color.a);
    SDL_SetSurfaceBlendMode(texture->internal, texture->blendMode);

    return true;
}

static bool PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{
    SDL_Surface *surface = (SDL_Surface *) texture->internal;

    if(SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    Uint8 *src = (Uint8 *) pixels;
    Uint8 *dst = (Uint8 *) surface->pixels
                 + rect->y * surface->pitch
                 + rect->x * surface->fmt->bytes_per_pixel;
    size_t length = rect->w * surface->fmt->bytes_per_pixel;
    for (int row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += surface->pitch;
    }

    if(SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);

    return true;
}

static bool PS3_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect,
                     const Uint8 *Yplane, int Ypitch,
                     const Uint8 *Uplane, int Upitch,
                     const Uint8 *Vplane, int Vpitch)
{
    return false;
}

static bool PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * rect, void **pixels, int *pitch)
{
    SDL_Surface *surface = (SDL_Surface *) texture->internal;

    *pixels =
        (void *) ((Uint8 *) surface->pixels + rect->y * surface->pitch +
                  rect->x * surface->fmt->bytes_per_pixel);
    *pitch = surface->pitch;
    return true;
}

static void PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
}

static bool PS3_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return true;
}

static bool PS3_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return true;
}

static bool PS3_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return true;
}

static void PS3_SetTextureScaleMode(SDL_ScaleMode scaleMode)
{
    // switch (scaleMode) {
    // case SDL_SCALEMODE_PIXELART:
    // case SDL_SCALEMODE_NEAREST:
    //     break;
    // case SDL_SCALEMODE_LINEAR:
    //     break;
    // default:
    //     break;
    // }
}

static bool PS3_UpdateViewport(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;
    SDL_Surface *surface = data->screens[0];

    if (!renderer->last_queued_viewport.w && !renderer->last_queued_viewport.h) {
        /* There may be no window, so update the viewport directly */
        renderer->last_queued_viewport.w = surface->w;
        renderer->last_queued_viewport.h = surface->h;
    }

    /* Center drawable region on screen */
    if (renderer->window && surface->w > renderer->window->w) {
        renderer->last_queued_viewport.x += (surface->w - renderer->window->w)/2;
    }
    if (renderer->window && surface->h > renderer->window->h) {
        renderer->last_queued_viewport.y += (surface->h - renderer->window->h)/2;
    }
    
    SDL_SetSurfaceClipRect(data->screens[0], &renderer->last_queued_viewport);
    SDL_SetSurfaceClipRect(data->screens[1], &renderer->last_queued_viewport);
    return true;
}

static bool PS3_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    SDL_Point *verts = (SDL_Point *)SDL_AllocateRenderVertices(renderer, count * sizeof(SDL_Point), 0, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return false;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, verts++, points++) {
        verts->x = (int)points->x;
        verts->y = (int)points->y;
    }

    return true;
}

static bool PS3_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    SDL_Rect *verts = (SDL_Rect *)SDL_AllocateRenderVertices(renderer, count * sizeof(SDL_Rect), 0, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return false;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, verts++, rects++) {
        verts->x = (int)rects->x;
        verts->y = (int)rects->y;
        verts->w = SDL_max((int)rects->w, 1);
        verts->h = SDL_max((int)rects->h, 1);
    }

    return true;
}

static bool PS3_QueueCopy(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect)
{
    const size_t outLen = sizeof (PS3_CopyData);
    PS3_CopyData *outData = (PS3_CopyData *) SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);

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
    return true;
}

static bool PS3_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    // PS3_RenderData *data = (PS3_RenderData *)renderer->internal;
    PS3_DrawStateCache drawstate = {NULL, NULL, true, {0, 0, 0, 255}};
    drawstate.viewport = NULL;

    if (!surface) {
        return false;
    }

    Uint8 r, g, b, a;
    while (cmd) {
        r = drawstate.color.r;
        g = drawstate.color.g;
        b = drawstate.color.b;
        a = drawstate.color.a;

        switch (cmd->command) {
        case SDL_RENDERCMD_GEOMETRY:
            break;

        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            drawstate.color = (SDL_Color) {
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.r * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.g * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.b * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.a, 0.0f, 1.0f) * 255.0f)
            };
            break;
        }

        case SDL_RENDERCMD_SETVIEWPORT:
            drawstate.viewport = &cmd->data.viewport.rect;
            // data->drawstate.surface_cliprect_dirty = true;
            break;

        case SDL_RENDERCMD_SETCLIPRECT:
            drawstate.cliprect = cmd->data.cliprect.enabled ? &cmd->data.cliprect.rect : NULL;
            // data->drawstate.surface_cliprect_dirty = true;
            break;

        case SDL_RENDERCMD_CLEAR:
            /* By definition the clear ignores the clip rect */
            SDL_SetSurfaceClipRect(surface, NULL);
            SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface,
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.r * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.g * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.b * cmd->data.color.color_scale, 0.0f, 1.0f) * 255.0f),
                (Uint8)SDL_roundf(SDL_clamp(cmd->data.color.color.a, 0.0f, 1.0f) * 255.0f)
            ));
            SDL_SetSurfaceClipRect(surface, &surface->clip_rect);
            break;

        case SDL_RENDERCMD_DRAW_POINTS:
        {
            SDL_Point *verts = (SDL_Point *) (((Uint8 *) vertices) + cmd->data.draw.first);
            /* Apply viewport */
            if (drawstate.viewport && (drawstate.viewport->x || drawstate.viewport->y)) {
                for (int i = 0; i < cmd->data.draw.count; i++) {
                    verts[i].x += drawstate.viewport->x;
                    verts[i].y += drawstate.viewport->y;
                }
            }

            /* Draw the points! */
            if (renderer->blendMode == SDL_BLENDMODE_NONE) {
                SDL_DrawPoints(surface, verts, cmd->data.draw.count, SDL_MapRGBA(surface->fmt, NULL, r, g, b, a));
            } else {
                SDL_BlendPoints(surface, verts, cmd->data.draw.count, renderer->blendMode, r, g, b, a);
            }
            break;
        }

        case SDL_RENDERCMD_DRAW_LINES:
        {
            SDL_Point *verts = (SDL_Point *) (((Uint8 *) vertices) + cmd->data.draw.first);

            if (drawstate.viewport && (drawstate.viewport->x || drawstate.viewport->y)) {
                for (int i = 0; i < cmd->data.draw.count; i++) {
                    verts[i].x += drawstate.viewport->x;
                    verts[i].y += drawstate.viewport->y;
                }
            }
            /* Draw the lines! */
            if (renderer->blendMode == SDL_BLENDMODE_NONE) {
                SDL_DrawLines(surface, verts, cmd->data.draw.count, SDL_MapRGBA(surface->fmt, NULL, r, g, b, a));
            } else {
                SDL_BlendLines(surface, verts, cmd->data.draw.count, renderer->blendMode, r, g, b, a);
            }
            break;
        }

        case SDL_RENDERCMD_FILL_RECTS:
        {
            SDL_Rect *rects = (SDL_Rect *) (((Uint8 *) vertices) + cmd->data.draw.first);

            if (drawstate.viewport && (drawstate.viewport->x || drawstate.viewport->y)) {
                for (int i = 0; i < cmd->data.draw.count; i++) {
                    rects[i].x += drawstate.viewport->x;
                    rects[i].y += drawstate.viewport->y;
                }
            }

            if (renderer->blendMode == SDL_BLENDMODE_NONE) {
                 SDL_FillSurfaceRects(surface, rects, cmd->data.draw.count, SDL_MapRGBA(surface->fmt, NULL, r, g, b, a));
            } else {
                 SDL_BlendFillRects(surface, rects, cmd->data.draw.count, renderer->blendMode, r, g, b, a);
            }
            break;
        }

        case SDL_RENDERCMD_COPY:
        {
            SDL_Surface *surface_src = (SDL_Surface *) cmd->data.draw.texture->internal;

            PS3_CopyData copyData;// = (PS3_CopyData *) (((Uint8 *) vertices) + cmd->data.draw.first);
            SDL_memcpy(&copyData, ((Uint8 *)vertices) + cmd->data.draw.first, sizeof(PS3_CopyData));

            PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

            SDL_FRect *srcrect = &copyData.srcRect;
            SDL_FRect *dstrect = &copyData.dstRect;

            // Convert to integers
            int dstx = (int)SDL_floorf(dstrect->x);
            int dsty = (int)SDL_floorf(dstrect->y);
            int dstw = (int)SDL_floorf(dstrect->w);
            int dsth = (int)SDL_floorf(dstrect->h);

            // Skip if completely offscreen
            if (dstx + dstw <= 0 ||
                dsty + dsth <= 0 ||
                dstx >= surface->w ||
                dsty >= surface->h) {
                break;
            }
            u32 src_offset, dst_offset;

            gcmAddressToOffset(surface->pixels, &dst_offset);
            gcmAddressToOffset(surface_src->pixels, &src_offset);

            // Apply viewport
            if (drawstate.viewport && (drawstate.viewport->x || drawstate.viewport->y)) {
                dstrect->x += drawstate.viewport->x;
                dstrect->y += drawstate.viewport->y;
            }

            int srcx = (int)SDL_floorf(srcrect->x);
            int srcy = (int)SDL_floorf(srcrect->y);
            int srcw = (int)SDL_floorf(srcrect->w);
            int srch = (int)SDL_floorf(srcrect->h);

            gcmTransferScale scale;
            scale.conversion = GCM_TRANSFER_CONVERSION_TRUNCATE;
            scale.format = GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
            scale.operation = GCM_TRANSFER_OPERATION_SRCCOPY;
            scale.conversion = GCM_TRANSFER_CONVERSION_TRUNCATE;
            scale.origin = GCM_TRANSFER_ORIGIN_CORNER;
            scale.clipX = (s16)dstx;
            scale.clipY = (s16)dsty;
            scale.clipW = (u16)dstw;
            scale.clipH = (u16)dsth;
            scale.outX = (s16)dstx;
            scale.outY = (s16)dsty;
            scale.outW = (u16)dstw;
            scale.outH = (u16)dsth;
            scale.ratioX = rsxGetFixedSint32(dstw/srcw);
            scale.ratioY = rsxGetFixedSint32(dsth/srch);
            scale.inX = rsxGetFixedUint16(srcx);
            scale.inY = rsxGetFixedUint16(srcy);
            scale.inW = (u16)(srcw & ~1);
            scale.inH = (u16)srch;
            scale.offset = src_offset;
            scale.pitch = surface_src->pitch;

            gcmTransferSurface gcm_surface;
            gcm_surface.format = GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
            gcm_surface.pitch = surface->pitch;
            gcm_surface.offset = dst_offset;

            // Fix offscreen drawing
            if (dstx < 0 || dstx + dstw > (s32)surface->w) {
                scale.clipX = SDL_max(dstx, 0);
                scale.outX = scale.clipX;
                scale.clipW = SDL_min(dstw + SDL_min(dstx, 0), surface->w - scale.clipX);
                scale.outW = scale.clipW;
                if (dstx < 0) {
                    scale.inX = rsxGetFixedUint16(srcx + (SDL_min(dstx, 0) * -1));
                }
            }
            if (dsty < 0 || dsty + dsth > (s32)surface->h) {
                scale.clipY = SDL_max(dsty, 0);
                scale.outY = scale.clipY;
                scale.clipH = SDL_min(dsth + SDL_min(dsty, 0), surface->h - scale.clipY);
                scale.outH = scale.clipH;
                if (dsty < 0) {
                    scale.inY = rsxGetFixedUint16(srcy + (SDL_min(dsty, 0) * -1));
                }
            }

            // Hardware accelerated blit with scaling
            rsxSetTransferScaleMode(data->context, GCM_TRANSFER_LOCAL_TO_LOCAL, GCM_TRANSFER_SURFACE);
            rsxSetTransferScaleSurface(data->context, &scale, &gcm_surface);
            break;
        }

        case SDL_RENDERCMD_COPY_EX:
        {
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
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    SDL_Rect final_rect;

    if (!surface) {
        return NULL;
    }

    if (renderer->last_queued_viewport.x || renderer->last_queued_viewport.y) {
        final_rect.x = renderer->last_queued_viewport.x + rect->x;
        final_rect.y = renderer->last_queued_viewport.y + rect->y;
        final_rect.w = rect->w;
        final_rect.h = rect->h;
        rect = &final_rect;
    }

    if (rect->x < 0 || rect->x+rect->w > surface->w ||
        rect->y < 0 || rect->y+rect->h > surface->h) {
        SDL_SetError("Tried to read outside of surface bounds");
        return NULL;
    }

    SDL_PixelFormat src_format = surface->format;
    void *src_pixels = (void*)((Uint8 *) surface->pixels +
                    rect->y * surface->pitch +
                    rect->x * surface->fmt->bits_per_pixel);

    // TODO: fix me
    // SDL_ConvertPixels(rect->w, rect->h,
    //                          src_format, src_pixels, surface->pitch,
    //                          format, pixels, pitch);

    return surface;
}

static bool PS3_RenderPresent(SDL_Renderer * renderer)
{

    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    gcmResetFlipStatus();
    gcmSetFlip(data->context, data->current_screen);
    rsxFlushBuffer(data->context);
    gcmSetWaitFlip(data->context);

    Uint64 timeout_timer = SDL_GetTicks();
    u32 res = gcmGetFlipStatus();
    while (res != 0)
    {
        sysUsleep(200);
        res = gcmGetFlipStatus();

        if (SDL_GetTicks() - timeout_timer >= 2000)
        {
            break;
        }
    }

    // Update the flipping chain, if any
    data->current_screen = (data->current_screen + 1) % 2;

    return true;
}

static void PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->internal;

    if (!surface) return;

    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    // TODO: Wait for the DMA transfer to complete
    rsxFinish(data->context, 1);
    rsxFree(surface->pixels);
    SDL_DestroySurface(surface);
}

static void PS3_DestroyRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    deprintf (1, "SDL_PS3_DestroyRenderer()\n");

    // stop RSX before exit
    gcmSetWaitFlip(data->context);
    rsxFinish(data->context, 1);

    if (data) {
        for (int i = 0; i < SDL_arraysize(data->textures); ++i) {
            if (data->screens[i]) {
               SDL_DestroySurface(data->screens[i]);
            }
            if (data->textures[i]) {
                rsxFinish(data->context, 1);
                rsxFree(data->textures[i]);
            }
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_PS3 */

/* vi: set ts=4 sw=4 expandtab: */
