/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

/* The SDL 2D rendering system */

#include "SDL_sysrender.h"
#include "software/SDL_render_sw_c.h"
#include "../video/SDL_pixels_c.h"
#include "../video/SDL_video_c.h"

#ifdef SDL_PLATFORM_ANDROID
#include "../core/android/SDL_android.h"
#endif

/* as a courtesy to iOS apps, we don't try to draw when in the background, as
that will crash the app. However, these apps _should_ have used
SDL_AddEventWatch to catch SDL_EVENT_WILL_ENTER_BACKGROUND events and stopped
drawing themselves. Other platforms still draw, as the compositor can use it,
and more importantly: drawing to render targets isn't lost. But I still think
this should probably be removed at some point in the future.  --ryan. */
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_TVOS) || defined(SDL_PLATFORM_ANDROID)
#define DONT_DRAW_WHILE_HIDDEN 1
#else
#define DONT_DRAW_WHILE_HIDDEN 0
#endif

#define SDL_PROP_WINDOW_RENDERER_POINTER "SDL.internal.window.renderer"
#define SDL_PROP_TEXTURE_PARENT_POINTER "SDL.internal.texture.parent"

#define CHECK_RENDERER_MAGIC_BUT_NOT_DESTROYED_FLAG(renderer, retval)   \
    if (!SDL_ObjectValid(renderer, SDL_OBJECT_TYPE_RENDERER)) {         \
        SDL_InvalidParamError("renderer");                              \
        return retval;                                                  \
    }

#define CHECK_RENDERER_MAGIC(renderer, retval)                  \
    CHECK_RENDERER_MAGIC_BUT_NOT_DESTROYED_FLAG(renderer, retval); \
    if ((renderer)->destroyed) { \
        SDL_SetError("Renderer's window has been destroyed, can't use further"); \
        return retval;                                          \
    }

#define CHECK_TEXTURE_MAGIC(texture, retval)                    \
    if (!SDL_ObjectValid(texture, SDL_OBJECT_TYPE_TEXTURE)) {   \
        SDL_InvalidParamError("texture");                       \
        return retval;                                          \
    }

/* Predefined blend modes */
#define SDL_COMPOSE_BLENDMODE(srcColorFactor, dstColorFactor, colorOperation, \
                              srcAlphaFactor, dstAlphaFactor, alphaOperation) \
    (SDL_BlendMode)(((Uint32)(colorOperation) << 0) |                         \
                    ((Uint32)(srcColorFactor) << 4) |                         \
                    ((Uint32)(dstColorFactor) << 8) |                         \
                    ((Uint32)(alphaOperation) << 16) |                        \
                    ((Uint32)(srcAlphaFactor) << 20) |                        \
                    ((Uint32)(dstAlphaFactor) << 24))

#define SDL_BLENDMODE_NONE_FULL                                                              \
    SDL_COMPOSE_BLENDMODE(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD, \
                          SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD)

#define SDL_BLENDMODE_BLEND_FULL                                                                                  \
    SDL_COMPOSE_BLENDMODE(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, \
                          SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD)

#define SDL_BLENDMODE_ADD_FULL                                                                    \
    SDL_COMPOSE_BLENDMODE(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD, \
                          SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD)

#define SDL_BLENDMODE_MOD_FULL                                                                     \
    SDL_COMPOSE_BLENDMODE(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_COLOR, SDL_BLENDOPERATION_ADD, \
                          SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD)

#define SDL_BLENDMODE_MUL_FULL                                                                                    \
    SDL_COMPOSE_BLENDMODE(SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, \
                          SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD)

#ifndef SDL_RENDER_DISABLED
static const SDL_RenderDriver *render_drivers[] = {
#if SDL_VIDEO_RENDER_D3D11
    &D3D11_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_D3D12
    &D3D12_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_D3D
    &D3D_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_METAL
    &METAL_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_OGL
    &GL_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_OGL_ES2
    &GLES2_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_PS2
    &PS2_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_PSP
    &PSP_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_VITA_GXM
    &VITA_GXM_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_VULKAN
    &VULKAN_RenderDriver,
#endif
#if SDL_VIDEO_RENDER_SW
    &SW_RenderDriver
#endif
};
#endif /* !SDL_RENDER_DISABLED */

static SDL_Renderer *SDL_renderers;

void SDL_QuitRender(void)
{
    while (SDL_renderers) {
        SDL_DestroyRenderer(SDL_renderers);
    }
}

int SDL_AddSupportedTextureFormat(SDL_Renderer *renderer, SDL_PixelFormatEnum format)
{
    SDL_PixelFormatEnum *texture_formats = (SDL_PixelFormatEnum *)SDL_realloc((void *)renderer->texture_formats, (renderer->num_texture_formats + 2) * sizeof(SDL_PixelFormatEnum));
    if (!texture_formats) {
        return -1;
    }
    texture_formats[renderer->num_texture_formats++] = format;
    texture_formats[renderer->num_texture_formats] = SDL_PIXELFORMAT_UNKNOWN;
    renderer->texture_formats = texture_formats;
    SDL_SetProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, texture_formats);
    return 0;
}

void SDL_SetupRendererColorspace(SDL_Renderer *renderer, SDL_PropertiesID props)
{
    renderer->output_colorspace = (SDL_Colorspace)SDL_GetNumberProperty(props, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
}

static float sRGBtoLinear(float v)
{
    return v <= 0.04045f ? (v / 12.92f) : SDL_powf(((v + 0.055f) / 1.055f), 2.4f);
}

static float sRGBfromLinear(float v)
{
    return v <= 0.0031308f ? (v * 12.92f) : (SDL_powf(v, 1.0f / 2.4f) * 1.055f - 0.055f);
}

SDL_bool SDL_RenderingLinearSpace(SDL_Renderer *renderer)
{
    SDL_Colorspace colorspace;

    if (renderer->target) {
        colorspace = renderer->target->colorspace;
    } else {
        colorspace = renderer->output_colorspace;
    }
    if (colorspace == SDL_COLORSPACE_SRGB_LINEAR) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

void SDL_ConvertToLinear(SDL_FColor *color)
{
    color->r = sRGBtoLinear(color->r);
    color->g = sRGBtoLinear(color->g);
    color->b = sRGBtoLinear(color->b);
}

void SDL_ConvertFromLinear(SDL_FColor *color)
{
    color->r = sRGBfromLinear(color->r);
    color->g = sRGBfromLinear(color->g);
    color->b = sRGBfromLinear(color->b);
}

static SDL_INLINE void DebugLogRenderCommands(const SDL_RenderCommand *cmd)
{
#if 0
    unsigned int i = 1;
    SDL_Log("Render commands to flush:");
    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_NO_OP:
                SDL_Log(" %u. no-op", i++);
                break;

            case SDL_RENDERCMD_SETVIEWPORT:
                SDL_Log(" %u. set viewport (first=%u, rect={(%d, %d), %dx%d})", i++,
                        (unsigned int) cmd->data.viewport.first,
                        cmd->data.viewport.rect.x, cmd->data.viewport.rect.y,
                        cmd->data.viewport.rect.w, cmd->data.viewport.rect.h);
                break;

            case SDL_RENDERCMD_SETCLIPRECT:
                SDL_Log(" %u. set cliprect (enabled=%s, rect={(%d, %d), %dx%d})", i++,
                        cmd->data.cliprect.enabled ? "true" : "false",
                        cmd->data.cliprect.rect.x, cmd->data.cliprect.rect.y,
                        cmd->data.cliprect.rect.w, cmd->data.cliprect.rect.h);
                break;

            case SDL_RENDERCMD_SETDRAWCOLOR:
                SDL_Log(" %u. set draw color (first=%u, r=%d, g=%d, b=%d, a=%d, color_scale=%g)", i++,
                        (unsigned int) cmd->data.color.first,
                        (int) cmd->data.color.color.r, (int) cmd->data.color.color.g,
                        (int) cmd->data.color.color.b, (int) cmd->data.color.color.a, cmd->data.color.color_scale);
                break;

            case SDL_RENDERCMD_CLEAR:
                SDL_Log(" %u. clear (first=%u, r=%d, g=%d, b=%d, a=%d, color_scale=%g)", i++,
                        (unsigned int) cmd->data.color.first,
                        (int) cmd->data.color.color.r, (int) cmd->data.color.color.g,
                        (int) cmd->data.color.color.b, (int) cmd->data.color.color.a, cmd->data.color.color_scale);
                break;

            case SDL_RENDERCMD_DRAW_POINTS:
                SDL_Log(" %u. draw points (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale);
                break;

            case SDL_RENDERCMD_DRAW_LINES:
                SDL_Log(" %u. draw lines (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale);
                break;

            case SDL_RENDERCMD_FILL_RECTS:
                SDL_Log(" %u. fill rects (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale);
                break;

            case SDL_RENDERCMD_COPY:
                SDL_Log(" %u. copy (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g, tex=%p)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale, cmd->data.draw.texture);
                break;


            case SDL_RENDERCMD_COPY_EX:
                SDL_Log(" %u. copyex (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g, tex=%p)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale, cmd->data.draw.texture);
                break;

            case SDL_RENDERCMD_GEOMETRY:
                SDL_Log(" %u. geometry (first=%u, count=%u, r=%d, g=%d, b=%d, a=%d, blend=%d, color_scale=%g, tex=%p)", i++,
                        (unsigned int) cmd->data.draw.first,
                        (unsigned int) cmd->data.draw.count,
                        (int) cmd->data.draw.color.r, (int) cmd->data.draw.color.g,
                        (int) cmd->data.draw.color.b, (int) cmd->data.draw.color.a,
                        (int) cmd->data.draw.blend, cmd->data.draw.color_scale, cmd->data.draw.texture);
                break;

        }
        cmd = cmd->next;
    }
#endif
}

static int FlushRenderCommands(SDL_Renderer *renderer)
{
    int retval;

    SDL_assert((renderer->render_commands == NULL) == (renderer->render_commands_tail == NULL));

    if (!renderer->render_commands) { /* nothing to do! */
        SDL_assert(renderer->vertex_data_used == 0);
        return 0;
    }

    DebugLogRenderCommands(renderer->render_commands);

    retval = renderer->RunCommandQueue(renderer, renderer->render_commands, renderer->vertex_data, renderer->vertex_data_used);

    /* Move the whole render command queue to the unused pool so we can reuse them next time. */
    if (renderer->render_commands_tail) {
        renderer->render_commands_tail->next = renderer->render_commands_pool;
        renderer->render_commands_pool = renderer->render_commands;
        renderer->render_commands_tail = NULL;
        renderer->render_commands = NULL;
    }
    renderer->vertex_data_used = 0;
    renderer->render_command_generation++;
    renderer->color_queued = SDL_FALSE;
    renderer->color_scale_queued = SDL_FALSE;
    renderer->viewport_queued = SDL_FALSE;
    renderer->cliprect_queued = SDL_FALSE;
    return retval;
}

static int FlushRenderCommandsIfTextureNeeded(SDL_Texture *texture)
{
    SDL_Renderer *renderer = texture->renderer;
    if (texture->last_command_generation == renderer->render_command_generation) {
        /* the current command queue depends on this texture, flush the queue now before it changes */
        return FlushRenderCommands(renderer);
    }
    return 0;
}

int SDL_FlushRenderer(SDL_Renderer *renderer)
{
    if (FlushRenderCommands(renderer) == -1) {
        return -1;
    }
    renderer->InvalidateCachedState(renderer);
    return 0;
}

void *SDL_AllocateRenderVertices(SDL_Renderer *renderer, const size_t numbytes, const size_t alignment, size_t *offset)
{
    const size_t needed = renderer->vertex_data_used + numbytes + alignment;
    const size_t current_offset = renderer->vertex_data_used;

    const size_t aligner = (alignment && ((current_offset & (alignment - 1)) != 0)) ? (alignment - (current_offset & (alignment - 1))) : 0;
    const size_t aligned = current_offset + aligner;

    if (renderer->vertex_data_allocation < needed) {
        const size_t current_allocation = renderer->vertex_data ? renderer->vertex_data_allocation : 1024;
        size_t newsize = current_allocation * 2;
        void *ptr;
        while (newsize < needed) {
            newsize *= 2;
        }

        ptr = SDL_realloc(renderer->vertex_data, newsize);

        if (!ptr) {
            return NULL;
        }
        renderer->vertex_data = ptr;
        renderer->vertex_data_allocation = newsize;
    }

    if (offset) {
        *offset = aligned;
    }

    renderer->vertex_data_used += aligner + numbytes;

    return ((Uint8 *)renderer->vertex_data) + aligned;
}

static SDL_RenderCommand *AllocateRenderCommand(SDL_Renderer *renderer)
{
    SDL_RenderCommand *retval = NULL;

    /* !!! FIXME: are there threading limitations in SDL's render API? If not, we need to mutex this. */
    retval = renderer->render_commands_pool;
    if (retval) {
        renderer->render_commands_pool = retval->next;
        retval->next = NULL;
    } else {
        retval = (SDL_RenderCommand *)SDL_calloc(1, sizeof(*retval));
        if (!retval) {
            return NULL;
        }
    }

    SDL_assert((renderer->render_commands == NULL) == (renderer->render_commands_tail == NULL));
    if (renderer->render_commands_tail) {
        renderer->render_commands_tail->next = retval;
    } else {
        renderer->render_commands = retval;
    }
    renderer->render_commands_tail = retval;

    return retval;
}

static void UpdatePixelViewport(SDL_Renderer *renderer, SDL_RenderViewState *view)
{
    view->pixel_viewport.x = (int)SDL_floorf(view->viewport.x * view->scale.x);
    view->pixel_viewport.y = (int)SDL_floorf(view->viewport.y * view->scale.y);
    if (view->viewport.w >= 0) {
        view->pixel_viewport.w = (int)SDL_ceilf(view->viewport.w * view->scale.x);
    } else {
        view->pixel_viewport.w = view->pixel_w;
    }
    if (view->viewport.h >= 0) {
        view->pixel_viewport.h = (int)SDL_ceilf(view->viewport.h * view->scale.y);
    } else {
        view->pixel_viewport.h = view->pixel_h;
    }
}

static int QueueCmdSetViewport(SDL_Renderer *renderer)
{
    SDL_Rect viewport;
    int retval = 0;

    viewport = renderer->view->pixel_viewport;

    if (!renderer->viewport_queued ||
        SDL_memcmp(&viewport, &renderer->last_queued_viewport, sizeof(viewport)) != 0) {
        SDL_RenderCommand *cmd = AllocateRenderCommand(renderer);
        if (cmd) {
            cmd->command = SDL_RENDERCMD_SETVIEWPORT;
            cmd->data.viewport.first = 0; /* render backend will fill this in. */
            SDL_copyp(&cmd->data.viewport.rect, &viewport);
            retval = renderer->QueueSetViewport(renderer, cmd);
            if (retval < 0) {
                cmd->command = SDL_RENDERCMD_NO_OP;
            } else {
                SDL_copyp(&renderer->last_queued_viewport, &viewport);
                renderer->viewport_queued = SDL_TRUE;
            }
        } else {
            retval = -1;
        }
    }
    return retval;
}

static void UpdatePixelClipRect(SDL_Renderer *renderer, SDL_RenderViewState *view)
{
    view->pixel_clip_rect.x = (int)SDL_floorf(view->clip_rect.x * view->scale.x);
    view->pixel_clip_rect.y = (int)SDL_floorf(view->clip_rect.y * view->scale.y);
    view->pixel_clip_rect.w = (int)SDL_ceilf(view->clip_rect.w * view->scale.x);
    view->pixel_clip_rect.h = (int)SDL_ceilf(view->clip_rect.h * view->scale.y);
}

static int QueueCmdSetClipRect(SDL_Renderer *renderer)
{
    SDL_Rect clip_rect;
    int retval = 0;

    clip_rect = renderer->view->pixel_clip_rect;

    if (!renderer->cliprect_queued ||
        renderer->view->clipping_enabled != renderer->last_queued_cliprect_enabled ||
        SDL_memcmp(&clip_rect, &renderer->last_queued_cliprect, sizeof(clip_rect)) != 0) {
        SDL_RenderCommand *cmd = AllocateRenderCommand(renderer);
        if (cmd) {
            cmd->command = SDL_RENDERCMD_SETCLIPRECT;
            cmd->data.cliprect.enabled = renderer->view->clipping_enabled;
            SDL_copyp(&cmd->data.cliprect.rect, &clip_rect);
            SDL_copyp(&renderer->last_queued_cliprect, &clip_rect);
            renderer->last_queued_cliprect_enabled = renderer->view->clipping_enabled;
            renderer->cliprect_queued = SDL_TRUE;
        } else {
            retval = -1;
        }
    }
    return retval;
}

static int QueueCmdSetDrawColor(SDL_Renderer *renderer, SDL_FColor *color)
{
    int retval = 0;

    if (!renderer->color_queued ||
        color->r != renderer->last_queued_color.r ||
        color->g != renderer->last_queued_color.g ||
        color->b != renderer->last_queued_color.b ||
        color->a != renderer->last_queued_color.a) {
        SDL_RenderCommand *cmd = AllocateRenderCommand(renderer);
        retval = -1;

        if (cmd) {
            cmd->command = SDL_RENDERCMD_SETDRAWCOLOR;
            cmd->data.color.first = 0; /* render backend will fill this in. */
            cmd->data.color.color_scale = renderer->color_scale;
            cmd->data.color.color = *color;
            retval = renderer->QueueSetDrawColor(renderer, cmd);
            if (retval < 0) {
                cmd->command = SDL_RENDERCMD_NO_OP;
            } else {
                renderer->last_queued_color = *color;
                renderer->color_queued = SDL_TRUE;
            }
        }
    }
    return retval;
}

static int QueueCmdClear(SDL_Renderer *renderer)
{
    SDL_RenderCommand *cmd = AllocateRenderCommand(renderer);
    if (!cmd) {
        return -1;
    }

    cmd->command = SDL_RENDERCMD_CLEAR;
    cmd->data.color.first = 0;
    cmd->data.color.color_scale = renderer->color_scale;
    cmd->data.color.color = renderer->color;
    return 0;
}

static SDL_RenderCommand *PrepQueueCmdDraw(SDL_Renderer *renderer, const SDL_RenderCommandType cmdtype, SDL_Texture *texture)
{
    SDL_RenderCommand *cmd = NULL;
    int retval = 0;
    SDL_FColor *color;
    SDL_BlendMode blendMode;

    if (texture) {
        color = &texture->color;
        blendMode = texture->blendMode;
    } else {
        color = &renderer->color;
        blendMode = renderer->blendMode;
    }

    if (cmdtype != SDL_RENDERCMD_GEOMETRY) {
        retval = QueueCmdSetDrawColor(renderer, color);
    }

    /* Set the viewport and clip rect directly before draws, so the backends
     * don't have to worry about that state not being valid at draw time. */
    if (retval == 0 && !renderer->viewport_queued) {
        retval = QueueCmdSetViewport(renderer);
    }
    if (retval == 0 && !renderer->cliprect_queued) {
        retval = QueueCmdSetClipRect(renderer);
    }

    if (retval == 0) {
        cmd = AllocateRenderCommand(renderer);
        if (cmd) {
            cmd->command = cmdtype;
            cmd->data.draw.first = 0; /* render backend will fill this in. */
            cmd->data.draw.count = 0; /* render backend will fill this in. */
            cmd->data.draw.color_scale = renderer->color_scale;
            cmd->data.draw.color = *color;
            cmd->data.draw.blend = blendMode;
            cmd->data.draw.texture = texture;
        }
    }
    return cmd;
}

static int QueueCmdDrawPoints(SDL_Renderer *renderer, const SDL_FPoint *points, const int count)
{
    SDL_RenderCommand *cmd = PrepQueueCmdDraw(renderer, SDL_RENDERCMD_DRAW_POINTS, NULL);
    int retval = -1;
    if (cmd) {
        retval = renderer->QueueDrawPoints(renderer, cmd, points, count);
        if (retval < 0) {
            cmd->command = SDL_RENDERCMD_NO_OP;
        }
    }
    return retval;
}

static int QueueCmdDrawLines(SDL_Renderer *renderer, const SDL_FPoint *points, const int count)
{
    SDL_RenderCommand *cmd = PrepQueueCmdDraw(renderer, SDL_RENDERCMD_DRAW_LINES, NULL);
    int retval = -1;
    if (cmd) {
        retval = renderer->QueueDrawLines(renderer, cmd, points, count);
        if (retval < 0) {
            cmd->command = SDL_RENDERCMD_NO_OP;
        }
    }
    return retval;
}

static int QueueCmdFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, const int count)
{
    SDL_RenderCommand *cmd;
    int retval = -1;
    const int use_rendergeometry = (!renderer->QueueFillRects);

    cmd = PrepQueueCmdDraw(renderer, (use_rendergeometry ? SDL_RENDERCMD_GEOMETRY : SDL_RENDERCMD_FILL_RECTS), NULL);

    if (cmd) {
        if (use_rendergeometry) {
            SDL_bool isstack1;
            SDL_bool isstack2;
            float *xy = SDL_small_alloc(float, 4 * 2 * count, &isstack1);
            int *indices = SDL_small_alloc(int, 6 * count, &isstack2);

            if (xy && indices) {
                int i;
                float *ptr_xy = xy;
                int *ptr_indices = indices;
                const int xy_stride = 2 * sizeof(float);
                const int num_vertices = 4 * count;
                const int num_indices = 6 * count;
                const int size_indices = 4;
                int cur_index = 0;
                const int *rect_index_order = renderer->rect_index_order;

                for (i = 0; i < count; ++i) {
                    float minx, miny, maxx, maxy;

                    minx = rects[i].x;
                    miny = rects[i].y;
                    maxx = rects[i].x + rects[i].w;
                    maxy = rects[i].y + rects[i].h;

                    *ptr_xy++ = minx;
                    *ptr_xy++ = miny;
                    *ptr_xy++ = maxx;
                    *ptr_xy++ = miny;
                    *ptr_xy++ = maxx;
                    *ptr_xy++ = maxy;
                    *ptr_xy++ = minx;
                    *ptr_xy++ = maxy;

                    *ptr_indices++ = cur_index + rect_index_order[0];
                    *ptr_indices++ = cur_index + rect_index_order[1];
                    *ptr_indices++ = cur_index + rect_index_order[2];
                    *ptr_indices++ = cur_index + rect_index_order[3];
                    *ptr_indices++ = cur_index + rect_index_order[4];
                    *ptr_indices++ = cur_index + rect_index_order[5];
                    cur_index += 4;
                }

                retval = renderer->QueueGeometry(renderer, cmd, NULL,
                                                 xy, xy_stride, &renderer->color, 0 /* color_stride */, NULL, 0,
                                                 num_vertices, indices, num_indices, size_indices,
                                                 1.0f, 1.0f);

                if (retval < 0) {
                    cmd->command = SDL_RENDERCMD_NO_OP;
                }
            }
            SDL_small_free(xy, isstack1);
            SDL_small_free(indices, isstack2);

        } else {
            retval = renderer->QueueFillRects(renderer, cmd, rects, count);
            if (retval < 0) {
                cmd->command = SDL_RENDERCMD_NO_OP;
            }
        }
    }
    return retval;
}

static int QueueCmdCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect)
{
    SDL_RenderCommand *cmd = PrepQueueCmdDraw(renderer, SDL_RENDERCMD_COPY, texture);
    int retval = -1;
    if (cmd) {
        retval = renderer->QueueCopy(renderer, cmd, texture, srcrect, dstrect);
        if (retval < 0) {
            cmd->command = SDL_RENDERCMD_NO_OP;
        }
    }
    return retval;
}

static int QueueCmdCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
                          const SDL_FRect *srcquad, const SDL_FRect *dstrect,
                          const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y)
{
    SDL_RenderCommand *cmd = PrepQueueCmdDraw(renderer, SDL_RENDERCMD_COPY_EX, texture);
    int retval = -1;
    if (cmd) {
        retval = renderer->QueueCopyEx(renderer, cmd, texture, srcquad, dstrect, angle, center, flip, scale_x, scale_y);
        if (retval < 0) {
            cmd->command = SDL_RENDERCMD_NO_OP;
        }
    }
    return retval;
}

static int QueueCmdGeometry(SDL_Renderer *renderer, SDL_Texture *texture,
                            const float *xy, int xy_stride,
                            const SDL_FColor *color, int color_stride,
                            const float *uv, int uv_stride,
                            int num_vertices,
                            const void *indices, int num_indices, int size_indices,
                            float scale_x, float scale_y)
{
    SDL_RenderCommand *cmd;
    int retval = -1;
    cmd = PrepQueueCmdDraw(renderer, SDL_RENDERCMD_GEOMETRY, texture);
    if (cmd) {
        retval = renderer->QueueGeometry(renderer, cmd, texture,
                                         xy, xy_stride,
                                         color, color_stride, uv, uv_stride,
                                         num_vertices, indices, num_indices, size_indices,
                                         scale_x, scale_y);
        if (retval < 0) {
            cmd->command = SDL_RENDERCMD_NO_OP;
        }
    }
    return retval;
}

static void UpdateMainViewDimensions(SDL_Renderer *renderer)
{
    int window_w = 0, window_h = 0;

    if (renderer->window) {
        SDL_GetWindowSize(renderer->window, &window_w, &window_h);
    }
    SDL_GetRenderOutputSize(renderer, &renderer->main_view.pixel_w, &renderer->main_view.pixel_h);
    if (window_w > 0 && window_h > 0) {
        renderer->dpi_scale.x = (float)renderer->main_view.pixel_w / window_w;
        renderer->dpi_scale.y = (float)renderer->main_view.pixel_h / window_h;
    } else {
        renderer->dpi_scale.x = 1.0f;
        renderer->dpi_scale.y = 1.0f;
    }
    UpdatePixelViewport(renderer, &renderer->main_view);
}

static void UpdateHDRProperties(SDL_Renderer *renderer)
{
    SDL_PropertiesID window_props;
    SDL_PropertiesID renderer_props;

    window_props = SDL_GetWindowProperties(renderer->window);
    if (!window_props) {
        return;
    }

    renderer_props = SDL_GetRendererProperties(renderer);
    if (!renderer_props) {
        return;
    }

    renderer->color_scale /= renderer->SDR_white_point;

    if (renderer->output_colorspace == SDL_COLORSPACE_SRGB_LINEAR) {
        renderer->SDR_white_point = SDL_GetFloatProperty(window_props, SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT, 1.0f);
        renderer->HDR_headroom = SDL_GetFloatProperty(window_props, SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT, 1.0f);
    } else {
        renderer->SDR_white_point = 1.0f;
        renderer->HDR_headroom = 1.0f;
    }

    if (renderer->HDR_headroom > 1.0f) {
        SDL_SetBooleanProperty(renderer_props, SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN, SDL_TRUE);
    } else {
        SDL_SetBooleanProperty(renderer_props, SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN, SDL_FALSE);
    }
    SDL_SetFloatProperty(renderer_props, SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT, renderer->SDR_white_point);
    SDL_SetFloatProperty(renderer_props, SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT, renderer->HDR_headroom);

    renderer->color_scale *= renderer->SDR_white_point;
}

static int UpdateLogicalPresentation(SDL_Renderer *renderer);


int SDL_GetNumRenderDrivers(void)
{
#ifndef SDL_RENDER_DISABLED
    return SDL_arraysize(render_drivers);
#else
    return 0;
#endif
}

// this returns string literals, so there's no need to use SDL_FreeLater.
const char *SDL_GetRenderDriver(int index)
{
#ifndef SDL_RENDER_DISABLED
    if (index < 0 || index >= SDL_GetNumRenderDrivers()) {
        SDL_SetError("index must be in the range of 0 - %d",
                            SDL_GetNumRenderDrivers() - 1);
        return NULL;
    }
    return render_drivers[index]->name;
#else
    SDL_SetError("SDL not built with rendering support");
    return NULL;
#endif
}

static int SDLCALL SDL_RendererEventWatch(void *userdata, SDL_Event *event)
{
    SDL_Renderer *renderer = (SDL_Renderer *)userdata;

    if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
        SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
        if (window == renderer->window) {
            if (renderer->WindowEvent) {
                renderer->WindowEvent(renderer, &event->window);
            }

            if (event->type == SDL_EVENT_WINDOW_RESIZED ||
                event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                UpdateMainViewDimensions(renderer);
                UpdateLogicalPresentation(renderer);
            } else if (event->type == SDL_EVENT_WINDOW_HIDDEN) {
                renderer->hidden = SDL_TRUE;
            } else if (event->type == SDL_EVENT_WINDOW_SHOWN) {
                if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) {
                    renderer->hidden = SDL_FALSE;
                }
            } else if (event->type == SDL_EVENT_WINDOW_MINIMIZED) {
                renderer->hidden = SDL_TRUE;
            } else if (event->type == SDL_EVENT_WINDOW_RESTORED ||
                       event->type == SDL_EVENT_WINDOW_MAXIMIZED) {
                if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN)) {
                    renderer->hidden = SDL_FALSE;
                }
            } else if (event->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED) {
                UpdateHDRProperties(renderer);
            }
        }
    } else if (event->type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED) {
        UpdateHDRProperties(renderer);
    }

    return 0;
}

int SDL_CreateWindowAndRenderer(const char *title, int width, int height, SDL_WindowFlags window_flags, SDL_Window **window, SDL_Renderer **renderer)
{
    SDL_bool hidden = (window_flags & SDL_WINDOW_HIDDEN) != 0;

    if (!window) {
        return SDL_InvalidParamError("window");
    }

    if (!renderer) {
        return SDL_InvalidParamError("renderer");
    }

    // Hide the window so if the renderer recreates it, we don't get a visual flash on screen
    window_flags |= SDL_WINDOW_HIDDEN;
    *window = SDL_CreateWindow(title, width, height, window_flags);
    if (!*window) {
        *renderer = NULL;
        return -1;
    }

    *renderer = SDL_CreateRenderer(*window, NULL);
    if (!*renderer) {
        SDL_DestroyWindow(*window);
        *window = NULL;
        return -1;
    }

    if (!hidden) {
        SDL_ShowWindow(*window);
    }

    return 0;
}

#ifndef SDL_RENDER_DISABLED
static SDL_INLINE void VerifyDrawQueueFunctions(const SDL_Renderer *renderer)
{
    /* all of these functions are required to be implemented, even as no-ops, so we don't
        have to check that they aren't NULL over and over. */
    SDL_assert(renderer->QueueSetViewport != NULL);
    SDL_assert(renderer->QueueSetDrawColor != NULL);
    SDL_assert(renderer->QueueDrawPoints != NULL);
    SDL_assert(renderer->QueueDrawLines != NULL || renderer->QueueGeometry != NULL);
    SDL_assert(renderer->QueueFillRects != NULL || renderer->QueueGeometry != NULL);
    SDL_assert(renderer->QueueCopy != NULL || renderer->QueueGeometry != NULL);
    SDL_assert(renderer->RunCommandQueue != NULL);
}

static SDL_RenderLineMethod SDL_GetRenderLineMethod(void)
{
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_LINE_METHOD);

    int method = 0;
    if (hint) {
        method = SDL_atoi(hint);
    }
    switch (method) {
    case 1:
        return SDL_RENDERLINEMETHOD_POINTS;
    case 2:
        return SDL_RENDERLINEMETHOD_LINES;
    case 3:
        return SDL_RENDERLINEMETHOD_GEOMETRY;
    default:
        return SDL_RENDERLINEMETHOD_POINTS;
    }
}

static void SDL_CalculateSimulatedVSyncInterval(SDL_Renderer *renderer, SDL_Window *window)
{
    SDL_DisplayID displayID = SDL_GetDisplayForWindow(window);
    const SDL_DisplayMode *mode;
    float refresh_rate;
    int num, den;

    if (displayID == 0) {
        displayID = SDL_GetPrimaryDisplay();
    }
    mode = SDL_GetDesktopDisplayMode(displayID);
    if (mode && mode->refresh_rate > 0.0f) {
        refresh_rate = mode->refresh_rate;
    } else {
        /* Pick a good default refresh rate */
        refresh_rate = 60.0f;
    }
    num = 100;
    den = (int)(100 * refresh_rate);
    renderer->simulate_vsync_interval_ns = (SDL_NS_PER_SECOND * num) / den;
}

#endif /* !SDL_RENDER_DISABLED */


SDL_Renderer *SDL_CreateRendererWithProperties(SDL_PropertiesID props)
{
#ifndef SDL_RENDER_DISABLED
    SDL_Window *window = (SDL_Window *)SDL_GetProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, NULL);
    SDL_Surface *surface = (SDL_Surface *)SDL_GetProperty(props, SDL_PROP_RENDERER_CREATE_SURFACE_POINTER, NULL);
    const char *name = SDL_GetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, NULL);
    const int n = SDL_GetNumRenderDrivers();
    const char *hint;
    int i, attempted = 0;
    SDL_PropertiesID new_props;

    SDL_Renderer *renderer = (SDL_Renderer *)SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        return NULL;
    }

    SDL_SetObjectValid(renderer, SDL_OBJECT_TYPE_RENDERER, SDL_TRUE);

#ifdef SDL_PLATFORM_ANDROID
    Android_ActivityMutex_Lock_Running();
#endif

    if ((!window && !surface) || (window && surface)) {
        SDL_InvalidParamError("window");
        goto error;
    }

    if (window && SDL_WindowHasSurface(window)) {
        SDL_SetError("Surface already associated with window");
        goto error;
    }

    if (window && SDL_GetRenderer(window)) {
        SDL_SetError("Renderer already associated with window");
        goto error;
    }

    hint = SDL_GetHint(SDL_HINT_RENDER_VSYNC);
    if (hint && *hint) {
        SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_GetHintBoolean(SDL_HINT_RENDER_VSYNC, SDL_TRUE));
    }

    if (surface) {
#if SDL_VIDEO_RENDER_SW
        const int rc = SW_CreateRendererForSurface(renderer, surface, props);
#else
        const int rc = SDL_SetError("SDL not built with software renderer");
#endif
        if (rc == -1) {
            goto error;
        }
    } else {
        int rc = -1;
        if (!name) {
            name = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
        }

        if (name) {
            for (i = 0; i < n; i++) {
                const SDL_RenderDriver *driver = render_drivers[i];
                if (SDL_strcasecmp(name, driver->name) == 0) {
                    // Create a new renderer instance
                    ++attempted;
                    rc = driver->CreateRenderer(renderer, window, props);
                    break;
                }
            }
        } else {
            for (i = 0; i < n; i++) {
                const SDL_RenderDriver *driver = render_drivers[i];
                // Create a new renderer instance
                ++attempted;
                rc = driver->CreateRenderer(renderer, window, props);
                if (rc == 0) {
                    break;  // Yay, we got one!
                }
                SDL_zerop(renderer);  // make sure we don't leave function pointers from a previous CreateRenderer() in this struct.
            }
        }

        if (rc == -1) {
            if (!name || !attempted) {
                SDL_SetError("Couldn't find matching render driver");
            }
            goto error;
        }
    }

    VerifyDrawQueueFunctions(renderer);

    renderer->window = window;
    renderer->target_mutex = SDL_CreateMutex();
    if (surface) {
        renderer->main_view.pixel_w = surface->w;
        renderer->main_view.pixel_h = surface->h;
    }
    renderer->main_view.viewport.w = -1;
    renderer->main_view.viewport.h = -1;
    renderer->main_view.scale.x = 1.0f;
    renderer->main_view.scale.y = 1.0f;
    renderer->view = &renderer->main_view;
    renderer->dpi_scale.x = 1.0f;
    renderer->dpi_scale.y = 1.0f;
    UpdatePixelViewport(renderer, &renderer->main_view);
    UpdatePixelClipRect(renderer, &renderer->main_view);
    UpdateMainViewDimensions(renderer);

    /* Default value, if not specified by the renderer back-end */
    if (renderer->rect_index_order[0] == 0 && renderer->rect_index_order[1] == 0) {
        renderer->rect_index_order[0] = 0;
        renderer->rect_index_order[1] = 1;
        renderer->rect_index_order[2] = 2;
        renderer->rect_index_order[3] = 0;
        renderer->rect_index_order[4] = 2;
        renderer->rect_index_order[5] = 3;
    }

    /* new textures start at zero, so we start at 1 so first render doesn't flush by accident. */
    renderer->render_command_generation = 1;

    if (renderer->software) {
        /* Software renderer always uses line method, for speed */
        renderer->line_method = SDL_RENDERLINEMETHOD_LINES;
    } else {
        renderer->line_method = SDL_GetRenderLineMethod();
    }

    renderer->SDR_white_point = 1.0f;
    renderer->HDR_headroom = 1.0f;
    renderer->color_scale = 1.0f;

    if (window) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_TRANSPARENT) {
            renderer->transparent_window = SDL_TRUE;
        }

        if (SDL_GetWindowFlags(window) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) {
            renderer->hidden = SDL_TRUE;
        }
    }

    new_props = SDL_GetRendererProperties(renderer);
    SDL_SetStringProperty(new_props, SDL_PROP_RENDERER_NAME_STRING, renderer->name);
    if (window) {
        SDL_SetProperty(new_props, SDL_PROP_RENDERER_WINDOW_POINTER, window);
    }
    if (surface) {
        SDL_SetProperty(new_props, SDL_PROP_RENDERER_SURFACE_POINTER, surface);
    }
    SDL_SetNumberProperty(new_props, SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER, renderer->output_colorspace);
    UpdateHDRProperties(renderer);

    if (window) {
        SDL_SetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_RENDERER_POINTER, renderer);
    }

    SDL_SetRenderViewport(renderer, NULL);

    if (window) {
        SDL_AddEventWatch(SDL_RendererEventWatch, renderer);
    }

    int vsync = (int)SDL_GetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 0);
    if (SDL_SetRenderVSync(renderer, vsync) < 0) {
        if (vsync == 0) {
            // Some renderers require vsync enabled
            SDL_SetRenderVSync(renderer, 1);
        }
    }
    SDL_CalculateSimulatedVSyncInterval(renderer, window);

    SDL_LogInfo(SDL_LOG_CATEGORY_RENDER,
                "Created renderer: %s", renderer->name);

    renderer->next = SDL_renderers;
    SDL_renderers = renderer;

#ifdef SDL_PLATFORM_ANDROID
    Android_ActivityMutex_Unlock();
#endif

    SDL_ClearError();

    return renderer;

error:

    SDL_SetObjectValid(renderer, SDL_OBJECT_TYPE_RENDERER, SDL_FALSE);

#ifdef SDL_PLATFORM_ANDROID
    Android_ActivityMutex_Unlock();
#endif
    SDL_free(renderer->texture_formats);
    SDL_free(renderer);
    return NULL;

#else
    SDL_SetError("SDL not built with rendering support");
    return NULL;
#endif
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, const char *name)
{
    SDL_Renderer *renderer;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, name);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    return renderer;
}

SDL_Renderer *SDL_CreateSoftwareRenderer(SDL_Surface *surface)
{
#if SDL_VIDEO_RENDER_SW
    SDL_Renderer *renderer;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetProperty(props, SDL_PROP_RENDERER_CREATE_SURFACE_POINTER, surface);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);
    return renderer;
#else
    SDL_SetError("SDL not built with rendering support");
    return NULL;
#endif /* !SDL_RENDER_DISABLED */
}

SDL_Renderer *SDL_GetRenderer(SDL_Window *window)
{
    return (SDL_Renderer *)SDL_GetProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_RENDERER_POINTER, NULL);
}

SDL_Window *SDL_GetRenderWindow(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, NULL);
    return renderer->window;
}

const char *SDL_GetRendererName(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, NULL);

    return renderer->name;
}

SDL_PropertiesID SDL_GetRendererProperties(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, 0);

    if (renderer->props == 0) {
        renderer->props = SDL_CreateProperties();
    }
    return renderer->props;
}

int SDL_GetRenderOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (renderer->GetOutputSize) {
        return renderer->GetOutputSize(renderer, w, h);
    } else if (renderer->window) {
        return SDL_GetWindowSizeInPixels(renderer->window, w, h);
    } else {
        SDL_assert(!"This should never happen");
        return SDL_SetError("Renderer doesn't support querying output size");
    }
}

int SDL_GetCurrentRenderOutputSize(SDL_Renderer *renderer, int *w, int *h)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (w) {
        *w = renderer->view->pixel_w;
    }
    if (h) {
        *h = renderer->view->pixel_h;
    }
    return 0;
}

static SDL_bool IsSupportedBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    switch (blendMode) {
    /* These are required to be supported by all renderers */
    case SDL_BLENDMODE_NONE:
    case SDL_BLENDMODE_BLEND:
    case SDL_BLENDMODE_ADD:
    case SDL_BLENDMODE_MOD:
    case SDL_BLENDMODE_MUL:
        return SDL_TRUE;

    default:
        return renderer->SupportsBlendMode && renderer->SupportsBlendMode(renderer, blendMode);
    }
}

static SDL_bool IsSupportedFormat(SDL_Renderer *renderer, SDL_PixelFormatEnum format)
{
    int i;

    for (i = 0; i < renderer->num_texture_formats; ++i) {
        if (renderer->texture_formats[i] == format) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static Uint32 GetClosestSupportedFormat(SDL_Renderer *renderer, SDL_PixelFormatEnum format)
{
    int i;

    if (SDL_ISPIXELFORMAT_FOURCC(format)) {
        /* Look for an exact match */
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (renderer->texture_formats[i] == format) {
                return renderer->texture_formats[i];
            }
        }
    } else if (SDL_ISPIXELFORMAT_10BIT(format) || SDL_ISPIXELFORMAT_FLOAT(format)) {
        if (SDL_ISPIXELFORMAT_10BIT(format)) {
            for (i = 0; i < renderer->num_texture_formats; ++i) {
                if (SDL_ISPIXELFORMAT_10BIT(renderer->texture_formats[i])) {
                    return renderer->texture_formats[i];
                }
            }
        }
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (SDL_ISPIXELFORMAT_FLOAT(renderer->texture_formats[i])) {
                return renderer->texture_formats[i];
            }
        }
    } else {
        SDL_bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(format);

        /* We just want to match the first format that has the same channels */
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (!SDL_ISPIXELFORMAT_FOURCC(renderer->texture_formats[i]) &&
                SDL_ISPIXELFORMAT_ALPHA(renderer->texture_formats[i]) == hasAlpha) {
                return renderer->texture_formats[i];
            }
        }
    }
    return renderer->texture_formats[0];
}

SDL_Texture *SDL_CreateTextureWithProperties(SDL_Renderer *renderer, SDL_PropertiesID props)
{
    SDL_Texture *texture;
    SDL_PixelFormatEnum format = (SDL_PixelFormatEnum)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_UNKNOWN);
    int access = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
    int w = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, 0);
    int h = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, 0);
    SDL_Colorspace default_colorspace;
    SDL_bool texture_is_fourcc_and_target;

    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (!format) {
        format = renderer->texture_formats[0];
    }
    if (SDL_BYTESPERPIXEL(format) == 0) {
        SDL_SetError("Invalid texture format");
        return NULL;
    }
    if (SDL_ISPIXELFORMAT_INDEXED(format)) {
        if (!IsSupportedFormat(renderer, format)) {
            SDL_SetError("Palettized textures are not supported");
            return NULL;
        }
    }
    if (w <= 0 || h <= 0) {
        SDL_SetError("Texture dimensions can't be 0");
        return NULL;
    }
    int max_texture_size = (int)SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    if (max_texture_size && (w > max_texture_size || h > max_texture_size)) {
        SDL_SetError("Texture dimensions are limited to %dx%d", max_texture_size, max_texture_size);
        return NULL;
    }

    default_colorspace = SDL_GetDefaultColorspaceForFormat(format);

    texture = (SDL_Texture *)SDL_calloc(1, sizeof(*texture));
    if (!texture) {
        return NULL;
    }
    SDL_SetObjectValid(texture, SDL_OBJECT_TYPE_TEXTURE, SDL_TRUE);
    texture->colorspace = (SDL_Colorspace)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, default_colorspace);
    texture->format = format;
    texture->access = access;
    texture->w = w;
    texture->h = h;
    texture->color.r = 1.0f;
    texture->color.g = 1.0f;
    texture->color.b = 1.0f;
    texture->color.a = 1.0f;
    texture->scaleMode = SDL_SCALEMODE_LINEAR;
    texture->view.pixel_w = w;
    texture->view.pixel_h = h;
    texture->view.viewport.w = -1;
    texture->view.viewport.h = -1;
    texture->view.scale.x = 1.0f;
    texture->view.scale.y = 1.0f;
    texture->renderer = renderer;
    texture->next = renderer->textures;
    if (renderer->textures) {
        renderer->textures->prev = texture;
    }
    renderer->textures = texture;

    UpdatePixelViewport(renderer, &texture->view);
    UpdatePixelClipRect(renderer, &texture->view);

    texture->SDR_white_point = SDL_GetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT, SDL_GetDefaultSDRWhitePoint(texture->colorspace));
    texture->HDR_headroom = SDL_GetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_HDR_HEADROOM_FLOAT, SDL_GetDefaultHDRHeadroom(texture->colorspace));

    /* FOURCC format cannot be used directly by renderer back-ends for target texture */
    texture_is_fourcc_and_target = (access == SDL_TEXTUREACCESS_TARGET && SDL_ISPIXELFORMAT_FOURCC(format));

    if (!texture_is_fourcc_and_target && IsSupportedFormat(renderer, format)) {
        if (renderer->CreateTexture(renderer, texture, props) < 0) {
            SDL_DestroyTexture(texture);
            return NULL;
        }
    } else {
        int closest_format;
        SDL_PropertiesID native_props = SDL_CreateProperties();

        if (!texture_is_fourcc_and_target) {
            closest_format = GetClosestSupportedFormat(renderer, format);
        } else {
            closest_format = renderer->texture_formats[0];
        }

        default_colorspace = SDL_GetDefaultColorspaceForFormat(closest_format);
        if (SDL_COLORSPACETYPE(texture->colorspace) == SDL_COLORSPACETYPE(default_colorspace)) {
            SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, texture->colorspace);
        } else {
            SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, default_colorspace);
        }
        SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, closest_format);
        SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, texture->access);
        SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, texture->w);
        SDL_SetNumberProperty(native_props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, texture->h);

        texture->native = SDL_CreateTextureWithProperties(renderer, native_props);
        SDL_DestroyProperties(native_props);
        if (!texture->native) {
            SDL_DestroyTexture(texture);
            return NULL;
        }

        SDL_SetProperty(SDL_GetTextureProperties(texture->native), SDL_PROP_TEXTURE_PARENT_POINTER, texture);

        /* Swap textures to have texture before texture->native in the list */
        texture->native->next = texture->next;
        if (texture->native->next) {
            texture->native->next->prev = texture->native;
        }
        texture->prev = texture->native->prev;
        if (texture->prev) {
            texture->prev->next = texture;
        }
        texture->native->prev = texture;
        texture->next = texture->native;
        renderer->textures = texture;

        if (SDL_ISPIXELFORMAT_FOURCC(texture->format)) {
#if SDL_HAVE_YUV
            texture->yuv = SDL_SW_CreateYUVTexture(format, w, h);
#else
            SDL_SetError("SDL not built with YUV support");
#endif
            if (!texture->yuv) {
                SDL_DestroyTexture(texture);
                return NULL;
            }
        } else if (access == SDL_TEXTUREACCESS_STREAMING) {
            /* The pitch is 4 byte aligned */
            texture->pitch = (((w * SDL_BYTESPERPIXEL(format)) + 3) & ~3);
            texture->pixels = SDL_calloc(1, (size_t)texture->pitch * h);
            if (!texture->pixels) {
                SDL_DestroyTexture(texture);
                return NULL;
            }
        }
    }

    /* Now set the properties for the new texture */
    props = SDL_GetTextureProperties(texture);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_COLORSPACE_NUMBER, texture->colorspace);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, texture->format);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, texture->access);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, texture->w);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, texture->h);
    SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_SDR_WHITE_POINT_FLOAT, texture->SDR_white_point);
    if (texture->HDR_headroom > 0.0f) {
        SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_HDR_HEADROOM_FLOAT, texture->HDR_headroom);
    }
    return texture;
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, SDL_PixelFormatEnum format, int access, int w, int h)
{
    SDL_Texture *texture;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, format);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, access);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, w);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, h);
    texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    return texture;
}

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface)
{
    const SDL_PixelFormat *fmt;
    SDL_bool needAlpha;
    SDL_bool direct_update;
    int i;
    SDL_PixelFormatEnum format = SDL_PIXELFORMAT_UNKNOWN;
    SDL_Texture *texture;
    SDL_PropertiesID surface_props, props;
    SDL_Colorspace surface_colorspace = SDL_COLORSPACE_UNKNOWN;
    SDL_Colorspace texture_colorspace = SDL_COLORSPACE_UNKNOWN;

    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (!surface) {
        SDL_InvalidParamError("SDL_CreateTextureFromSurface(): surface");
        return NULL;
    }

    /* See what the best texture format is */
    fmt = surface->format;
    if (fmt->Amask || SDL_SurfaceHasColorKey(surface)) {
        needAlpha = SDL_TRUE;
    } else {
        needAlpha = SDL_FALSE;
    }

    /* If Palette contains alpha values, promotes to alpha format */
    if (fmt->palette) {
        SDL_bool is_opaque, has_alpha_channel;
        SDL_DetectPalette(fmt->palette, &is_opaque, &has_alpha_channel);
        if (!is_opaque) {
            needAlpha = SDL_TRUE;
        }
    }

    if (SDL_GetSurfaceColorspace(surface, &surface_colorspace) < 0) {
        return NULL;
    }
    texture_colorspace = surface_colorspace;

    /* Try to have the best pixel format for the texture */
    /* No alpha, but a colorkey => promote to alpha */
    if (!fmt->Amask && SDL_SurfaceHasColorKey(surface)) {
        if (fmt->format == SDL_PIXELFORMAT_XRGB8888) {
            for (i = 0; i < renderer->num_texture_formats; ++i) {
                if (renderer->texture_formats[i] == SDL_PIXELFORMAT_ARGB8888) {
                    format = SDL_PIXELFORMAT_ARGB8888;
                    break;
                }
            }
        } else if (fmt->format == SDL_PIXELFORMAT_XBGR8888) {
            for (i = 0; i < renderer->num_texture_formats; ++i) {
                if (renderer->texture_formats[i] == SDL_PIXELFORMAT_ABGR8888) {
                    format = SDL_PIXELFORMAT_ABGR8888;
                    break;
                }
            }
        }
    } else {
        /* Exact match would be fine */
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (renderer->texture_formats[i] == fmt->format) {
                format = fmt->format;
                break;
            }
        }
    }

    /* Look for 10-bit pixel formats if needed */
    if (format == SDL_PIXELFORMAT_UNKNOWN && SDL_ISPIXELFORMAT_10BIT(fmt->format)) {
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (SDL_ISPIXELFORMAT_10BIT(renderer->texture_formats[i])) {
                format = renderer->texture_formats[i];
                break;
            }
        }
    }

    /* Look for floating point pixel formats if needed */
    if (format == SDL_PIXELFORMAT_UNKNOWN &&
        (SDL_ISPIXELFORMAT_10BIT(fmt->format) || SDL_ISPIXELFORMAT_FLOAT(fmt->format))) {
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (SDL_ISPIXELFORMAT_FLOAT(renderer->texture_formats[i])) {
                format = renderer->texture_formats[i];
                break;
            }
        }
    }

    /* Fallback, choose a valid pixel format */
    if (format == SDL_PIXELFORMAT_UNKNOWN) {
        format = renderer->texture_formats[0];
        for (i = 0; i < renderer->num_texture_formats; ++i) {
            if (!SDL_ISPIXELFORMAT_FOURCC(renderer->texture_formats[i]) &&
                SDL_ISPIXELFORMAT_ALPHA(renderer->texture_formats[i]) == needAlpha) {
                format = renderer->texture_formats[i];
                break;
            }
        }
    }

    if (surface_colorspace == SDL_COLORSPACE_SRGB_LINEAR ||
        SDL_COLORSPACETRANSFER(surface_colorspace) == SDL_TRANSFER_CHARACTERISTICS_PQ) {
        if (SDL_ISPIXELFORMAT_FLOAT(format)) {
            texture_colorspace = SDL_COLORSPACE_SRGB_LINEAR;
        } else if (SDL_ISPIXELFORMAT_10BIT(format)) {
            texture_colorspace = SDL_COLORSPACE_HDR10;
        } else {
            texture_colorspace = SDL_COLORSPACE_SRGB;
        }
    }

    if (format == surface->format->format && texture_colorspace == surface_colorspace) {
        if (surface->format->Amask && SDL_SurfaceHasColorKey(surface)) {
            /* Surface and Renderer formats are identical.
             * Intermediate conversion is needed to convert color key to alpha (SDL_ConvertColorkeyToAlpha()). */
            direct_update = SDL_FALSE;
        } else {
            /* Update Texture directly */
            direct_update = SDL_TRUE;
        }
    } else {
        /* Surface and Renderer formats are different, it needs an intermediate conversion. */
        direct_update = SDL_FALSE;
    }

    if (surface->flags & SDL_SURFACE_USES_PROPERTIES) {
        surface_props = SDL_GetSurfaceProperties(surface);
    } else {
        surface_props = 0;
    }

    props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, texture_colorspace);
    if (surface_colorspace == texture_colorspace) {
        SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT,
                             SDL_GetSurfaceSDRWhitePoint(surface, surface_colorspace));
    }
    SDL_SetFloatProperty(props, SDL_PROP_TEXTURE_CREATE_HDR_HEADROOM_FLOAT,
                         SDL_GetSurfaceHDRHeadroom(surface, surface_colorspace));
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, format);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, surface->w);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, surface->h);
    texture = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!texture) {
        return NULL;
    }

    if (direct_update) {
        if (SDL_MUSTLOCK(surface)) {
            SDL_LockSurface(surface);
            SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
            SDL_UnlockSurface(surface);
        } else {
            SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
        }
    } else {
        SDL_Surface *temp = NULL;

        /* Set up a destination surface for the texture update */
        temp = SDL_ConvertSurfaceFormatAndColorspace(surface, format, texture_colorspace, surface_props);
        if (temp) {
            SDL_UpdateTexture(texture, NULL, temp->pixels, temp->pitch);
            SDL_DestroySurface(temp);
        } else {
            SDL_DestroyTexture(texture);
            return NULL;
        }
    }

    {
        Uint8 r, g, b, a;
        SDL_BlendMode blendMode;

        SDL_GetSurfaceColorMod(surface, &r, &g, &b);
        SDL_SetTextureColorMod(texture, r, g, b);

        SDL_GetSurfaceAlphaMod(surface, &a);
        SDL_SetTextureAlphaMod(texture, a);

        if (SDL_SurfaceHasColorKey(surface)) {
            /* We converted to a texture with alpha format */
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        } else {
            SDL_GetSurfaceBlendMode(surface, &blendMode);
            SDL_SetTextureBlendMode(texture, blendMode);
        }
    }
    return texture;
}

SDL_Renderer *SDL_GetRendererFromTexture(SDL_Texture *texture)
{
    CHECK_TEXTURE_MAGIC(texture, NULL);
    return texture->renderer;
}

SDL_PropertiesID SDL_GetTextureProperties(SDL_Texture *texture)
{
    CHECK_TEXTURE_MAGIC(texture, 0);

    if (texture->props == 0) {
        texture->props = SDL_CreateProperties();
    }
    return texture->props;
}

int SDL_GetTextureSize(SDL_Texture *texture, float *w, float *h)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (w) {
        *w = (float)texture->w;
    }
    if (h) {
        *h = (float)texture->h;
    }
    return 0;
}

int SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b)
{
    const float fR = (float)r / 255.0f;
    const float fG = (float)g / 255.0f;
    const float fB = (float)b / 255.0f;

    return SDL_SetTextureColorModFloat(texture, fR, fG, fB);
}

int SDL_SetTextureColorModFloat(SDL_Texture *texture, float r, float g, float b)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    texture->color.r = r;
    texture->color.g = g;
    texture->color.b = b;
    if (texture->native) {
        return SDL_SetTextureColorModFloat(texture->native, r, g, b);
    }
    return 0;
}

int SDL_GetTextureColorMod(SDL_Texture *texture, Uint8 *r, Uint8 *g, Uint8 *b)
{
    float fR, fG, fB;

    if (SDL_GetTextureColorModFloat(texture, &fR, &fG, &fB) < 0) {
        return -1;
    }

    if (r) {
        *r = (Uint8)(fR * 255.0f);
    }
    if (g) {
        *g = (Uint8)(fG * 255.0f);
    }
    if (b) {
        *b = (Uint8)(fB * 255.0f);
    }
    return 0;
}

int SDL_GetTextureColorModFloat(SDL_Texture *texture, float *r, float *g, float *b)
{
    SDL_FColor color;

    CHECK_TEXTURE_MAGIC(texture, -1);

    color = texture->color;

    if (r) {
        *r = color.r;
    }
    if (g) {
        *g = color.g;
    }
    if (b) {
        *b = color.b;
    }
    return 0;
}

int SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha)
{
    const float fA = (float)alpha / 255.0f;

    return SDL_SetTextureAlphaModFloat(texture, fA);
}

int SDL_SetTextureAlphaModFloat(SDL_Texture *texture, float alpha)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    texture->color.a = alpha;
    if (texture->native) {
        return SDL_SetTextureAlphaModFloat(texture->native, alpha);
    }
    return 0;
}

int SDL_GetTextureAlphaMod(SDL_Texture *texture, Uint8 *alpha)
{
    float fA = 1.0f;

    if (SDL_GetTextureAlphaModFloat(texture, &fA) < 0) {
        return -1;
    }

    if (alpha) {
        *alpha = (Uint8)(fA * 255.0f);
    }
    return 0;
}

int SDL_GetTextureAlphaModFloat(SDL_Texture *texture, float *alpha)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (alpha) {
        *alpha = texture->color.a;
    }
    return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode)
{
    SDL_Renderer *renderer;

    CHECK_TEXTURE_MAGIC(texture, -1);

    renderer = texture->renderer;
    if (!IsSupportedBlendMode(renderer, blendMode)) {
        return SDL_Unsupported();
    }
    texture->blendMode = blendMode;
    if (texture->native) {
        return SDL_SetTextureBlendMode(texture->native, blendMode);
    }
    return 0;
}

int SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (blendMode) {
        *blendMode = texture->blendMode;
    }
    return 0;
}

int SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode scaleMode)
{
    SDL_Renderer *renderer;

    CHECK_TEXTURE_MAGIC(texture, -1);

    renderer = texture->renderer;
    texture->scaleMode = scaleMode;
    if (texture->native) {
        return SDL_SetTextureScaleMode(texture->native, scaleMode);
    } else {
        renderer->SetTextureScaleMode(renderer, texture, scaleMode);
    }
    return 0;
}

int SDL_GetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode *scaleMode)
{
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (scaleMode) {
        *scaleMode = texture->scaleMode;
    }
    return 0;
}

#if SDL_HAVE_YUV
static int SDL_UpdateTextureYUV(SDL_Texture *texture, const SDL_Rect *rect,
                                const void *pixels, int pitch)
{
    SDL_Texture *native = texture->native;
    SDL_Rect full_rect;

    if (SDL_SW_UpdateYUVTexture(texture->yuv, rect, pixels, pitch) < 0) {
        return -1;
    }

    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = texture->w;
    full_rect.h = texture->h;
    rect = &full_rect;

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        /* We can lock the texture and copy to it */
        void *native_pixels = NULL;
        int native_pitch = 0;

        if (SDL_LockTexture(native, rect, &native_pixels, &native_pitch) < 0) {
            return -1;
        }
        SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                            rect->w, rect->h, native_pixels, native_pitch);
        SDL_UnlockTexture(native);
    } else {
        /* Use a temporary buffer for updating */
        const int temp_pitch = (((rect->w * SDL_BYTESPERPIXEL(native->format)) + 3) & ~3);
        const size_t alloclen = (size_t)rect->h * temp_pitch;
        if (alloclen > 0) {
            void *temp_pixels = SDL_malloc(alloclen);
            if (!temp_pixels) {
                return -1;
            }
            SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                                rect->w, rect->h, temp_pixels, temp_pitch);
            SDL_UpdateTexture(native, rect, temp_pixels, temp_pitch);
            SDL_free(temp_pixels);
        }
    }
    return 0;
}
#endif /* SDL_HAVE_YUV */

static int SDL_UpdateTextureNative(SDL_Texture *texture, const SDL_Rect *rect,
                                   const void *pixels, int pitch)
{
    SDL_Texture *native = texture->native;

    if (!rect->w || !rect->h) {
        return 0; /* nothing to do. */
    }

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        /* We can lock the texture and copy to it */
        void *native_pixels = NULL;
        int native_pitch = 0;

        if (SDL_LockTexture(native, rect, &native_pixels, &native_pitch) < 0) {
            return -1;
        }
        SDL_ConvertPixels(rect->w, rect->h,
                          texture->format, pixels, pitch,
                          native->format, native_pixels, native_pitch);
        SDL_UnlockTexture(native);
    } else {
        /* Use a temporary buffer for updating */
        const int temp_pitch = (((rect->w * SDL_BYTESPERPIXEL(native->format)) + 3) & ~3);
        const size_t alloclen = (size_t)rect->h * temp_pitch;
        if (alloclen > 0) {
            void *temp_pixels = SDL_malloc(alloclen);
            if (!temp_pixels) {
                return -1;
            }
            SDL_ConvertPixels(rect->w, rect->h,
                              texture->format, pixels, pitch,
                              native->format, temp_pixels, temp_pitch);
            SDL_UpdateTexture(native, rect, temp_pixels, temp_pitch);
            SDL_free(temp_pixels);
        }
    }
    return 0;
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch)
{
    SDL_Rect real_rect;

    CHECK_TEXTURE_MAGIC(texture, -1);

    if (!pixels) {
        return SDL_InvalidParamError("pixels");
    }
    if (!pitch) {
        return SDL_InvalidParamError("pitch");
    }

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = texture->w;
    real_rect.h = texture->h;
    if (rect) {
        if (!SDL_GetRectIntersection(rect, &real_rect, &real_rect)) {
            return 0;
        }
    }

    if (real_rect.w == 0 || real_rect.h == 0) {
        return 0; /* nothing to do. */
#if SDL_HAVE_YUV
    } else if (texture->yuv) {
        return SDL_UpdateTextureYUV(texture, &real_rect, pixels, pitch);
#endif
    } else if (texture->native) {
        return SDL_UpdateTextureNative(texture, &real_rect, pixels, pitch);
    } else {
        SDL_Renderer *renderer = texture->renderer;
        if (FlushRenderCommandsIfTextureNeeded(texture) < 0) {
            return -1;
        }
        return renderer->UpdateTexture(renderer, texture, &real_rect, pixels, pitch);
    }
}

#if SDL_HAVE_YUV
static int SDL_UpdateTextureYUVPlanar(SDL_Texture *texture, const SDL_Rect *rect,
                                      const Uint8 *Yplane, int Ypitch,
                                      const Uint8 *Uplane, int Upitch,
                                      const Uint8 *Vplane, int Vpitch)
{
    SDL_Texture *native = texture->native;
    SDL_Rect full_rect;

    if (SDL_SW_UpdateYUVTexturePlanar(texture->yuv, rect, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch) < 0) {
        return -1;
    }

    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = texture->w;
    full_rect.h = texture->h;
    rect = &full_rect;

    if (!rect->w || !rect->h) {
        return 0; /* nothing to do. */
    }

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        /* We can lock the texture and copy to it */
        void *native_pixels = NULL;
        int native_pitch = 0;

        if (SDL_LockTexture(native, rect, &native_pixels, &native_pitch) < 0) {
            return -1;
        }
        SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                            rect->w, rect->h, native_pixels, native_pitch);
        SDL_UnlockTexture(native);
    } else {
        /* Use a temporary buffer for updating */
        const int temp_pitch = (((rect->w * SDL_BYTESPERPIXEL(native->format)) + 3) & ~3);
        const size_t alloclen = (size_t)rect->h * temp_pitch;
        if (alloclen > 0) {
            void *temp_pixels = SDL_malloc(alloclen);
            if (!temp_pixels) {
                return -1;
            }
            SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                                rect->w, rect->h, temp_pixels, temp_pitch);
            SDL_UpdateTexture(native, rect, temp_pixels, temp_pitch);
            SDL_free(temp_pixels);
        }
    }
    return 0;
}

static int SDL_UpdateTextureNVPlanar(SDL_Texture *texture, const SDL_Rect *rect,
                                     const Uint8 *Yplane, int Ypitch,
                                     const Uint8 *UVplane, int UVpitch)
{
    SDL_Texture *native = texture->native;
    SDL_Rect full_rect;

    if (SDL_SW_UpdateNVTexturePlanar(texture->yuv, rect, Yplane, Ypitch, UVplane, UVpitch) < 0) {
        return -1;
    }

    full_rect.x = 0;
    full_rect.y = 0;
    full_rect.w = texture->w;
    full_rect.h = texture->h;
    rect = &full_rect;

    if (!rect->w || !rect->h) {
        return 0; /* nothing to do. */
    }

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        /* We can lock the texture and copy to it */
        void *native_pixels = NULL;
        int native_pitch = 0;

        if (SDL_LockTexture(native, rect, &native_pixels, &native_pitch) < 0) {
            return -1;
        }
        SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                            rect->w, rect->h, native_pixels, native_pitch);
        SDL_UnlockTexture(native);
    } else {
        /* Use a temporary buffer for updating */
        const int temp_pitch = (((rect->w * SDL_BYTESPERPIXEL(native->format)) + 3) & ~3);
        const size_t alloclen = (size_t)rect->h * temp_pitch;
        if (alloclen > 0) {
            void *temp_pixels = SDL_malloc(alloclen);
            if (!temp_pixels) {
                return -1;
            }
            SDL_SW_CopyYUVToRGB(texture->yuv, rect, native->format,
                                rect->w, rect->h, temp_pixels, temp_pitch);
            SDL_UpdateTexture(native, rect, temp_pixels, temp_pitch);
            SDL_free(temp_pixels);
        }
    }
    return 0;
}

#endif /* SDL_HAVE_YUV */

int SDL_UpdateYUVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                         const Uint8 *Yplane, int Ypitch,
                         const Uint8 *Uplane, int Upitch,
                         const Uint8 *Vplane, int Vpitch)
{
#if SDL_HAVE_YUV
    SDL_Renderer *renderer;
    SDL_Rect real_rect;

    CHECK_TEXTURE_MAGIC(texture, -1);

    if (!Yplane) {
        return SDL_InvalidParamError("Yplane");
    }
    if (!Ypitch) {
        return SDL_InvalidParamError("Ypitch");
    }
    if (!Uplane) {
        return SDL_InvalidParamError("Uplane");
    }
    if (!Upitch) {
        return SDL_InvalidParamError("Upitch");
    }
    if (!Vplane) {
        return SDL_InvalidParamError("Vplane");
    }
    if (!Vpitch) {
        return SDL_InvalidParamError("Vpitch");
    }

    if (texture->format != SDL_PIXELFORMAT_YV12 &&
        texture->format != SDL_PIXELFORMAT_IYUV) {
        return SDL_SetError("Texture format must by YV12 or IYUV");
    }

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = texture->w;
    real_rect.h = texture->h;
    if (rect) {
        SDL_GetRectIntersection(rect, &real_rect, &real_rect);
    }

    if (real_rect.w == 0 || real_rect.h == 0) {
        return 0; /* nothing to do. */
    }

    if (texture->yuv) {
        return SDL_UpdateTextureYUVPlanar(texture, &real_rect, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch);
    } else {
        SDL_assert(!texture->native);
        renderer = texture->renderer;
        SDL_assert(renderer->UpdateTextureYUV);
        if (renderer->UpdateTextureYUV) {
            if (FlushRenderCommandsIfTextureNeeded(texture) < 0) {
                return -1;
            }
            return renderer->UpdateTextureYUV(renderer, texture, &real_rect, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch);
        } else {
            return SDL_Unsupported();
        }
    }
#else
    return -1;
#endif
}

int SDL_UpdateNVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                        const Uint8 *Yplane, int Ypitch,
                        const Uint8 *UVplane, int UVpitch)
{
#if SDL_HAVE_YUV
    SDL_Renderer *renderer;
    SDL_Rect real_rect;

    CHECK_TEXTURE_MAGIC(texture, -1);

    if (!Yplane) {
        return SDL_InvalidParamError("Yplane");
    }
    if (!Ypitch) {
        return SDL_InvalidParamError("Ypitch");
    }
    if (!UVplane) {
        return SDL_InvalidParamError("UVplane");
    }
    if (!UVpitch) {
        return SDL_InvalidParamError("UVpitch");
    }

    if (texture->format != SDL_PIXELFORMAT_NV12 &&
        texture->format != SDL_PIXELFORMAT_NV21) {
        return SDL_SetError("Texture format must by NV12 or NV21");
    }

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = texture->w;
    real_rect.h = texture->h;
    if (rect) {
        SDL_GetRectIntersection(rect, &real_rect, &real_rect);
    }

    if (real_rect.w == 0 || real_rect.h == 0) {
        return 0; /* nothing to do. */
    }

    if (texture->yuv) {
        return SDL_UpdateTextureNVPlanar(texture, &real_rect, Yplane, Ypitch, UVplane, UVpitch);
    } else {
        SDL_assert(!texture->native);
        renderer = texture->renderer;
        SDL_assert(renderer->UpdateTextureNV);
        if (renderer->UpdateTextureNV) {
            if (FlushRenderCommandsIfTextureNeeded(texture) < 0) {
                return -1;
            }
            return renderer->UpdateTextureNV(renderer, texture, &real_rect, Yplane, Ypitch, UVplane, UVpitch);
        } else {
            return SDL_Unsupported();
        }
    }
#else
    return -1;
#endif
}

#if SDL_HAVE_YUV
static int SDL_LockTextureYUV(SDL_Texture *texture, const SDL_Rect *rect,
                              void **pixels, int *pitch)
{
    return SDL_SW_LockYUVTexture(texture->yuv, rect, pixels, pitch);
}
#endif /* SDL_HAVE_YUV */

static int SDL_LockTextureNative(SDL_Texture *texture, const SDL_Rect *rect,
                                 void **pixels, int *pitch)
{
    texture->locked_rect = *rect;
    *pixels = (void *)((Uint8 *)texture->pixels +
                       rect->y * texture->pitch +
                       rect->x * SDL_BYTESPERPIXEL(texture->format));
    *pitch = texture->pitch;
    return 0;
}

int SDL_LockTexture(SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch)
{
    SDL_Rect full_rect;

    CHECK_TEXTURE_MAGIC(texture, -1);

    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        return SDL_SetError("SDL_LockTexture(): texture must be streaming");
    }

    if (!rect) {
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.w = texture->w;
        full_rect.h = texture->h;
        rect = &full_rect;
    }

#if SDL_HAVE_YUV
    if (texture->yuv) {
        if (FlushRenderCommandsIfTextureNeeded(texture) < 0) {
            return -1;
        }
        return SDL_LockTextureYUV(texture, rect, pixels, pitch);
    } else
#endif
        if (texture->native) {
        /* Calls a real SDL_LockTexture/SDL_UnlockTexture on unlock, flushing then. */
        return SDL_LockTextureNative(texture, rect, pixels, pitch);
    } else {
        SDL_Renderer *renderer = texture->renderer;
        if (FlushRenderCommandsIfTextureNeeded(texture) < 0) {
            return -1;
        }
        return renderer->LockTexture(renderer, texture, rect, pixels, pitch);
    }
}

int SDL_LockTextureToSurface(SDL_Texture *texture, const SDL_Rect *rect, SDL_Surface **surface)
{
    SDL_Rect real_rect;
    void *pixels = NULL;
    int pitch = 0; /* fix static analysis */
    int ret;

    if (!texture || !surface) {
        return -1;
    }

    real_rect.x = 0;
    real_rect.y = 0;
    real_rect.w = texture->w;
    real_rect.h = texture->h;
    if (rect) {
        SDL_GetRectIntersection(rect, &real_rect, &real_rect);
    }

    ret = SDL_LockTexture(texture, &real_rect, &pixels, &pitch);
    if (ret < 0) {
        return ret;
    }

    texture->locked_surface = SDL_CreateSurfaceFrom(pixels, real_rect.w, real_rect.h, pitch, texture->format);
    if (!texture->locked_surface) {
        SDL_UnlockTexture(texture);
        return -1;
    }

    *surface = texture->locked_surface;
    return 0;
}

#if SDL_HAVE_YUV
static void SDL_UnlockTextureYUV(SDL_Texture *texture)
{
    SDL_Texture *native = texture->native;
    void *native_pixels = NULL;
    int native_pitch = 0;
    SDL_Rect rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = texture->w;
    rect.h = texture->h;

    if (SDL_LockTexture(native, &rect, &native_pixels, &native_pitch) < 0) {
        return;
    }
    SDL_SW_CopyYUVToRGB(texture->yuv, &rect, native->format,
                        rect.w, rect.h, native_pixels, native_pitch);
    SDL_UnlockTexture(native);
}
#endif /* SDL_HAVE_YUV */

static void SDL_UnlockTextureNative(SDL_Texture *texture)
{
    SDL_Texture *native = texture->native;
    void *native_pixels = NULL;
    int native_pitch = 0;
    const SDL_Rect *rect = &texture->locked_rect;
    const void *pixels = (void *)((Uint8 *)texture->pixels +
                                  rect->y * texture->pitch +
                                  rect->x * SDL_BYTESPERPIXEL(texture->format));
    int pitch = texture->pitch;

    if (SDL_LockTexture(native, rect, &native_pixels, &native_pitch) < 0) {
        return;
    }
    SDL_ConvertPixels(rect->w, rect->h,
                      texture->format, pixels, pitch,
                      native->format, native_pixels, native_pitch);
    SDL_UnlockTexture(native);
}

void SDL_UnlockTexture(SDL_Texture *texture)
{
    CHECK_TEXTURE_MAGIC(texture,);

    if (texture->access != SDL_TEXTUREACCESS_STREAMING) {
        return;
    }
#if SDL_HAVE_YUV
    if (texture->yuv) {
        SDL_UnlockTextureYUV(texture);
    } else
#endif
        if (texture->native) {
        SDL_UnlockTextureNative(texture);
    } else {
        SDL_Renderer *renderer = texture->renderer;
        renderer->UnlockTexture(renderer, texture);
    }

    SDL_DestroySurface(texture->locked_surface);
    texture->locked_surface = NULL;
}

static int SDL_SetRenderTargetInternal(SDL_Renderer *renderer, SDL_Texture *texture)
{
    /* texture == NULL is valid and means reset the target to the window */
    if (texture) {
        CHECK_TEXTURE_MAGIC(texture, -1);
        if (renderer != texture->renderer) {
            return SDL_SetError("Texture was not created with this renderer");
        }
        if (texture->access != SDL_TEXTUREACCESS_TARGET) {
            return SDL_SetError("Texture not created with SDL_TEXTUREACCESS_TARGET");
        }
        if (texture->native) {
            /* Always render to the native texture */
            texture = texture->native;
        }
    }

    if (texture == renderer->target) {
        /* Nothing to do! */
        return 0;
    }

    FlushRenderCommands(renderer); /* time to send everything to the GPU! */

    SDL_LockMutex(renderer->target_mutex);

    renderer->target = texture;
    if (texture) {
        renderer->view = &texture->view;
    } else {
        renderer->view = &renderer->main_view;
    }

    if (renderer->SetRenderTarget(renderer, texture) < 0) {
        SDL_UnlockMutex(renderer->target_mutex);
        return -1;
    }

    SDL_UnlockMutex(renderer->target_mutex);

    if (QueueCmdSetViewport(renderer) < 0) {
        return -1;
    }
    if (QueueCmdSetClipRect(renderer) < 0) {
        return -1;
    }

    /* All set! */
    return 0;
}

int SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    if (!texture && renderer->logical_target) {
        return SDL_SetRenderTargetInternal(renderer, renderer->logical_target);
    } else {
        return SDL_SetRenderTargetInternal(renderer, texture);
    }
}

SDL_Texture *SDL_GetRenderTarget(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (renderer->target == renderer->logical_target) {
        return NULL;
    } else {
        return (SDL_Texture *)SDL_GetProperty(SDL_GetTextureProperties(renderer->target), SDL_PROP_TEXTURE_PARENT_POINTER, renderer->target);
    }
}

static int UpdateLogicalPresentation(SDL_Renderer *renderer)
{
    float logical_w = 1.0f, logical_h = 1.0f;
    float output_w = (float)renderer->main_view.pixel_w;
    float output_h = (float)renderer->main_view.pixel_h;
    float want_aspect = 1.0f;
    float real_aspect = 1.0f;
    float scale;

    if (renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_DISABLED) {
        /* All done! */
        return 0;
    }

    if (SDL_GetTextureSize(renderer->logical_target, &logical_w, &logical_h) < 0) {
        goto error;
    }

    want_aspect = logical_w / logical_h;
    real_aspect = output_w / output_h;

    renderer->logical_src_rect.x = 0.0f;
    renderer->logical_src_rect.y = 0.0f;
    renderer->logical_src_rect.w = logical_w;
    renderer->logical_src_rect.h = logical_h;

    if (renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE) {
        if (want_aspect > real_aspect) {
            scale = (float)((int)output_w / (int)logical_w); /* This an integer division! */
        } else {
            scale = (float)((int)output_h / (int)logical_h); /* This an integer division! */
        }

        if (scale < 1.0f) {
            scale = 1.0f;
        }

        renderer->logical_dst_rect.w = SDL_floorf(logical_w * scale);
        renderer->logical_dst_rect.x = (output_w - renderer->logical_dst_rect.w) / 2.0f;
        renderer->logical_dst_rect.h = SDL_floorf(logical_h * scale);
        renderer->logical_dst_rect.y = (output_h - renderer->logical_dst_rect.h) / 2.0f;

    } else if (renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_STRETCH ||
               SDL_fabsf(want_aspect - real_aspect) < 0.0001f) {
        renderer->logical_dst_rect.x = 0.0f;
        renderer->logical_dst_rect.y = 0.0f;
        renderer->logical_dst_rect.w = output_w;
        renderer->logical_dst_rect.h = output_h;

    } else if (want_aspect > real_aspect) {
        if (renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX) {
            /* We want a wider aspect ratio than is available - letterbox it */
            scale = output_w / logical_w;
            renderer->logical_dst_rect.x = 0.0f;
            renderer->logical_dst_rect.w = output_w;
            renderer->logical_dst_rect.h = SDL_floorf(logical_h * scale);
            renderer->logical_dst_rect.y = (output_h - renderer->logical_dst_rect.h) / 2.0f;
        } else { /* renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_OVERSCAN */
            /* We want a wider aspect ratio than is available -
               zoom so logical height matches the real height
               and the width will grow off the screen
             */
            scale = output_h / logical_h;
            renderer->logical_dst_rect.y = 0.0f;
            renderer->logical_dst_rect.h = output_h;
            renderer->logical_dst_rect.w = SDL_floorf(logical_w * scale);
            renderer->logical_dst_rect.x = (output_w - renderer->logical_dst_rect.w) / 2.0f;
        }
    } else {
        if (renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX) {
            /* We want a narrower aspect ratio than is available - use side-bars */
            scale = output_h / logical_h;
            renderer->logical_dst_rect.y = 0.0f;
            renderer->logical_dst_rect.h = output_h;
            renderer->logical_dst_rect.w = SDL_floorf(logical_w * scale);
            renderer->logical_dst_rect.x = (output_w - renderer->logical_dst_rect.w) / 2.0f;
        } else { /* renderer->logical_presentation_mode == SDL_LOGICAL_PRESENTATION_OVERSCAN */
            /* We want a narrower aspect ratio than is available -
               zoom so logical width matches the real width
               and the height will grow off the screen
             */
            scale = output_w / logical_w;
            renderer->logical_dst_rect.x = 0.0f;
            renderer->logical_dst_rect.w = output_w;
            renderer->logical_dst_rect.h = SDL_floorf(logical_h * scale);
            renderer->logical_dst_rect.y = (output_h - renderer->logical_dst_rect.h) / 2.0f;
        }
    }

    SDL_SetTextureScaleMode(renderer->logical_target, renderer->logical_scale_mode);

    if (!renderer->target) {
        SDL_SetRenderTarget(renderer, renderer->logical_target);
    }

    return 0;

error:
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED, SDL_SCALEMODE_NEAREST);
    return -1;
}

int SDL_SetRenderLogicalPresentation(SDL_Renderer *renderer, int w, int h, SDL_RendererLogicalPresentation mode, SDL_ScaleMode scale_mode)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (mode == SDL_LOGICAL_PRESENTATION_DISABLED) {
        if (renderer->logical_target) {
            SDL_DestroyTexture(renderer->logical_target);
        }
    } else {
        if (renderer->logical_target) {
            SDL_PropertiesID props = SDL_GetTextureProperties(renderer->logical_target);
            if (!props) {
                goto error;
            }

            int existing_w = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
            int existing_h = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);

            if (w != existing_w || h != existing_h) {
                SDL_DestroyTexture(renderer->logical_target);
            }
        }
        if (!renderer->logical_target) {
            renderer->logical_target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_TARGET, w, h);
            if (!renderer->logical_target) {
                goto error;
            }
            SDL_SetTextureBlendMode(renderer->logical_target, SDL_BLENDMODE_NONE);
        }
    }

    renderer->logical_presentation_mode = mode;
    renderer->logical_scale_mode = scale_mode;

    return UpdateLogicalPresentation(renderer);

error:
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED, SDL_SCALEMODE_NEAREST);
    return -1;
}

int SDL_GetRenderLogicalPresentation(SDL_Renderer *renderer, int *w, int *h, SDL_RendererLogicalPresentation *mode, SDL_ScaleMode *scale_mode)
{
    if (w) {
        *w = 0;
    }
    if (h) {
        *h = 0;
    }
    if (mode) {
        *mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    }
    if (scale_mode) {
        *scale_mode = SDL_SCALEMODE_NEAREST;
    }

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (renderer->logical_target) {
        SDL_PropertiesID props = SDL_GetTextureProperties(renderer->logical_target);
        if (!props) {
            return -1;
        }

        if (w) {
            *w = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0);
        }
        if (h) {
            *h = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0);
        }
    }

    if (mode) {
        *mode = renderer->logical_presentation_mode;
    }
    if (scale_mode) {
        *scale_mode = renderer->logical_scale_mode;
    }

    return 0;
}

static void SDL_RenderLogicalBorders(SDL_Renderer *renderer)
{
    const SDL_FRect *dst = &renderer->logical_dst_rect;

    if (dst->x > 0.0f || dst->y > 0.0f) {
        SDL_BlendMode saved_blend_mode = renderer->blendMode;
        SDL_FColor saved_color = renderer->color;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 1.0f);

        if (dst->x > 0.0f) {
            SDL_FRect rect;

            rect.x = 0.0f;
            rect.y = 0.0f;
            rect.w = dst->x;
            rect.h = (float)renderer->view->pixel_h;
            SDL_RenderFillRect(renderer, &rect);

            rect.x = dst->x + dst->w;
            rect.w = (float)renderer->view->pixel_w - rect.x;
            SDL_RenderFillRect(renderer, &rect);
        }

        if (dst->y > 0.0f) {
            SDL_FRect rect;

            rect.x = 0.0f;
            rect.y = 0.0f;
            rect.w = (float)renderer->view->pixel_w;
            rect.h = dst->y;
            SDL_RenderFillRect(renderer, &rect);

            rect.y = dst->y + dst->h;
            rect.h = (float)renderer->view->pixel_h - rect.y;
            SDL_RenderFillRect(renderer, &rect);
        }

        SDL_SetRenderDrawBlendMode(renderer, saved_blend_mode);
        SDL_SetRenderDrawColorFloat(renderer, saved_color.r, saved_color.g, saved_color.b, saved_color.a);
    }
}

static void SDL_RenderLogicalPresentation(SDL_Renderer *renderer)
{
    SDL_assert(renderer->target == NULL);
    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_RenderLogicalBorders(renderer);
    SDL_RenderTexture(renderer, renderer->logical_target, &renderer->logical_src_rect, &renderer->logical_dst_rect);
}

int SDL_RenderCoordinatesFromWindow(SDL_Renderer *renderer, float window_x, float window_y, float *x, float *y)
{
    SDL_RenderViewState *view;
    float render_x, render_y;

    CHECK_RENDERER_MAGIC(renderer, -1);

    /* Convert from window coordinates to pixels within the window */
    render_x = window_x * renderer->dpi_scale.x;
    render_y = window_y * renderer->dpi_scale.y;

    /* Convert from pixels within the window to pixels within the view */
    if (renderer->logical_target) {
        const SDL_FRect *src = &renderer->logical_src_rect;
        const SDL_FRect *dst = &renderer->logical_dst_rect;
        render_x = ((render_x - dst->x) * src->w) / dst->w;
        render_y = ((render_y - dst->y) * src->h) / dst->h;
    }

    /* Convert from pixels within the view to render coordinates */
    if (renderer->logical_target) {
        view = &renderer->logical_target->view;
    } else {
        view = &renderer->main_view;
    }
    render_x = (render_x / view->scale.x) - view->viewport.x;
    render_y = (render_y / view->scale.y) - view->viewport.y;

    if (x) {
        *x = render_x;
    }
    if (y) {
        *y = render_y;
    }
    return 0;
}

int SDL_RenderCoordinatesToWindow(SDL_Renderer *renderer, float x, float y, float *window_x, float *window_y)
{
    SDL_RenderViewState *view;

    CHECK_RENDERER_MAGIC(renderer, -1);

    /* Convert from render coordinates to pixels within the view */
    if (renderer->logical_target) {
        view = &renderer->logical_target->view;
    } else {
        view = &renderer->main_view;
    }
    x = (view->viewport.x + x) * view->scale.x;
    y = (view->viewport.y + y) * view->scale.y;

    /* Convert from pixels within the view to pixels within the window */
    if (renderer->logical_target) {
        const SDL_FRect *src = &renderer->logical_src_rect;
        const SDL_FRect *dst = &renderer->logical_dst_rect;
        x = dst->x + ((x * dst->w) / src->w);
        y = dst->y + ((y * dst->h) / src->h);
    }

    /* Convert from pixels within the window to window coordinates */
    x /= renderer->dpi_scale.x;
    y /= renderer->dpi_scale.y;

    if (window_x) {
        *window_x = x;
    }
    if (window_y) {
        *window_y = y;
    }
    return 0;
}

int SDL_ConvertEventToRenderCoordinates(SDL_Renderer *renderer, SDL_Event *event)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        SDL_Window *window = SDL_GetWindowFromID(event->motion.windowID);
        if (window == renderer->window) {
            SDL_RenderCoordinatesFromWindow(renderer, event->motion.x, event->motion.y, &event->motion.x, &event->motion.y);

            if (event->motion.xrel != 0.0f) {
                SDL_RenderViewState *view;

                /* Convert from window coordinates to pixels within the window */
                float scale = renderer->dpi_scale.x;

                /* Convert from pixels within the window to pixels within the view */
                if (renderer->logical_target) {
                    const SDL_FRect *src = &renderer->logical_src_rect;
                    const SDL_FRect *dst = &renderer->logical_dst_rect;
                    scale = (scale * src->w) / dst->w;
                }

                /* Convert from pixels within the view to render coordinates */
                if (renderer->logical_target) {
                    view = &renderer->logical_target->view;
                } else {
                    view = &renderer->main_view;
                }
                scale = (scale / view->scale.x);

                event->motion.xrel *= scale;
            }
            if (event->motion.yrel != 0.0f) {
                SDL_RenderViewState *view;

                /* Convert from window coordinates to pixels within the window */
                float scale = renderer->dpi_scale.y;

                /* Convert from pixels within the window to pixels within the view */
                if (renderer->logical_target) {
                    const SDL_FRect *src = &renderer->logical_src_rect;
                    const SDL_FRect *dst = &renderer->logical_dst_rect;
                    scale = (scale * src->h) / dst->h;
                }

                /* Convert from pixels within the view to render coordinates */
                if (renderer->logical_target) {
                    view = &renderer->logical_target->view;
                } else {
                    view = &renderer->main_view;
                }
                scale = (scale / view->scale.y);

                event->motion.yrel *= scale;
            }
        }
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
               event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        SDL_Window *window = SDL_GetWindowFromID(event->button.windowID);
        if (window == renderer->window) {
            SDL_RenderCoordinatesFromWindow(renderer, event->button.x, event->button.y, &event->button.x, &event->button.y);
        }
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        SDL_Window *window = SDL_GetWindowFromID(event->wheel.windowID);
        if (window == renderer->window) {
            SDL_RenderCoordinatesFromWindow(renderer, event->wheel.mouse_x,
                                            event->wheel.mouse_y,
                                            &event->wheel.mouse_x,
                                            &event->wheel.mouse_y);
        }
    } else if (event->type == SDL_EVENT_FINGER_DOWN ||
               event->type == SDL_EVENT_FINGER_UP ||
               event->type == SDL_EVENT_FINGER_MOTION) {
        /* FIXME: Are these events guaranteed to be window relative? */
        if (renderer->window) {
            int w, h;
            if (SDL_GetWindowSize(renderer->window, &w, &h) < 0) {
                return -1;
            }
            SDL_RenderCoordinatesFromWindow(renderer, event->tfinger.x * w, event->tfinger.y * h, &event->tfinger.x, &event->tfinger.y);
        }
    } else if (event->type == SDL_EVENT_DROP_POSITION ||
               event->type == SDL_EVENT_DROP_FILE ||
               event->type == SDL_EVENT_DROP_TEXT ||
               event->type == SDL_EVENT_DROP_COMPLETE) {
        SDL_Window *window = SDL_GetWindowFromID(event->drop.windowID);
        if (window == renderer->window) {
            SDL_RenderCoordinatesFromWindow(renderer, event->drop.x, event->drop.y, &event->drop.x, &event->drop.y);
        }
    }
    return 0;
}

int SDL_SetRenderViewport(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (rect) {
        SDL_copyp(&renderer->view->viewport, rect);
    } else {
        renderer->view->viewport.x = 0;
        renderer->view->viewport.y = 0;
        renderer->view->viewport.w = -1;
        renderer->view->viewport.h = -1;
    }
    UpdatePixelViewport(renderer, renderer->view);

    return QueueCmdSetViewport(renderer);
}

int SDL_GetRenderViewport(SDL_Renderer *renderer, SDL_Rect *rect)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (rect) {
        rect->x = renderer->view->viewport.x;
        rect->y = renderer->view->viewport.y;
        if (renderer->view->viewport.w >= 0) {
            rect->w = renderer->view->viewport.w;
        } else {
            rect->w = (int)SDL_ceilf(renderer->view->pixel_w / renderer->view->scale.x);
        }
        if (renderer->view->viewport.h >= 0) {
            rect->h = renderer->view->viewport.h;
        } else {
            rect->h = (int)SDL_ceilf(renderer->view->pixel_h / renderer->view->scale.y);
        }
    }
    return 0;
}

SDL_bool SDL_RenderViewportSet(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (renderer->view->viewport.w >= 0 &&
        renderer->view->viewport.h >= 0) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static void GetRenderViewportSize(SDL_Renderer *renderer, SDL_FRect *rect)
{
    rect->x = 0.0f;
    rect->y = 0.0f;
    if (renderer->view->viewport.w >= 0) {
        rect->w = (float)renderer->view->viewport.w;
    } else {
        rect->w = renderer->view->pixel_w / renderer->view->scale.x;
    }
    if (renderer->view->viewport.h >= 0) {
        rect->h = (float)renderer->view->viewport.h;
    } else {
        rect->h = renderer->view->pixel_h / renderer->view->scale.y;
    }
}

int SDL_SetRenderClipRect(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    CHECK_RENDERER_MAGIC(renderer, -1)

    if (rect && rect->w >= 0 && rect->h >= 0) {
        renderer->view->clipping_enabled = SDL_TRUE;
        SDL_copyp(&renderer->view->clip_rect, rect);
    } else {
        renderer->view->clipping_enabled = SDL_FALSE;
        SDL_zero(renderer->view->clip_rect);
    }
    UpdatePixelClipRect(renderer, renderer->view);

    return QueueCmdSetClipRect(renderer);
}

int SDL_GetRenderClipRect(SDL_Renderer *renderer, SDL_Rect *rect)
{
    CHECK_RENDERER_MAGIC(renderer, -1)

    if (rect) {
        SDL_copyp(rect, &renderer->view->clip_rect);
    }
    return 0;
}

SDL_bool SDL_RenderClipEnabled(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, SDL_FALSE)
    return renderer->view->clipping_enabled;
}

int SDL_SetRenderScale(SDL_Renderer *renderer, float scaleX, float scaleY)
{
    int retval = 0;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (renderer->view->scale.x == scaleX &&
        renderer->view->scale.y == scaleY) {
        return 0;
    }

    renderer->view->scale.x = scaleX;
    renderer->view->scale.y = scaleY;
    UpdatePixelViewport(renderer, renderer->view);
    UpdatePixelClipRect(renderer, renderer->view);

    /* The scale affects the existing viewport and clip rectangle */
    retval += QueueCmdSetViewport(renderer);
    retval += QueueCmdSetClipRect(renderer);
    return retval;
}

int SDL_GetRenderScale(SDL_Renderer *renderer, float *scaleX, float *scaleY)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (scaleX) {
        *scaleX = renderer->view->scale.x;
    }
    if (scaleY) {
        *scaleY = renderer->view->scale.y;
    }
    return 0;
}

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    const float fR = (float)r / 255.0f;
    const float fG = (float)g / 255.0f;
    const float fB = (float)b / 255.0f;
    const float fA = (float)a / 255.0f;

    return SDL_SetRenderDrawColorFloat(renderer, fR, fG, fB, fA);
}

int SDL_SetRenderDrawColorFloat(SDL_Renderer *renderer, float r, float g, float b, float a)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    renderer->color.r = r;
    renderer->color.g = g;
    renderer->color.b = b;
    renderer->color.a = a;
    return 0;
}

int SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    float fR, fG, fB, fA;

    if (SDL_GetRenderDrawColorFloat(renderer, &fR, &fG, &fB, &fA) < 0) {
        return -1;
    }

    if (r) {
        *r = (Uint8)(fR * 255.0f);
    }
    if (g) {
        *g = (Uint8)(fG * 255.0f);
    }
    if (b) {
        *b = (Uint8)(fB * 255.0f);
    }
    if (a) {
        *a = (Uint8)(fA * 255.0f);
    }
    return 0;
}

int SDL_GetRenderDrawColorFloat(SDL_Renderer *renderer, float *r, float *g, float *b, float *a)
{
    SDL_FColor color;

    CHECK_RENDERER_MAGIC(renderer, -1);

    color = renderer->color;

    if (r) {
        *r = color.r;
    }
    if (g) {
        *g = color.g;
    }
    if (b) {
        *b = color.b;
    }
    if (a) {
        *a = color.a;
    }
    return 0;
}

int SDL_SetRenderColorScale(SDL_Renderer *renderer, float scale)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    renderer->color_scale = scale * renderer->SDR_white_point;
    return 0;
}

int SDL_GetRenderColorScale(SDL_Renderer *renderer, float *scale)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (scale) {
        *scale = renderer->color_scale / renderer->SDR_white_point;
    }
    return 0;
}

int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!IsSupportedBlendMode(renderer, blendMode)) {
        return SDL_Unsupported();
    }
    renderer->blendMode = blendMode;
    return 0;
}

int SDL_GetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode *blendMode)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    *blendMode = renderer->blendMode;
    return 0;
}

int SDL_RenderClear(SDL_Renderer *renderer)
{
    int retval;
    CHECK_RENDERER_MAGIC(renderer, -1);
    retval = QueueCmdClear(renderer);
    return retval;
}

int SDL_RenderPoint(SDL_Renderer *renderer, float x, float y)
{
    SDL_FPoint fpoint;
    fpoint.x = x;
    fpoint.y = y;
    return SDL_RenderPoints(renderer, &fpoint, 1);
}

static int RenderPointsWithRects(SDL_Renderer *renderer, const SDL_FPoint *fpoints, const int count)
{
    int retval;
    SDL_bool isstack;
    SDL_FRect *frects;
    int i;

    if (count < 1) {
        return 0;
    }

    frects = SDL_small_alloc(SDL_FRect, count, &isstack);
    if (!frects) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        frects[i].x = fpoints[i].x * renderer->view->scale.x;
        frects[i].y = fpoints[i].y * renderer->view->scale.y;
        frects[i].w = renderer->view->scale.x;
        frects[i].h = renderer->view->scale.y;
    }

    retval = QueueCmdFillRects(renderer, frects, count);

    SDL_small_free(frects, isstack);

    return retval;
}

int SDL_RenderPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count)
{
    int retval;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!points) {
        return SDL_InvalidParamError("SDL_RenderPoints(): points");
    }
    if (count < 1) {
        return 0;
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    if (renderer->view->scale.x != 1.0f || renderer->view->scale.y != 1.0f) {
        retval = RenderPointsWithRects(renderer, points, count);
    } else {
        retval = QueueCmdDrawPoints(renderer, points, count);
    }
    return retval;
}

int SDL_RenderLine(SDL_Renderer *renderer, float x1, float y1, float x2, float y2)
{
    SDL_FPoint points[2];
    points[0].x = x1;
    points[0].y = y1;
    points[1].x = x2;
    points[1].y = y2;
    return SDL_RenderLines(renderer, points, 2);
}

static int RenderLineBresenham(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, SDL_bool draw_last)
{
    const int MAX_PIXELS = SDL_max(renderer->view->pixel_w, renderer->view->pixel_h) * 4;
    int i, deltax, deltay, numpixels;
    int d, dinc1, dinc2;
    int x, xinc1, xinc2;
    int y, yinc1, yinc2;
    int retval;
    SDL_bool isstack;
    SDL_FPoint *points;
    SDL_Rect viewport;

    /* the backend might clip this further to the clipping rect, but we
       just want a basic safety against generating millions of points for
       massive lines. */
    viewport = renderer->view->pixel_viewport;
    viewport.x = 0;
    viewport.y = 0;
    if (!SDL_GetRectAndLineIntersection(&viewport, &x1, &y1, &x2, &y2)) {
        return 0;
    }

    deltax = SDL_abs(x2 - x1);
    deltay = SDL_abs(y2 - y1);

    if (deltax >= deltay) {
        numpixels = deltax + 1;
        d = (2 * deltay) - deltax;
        dinc1 = deltay * 2;
        dinc2 = (deltay - deltax) * 2;
        xinc1 = 1;
        xinc2 = 1;
        yinc1 = 0;
        yinc2 = 1;
    } else {
        numpixels = deltay + 1;
        d = (2 * deltax) - deltay;
        dinc1 = deltax * 2;
        dinc2 = (deltax - deltay) * 2;
        xinc1 = 0;
        xinc2 = 1;
        yinc1 = 1;
        yinc2 = 1;
    }

    if (x1 > x2) {
        xinc1 = -xinc1;
        xinc2 = -xinc2;
    }
    if (y1 > y2) {
        yinc1 = -yinc1;
        yinc2 = -yinc2;
    }

    x = x1;
    y = y1;

    if (!draw_last) {
        --numpixels;
    }

    if (numpixels > MAX_PIXELS) {
        return SDL_SetError("Line too long (tried to draw %d pixels, max %d)", numpixels, MAX_PIXELS);
    }

    points = SDL_small_alloc(SDL_FPoint, numpixels, &isstack);
    if (!points) {
        return -1;
    }
    for (i = 0; i < numpixels; ++i) {
        points[i].x = (float)x;
        points[i].y = (float)y;

        if (d < 0) {
            d += dinc1;
            x += xinc1;
            y += yinc1;
        } else {
            d += dinc2;
            x += xinc2;
            y += yinc2;
        }
    }

    if (renderer->view->scale.x != 1.0f || renderer->view->scale.y != 1.0f) {
        retval = RenderPointsWithRects(renderer, points, numpixels);
    } else {
        retval = QueueCmdDrawPoints(renderer, points, numpixels);
    }

    SDL_small_free(points, isstack);

    return retval;
}

static int RenderLinesWithRectsF(SDL_Renderer *renderer,
                                     const SDL_FPoint *points, const int count)
{
    const float scale_x = renderer->view->scale.x;
    const float scale_y = renderer->view->scale.y;
    SDL_FRect *frect;
    SDL_FRect *frects;
    int i, nrects = 0;
    int retval = 0;
    SDL_bool isstack;
    SDL_bool drew_line = SDL_FALSE;
    SDL_bool draw_last = SDL_FALSE;

    frects = SDL_small_alloc(SDL_FRect, count - 1, &isstack);
    if (!frects) {
        return -1;
    }

    for (i = 0; i < count - 1; ++i) {
        SDL_bool same_x = (points[i].x == points[i + 1].x);
        SDL_bool same_y = (points[i].y == points[i + 1].y);

        if (i == (count - 2)) {
            if (!drew_line || points[i + 1].x != points[0].x || points[i + 1].y != points[0].y) {
                draw_last = SDL_TRUE;
            }
        } else {
            if (same_x && same_y) {
                continue;
            }
        }
        if (same_x) {
            const float minY = SDL_min(points[i].y, points[i + 1].y);
            const float maxY = SDL_max(points[i].y, points[i + 1].y);

            frect = &frects[nrects++];
            frect->x = points[i].x * scale_x;
            frect->y = minY * scale_y;
            frect->w = scale_x;
            frect->h = (maxY - minY + draw_last) * scale_y;
            if (!draw_last && points[i + 1].y < points[i].y) {
                frect->y += scale_y;
            }
        } else if (same_y) {
            const float minX = SDL_min(points[i].x, points[i + 1].x);
            const float maxX = SDL_max(points[i].x, points[i + 1].x);

            frect = &frects[nrects++];
            frect->x = minX * scale_x;
            frect->y = points[i].y * scale_y;
            frect->w = (maxX - minX + draw_last) * scale_x;
            frect->h = scale_y;
            if (!draw_last && points[i + 1].x < points[i].x) {
                frect->x += scale_x;
            }
        } else {
            retval += RenderLineBresenham(renderer, (int)SDL_roundf(points[i].x), (int)SDL_roundf(points[i].y),
                                              (int)SDL_roundf(points[i + 1].x), (int)SDL_roundf(points[i + 1].y), draw_last);
        }
        drew_line = SDL_TRUE;
    }

    if (nrects) {
        retval += QueueCmdFillRects(renderer, frects, nrects);
    }

    SDL_small_free(frects, isstack);

    if (retval < 0) {
        retval = -1;
    }
    return retval;
}

int SDL_RenderLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count)
{
    int retval = 0;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!points) {
        return SDL_InvalidParamError("SDL_RenderLines(): points");
    }
    if (count < 2) {
        return 0;
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    if (renderer->line_method == SDL_RENDERLINEMETHOD_POINTS) {
        retval = RenderLinesWithRectsF(renderer, points, count);
    } else if (renderer->line_method == SDL_RENDERLINEMETHOD_GEOMETRY) {
        SDL_bool isstack1;
        SDL_bool isstack2;
        const float scale_x = renderer->view->scale.x;
        const float scale_y = renderer->view->scale.y;
        float *xy = SDL_small_alloc(float, 4 * 2 * count, &isstack1);
        int *indices = SDL_small_alloc(int,
                                       (4) * 3 * (count - 1) + (2) * 3 * (count), &isstack2);

        if (xy && indices) {
            int i;
            float *ptr_xy = xy;
            int *ptr_indices = indices;
            const int xy_stride = 2 * sizeof(float);
            int num_vertices = 4 * count;
            int num_indices = 0;
            const int size_indices = 4;
            int cur_index = -4;
            const int is_looping = (points[0].x == points[count - 1].x && points[0].y == points[count - 1].y);
            SDL_FPoint p; /* previous point */
            p.x = p.y = 0.0f;
            /*       p            q

                    0----1------ 4----5
                    | \  |``\    | \  |
                    |  \ |   ` `\|  \ |
                    3----2-------7----6
            */
            for (i = 0; i < count; ++i) {
                SDL_FPoint q = points[i]; /* current point */

                q.x *= scale_x;
                q.y *= scale_y;

                *ptr_xy++ = q.x;
                *ptr_xy++ = q.y;
                *ptr_xy++ = q.x + scale_x;
                *ptr_xy++ = q.y;
                *ptr_xy++ = q.x + scale_x;
                *ptr_xy++ = q.y + scale_y;
                *ptr_xy++ = q.x;
                *ptr_xy++ = q.y + scale_y;

#define ADD_TRIANGLE(i1, i2, i3)        \
    *ptr_indices++ = cur_index + (i1);  \
    *ptr_indices++ = cur_index + (i2);  \
    *ptr_indices++ = cur_index + (i3);  \
    num_indices += 3;

                /* closed polyline, don´t draw twice the point */
                if (i || is_looping == 0) {
                    ADD_TRIANGLE(4, 5, 6)
                    ADD_TRIANGLE(4, 6, 7)
                }

                /* first point only, no segment */
                if (i == 0) {
                    p = q;
                    cur_index += 4;
                    continue;
                }

                /* draw segment */
                if (p.y == q.y) {
                    if (p.x < q.x) {
                        ADD_TRIANGLE(1, 4, 7)
                        ADD_TRIANGLE(1, 7, 2)
                    } else {
                        ADD_TRIANGLE(5, 0, 3)
                        ADD_TRIANGLE(5, 3, 6)
                    }
                } else if (p.x == q.x) {
                    if (p.y < q.y) {
                        ADD_TRIANGLE(2, 5, 4)
                        ADD_TRIANGLE(2, 4, 3)
                    } else {
                        ADD_TRIANGLE(6, 1, 0)
                        ADD_TRIANGLE(6, 0, 7)
                    }
                } else {
                    if (p.y < q.y) {
                        if (p.x < q.x) {
                            ADD_TRIANGLE(1, 5, 4)
                            ADD_TRIANGLE(1, 4, 2)
                            ADD_TRIANGLE(2, 4, 7)
                            ADD_TRIANGLE(2, 7, 3)
                        } else {
                            ADD_TRIANGLE(4, 0, 5)
                            ADD_TRIANGLE(5, 0, 3)
                            ADD_TRIANGLE(5, 3, 6)
                            ADD_TRIANGLE(6, 3, 2)
                        }
                    } else {
                        if (p.x < q.x) {
                            ADD_TRIANGLE(0, 4, 7)
                            ADD_TRIANGLE(0, 7, 1)
                            ADD_TRIANGLE(1, 7, 6)
                            ADD_TRIANGLE(1, 6, 2)
                        } else {
                            ADD_TRIANGLE(6, 5, 1)
                            ADD_TRIANGLE(6, 1, 0)
                            ADD_TRIANGLE(7, 6, 0)
                            ADD_TRIANGLE(7, 0, 3)
                        }
                    }
                }

                p = q;
                cur_index += 4;
            }

            retval = QueueCmdGeometry(renderer, NULL,
                                      xy, xy_stride, &renderer->color, 0 /* color_stride */, NULL, 0,
                                      num_vertices, indices, num_indices, size_indices,
                                      1.0f, 1.0f);
        }

        SDL_small_free(xy, isstack1);
        SDL_small_free(indices, isstack2);

    } else if (renderer->view->scale.x != 1.0f || renderer->view->scale.y != 1.0f) {
        retval = RenderLinesWithRectsF(renderer, points, count);
    } else {
        retval = QueueCmdDrawLines(renderer, points, count);
    }

    return retval;
}

int SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *rect)
{
    SDL_FRect frect;
    SDL_FPoint points[5];

    CHECK_RENDERER_MAGIC(renderer, -1);

    /* If 'rect' == NULL, then outline the whole surface */
    if (!rect) {
        GetRenderViewportSize(renderer, &frect);
        rect = &frect;
    }

    points[0].x = rect->x;
    points[0].y = rect->y;
    points[1].x = rect->x + rect->w - 1;
    points[1].y = rect->y;
    points[2].x = rect->x + rect->w - 1;
    points[2].y = rect->y + rect->h - 1;
    points[3].x = rect->x;
    points[3].y = rect->y + rect->h - 1;
    points[4].x = rect->x;
    points[4].y = rect->y;
    return SDL_RenderLines(renderer, points, 5);
}

int SDL_RenderRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count)
{
    int i;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!rects) {
        return SDL_InvalidParamError("SDL_RenderRects(): rects");
    }
    if (count < 1) {
        return 0;
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    for (i = 0; i < count; ++i) {
        if (SDL_RenderRect(renderer, &rects[i]) < 0) {
            return -1;
        }
    }
    return 0;
}

int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect)
{
    SDL_FRect frect;

    CHECK_RENDERER_MAGIC(renderer, -1);

    /* If 'rect' == NULL, then fill the whole surface */
    if (!rect) {
        GetRenderViewportSize(renderer, &frect);
        rect = &frect;
    }
    return SDL_RenderFillRects(renderer, rect, 1);
}

int SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count)
{
    SDL_FRect *frects;
    int i;
    int retval;
    SDL_bool isstack;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!rects) {
        return SDL_InvalidParamError("SDL_RenderFillRects(): rects");
    }
    if (count < 1) {
        return 0;
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    frects = SDL_small_alloc(SDL_FRect, count, &isstack);
    if (!frects) {
        return -1;
    }
    for (i = 0; i < count; ++i) {
        frects[i].x = rects[i].x * renderer->view->scale.x;
        frects[i].y = rects[i].y * renderer->view->scale.y;
        frects[i].w = rects[i].w * renderer->view->scale.x;
        frects[i].h = rects[i].h * renderer->view->scale.y;
    }

    retval = QueueCmdFillRects(renderer, frects, count);

    SDL_small_free(frects, isstack);

    return retval;
}

int SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect)
{
    SDL_FRect real_srcrect;
    SDL_FRect real_dstrect;
    int retval;
    int use_rendergeometry;

    CHECK_RENDERER_MAGIC(renderer, -1);
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (renderer != texture->renderer) {
        return SDL_SetError("Texture was not created with this renderer");
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    use_rendergeometry = (!renderer->QueueCopy);

    real_srcrect.x = 0.0f;
    real_srcrect.y = 0.0f;
    real_srcrect.w = (float)texture->w;
    real_srcrect.h = (float)texture->h;
    if (srcrect) {
        if (!SDL_GetRectIntersectionFloat(srcrect, &real_srcrect, &real_srcrect)) {
            return 0;
        }
    }

    GetRenderViewportSize(renderer, &real_dstrect);
    if (dstrect) {
        if (!SDL_HasRectIntersectionFloat(dstrect, &real_dstrect)) {
            return 0;
        }
        real_dstrect = *dstrect;
    }

    if (texture->native) {
        texture = texture->native;
    }

    texture->last_command_generation = renderer->render_command_generation;

    if (use_rendergeometry) {
        float xy[8];
        const int xy_stride = 2 * sizeof(float);
        float uv[8];
        const int uv_stride = 2 * sizeof(float);
        const int num_vertices = 4;
        const int *indices = renderer->rect_index_order;
        const int num_indices = 6;
        const int size_indices = 4;
        float minu, minv, maxu, maxv;
        float minx, miny, maxx, maxy;

        minu = real_srcrect.x / texture->w;
        minv = real_srcrect.y / texture->h;
        maxu = (real_srcrect.x + real_srcrect.w) / texture->w;
        maxv = (real_srcrect.y + real_srcrect.h) / texture->h;

        minx = real_dstrect.x;
        miny = real_dstrect.y;
        maxx = real_dstrect.x + real_dstrect.w;
        maxy = real_dstrect.y + real_dstrect.h;

        uv[0] = minu;
        uv[1] = minv;
        uv[2] = maxu;
        uv[3] = minv;
        uv[4] = maxu;
        uv[5] = maxv;
        uv[6] = minu;
        uv[7] = maxv;

        xy[0] = minx;
        xy[1] = miny;
        xy[2] = maxx;
        xy[3] = miny;
        xy[4] = maxx;
        xy[5] = maxy;
        xy[6] = minx;
        xy[7] = maxy;

        retval = QueueCmdGeometry(renderer, texture,
                                  xy, xy_stride, &texture->color, 0 /* color_stride */, uv, uv_stride,
                                  num_vertices,
                                  indices, num_indices, size_indices,
                                  renderer->view->scale.x,
                                  renderer->view->scale.y);
    } else {

        real_dstrect.x *= renderer->view->scale.x;
        real_dstrect.y *= renderer->view->scale.y;
        real_dstrect.w *= renderer->view->scale.x;
        real_dstrect.h *= renderer->view->scale.y;

        retval = QueueCmdCopy(renderer, texture, &real_srcrect, &real_dstrect);
    }
    return retval;
}

int SDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture,
                      const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                      const double angle, const SDL_FPoint *center, const SDL_FlipMode flip)
{
    SDL_FRect real_srcrect;
    SDL_FRect real_dstrect;
    SDL_FPoint real_center;
    int retval;
    int use_rendergeometry;

    if (flip == SDL_FLIP_NONE && (int)(angle / 360) == angle / 360) { /* fast path when we don't need rotation or flipping */
        return SDL_RenderTexture(renderer, texture, srcrect, dstrect);
    }

    CHECK_RENDERER_MAGIC(renderer, -1);
    CHECK_TEXTURE_MAGIC(texture, -1);

    if (renderer != texture->renderer) {
        return SDL_SetError("Texture was not created with this renderer");
    }
    if (!renderer->QueueCopyEx && !renderer->QueueGeometry) {
        return SDL_SetError("Renderer does not support RenderCopyEx");
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    use_rendergeometry = (!renderer->QueueCopyEx);

    real_srcrect.x = 0.0f;
    real_srcrect.y = 0.0f;
    real_srcrect.w = (float)texture->w;
    real_srcrect.h = (float)texture->h;
    if (srcrect) {
        if (!SDL_GetRectIntersectionFloat(srcrect, &real_srcrect, &real_srcrect)) {
            return 0;
        }
    }

    /* We don't intersect the dstrect with the viewport as RenderCopy does because of potential rotation clipping issues... TODO: should we? */
    if (dstrect) {
        real_dstrect = *dstrect;
    } else {
        GetRenderViewportSize(renderer, &real_dstrect);
    }

    if (texture->native) {
        texture = texture->native;
    }

    if (center) {
        real_center = *center;
    } else {
        real_center.x = real_dstrect.w / 2.0f;
        real_center.y = real_dstrect.h / 2.0f;
    }

    texture->last_command_generation = renderer->render_command_generation;

    if (use_rendergeometry) {
        float xy[8];
        const int xy_stride = 2 * sizeof(float);
        float uv[8];
        const int uv_stride = 2 * sizeof(float);
        const int num_vertices = 4;
        const int *indices = renderer->rect_index_order;
        const int num_indices = 6;
        const int size_indices = 4;
        float minu, minv, maxu, maxv;
        float minx, miny, maxx, maxy;
        float centerx, centery;

        float s_minx, s_miny, s_maxx, s_maxy;
        float c_minx, c_miny, c_maxx, c_maxy;

        const float radian_angle = (float)((SDL_PI_D * angle) / 180.0);
        const float s = SDL_sinf(radian_angle);
        const float c = SDL_cosf(radian_angle);

        minu = real_srcrect.x / texture->w;
        minv = real_srcrect.y / texture->h;
        maxu = (real_srcrect.x + real_srcrect.w) / texture->w;
        maxv = (real_srcrect.y + real_srcrect.h) / texture->h;

        centerx = real_center.x + real_dstrect.x;
        centery = real_center.y + real_dstrect.y;

        if (flip & SDL_FLIP_HORIZONTAL) {
            minx = real_dstrect.x + real_dstrect.w;
            maxx = real_dstrect.x;
        } else {
            minx = real_dstrect.x;
            maxx = real_dstrect.x + real_dstrect.w;
        }

        if (flip & SDL_FLIP_VERTICAL) {
            miny = real_dstrect.y + real_dstrect.h;
            maxy = real_dstrect.y;
        } else {
            miny = real_dstrect.y;
            maxy = real_dstrect.y + real_dstrect.h;
        }

        uv[0] = minu;
        uv[1] = minv;
        uv[2] = maxu;
        uv[3] = minv;
        uv[4] = maxu;
        uv[5] = maxv;
        uv[6] = minu;
        uv[7] = maxv;

        /* apply rotation with 2x2 matrix ( c -s )
         *                                ( s  c ) */
        s_minx = s * (minx - centerx);
        s_miny = s * (miny - centery);
        s_maxx = s * (maxx - centerx);
        s_maxy = s * (maxy - centery);
        c_minx = c * (minx - centerx);
        c_miny = c * (miny - centery);
        c_maxx = c * (maxx - centerx);
        c_maxy = c * (maxy - centery);

        /* (minx, miny) */
        xy[0] = (c_minx - s_miny) + centerx;
        xy[1] = (s_minx + c_miny) + centery;
        /* (maxx, miny) */
        xy[2] = (c_maxx - s_miny) + centerx;
        xy[3] = (s_maxx + c_miny) + centery;
        /* (maxx, maxy) */
        xy[4] = (c_maxx - s_maxy) + centerx;
        xy[5] = (s_maxx + c_maxy) + centery;
        /* (minx, maxy) */
        xy[6] = (c_minx - s_maxy) + centerx;
        xy[7] = (s_minx + c_maxy) + centery;

        retval = QueueCmdGeometry(renderer, texture,
                                  xy, xy_stride, &texture->color, 0 /* color_stride */, uv, uv_stride,
                                  num_vertices,
                                  indices, num_indices, size_indices,
                                  renderer->view->scale.x,
                                  renderer->view->scale.y);
    } else {

        retval = QueueCmdCopyEx(renderer, texture, &real_srcrect, &real_dstrect, angle, &real_center, flip,
                                renderer->view->scale.x,
                                renderer->view->scale.y);
    }
    return retval;
}

int SDL_RenderGeometry(SDL_Renderer *renderer,
                       SDL_Texture *texture,
                       const SDL_Vertex *vertices, int num_vertices,
                       const int *indices, int num_indices)
{
    if (vertices) {
        const float *xy = &vertices->position.x;
        int xy_stride = sizeof(SDL_Vertex);
        const SDL_FColor *color = &vertices->color;
        int color_stride = sizeof(SDL_Vertex);
        const float *uv = &vertices->tex_coord.x;
        int uv_stride = sizeof(SDL_Vertex);
        int size_indices = 4;
        return SDL_RenderGeometryRawFloat(renderer, texture, xy, xy_stride, color, color_stride, uv, uv_stride, num_vertices, indices, num_indices, size_indices);
    } else {
        return SDL_InvalidParamError("vertices");
    }
}

#if SDL_VIDEO_RENDER_SW
static int remap_one_indice(
    int prev,
    int k,
    SDL_Texture *texture,
    const float *xy, int xy_stride,
    const SDL_FColor *color, int color_stride,
    const float *uv, int uv_stride)
{
    const float *xy0_, *xy1_, *uv0_, *uv1_;
    const SDL_FColor *col0_, *col1_;
    xy0_ = (const float *)((const char *)xy + prev * xy_stride);
    xy1_ = (const float *)((const char *)xy + k * xy_stride);
    if (xy0_[0] != xy1_[0]) {
        return k;
    }
    if (xy0_[1] != xy1_[1]) {
        return k;
    }
    if (texture) {
        uv0_ = (const float *)((const char *)uv + prev * uv_stride);
        uv1_ = (const float *)((const char *)uv + k * uv_stride);
        if (uv0_[0] != uv1_[0]) {
            return k;
        }
        if (uv0_[1] != uv1_[1]) {
            return k;
        }
    }
    col0_ = (const SDL_FColor *)((const char *)color + prev * color_stride);
    col1_ = (const SDL_FColor *)((const char *)color + k * color_stride);

    if (SDL_memcmp(col0_, col1_, sizeof(*col0_)) != 0) {
        return k;
    }

    return prev;
}

static int remap_indices(
    int prev[3],
    int k,
    SDL_Texture *texture,
    const float *xy, int xy_stride,
    const SDL_FColor *color, int color_stride,
    const float *uv, int uv_stride)
{
    int i;
    if (prev[0] == -1) {
        return k;
    }

    for (i = 0; i < 3; i++) {
        int new_k = remap_one_indice(prev[i], k, texture, xy, xy_stride, color, color_stride, uv, uv_stride);
        if (new_k != k) {
            return new_k;
        }
    }
    return k;
}

#define DEBUG_SW_RENDER_GEOMETRY 0
/* For the software renderer, try to reinterpret triangles as SDL_Rect */
static int SDLCALL SDL_SW_RenderGeometryRaw(SDL_Renderer *renderer,
                                            SDL_Texture *texture,
                                            const float *xy, int xy_stride,
                                            const SDL_FColor *color, int color_stride,
                                            const float *uv, int uv_stride,
                                            int num_vertices,
                                            const void *indices, int num_indices, int size_indices)
{
    int i;
    int retval = 0;
    int count = indices ? num_indices : num_vertices;
    int prev[3]; /* Previous triangle vertex indices */
    float texw = 0.0f, texh = 0.0f;
    SDL_BlendMode blendMode = SDL_BLENDMODE_NONE;
    float r = 0, g = 0, b = 0, a = 0;

    /* Save */
    SDL_GetRenderDrawBlendMode(renderer, &blendMode);
    SDL_GetRenderDrawColorFloat(renderer, &r, &g, &b, &a);

    if (texture) {
        SDL_GetTextureSize(texture, &texw, &texh);
    }

    prev[0] = -1;
    prev[1] = -1;
    prev[2] = -1;
    size_indices = indices ? size_indices : 0;

    for (i = 0; i < count; i += 3) {
        int k0, k1, k2; /* Current triangle indices */
        int is_quad = 1;
#if DEBUG_SW_RENDER_GEOMETRY
        int is_uniform = 1;
        int is_rectangle = 1;
#endif
        int A = -1;  /* Top left vertex */
        int B = -1;  /* Bottom right vertex */
        int C = -1;  /* Third vertex of current triangle */
        int C2 = -1; /* Last, vertex of previous triangle */

        if (size_indices == 4) {
            k0 = ((const Uint32 *)indices)[i];
            k1 = ((const Uint32 *)indices)[i + 1];
            k2 = ((const Uint32 *)indices)[i + 2];
        } else if (size_indices == 2) {
            k0 = ((const Uint16 *)indices)[i];
            k1 = ((const Uint16 *)indices)[i + 1];
            k2 = ((const Uint16 *)indices)[i + 2];
        } else if (size_indices == 1) {
            k0 = ((const Uint8 *)indices)[i];
            k1 = ((const Uint8 *)indices)[i + 1];
            k2 = ((const Uint8 *)indices)[i + 2];
        } else {
            /* Vertices were not provided by indices. Maybe some are duplicated.
             * We try to indentificate the duplicates by comparing with the previous three vertices */
            k0 = remap_indices(prev, i, texture, xy, xy_stride, color, color_stride, uv, uv_stride);
            k1 = remap_indices(prev, i + 1, texture, xy, xy_stride, color, color_stride, uv, uv_stride);
            k2 = remap_indices(prev, i + 2, texture, xy, xy_stride, color, color_stride, uv, uv_stride);
        }

        if (prev[0] == -1) {
            prev[0] = k0;
            prev[1] = k1;
            prev[2] = k2;
            continue;
        }

        /* Two triangles forming a quadialateral,
         * prev and current triangles must have exactly 2 common vertices */
        {
            int cnt = 0, j = 3;
            while (j--) {
                int p = prev[j];
                if (p == k0 || p == k1 || p == k2) {
                    cnt++;
                }
            }
            is_quad = (cnt == 2);
        }

        /* Identify vertices */
        if (is_quad) {
            const float *xy0_, *xy1_, *xy2_;
            float x0, x1, x2;
            float y0, y1, y2;
            xy0_ = (const float *)((const char *)xy + k0 * xy_stride);
            xy1_ = (const float *)((const char *)xy + k1 * xy_stride);
            xy2_ = (const float *)((const char *)xy + k2 * xy_stride);
            x0 = xy0_[0];
            y0 = xy0_[1];
            x1 = xy1_[0];
            y1 = xy1_[1];
            x2 = xy2_[0];
            y2 = xy2_[1];

            /* Find top-left */
            if (x0 <= x1 && y0 <= y1) {
                if (x0 <= x2 && y0 <= y2) {
                    A = k0;
                } else {
                    A = k2;
                }
            } else {
                if (x1 <= x2 && y1 <= y2) {
                    A = k1;
                } else {
                    A = k2;
                }
            }

            /* Find bottom-right */
            if (x0 >= x1 && y0 >= y1) {
                if (x0 >= x2 && y0 >= y2) {
                    B = k0;
                } else {
                    B = k2;
                }
            } else {
                if (x1 >= x2 && y1 >= y2) {
                    B = k1;
                } else {
                    B = k2;
                }
            }

            /* Find C */
            if (k0 != A && k0 != B) {
                C = k0;
            } else if (k1 != A && k1 != B) {
                C = k1;
            } else {
                C = k2;
            }

            /* Find C2 */
            if (prev[0] != A && prev[0] != B) {
                C2 = prev[0];
            } else if (prev[1] != A && prev[1] != B) {
                C2 = prev[1];
            } else {
                C2 = prev[2];
            }

            xy0_ = (const float *)((const char *)xy + A * xy_stride);
            xy1_ = (const float *)((const char *)xy + B * xy_stride);
            xy2_ = (const float *)((const char *)xy + C * xy_stride);
            x0 = xy0_[0];
            y0 = xy0_[1];
            x1 = xy1_[0];
            y1 = xy1_[1];
            x2 = xy2_[0];
            y2 = xy2_[1];

            /* Check if triangle A B C is rectangle */
            if ((x0 == x2 && y1 == y2) || (y0 == y2 && x1 == x2)) {
                /* ok */
            } else {
                is_quad = 0;
#if DEBUG_SW_RENDER_GEOMETRY
                is_rectangle = 0;
#endif
            }

            xy2_ = (const float *)((const char *)xy + C2 * xy_stride);
            x2 = xy2_[0];
            y2 = xy2_[1];

            /* Check if triangle A B C2 is rectangle */
            if ((x0 == x2 && y1 == y2) || (y0 == y2 && x1 == x2)) {
                /* ok */
            } else {
                is_quad = 0;
#if DEBUG_SW_RENDER_GEOMETRY
                is_rectangle = 0;
#endif
            }
        }

        /* Check if uniformly colored */
        if (is_quad) {
            const SDL_FColor *col0_ = (const SDL_FColor *)((const char *)color + A * color_stride);
            const SDL_FColor *col1_ = (const SDL_FColor *)((const char *)color + B * color_stride);
            const SDL_FColor *col2_ = (const SDL_FColor *)((const char *)color + C * color_stride);
            const SDL_FColor *col3_ = (const SDL_FColor *)((const char *)color + C2 * color_stride);
            if (SDL_memcmp(col0_, col1_, sizeof(*col0_)) == 0 &&
                SDL_memcmp(col0_, col2_, sizeof(*col0_)) == 0 &&
                SDL_memcmp(col0_, col3_, sizeof(*col0_)) == 0) {
                /* ok */
            } else {
                is_quad = 0;
#if DEBUG_SW_RENDER_GEOMETRY
                is_uniform = 0;
#endif
            }
        }

        /* Start rendering rect */
        if (is_quad) {
            SDL_FRect s;
            SDL_FRect d;
            const float *xy0_, *xy1_, *uv0_, *uv1_;
            const SDL_FColor *col0_ = (const SDL_FColor *)((const char *)color + k0 * color_stride);

            xy0_ = (const float *)((const char *)xy + A * xy_stride);
            xy1_ = (const float *)((const char *)xy + B * xy_stride);

            if (texture) {
                uv0_ = (const float *)((const char *)uv + A * uv_stride);
                uv1_ = (const float *)((const char *)uv + B * uv_stride);
                s.x = uv0_[0] * texw;
                s.y = uv0_[1] * texh;
                s.w = uv1_[0] * texw - s.x;
                s.h = uv1_[1] * texh - s.y;
            } else {
                s.x = s.y = s.w = s.h = 0;
            }

            d.x = xy0_[0];
            d.y = xy0_[1];
            d.w = xy1_[0] - d.x;
            d.h = xy1_[1] - d.y;

            /* Rect + texture */
            if (texture && s.w != 0 && s.h != 0) {
                SDL_SetTextureAlphaModFloat(texture, col0_->a);
                SDL_SetTextureColorModFloat(texture, col0_->r, col0_->g, col0_->b);
                if (s.w > 0 && s.h > 0) {
                    SDL_RenderTexture(renderer, texture, &s, &d);
                } else {
                    int flags = 0;
                    if (s.w < 0) {
                        flags |= SDL_FLIP_HORIZONTAL;
                        s.w *= -1;
                        s.x -= s.w;
                    }
                    if (s.h < 0) {
                        flags |= SDL_FLIP_VERTICAL;
                        s.h *= -1;
                        s.y -= s.h;
                    }
                    SDL_RenderTextureRotated(renderer, texture, &s, &d, 0, NULL, (SDL_FlipMode)flags);
                }

#if DEBUG_SW_RENDER_GEOMETRY
                SDL_Log("Rect-COPY: RGB %f %f %f - Alpha:%f - texture=%p: src=(%d,%d, %d x %d) dst (%f, %f, %f x %f)", col0_->r, col0_->g, col0_->b, col0_->a,
                        (void *)texture, s.x, s.y, s.w, s.h, d.x, d.y, d.w, d.h);
#endif
            } else if (d.w != 0.0f && d.h != 0.0f) { /* Rect, no texture */
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColorFloat(renderer, col0_->r, col0_->g, col0_->b, col0_->a);
                SDL_RenderFillRect(renderer, &d);
#if DEBUG_SW_RENDER_GEOMETRY
                SDL_Log("Rect-FILL: RGB %f %f %f - Alpha:%f - texture=%p: dst (%f, %f, %f x %f)", col0_->r, col0_->g, col0_->b, col0_->a,
                        (void *)texture, d.x, d.y, d.w, d.h);
            } else {
                SDL_Log("Rect-DISMISS: RGB %f %f %f - Alpha:%f - texture=%p: src=(%d,%d, %d x %d) dst (%f, %f, %f x %f)", col0_->r, col0_->g, col0_->b, col0_->a,
                        (void *)texture, s.x, s.y, s.w, s.h, d.x, d.y, d.w, d.h);
#endif
            }

            prev[0] = -1;
        } else {
            /* Render triangles */
            if (prev[0] != -1) {
#if DEBUG_SW_RENDER_GEOMETRY
                SDL_Log("Triangle %d %d %d - is_uniform:%d is_rectangle:%d", prev[0], prev[1], prev[2], is_uniform, is_rectangle);
#endif
                retval = QueueCmdGeometry(renderer, texture,
                                          xy, xy_stride, color, color_stride, uv, uv_stride,
                                          num_vertices, prev, 3, 4,
                                          renderer->view->scale.x,
                                          renderer->view->scale.y);
                if (retval < 0) {
                    goto end;
                }
            }

            prev[0] = k0;
            prev[1] = k1;
            prev[2] = k2;
        }
    } /* End for (), next triangle */

    if (prev[0] != -1) {
        /* flush the last triangle */
#if DEBUG_SW_RENDER_GEOMETRY
        SDL_Log("Last triangle %d %d %d", prev[0], prev[1], prev[2]);
#endif
        retval = QueueCmdGeometry(renderer, texture,
                                  xy, xy_stride, color, color_stride, uv, uv_stride,
                                  num_vertices, prev, 3, 4,
                                  renderer->view->scale.x,
                                  renderer->view->scale.y);
        if (retval < 0) {
            goto end;
        }
    }

end:
    /* Restore */
    SDL_SetRenderDrawBlendMode(renderer, blendMode);
    SDL_SetRenderDrawColorFloat(renderer, r, g, b, a);

    return retval;
}
#endif /* SDL_VIDEO_RENDER_SW */

int SDL_RenderGeometryRawFloat(SDL_Renderer *renderer,
                          SDL_Texture *texture,
                          const float *xy, int xy_stride,
                          const SDL_FColor *color, int color_stride,
                          const float *uv, int uv_stride,
                          int num_vertices,
                          const void *indices, int num_indices, int size_indices)
{
    int i;
    int count = indices ? num_indices : num_vertices;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!renderer->QueueGeometry) {
        return SDL_Unsupported();
    }

    if (texture) {
        CHECK_TEXTURE_MAGIC(texture, -1);

        if (renderer != texture->renderer) {
            return SDL_SetError("Texture was not created with this renderer");
        }
    }

    if (!xy) {
        return SDL_InvalidParamError("xy");
    }

    if (!color) {
        return SDL_InvalidParamError("color");
    }

    if (texture && !uv) {
        return SDL_InvalidParamError("uv");
    }

    if (count % 3 != 0) {
        return SDL_InvalidParamError(indices ? "num_indices" : "num_vertices");
    }

    if (indices) {
        if (size_indices != 1 && size_indices != 2 && size_indices != 4) {
            return SDL_InvalidParamError("size_indices");
        }
    } else {
        size_indices = 0;
    }

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't draw while we're hidden */
    if (renderer->hidden) {
        return 0;
    }
#endif

    if (num_vertices < 3) {
        return 0;
    }

    if (texture && texture->native) {
        texture = texture->native;
    }

    if (texture) {
        for (i = 0; i < num_vertices; ++i) {
            const float *uv_ = (const float *)((const char *)uv + i * uv_stride);
            float u = uv_[0];
            float v = uv_[1];
            if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) {
                return SDL_SetError("Values of 'uv' out of bounds %f %f at %d/%d", u, v, i, num_vertices);
            }
        }
    }

    if (indices) {
        for (i = 0; i < num_indices; ++i) {
            int j;
            if (size_indices == 4) {
                j = ((const Uint32 *)indices)[i];
            } else if (size_indices == 2) {
                j = ((const Uint16 *)indices)[i];
            } else {
                j = ((const Uint8 *)indices)[i];
            }
            if (j < 0 || j >= num_vertices) {
                return SDL_SetError("Values of 'indices' out of bounds");
            }
        }
    }

    if (texture) {
        texture->last_command_generation = renderer->render_command_generation;
    }

    /* For the software renderer, try to reinterpret triangles as SDL_Rect */
#if SDL_VIDEO_RENDER_SW
    if (renderer->software) {
        return SDL_SW_RenderGeometryRaw(renderer, texture,
                                        xy, xy_stride, color, color_stride, uv, uv_stride, num_vertices,
                                        indices, num_indices, size_indices);
    }
#endif

    return QueueCmdGeometry(renderer, texture,
                              xy, xy_stride, color, color_stride, uv, uv_stride,
                              num_vertices,
                              indices, num_indices, size_indices,
                              renderer->view->scale.x,
                              renderer->view->scale.y);
}

int SDL_RenderGeometryRaw(SDL_Renderer *renderer, SDL_Texture *texture, const float *xy, int xy_stride, const SDL_Color *color, int color_stride, const float *uv, int uv_stride, int num_vertices, const void *indices, int num_indices, int size_indices)
{
    int i, retval, isstack;
    const Uint8 *color2 = (const Uint8 *)color;
    SDL_FColor *color3;

    if (num_vertices <= 0) {
        return SDL_InvalidParamError("num_vertices");
    }
    if (!color) {
        return SDL_InvalidParamError("color");
    }

    color3 = (SDL_FColor *)SDL_small_alloc(SDL_FColor, num_vertices, &isstack);
    if (!color3) {
        return -1;
    }

    for (i = 0; i < num_vertices; ++i) {
        color3[i].r = color->r / 255.0f;
        color3[i].g = color->g / 255.0f;
        color3[i].b = color->b / 255.0f;
        color3[i].a = color->a / 255.0f;
        color2 += color_stride;
        color = (const SDL_Color *)color2;
    }

    retval = SDL_RenderGeometryRawFloat(renderer, texture, xy, xy_stride, color3, sizeof(*color3), uv, uv_stride, num_vertices, indices, num_indices, size_indices);

    SDL_small_free(color3, isstack);

    return retval;
}

SDL_Surface *SDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect)
{
    SDL_Rect real_rect;
    SDL_Surface *surface;

    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (!renderer->RenderReadPixels) {
        SDL_Unsupported();
        return NULL;
    }

    FlushRenderCommands(renderer); /* we need to render before we read the results. */

    real_rect = renderer->view->pixel_viewport;

    if (rect) {
        if (!SDL_GetRectIntersection(rect, &real_rect, &real_rect)) {
            return NULL;
        }
    }

    surface = renderer->RenderReadPixels(renderer, &real_rect);
    if (surface) {
        SDL_PropertiesID props = SDL_GetSurfaceProperties(surface);

        if (renderer->target) {
            SDL_SetFloatProperty(props, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, renderer->target->SDR_white_point);
            SDL_SetFloatProperty(props, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, renderer->target->HDR_headroom);
        } else {
            SDL_SetFloatProperty(props, SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, renderer->SDR_white_point);
            SDL_SetFloatProperty(props, SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, renderer->HDR_headroom);
        }
    }
    return surface;
}

static void SDL_RenderApplyWindowShape(SDL_Renderer *renderer)
{
    SDL_Surface *shape = (SDL_Surface *)SDL_GetProperty(SDL_GetWindowProperties(renderer->window), SDL_PROP_WINDOW_SHAPE_POINTER, NULL);
    if (shape != renderer->shape_surface) {
        if (renderer->shape_texture) {
            SDL_DestroyTexture(renderer->shape_texture);
            renderer->shape_texture = NULL;
        }

        if (shape) {
            /* There's nothing we can do if this fails, so just keep on going */
            renderer->shape_texture = SDL_CreateTextureFromSurface(renderer, shape);

            SDL_SetTextureBlendMode(renderer->shape_texture,
                SDL_ComposeCustomBlendMode(
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                    SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD));
        }
        renderer->shape_surface = shape;
    }

    if (renderer->shape_texture) {
        SDL_RenderTexture(renderer, renderer->shape_texture, NULL, NULL);
    }
}

static void SDL_SimulateRenderVSync(SDL_Renderer *renderer)
{
    Uint64 now, elapsed;
    const Uint64 interval = renderer->simulate_vsync_interval_ns;

    if (!interval) {
        /* We can't do sub-ns delay, so just return here */
        return;
    }

    now = SDL_GetTicksNS();
    elapsed = (now - renderer->last_present);
    if (elapsed < interval) {
        Uint64 duration = (interval - elapsed);
        SDL_DelayNS(duration);
        now = SDL_GetTicksNS();
    }

    elapsed = (now - renderer->last_present);
    if (!renderer->last_present || elapsed > SDL_MS_TO_NS(1000)) {
        /* It's been too long, reset the presentation timeline */
        renderer->last_present = now;
    } else {
        renderer->last_present += (elapsed / interval) * interval;
    }
}

int SDL_RenderPresent(SDL_Renderer *renderer)
{
    SDL_bool presented = SDL_TRUE;

    CHECK_RENDERER_MAGIC(renderer, -1);

    if (renderer->logical_target) {
        SDL_SetRenderTargetInternal(renderer, NULL);
        SDL_RenderLogicalPresentation(renderer);
    }

    if (renderer->transparent_window) {
        SDL_RenderApplyWindowShape(renderer);
    }

    FlushRenderCommands(renderer); /* time to send everything to the GPU! */

#if DONT_DRAW_WHILE_HIDDEN
    /* Don't present while we're hidden */
    if (renderer->hidden) {
        presented = SDL_FALSE;
    } else
#endif
    if (renderer->RenderPresent(renderer) < 0) {
        presented = SDL_FALSE;
    }

    if (renderer->logical_target) {
        SDL_SetRenderTargetInternal(renderer, renderer->logical_target);
    }

    if (renderer->simulate_vsync ||
        (!presented && renderer->wanted_vsync)) {
        SDL_SimulateRenderVSync(renderer);
    }
    return 0;
}

static int SDL_DestroyTextureInternal(SDL_Texture *texture, SDL_bool is_destroying)
{
    SDL_Renderer *renderer;

    CHECK_TEXTURE_MAGIC(texture, -1);

    SDL_DestroyProperties(texture->props);

    renderer = texture->renderer;
    if (is_destroying) {
        /* Renderer get destroyed, avoid to queue more commands */
    } else {
        if (texture == renderer->target) {
            SDL_SetRenderTargetInternal(renderer, NULL); /* implies command queue flush */

            if (texture == renderer->logical_target) {
                /* Complete any logical presentation */
                SDL_RenderLogicalPresentation(renderer);
                FlushRenderCommands(renderer);
            }
        } else {
            FlushRenderCommandsIfTextureNeeded(texture);
        }
    }

    if (texture == renderer->logical_target) {
        renderer->logical_target = NULL;
    }

    SDL_SetObjectValid(texture, SDL_OBJECT_TYPE_TEXTURE, SDL_FALSE);

    if (texture->next) {
        texture->next->prev = texture->prev;
    }
    if (texture->prev) {
        texture->prev->next = texture->next;
    } else {
        renderer->textures = texture->next;
    }

    if (texture->native) {
        SDL_DestroyTextureInternal(texture->native, is_destroying);
    }
#if SDL_HAVE_YUV
    if (texture->yuv) {
        SDL_SW_DestroyYUVTexture(texture->yuv);
    }
#endif
    SDL_free(texture->pixels);

    renderer->DestroyTexture(renderer, texture);

    SDL_DestroySurface(texture->locked_surface);
    texture->locked_surface = NULL;

    SDL_free(texture);
    return 0;
}

void SDL_DestroyTexture(SDL_Texture *texture)
{
    SDL_DestroyTextureInternal(texture, SDL_FALSE /* is_destroying */);
}

static void SDL_DiscardAllCommands(SDL_Renderer *renderer)
{
    SDL_RenderCommand *cmd;

    if (renderer->render_commands_tail) {
        renderer->render_commands_tail->next = renderer->render_commands_pool;
        cmd = renderer->render_commands;
    } else {
        cmd = renderer->render_commands_pool;
    }

    renderer->render_commands_pool = NULL;
    renderer->render_commands_tail = NULL;
    renderer->render_commands = NULL;

    while (cmd) {
        SDL_RenderCommand *next = cmd->next;
        SDL_free(cmd);
        cmd = next;
    }
}

void SDL_DestroyRendererWithoutFreeing(SDL_Renderer *renderer)
{
    SDL_assert(renderer != NULL);
    SDL_assert(!renderer->destroyed);

    renderer->destroyed = SDL_TRUE;

    SDL_DestroyProperties(renderer->props);

    SDL_DelEventWatch(SDL_RendererEventWatch, renderer);

    SDL_DiscardAllCommands(renderer);

    /* Free existing textures for this renderer */
    while (renderer->textures) {
        SDL_Texture *tex = renderer->textures;
        SDL_DestroyTextureInternal(renderer->textures, SDL_TRUE /* is_destroying */);
        SDL_assert(tex != renderer->textures); /* satisfy static analysis. */
    }

    SDL_free(renderer->vertex_data);

    if (renderer->window) {
        SDL_ClearProperty(SDL_GetWindowProperties(renderer->window), SDL_PROP_WINDOW_RENDERER_POINTER);
    }

    /* Free the target mutex */
    SDL_DestroyMutex(renderer->target_mutex);
    renderer->target_mutex = NULL;

    /* Clean up renderer-specific resources */
    renderer->DestroyRenderer(renderer);
}

void SDL_DestroyRenderer(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC_BUT_NOT_DESTROYED_FLAG(renderer,);

    // if we've already destroyed the renderer through SDL_DestroyWindow, we just need
    // to free the renderer pointer. This lets apps destroy the window and renderer
    // in either order.
    if (!renderer->destroyed) {
        SDL_DestroyRendererWithoutFreeing(renderer);
    }

    SDL_Renderer *curr = SDL_renderers;
    SDL_Renderer *prev = NULL;
    while (curr) {
        if (curr == renderer) {
            if (prev) {
                prev->next = renderer->next;
            } else {
                SDL_renderers = renderer->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    SDL_SetObjectValid(renderer, SDL_OBJECT_TYPE_RENDERER, SDL_FALSE);  // It's no longer magical...

    SDL_free(renderer->texture_formats);
    SDL_free(renderer);
}

void *SDL_GetRenderMetalLayer(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (renderer->GetMetalLayer) {
        FlushRenderCommands(renderer); /* in case the app is going to mess with it. */
        return renderer->GetMetalLayer(renderer);
    }
    return NULL;
}

void *SDL_GetRenderMetalCommandEncoder(SDL_Renderer *renderer)
{
    CHECK_RENDERER_MAGIC(renderer, NULL);

    if (renderer->GetMetalCommandEncoder) {
        FlushRenderCommands(renderer); /* in case the app is going to mess with it. */
        return renderer->GetMetalCommandEncoder(renderer);
    }
    return NULL;
}

int SDL_AddVulkanRenderSemaphores(SDL_Renderer *renderer, Uint32 wait_stage_mask, Sint64 wait_semaphore, Sint64 signal_semaphore)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    if (!renderer->AddVulkanRenderSemaphores) {
        return SDL_Unsupported();
    }
    return renderer->AddVulkanRenderSemaphores(renderer, wait_stage_mask, wait_semaphore, signal_semaphore);
}

static SDL_BlendMode SDL_GetShortBlendMode(SDL_BlendMode blendMode)
{
    if (blendMode == SDL_BLENDMODE_NONE_FULL) {
        return SDL_BLENDMODE_NONE;
    }
    if (blendMode == SDL_BLENDMODE_BLEND_FULL) {
        return SDL_BLENDMODE_BLEND;
    }
    if (blendMode == SDL_BLENDMODE_ADD_FULL) {
        return SDL_BLENDMODE_ADD;
    }
    if (blendMode == SDL_BLENDMODE_MOD_FULL) {
        return SDL_BLENDMODE_MOD;
    }
    if (blendMode == SDL_BLENDMODE_MUL_FULL) {
        return SDL_BLENDMODE_MUL;
    }
    return blendMode;
}

static SDL_BlendMode SDL_GetLongBlendMode(SDL_BlendMode blendMode)
{
    if (blendMode == SDL_BLENDMODE_NONE) {
        return SDL_BLENDMODE_NONE_FULL;
    }
    if (blendMode == SDL_BLENDMODE_BLEND) {
        return SDL_BLENDMODE_BLEND_FULL;
    }
    if (blendMode == SDL_BLENDMODE_ADD) {
        return SDL_BLENDMODE_ADD_FULL;
    }
    if (blendMode == SDL_BLENDMODE_MOD) {
        return SDL_BLENDMODE_MOD_FULL;
    }
    if (blendMode == SDL_BLENDMODE_MUL) {
        return SDL_BLENDMODE_MUL_FULL;
    }
    return blendMode;
}

SDL_BlendMode SDL_ComposeCustomBlendMode(SDL_BlendFactor srcColorFactor, SDL_BlendFactor dstColorFactor,
                                         SDL_BlendOperation colorOperation,
                                         SDL_BlendFactor srcAlphaFactor, SDL_BlendFactor dstAlphaFactor,
                                         SDL_BlendOperation alphaOperation)
{
    SDL_BlendMode blendMode = SDL_COMPOSE_BLENDMODE(srcColorFactor, dstColorFactor, colorOperation,
                                                    srcAlphaFactor, dstAlphaFactor, alphaOperation);
    return SDL_GetShortBlendMode(blendMode);
}

SDL_BlendFactor SDL_GetBlendModeSrcColorFactor(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendFactor)(((Uint32)blendMode >> 4) & 0xF);
}

SDL_BlendFactor SDL_GetBlendModeDstColorFactor(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendFactor)(((Uint32)blendMode >> 8) & 0xF);
}

SDL_BlendOperation SDL_GetBlendModeColorOperation(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendOperation)(((Uint32)blendMode >> 0) & 0xF);
}

SDL_BlendFactor SDL_GetBlendModeSrcAlphaFactor(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendFactor)(((Uint32)blendMode >> 20) & 0xF);
}

SDL_BlendFactor SDL_GetBlendModeDstAlphaFactor(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendFactor)(((Uint32)blendMode >> 24) & 0xF);
}

SDL_BlendOperation SDL_GetBlendModeAlphaOperation(SDL_BlendMode blendMode)
{
    blendMode = SDL_GetLongBlendMode(blendMode);
    return (SDL_BlendOperation)(((Uint32)blendMode >> 16) & 0xF);
}

int SDL_SetRenderVSync(SDL_Renderer *renderer, int vsync)
{
    CHECK_RENDERER_MAGIC(renderer, -1);

    renderer->wanted_vsync = vsync ? SDL_TRUE : SDL_FALSE;

    /* for the software renderer, forward the call to the WindowTexture renderer */
#if SDL_VIDEO_RENDER_SW
    if (renderer->software) {
        if (!renderer->window) {
            return SDL_Unsupported();
        }
        if (SDL_SetWindowTextureVSync(NULL, renderer->window, vsync) == 0) {
            renderer->simulate_vsync = SDL_FALSE;
            return 0;
        }
    }
#endif

    if (!renderer->SetVSync) {
        switch (vsync) {
        case 0:
            renderer->simulate_vsync = SDL_FALSE;
            break;
        case 1:
            renderer->simulate_vsync = SDL_TRUE;
            break;
        default:
            return SDL_Unsupported();
        }
    } else if (renderer->SetVSync(renderer, vsync) < 0) {
        if (vsync == 1) {
            renderer->simulate_vsync = SDL_TRUE;
        } else {
            return -1;
        }
    }
    SDL_SetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_VSYNC_NUMBER, vsync);
    return 0;
}

int SDL_GetRenderVSync(SDL_Renderer *renderer, int *vsync)
{
    CHECK_RENDERER_MAGIC(renderer, -1);
    if (!vsync) {
        return SDL_InvalidParamError("vsync");
    }
    *vsync = (int)SDL_GetNumberProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_VSYNC_NUMBER, 0);
    return 0;
}
