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

#ifndef SDL_sysrender_h_
#define SDL_sysrender_h_

#include "SDL_yuv_sw_c.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * A rectangle, with the origin at the upper left (double precision).
 */
typedef struct SDL_DRect
{
    double x;
    double y;
    double w;
    double h;
} SDL_DRect;

/* The SDL 2D rendering system */

typedef struct SDL_RenderDriver SDL_RenderDriver;

/* Rendering view state */
typedef struct SDL_RenderViewState
{
    int pixel_w;
    int pixel_h;
    SDL_Rect viewport;
    SDL_Rect pixel_viewport;
    SDL_Rect clip_rect;
    SDL_Rect pixel_clip_rect;
    SDL_bool clipping_enabled;
    SDL_FPoint scale;

} SDL_RenderViewState;

/* Define the SDL texture structure */
struct SDL_Texture
{
    SDL_Colorspace colorspace;  /**< The colorspace of the texture */
    float SDR_white_point;      /**< The SDR white point for this content */
    float HDR_headroom;         /**< The HDR headroom needed by this content */
    SDL_PixelFormat format;     /**< The pixel format of the texture */
    int access;                 /**< SDL_TextureAccess */
    int w;                      /**< The width of the texture */
    int h;                      /**< The height of the texture */
    SDL_BlendMode blendMode;    /**< The texture blend mode */
    SDL_ScaleMode scaleMode;    /**< The texture scale mode */
    SDL_FColor color;           /**< Texture modulation values */
    SDL_RenderViewState view;   /**< Target texture view state */

    SDL_Renderer *renderer;

    /* Support for formats not supported directly by the renderer */
    SDL_Texture *native;
    SDL_SW_YUVTexture *yuv;
    void *pixels;
    int pitch;
    SDL_Rect locked_rect;
    SDL_Surface *locked_surface; /**< Locked region exposed as a SDL surface */

    Uint32 last_command_generation; /* last command queue generation this texture was in. */

    SDL_PropertiesID props;

    void *driverdata; /**< Driver specific texture representation */

    SDL_Texture *prev;
    SDL_Texture *next;
};

typedef enum
{
    SDL_RENDERCMD_NO_OP,
    SDL_RENDERCMD_SETVIEWPORT,
    SDL_RENDERCMD_SETCLIPRECT,
    SDL_RENDERCMD_SETDRAWCOLOR,
    SDL_RENDERCMD_CLEAR,
    SDL_RENDERCMD_DRAW_POINTS,
    SDL_RENDERCMD_DRAW_LINES,
    SDL_RENDERCMD_FILL_RECTS,
    SDL_RENDERCMD_COPY,
    SDL_RENDERCMD_COPY_EX,
    SDL_RENDERCMD_GEOMETRY
} SDL_RenderCommandType;

typedef struct SDL_RenderCommand
{
    SDL_RenderCommandType command;
    union
    {
        struct
        {
            size_t first;
            SDL_Rect rect;
        } viewport;
        struct
        {
            SDL_bool enabled;
            SDL_Rect rect;
        } cliprect;
        struct
        {
            size_t first;
            size_t count;
            float color_scale;
            SDL_FColor color;
            SDL_BlendMode blend;
            SDL_Texture *texture;
        } draw;
        struct
        {
            size_t first;
            float color_scale;
            SDL_FColor color;
        } color;
    } data;
    struct SDL_RenderCommand *next;
} SDL_RenderCommand;

typedef struct SDL_VertexSolid
{
    SDL_FPoint position;
    SDL_FColor color;
} SDL_VertexSolid;

typedef enum
{
    SDL_RENDERLINEMETHOD_POINTS,
    SDL_RENDERLINEMETHOD_LINES,
    SDL_RENDERLINEMETHOD_GEOMETRY,
} SDL_RenderLineMethod;

/* Define the SDL renderer structure */
struct SDL_Renderer
{
    void (*WindowEvent)(SDL_Renderer *renderer, const SDL_WindowEvent *event);
    int (*GetOutputSize)(SDL_Renderer *renderer, int *w, int *h);
    SDL_bool (*SupportsBlendMode)(SDL_Renderer *renderer, SDL_BlendMode blendMode);
    int (*CreateTexture)(SDL_Renderer *renderer, SDL_Texture *texture, SDL_PropertiesID create_props);
    int (*QueueSetViewport)(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
    int (*QueueSetDrawColor)(SDL_Renderer *renderer, SDL_RenderCommand *cmd);
    int (*QueueDrawPoints)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points,
                           int count);
    int (*QueueDrawLines)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FPoint *points,
                          int count);
    int (*QueueFillRects)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, const SDL_FRect *rects,
                          int count);
    int (*QueueCopy)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                     const SDL_FRect *srcrect, const SDL_FRect *dstrect);
    int (*QueueCopyEx)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                       const SDL_FRect *srcquad, const SDL_FRect *dstrect,
                       const double angle, const SDL_FPoint *center, const SDL_FlipMode flip, float scale_x, float scale_y);
    int (*QueueGeometry)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, SDL_Texture *texture,
                         const float *xy, int xy_stride, const SDL_FColor *color, int color_stride, const float *uv, int uv_stride,
                         int num_vertices, const void *indices, int num_indices, int size_indices,
                         float scale_x, float scale_y);

    void (*InvalidateCachedState)(SDL_Renderer *renderer);
    int (*RunCommandQueue)(SDL_Renderer *renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);
    int (*UpdateTexture)(SDL_Renderer *renderer, SDL_Texture *texture,
                         const SDL_Rect *rect, const void *pixels,
                         int pitch);
#if SDL_HAVE_YUV
    int (*UpdateTextureYUV)(SDL_Renderer *renderer, SDL_Texture *texture,
                            const SDL_Rect *rect,
                            const Uint8 *Yplane, int Ypitch,
                            const Uint8 *Uplane, int Upitch,
                            const Uint8 *Vplane, int Vpitch);
    int (*UpdateTextureNV)(SDL_Renderer *renderer, SDL_Texture *texture,
                           const SDL_Rect *rect,
                           const Uint8 *Yplane, int Ypitch,
                           const Uint8 *UVplane, int UVpitch);
#endif
    int (*LockTexture)(SDL_Renderer *renderer, SDL_Texture *texture,
                       const SDL_Rect *rect, void **pixels, int *pitch);
    void (*UnlockTexture)(SDL_Renderer *renderer, SDL_Texture *texture);
    void (*SetTextureScaleMode)(SDL_Renderer *renderer, SDL_Texture *texture, SDL_ScaleMode scaleMode);
    int (*SetRenderTarget)(SDL_Renderer *renderer, SDL_Texture *texture);
    SDL_Surface *(*RenderReadPixels)(SDL_Renderer *renderer, const SDL_Rect *rect);
    int (*RenderPresent)(SDL_Renderer *renderer);
    void (*DestroyTexture)(SDL_Renderer *renderer, SDL_Texture *texture);

    void (*DestroyRenderer)(SDL_Renderer *renderer);

    int (*SetVSync)(SDL_Renderer *renderer, int vsync);

    void *(*GetMetalLayer)(SDL_Renderer *renderer);
    void *(*GetMetalCommandEncoder)(SDL_Renderer *renderer);

    int (*AddVulkanRenderSemaphores)(SDL_Renderer *renderer, Uint32 wait_stage_mask, Sint64 wait_semaphore, Sint64 signal_semaphore);

    /* The current renderer info */
    const char *name;
    SDL_PixelFormat *texture_formats;
    int num_texture_formats;
    SDL_bool software;

    /* The window associated with the renderer */
    SDL_Window *window;
    SDL_bool hidden;

    /* Whether we should simulate vsync */
    SDL_bool wanted_vsync;
    SDL_bool simulate_vsync;
    Uint64 simulate_vsync_interval_ns;
    Uint64 last_present;

    /* Support for logical output coordinates */
    SDL_Texture *logical_target;
    SDL_RendererLogicalPresentation logical_presentation_mode;
    SDL_ScaleMode logical_scale_mode;
    SDL_FRect logical_src_rect;
    SDL_FRect logical_dst_rect;

    SDL_RenderViewState *view;
    SDL_RenderViewState main_view;

    /* The window pixel to point coordinate scale */
    SDL_FPoint dpi_scale;

    /* The method of drawing lines */
    SDL_RenderLineMethod line_method;

    /* List of triangle indices to draw rects */
    int rect_index_order[6];

    /* The list of textures */
    SDL_Texture *textures;
    SDL_Texture *target;
    SDL_Mutex *target_mutex;

    SDL_Colorspace output_colorspace;
    float SDR_white_point;
    float HDR_headroom;

    float color_scale;
    SDL_FColor color;        /**< Color for drawing operations values */
    SDL_BlendMode blendMode; /**< The drawing blend mode */

    SDL_RenderCommand *render_commands;
    SDL_RenderCommand *render_commands_tail;
    SDL_RenderCommand *render_commands_pool;
    Uint32 render_command_generation;
    SDL_FColor last_queued_color;
    float last_queued_color_scale;
    SDL_Rect last_queued_viewport;
    SDL_Rect last_queued_cliprect;
    SDL_bool last_queued_cliprect_enabled;
    SDL_bool color_queued;
    SDL_bool color_scale_queued;
    SDL_bool viewport_queued;
    SDL_bool cliprect_queued;

    void *vertex_data;
    size_t vertex_data_used;
    size_t vertex_data_allocation;

    /* Shaped window support */
    SDL_bool transparent_window;
    SDL_Surface *shape_surface;
    SDL_Texture *shape_texture;

    SDL_PropertiesID props;

    SDL_bool destroyed;   // already destroyed by SDL_DestroyWindow; just free this struct in SDL_DestroyRenderer.

    void *driverdata;

    SDL_Renderer *next;
};

/* Define the SDL render driver structure */
struct SDL_RenderDriver
{
    int (*CreateRenderer)(SDL_Renderer *renderer, SDL_Window *window, SDL_PropertiesID props);

    const char *name;
};

/* Not all of these are available in a given build. Use #ifdefs, etc. */
extern SDL_RenderDriver D3D_RenderDriver;
extern SDL_RenderDriver D3D11_RenderDriver;
extern SDL_RenderDriver D3D12_RenderDriver;
extern SDL_RenderDriver GL_RenderDriver;
extern SDL_RenderDriver GLES2_RenderDriver;
extern SDL_RenderDriver METAL_RenderDriver;
extern SDL_RenderDriver VULKAN_RenderDriver;
extern SDL_RenderDriver PS2_RenderDriver;
extern SDL_RenderDriver PSP_RenderDriver;
extern SDL_RenderDriver SW_RenderDriver;
extern SDL_RenderDriver VITA_GXM_RenderDriver;

/* Clean up any renderers at shutdown */
extern void SDL_QuitRender(void);

/* Add a supported texture format to a renderer */
extern int SDL_AddSupportedTextureFormat(SDL_Renderer *renderer, SDL_PixelFormat format);

/* Setup colorspace conversion */
extern void SDL_SetupRendererColorspace(SDL_Renderer *renderer, SDL_PropertiesID props);

/* Colorspace conversion functions */
extern SDL_bool SDL_RenderingLinearSpace(SDL_Renderer *renderer);
extern void SDL_ConvertToLinear(SDL_FColor *color);
extern void SDL_ConvertFromLinear(SDL_FColor *color);

/* Blend mode functions */
extern SDL_BlendFactor SDL_GetBlendModeSrcColorFactor(SDL_BlendMode blendMode);
extern SDL_BlendFactor SDL_GetBlendModeDstColorFactor(SDL_BlendMode blendMode);
extern SDL_BlendOperation SDL_GetBlendModeColorOperation(SDL_BlendMode blendMode);
extern SDL_BlendFactor SDL_GetBlendModeSrcAlphaFactor(SDL_BlendMode blendMode);
extern SDL_BlendFactor SDL_GetBlendModeDstAlphaFactor(SDL_BlendMode blendMode);
extern SDL_BlendOperation SDL_GetBlendModeAlphaOperation(SDL_BlendMode blendMode);

/* drivers call this during their Queue*() methods to make space in a array that are used
   for a vertex buffer during RunCommandQueue(). Pointers returned here are only valid until
   the next call, because it might be in an array that gets realloc()'d. */
extern void *SDL_AllocateRenderVertices(SDL_Renderer *renderer, const size_t numbytes, const size_t alignment, size_t *offset);

// Let the video subsystem destroy a renderer without making its pointer invalid.
extern void SDL_DestroyRendererWithoutFreeing(SDL_Renderer *renderer);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_sysrender_h_ */
