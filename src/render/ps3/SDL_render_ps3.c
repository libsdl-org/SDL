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
static bool PS3_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_Rect * dstrect);
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

typedef struct
{
    const SDL_Rect *viewport;
    const SDL_Rect *cliprect;
    bool surface_cliprect_dirty;
    SDL_Color color;
} PS3_DrawStateCache;

typedef struct
{
    bool first_fb; // Is this the first flip ?
    int current_screen;
    SDL_Surface *screens[3];
    void *textures[3];
    gcmContextData *context; // Context to keep track of the RSX buffer.
} PS3_RenderData;

typedef struct
{
    SDL_Rect    srcRect;
    SDL_Rect   dstRect;
} PS3_CopyData;


static void waitFlip()
{
    while (gcmGetFlipStatus() != 0)
      sysUsleep(200);
    gcmResetFlipStatus();
}

static SDL_Surface *PS3_ActivateRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    return data->screens[data->current_screen];
}

static bool PS3_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID create_props)
{
    PS3_RenderData *data;
    
    SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);

    const SDL_DisplayMode *displayMode = SDL_GetCurrentDisplayMode(displayID);

    int i, n;
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

    SDL_DeviceData *devdata = (SDL_DeviceData *)videoDevice->internal;

    if (!devdata->_CommandBuffer) {
        SDL_free(data);
        return false;
    }

    // Get a copy of the command buffer
    data->context = devdata->_CommandBuffer;
    data->current_screen = 0;
    data->first_fb = true;
    
    pitch = displayMode->w * SDL_BYTESPERPIXEL(displayMode->format);
    
    n = 2;

    for (i = 0; i < n; ++i) {
        deprintf (1,  "\t\tAllocate RSX memory for pixels\n");
        /* Allocate RSX memory for pixels */
        data->textures[i] = rsxMemalign(64, displayMode->h * pitch);
        if (!data->textures[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }

        memset(data->textures[i], 0, displayMode->h * pitch);

        deprintf (1,  "\t\tSDL_CreateRGBSurfaceFrom( w: %d, h: %d)\n", displayMode->w, displayMode->h);
        data->screens[i] =
            SDL_CreateSurfaceFrom(displayMode->w, displayMode->h, displayMode->format,data->textures[i], pitch);

        if (!data->screens[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            return NULL;
        }

        u32 offset = 0;
        if (gcmAddressToOffset(data->screens[i]->pixels, &offset) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }
        
        deprintf (1,  "\t\tSetup the display buffers\n");
        // Setup the display buffers
        if (gcmSetDisplayBuffer(i, offset, data->screens[i]->pitch, data->screens[i]->w, data->screens[i]->h) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    renderer->name = PS3_RenderDriver.name;
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
    // rsxSetBlendEnable(data->context, GCM_TRUE);
    // rsxSetBlendFunc(data->context,
    //                GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA,
    //                GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
    // rsxSetBlendEquation(data->context, GCM_FUNC_ADD, GCM_FUNC_ADD);

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
    int bpp;
    int pitch;
    void *pixels;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!SDL_GetMasksForPixelFormat
        (texture->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
        SDL_SetError("Unknown texture format");
        return false;
    }

    // Allocate GFX memory for textures
    pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
    pixels = rsxMemalign(64, texture->h * pitch);

    texture->internal =
        SDL_CreateSurfaceFrom(texture->w, texture->h, texture->format, pixels, pitch);

    SDL_SetSurfaceColorMod(texture->internal, (Uint8)texture->color.r, (Uint8)texture->color.g,
                           (Uint8)texture->color.b);
    SDL_SetSurfaceAlphaMod(texture->internal, (Uint8)texture->color.a);
    SDL_SetSurfaceBlendMode(texture->internal, texture->blendMode);

    if (!texture->internal) {
        return false;
    }
    return true;
}

static bool PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{
    SDL_Surface *surface = (SDL_Surface *) texture->internal;
    Uint8 *src, *dst;
    int row;
    size_t length;

    src = (Uint8 *) pixels;
    dst = (Uint8 *) surface->pixels +
                        rect->y * surface->pitch +
                        rect->x * surface->fmt->bytes_per_pixel;
    length = rect->w * surface->fmt->bytes_per_pixel;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += surface->pitch;
    }
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

}

static bool PS3_UpdateViewport(SDL_Renderer * renderer)
{
    printf("PS3_UpdateViewport  start\n"); fflush(stdout);
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
    printf("PS3_UpdateViewport  end\n"); fflush(stdout);
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
    
    SDL_memcpy(&outData->srcRect, srcrect, sizeof(SDL_Rect));
    
    outData->dstRect.x = (int)dstrect->x;
    outData->dstRect.y = (int)dstrect->y;
    outData->dstRect.w = (int)dstrect->w;
    outData->dstRect.h = (int)dstrect->h;

    return true;
}

static bool PS3_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;
    SDL_Surface *dst = PS3_ActivateRenderer(renderer);
    SDL_Surface *src = (SDL_Surface *) texture->internal;
    SDL_Rect final_rect = *dstrect;
    u32 src_offset, dst_offset;

    if (!dst) {
        return false;
    }

    if (renderer->last_queued_viewport.x || renderer->last_queued_viewport.y) {
        final_rect.x += renderer->last_queued_viewport.x;
        final_rect.y += renderer->last_queued_viewport.y;
    }

    gcmAddressToOffset(dst->pixels, &dst_offset);
    gcmAddressToOffset(src->pixels, &src_offset);

    gcmTransferScale scale;
    scale.conversion = GCM_TRANSFER_CONVERSION_TRUNCATE;
    scale.format = GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
    scale.operation = GCM_TRANSFER_OPERATION_SRCCOPY;
    scale.clipX = final_rect.x;
    scale.clipY = final_rect.y;
    scale.clipW = final_rect.w;
    scale.clipH = final_rect.h;
    scale.outX = final_rect.x;
    scale.outY = final_rect.y;
    scale.outW = final_rect.w;
    scale.outH = final_rect.h;
    scale.ratioX = (srcrect->w << 20) / final_rect.w;
    scale.ratioY = (srcrect->h << 20) / final_rect.h;
    scale.inX = srcrect->x;
    scale.inY = srcrect->y;
    scale.inW = srcrect->w;
    scale.inH = srcrect->h;
    scale.offset = src_offset;
    scale.pitch = src->pitch;
    scale.origin = GCM_TRANSFER_ORIGIN_CORNER;
    scale.interp = GCM_TRANSFER_INTERPOLATOR_LINEAR;

    gcmTransferSurface surface;
    surface.format = GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
    surface.pitch = dst->pitch;
    surface.offset = dst_offset;

    // Hardware accelerated blit with scaling
    rsxSetTransferScaleMode(data->context, GCM_TRANSFER_LOCAL_TO_LOCAL, GCM_TRANSFER_SURFACE);
    rsxSetTransferScaleSurface(data->context, &scale, &surface);

    // TODO: Blending / clipping
    // rsxSetBlendEnable(data->context, GCM_FALSE);
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
            const PS3_CopyData *copyData = (PS3_CopyData *) (((Uint8 *) vertices) + cmd->data.draw.first);
            PS3_RenderCopy(renderer, cmd->data.draw.texture, &copyData->srcRect, &copyData->dstRect);
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
    SDL_PixelFormat src_format;
    void *src_pixels;
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

    src_format = surface->format;
    src_pixels = (void*)((Uint8 *) surface->pixels +
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

    if (data->first_fb)
    {
        gcmResetFlipStatus();
    }

    gcmSetFlip(data->context, data->current_screen);
    rsxFlushBuffer(data->context);
    gcmSetWaitFlip(data->context);
    waitFlip();

    data->first_fb = false;

    // Update the flipping chain, if any
    data->current_screen = (data->current_screen + 1) % 2;

    return true;
}

static void PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->internal;

    if (!surface)
    {
        return;
    }

    // TODO: Wait for the DMA transfer to complete
    rsxFree(surface->pixels);
    SDL_DestroySurface(surface);
}

static void PS3_DestroyRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->internal;

    deprintf (1, "SDL_PS3_DestroyRenderer()\n");

    if (data) {
        for (int i = 0; i < SDL_arraysize(data->screens); ++i) {
            if (data->screens[i]) {
               SDL_DestroySurface(data->screens[i]);
            }
            if (data->textures[i]) {
                rsxFree(data->textures[i]);
            }
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_PS3 */

/* vi: set ts=4 sw=4 expandtab: */
