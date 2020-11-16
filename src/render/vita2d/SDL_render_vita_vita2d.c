/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_RENDER_VITA_VITA2D

#include "SDL_hints.h"
#include "../SDL_sysrender.h"

#include <psp2/types.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <vita2d.h>

#define sceKernelDcacheWritebackAll() (void)0

/* VITA renderer implementation, based on the vita2d lib  */

extern int SDL_RecreateWindow(SDL_Window *window, Uint32 flags);

static SDL_Renderer *VITA_VITA2D_CreateRenderer(SDL_Window *window, Uint32 flags);
static void VITA_VITA2D_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event);
static SDL_bool VITA_VITA2D_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode);
static int VITA_VITA2D_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static int VITA_VITA2D_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, const void *pixels, int pitch);
static int VITA_VITA2D_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect,
                     const Uint8 *Yplane, int Ypitch,
                     const Uint8 *Uplane, int Upitch,
                     const Uint8 *Vplane, int Vpitch);

static int VITA_VITA2D_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, void **pixels, int *pitch);

static void VITA_VITA2D_UnlockTexture(SDL_Renderer *renderer,
     SDL_Texture *texture);

static void VITA_VITA2D_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode);

static int VITA_VITA2D_SetRenderTarget(SDL_Renderer *renderer,
         SDL_Texture *texture);

static int VITA_VITA2D_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd);

static int VITA_VITA2D_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd);

static int VITA_VITA2D_RenderClear(SDL_Renderer *renderer);

static int VITA_VITA2D_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count);

static int VITA_VITA2D_RenderDrawPoints(SDL_Renderer *renderer,
        const SDL_FPoint *points, int count);

static int VITA_VITA2D_RenderDrawLines(SDL_Renderer *renderer,
        const SDL_FPoint *points, int count);

static int VITA_VITA2D_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count);

static int VITA_VITA2D_RenderFillRects(SDL_Renderer *renderer,
        const SDL_FRect *rects, int count);

static int VITA_VITA2D_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                            const SDL_Rect * srcrect, const SDL_FRect * dstrect);

static int VITA_VITA2D_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);

static int VITA_VITA2D_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
    Uint32 pixel_format, void *pixels, int pitch);

static int VITA_VITA2D_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                            const SDL_Rect * srcquad, const SDL_FRect * dstrect,
                            const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);

static void VITA_VITA2D_RenderPresent(SDL_Renderer *renderer);
static void VITA_VITA2D_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void VITA_VITA2D_DestroyRenderer(SDL_Renderer *renderer);


SDL_RenderDriver VITA_VITA2D_RenderDriver = {
    .CreateRenderer = VITA_VITA2D_CreateRenderer,
    .info = {
        .name = "VITA",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC,
        .num_texture_formats = 1,
        .texture_formats = {
        [0] = SDL_PIXELFORMAT_ABGR8888,
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
     }
};

#define VITA_VITA2D_SCREEN_WIDTH     960
#define VITA_VITA2D_SCREEN_HEIGHT    544

#define VITA_VITA2D_FRAME_BUFFER_WIDTH   1024
#define VITA_VITA2D_FRAME_BUFFER_SIZE    (VITA_VITA2D_FRAME_BUFFER_WIDTH*VITA_VITA2D_SCREEN_HEIGHT)

#define COL5650(r,g,b,a)    ((r>>3) | ((g>>2)<<5) | ((b>>3)<<11))
#define COL5551(r,g,b,a)    ((r>>3) | ((g>>3)<<5) | ((b>>3)<<10) | (a>0?0x7000:0))
#define COL4444(r,g,b,a)    ((r>>4) | ((g>>4)<<4) | ((b>>4)<<8) | ((a>>4)<<12))
#define COL8888(r,g,b,a)    ((r) | ((g)<<8) | ((b)<<16) | ((a)<<24))

typedef struct
{
    void          *frontbuffer;
    void          *backbuffer;
    SDL_bool      initialized;
    SDL_bool      displayListAvail;
    unsigned int  psm;
    unsigned int  bpp;
    SDL_bool      vsync;
    unsigned int  currentColor;
    int           currentBlendMode;

} VITA_VITA2D_RenderData;


typedef struct
{
    vita2d_texture  *tex;
    unsigned int    pitch;
    unsigned int    w;
    unsigned int    h;
} VITA_VITA2D_TextureData;

typedef struct
{
    SDL_Rect    srcRect;
    SDL_FRect   dstRect;
} VITA_VITA2D_CopyData;

void
StartDrawing(SDL_Renderer *renderer)
{
    VITA_VITA2D_RenderData *data = (VITA_VITA2D_RenderData *) renderer->driverdata;
    if(data->displayListAvail)
        return;

    vita2d_start_drawing();

    data->displayListAvail = SDL_TRUE;
}

SDL_Renderer *
VITA_VITA2D_CreateRenderer(SDL_Window *window, Uint32 flags)
{

    SDL_Renderer *renderer;
    VITA_VITA2D_RenderData *data;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (VITA_VITA2D_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        VITA_VITA2D_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    renderer->WindowEvent = VITA_VITA2D_WindowEvent;
    renderer->SupportsBlendMode = VITA_VITA2D_SupportsBlendMode;
    renderer->CreateTexture = VITA_VITA2D_CreateTexture;
    renderer->UpdateTexture = VITA_VITA2D_UpdateTexture;
    renderer->UpdateTextureYUV = VITA_VITA2D_UpdateTextureYUV;
    renderer->LockTexture = VITA_VITA2D_LockTexture;
    renderer->UnlockTexture = VITA_VITA2D_UnlockTexture;
    renderer->SetTextureScaleMode = VITA_VITA2D_SetTextureScaleMode;
    renderer->SetRenderTarget = VITA_VITA2D_SetRenderTarget;
    renderer->QueueSetViewport = VITA_VITA2D_QueueSetViewport;
    renderer->QueueSetDrawColor = VITA_VITA2D_QueueSetDrawColor;
    renderer->QueueDrawPoints = VITA_VITA2D_QueueDrawPoints;
    renderer->QueueDrawLines = VITA_VITA2D_QueueDrawPoints;  // lines and points queue the same way.
    renderer->QueueFillRects = VITA_VITA2D_QueueFillRects;
    renderer->QueueCopy = VITA_VITA2D_QueueCopy;
    renderer->QueueCopyEx = VITA_VITA2D_QueueCopyEx;
    renderer->RunCommandQueue = VITA_VITA2D_RunCommandQueue;
    renderer->RenderReadPixels = VITA_VITA2D_RenderReadPixels;
    renderer->RenderPresent = VITA_VITA2D_RenderPresent;
    renderer->DestroyTexture = VITA_VITA2D_DestroyTexture;
    renderer->DestroyRenderer = VITA_VITA2D_DestroyRenderer;
    renderer->info = VITA_VITA2D_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    renderer->driverdata = data;
    renderer->window = window;

    if (data->initialized != SDL_FALSE)
        return 0;
    data->initialized = SDL_TRUE;

    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        data->vsync = SDL_TRUE;
    } else {
        data->vsync = SDL_FALSE;
    }

    vita2d_init();
    vita2d_set_vblank_wait(data->vsync);

    return renderer;
}

static void
VITA_VITA2D_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{

}

static SDL_bool
VITA_VITA2D_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
    return SDL_FALSE;
}

static int
VITA_VITA2D_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_VITA2D_TextureData* vita_texture = (VITA_VITA2D_TextureData*) SDL_calloc(1, sizeof(*vita_texture));

    if(!vita_texture)
        return -1;

    vita_texture->tex = vita2d_create_empty_texture(texture->w, texture->h);

    if(!vita_texture->tex)
    {
        SDL_free(vita_texture);
        return SDL_OutOfMemory();
    }

    texture->driverdata = vita_texture;

    VITA_VITA2D_SetTextureScaleMode(renderer, texture, texture->scaleMode);

    vita_texture->w = vita2d_texture_get_width(vita_texture->tex);
    vita_texture->h = vita2d_texture_get_height(vita_texture->tex);
    vita_texture->pitch = vita2d_texture_get_stride(vita_texture->tex);

    return 0;
}


static int
VITA_VITA2D_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, const void *pixels, int pitch)
{
    const Uint8 *src;
    Uint8 *dst;
    int row, length,dpitch;
    src = pixels;

    VITA_VITA2D_LockTexture(renderer, texture, rect, (void **)&dst, &dpitch);
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

    sceKernelDcacheWritebackAll();
    return 0;
}

static int
VITA_VITA2D_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
    const SDL_Rect * rect,
    const Uint8 *Yplane, int Ypitch,
    const Uint8 *Uplane, int Upitch,
    const Uint8 *Vplane, int Vpitch)
{
    return 0;
}

static int
VITA_VITA2D_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
    const SDL_Rect *rect, void **pixels, int *pitch)
{
    VITA_VITA2D_TextureData *vita_texture = (VITA_VITA2D_TextureData *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) vita2d_texture_get_datap(vita_texture->tex)
            + (rect->y * vita_texture->pitch) + rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = vita_texture->pitch;
    return 0;
}

static void
VITA_VITA2D_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    // no needs to update texture data on ps vita. VITA_VITA2D_LockTexture
    // already return a pointer to the vita2d texture pixels buffer.
    // This really improve framerate when using lock/unlock.

    /*
    VITA_VITA2D_TextureData *vita_texture = (VITA_VITA2D_TextureData *) texture->driverdata;
    SDL_Rect rect;

    // We do whole texture updates, at least for now
    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;
    VITA_VITA2D_UpdateTexture(renderer, texture, &rect, vita_texture->data, vita_texture->pitch);
    */
}

static void
VITA_VITA2D_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
    VITA_VITA2D_TextureData *vita_texture = (VITA_VITA2D_TextureData *) texture->driverdata;

    /*
     set texture filtering according to scaleMode
     suported hint values are nearest (0, default) or linear (1)
     vitaScaleMode is either SCE_GXM_TEXTURE_FILTER_POINT (good for tile-map)
     or SCE_GXM_TEXTURE_FILTER_LINEAR (good for scaling)
     */

    int vitaScaleMode = (scaleMode == SDL_ScaleModeNearest
                        ? SCE_GXM_TEXTURE_FILTER_POINT
                        : SCE_GXM_TEXTURE_FILTER_LINEAR);
    vita2d_texture_set_filters(vita_texture->tex, vitaScaleMode, vitaScaleMode); 

    return;
}

static int
VITA_VITA2D_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0;
}

static int
VITA_VITA2D_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;
}

static int
VITA_VITA2D_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;
}


static void
VITA_VITA2D_SetBlendMode(SDL_Renderer *renderer, int blendMode)
{
    /*VITA_VITA2D_RenderData *data = (VITA_VITA2D_RenderData *) renderer->driverdata;
    if (blendMode != data-> currentBlendMode) {
        switch (blendMode) {
        case SDL_BLENDMODE_NONE:
                sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
                sceGuDisable(GU_BLEND);
            break;
        case SDL_BLENDMODE_BLEND:
                sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                sceGuEnable(GU_BLEND);
                sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
            break;
        case SDL_BLENDMODE_ADD:
                sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                sceGuEnable(GU_BLEND);
                sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00FFFFFF );
            break;
        case SDL_BLENDMODE_MOD:
                sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
                sceGuEnable(GU_BLEND);
                sceGuBlendFunc( GU_ADD, GU_FIX, GU_SRC_COLOR, 0, 0);
            break;
        }
        data->currentBlendMode = blendMode;
    }*/
}



static int
VITA_VITA2D_RenderClear(SDL_Renderer *renderer)
{
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    vita2d_set_clear_color(color);

    vita2d_clear_screen();

    return 0;
}

static int
VITA_VITA2D_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    const size_t vertlen = (sizeof (float) * 2) * count;
    float *verts = (float *) SDL_AllocateRenderVertices(renderer, vertlen, 0, &cmd->data.draw.first);
    if (!verts) {
        return -1;
    }
    cmd->data.draw.count = count;
    SDL_memcpy(verts, points, vertlen);
    return 0;
}

static int
VITA_VITA2D_RenderDrawPoints(SDL_Renderer *renderer, const SDL_FPoint *points,
    int count)
{
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;

    for (i = 0; i < count; ++i) {
        vita2d_draw_pixel(points[i].x, points[i].y, color);
    }

    return 0;
}

static int
VITA_VITA2D_RenderDrawLines(SDL_Renderer *renderer, const SDL_FPoint *points,
    int count)
{
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;

    for (i = 0; i < count; ++i) {
        if (i < count -1) {
            vita2d_draw_line(points[i].x, points[i].y, points[i+1].x, points[i+1].y, color);
        }
    }

    return 0;
}

static int
VITA_VITA2D_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    const size_t outLen = count * sizeof (SDL_FRect);
    SDL_FRect *outRects = (SDL_FRect *) SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);

    if (!outRects) {
        return -1;
    }
    cmd->data.draw.count = count;
    SDL_memcpy(outRects, rects, outLen);

    return 0;
}

static int
VITA_VITA2D_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects,
                     int count)
{
    int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
    int i;

    for (i = 0; i < count; ++i) {
        const SDL_FRect *rect = &rects[i];

        vita2d_draw_rectangle(rect->x, rect->y, rect->w, rect->h, color);
    }

    return 0;
}


#define PI   3.14159265358979f

#define radToDeg(x) ((x)*180.f/PI)
#define degToRad(x) ((x)*PI/180.f)

float MathAbs(float x)
{
    return (x < 0) ? -x : x;
}

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
VITA_VITA2D_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    const size_t outLen = sizeof (VITA_VITA2D_CopyData);
    VITA_VITA2D_CopyData *outData = (VITA_VITA2D_CopyData *) SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);

    if (!outData) {
        return -1;
    }
    cmd->data.draw.count = 1;

    SDL_memcpy(&outData->srcRect, srcrect, sizeof(SDL_Rect));
    SDL_memcpy(&outData->dstRect, dstrect, sizeof(SDL_FRect));

    Uint8 r, g, b, a;
    SDL_GetTextureColorMod(texture, &r, &g, &b);
    SDL_GetTextureAlphaMod(texture, &a);

    cmd->data.draw.r = r;
    cmd->data.draw.g = g;
    cmd->data.draw.b = b;
    cmd->data.draw.a = a;
    cmd->data.draw.blend = renderer->blendMode;

    return 0;
}


static int
VITA_VITA2D_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    StartDrawing(renderer);

    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR: {
                break;
            }

            case SDL_RENDERCMD_SETVIEWPORT: {
                break;
            }

            case SDL_RENDERCMD_SETCLIPRECT: {
                break;
            }

            case SDL_RENDERCMD_CLEAR: {
                VITA_VITA2D_RenderClear(renderer);
                break;
            }

            case SDL_RENDERCMD_DRAW_POINTS: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_FPoint *points = (SDL_FPoint *) (((Uint8 *) vertices) + first);
                VITA_VITA2D_RenderDrawPoints(renderer, points, count);
                break;
            }

            case SDL_RENDERCMD_DRAW_LINES: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_FPoint *points = (SDL_FPoint *) (((Uint8 *) vertices) + first);

                VITA_VITA2D_RenderDrawLines(renderer, points, count);
                break;
            }

            case SDL_RENDERCMD_FILL_RECTS: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_FRect *rects = (SDL_FRect *) (((Uint8 *) vertices) + first);

                VITA_VITA2D_RenderFillRects(renderer, rects, count);
                break;
            }

            case SDL_RENDERCMD_COPY: {
                const size_t first = cmd->data.draw.first;
                const VITA_VITA2D_CopyData *copyData = (VITA_VITA2D_CopyData *) (((Uint8 *) vertices) + first);

                VITA_VITA2D_TextureData *vita_texture = (VITA_VITA2D_TextureData *) cmd->data.draw.texture->driverdata;

                const SDL_Rect *srcrect = &copyData->srcRect;
                const SDL_FRect *dstrect = &copyData->dstRect;

                float scaleX = dstrect->w == srcrect->w ? 1 : (float)(dstrect->w/srcrect->w);
                float scaleY = dstrect->h == srcrect->h ? 1 : (float)(dstrect->h/srcrect->h);

                Uint8 r, g, b, a;
                r = cmd->data.draw.r;
                g = cmd->data.draw.g;
                b = cmd->data.draw.b;
                a = cmd->data.draw.a;

                VITA_VITA2D_SetBlendMode(renderer, cmd->data.draw.blend);

                if(r == 255 && g == 255 && b == 255 && a == 255)
                {
                    vita2d_draw_texture_part_scale(vita_texture->tex, dstrect->x, dstrect->y,
                        srcrect->x, srcrect->y, srcrect->w, srcrect->h, scaleX, scaleY);
                } else {
                    vita2d_draw_texture_tint_part_scale(vita_texture->tex, dstrect->x, dstrect->y,
                        srcrect->x, srcrect->y, srcrect->w, srcrect->h, scaleX, scaleY, (a << 24) + (b << 16) + (g << 8) + r);
                }

                break;
            }

            case SDL_RENDERCMD_COPY_EX: {
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
VITA_VITA2D_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
    Uint32 pixel_format, void *pixels, int pitch)
{
    return 0;
}

static int
VITA_VITA2D_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
    const SDL_Rect * srcquad, const SDL_FRect * dstrect,
    const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    return 0;
}

static void
VITA_VITA2D_RenderPresent(SDL_Renderer *renderer)
{
    VITA_VITA2D_RenderData *data = (VITA_VITA2D_RenderData *) renderer->driverdata;
    if(!data->displayListAvail)
        return;

    vita2d_end_drawing();
    vita2d_wait_rendering_done();
    vita2d_swap_buffers();

    data->displayListAvail = SDL_FALSE;
}

static void
VITA_VITA2D_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    VITA_VITA2D_RenderData *renderdata = (VITA_VITA2D_RenderData *) renderer->driverdata;
    VITA_VITA2D_TextureData *vita_texture = (VITA_VITA2D_TextureData *) texture->driverdata;

    if (renderdata == 0)
        return;

    if(vita_texture == 0)
        return;

    vita2d_wait_rendering_done();
    vita2d_free_texture(vita_texture->tex);
    SDL_free(vita_texture);
    texture->driverdata = NULL;
}

static void
VITA_VITA2D_DestroyRenderer(SDL_Renderer *renderer)
{
    VITA_VITA2D_RenderData *data = (VITA_VITA2D_RenderData *) renderer->driverdata;
    if (data) {
        if (!data->initialized)
            return;

        vita2d_fini();

        data->initialized = SDL_FALSE;
        data->displayListAvail = SDL_FALSE;
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_VITA_VITA2D */

/* vi: set ts=4 sw=4 expandtab: */

