/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#ifndef _SDL_sysrender_h
#define _SDL_sysrender_h

#include "SDL_render.h"
#include "SDL_events.h"
#include "SDL_yuv_sw_c.h"

/* The SDL 2D rendering system */

typedef struct SDL_RenderDriver SDL_RenderDriver;

typedef struct
{
    float x;
    float y;
} SDL_FPoint;

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} SDL_FRect;

/* Define the SDL texture structure */
struct SDL_Texture
{
    const void *magic;
    Uint32 format;              /**< The pixel format of the texture */
    int access;                 /**< SDL_TextureAccess */
    int w;                      /**< The width of the texture */
    int h;                      /**< The height of the texture */
    int modMode;                /**< The texture modulation mode */
    SDL_BlendMode blendMode;    /**< The texture blend mode */
    Uint8 r, g, b, a;           /**< Texture modulation values */

    SDL_Renderer *renderer;

    /* Support for formats not supported directly by the renderer */
    SDL_Texture *native;
    SDL_SW_YUVTexture *yuv;
    void *pixels;
    int pitch;
    SDL_Rect locked_rect;

    void *driverdata;           /**< Driver specific texture representation */

    SDL_Texture *prev;
    SDL_Texture *next;
};

/* Define the SDL renderer structure */
struct SDL_Renderer
{
    const void *magic;

    void (*WindowEvent) (SDL_Renderer * renderer, const SDL_WindowEvent *event);
    int (*GetOutputSize) (SDL_Renderer * renderer, int *w, int *h);
    int (*CreateTexture) (SDL_Renderer * renderer, SDL_Texture * texture);
    int (*SetTextureColorMod) (SDL_Renderer * renderer,
                               SDL_Texture * texture);
    int (*SetTextureAlphaMod) (SDL_Renderer * renderer,
                               SDL_Texture * texture);
    int (*SetTextureBlendMode) (SDL_Renderer * renderer,
                                SDL_Texture * texture);
    int (*UpdateTexture) (SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * rect, const void *pixels,
                          int pitch);
    int (*UpdateTextureYUV) (SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect,
                            const Uint8 *Yplane, int Ypitch,
                            const Uint8 *Uplane, int Upitch,
                            const Uint8 *Vplane, int Vpitch);
    int (*LockTexture) (SDL_Renderer * renderer, SDL_Texture * texture,
                        const SDL_Rect * rect, void **pixels, int *pitch);
    void (*UnlockTexture) (SDL_Renderer * renderer, SDL_Texture * texture);
    int (*SetRenderTarget) (SDL_Renderer * renderer, SDL_Texture * texture);
    int (*UpdateViewport) (SDL_Renderer * renderer);
    int (*UpdateClipRect) (SDL_Renderer * renderer);
    int (*RenderClear) (SDL_Renderer * renderer);
    int (*RenderDrawPoints) (SDL_Renderer * renderer, const SDL_FPoint * points,
                             int count);
    int (*RenderDrawLines) (SDL_Renderer * renderer, const SDL_FPoint * points,
                            int count);
    int (*RenderFillRects) (SDL_Renderer * renderer, const SDL_FRect * rects,
                            int count);
    int (*RenderCopy) (SDL_Renderer * renderer, SDL_Texture * texture,
                       const SDL_Rect * srcrect, const SDL_FRect * dstrect);
    int (*RenderCopyEx) (SDL_Renderer * renderer, SDL_Texture * texture,
                       const SDL_Rect * srcquad, const SDL_FRect * dstrect,
                       const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
    int (*RenderReadPixels) (SDL_Renderer * renderer, const SDL_Rect * rect,
                             Uint32 format, void * pixels, int pitch);
    void (*RenderPresent) (SDL_Renderer * renderer);
    void (*DestroyTexture) (SDL_Renderer * renderer, SDL_Texture * texture);

    void (*DestroyRenderer) (SDL_Renderer * renderer);

    int (*GL_BindTexture) (SDL_Renderer * renderer, SDL_Texture *texture, float *texw, float *texh);
    int (*GL_UnbindTexture) (SDL_Renderer * renderer, SDL_Texture *texture);

    /* The current renderer info */
    SDL_RendererInfo info;

    /* The window associated with the renderer */
    SDL_Window *window;
    SDL_bool hidden;

    /* The logical resolution for rendering */
    int logical_w;
    int logical_h;
    int logical_w_backup;
    int logical_h_backup;

    /* The drawable area within the window */
    SDL_Rect viewport;
    SDL_Rect viewport_backup;

    /* The clip rectangle within the window */
    SDL_Rect clip_rect;
    SDL_Rect clip_rect_backup;

    /* The render output coordinate scale */
    SDL_FPoint scale;
    SDL_FPoint scale_backup;

    /* The list of textures */
    SDL_Texture *textures;
    SDL_Texture *target;

    Uint8 r, g, b, a;                   /**< Color for drawing operations values */
    SDL_BlendMode blendMode;            /**< The drawing blend mode */

    void *driverdata;
};

/* Define the SDL render driver structure */
struct SDL_RenderDriver
{
    SDL_Renderer *(*CreateRenderer) (SDL_Window * window, Uint32 flags);

    /* Info about the renderer capabilities */
    SDL_RendererInfo info;
};

#if !SDL_RENDER_DISABLED

#if SDL_VIDEO_RENDER_D3D
extern SDL_RenderDriver D3D_RenderDriver;
#endif
#if SDL_VIDEO_RENDER_OGL
extern SDL_RenderDriver GL_RenderDriver;
#endif
#if SDL_VIDEO_RENDER_OGL_ES2
extern SDL_RenderDriver GLES2_RenderDriver;
#endif
#if SDL_VIDEO_RENDER_OGL_ES
extern SDL_RenderDriver GLES_RenderDriver;
#endif
#if SDL_VIDEO_RENDER_DIRECTFB
extern SDL_RenderDriver DirectFB_RenderDriver;
#endif
#if SDL_VIDEO_RENDER_PSP
extern SDL_RenderDriver PSP_RenderDriver;
#endif
extern SDL_RenderDriver SW_RenderDriver;

#endif /* !SDL_RENDER_DISABLED */

#endif /* _SDL_sysrender_h */

/* vi: set ts=4 sw=4 expandtab: */
