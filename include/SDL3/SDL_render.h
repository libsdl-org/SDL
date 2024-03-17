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

/**
 *  \file SDL_render.h
 *
 *  Header file for SDL 2D rendering functions.
 *
 *  This API supports the following features:
 *      * single pixel points
 *      * single pixel lines
 *      * filled rectangles
 *      * texture images
 *
 *  The primitives may be drawn in opaque, blended, or additive modes.
 *
 *  The texture images may be drawn in opaque, blended, or additive modes.
 *  They can have an additional color tint or alpha modulation applied to
 *  them, and may also be stretched with linear interpolation.
 *
 *  This API is designed to accelerate simple 2D operations. You may
 *  want more functionality such as polygons and particle effects and
 *  in that case you should use SDL's OpenGL/Direct3D support or one
 *  of the many good 3D engines.
 *
 *  These functions must be called from the main thread.
 *  See this bug for details: https://github.com/libsdl-org/SDL/issues/986
 */

#ifndef SDL_render_h_
#define SDL_render_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Flags used when creating a rendering context
 */
typedef enum
{
    SDL_RENDERER_SOFTWARE = 0x00000001,         /**< The renderer is a software fallback */
    SDL_RENDERER_ACCELERATED = 0x00000002,      /**< The renderer uses hardware
                                                     acceleration */
    SDL_RENDERER_PRESENTVSYNC = 0x00000004      /**< Present is synchronized
                                                     with the refresh rate */
} SDL_RendererFlags;

/**
 * Information on the capabilities of a render driver or context.
 */
typedef struct SDL_RendererInfo
{
    const char *name;           /**< The name of the renderer */
    Uint32 flags;               /**< Supported ::SDL_RendererFlags */
    int num_texture_formats;    /**< The number of available texture formats */
    SDL_PixelFormatEnum texture_formats[16]; /**< The available texture formats */
    int max_texture_width;      /**< The maximum texture width */
    int max_texture_height;     /**< The maximum texture height */
} SDL_RendererInfo;

/**
 *  Vertex structure
 */
typedef struct SDL_Vertex
{
    SDL_FPoint position;        /**< Vertex position, in SDL_Renderer coordinates  */
    SDL_FColor color;           /**< Vertex color */
    SDL_FPoint tex_coord;       /**< Normalized texture coordinates, if needed */
} SDL_Vertex;

/**
 * The access pattern allowed for a texture.
 */
typedef enum
{
    SDL_TEXTUREACCESS_STATIC,    /**< Changes rarely, not lockable */
    SDL_TEXTUREACCESS_STREAMING, /**< Changes frequently, lockable */
    SDL_TEXTUREACCESS_TARGET     /**< Texture can be used as a render target */
} SDL_TextureAccess;

/**
 * How the logical size is mapped to the output
 */
typedef enum
{
    SDL_LOGICAL_PRESENTATION_DISABLED,  /**< There is no logical size in effect */
    SDL_LOGICAL_PRESENTATION_STRETCH,   /**< The rendered content is stretched to the output resolution */
    SDL_LOGICAL_PRESENTATION_LETTERBOX, /**< The rendered content is fit to the largest dimension and the other dimension is letterboxed with black bars */
    SDL_LOGICAL_PRESENTATION_OVERSCAN,  /**< The rendered content is fit to the smallest dimension and the other dimension extends beyond the output bounds */
    SDL_LOGICAL_PRESENTATION_INTEGER_SCALE   /**< The rendered content is scaled up by integer multiples to fit the output resolution */
} SDL_RendererLogicalPresentation;

/**
 * A structure representing rendering state
 */
struct SDL_Renderer;
typedef struct SDL_Renderer SDL_Renderer;

/**
 * An efficient driver-specific representation of pixel data
 */
struct SDL_Texture;
typedef struct SDL_Texture SDL_Texture;

/* Function prototypes */

/**
 * Get the number of 2D rendering drivers available for the current display.
 *
 * A render driver is a set of code that handles rendering and texture
 * management on a particular display. Normally there is only one, but some
 * drivers may have several available with different capabilities.
 *
 * There may be none if SDL was compiled without render support.
 *
 * \returns a number >= 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRenderer
 * \sa SDL_GetRenderDriver
 */
extern DECLSPEC int SDLCALL SDL_GetNumRenderDrivers(void);

/**
 * Use this function to get the name of a built in 2D rendering driver.
 *
 * The list of rendering drivers is given in the order that they are normally
 * initialized by default; the drivers that seem more reasonable to choose
 * first (as far as the SDL developers believe) are earlier in the list.
 *
 * The names of drivers are all simple, low-ASCII identifiers, like "opengl",
 * "direct3d12" or "metal". These never have Unicode characters, and are not
 * meant to be proper names.
 *
 * The returned value points to a static, read-only string; do not modify or
 * free it!
 *
 * \param index the index of the rendering driver; the value ranges from 0 to
 *              SDL_GetNumRenderDrivers() - 1
 * \returns the name of the rendering driver at the requested index, or NULL
 *          if an invalid index was specified.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetNumRenderDrivers
 */
extern DECLSPEC const char *SDLCALL SDL_GetRenderDriver(int index);

/**
 * Create a window and default renderer.
 *
 * \param width the width of the window
 * \param height the height of the window
 * \param window_flags the flags used to create the window (see
 *                     SDL_CreateWindow())
 * \param window a pointer filled with the window, or NULL on error
 * \param renderer a pointer filled with the renderer, or NULL on error
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRenderer
 * \sa SDL_CreateWindow
 */
extern DECLSPEC int SDLCALL SDL_CreateWindowAndRenderer(int width, int height, SDL_WindowFlags window_flags, SDL_Window **window, SDL_Renderer **renderer);

/**
 * Create a 2D rendering context for a window.
 *
 * If you want a specific renderer, you can specify its name here. A list of
 * available renderers can be obtained by calling SDL_GetRenderDriver multiple
 * times, with indices from 0 to SDL_GetNumRenderDrivers()-1. If you don't
 * need a specific renderer, specify NULL and SDL will attempt to choose the
 * best option for you, based on what is available on the user's system.
 *
 * If you pass SDL_RENDERER_SOFTWARE in the flags, you will get a software
 * renderer, otherwise you will get a hardware accelerated renderer if
 * available.
 *
 * By default the rendering size matches the window size in pixels, but you
 * can call SDL_SetRenderLogicalPresentation() to change the content size and
 * scaling options.
 *
 * \param window the window where rendering is displayed
 * \param name the name of the rendering driver to initialize, or NULL to
 *             initialize the first one supporting the requested flags
 * \param flags 0, or one or more SDL_RendererFlags OR'd together
 * \returns a valid rendering context or NULL if there was an error; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRendererWithProperties
 * \sa SDL_CreateSoftwareRenderer
 * \sa SDL_DestroyRenderer
 * \sa SDL_GetNumRenderDrivers
 * \sa SDL_GetRenderDriver
 * \sa SDL_GetRendererInfo
 */
extern DECLSPEC SDL_Renderer * SDLCALL SDL_CreateRenderer(SDL_Window *window, const char *name, Uint32 flags);

/**
 * Create a 2D rendering context for a window, with the specified properties.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_RENDERER_CREATE_NAME_STRING`: the name of the rendering driver
 *   to use, if a specific one is desired
 * - `SDL_PROP_RENDERER_CREATE_WINDOW_POINTER`: the window where rendering is
 *   displayed, required if this isn't a software renderer using a surface
 * - `SDL_PROP_RENDERER_CREATE_SURFACE_POINTER`: the surface where rendering
 *   is displayed, if you want a software renderer without a window
 * - `SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER`: an SDL_ColorSpace
 *   value describing the colorspace for output to the display, defaults to
 *   SDL_COLORSPACE_SRGB. The direct3d11, direct3d12, and metal renderers
 *   support SDL_COLORSPACE_SRGB_LINEAR, which is a linear color space and
 *   supports HDR output. If you select SDL_COLORSPACE_SRGB_LINEAR, drawing
 *   still uses the sRGB colorspace, but values can go beyond 1.0 and float
 *   (linear) format textures can be used for HDR content.
 * - `SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_BOOLEAN`: true if you want
 *   present synchronized with the refresh rate
 *
 * With the vulkan renderer:
 *
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER`: the VkInstance to use
 *   with the renderer, optional.
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_SURFACE_NUMBER`: the VkSurfaceKHR to use
 *   with the renderer, optional.
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER`: the
 *   VkPhysicalDevice to use with the renderer, optional.
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER`: the VkDevice to use
 *   with the renderer, optional.
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER`: the
 *   queue family index used for rendering.
 * - `SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER`: the
 *   queue family index used for presentation.
 *
 * \param props the properties to use
 * \returns a valid rendering context or NULL if there was an error; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateProperties
 * \sa SDL_CreateRenderer
 * \sa SDL_CreateSoftwareRenderer
 * \sa SDL_DestroyRenderer
 * \sa SDL_GetRendererInfo
 */
extern DECLSPEC SDL_Renderer * SDLCALL SDL_CreateRendererWithProperties(SDL_PropertiesID props);

#define SDL_PROP_RENDERER_CREATE_NAME_STRING                                "name"
#define SDL_PROP_RENDERER_CREATE_WINDOW_POINTER                             "window"
#define SDL_PROP_RENDERER_CREATE_SURFACE_POINTER                            "surface"
#define SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER                   "output_colorspace"
#define SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_BOOLEAN                      "present_vsync"
#define SDL_PROP_RENDERER_CREATE_VULKAN_INSTANCE_POINTER                    "vulkan.instance"
#define SDL_PROP_RENDERER_CREATE_VULKAN_SURFACE_NUMBER                      "vulkan.surface"
#define SDL_PROP_RENDERER_CREATE_VULKAN_PHYSICAL_DEVICE_POINTER             "vulkan.physical_device"
#define SDL_PROP_RENDERER_CREATE_VULKAN_DEVICE_POINTER                      "vulkan.device"
#define SDL_PROP_RENDERER_CREATE_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER  "vulkan.graphics_queue_family_index"
#define SDL_PROP_RENDERER_CREATE_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER   "vulkan.present_queue_family_index"

/**
 * Create a 2D software rendering context for a surface.
 *
 * Two other API which can be used to create SDL_Renderer:
 * SDL_CreateRenderer() and SDL_CreateWindowAndRenderer(). These can _also_
 * create a software renderer, but they are intended to be used with an
 * SDL_Window as the final destination and not an SDL_Surface.
 *
 * \param surface the SDL_Surface structure representing the surface where
 *                rendering is done
 * \returns a valid rendering context or NULL if there was an error; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_DestroyRenderer
 */
extern DECLSPEC SDL_Renderer *SDLCALL SDL_CreateSoftwareRenderer(SDL_Surface *surface);

/**
 * Get the renderer associated with a window.
 *
 * \param window the window to query
 * \returns the rendering context on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Renderer *SDLCALL SDL_GetRenderer(SDL_Window *window);

/**
 * Get the window associated with a renderer.
 *
 * \param renderer the renderer to query
 * \returns the window on success or NULL on failure; call SDL_GetError() for
 *          more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Window *SDLCALL SDL_GetRenderWindow(SDL_Renderer *renderer);

/**
 * Get information about a rendering context.
 *
 * \param renderer the rendering context
 * \param info an SDL_RendererInfo structure filled with information about the
 *             current renderer
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRenderer
 * \sa SDL_CreateRendererWithProperties
 */
extern DECLSPEC int SDLCALL SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info);

/**
 * Get the properties associated with a renderer.
 *
 * The following read-only properties are provided by SDL:
 *
 * - `SDL_PROP_RENDERER_NAME_STRING`: the name of the rendering driver
 * - `SDL_PROP_RENDERER_WINDOW_POINTER`: the window where rendering is
 *   displayed, if any
 * - `SDL_PROP_RENDERER_SURFACE_POINTER`: the surface where rendering is
 *   displayed, if this is a software renderer without a window
 * - `SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER`: an SDL_ColorSpace value
 *   describing the colorspace for output to the display, defaults to
 *   SDL_COLORSPACE_SRGB.
 * - `SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN`: true if the output colorspace is
 *   SDL_COLORSPACE_SRGB_LINEAR and the renderer is showing on a display with
 *   HDR enabled. This property can change dynamically when
 *   SDL_EVENT_DISPLAY_HDR_STATE_CHANGED is sent.
 * - `SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT`: the value of SDR white in the
 *   SDL_COLORSPACE_SRGB_LINEAR colorspace. When HDR is enabled, this value is
 *   automatically multiplied into the color scale. This property can change
 *   dynamically when SDL_EVENT_DISPLAY_HDR_STATE_CHANGED is sent.
 * - `SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT`: the additional high dynamic range
 *   that can be displayed, in terms of the SDR white point. When HDR is not
 *   enabled, this will be 1.0. This property can change dynamically when
 *   SDL_EVENT_DISPLAY_HDR_STATE_CHANGED is sent.
 *
 * With the direct3d renderer:
 *
 * - `SDL_PROP_RENDERER_D3D9_DEVICE_POINTER`: the IDirect3DDevice9 associated
 *   with the renderer
 *
 * With the direct3d11 renderer:
 *
 * - `SDL_PROP_RENDERER_D3D11_DEVICE_POINTER`: the ID3D11Device associated
 *   with the renderer
 *
 * With the direct3d12 renderer:
 *
 * - `SDL_PROP_RENDERER_D3D12_DEVICE_POINTER`: the ID3D12Device associated
 *   with the renderer
 * - `SDL_PROP_RENDERER_D3D12_COMMAND_QUEUE_POINTER`: the ID3D12CommandQueue
 *   associated with the renderer
 *
 * With the vulkan renderer:
 *
 * - `SDL_PROP_RENDERER_VULKAN_INSTANCE_POINTER`: the VkInstance associated
 *   with the renderer
 * - `SDL_PROP_RENDERER_VULKAN_SURFACE_NUMBER`: the VkSurfaceKHR associated
 *   with the renderer
 * - `SDL_PROP_RENDERER_VULKAN_PHYSICAL_DEVICE_POINTER`: the VkPhysicalDevice
 *   associated with the renderer
 * - `SDL_PROP_RENDERER_VULKAN_DEVICE_POINTER`: the VkDevice associated with
 *   the renderer
 * - `SDL_PROP_RENDERER_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER`: the queue
 *   family index used for rendering
 * - `SDL_PROP_RENDERER_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER`: the queue
 *   family index used for presentation
 * - `SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER`: the number of
 *   swapchain images, or potential frames in flight, used by the Vulkan
 *   renderer
 *
 * \param renderer the rendering context
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetProperty
 */
extern DECLSPEC SDL_PropertiesID SDLCALL SDL_GetRendererProperties(SDL_Renderer *renderer);

#define SDL_PROP_RENDERER_NAME_STRING                               "SDL.renderer.name"
#define SDL_PROP_RENDERER_WINDOW_POINTER                            "SDL.renderer.window"
#define SDL_PROP_RENDERER_SURFACE_POINTER                           "SDL.renderer.surface"
#define SDL_PROP_RENDERER_OUTPUT_COLORSPACE_NUMBER                  "SDL.renderer.output_colorspace"
#define SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN                       "SDL.renderer.HDR_enabled"
#define SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT                     "SDL.renderer.SDR_white_point"
#define SDL_PROP_RENDERER_HDR_HEADROOM_FLOAT                        "SDL.renderer.HDR_headroom"
#define SDL_PROP_RENDERER_D3D9_DEVICE_POINTER                       "SDL.renderer.d3d9.device"
#define SDL_PROP_RENDERER_D3D11_DEVICE_POINTER                      "SDL.renderer.d3d11.device"
#define SDL_PROP_RENDERER_D3D12_DEVICE_POINTER                      "SDL.renderer.d3d12.device"
#define SDL_PROP_RENDERER_D3D12_COMMAND_QUEUE_POINTER               "SDL.renderer.d3d12.command_queue"
#define SDL_PROP_RENDERER_VULKAN_INSTANCE_POINTER                   "SDL.renderer.vulkan.instance"
#define SDL_PROP_RENDERER_VULKAN_SURFACE_NUMBER                     "SDL.renderer.vulkan.surface"
#define SDL_PROP_RENDERER_VULKAN_PHYSICAL_DEVICE_POINTER            "SDL.renderer.vulkan.physical_device"
#define SDL_PROP_RENDERER_VULKAN_DEVICE_POINTER                     "SDL.renderer.vulkan.device"
#define SDL_PROP_RENDERER_VULKAN_GRAPHICS_QUEUE_FAMILY_INDEX_NUMBER "SDL.renderer.vulkan.graphics_queue_family_index"
#define SDL_PROP_RENDERER_VULKAN_PRESENT_QUEUE_FAMILY_INDEX_NUMBER  "SDL.renderer.vulkan.present_queue_family_index"
#define SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER       "SDL.renderer.vulkan.swapchain_image_count"

/**
 * Get the output size in pixels of a rendering context.
 *
 * This returns the true output size in pixels, ignoring any render targets or
 * logical size and presentation.
 *
 * \param renderer the rendering context
 * \param w a pointer filled in with the width in pixels
 * \param h a pointer filled in with the height in pixels
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetCurrentRenderOutputSize
 */
extern DECLSPEC int SDLCALL SDL_GetRenderOutputSize(SDL_Renderer *renderer, int *w, int *h);

/**
 * Get the current output size in pixels of a rendering context.
 *
 * If a rendering target is active, this will return the size of the rendering
 * target in pixels, otherwise if a logical size is set, it will return the
 * logical size, otherwise it will return the value of
 * SDL_GetRenderOutputSize().
 *
 * \param renderer the rendering context
 * \param w a pointer filled in with the current width
 * \param h a pointer filled in with the current height
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderOutputSize
 */
extern DECLSPEC int SDLCALL SDL_GetCurrentRenderOutputSize(SDL_Renderer *renderer, int *w, int *h);

/**
 * Create a texture for a rendering context.
 *
 * You can set the texture scaling method by setting
 * `SDL_HINT_RENDER_SCALE_QUALITY` before creating the texture.
 *
 * \param renderer the rendering context
 * \param format one of the enumerated values in SDL_PixelFormatEnum
 * \param access one of the enumerated values in SDL_TextureAccess
 * \param w the width of the texture in pixels
 * \param h the height of the texture in pixels
 * \returns a pointer to the created texture or NULL if no rendering context
 *          was active, the format was unsupported, or the width or height
 *          were out of range; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTextureFromSurface
 * \sa SDL_CreateTextureWithProperties
 * \sa SDL_DestroyTexture
 * \sa SDL_QueryTexture
 * \sa SDL_UpdateTexture
 */
extern DECLSPEC SDL_Texture *SDLCALL SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h);

/**
 * Create a texture from an existing surface.
 *
 * The surface is not modified or freed by this function.
 *
 * The SDL_TextureAccess hint for the created texture is
 * `SDL_TEXTUREACCESS_STATIC`.
 *
 * The pixel format of the created texture may be different from the pixel
 * format of the surface. Use SDL_QueryTexture() to query the pixel format of
 * the texture.
 *
 * \param renderer the rendering context
 * \param surface the SDL_Surface structure containing pixel data used to fill
 *                the texture
 * \returns the created texture or NULL on failure; call SDL_GetError() for
 *          more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTexture
 * \sa SDL_CreateTextureWithProperties
 * \sa SDL_DestroyTexture
 * \sa SDL_QueryTexture
 */
extern DECLSPEC SDL_Texture *SDLCALL SDL_CreateTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface);

/**
 * Create a texture for a rendering context with the specified properties.
 *
 * These are the supported properties:
 *
 * - `SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER`: an SDL_ColorSpace value
 *   describing the texture colorspace, defaults to SDL_COLORSPACE_SRGB_LINEAR
 *   for floating point textures, SDL_COLORSPACE_HDR10 for 10-bit textures,
 *   SDL_COLORSPACE_SRGB for other RGB textures and SDL_COLORSPACE_JPEG for
 *   YUV textures.
 * - `SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER`: one of the enumerated values in
 *   SDL_PixelFormatEnum, defaults to the best RGBA format for the renderer
 * - `SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER`: one of the enumerated values in
 *   SDL_TextureAccess, defaults to SDL_TEXTUREACCESS_STATIC
 * - `SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER`: the width of the texture in
 *   pixels, required
 * - `SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER`: the height of the texture in
 *   pixels, required
 * - `SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT`: for HDR10 and floating
 *   point textures, this defines the value of 100% diffuse white, with higher
 *   values being displayed in the High Dynamic Range headroom. This defaults
 *   to 100 for HDR10 textures and 1.0 for floating point textures.
 * - `SDL_PROP_TEXTURE_CREATE_HDR_HEADROOM_FLOAT`: for HDR10 and floating
 *   point textures, this defines the maximum dynamic range used by the
 *   content, in terms of the SDR white point. This would be equivalent to
 *   maxCLL / SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT for HDR10 content.
 *   If this is defined, any values outside the range supported by the display
 *   will be scaled into the available HDR headroom, otherwise they are
 *   clipped.
 *
 * With the direct3d11 renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER`: the ID3D11Texture2D
 *   associated with the texture, if you want to wrap an existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_U_POINTER`: the ID3D11Texture2D
 *   associated with the U plane of a YUV texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_V_POINTER`: the ID3D11Texture2D
 *   associated with the V plane of a YUV texture, if you want to wrap an
 *   existing texture.
 *
 * With the direct3d12 renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_POINTER`: the ID3D12Resource
 *   associated with the texture, if you want to wrap an existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_U_POINTER`: the ID3D12Resource
 *   associated with the U plane of a YUV texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_V_POINTER`: the ID3D12Resource
 *   associated with the V plane of a YUV texture, if you want to wrap an
 *   existing texture.
 *
 * With the metal renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER`: the CVPixelBufferRef
 *   associated with the texture, if you want to create a texture from an
 *   existing pixel buffer.
 *
 * With the opengl renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_NUMBER`: the GLuint texture
 *   associated with the texture, if you want to wrap an existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_UV_NUMBER`: the GLuint texture
 *   associated with the UV plane of an NV12 texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_U_NUMBER`: the GLuint texture
 *   associated with the U plane of a YUV texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_V_NUMBER`: the GLuint texture
 *   associated with the V plane of a YUV texture, if you want to wrap an
 *   existing texture.
 *
 * With the opengles2 renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_NUMBER`: the GLuint texture
 *   associated with the texture, if you want to wrap an existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_NUMBER`: the GLuint texture
 *   associated with the texture, if you want to wrap an existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_UV_NUMBER`: the GLuint texture
 *   associated with the UV plane of an NV12 texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_U_NUMBER`: the GLuint texture
 *   associated with the U plane of a YUV texture, if you want to wrap an
 *   existing texture.
 * - `SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_V_NUMBER`: the GLuint texture
 *   associated with the V plane of a YUV texture, if you want to wrap an
 *   existing texture.
 *
 * With the vulkan renderer:
 *
 * - `SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER`: the VkImage with layout
 *   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL associated with the texture, if
 *   you want to wrap an existing texture.
 *
 * \param renderer the rendering context
 * \param props the properties to use
 * \returns a pointer to the created texture or NULL if no rendering context
 *          was active, the format was unsupported, or the width or height
 *          were out of range; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateProperties
 * \sa SDL_CreateTexture
 * \sa SDL_CreateTextureFromSurface
 * \sa SDL_DestroyTexture
 * \sa SDL_QueryTexture
 * \sa SDL_UpdateTexture
 */
extern DECLSPEC SDL_Texture *SDLCALL SDL_CreateTextureWithProperties(SDL_Renderer *renderer, SDL_PropertiesID props);

#define SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER           "colorspace"
#define SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER               "format"
#define SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER               "access"
#define SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER                "width"
#define SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER               "height"
#define SDL_PROP_TEXTURE_CREATE_SDR_WHITE_POINT_FLOAT       "SDR_white_point"
#define SDL_PROP_TEXTURE_CREATE_HDR_HEADROOM_FLOAT          "HDR_headroom"
#define SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_POINTER       "d3d11.texture"
#define SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_U_POINTER     "d3d11.texture_u"
#define SDL_PROP_TEXTURE_CREATE_D3D11_TEXTURE_V_POINTER     "d3d11.texture_v"
#define SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_POINTER       "d3d12.texture"
#define SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_U_POINTER     "d3d12.texture_u"
#define SDL_PROP_TEXTURE_CREATE_D3D12_TEXTURE_V_POINTER     "d3d12.texture_v"
#define SDL_PROP_TEXTURE_CREATE_METAL_PIXELBUFFER_POINTER   "metal.pixelbuffer"
#define SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_NUMBER       "opengl.texture"
#define SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_UV_NUMBER    "opengl.texture_uv"
#define SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_U_NUMBER     "opengl.texture_u"
#define SDL_PROP_TEXTURE_CREATE_OPENGL_TEXTURE_V_NUMBER     "opengl.texture_v"
#define SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_NUMBER    "opengles2.texture"
#define SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_UV_NUMBER "opengles2.texture_uv"
#define SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_U_NUMBER  "opengles2.texture_u"
#define SDL_PROP_TEXTURE_CREATE_OPENGLES2_TEXTURE_V_NUMBER  "opengles2.texture_v"
#define SDL_PROP_TEXTURE_CREATE_VULKAN_TEXTURE_NUMBER       "vulkan.texture"

/**
 * Get the properties associated with a texture.
 *
 * The following read-only properties are provided by SDL:
 *
 * - `SDL_PROP_TEXTURE_COLORSPACE_NUMBER`: an SDL_ColorSpace value describing
 *   the colorspace used by the texture
 * - `SDL_PROP_TEXTURE_SDR_WHITE_POINT_FLOAT`: for HDR10 and floating point
 *   textures, this defines the value of 100% diffuse white, with higher
 *   values being displayed in the High Dynamic Range headroom. This defaults
 *   to 100 for HDR10 textures and 1.0 for other textures.
 * - `SDL_PROP_TEXTURE_HDR_HEADROOM_FLOAT`: for HDR10 and floating point
 *   textures, this defines the maximum dynamic range used by the content, in
 *   terms of the SDR white point. If this is defined, any values outside the
 *   range supported by the display will be scaled into the available HDR
 *   headroom, otherwise they are clipped. This defaults to 1.0 for SDR
 *   textures, 4.0 for HDR10 textures, and no default for floating point
 *   textures.
 *
 * With the direct3d11 renderer:
 *
 * - `SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER`: the ID3D11Texture2D associated
 *   with the texture
 * - `SDL_PROP_TEXTURE_D3D11_TEXTURE_U_POINTER`: the ID3D11Texture2D
 *   associated with the U plane of a YUV texture
 * - `SDL_PROP_TEXTURE_D3D11_TEXTURE_V_POINTER`: the ID3D11Texture2D
 *   associated with the V plane of a YUV texture
 *
 * With the direct3d12 renderer:
 *
 * - `SDL_PROP_TEXTURE_D3D12_TEXTURE_POINTER`: the ID3D12Resource associated
 *   with the texture
 * - `SDL_PROP_TEXTURE_D3D12_TEXTURE_U_POINTER`: the ID3D12Resource associated
 *   with the U plane of a YUV texture
 * - `SDL_PROP_TEXTURE_D3D12_TEXTURE_V_POINTER`: the ID3D12Resource associated
 *   with the V plane of a YUV texture
 *
 * With the vulkan renderer:
 *
 * - `SDL_PROP_TEXTURE_VULKAN_TEXTURE_POINTER`: the VkImage associated with
 *   the texture
 * - `SDL_PROP_TEXTURE_VULKAN_TEXTURE_U_POINTER`: the VkImage associated with
 *   the U plane of a YUV texture
 * - `SDL_PROP_TEXTURE_VULKAN_TEXTURE_V_POINTER`: the VkImage associated with
 *   the V plane of a YUV texture
 * - `SDL_PROP_TEXTURE_VULKAN_TEXTURE_UV_POINTER`: the VkImage associated with
 *   the UV plane of a NV12/NV21 texture
 *
 * With the opengl renderer:
 *
 * - `SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER`: the GLuint texture associated
 *   with the texture
 * - `SDL_PROP_TEXTURE_OPENGL_TEXTURE_UV_NUMBER`: the GLuint texture
 *   associated with the UV plane of an NV12 texture
 * - `SDL_PROP_TEXTURE_OPENGL_TEXTURE_U_NUMBER`: the GLuint texture associated
 *   with the U plane of a YUV texture
 * - `SDL_PROP_TEXTURE_OPENGL_TEXTURE_V_NUMBER`: the GLuint texture associated
 *   with the V plane of a YUV texture
 * - `SDL_PROP_TEXTURE_OPENGL_TEXTURE_TARGET_NUMBER`: the GLenum for the
 *   texture target (`GL_TEXTURE_2D`, `GL_TEXTURE_RECTANGLE_ARB`, etc)
 * - `SDL_PROP_TEXTURE_OPENGL_TEX_W_FLOAT`: the texture coordinate width of
 *   the texture (0.0 - 1.0)
 * - `SDL_PROP_TEXTURE_OPENGL_TEX_H_FLOAT`: the texture coordinate height of
 *   the texture (0.0 - 1.0)
 *
 * With the opengles2 renderer:
 *
 * - `SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER`: the GLuint texture
 *   associated with the texture
 * - `SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER`: the GLuint texture
 *   associated with the UV plane of an NV12 texture
 * - `SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_U_NUMBER`: the GLuint texture
 *   associated with the U plane of a YUV texture
 * - `SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_V_NUMBER`: the GLuint texture
 *   associated with the V plane of a YUV texture
 * - `SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_TARGET_NUMBER`: the GLenum for the
 *   texture target (`GL_TEXTURE_2D`, `GL_TEXTURE_EXTERNAL_OES`, etc)
 *
 * With the vulkan renderer:
 *
 * - `SDL_PROP_TEXTURE_VULKAN_TEXTURE_NUMBER`: the VkImage associated with the
 *   texture
 *
 * \param texture the texture to query
 * \returns a valid property ID on success or 0 on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetProperty
 * \sa SDL_SetProperty
 */
extern DECLSPEC SDL_PropertiesID SDLCALL SDL_GetTextureProperties(SDL_Texture *texture);

#define SDL_PROP_TEXTURE_COLORSPACE_NUMBER                  "SDL.texture.colorspace"
#define SDL_PROP_TEXTURE_SDR_WHITE_POINT_FLOAT              "SDL.texture.SDR_white_point"
#define SDL_PROP_TEXTURE_HDR_HEADROOM_FLOAT                 "SDL.texture.HDR_headroom"
#define SDL_PROP_TEXTURE_D3D11_TEXTURE_POINTER              "SDL.texture.d3d11.texture"
#define SDL_PROP_TEXTURE_D3D11_TEXTURE_U_POINTER            "SDL.texture.d3d11.texture_u"
#define SDL_PROP_TEXTURE_D3D11_TEXTURE_V_POINTER            "SDL.texture.d3d11.texture_v"
#define SDL_PROP_TEXTURE_D3D12_TEXTURE_POINTER              "SDL.texture.d3d12.texture"
#define SDL_PROP_TEXTURE_D3D12_TEXTURE_U_POINTER            "SDL.texture.d3d12.texture_u"
#define SDL_PROP_TEXTURE_D3D12_TEXTURE_V_POINTER            "SDL.texture.d3d12.texture_v"
#define SDL_PROP_TEXTURE_OPENGL_TEXTURE_NUMBER              "SDL.texture.opengl.texture"
#define SDL_PROP_TEXTURE_OPENGL_TEXTURE_UV_NUMBER           "SDL.texture.opengl.texture_uv"
#define SDL_PROP_TEXTURE_OPENGL_TEXTURE_U_NUMBER            "SDL.texture.opengl.texture_u"
#define SDL_PROP_TEXTURE_OPENGL_TEXTURE_V_NUMBER            "SDL.texture.opengl.texture_v"
#define SDL_PROP_TEXTURE_OPENGL_TEXTURE_TARGET_NUMBER       "SDL.texture.opengl.target"
#define SDL_PROP_TEXTURE_OPENGL_TEX_W_FLOAT                 "SDL.texture.opengl.tex_w"
#define SDL_PROP_TEXTURE_OPENGL_TEX_H_FLOAT                 "SDL.texture.opengl.tex_h"
#define SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_NUMBER           "SDL.texture.opengles2.texture"
#define SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_UV_NUMBER        "SDL.texture.opengles2.texture_uv"
#define SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_U_NUMBER         "SDL.texture.opengles2.texture_u"
#define SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_V_NUMBER         "SDL.texture.opengles2.texture_v"
#define SDL_PROP_TEXTURE_OPENGLES2_TEXTURE_TARGET_NUMBER    "SDL.texture.opengles2.target"
#define SDL_PROP_TEXTURE_VULKAN_TEXTURE_NUMBER              "SDL.texture.vulkan.texture"

/**
 * Get the renderer that created an SDL_Texture.
 *
 * \param texture the texture to query
 * \returns a pointer to the SDL_Renderer that created the texture, or NULL on
 *          failure; call SDL_GetError() for more information.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Renderer *SDLCALL SDL_GetRendererFromTexture(SDL_Texture *texture);

/**
 * Query the attributes of a texture.
 *
 * \param texture the texture to query
 * \param format a pointer filled in with the raw format of the texture; the
 *               actual format may differ, but pixel transfers will use this
 *               format (one of the SDL_PixelFormatEnum values). This argument
 *               can be NULL if you don't need this information.
 * \param access a pointer filled in with the actual access to the texture
 *               (one of the SDL_TextureAccess values). This argument can be
 *               NULL if you don't need this information.
 * \param w a pointer filled in with the width of the texture in pixels. This
 *          argument can be NULL if you don't need this information.
 * \param h a pointer filled in with the height of the texture in pixels. This
 *          argument can be NULL if you don't need this information.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access, int *w, int *h);

/**
 * Set an additional color value multiplied into render copy operations.
 *
 * When this texture is rendered, during the copy operation each source color
 * channel is modulated by the appropriate color value according to the
 * following formula:
 *
 * `srcC = srcC * (color / 255)`
 *
 * Color modulation is not always supported by the renderer; it will return -1
 * if color modulation is not supported.
 *
 * \param texture the texture to update
 * \param r the red color value multiplied into copy operations
 * \param g the green color value multiplied into copy operations
 * \param b the blue color value multiplied into copy operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureColorMod
 * \sa SDL_SetTextureAlphaMod
 * \sa SDL_SetTextureColorModFloat
 */
extern DECLSPEC int SDLCALL SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b);


/**
 * Set an additional color value multiplied into render copy operations.
 *
 * When this texture is rendered, during the copy operation each source color
 * channel is modulated by the appropriate color value according to the
 * following formula:
 *
 * `srcC = srcC * color`
 *
 * Color modulation is not always supported by the renderer; it will return -1
 * if color modulation is not supported.
 *
 * \param texture the texture to update
 * \param r the red color value multiplied into copy operations
 * \param g the green color value multiplied into copy operations
 * \param b the blue color value multiplied into copy operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureColorModFloat
 * \sa SDL_SetTextureAlphaModFloat
 * \sa SDL_SetTextureColorMod
 */
extern DECLSPEC int SDLCALL SDL_SetTextureColorModFloat(SDL_Texture *texture, float r, float g, float b);


/**
 * Get the additional color value multiplied into render copy operations.
 *
 * \param texture the texture to query
 * \param r a pointer filled in with the current red color value
 * \param g a pointer filled in with the current green color value
 * \param b a pointer filled in with the current blue color value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaMod
 * \sa SDL_GetTextureColorModFloat
 * \sa SDL_SetTextureColorMod
 */
extern DECLSPEC int SDLCALL SDL_GetTextureColorMod(SDL_Texture *texture, Uint8 *r, Uint8 *g, Uint8 *b);

/**
 * Get the additional color value multiplied into render copy operations.
 *
 * \param texture the texture to query
 * \param r a pointer filled in with the current red color value
 * \param g a pointer filled in with the current green color value
 * \param b a pointer filled in with the current blue color value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaModFloat
 * \sa SDL_GetTextureColorMod
 * \sa SDL_SetTextureColorModFloat
 */
extern DECLSPEC int SDLCALL SDL_GetTextureColorModFloat(SDL_Texture *texture, float *r, float *g, float *b);

/**
 * Set an additional alpha value multiplied into render copy operations.
 *
 * When this texture is rendered, during the copy operation the source alpha
 * value is modulated by this alpha value according to the following formula:
 *
 * `srcA = srcA * (alpha / 255)`
 *
 * Alpha modulation is not always supported by the renderer; it will return -1
 * if alpha modulation is not supported.
 *
 * \param texture the texture to update
 * \param alpha the source alpha value multiplied into copy operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaMod
 * \sa SDL_SetTextureAlphaModFloat
 * \sa SDL_SetTextureColorMod
 */
extern DECLSPEC int SDLCALL SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha);

/**
 * Set an additional alpha value multiplied into render copy operations.
 *
 * When this texture is rendered, during the copy operation the source alpha
 * value is modulated by this alpha value according to the following formula:
 *
 * `srcA = srcA * alpha`
 *
 * Alpha modulation is not always supported by the renderer; it will return -1
 * if alpha modulation is not supported.
 *
 * \param texture the texture to update
 * \param alpha the source alpha value multiplied into copy operations
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaModFloat
 * \sa SDL_SetTextureAlphaMod
 * \sa SDL_SetTextureColorModFloat
 */
extern DECLSPEC int SDLCALL SDL_SetTextureAlphaModFloat(SDL_Texture *texture, float alpha);

/**
 * Get the additional alpha value multiplied into render copy operations.
 *
 * \param texture the texture to query
 * \param alpha a pointer filled in with the current alpha value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaModFloat
 * \sa SDL_GetTextureColorMod
 * \sa SDL_SetTextureAlphaMod
 */
extern DECLSPEC int SDLCALL SDL_GetTextureAlphaMod(SDL_Texture *texture, Uint8 *alpha);

/**
 * Get the additional alpha value multiplied into render copy operations.
 *
 * \param texture the texture to query
 * \param alpha a pointer filled in with the current alpha value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureAlphaMod
 * \sa SDL_GetTextureColorModFloat
 * \sa SDL_SetTextureAlphaModFloat
 */
extern DECLSPEC int SDLCALL SDL_GetTextureAlphaModFloat(SDL_Texture *texture, float *alpha);

/**
 * Set the blend mode for a texture, used by SDL_RenderTexture().
 *
 * If the blend mode is not supported, the closest supported mode is chosen
 * and this function returns -1.
 *
 * \param texture the texture to update
 * \param blendMode the SDL_BlendMode to use for texture blending
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureBlendMode
 */
extern DECLSPEC int SDLCALL SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode);

/**
 * Get the blend mode used for texture copy operations.
 *
 * \param texture the texture to query
 * \param blendMode a pointer filled in with the current SDL_BlendMode
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetTextureBlendMode
 */
extern DECLSPEC int SDLCALL SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode);

/**
 * Set the scale mode used for texture scale operations.
 *
 * The default texture scale mode is SDL_SCALEMODE_LINEAR.
 *
 * If the scale mode is not supported, the closest supported mode is chosen.
 *
 * \param texture The texture to update.
 * \param scaleMode the SDL_ScaleMode to use for texture scaling.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTextureScaleMode
 */
extern DECLSPEC int SDLCALL SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode scaleMode);

/**
 * Get the scale mode used for texture scale operations.
 *
 * \param texture the texture to query.
 * \param scaleMode a pointer filled in with the current scale mode.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetTextureScaleMode
 */
extern DECLSPEC int SDLCALL SDL_GetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode *scaleMode);

/**
 * Update the given texture rectangle with new pixel data.
 *
 * The pixel data must be in the pixel format of the texture. Use
 * SDL_QueryTexture() to query the pixel format of the texture.
 *
 * This is a fairly slow function, intended for use with static textures that
 * do not change often.
 *
 * If the texture is intended to be updated often, it is preferred to create
 * the texture as streaming and use the locking functions referenced below.
 * While this function will work with streaming textures, for optimization
 * reasons you may not get the pixels back if you lock the texture afterward.
 *
 * \param texture the texture to update
 * \param rect an SDL_Rect structure representing the area to update, or NULL
 *             to update the entire texture
 * \param pixels the raw pixel data in the format of the texture
 * \param pitch the number of bytes in a row of pixel data, including padding
 *              between lines
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockTexture
 * \sa SDL_UnlockTexture
 * \sa SDL_UpdateNVTexture
 * \sa SDL_UpdateYUVTexture
 */
extern DECLSPEC int SDLCALL SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);

/**
 * Update a rectangle within a planar YV12 or IYUV texture with new pixel
 * data.
 *
 * You can use SDL_UpdateTexture() as long as your pixel data is a contiguous
 * block of Y and U/V planes in the proper order, but this function is
 * available if your pixel data is not contiguous.
 *
 * \param texture the texture to update
 * \param rect a pointer to the rectangle of pixels to update, or NULL to
 *             update the entire texture
 * \param Yplane the raw pixel data for the Y plane
 * \param Ypitch the number of bytes between rows of pixel data for the Y
 *               plane
 * \param Uplane the raw pixel data for the U plane
 * \param Upitch the number of bytes between rows of pixel data for the U
 *               plane
 * \param Vplane the raw pixel data for the V plane
 * \param Vpitch the number of bytes between rows of pixel data for the V
 *               plane
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UpdateNVTexture
 * \sa SDL_UpdateTexture
 */
extern DECLSPEC int SDLCALL SDL_UpdateYUVTexture(SDL_Texture *texture,
                                                 const SDL_Rect *rect,
                                                 const Uint8 *Yplane, int Ypitch,
                                                 const Uint8 *Uplane, int Upitch,
                                                 const Uint8 *Vplane, int Vpitch);

/**
 * Update a rectangle within a planar NV12 or NV21 texture with new pixels.
 *
 * You can use SDL_UpdateTexture() as long as your pixel data is a contiguous
 * block of NV12/21 planes in the proper order, but this function is available
 * if your pixel data is not contiguous.
 *
 * \param texture the texture to update
 * \param rect a pointer to the rectangle of pixels to update, or NULL to
 *             update the entire texture.
 * \param Yplane the raw pixel data for the Y plane.
 * \param Ypitch the number of bytes between rows of pixel data for the Y
 *               plane.
 * \param UVplane the raw pixel data for the UV plane.
 * \param UVpitch the number of bytes between rows of pixel data for the UV
 *                plane.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_UpdateTexture
 * \sa SDL_UpdateYUVTexture
 */
extern DECLSPEC int SDLCALL SDL_UpdateNVTexture(SDL_Texture *texture,
                                                 const SDL_Rect *rect,
                                                 const Uint8 *Yplane, int Ypitch,
                                                 const Uint8 *UVplane, int UVpitch);

/**
 * Lock a portion of the texture for **write-only** pixel access.
 *
 * As an optimization, the pixels made available for editing don't necessarily
 * contain the old texture data. This is a write-only operation, and if you
 * need to keep a copy of the texture data you should do that at the
 * application level.
 *
 * You must use SDL_UnlockTexture() to unlock the pixels and apply any
 * changes.
 *
 * \param texture the texture to lock for access, which was created with
 *                `SDL_TEXTUREACCESS_STREAMING`
 * \param rect an SDL_Rect structure representing the area to lock for access;
 *             NULL to lock the entire texture
 * \param pixels this is filled in with a pointer to the locked pixels,
 *               appropriately offset by the locked area
 * \param pitch this is filled in with the pitch of the locked pixels; the
 *              pitch is the length of one row in bytes
 * \returns 0 on success or a negative error code if the texture is not valid
 *          or was not created with `SDL_TEXTUREACCESS_STREAMING`; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockTextureToSurface
 * \sa SDL_UnlockTexture
 */
extern DECLSPEC int SDLCALL SDL_LockTexture(SDL_Texture *texture,
                                            const SDL_Rect *rect,
                                            void **pixels, int *pitch);

/**
 * Lock a portion of the texture for **write-only** pixel access, and expose
 * it as a SDL surface.
 *
 * Besides providing an SDL_Surface instead of raw pixel data, this function
 * operates like SDL_LockTexture.
 *
 * As an optimization, the pixels made available for editing don't necessarily
 * contain the old texture data. This is a write-only operation, and if you
 * need to keep a copy of the texture data you should do that at the
 * application level.
 *
 * You must use SDL_UnlockTexture() to unlock the pixels and apply any
 * changes.
 *
 * The returned surface is freed internally after calling SDL_UnlockTexture()
 * or SDL_DestroyTexture(). The caller should not free it.
 *
 * \param texture the texture to lock for access, which must be created with
 *                `SDL_TEXTUREACCESS_STREAMING`
 * \param rect a pointer to the rectangle to lock for access. If the rect is
 *             NULL, the entire texture will be locked
 * \param surface this is filled in with an SDL surface representing the
 *                locked area
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockTexture
 * \sa SDL_UnlockTexture
 */
extern DECLSPEC int SDLCALL SDL_LockTextureToSurface(SDL_Texture *texture,
                                            const SDL_Rect *rect,
                                            SDL_Surface **surface);

/**
 * Unlock a texture, uploading the changes to video memory, if needed.
 *
 * **Warning**: Please note that SDL_LockTexture() is intended to be
 * write-only; it will not guarantee the previous contents of the texture will
 * be provided. You must fully initialize any area of a texture that you lock
 * before unlocking it, as the pixels might otherwise be uninitialized memory.
 *
 * Which is to say: locking and immediately unlocking a texture can result in
 * corrupted textures, depending on the renderer in use.
 *
 * \param texture a texture locked by SDL_LockTexture()
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_LockTexture
 */
extern DECLSPEC void SDLCALL SDL_UnlockTexture(SDL_Texture *texture);

/**
 * Set a texture as the current rendering target.
 *
 * The default render target is the window for which the renderer was created.
 * To stop rendering to a texture and render to the window again, call this
 * function with a NULL `texture`.
 *
 * \param renderer the rendering context
 * \param texture the targeted texture, which must be created with the
 *                `SDL_TEXTUREACCESS_TARGET` flag, or NULL to render to the
 *                window instead of a texture.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderTarget
 */
extern DECLSPEC int SDLCALL SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);

/**
 * Get the current render target.
 *
 * The default render target is the window for which the renderer was created,
 * and is reported a NULL here.
 *
 * \param renderer the rendering context
 * \returns the current render target or NULL for the default render target.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderTarget
 */
extern DECLSPEC SDL_Texture *SDLCALL SDL_GetRenderTarget(SDL_Renderer *renderer);

/**
 * Set a device independent resolution and presentation mode for rendering.
 *
 * This function sets the width and height of the logical rendering output. A
 * render target is created at the specified size and used for rendering and
 * then copied to the output during presentation.
 *
 * You can disable logical coordinates by setting the mode to
 * SDL_LOGICAL_PRESENTATION_DISABLED, and in that case you get the full pixel
 * resolution of the output window.
 *
 * You can convert coordinates in an event into rendering coordinates using
 * SDL_ConvertEventToRenderCoordinates().
 *
 * \param renderer the rendering context
 * \param w the width of the logical resolution
 * \param h the height of the logical resolution
 * \param mode the presentation mode used
 * \param scale_mode the scale mode used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_ConvertEventToRenderCoordinates
 * \sa SDL_GetRenderLogicalPresentation
 */
extern DECLSPEC int SDLCALL SDL_SetRenderLogicalPresentation(SDL_Renderer *renderer, int w, int h, SDL_RendererLogicalPresentation mode, SDL_ScaleMode scale_mode);

/**
 * Get device independent resolution and presentation mode for rendering.
 *
 * This function gets the width and height of the logical rendering output, or
 * the output size in pixels if a logical resolution is not enabled.
 *
 * \param renderer the rendering context
 * \param w an int to be filled with the width
 * \param h an int to be filled with the height
 * \param mode a pointer filled in with the presentation mode
 * \param scale_mode a pointer filled in with the scale mode
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderLogicalPresentation
 */
extern DECLSPEC int SDLCALL SDL_GetRenderLogicalPresentation(SDL_Renderer *renderer, int *w, int *h, SDL_RendererLogicalPresentation *mode, SDL_ScaleMode *scale_mode);

/**
 * Get a point in render coordinates when given a point in window coordinates.
 *
 * \param renderer the rendering context
 * \param window_x the x coordinate in window coordinates
 * \param window_y the y coordinate in window coordinates
 * \param x a pointer filled with the x coordinate in render coordinates
 * \param y a pointer filled with the y coordinate in render coordinates
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderLogicalPresentation
 * \sa SDL_SetRenderScale
 */
extern DECLSPEC int SDLCALL SDL_RenderCoordinatesFromWindow(SDL_Renderer *renderer, float window_x, float window_y, float *x, float *y);

/**
 * Get a point in window coordinates when given a point in render coordinates.
 *
 * \param renderer the rendering context
 * \param x the x coordinate in render coordinates
 * \param y the y coordinate in render coordinates
 * \param window_x a pointer filled with the x coordinate in window
 *                 coordinates
 * \param window_y a pointer filled with the y coordinate in window
 *                 coordinates
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderLogicalPresentation
 * \sa SDL_SetRenderScale
 */
extern DECLSPEC int SDLCALL SDL_RenderCoordinatesToWindow(SDL_Renderer *renderer, float x, float y, float *window_x, float *window_y);

/**
 * Convert the coordinates in an event to render coordinates.
 *
 * Touch coordinates are converted from normalized coordinates in the window
 * to non-normalized rendering coordinates.
 *
 * Once converted, the coordinates may be outside the rendering area.
 *
 * \param renderer the rendering context
 * \param event the event to modify
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderCoordinatesFromWindowCoordinates
 */
extern DECLSPEC int SDLCALL SDL_ConvertEventToRenderCoordinates(SDL_Renderer *renderer, SDL_Event *event);

/**
 * Set the drawing area for rendering on the current target.
 *
 * \param renderer the rendering context
 * \param rect the SDL_Rect structure representing the drawing area, or NULL
 *             to set the viewport to the entire target
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderViewport
 * \sa SDL_RenderViewportSet
 */
extern DECLSPEC int SDLCALL SDL_SetRenderViewport(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Get the drawing area for the current target.
 *
 * \param renderer the rendering context
 * \param rect an SDL_Rect structure filled in with the current drawing area
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderViewportSet
 * \sa SDL_SetRenderViewport
 */
extern DECLSPEC int SDLCALL SDL_GetRenderViewport(SDL_Renderer *renderer, SDL_Rect *rect);

/**
 * Return whether an explicit rectangle was set as the viewport.
 *
 * This is useful if you're saving and restoring the viewport and want to know
 * whether you should restore a specific rectangle or NULL. Note that the
 * viewport is always reset when changing rendering targets.
 *
 * \param renderer the rendering context
 * \returns SDL_TRUE if the viewport was set to a specific rectangle, or
 *          SDL_FALSE if it was set to NULL (the entire target)
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderViewport
 * \sa SDL_SetRenderViewport
 */
extern DECLSPEC SDL_bool SDLCALL SDL_RenderViewportSet(SDL_Renderer *renderer);

/**
 * Set the clip rectangle for rendering on the specified target.
 *
 * \param renderer the rendering context
 * \param rect an SDL_Rect structure representing the clip area, relative to
 *             the viewport, or NULL to disable clipping
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderClipRect
 * \sa SDL_RenderClipEnabled
 */
extern DECLSPEC int SDLCALL SDL_SetRenderClipRect(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Get the clip rectangle for the current target.
 *
 * \param renderer the rendering context
 * \param rect an SDL_Rect structure filled in with the current clipping area
 *             or an empty rectangle if clipping is disabled
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderClipEnabled
 * \sa SDL_SetRenderClipRect
 */
extern DECLSPEC int SDLCALL SDL_GetRenderClipRect(SDL_Renderer *renderer, SDL_Rect *rect);

/**
 * Get whether clipping is enabled on the given renderer.
 *
 * \param renderer the rendering context
 * \returns SDL_TRUE if clipping is enabled or SDL_FALSE if not; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderClipRect
 * \sa SDL_SetRenderClipRect
 */
extern DECLSPEC SDL_bool SDLCALL SDL_RenderClipEnabled(SDL_Renderer *renderer);

/**
 * Set the drawing scale for rendering on the current target.
 *
 * The drawing coordinates are scaled by the x/y scaling factors before they
 * are used by the renderer. This allows resolution independent drawing with a
 * single coordinate system.
 *
 * If this results in scaling or subpixel drawing by the rendering backend, it
 * will be handled using the appropriate quality hints. For best results use
 * integer scaling factors.
 *
 * \param renderer the rendering context
 * \param scaleX the horizontal scaling factor
 * \param scaleY the vertical scaling factor
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderScale
 */
extern DECLSPEC int SDLCALL SDL_SetRenderScale(SDL_Renderer *renderer, float scaleX, float scaleY);

/**
 * Get the drawing scale for the current target.
 *
 * \param renderer the rendering context
 * \param scaleX a pointer filled in with the horizontal scaling factor
 * \param scaleY a pointer filled in with the vertical scaling factor
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderScale
 */
extern DECLSPEC int SDLCALL SDL_GetRenderScale(SDL_Renderer *renderer, float *scaleX, float *scaleY);

/**
 * Set the color used for drawing operations.
 *
 * Set the color for drawing or filling rectangles, lines, and points, and for
 * SDL_RenderClear().
 *
 * \param renderer the rendering context
 * \param r the red value used to draw on the rendering target
 * \param g the green value used to draw on the rendering target
 * \param b the blue value used to draw on the rendering target
 * \param a the alpha value used to draw on the rendering target; usually
 *          `SDL_ALPHA_OPAQUE` (255). Use SDL_SetRenderDrawBlendMode to
 *          specify how the alpha channel is used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderDrawColor
 * \sa SDL_SetRenderDrawColorFloat
 */
extern DECLSPEC int SDLCALL SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/**
 * Set the color used for drawing operations (Rect, Line and Clear).
 *
 * Set the color for drawing or filling rectangles, lines, and points, and for
 * SDL_RenderClear().
 *
 * \param renderer the rendering context
 * \param r the red value used to draw on the rendering target
 * \param g the green value used to draw on the rendering target
 * \param b the blue value used to draw on the rendering target
 * \param a the alpha value used to draw on the rendering target. Use
 *          SDL_SetRenderDrawBlendMode to specify how the alpha channel is
 *          used
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderDrawColorFloat
 * \sa SDL_SetRenderDrawColor
 */
extern DECLSPEC int SDLCALL SDL_SetRenderDrawColorFloat(SDL_Renderer *renderer, float r, float g, float b, float a);

/**
 * Get the color used for drawing operations (Rect, Line and Clear).
 *
 * \param renderer the rendering context
 * \param r a pointer filled in with the red value used to draw on the
 *          rendering target
 * \param g a pointer filled in with the green value used to draw on the
 *          rendering target
 * \param b a pointer filled in with the blue value used to draw on the
 *          rendering target
 * \param a a pointer filled in with the alpha value used to draw on the
 *          rendering target; usually `SDL_ALPHA_OPAQUE` (255)
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderDrawColorFloat
 * \sa SDL_SetRenderDrawColor
 */
extern DECLSPEC int SDLCALL SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);

/**
 * Get the color used for drawing operations (Rect, Line and Clear).
 *
 * \param renderer the rendering context
 * \param r a pointer filled in with the red value used to draw on the
 *          rendering target
 * \param g a pointer filled in with the green value used to draw on the
 *          rendering target
 * \param b a pointer filled in with the blue value used to draw on the
 *          rendering target
 * \param a a pointer filled in with the alpha value used to draw on the
 *          rendering target
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderDrawColorFloat
 * \sa SDL_GetRenderDrawColor
 */
extern DECLSPEC int SDLCALL SDL_GetRenderDrawColorFloat(SDL_Renderer *renderer, float *r, float *g, float *b, float *a);

/**
 * Set the color scale used for render operations.
 *
 * The color scale is an additional scale multiplied into the pixel color
 * value while rendering. This can be used to adjust the brightness of colors
 * during HDR rendering, or changing HDR video brightness when playing on an
 * SDR display.
 *
 * The color scale does not affect the alpha channel, only the color
 * brightness.
 *
 * \param renderer the rendering context
 * \param scale the color scale value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderColorScale
 */
extern DECLSPEC int SDLCALL SDL_SetRenderColorScale(SDL_Renderer *renderer, float scale);

/**
 * Get the color scale used for render operations.
 *
 * \param renderer the rendering context
 * \param scale a pointer filled in with the current color scale value
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderColorScale
 */
extern DECLSPEC int SDLCALL SDL_GetRenderColorScale(SDL_Renderer *renderer, float *scale);

/**
 * Set the blend mode used for drawing operations (Fill and Line).
 *
 * If the blend mode is not supported, the closest supported mode is chosen.
 *
 * \param renderer the rendering context
 * \param blendMode the SDL_BlendMode to use for blending
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderDrawBlendMode
 */
extern DECLSPEC int SDLCALL SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode blendMode);

/**
 * Get the blend mode used for drawing operations.
 *
 * \param renderer the rendering context
 * \param blendMode a pointer filled in with the current SDL_BlendMode
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderDrawBlendMode
 */
extern DECLSPEC int SDLCALL SDL_GetRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode *blendMode);

/**
 * Clear the current rendering target with the drawing color.
 *
 * This function clears the entire rendering target, ignoring the viewport and
 * the clip rectangle.
 *
 * \param renderer the rendering context
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderDrawColor
 */
extern DECLSPEC int SDLCALL SDL_RenderClear(SDL_Renderer *renderer);

/**
 * Draw a point on the current rendering target at subpixel precision.
 *
 * \param renderer The renderer which should draw a point.
 * \param x The x coordinate of the point.
 * \param y The y coordinate of the point.
 * \returns 0 on success, or -1 on error
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderPoints
 */
extern DECLSPEC int SDLCALL SDL_RenderPoint(SDL_Renderer *renderer, float x, float y);

/**
 * Draw multiple points on the current rendering target at subpixel precision.
 *
 * \param renderer The renderer which should draw multiple points.
 * \param points The points to draw
 * \param count The number of points to draw
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderPoint
 */
extern DECLSPEC int SDLCALL SDL_RenderPoints(SDL_Renderer *renderer, const SDL_FPoint *points, int count);

/**
 * Draw a line on the current rendering target at subpixel precision.
 *
 * \param renderer The renderer which should draw a line.
 * \param x1 The x coordinate of the start point.
 * \param y1 The y coordinate of the start point.
 * \param x2 The x coordinate of the end point.
 * \param y2 The y coordinate of the end point.
 * \returns 0 on success, or -1 on error
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderLines
 */
extern DECLSPEC int SDLCALL SDL_RenderLine(SDL_Renderer *renderer, float x1, float y1, float x2, float y2);

/**
 * Draw a series of connected lines on the current rendering target at
 * subpixel precision.
 *
 * \param renderer The renderer which should draw multiple lines.
 * \param points The points along the lines
 * \param count The number of points, drawing count-1 lines
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderLine
 */
extern DECLSPEC int SDLCALL SDL_RenderLines(SDL_Renderer *renderer, const SDL_FPoint *points, int count);

/**
 * Draw a rectangle on the current rendering target at subpixel precision.
 *
 * \param renderer The renderer which should draw a rectangle.
 * \param rect A pointer to the destination rectangle, or NULL to outline the
 *             entire rendering target.
 * \returns 0 on success, or -1 on error
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderRects
 */
extern DECLSPEC int SDLCALL SDL_RenderRect(SDL_Renderer *renderer, const SDL_FRect *rect);

/**
 * Draw some number of rectangles on the current rendering target at subpixel
 * precision.
 *
 * \param renderer The renderer which should draw multiple rectangles.
 * \param rects A pointer to an array of destination rectangles.
 * \param count The number of rectangles.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderRect
 */
extern DECLSPEC int SDLCALL SDL_RenderRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);

/**
 * Fill a rectangle on the current rendering target with the drawing color at
 * subpixel precision.
 *
 * \param renderer The renderer which should fill a rectangle.
 * \param rect A pointer to the destination rectangle, or NULL for the entire
 *             rendering target.
 * \returns 0 on success, or -1 on error
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderFillRects
 */
extern DECLSPEC int SDLCALL SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect);

/**
 * Fill some number of rectangles on the current rendering target with the
 * drawing color at subpixel precision.
 *
 * \param renderer The renderer which should fill multiple rectangles.
 * \param rects A pointer to an array of destination rectangles.
 * \param count The number of rectangles.
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderFillRect
 */
extern DECLSPEC int SDLCALL SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);

/**
 * Copy a portion of the texture to the current rendering target at subpixel
 * precision.
 *
 * \param renderer The renderer which should copy parts of a texture.
 * \param texture The source texture.
 * \param srcrect A pointer to the source rectangle, or NULL for the entire
 *                texture.
 * \param dstrect A pointer to the destination rectangle, or NULL for the
 *                entire rendering target.
 * \returns 0 on success, or -1 on error
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderTextureRotated
 */
extern DECLSPEC int SDLCALL SDL_RenderTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_FRect *srcrect, const SDL_FRect *dstrect);

/**
 * Copy a portion of the source texture to the current rendering target, with
 * rotation and flipping, at subpixel precision.
 *
 * \param renderer The renderer which should copy parts of a texture.
 * \param texture The source texture.
 * \param srcrect A pointer to the source rectangle, or NULL for the entire
 *                texture.
 * \param dstrect A pointer to the destination rectangle, or NULL for the
 *                entire rendering target.
 * \param angle An angle in degrees that indicates the rotation that will be
 *              applied to dstrect, rotating it in a clockwise direction
 * \param center A pointer to a point indicating the point around which
 *               dstrect will be rotated (if NULL, rotation will be done
 *               around dstrect.w/2, dstrect.h/2).
 * \param flip An SDL_FlipMode value stating which flipping actions should be
 *             performed on the texture
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderTexture
 */
extern DECLSPEC int SDLCALL SDL_RenderTextureRotated(SDL_Renderer *renderer, SDL_Texture *texture,
                                                     const SDL_FRect *srcrect, const SDL_FRect *dstrect,
                                                     const double angle, const SDL_FPoint *center,
                                                     const SDL_FlipMode flip);

/**
 * Render a list of triangles, optionally using a texture and indices into the
 * vertex array Color and alpha modulation is done per vertex
 * (SDL_SetTextureColorMod and SDL_SetTextureAlphaMod are ignored).
 *
 * \param renderer The rendering context.
 * \param texture (optional) The SDL texture to use.
 * \param vertices Vertices.
 * \param num_vertices Number of vertices.
 * \param indices (optional) An array of integer indices into the 'vertices'
 *                array, if NULL all vertices will be rendered in sequential
 *                order.
 * \param num_indices Number of indices.
 * \returns 0 on success, or -1 if the operation is not supported
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderGeometryRaw
 */
extern DECLSPEC int SDLCALL SDL_RenderGeometry(SDL_Renderer *renderer,
                                               SDL_Texture *texture,
                                               const SDL_Vertex *vertices, int num_vertices,
                                               const int *indices, int num_indices);

/**
 * Render a list of triangles, optionally using a texture and indices into the
 * vertex arrays Color and alpha modulation is done per vertex
 * (SDL_SetTextureColorMod and SDL_SetTextureAlphaMod are ignored).
 *
 * \param renderer The rendering context.
 * \param texture (optional) The SDL texture to use.
 * \param xy Vertex positions
 * \param xy_stride Byte size to move from one element to the next element
 * \param color Vertex colors (as SDL_Color)
 * \param color_stride Byte size to move from one element to the next element
 * \param uv Vertex normalized texture coordinates
 * \param uv_stride Byte size to move from one element to the next element
 * \param num_vertices Number of vertices.
 * \param indices (optional) An array of indices into the 'vertices' arrays,
 *                if NULL all vertices will be rendered in sequential order.
 * \param num_indices Number of indices.
 * \param size_indices Index size: 1 (byte), 2 (short), 4 (int)
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderGeometry
 */
extern DECLSPEC int SDLCALL SDL_RenderGeometryRaw(SDL_Renderer *renderer,
                                               SDL_Texture *texture,
                                               const float *xy, int xy_stride,
                                               const SDL_Color *color, int color_stride,
                                               const float *uv, int uv_stride,
                                               int num_vertices,
                                               const void *indices, int num_indices, int size_indices);

/**
 * Render a list of triangles, optionally using a texture and indices into the
 * vertex arrays Color and alpha modulation is done per vertex
 * (SDL_SetTextureColorMod and SDL_SetTextureAlphaMod are ignored).
 *
 * \param renderer The rendering context.
 * \param texture (optional) The SDL texture to use.
 * \param xy Vertex positions
 * \param xy_stride Byte size to move from one element to the next element
 * \param color Vertex colors (as SDL_FColor)
 * \param color_stride Byte size to move from one element to the next element
 * \param uv Vertex normalized texture coordinates
 * \param uv_stride Byte size to move from one element to the next element
 * \param num_vertices Number of vertices.
 * \param indices (optional) An array of indices into the 'vertices' arrays,
 *                if NULL all vertices will be rendered in sequential order.
 * \param num_indices Number of indices.
 * \param size_indices Index size: 1 (byte), 2 (short), 4 (int)
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderGeometry
 * \sa SDL_RenderGeometryRaw
 */
extern DECLSPEC int SDLCALL SDL_RenderGeometryRawFloat(SDL_Renderer *renderer,
                                               SDL_Texture *texture,
                                               const float *xy, int xy_stride,
                                               const SDL_FColor *color, int color_stride,
                                               const float *uv, int uv_stride,
                                               int num_vertices,
                                               const void *indices, int num_indices, int size_indices);

/**
 * Read pixels from the current rendering target.
 *
 * The returned surface should be freed with SDL_DestroySurface()
 *
 * **WARNING**: This is a very slow operation, and should not be used
 * frequently. If you're using this on the main rendering target, it should be
 * called after rendering and before SDL_RenderPresent().
 *
 * \param renderer the rendering context
 * \param rect an SDL_Rect structure representing the area in pixels relative
 *             to the to current viewport, or NULL for the entire viewport
 * \returns a new SDL_Surface on success or NULL on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Surface * SDLCALL SDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect);

/**
 * Update the screen with any rendering performed since the previous call.
 *
 * SDL's rendering functions operate on a backbuffer; that is, calling a
 * rendering function such as SDL_RenderLine() does not directly put a line on
 * the screen, but rather updates the backbuffer. As such, you compose your
 * entire scene and *present* the composed backbuffer to the screen as a
 * complete picture.
 *
 * Therefore, when using SDL's rendering API, one does all drawing intended
 * for the frame, and then calls this function once per frame to present the
 * final drawing to the user.
 *
 * The backbuffer should be considered invalidated after each present; do not
 * assume that previous contents will exist between frames. You are strongly
 * encouraged to call SDL_RenderClear() to initialize the backbuffer before
 * starting each new frame's drawing, even if you plan to overwrite every
 * pixel.
 *
 * \param renderer the rendering context
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \threadsafety You may only call this function on the main thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_RenderClear
 * \sa SDL_RenderLine
 * \sa SDL_RenderLines
 * \sa SDL_RenderPoint
 * \sa SDL_RenderPoints
 * \sa SDL_RenderRect
 * \sa SDL_RenderRects
 * \sa SDL_RenderFillRect
 * \sa SDL_RenderFillRects
 * \sa SDL_SetRenderDrawBlendMode
 * \sa SDL_SetRenderDrawColor
 */
extern DECLSPEC int SDLCALL SDL_RenderPresent(SDL_Renderer *renderer);

/**
 * Destroy the specified texture.
 *
 * Passing NULL or an otherwise invalid texture will set the SDL error message
 * to "Invalid texture".
 *
 * \param texture the texture to destroy
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTexture
 * \sa SDL_CreateTextureFromSurface
 */
extern DECLSPEC void SDLCALL SDL_DestroyTexture(SDL_Texture *texture);

/**
 * Destroy the rendering context for a window and free associated textures.
 *
 * If `renderer` is NULL, this function will return immediately after setting
 * the SDL error message to "Invalid renderer". See SDL_GetError().
 *
 * \param renderer the rendering context
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateRenderer
 */
extern DECLSPEC void SDLCALL SDL_DestroyRenderer(SDL_Renderer *renderer);

/**
 * Force the rendering context to flush any pending commands and state.
 *
 * You do not need to (and in fact, shouldn't) call this function unless you
 * are planning to call into OpenGL/Direct3D/Metal/whatever directly, in
 * addition to using an SDL_Renderer.
 *
 * This is for a very-specific case: if you are using SDL's render API, and
 * you plan to make OpenGL/D3D/whatever calls in addition to SDL render API
 * calls. If this applies, you should call this function between calls to
 * SDL's render API and the low-level API you're using in cooperation.
 *
 * In all other cases, you can ignore this function.
 *
 * This call makes SDL flush any pending rendering work it was queueing up to
 * do later in a single batch, and marks any internal cached state as invalid,
 * so it'll prepare all its state again later, from scratch.
 *
 * This means you do not need to save state in your rendering code to protect
 * the SDL renderer. However, there lots of arbitrary pieces of Direct3D and
 * OpenGL state that can confuse things; you should use your best judgement
 * and be prepared to make changes if specific state needs to be protected.
 *
 * \param renderer the rendering context
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_FlushRenderer(SDL_Renderer *renderer);

/**
 * Get the CAMetalLayer associated with the given Metal renderer.
 *
 * This function returns `void *`, so SDL doesn't have to include Metal's
 * headers, but it can be safely cast to a `CAMetalLayer *`.
 *
 * \param renderer The renderer to query
 * \returns a `CAMetalLayer *` on success, or NULL if the renderer isn't a
 *          Metal renderer
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderMetalCommandEncoder
 */
extern DECLSPEC void *SDLCALL SDL_GetRenderMetalLayer(SDL_Renderer *renderer);

/**
 * Get the Metal command encoder for the current frame
 *
 * This function returns `void *`, so SDL doesn't have to include Metal's
 * headers, but it can be safely cast to an `id<MTLRenderCommandEncoder>`.
 *
 * Note that as of SDL 2.0.18, this will return NULL if Metal refuses to give
 * SDL a drawable to render to, which might happen if the window is
 * hidden/minimized/offscreen. This doesn't apply to command encoders for
 * render targets, just the window's backbuffer. Check your return values!
 *
 * \param renderer The renderer to query
 * \returns an `id<MTLRenderCommandEncoder>` on success, or NULL if the
 *          renderer isn't a Metal renderer or there was an error.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderMetalLayer
 */
extern DECLSPEC void *SDLCALL SDL_GetRenderMetalCommandEncoder(SDL_Renderer *renderer);


/**
 * Add a set of synchronization semaphores for the current frame.
 *
 * The Vulkan renderer will wait for `wait_semaphore` before submitting
 * rendering commands and signal `signal_semaphore` after rendering commands
 * are complete for this frame.
 *
 * This should be called each frame that you want semaphore synchronization.
 * The Vulkan renderer may have multiple frames in flight on the GPU, so you
 * should have multiple semaphores that are used for synchronization. Querying
 * SDL_PROP_RENDERER_VULKAN_SWAPCHAIN_IMAGE_COUNT_NUMBER will give you the
 * maximum number of semaphores you'll need.
 *
 * \param renderer the rendering context
 * \param wait_stage_mask the VkPipelineStageFlags for the wait
 * \param wait_semaphore a VkSempahore to wait on before rendering the current
 *                       frame, or 0 if not needed
 * \param signal_semaphore a VkSempahore that SDL will signal when rendering
 *                         for the current frame is complete, or 0 if not
 *                         needed
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_AddVulkanRenderSemaphores(SDL_Renderer *renderer, Uint32 wait_stage_mask, Sint64 wait_semaphore, Sint64 signal_semaphore);

/**
 * Toggle VSync of the given renderer.
 *
 * \param renderer The renderer to toggle
 * \param vsync 1 for on, 0 for off. All other values are reserved
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetRenderVSync
 */
extern DECLSPEC int SDLCALL SDL_SetRenderVSync(SDL_Renderer *renderer, int vsync);

/**
 * Get VSync of the given renderer.
 *
 * \param renderer The renderer to toggle
 * \param vsync an int filled with 1 for on, 0 for off. All other values are
 *              reserved
 * \returns 0 on success or a negative error code on failure; call
 *          SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetRenderVSync
 */
extern DECLSPEC int SDLCALL SDL_GetRenderVSync(SDL_Renderer *renderer, int *vsync);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_render_h_ */
