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

#if SDL_VIDEO_RENDER_PS2

#include "../SDL_sysrender.h"
#include "SDL_hints.h"

#include <kernel.h>
#include <malloc.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <gsInline.h>

/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80)

typedef struct clear_vertex {
    float x;
    float y;
} clear_vertex;

typedef struct texture_vertex {
    float x;
    float y;
    float u;
    float v;
    uint64_t color;
} texture_vertex;

typedef struct color_vertex {
    float x;
    float y;
    uint64_t color;
} color_vertex;

typedef struct
{
    GSGLOBAL *gsGlobal;
    uint64_t drawColor;
    int32_t vsync_callback_id;
    SDL_bool vsync; /* wether we do vsync */
} PS2_RenderData;


static int vsync_sema_id = 0;

/* PRIVATE METHODS */
static int vsync_handler()
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame) WaitSema(vsync_sema_id);
   while (PollSema(vsync_sema_id) >= 0)
   	;
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2( gsGlobal->ScreenBuffer[
               gsGlobal->ActiveBuffer & 1] / 8192,
               gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );

         gsGlobal->ActiveBuffer ^= 1;
      }

   }

   gsKit_setactive(gsGlobal);
}

static int PixelFormatToPS2PSM(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_ABGR1555:
        return GS_PSM_CT16S;
    case SDL_PIXELFORMAT_BGR888:
        return GS_PSM_CT24;
    case SDL_PIXELFORMAT_ABGR8888:
        return GS_PSM_CT32;
    default:
        return GS_PSM_CT32;
    }
}

static void
PS2_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{

}

static int
PS2_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GSTEXTURE* ps2_tex = (GSTEXTURE*) SDL_calloc(1, sizeof(GSTEXTURE));

    if (!ps2_tex)
        return SDL_OutOfMemory();

    ps2_tex->Width = texture->w;
    ps2_tex->Height = texture->h;
    ps2_tex->PSM = PixelFormatToPS2PSM(texture->format);
    ps2_tex->Mem = memalign(128, gsKit_texture_size_ee(ps2_tex->Width, ps2_tex->Height, ps2_tex->PSM));

    if (!ps2_tex->Mem)
    {
        SDL_free(ps2_tex);
        return SDL_OutOfMemory();
    }

    texture->driverdata = ps2_tex;

    return 0;
}

static int
PS2_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, void **pixels, int *pitch)
{
    GSTEXTURE *ps2_texture = (GSTEXTURE *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) ps2_texture->Mem + rect->y * ps2_texture->Width * SDL_BYTESPERPIXEL(texture->format) +
                  rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = ps2_texture->Width * SDL_BYTESPERPIXEL(texture->format);
    return 0;
}

static int
PS2_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                   const SDL_Rect * rect, const void *pixels, int pitch)
{
/*  PSP_TextureData *psp_texture = (PSP_TextureData *) texture->driverdata; */
    const Uint8 *src;
    Uint8 *dst;
    int row, length,dpitch;
    src = pixels;

    PS2_LockTexture(renderer, texture, rect, (void **)&dst, &dpitch);
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

static void
PS2_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
}

static void
PS2_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
}

static int
PS2_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    return 0;
}

static int
PS2_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;  /* nothing to do in this backend. */
}

static int
PS2_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    clear_vertex *verts = (clear_vertex *) SDL_AllocateRenderVertices(renderer, count * sizeof (clear_vertex), 4, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, verts++, points++) {
        verts->x = points->x;
        verts->y = points->y;
    }
    return 0;
}

static int
PS2_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    SDL_Rect *verts = (SDL_Rect *) SDL_AllocateRenderVertices(renderer, count * sizeof (SDL_Rect), 4, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, verts++, rects++) {
        verts->x = (int)rects->x;
        verts->y = (int)rects->y;
        verts->w = rects->w + 0.5f;
        verts->h = rects->h + 0.5f;
    }

    return 0;
}

static int
PS2_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
        const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride,
        int num_vertices, const void *indices, int num_indices, int size_indices,
        float scale_x, float scale_y)
{
    int i;
    int count = indices ? num_indices : num_vertices;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    if (texture) {
        texture_vertex *vertices;

        vertices = (texture_vertex *)SDL_AllocateRenderVertices(renderer, 
                                                                count * sizeof(texture_vertex), 
                                                                4, 
                                                                &cmd->data.draw.first);

        if (!vertices) {
            return -1;
        }


        for (i = 0; i < count; i++) {
            int j;
            float *xy_;
            float *uv_;
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
            uv_ = (float *)((char*)uv + j * uv_stride);

            vertices->x = xy_[0] * scale_x;
            vertices->y = xy_[1] * scale_y;
            vertices->u = uv_[0];
            vertices->v = uv_[1];
            vertices->color = GS_SETREG_RGBAQ(col_.r >> 1, col_.g >> 1, col_.b >> 1, col_.a >> 1, 0x00);

            vertices++;
        }

    } else {
        color_vertex *vertices;

        vertices = (color_vertex *)SDL_AllocateRenderVertices(renderer, 
                                                        count * sizeof(color_vertex), 
                                                        4, 
                                                        &cmd->data.draw.first);

        if (!vertices) {
            return -1;
        }


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

            vertices->x = xy_[0] * scale_x;
            vertices->y = xy_[1] * scale_y;
            vertices->color =  GS_SETREG_RGBAQ(col_.r >> 1, col_.g >> 1, col_.b >> 1, col_.a >> 1, 0x00);;

            vertices++;
        }

    }

    return 0;

}


static int
PS2_RenderSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    int colorR, colorG, colorB, colorA;

    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    
    colorR = (cmd->data.color.r) >> 1;
    colorG = (cmd->data.color.g) >> 1;
    colorB = (cmd->data.color.b) >> 1;
    colorA = (cmd->data.color.a) >> 1;
    data->drawColor = GS_SETREG_RGBAQ(colorR, colorG, colorB, colorA, 0x00);
    return 0;
}

static int
PS2_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    int colorR, colorG, colorB, colorA;

    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    
    colorR = (cmd->data.color.r) >> 1;
    colorG = (cmd->data.color.g) >> 1;
    colorB = (cmd->data.color.b) >> 1;
    colorA = (cmd->data.color.a) >> 1;
    gsKit_clear(data->gsGlobal, GS_SETREG_RGBAQ(colorR, colorG, colorB, colorA, 0x00));
    
    return 0;
}

static int
PS2_RenderGeometry(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand *cmd)
{

    int i;
    uint64_t c1, c2, c3;
    float x1, y1, x2, y2, x3, y3;
    float u1, v1, u2, v2, u3, v3;
    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    
    const size_t count = cmd->data.draw.count;

    if (cmd->data.draw.texture) {
        const texture_vertex *verts = (texture_vertex *) (vertices + cmd->data.draw.first);
        GSTEXTURE *ps2_tex = (GSTEXTURE *) cmd->data.draw.texture->driverdata;
        
        for (i = 0; i < count/3; i++) {
            x1 = verts->x;
            y1 = verts->y;

            u1 = verts->u * ps2_tex->Width;
            v1 = verts->v * ps2_tex->Height;

            c1 = verts->color;

            verts++;

            x2 = verts->x;
            y2 = verts->y;

            u2 = verts->u * ps2_tex->Width;
            v2 = verts->v * ps2_tex->Height;

            c2 = verts->color;

            verts++;

            x3 = verts->x;
            y3 = verts->y;

            u3 = verts->u * ps2_tex->Width;
            v3 = verts->v * ps2_tex->Height;

            c3 = verts->color;

            verts++;

	        gsKit_TexManager_bind(data->gsGlobal, ps2_tex);

            gsKit_prim_triangle_goraud_texture(data->gsGlobal, ps2_tex, x1, y1, u1, v1, x2, y2, u2, v2, x3, y3, u3, v3, 0, c1, c2, c3);
        }
    } else {
        const color_vertex *verts = (color_vertex *) (vertices + cmd->data.draw.first);

        for (i = 0; i < count/3; i++) {
            x1 = verts->x;
            y1 = verts->y;

            c1 = verts->color;

            verts++;

            x2 = verts->x;
            y2 = verts->y;
            
            c2 = verts->color;

            verts++;

            x3 = verts->x;
            y3 = verts->y;
            
            c3 = verts->color;

            verts++;

            gsKit_prim_triangle_gouraud(data->gsGlobal, x1, y1, x2, y2, x3, y3, 0, c1, c2, c3);
            
        }
    } 
    
    return 0;
}

int
PS2_FillRects(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand * cmd)
{
    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    const SDL_Rect* rect;
    uint64_t color;
    int i;

    const size_t count = cmd->data.draw.count;

   const uint8_t ColorR = cmd->data.draw.r >> 1;
   const uint8_t ColorG = cmd->data.draw.g >> 1;
   const uint8_t ColorB = cmd->data.draw.b >> 1;
   const uint8_t ColorA = cmd->data.draw.a >> 1;

    color = GS_SETREG_RGBAQ(ColorR, ColorG, ColorB, ColorA, 0x00);

    SDL_Rect *rects = (SDL_Rect *) (vertices + cmd->data.draw.first);

    for (i = 0; i < count; i++) {
        rect = &rects[i];
        gsKit_prim_sprite(data->gsGlobal, rect->x, rect->y, rect->w, rect->h, 0, color);

    }

    /* We're done! */
    return 0;
}

int
PS2_RenderPoints(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand * cmd)
{
    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    uint64_t color;
    int i;

    const size_t count = cmd->data.draw.count;

    const uint8_t ColorR = cmd->data.draw.r >> 1;
    const uint8_t ColorG = cmd->data.draw.g >> 1;
    const uint8_t ColorB = cmd->data.draw.b >> 1;
    const uint8_t ColorA = cmd->data.draw.a >> 1;

    color = GS_SETREG_RGBAQ(ColorR, ColorG, ColorB, ColorA, 0x00);

    const clear_vertex *verts = (clear_vertex *) (vertices + cmd->data.draw.first);

    for (i = 0; i < count; i++, verts++) {
        gsKit_prim_point(data->gsGlobal, verts->x, verts->y, 0, color);
    }

    /* We're done! */
    return 0;
}

static int
PS2_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR: {
                PS2_RenderSetDrawColor(renderer, cmd);
                break;
            }
            case SDL_RENDERCMD_CLEAR: {
                PS2_RenderClear(renderer, cmd);
                break;
            }
            case SDL_RENDERCMD_DRAW_POINTS: {
                PS2_RenderPoints(renderer, vertices, cmd);
                break;
            }
            case SDL_RENDERCMD_FILL_RECTS: {
                PS2_FillRects(renderer, vertices, cmd);
                break;
            }

            case SDL_RENDERCMD_COPY: /* unused */
                break;

            case SDL_RENDERCMD_COPY_EX: /* unused */
                break;
            case SDL_RENDERCMD_GEOMETRY: {
                PS2_RenderGeometry(renderer, vertices, cmd);
                break;
            }
                
            default: 
                break;
        }
        cmd = cmd->next;
    }
    return 0;
}

static int
PS2_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 format, void * pixels, int pitch)
{
    return 0;
}

static void
PS2_RenderPresent(SDL_Renderer * renderer)
{
    PS2_RenderData *data = (PS2_RenderData *) renderer->driverdata;

    if (data->gsGlobal->DoubleBuffering == GS_SETTING_OFF) {
		if (data->vsync)
            gsKit_sync(data->gsGlobal);
		gsKit_queue_exec(data->gsGlobal);
    } else {
		gsKit_queue_exec(data->gsGlobal);
		gsKit_finish();
		if (data->vsync)
            gsKit_sync(data->gsGlobal);
		gsKit_flip(data->gsGlobal);
	}
	gsKit_TexManager_nextFrame(data->gsGlobal);
    gsKit_clear(data->gsGlobal, GS_BLACK);
}

static void
PS2_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    GSTEXTURE *ps2_texture = (GSTEXTURE *) texture->driverdata;

    SDL_free(ps2_texture->Mem);
    SDL_free(ps2_texture);
}

static void
PS2_DestroyRenderer(SDL_Renderer * renderer)
{
    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;

    if (data) {
        gsKit_clear(data->gsGlobal, GS_BLACK);
        gsKit_vram_clear(data->gsGlobal);
        gsKit_deinit_global(data->gsGlobal);
        gsKit_remove_vsync_handler(data->vsync_callback_id);

        SDL_free(data);
    }

    if (vsync_sema_id >= 0)
        DeleteSema(vsync_sema_id);

    SDL_free(renderer);
}

static int
PS2_SetVSync(SDL_Renderer * renderer, const int vsync)
{
    PS2_RenderData *data = (PS2_RenderData *)renderer->driverdata;
    data->vsync = vsync;
    return 0;
}

static SDL_Renderer *
PS2_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Renderer *renderer;
    PS2_RenderData *data;
    GSGLOBAL *gsGlobal;
    ee_sema_t sema;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (PS2_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        PS2_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }

    /* Specific gsKit init */
    sema.init_count = 0;
    sema.max_count = 1;
    sema.option = 0;
    vsync_sema_id = CreateSema(&sema);
    
    gsGlobal = gsKit_init_global();

	gsGlobal->Mode = gsKit_check_rom();
	if (gsGlobal->Mode == GS_MODE_PAL){
		gsGlobal->Height = 512;
	} else {
		gsGlobal->Height = 448;
	}

	gsGlobal->PSM  = GS_PSM_CT24;
	gsGlobal->PSMZ = GS_PSMZ_16S;
	gsGlobal->ZBuffering = GS_SETTING_OFF;
	gsGlobal->DoubleBuffering = GS_SETTING_ON;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->Dithering = GS_SETTING_OFF;

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

	gsKit_vram_clear(gsGlobal);

	gsKit_init_screen(gsGlobal);

	gsKit_TexManager_init(gsGlobal);

	data->vsync_callback_id = gsKit_add_vsync_handler(vsync_handler);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    gsKit_clear(gsGlobal, GS_BLACK);

    data->gsGlobal = gsGlobal;
    if (flags & SDL_RENDERER_PRESENTVSYNC) {
        data->vsync = SDL_TRUE;
    } else {
        data->vsync = SDL_FALSE;
    }	

    renderer->WindowEvent = PS2_WindowEvent;
    renderer->CreateTexture = PS2_CreateTexture;
    renderer->UpdateTexture = PS2_UpdateTexture;
    renderer->LockTexture = PS2_LockTexture;
    renderer->UnlockTexture = PS2_UnlockTexture;
    renderer->SetTextureScaleMode = PS2_SetTextureScaleMode;
    renderer->SetRenderTarget = PS2_SetRenderTarget;
    renderer->QueueSetViewport = PS2_QueueSetViewport;
    renderer->QueueSetDrawColor = PS2_QueueSetViewport;  /* SetViewport and SetDrawColor are (currently) no-ops. */
    renderer->QueueDrawPoints = PS2_QueueDrawPoints;
    renderer->QueueDrawLines = PS2_QueueDrawPoints;  /* lines and points queue vertices the same way. */
    renderer->QueueFillRects = PS2_QueueFillRects;
    renderer->QueueGeometry = PS2_QueueGeometry;
    renderer->RunCommandQueue = PS2_RunCommandQueue;
    renderer->RenderReadPixels = PS2_RenderReadPixels;
    renderer->RenderPresent = PS2_RenderPresent;
    renderer->DestroyTexture = PS2_DestroyTexture;
    renderer->DestroyRenderer = PS2_DestroyRenderer;
    renderer->SetVSync = PS2_SetVSync;
    renderer->info = PS2_RenderDriver.info;
    renderer->driverdata = data;
    renderer->window = window;

    return renderer;
}

SDL_RenderDriver PS2_RenderDriver = {
    .CreateRenderer = PS2_CreateRenderer,
    .info = {
        .name = "PS2 gsKit",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 3,
        .texture_formats = { 
            [0] = SDL_PIXELFORMAT_ABGR1555,
            [1] = SDL_PIXELFORMAT_ABGR8888,
            [2] = SDL_PIXELFORMAT_BGR888,
        },
        .max_texture_width = 1024,
        .max_texture_height = 1024,
    }
};

#endif /* SDL_VIDEO_RENDER_PS2 */

/* vi: set ts=4 sw=4 expandtab: */
