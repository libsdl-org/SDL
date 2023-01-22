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
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_PSP

#include "../SDL_sysrender.h"
#include "SDL_hints.h"

#include <pspgu.h>
#include <pspintrman.h>
#include "SDL_render_psp.h"

static unsigned int __attribute__((aligned(16))) list[262144];
typedef struct
{
    void *frontbuffer;         /**< main screen buffer */
    void *backbuffer;          /**< buffer presented to display */
    uint64_t drawColor;
    int32_t vsync_callback_id;
    uint8_t vsync; /* 0 (Disabled), 1 (Enabled), 2 (Dynamic) */
} PSP_RenderData;

typedef struct PSP_Texture
{
    void *data;                 /**< Image data. */
    unsigned int width;         /**< Image width. */
    unsigned int height;        /**< Image height. */
    unsigned int textureWidth;  /**< Texture width (power of two). */
    unsigned int textureHeight; /**< Texture height (power of two). */
    unsigned int format;        /**< Image format - one of ::pgePixelFormat. */
    unsigned int filter;        /**< Image filter - one of GU_NEAREST or GU_LINEAR. */
    unsigned int pitch;
} PSP_Texture;

typedef struct
{
    float x, y, z;
} VertV;

typedef struct
{
    SDL_Color col;
    float x, y, z;
} VertCV;

typedef struct
{
    float u, v;
    SDL_Color col;
    float x, y, z;
} VertTCV;

int SDL_PSP_RenderGetProp(SDL_Renderer *r, enum SDL_PSP_RenderProps which, void** out)
{
    PSP_RenderData *rd;
    if (r == NULL) {
        return -1;
    }
    rd = r->driverdata;
    switch (which) {
        case SDL_PSP_RENDERPROPS_FRONTBUFFER:
            *out = rd->frontbuffer;
            return 0;
        case SDL_PSP_RENDERPROPS_BACKBUFFER:
            *out = rd->backbuffer;
            return 0;
    }
    return -1;
}

static int vsync_sema_id = 0;

/* PRIVATE METHODS */

static unsigned int getMemorySize(unsigned int width, unsigned int height, unsigned int psm)
{
	switch (psm)
	{
		case GU_PSM_T4:
			return (width * height) >> 1;

		case GU_PSM_T8:
			return width * height;

		case GU_PSM_5650:
		case GU_PSM_5551:
		case GU_PSM_4444:
		case GU_PSM_T16:
			return 2 * width * height;

		case GU_PSM_8888:
		case GU_PSM_T32:
			return 4 * width * height;

		default:
			return 0;
	}
}

static int PixelFormatToPSPFMT(Uint32 format)
{
    switch (format) {
    case SDL_PIXELFORMAT_BGR565:
        return GU_PSM_5650;
    case SDL_PIXELFORMAT_ABGR1555:
        return GU_PSM_5551;
    case SDL_PIXELFORMAT_ABGR4444:
        return GU_PSM_4444;
    case SDL_PIXELFORMAT_ABGR8888:
        return GU_PSM_8888;
    default:
        return GU_PSM_8888;
    }
}

/* Return next power of 2 */
static int TextureNextPow2(unsigned int w)
{
    unsigned int n = 2;
    if (w == 0) {
        return 0;
    }

    while (w > n) {
        n <<= 1;
    }

    return n;
}

static void PSP_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{
}

static int PSP_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    PSP_Texture *psp_tex = (PSP_Texture *)SDL_calloc(1, sizeof(PSP_Texture));

    if (!psp_tex) {
        return SDL_OutOfMemory();
    }

    psp_tex->width = texture->w;
    psp_tex->height = texture->h;
    psp_tex->textureWidth = TextureNextPow2(texture->w);
    psp_tex->textureHeight = TextureNextPow2(texture->h);
    psp_tex->format = PixelFormatToPSPFMT(texture->format);
    psp_tex->data = SDL_calloc(1, getMemorySize(psp_tex->width, psp_tex->height, psp_tex->format));

    if (!psp_tex->data) {
        SDL_free(psp_tex);
        return SDL_OutOfMemory();
    }

    texture->driverdata = psp_tex;

    return 0;
}

static int PSP_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                           const SDL_Rect *rect, void **pixels, int *pitch)
{
    PSP_Texture *psp_texture = (PSP_Texture *)texture->driverdata;

    *pixels =
        (void *)((Uint8 *)psp_texture->data + rect->y * psp_texture->width * SDL_BYTESPERPIXEL(texture->format) +
                 rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = psp_texture->width * SDL_BYTESPERPIXEL(texture->format);
    return 0;
}

static void PSP_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    PSP_Texture *psp_texture = (PSP_Texture *)texture->driverdata;
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    // gsKit_TexManager_invalidate(data->gsGlobal, psp_texture);
}

static int PSP_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                             const SDL_Rect *rect, const void *pixels, int pitch)
{
    const Uint8 *src;
    Uint8 *dst;
    int row, length, dpitch;
    src = pixels;

    PSP_LockTexture(renderer, texture, rect, (void **)&dst, &dpitch);
    length = rect->w * SDL_BYTESPERPIXEL(texture->format);
    if (length == pitch && length == dpitch) {
        SDL_memcpy(dst, src, length * rect->h);
    } else {
        for (row = 0; row < rect->h; ++row) {
            SDL_memcpy(dst, src, length);
            src += pitch;
            dst += dpitch;
        }
    }

    PSP_UnlockTexture(renderer, texture);

    return 0;
}

static void PSP_SetTextureScaleMode(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
    PSP_Texture *psp_texture = (PSP_Texture *)texture->driverdata;
    /*
     set texture filtering according to scaleMode
     suported hint values are nearest (0, default) or linear (1)
     GU scale mode is either GU_NEAREST (good for tile-map)
     or GU_LINEAR (good for scaling)
     */
    uint32_t guScaleMode = (scaleMode == SDL_ScaleModeNearest
                                   ? GU_NEAREST
                                   : GU_LINEAR);
    psp_texture->filter = guScaleMode;
}

static int PSP_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0;
}

static int PSP_QueueSetViewport(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    return 0; /* nothing to do in this backend. */
}

static int PSP_QueueDrawPoints(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points, int count)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    VertV *verts = (VertV *)SDL_AllocateRenderVertices(renderer, count * sizeof(VertV), 4, &cmd->data.draw.first);
    int i;

    if (!verts) {
        return -1;
    }

    cmd->data.draw.count = count;

    for (i = 0; i < count; i++, verts++, points++) {
        verts->x = points->x;
        verts->y = points->y;
        verts->z = 0.0f;
    }
    return 0;
}

static int PSP_QueueGeometry(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                             const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride,
                             int num_vertices, const void *indices, int num_indices, int size_indices,
                             float scale_x, float scale_y)
{
    int i;
    int count = indices ? num_indices : num_vertices;
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    cmd->data.draw.count = count;
    size_indices = indices ? size_indices : 0;

    if (texture) {
        VertTCV *vertices = (VertTCV *) SDL_AllocateRenderVertices(renderer, count * sizeof (VertTCV), 4, &cmd->data.draw.first);
        PSP_Texture *psp_tex = (PSP_Texture *) texture->driverdata;

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

            xy_ = (float *)((char *)xy + j * xy_stride);
            col_ = *(SDL_Color *)((char *)color + j * color_stride);
            uv_ = (float *)((char *)uv + j * uv_stride);

            vertices->x = xy_[0] * scale_x;
            vertices->y = xy_[1] * scale_y;
            vertices->z = 0;

            vertices->col = col_;

            vertices->u = uv_[0] * psp_tex->width;
            vertices->v = uv_[1] * psp_tex->height;

            vertices++;
        }

    } else {
        VertCV *vertices = (VertCV *)SDL_AllocateRenderVertices(renderer, count * sizeof(VertCV), 4, &cmd->data.draw.first);

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

            xy_ = (float *)((char *)xy + j * xy_stride);
            col_ = *(SDL_Color *)((char *)color + j * color_stride);

            vertices->x = xy_[0] * scale_x;
            vertices->y = xy_[1] * scale_y;
            vertices->z = 0;
            vertices->col = col_;

            vertices++;
        }
    }

    return 0;
}

static int PSP_RenderSetViewPort(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    const SDL_Rect *viewport = &cmd->data.viewport.rect;

    sceGuOffset(2048 - (viewport->w >> 1), 2048 - (viewport->h >> 1));
    sceGuViewport(2048, 2048, viewport->w, viewport->h);
    sceGuScissor(viewport->x, viewport->y, viewport->w, viewport->h);

    return 0;
}

static int PSP_RenderSetClipRect(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    const SDL_Rect *rect = &cmd->data.cliprect.rect;

    if (cmd->data.cliprect.enabled) {
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuScissor(rect->x, rect->y, rect->w, rect->h);
    } else {
        sceGuDisable(GU_SCISSOR_TEST);
    }

    return 0;
}

static int PSP_RenderSetDrawColor(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    uint8_t colorR, colorG, colorB, colorA;

    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    colorR = cmd->data.color.r;
    colorG = cmd->data.color.g;
    colorB = cmd->data.color.b;
    colorA = cmd->data.color.a;
    sceGuColor(GU_RGBA(colorR, colorG, colorB, colorA));
    return 0;
}

static int PSP_RenderClear(SDL_Renderer *renderer, SDL_RenderCommand *cmd)
{
    uint8_t colorR, colorG, colorB, colorA;

    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    colorR = cmd->data.color.r;
    colorG = cmd->data.color.g;
    colorB = cmd->data.color.b;
    colorA = cmd->data.color.a;

    sceGuClearColor(GU_RGBA(colorR, colorG, colorB, colorA));
    sceGuClearStencil(colorA);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

    return 0;
}

static void PSP_SetBlendMode(PSP_RenderData *data, int blendMode)
{
    switch (blendMode) {
    case SDL_BLENDMODE_NONE:
    {
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuDisable(GU_BLEND);
        break;
    }
    case SDL_BLENDMODE_BLEND:
    {
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        sceGuEnable(GU_BLEND);
        break;
    }
    case SDL_BLENDMODE_ADD:
    {
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00FFFFFF);
        sceGuEnable(GU_BLEND);
        break;
    }
    case SDL_BLENDMODE_MOD:
    {
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuBlendFunc(GU_ADD, GU_FIX, GU_SRC_COLOR, 0, 0);
        sceGuEnable(GU_BLEND);
        break;
    }
    case SDL_BLENDMODE_MUL:
    {
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            /* FIXME SDL_BLENDMODE_MUL is simplified, and dstA is in fact un-changed.*/
        sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        sceGuEnable(GU_BLEND);
        break;
    }
    }
}

static int PSP_RenderGeometry(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand *cmd)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    const size_t count = cmd->data.draw.count;

    PSP_SetBlendMode(data, cmd->data.draw.blend);

    if (cmd->data.draw.texture) {
        const VertTCV *verts = (VertTCV *)(vertices + cmd->data.draw.first);
        PSP_Texture *psp_tex = (PSP_Texture *)cmd->data.draw.texture->driverdata;

        sceGuEnable(GU_TEXTURE_2D);
        sceGuTexMode(psp_tex->format, 0, 0, GU_FALSE);
        sceGuTexFilter(psp_tex->filter, psp_tex->filter);
        sceGuTexImage(0, psp_tex->textureWidth, psp_tex->textureHeight, psp_tex->textureWidth, psp_tex->data);
        sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, count, 0, verts);
        sceGuDisable(GU_TEXTURE_2D);
    } else {
        const VertCV *verts = (VertCV *)(vertices + cmd->data.draw.first);
        sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, count, 0, verts);
    }

    return 0;
}

int PSP_RenderLines(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand *cmd)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    const size_t count = cmd->data.draw.count;
    const VertV *verts = (VertV *)(vertices + cmd->data.draw.first);

    PSP_SetBlendMode(data, cmd->data.draw.blend);
    sceGuDrawArray(GU_LINES, GU_VERTEX_32BITF | GU_TRANSFORM_2D, count, 0, verts);

    /* We're done! */
    return 0;
}

int PSP_RenderPoints(SDL_Renderer *renderer, void *vertices, SDL_RenderCommand *cmd)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    const size_t count = cmd->data.draw.count;
    const VertV *verts = (VertV *)(vertices + cmd->data.draw.first);

    PSP_SetBlendMode(data, cmd->data.draw.blend);
    sceGuDrawArray(GU_POINTS, GU_VERTEX_32BITF | GU_TRANSFORM_2D, count, 0, verts);

    /* We're done! */
    return 0;
}

static int PSP_RunCommandQueue(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    while (cmd) {
        switch (cmd->command) {
        case SDL_RENDERCMD_SETVIEWPORT:
        {
            PSP_RenderSetViewPort(renderer, cmd);
            /* FIXME: We need to update the clip rect too, see https://github.com/libsdl-org/SDL/issues/9094 */
            break;
        }
        case SDL_RENDERCMD_SETCLIPRECT:
        {
            PSP_RenderSetClipRect(renderer, cmd);
            break;
        }
        case SDL_RENDERCMD_SETDRAWCOLOR:
        {
            PSP_RenderSetDrawColor(renderer, cmd);
            break;
        }
        case SDL_RENDERCMD_CLEAR:
        {
            PSP_RenderClear(renderer, cmd);
            break;
        }
        case SDL_RENDERCMD_DRAW_POINTS:
        {
            PSP_RenderPoints(renderer, vertices, cmd);
            break;
        }
        case SDL_RENDERCMD_DRAW_LINES:
        {
            PSP_RenderLines(renderer, vertices, cmd);
            break;
        }
        case SDL_RENDERCMD_FILL_RECTS: /* unused */
            break;
        case SDL_RENDERCMD_COPY: /* unused */
            break;
        case SDL_RENDERCMD_COPY_EX: /* unused */
            break;
        case SDL_RENDERCMD_GEOMETRY:
        {
            PSP_RenderGeometry(renderer, vertices, cmd);
            break;
        }
        case SDL_RENDERCMD_NO_OP:
            break;
        }
        cmd = cmd->next;
    }
    return 0;
}

static int PSP_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                                Uint32 format, void *pixels, int pitch)
{
    return SDL_Unsupported();
}

static int PSP_RenderPresent(SDL_Renderer *renderer)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    // if (data->gsGlobal->DoubleBuffering == GS_SETTING_OFF) {
    //     if (data->vsync == 2) { // Dynamic
    //         gsKit_sync(data->gsGlobal);
    //     } else if (data->vsync == 1) {
    //         gsKit_vsync_wait();
    //     }
    //     gsKit_queue_exec(data->gsGlobal);
    // } else {
    //     gsKit_queue_exec(data->gsGlobal);
    //     gsKit_finish();
    //     if (data->vsync == 2) { // Dynamic
    //         gsKit_sync(data->gsGlobal);
    //     } else if (data->vsync == 1) {
    //         gsKit_vsync_wait();
    //     }
    //     gsKit_flip(data->gsGlobal);
    // }
    // gsKit_TexManager_nextFrame(data->gsGlobal);
    // gsKit_clear(data->gsGlobal, GS_BLACK);

    sceGuFinish();
    sceGuSync(0,0);
    sceGuSwapBuffers();

    // Starting a new frame
    sceGuStart(GU_DIRECT,list);

    return 0;
}

static void PSP_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
    PSP_Texture *psp_texture = (PSP_Texture *)texture->driverdata;
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;

    if (!data) {
        return;
    }

    if (!psp_texture) {
        return;
    }

    SDL_free(psp_texture->data);
    SDL_free(psp_texture);
    texture->driverdata = NULL;
}

static void PSP_DestroyRenderer(SDL_Renderer *renderer)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    if (data) {
        sceKernelDisableSubIntr(PSP_VBLANK_INT, 0);
        sceKernelReleaseSubIntrHandler(PSP_VBLANK_INT, 0);
        sceDisplayWaitVblankStart();
        sceGuDisplay(GU_FALSE);

        sceGuTerm();
        vfree(data->backbuffer);
        vfree(data->frontbuffer);

        SDL_free(data);
    }

    if (vsync_sema_id >= 0) {
        // DeleteSema(vsync_sema_id);
    }

}

static int PSP_SetVSync(SDL_Renderer *renderer, const int vsync)
{
    PSP_RenderData *data = (PSP_RenderData *)renderer->driverdata;
    SDL_bool dynamicVsync = SDL_GetHintBoolean(SDL_HINT_PSP_DYNAMIC_VSYNC, SDL_FALSE);
    data->vsync = vsync ? (dynamicVsync ? 2 : 1) : 0;
    return 0;
}

static int PSP_CreateRenderer(SDL_Renderer *renderer, SDL_Window *window, Uint32 flags)
{
    PSP_RenderData *data;
    void *doublebuffer = NULL;
    uint32_t bufferSize = 0;
    SDL_bool dynamicVsync;

    data = (PSP_RenderData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        PSP_DestroyRenderer(renderer);
        return SDL_OutOfMemory();
    }

    /* Specific GU init */
    bufferSize = getMemorySize(PSP_FRAME_BUFFER_WIDTH, PSP_SCREEN_HEIGHT, GU_PSM_8888);
    doublebuffer = vramalloc(bufferSize * 2);
    data->backbuffer = doublebuffer;
    data->frontbuffer = ((uint8_t *)doublebuffer) + bufferSize;

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, vrelptr(data->frontbuffer), PSP_FRAME_BUFFER_WIDTH);
    sceGuDispBuffer(PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, vrelptr(data->backbuffer), PSP_FRAME_BUFFER_WIDTH);
	
    sceGuOffset(2048 - (PSP_SCREEN_WIDTH >> 1), 2048 - (PSP_SCREEN_HEIGHT >> 1));
    sceGuViewport(2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	
    sceGuDisable(GU_DEPTH_TEST);

	sceGuScissor(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	
	sceGuFinish();
	sceGuSync(0,0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

    dynamicVsync = SDL_GetHintBoolean(SDL_HINT_PSP_DYNAMIC_VSYNC, SDL_FALSE);
    data->vsync = flags & SDL_RENDERER_PRESENTVSYNC ? (dynamicVsync ? 2 : 1) : 0;

    renderer->WindowEvent = PSP_WindowEvent;
    renderer->CreateTexture = PSP_CreateTexture;
    renderer->UpdateTexture = PSP_UpdateTexture;
    renderer->LockTexture = PSP_LockTexture;
    renderer->UnlockTexture = PSP_UnlockTexture;
    renderer->SetTextureScaleMode = PSP_SetTextureScaleMode;
    renderer->SetRenderTarget = PSP_SetRenderTarget;
    renderer->QueueSetViewport = PSP_QueueSetViewport;
    renderer->QueueSetDrawColor = PSP_QueueSetViewport;
    renderer->QueueDrawPoints = PSP_QueueDrawPoints;
    renderer->QueueDrawLines = PSP_QueueDrawPoints;
    renderer->QueueGeometry = PSP_QueueGeometry;
    renderer->RunCommandQueue = PSP_RunCommandQueue;
    renderer->RenderReadPixels = PSP_RenderReadPixels;
    renderer->RenderPresent = PSP_RenderPresent;
    renderer->DestroyTexture = PSP_DestroyTexture;
    renderer->DestroyRenderer = PSP_DestroyRenderer;
    renderer->SetVSync = PSP_SetVSync;
    renderer->info = PSP_RenderDriver.info;
    renderer->driverdata = data;
    renderer->window = window;

    return 0;
}

SDL_RenderDriver PSP_RenderDriver = {
    .CreateRenderer = PSP_CreateRenderer,
    .info = {
        .name = "PSP GU",
        .flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE,
        .num_texture_formats = 4,
        .texture_formats = {
            [0] = SDL_PIXELFORMAT_BGR565,
            [1] = SDL_PIXELFORMAT_ABGR1555,
            [2] = SDL_PIXELFORMAT_ABGR4444,
            [3] = SDL_PIXELFORMAT_ABGR8888,
        },
        .max_texture_width = 512,
        .max_texture_height = 512,
    }
};

#endif /* SDL_VIDEO_RENDER_PSP */

/* vi: set ts=4 sw=4 expandtab: */
